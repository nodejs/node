// Copyright 2012 the V8 project authors. All rights reserved.
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

#include "api.h"
#include "arguments.h"
#include "bootstrapper.h"
#include "codegen.h"
#include "debug.h"
#include "deoptimizer.h"
#include "date.h"
#include "elements.h"
#include "execution.h"
#include "full-codegen.h"
#include "hydrogen.h"
#include "objects-inl.h"
#include "objects-visiting.h"
#include "objects-visiting-inl.h"
#include "macro-assembler.h"
#include "mark-compact.h"
#include "safepoint-table.h"
#include "string-stream.h"
#include "utils.h"
#include "vm-state-inl.h"

#ifdef ENABLE_DISASSEMBLER
#include "disasm.h"
#include "disassembler.h"
#endif

namespace v8 {
namespace internal {

void PrintElementsKind(FILE* out, ElementsKind kind) {
  ElementsAccessor* accessor = ElementsAccessor::ForKind(kind);
  PrintF(out, "%s", accessor->name());
}


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


MaybeObject* Object::ToObject(Context* global_context) {
  if (IsNumber()) {
    return CreateJSValue(global_context->number_function(), this);
  } else if (IsBoolean()) {
    return CreateJSValue(global_context->boolean_function(), this);
  } else if (IsString()) {
    return CreateJSValue(global_context->string_function(), this);
  }
  ASSERT(IsJSObject());
  return this;
}


MaybeObject* Object::ToObject() {
  if (IsJSReceiver()) {
    return this;
  } else if (IsNumber()) {
    Isolate* isolate = Isolate::Current();
    Context* global_context = isolate->context()->global_context();
    return CreateJSValue(global_context->number_function(), this);
  } else if (IsBoolean()) {
    Isolate* isolate = HeapObject::cast(this)->GetIsolate();
    Context* global_context = isolate->context()->global_context();
    return CreateJSValue(global_context->boolean_function(), this);
  } else if (IsString()) {
    Isolate* isolate = HeapObject::cast(this)->GetIsolate();
    Context* global_context = isolate->context()->global_context();
    return CreateJSValue(global_context->string_function(), this);
  }

  // Throw a type error.
  return Failure::InternalError();
}


Object* Object::ToBoolean() {
  if (IsTrue()) return this;
  if (IsFalse()) return this;
  if (IsSmi()) {
    return Isolate::Current()->heap()->ToBoolean(Smi::cast(this)->value() != 0);
  }
  HeapObject* heap_object = HeapObject::cast(this);
  if (heap_object->IsUndefined() || heap_object->IsNull()) {
    return heap_object->GetHeap()->false_value();
  }
  // Undetectable object is false
  if (heap_object->IsUndetectableObject()) {
    return heap_object->GetHeap()->false_value();
  }
  if (heap_object->IsString()) {
    return heap_object->GetHeap()->ToBoolean(
        String::cast(this)->length() != 0);
  }
  if (heap_object->IsHeapNumber()) {
    return HeapNumber::cast(this)->HeapNumberToBoolean();
  }
  return heap_object->GetHeap()->true_value();
}


void Object::Lookup(String* name, LookupResult* result) {
  Object* holder = NULL;
  if (IsJSReceiver()) {
    holder = this;
  } else {
    Context* global_context = Isolate::Current()->context()->global_context();
    if (IsNumber()) {
      holder = global_context->number_function()->instance_prototype();
    } else if (IsString()) {
      holder = global_context->string_function()->instance_prototype();
    } else if (IsBoolean()) {
      holder = global_context->boolean_function()->instance_prototype();
    }
  }
  ASSERT(holder != NULL);  // Cannot handle null or undefined.
  JSReceiver::cast(holder)->Lookup(name, result);
}


MaybeObject* Object::GetPropertyWithReceiver(Object* receiver,
                                             String* name,
                                             PropertyAttributes* attributes) {
  LookupResult result(name->GetIsolate());
  Lookup(name, &result);
  MaybeObject* value = GetProperty(receiver, &result, name, attributes);
  ASSERT(*attributes <= ABSENT);
  return value;
}


MaybeObject* JSObject::GetPropertyWithCallback(Object* receiver,
                                               Object* structure,
                                               String* name) {
  Isolate* isolate = name->GetIsolate();
  // To accommodate both the old and the new api we switch on the
  // data structure used to store the callbacks.  Eventually foreign
  // callbacks should be phased out.
  if (structure->IsForeign()) {
    AccessorDescriptor* callback =
        reinterpret_cast<AccessorDescriptor*>(
            Foreign::cast(structure)->foreign_address());
    MaybeObject* value = (callback->getter)(receiver, callback->data);
    RETURN_IF_SCHEDULED_EXCEPTION(isolate);
    return value;
  }

  // api style callbacks.
  if (structure->IsAccessorInfo()) {
    AccessorInfo* data = AccessorInfo::cast(structure);
    Object* fun_obj = data->getter();
    v8::AccessorGetter call_fun = v8::ToCData<v8::AccessorGetter>(fun_obj);
    HandleScope scope(isolate);
    JSObject* self = JSObject::cast(receiver);
    Handle<String> key(name);
    LOG(isolate, ApiNamedPropertyAccess("load", self, name));
    CustomArguments args(isolate, data->data(), self, this);
    v8::AccessorInfo info(args.end());
    v8::Handle<v8::Value> result;
    {
      // Leaving JavaScript.
      VMState state(isolate, EXTERNAL);
      result = call_fun(v8::Utils::ToLocal(key), info);
    }
    RETURN_IF_SCHEDULED_EXCEPTION(isolate);
    if (result.IsEmpty()) {
      return isolate->heap()->undefined_value();
    }
    return *v8::Utils::OpenHandle(*result);
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
                                             String* name_raw) {
  Isolate* isolate = GetIsolate();
  HandleScope scope(isolate);
  Handle<Object> receiver(receiver_raw);
  Handle<Object> name(name_raw);

  Handle<Object> args[] = { receiver, name };
  Handle<Object> result = CallTrap(
    "get", isolate->derived_get_trap(), ARRAY_SIZE(args), args);
  if (isolate->has_pending_exception()) return Failure::Exception();

  return *result;
}


Handle<Object> Object::GetElement(Handle<Object> object, uint32_t index) {
  Isolate* isolate = object->IsHeapObject()
      ? Handle<HeapObject>::cast(object)->GetIsolate()
      : Isolate::Current();
  CALL_HEAP_FUNCTION(isolate, object->GetElement(index), Object);
}


MaybeObject* JSProxy::GetElementWithHandler(Object* receiver,
                                            uint32_t index) {
  String* name;
  MaybeObject* maybe = GetHeap()->Uint32ToString(index);
  if (!maybe->To<String>(&name)) return maybe;
  return GetPropertyWithHandler(receiver, name);
}


MaybeObject* JSProxy::SetElementWithHandler(uint32_t index,
                                            Object* value,
                                            StrictModeFlag strict_mode) {
  String* name;
  MaybeObject* maybe = GetHeap()->Uint32ToString(index);
  if (!maybe->To<String>(&name)) return maybe;
  return SetPropertyWithHandler(name, value, NONE, strict_mode);
}


bool JSProxy::HasElementWithHandler(uint32_t index) {
  String* name;
  MaybeObject* maybe = GetHeap()->Uint32ToString(index);
  if (!maybe->To<String>(&name)) return maybe;
  return HasPropertyWithHandler(name);
}


MaybeObject* Object::GetPropertyWithDefinedGetter(Object* receiver,
                                                  JSReceiver* getter) {
  HandleScope scope;
  Handle<JSReceiver> fun(getter);
  Handle<Object> self(receiver);
#ifdef ENABLE_DEBUGGER_SUPPORT
  Debug* debug = fun->GetHeap()->isolate()->debug();
  // Handle stepping into a getter if step into is active.
  // TODO(rossberg): should this apply to getters that are function proxies?
  if (debug->StepInActive() && fun->IsJSFunction()) {
    debug->HandleStepIn(
        Handle<JSFunction>::cast(fun), Handle<Object>::null(), 0, false);
  }
#endif

  bool has_pending_exception;
  Handle<Object> result =
      Execution::Call(fun, self, 0, NULL, &has_pending_exception, true);
  // Check for pending exception and return the result.
  if (has_pending_exception) return Failure::Exception();
  return *result;
}


// Only deal with CALLBACKS and INTERCEPTOR
MaybeObject* JSObject::GetPropertyWithFailedAccessCheck(
    Object* receiver,
    LookupResult* result,
    String* name,
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
        }
        break;
      }
      case NORMAL:
      case FIELD:
      case CONSTANT_FUNCTION: {
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
  heap->isolate()->ReportFailedAccessCheck(this, v8::ACCESS_GET);
  return heap->undefined_value();
}


PropertyAttributes JSObject::GetPropertyAttributeWithFailedAccessCheck(
    Object* receiver,
    LookupResult* result,
    String* name,
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
        }
        break;
      }

      case NORMAL:
      case FIELD:
      case CONSTANT_FUNCTION: {
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
        if (r.IsProperty()) {
          return GetPropertyAttributeWithFailedAccessCheck(receiver,
                                                           &r,
                                                           name,
                                                           continue_search);
        }
        break;
      }

      default:
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
    value = JSGlobalPropertyCell::cast(value)->value();
  }
  ASSERT(!value->IsJSGlobalPropertyCell());
  return value;
}


Object* JSObject::SetNormalizedProperty(LookupResult* result, Object* value) {
  ASSERT(!HasFastProperties());
  if (IsGlobalObject()) {
    JSGlobalPropertyCell* cell =
        JSGlobalPropertyCell::cast(
            property_dictionary()->ValueAt(result->GetDictionaryEntry()));
    cell->set_value(value);
  } else {
    property_dictionary()->ValueAtPut(result->GetDictionaryEntry(), value);
  }
  return value;
}


Handle<Object> JSObject::SetNormalizedProperty(Handle<JSObject> object,
                                               Handle<String> key,
                                               Handle<Object> value,
                                               PropertyDetails details) {
  CALL_HEAP_FUNCTION(object->GetIsolate(),
                     object->SetNormalizedProperty(*key, *value, details),
                     Object);
}


MaybeObject* JSObject::SetNormalizedProperty(String* name,
                                             Object* value,
                                             PropertyDetails details) {
  ASSERT(!HasFastProperties());
  int entry = property_dictionary()->FindEntry(name);
  if (entry == StringDictionary::kNotFound) {
    Object* store_value = value;
    if (IsGlobalObject()) {
      Heap* heap = name->GetHeap();
      MaybeObject* maybe_store_value =
          heap->AllocateJSGlobalPropertyCell(value);
      if (!maybe_store_value->ToObject(&store_value)) return maybe_store_value;
    }
    Object* dict;
    { MaybeObject* maybe_dict =
          property_dictionary()->Add(name, store_value, details);
      if (!maybe_dict->ToObject(&dict)) return maybe_dict;
    }
    set_properties(StringDictionary::cast(dict));
    return value;
  }
  // Preserve enumeration index.
  details = PropertyDetails(details.attributes(),
                            details.type(),
                            property_dictionary()->DetailsAt(entry).index());
  if (IsGlobalObject()) {
    JSGlobalPropertyCell* cell =
        JSGlobalPropertyCell::cast(property_dictionary()->ValueAt(entry));
    cell->set_value(value);
    // Please note we have to update the property details.
    property_dictionary()->DetailsAtPut(entry, details);
  } else {
    property_dictionary()->SetEntry(entry, name, value, details);
  }
  return value;
}


MaybeObject* JSObject::DeleteNormalizedProperty(String* name, DeleteMode mode) {
  ASSERT(!HasFastProperties());
  StringDictionary* dictionary = property_dictionary();
  int entry = dictionary->FindEntry(name);
  if (entry != StringDictionary::kNotFound) {
    // If we have a global object set the cell to the hole.
    if (IsGlobalObject()) {
      PropertyDetails details = dictionary->DetailsAt(entry);
      if (details.IsDontDelete()) {
        if (mode != FORCE_DELETION) return GetHeap()->false_value();
        // When forced to delete global properties, we have to make a
        // map change to invalidate any ICs that think they can load
        // from the DontDelete cell without checking if it contains
        // the hole value.
        Object* new_map;
        { MaybeObject* maybe_new_map = map()->CopyDropDescriptors();
          if (!maybe_new_map->ToObject(&new_map)) return maybe_new_map;
        }
        set_map(Map::cast(new_map));
      }
      JSGlobalPropertyCell* cell =
          JSGlobalPropertyCell::cast(dictionary->ValueAt(entry));
      cell->set_value(cell->GetHeap()->the_hole_value());
      dictionary->DetailsAtPut(entry, details.AsDeleted());
    } else {
      Object* deleted = dictionary->DeleteProperty(entry, mode);
      if (deleted == GetHeap()->true_value()) {
        FixedArray* new_properties = NULL;
        MaybeObject* maybe_properties = dictionary->Shrink(name);
        if (!maybe_properties->To(&new_properties)) {
          return maybe_properties;
        }
        set_properties(new_properties);
      }
      return deleted;
    }
  }
  return GetHeap()->true_value();
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
      || !HasFastElements()
      || !HasFastProperties();
}


Handle<Object> Object::GetProperty(Handle<Object> object,
                                   Handle<Object> receiver,
                                   LookupResult* result,
                                   Handle<String> key,
                                   PropertyAttributes* attributes) {
  Isolate* isolate = object->IsHeapObject()
      ? Handle<HeapObject>::cast(object)->GetIsolate()
      : Isolate::Current();
  CALL_HEAP_FUNCTION(
      isolate,
      object->GetProperty(*receiver, result, *key, attributes),
      Object);
}


MaybeObject* Object::GetProperty(Object* receiver,
                                 LookupResult* result,
                                 String* name,
                                 PropertyAttributes* attributes) {
  // Make sure that the top context does not change when doing
  // callbacks or interceptor calls.
  AssertNoContextChange ncc;
  Heap* heap = name->GetHeap();

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
    ASSERT(this != this->GetPrototype());
    for (Object* current = this; true; current = current->GetPrototype()) {
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
    case FIELD:
      value = result->holder()->FastPropertyAt(result->GetFieldIndex());
      ASSERT(!value->IsTheHole() || result->IsReadOnly());
      return value->IsTheHole() ? heap->undefined_value() : value;
    case CONSTANT_FUNCTION:
      return result->GetConstantFunction();
    case CALLBACKS:
      return result->holder()->GetPropertyWithCallback(
          receiver, result->GetCallbackObject(), name);
    case HANDLER:
      return result->proxy()->GetPropertyWithHandler(receiver, name);
    case INTERCEPTOR: {
      JSObject* recvr = JSObject::cast(receiver);
      return result->holder()->GetPropertyWithInterceptor(
          recvr, name, attributes);
    }
    case MAP_TRANSITION:
    case ELEMENTS_TRANSITION:
    case CONSTANT_TRANSITION:
    case NULL_DESCRIPTOR:
      break;
  }
  UNREACHABLE();
  return NULL;
}


MaybeObject* Object::GetElementWithReceiver(Object* receiver, uint32_t index) {
  Heap* heap = IsSmi()
      ? Isolate::Current()->heap()
      : HeapObject::cast(this)->GetHeap();
  Object* holder = this;

  // Iterate up the prototype chain until an element is found or the null
  // prototype is encountered.
  for (holder = this;
       holder != heap->null_value();
       holder = holder->GetPrototype()) {
    if (!holder->IsJSObject()) {
      Isolate* isolate = heap->isolate();
      Context* global_context = isolate->context()->global_context();
      if (holder->IsNumber()) {
        holder = global_context->number_function()->instance_prototype();
      } else if (holder->IsString()) {
        holder = global_context->string_function()->instance_prototype();
      } else if (holder->IsBoolean()) {
        holder = global_context->boolean_function()->instance_prototype();
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


Object* Object::GetPrototype() {
  if (IsSmi()) {
    Heap* heap = Isolate::Current()->heap();
    Context* context = heap->isolate()->context()->global_context();
    return context->number_function()->instance_prototype();
  }

  HeapObject* heap_object = HeapObject::cast(this);

  // The object is either a number, a string, a boolean,
  // a real JS object, or a Harmony proxy.
  if (heap_object->IsJSReceiver()) {
    return heap_object->map()->prototype();
  }
  Heap* heap = heap_object->GetHeap();
  Context* context = heap->isolate()->context()->global_context();

  if (heap_object->IsHeapNumber()) {
    return context->number_function()->instance_prototype();
  }
  if (heap_object->IsString()) {
    return context->string_function()->instance_prototype();
  }
  if (heap_object->IsBoolean()) {
    return context->boolean_function()->instance_prototype();
  } else {
    return heap->null_value();
  }
}


MaybeObject* Object::GetHash(CreationFlag flag) {
  // The object is either a number, a string, an odd-ball,
  // a real JS object, or a Harmony proxy.
  if (IsNumber()) {
    uint32_t hash = ComputeLongHash(double_to_uint64(Number()));
    return Smi::FromInt(hash & Smi::kMaxValue);
  }
  if (IsString()) {
    uint32_t hash = String::cast(this)->Hash();
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
  if (!IsHeapObject() || !other->IsHeapObject()) return false;

  // The object is either a number, a string, an odd-ball,
  // a real JS object, or a Harmony proxy.
  if (IsNumber() && other->IsNumber()) {
    double this_value = Number();
    double other_value = other->Number();
    return (this_value == other_value) ||
        (isnan(this_value) && isnan(other_value));
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
  if (!HEAP->IsAllocationAllowed()) return this;
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
      if (IsAsciiRepresentation()) {
        { MaybeObject* maybe_object = heap->AllocateRawAsciiString(len, tenure);
          if (!maybe_object->ToObject(&object)) return maybe_object;
        }
        result = String::cast(object);
        String* first = cs->first();
        int first_length = first->length();
        char* dest = SeqAsciiString::cast(result)->GetChars();
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
  bool is_ascii = this->IsAsciiRepresentation();
  bool is_symbol = this->IsSymbol();

  // Morph the object to an external string by adjusting the map and
  // reinitializing the fields.
  if (size >= ExternalString::kSize) {
    this->set_map_no_write_barrier(
        is_symbol
            ? (is_ascii ?  heap->external_symbol_with_ascii_data_map()
                        :  heap->external_symbol_map())
            : (is_ascii ?  heap->external_string_with_ascii_data_map()
                        :  heap->external_string_map()));
  } else {
    this->set_map_no_write_barrier(
        is_symbol
            ? (is_ascii ?  heap->short_external_symbol_with_ascii_data_map()
                        :  heap->short_external_symbol_map())
            : (is_ascii ?  heap->short_external_string_with_ascii_data_map()
                        :  heap->short_external_string_map()));
  }
  ExternalTwoByteString* self = ExternalTwoByteString::cast(this);
  self->set_resource(resource);
  if (is_symbol) self->Hash();  // Force regeneration of the hash value.

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
  bool is_symbol = this->IsSymbol();

  // Morph the object to an external string by adjusting the map and
  // reinitializing the fields.  Use short version if space is limited.
  if (size >= ExternalString::kSize) {
    this->set_map_no_write_barrier(
        is_symbol ? heap->external_ascii_symbol_map()
                  : heap->external_ascii_string_map());
  } else {
    this->set_map_no_write_barrier(
        is_symbol ? heap->short_external_ascii_symbol_map()
                  : heap->short_external_ascii_string_map());
  }
  ExternalAsciiString* self = ExternalAsciiString::cast(this);
  self->set_resource(resource);
  if (is_symbol) self->Hash();  // Force regeneration of the hash value.

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

  StringInputBuffer buf(this);

  bool truncated = false;
  if (len > kMaxShortPrintLength) {
    len = kMaxShortPrintLength;
    truncated = true;
  }
  bool ascii = true;
  for (int i = 0; i < len; i++) {
    int c = buf.GetNext();

    if (c < 32 || c >= 127) {
      ascii = false;
    }
  }
  buf.Reset(this);
  if (ascii) {
    accumulator->Add("<String[%u]: ", length());
    for (int i = 0; i < len; i++) {
      accumulator->Put(buf.GetNext());
    }
    accumulator->Put('>');
  } else {
    // Backslash indicates that the string contains control
    // characters and that backslashes are therefore escaped.
    accumulator->Add("<String[%u]\\: ", length());
    for (int i = 0; i < len; i++) {
      int c = buf.GetNext();
      if (c == '\n') {
        accumulator->Add("\\n");
      } else if (c == '\r') {
        accumulator->Add("\\r");
      } else if (c == '\\') {
        accumulator->Add("\\\\");
      } else if (c < 32 || c > 126) {
        accumulator->Add("\\x%02x", c);
      } else {
        accumulator->Put(c);
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
      double length = JSArray::cast(this)->length()->Number();
      accumulator->Add("<JS Array[%u]>", static_cast<uint32_t>(length));
      break;
    }
    case JS_WEAK_MAP_TYPE: {
      accumulator->Add("<JS WeakMap>");
      break;
    }
    case JS_REGEXP_TYPE: {
      accumulator->Add("<JS RegExp>");
      break;
    }
    case JS_FUNCTION_TYPE: {
      Object* fun_name = JSFunction::cast(this)->shared()->name();
      bool printed = false;
      if (fun_name->IsString()) {
        String* str = String::cast(fun_name);
        if (str->length() > 0) {
          accumulator->Add("<JS Function ");
          accumulator->Put(str);
          accumulator->Put('>');
          printed = true;
        }
      }
      if (!printed) {
        accumulator->Add("<JS Function>");
      }
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
    JavaScriptFrame::PrintTop(file, false, true);
    PrintF(file, " for ");
    ShortPrint(file);
    PrintF(file, " from ");
    from_elements->ShortPrint(file);
    PrintF(file, " to ");
    to_elements->ShortPrint(file);
    PrintF(file, "\n");
  }
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
    case SHARED_FUNCTION_INFO_TYPE:
      accumulator->Add("<SharedFunctionInfo>");
      break;
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
    case JS_GLOBAL_PROPERTY_CELL_TYPE:
      accumulator->Add("Cell for ");
      JSGlobalPropertyCell::cast(this)->value()->ShortPrint(accumulator);
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
        if ((type & kStringEncodingMask) == kAsciiStringTag) {
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
    case JS_VALUE_TYPE:
    case JS_DATE_TYPE:
    case JS_ARRAY_TYPE:
    case JS_SET_TYPE:
    case JS_MAP_TYPE:
    case JS_WEAK_MAP_TYPE:
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
    case JS_GLOBAL_PROPERTY_CELL_TYPE:
      JSGlobalPropertyCell::BodyDescriptor::IterateBody(this, v);
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
      SharedFunctionInfo* shared = reinterpret_cast<SharedFunctionInfo*>(this);
      shared->SharedFunctionInfoIterateBody(v);
      break;
    }

#define MAKE_STRUCT_CASE(NAME, Name, name) \
        case NAME##_TYPE:
      STRUCT_LIST(MAKE_STRUCT_CASE)
#undef MAKE_STRUCT_CASE
      StructBodyDescriptor::IterateBody(this, object_size, v);
      break;
    default:
      PrintF("Unknown type: %d\n", type);
      UNREACHABLE();
  }
}


Object* HeapNumber::HeapNumberToBoolean() {
  // NaN, +0, and -0 should return the false object
#if __BYTE_ORDER == __LITTLE_ENDIAN
  union IeeeDoubleLittleEndianArchType u;
#elif __BYTE_ORDER == __BIG_ENDIAN
  union IeeeDoubleBigEndianArchType u;
#endif
  u.d = value();
  if (u.bits.exp == 2047) {
    // Detect NaN for IEEE double precision floating point.
    if ((u.bits.man_low | u.bits.man_high) != 0)
      return GetHeap()->false_value();
  }
  if (u.bits.exp == 0) {
    // Detect +0, and -0 for IEEE double precision floating point.
    if ((u.bits.man_low | u.bits.man_high) == 0)
      return GetHeap()->false_value();
  }
  return GetHeap()->true_value();
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
    return GetHeap()->function_class_symbol();
  }
  if (map()->constructor()->IsJSFunction()) {
    JSFunction* constructor = JSFunction::cast(map()->constructor());
    return String::cast(constructor->shared()->instance_class_name());
  }
  // If the constructor is not present, return "Object".
  return GetHeap()->Object_symbol();
}


String* JSReceiver::constructor_name() {
  if (map()->constructor()->IsJSFunction()) {
    JSFunction* constructor = JSFunction::cast(map()->constructor());
    String* name = String::cast(constructor->shared()->name());
    if (name->length() > 0) return name;
    String* inferred_name = constructor->shared()->inferred_name();
    if (inferred_name->length() > 0) return inferred_name;
    Object* proto = GetPrototype();
    if (proto->IsJSObject()) return JSObject::cast(proto)->constructor_name();
  }
  // TODO(rossberg): what about proxies?
  // If the constructor is not present, return "Object".
  return GetHeap()->Object_symbol();
}


MaybeObject* JSObject::AddFastPropertyUsingMap(Map* new_map,
                                               String* name,
                                               Object* value) {
  int index = new_map->PropertyIndexFor(name);
  if (map()->unused_property_fields() == 0) {
    ASSERT(map()->unused_property_fields() == 0);
    int new_unused = new_map->unused_property_fields();
    Object* values;
    { MaybeObject* maybe_values =
          properties()->CopySize(properties()->length() + new_unused + 1);
      if (!maybe_values->ToObject(&values)) return maybe_values;
    }
    set_properties(FixedArray::cast(values));
  }
  set_map(new_map);
  return FastPropertyAtPut(index, value);
}


static bool IsIdentifier(UnicodeCache* cache,
                         unibrow::CharacterStream* buffer) {
  // Checks whether the buffer contains an identifier (no escape).
  if (!buffer->has_more()) return false;
  if (!cache->IsIdentifierStart(buffer->GetNext())) {
    return false;
  }
  while (buffer->has_more()) {
    if (!cache->IsIdentifierPart(buffer->GetNext())) {
      return false;
    }
  }
  return true;
}


MaybeObject* JSObject::AddFastProperty(String* name,
                                       Object* value,
                                       PropertyAttributes attributes) {
  ASSERT(!IsJSGlobalProxy());

  // Normalize the object if the name is an actual string (not the
  // hidden symbols) and is not a real identifier.
  Isolate* isolate = GetHeap()->isolate();
  StringInputBuffer buffer(name);
  if (!IsIdentifier(isolate->unicode_cache(), &buffer)
      && name != isolate->heap()->hidden_symbol()) {
    Object* obj;
    { MaybeObject* maybe_obj =
          NormalizeProperties(CLEAR_INOBJECT_PROPERTIES, 0);
      if (!maybe_obj->ToObject(&obj)) return maybe_obj;
    }
    return AddSlowProperty(name, value, attributes);
  }

  DescriptorArray* old_descriptors = map()->instance_descriptors();
  // Compute the new index for new field.
  int index = map()->NextFreePropertyIndex();

  // Allocate new instance descriptors with (name, index) added
  FieldDescriptor new_field(name, index, attributes);
  Object* new_descriptors;
  { MaybeObject* maybe_new_descriptors =
        old_descriptors->CopyInsert(&new_field, REMOVE_TRANSITIONS);
    if (!maybe_new_descriptors->ToObject(&new_descriptors)) {
      return maybe_new_descriptors;
    }
  }

  // Only allow map transition if the object isn't the global object and there
  // is not a transition for the name, or there's a transition for the name but
  // it's unrelated to properties.
  int descriptor_index = old_descriptors->Search(name);

  // Element transitions are stored in the descriptor for property "", which is
  // not a identifier and should have forced a switch to slow properties above.
  ASSERT(descriptor_index == DescriptorArray::kNotFound ||
      old_descriptors->GetType(descriptor_index) != ELEMENTS_TRANSITION);
  bool can_insert_transition = descriptor_index == DescriptorArray::kNotFound ||
      old_descriptors->GetType(descriptor_index) == ELEMENTS_TRANSITION;
  bool allow_map_transition =
      can_insert_transition &&
      (isolate->context()->global_context()->object_function()->map() != map());

  ASSERT(index < map()->inobject_properties() ||
         (index - map()->inobject_properties()) < properties()->length() ||
         map()->unused_property_fields() == 0);
  // Allocate a new map for the object.
  Object* r;
  { MaybeObject* maybe_r = map()->CopyDropDescriptors();
    if (!maybe_r->ToObject(&r)) return maybe_r;
  }
  Map* new_map = Map::cast(r);
  if (allow_map_transition) {
    // Allocate new instance descriptors for the old map with map transition.
    MapTransitionDescriptor d(name, Map::cast(new_map), attributes);
    Object* r;
    { MaybeObject* maybe_r = old_descriptors->CopyInsert(&d, KEEP_TRANSITIONS);
      if (!maybe_r->ToObject(&r)) return maybe_r;
    }
    old_descriptors = DescriptorArray::cast(r);
  }

  if (map()->unused_property_fields() == 0) {
    if (properties()->length() > MaxFastProperties()) {
      Object* obj;
      { MaybeObject* maybe_obj =
            NormalizeProperties(CLEAR_INOBJECT_PROPERTIES, 0);
        if (!maybe_obj->ToObject(&obj)) return maybe_obj;
      }
      return AddSlowProperty(name, value, attributes);
    }
    // Make room for the new value
    Object* values;
    { MaybeObject* maybe_values =
          properties()->CopySize(properties()->length() + kFieldsAdded);
      if (!maybe_values->ToObject(&values)) return maybe_values;
    }
    set_properties(FixedArray::cast(values));
    new_map->set_unused_property_fields(kFieldsAdded - 1);
  } else {
    new_map->set_unused_property_fields(map()->unused_property_fields() - 1);
  }
  // We have now allocated all the necessary objects.
  // All the changes can be applied at once, so they are atomic.
  map()->set_instance_descriptors(old_descriptors);
  new_map->set_instance_descriptors(DescriptorArray::cast(new_descriptors));
  set_map(new_map);
  return FastPropertyAtPut(index, value);
}


MaybeObject* JSObject::AddConstantFunctionProperty(
    String* name,
    JSFunction* function,
    PropertyAttributes attributes) {
  // Allocate new instance descriptors with (name, function) added
  ConstantFunctionDescriptor d(name, function, attributes);
  Object* new_descriptors;
  { MaybeObject* maybe_new_descriptors =
        map()->instance_descriptors()->CopyInsert(&d, REMOVE_TRANSITIONS);
    if (!maybe_new_descriptors->ToObject(&new_descriptors)) {
      return maybe_new_descriptors;
    }
  }

  // Allocate a new map for the object.
  Object* new_map;
  { MaybeObject* maybe_new_map = map()->CopyDropDescriptors();
    if (!maybe_new_map->ToObject(&new_map)) return maybe_new_map;
  }

  DescriptorArray* descriptors = DescriptorArray::cast(new_descriptors);
  Map::cast(new_map)->set_instance_descriptors(descriptors);
  Map* old_map = map();
  set_map(Map::cast(new_map));

  // If the old map is the global object map (from new Object()),
  // then transitions are not added to it, so we are done.
  Heap* heap = GetHeap();
  if (old_map == heap->isolate()->context()->global_context()->
      object_function()->map()) {
    return function;
  }

  // Do not add CONSTANT_TRANSITIONS to global objects
  if (IsGlobalObject()) {
    return function;
  }

  // Add a CONSTANT_TRANSITION descriptor to the old map,
  // so future assignments to this property on other objects
  // of the same type will create a normal field, not a constant function.
  // Don't do this for special properties, with non-trival attributes.
  if (attributes != NONE) {
    return function;
  }
  ConstTransitionDescriptor mark(name, Map::cast(new_map));
  { MaybeObject* maybe_new_descriptors =
        old_map->instance_descriptors()->CopyInsert(&mark, KEEP_TRANSITIONS);
    if (!maybe_new_descriptors->ToObject(&new_descriptors)) {
      // We have accomplished the main goal, so return success.
      return function;
    }
  }
  old_map->set_instance_descriptors(DescriptorArray::cast(new_descriptors));

  return function;
}


// Add property in slow mode
MaybeObject* JSObject::AddSlowProperty(String* name,
                                       Object* value,
                                       PropertyAttributes attributes) {
  ASSERT(!HasFastProperties());
  StringDictionary* dict = property_dictionary();
  Object* store_value = value;
  if (IsGlobalObject()) {
    // In case name is an orphaned property reuse the cell.
    int entry = dict->FindEntry(name);
    if (entry != StringDictionary::kNotFound) {
      store_value = dict->ValueAt(entry);
      JSGlobalPropertyCell::cast(store_value)->set_value(value);
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
          heap->AllocateJSGlobalPropertyCell(value);
      if (!maybe_store_value->ToObject(&store_value)) return maybe_store_value;
    }
    JSGlobalPropertyCell::cast(store_value)->set_value(value);
  }
  PropertyDetails details = PropertyDetails(attributes, NORMAL);
  Object* result;
  { MaybeObject* maybe_result = dict->Add(name, store_value, details);
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }
  if (dict != result) set_properties(StringDictionary::cast(result));
  return value;
}


MaybeObject* JSObject::AddProperty(String* name,
                                   Object* value,
                                   PropertyAttributes attributes,
                                   StrictModeFlag strict_mode) {
  ASSERT(!IsJSGlobalProxy());
  Map* map_of_this = map();
  Heap* heap = GetHeap();
  if (!map_of_this->is_extensible()) {
    if (strict_mode == kNonStrictMode) {
      return value;
    } else {
      Handle<Object> args[1] = {Handle<String>(name)};
      return heap->isolate()->Throw(
          *FACTORY->NewTypeError("object_not_extensible",
                                 HandleVector(args, 1)));
    }
  }
  if (HasFastProperties()) {
    // Ensure the descriptor array does not get too big.
    if (map_of_this->instance_descriptors()->number_of_descriptors() <
        DescriptorArray::kMaxNumberOfDescriptors) {
      if (value->IsJSFunction()) {
        return AddConstantFunctionProperty(name,
                                           JSFunction::cast(value),
                                           attributes);
      } else {
        return AddFastProperty(name, value, attributes);
      }
    } else {
      // Normalize the object to prevent very large instance descriptors.
      // This eliminates unwanted N^2 allocation and lookup behavior.
      Object* obj;
      { MaybeObject* maybe_obj =
            NormalizeProperties(CLEAR_INOBJECT_PROPERTIES, 0);
        if (!maybe_obj->ToObject(&obj)) return maybe_obj;
      }
    }
  }
  return AddSlowProperty(name, value, attributes);
}


MaybeObject* JSObject::SetPropertyPostInterceptor(
    String* name,
    Object* value,
    PropertyAttributes attributes,
    StrictModeFlag strict_mode) {
  // Check local property, ignore interceptor.
  LookupResult result(GetIsolate());
  LocalLookupRealNamedProperty(name, &result);
  if (result.IsFound()) {
    // An existing property, a map transition or a null descriptor was
    // found.  Use set property to handle all these cases.
    return SetProperty(&result, name, value, attributes, strict_mode);
  }
  bool found = false;
  MaybeObject* result_object;
  result_object = SetPropertyWithCallbackSetterInPrototypes(name,
                                                            value,
                                                            attributes,
                                                            &found,
                                                            strict_mode);
  if (found) return result_object;
  // Add a new real property.
  return AddProperty(name, value, attributes, strict_mode);
}


MaybeObject* JSObject::ReplaceSlowProperty(String* name,
                                           Object* value,
                                           PropertyAttributes attributes) {
  StringDictionary* dictionary = property_dictionary();
  int old_index = dictionary->FindEntry(name);
  int new_enumeration_index = 0;  // 0 means "Use the next available index."
  if (old_index != -1) {
    // All calls to ReplaceSlowProperty have had all transitions removed.
    ASSERT(!dictionary->ContainsTransition(old_index));
    new_enumeration_index = dictionary->DetailsAt(old_index).index();
  }

  PropertyDetails new_details(attributes, NORMAL, new_enumeration_index);
  return SetNormalizedProperty(name, value, new_details);
}


MaybeObject* JSObject::ConvertDescriptorToFieldAndMapTransition(
    String* name,
    Object* new_value,
    PropertyAttributes attributes) {
  Map* old_map = map();
  Object* result;
  { MaybeObject* maybe_result =
        ConvertDescriptorToField(name, new_value, attributes);
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }
  // If we get to this point we have succeeded - do not return failure
  // after this point.  Later stuff is optional.
  if (!HasFastProperties()) {
    return result;
  }
  // Do not add transitions to the map of "new Object()".
  if (map() == GetIsolate()->context()->global_context()->
      object_function()->map()) {
    return result;
  }

  MapTransitionDescriptor transition(name,
                                     map(),
                                     attributes);
  Object* new_descriptors;
  { MaybeObject* maybe_new_descriptors = old_map->instance_descriptors()->
        CopyInsert(&transition, KEEP_TRANSITIONS);
    if (!maybe_new_descriptors->ToObject(&new_descriptors)) {
      return result;  // Yes, return _result_.
    }
  }
  old_map->set_instance_descriptors(DescriptorArray::cast(new_descriptors));
  return result;
}


MaybeObject* JSObject::ConvertDescriptorToField(String* name,
                                                Object* new_value,
                                                PropertyAttributes attributes) {
  if (map()->unused_property_fields() == 0 &&
      properties()->length() > MaxFastProperties()) {
    Object* obj;
    { MaybeObject* maybe_obj =
          NormalizeProperties(CLEAR_INOBJECT_PROPERTIES, 0);
      if (!maybe_obj->ToObject(&obj)) return maybe_obj;
    }
    return ReplaceSlowProperty(name, new_value, attributes);
  }

  int index = map()->NextFreePropertyIndex();
  FieldDescriptor new_field(name, index, attributes);
  // Make a new DescriptorArray replacing an entry with FieldDescriptor.
  Object* descriptors_unchecked;
  { MaybeObject* maybe_descriptors_unchecked = map()->instance_descriptors()->
                                  CopyInsert(&new_field, REMOVE_TRANSITIONS);
    if (!maybe_descriptors_unchecked->ToObject(&descriptors_unchecked)) {
      return maybe_descriptors_unchecked;
    }
  }
  DescriptorArray* new_descriptors =
      DescriptorArray::cast(descriptors_unchecked);

  // Make a new map for the object.
  Object* new_map_unchecked;
  { MaybeObject* maybe_new_map_unchecked = map()->CopyDropDescriptors();
    if (!maybe_new_map_unchecked->ToObject(&new_map_unchecked)) {
      return maybe_new_map_unchecked;
    }
  }
  Map* new_map = Map::cast(new_map_unchecked);
  new_map->set_instance_descriptors(new_descriptors);

  // Make new properties array if necessary.
  FixedArray* new_properties = 0;  // Will always be NULL or a valid pointer.
  int new_unused_property_fields = map()->unused_property_fields() - 1;
  if (map()->unused_property_fields() == 0) {
    new_unused_property_fields = kFieldsAdded - 1;
    Object* new_properties_object;
    { MaybeObject* maybe_new_properties_object =
          properties()->CopySize(properties()->length() + kFieldsAdded);
      if (!maybe_new_properties_object->ToObject(&new_properties_object)) {
        return maybe_new_properties_object;
      }
    }
    new_properties = FixedArray::cast(new_properties_object);
  }

  // Update pointers to commit changes.
  // Object points to the new map.
  new_map->set_unused_property_fields(new_unused_property_fields);
  set_map(new_map);
  if (new_properties) {
    set_properties(FixedArray::cast(new_properties));
  }
  return FastPropertyAtPut(index, new_value);
}



MaybeObject* JSObject::SetPropertyWithInterceptor(
    String* name,
    Object* value,
    PropertyAttributes attributes,
    StrictModeFlag strict_mode) {
  Isolate* isolate = GetIsolate();
  HandleScope scope(isolate);
  Handle<JSObject> this_handle(this);
  Handle<String> name_handle(name);
  Handle<Object> value_handle(value, isolate);
  Handle<InterceptorInfo> interceptor(GetNamedInterceptor());
  if (!interceptor->setter()->IsUndefined()) {
    LOG(isolate, ApiNamedPropertyAccess("interceptor-named-set", this, name));
    CustomArguments args(isolate, interceptor->data(), this, this);
    v8::AccessorInfo info(args.end());
    v8::NamedPropertySetter setter =
        v8::ToCData<v8::NamedPropertySetter>(interceptor->setter());
    v8::Handle<v8::Value> result;
    {
      // Leaving JavaScript.
      VMState state(isolate, EXTERNAL);
      Handle<Object> value_unhole(value->IsTheHole() ?
                                  isolate->heap()->undefined_value() :
                                  value,
                                  isolate);
      result = setter(v8::Utils::ToLocal(name_handle),
                      v8::Utils::ToLocal(value_unhole),
                      info);
    }
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
                                       Handle<String> key,
                                       Handle<Object> value,
                                       PropertyAttributes attributes,
                                       StrictModeFlag strict_mode) {
  CALL_HEAP_FUNCTION(object->GetIsolate(),
                     object->SetProperty(*key, *value, attributes, strict_mode),
                     Object);
}


MaybeObject* JSReceiver::SetProperty(String* name,
                                     Object* value,
                                     PropertyAttributes attributes,
                                     StrictModeFlag strict_mode) {
  LookupResult result(GetIsolate());
  LocalLookup(name, &result);
  return SetProperty(&result, name, value, attributes, strict_mode);
}


MaybeObject* JSObject::SetPropertyWithCallback(Object* structure,
                                               String* name,
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
    MaybeObject* obj = (callback->setter)(this,  value, callback->data);
    RETURN_IF_SCHEDULED_EXCEPTION(isolate);
    if (obj->IsFailure()) return obj;
    return *value_handle;
  }

  if (structure->IsAccessorInfo()) {
    // api style callbacks
    AccessorInfo* data = AccessorInfo::cast(structure);
    Object* call_obj = data->setter();
    v8::AccessorSetter call_fun = v8::ToCData<v8::AccessorSetter>(call_obj);
    if (call_fun == NULL) return value;
    Handle<String> key(name);
    LOG(isolate, ApiNamedPropertyAccess("store", this, name));
    CustomArguments args(isolate, data->data(), this, JSObject::cast(holder));
    v8::AccessorInfo info(args.end());
    {
      // Leaving JavaScript.
      VMState state(isolate, EXTERNAL);
      call_fun(v8::Utils::ToLocal(key),
               v8::Utils::ToLocal(value_handle),
               info);
    }
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
      Handle<String> key(name);
      Handle<Object> holder_handle(holder, isolate);
      Handle<Object> args[2] = { key, holder_handle };
      return isolate->Throw(
          *isolate->factory()->NewTypeError("no_setter_in_callback",
                                            HandleVector(args, 2)));
    }
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
  Execution::Call(fun, self, ARRAY_SIZE(argv), argv, &has_pending_exception);
  // Check for pending exception and return the result.
  if (has_pending_exception) return Failure::Exception();
  return *value_handle;
}


void JSObject::LookupCallbackSetterInPrototypes(String* name,
                                                LookupResult* result) {
  Heap* heap = GetHeap();
  for (Object* pt = GetPrototype();
       pt != heap->null_value();
       pt = pt->GetPrototype()) {
    if (pt->IsJSProxy()) {
      return result->HandlerResult(JSProxy::cast(pt));
    }
    JSObject::cast(pt)->LocalLookupRealNamedProperty(name, result);
    if (result->IsProperty()) {
      if (result->type() == CALLBACKS && !result->IsReadOnly()) return;
      // Found non-callback or read-only callback, stop looking.
      break;
    }
  }
  result->NotFound();
}


MaybeObject* JSObject::SetElementWithCallbackSetterInPrototypes(
    uint32_t index,
    Object* value,
    bool* found,
    StrictModeFlag strict_mode) {
  Heap* heap = GetHeap();
  for (Object* pt = GetPrototype();
       pt != heap->null_value();
       pt = pt->GetPrototype()) {
    if (pt->IsJSProxy()) {
      String* name;
      MaybeObject* maybe = GetHeap()->Uint32ToString(index);
      if (!maybe->To<String>(&name)) {
        *found = true;  // Force abort
        return maybe;
      }
      return JSProxy::cast(pt)->SetPropertyWithHandlerIfDefiningSetter(
          name, value, NONE, strict_mode, found);
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

MaybeObject* JSObject::SetPropertyWithCallbackSetterInPrototypes(
    String* name,
    Object* value,
    PropertyAttributes attributes,
    bool* found,
    StrictModeFlag strict_mode) {
  Heap* heap = GetHeap();
  // We could not find a local property so let's check whether there is an
  // accessor that wants to handle the property.
  LookupResult accessor_result(heap->isolate());
  LookupCallbackSetterInPrototypes(name, &accessor_result);
  if (accessor_result.IsFound()) {
    *found = true;
    if (accessor_result.type() == CALLBACKS) {
      return SetPropertyWithCallback(accessor_result.GetCallbackObject(),
                                     name,
                                     value,
                                     accessor_result.holder(),
                                     strict_mode);
    } else if (accessor_result.type() == HANDLER) {
      // There is a proxy in the prototype chain. Invoke its
      // getPropertyDescriptor trap.
      bool found = false;
      // SetPropertyWithHandlerIfDefiningSetter can cause GC,
      // make sure to use the handlified references after calling
      // the function.
      Handle<JSObject> self(this);
      Handle<String> hname(name);
      Handle<Object> hvalue(value);
      MaybeObject* result =
          accessor_result.proxy()->SetPropertyWithHandlerIfDefiningSetter(
              name, value, attributes, strict_mode, &found);
      if (found) return result;
      // The proxy does not define the property as an accessor.
      // Consequently, it has no effect on setting the receiver.
      return self->AddProperty(*hname, *hvalue, attributes, strict_mode);
    }
  }
  *found = false;
  return heap->the_hole_value();
}


void JSObject::LookupInDescriptor(String* name, LookupResult* result) {
  DescriptorArray* descriptors = map()->instance_descriptors();
  int number = descriptors->SearchWithCache(name);
  if (number != DescriptorArray::kNotFound) {
    result->DescriptorResult(this, descriptors->GetDetails(number), number);
  } else {
    result->NotFound();
  }
}


void Map::LookupInDescriptors(JSObject* holder,
                              String* name,
                              LookupResult* result) {
  DescriptorArray* descriptors = instance_descriptors();
  DescriptorLookupCache* cache =
      GetHeap()->isolate()->descriptor_lookup_cache();
  int number = cache->Lookup(descriptors, name);
  if (number == DescriptorLookupCache::kAbsent) {
    number = descriptors->Search(name);
    cache->Update(descriptors, name, number);
  }
  if (number != DescriptorArray::kNotFound) {
    result->DescriptorResult(holder, descriptors->GetDetails(number), number);
  } else {
    result->NotFound();
  }
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
  ElementsKind elms_kind = elements_kind();
  if (elms_kind == FAST_DOUBLE_ELEMENTS) {
    bool dummy = true;
    Handle<Map> fast_map =
        MaybeNull(LookupElementsTransitionMap(FAST_ELEMENTS, &dummy));
    if (!fast_map.is_null() && ContainsMap(candidates, fast_map)) {
      return fast_map;
    }
    return Handle<Map>::null();
  }
  if (elms_kind == FAST_SMI_ONLY_ELEMENTS) {
    bool dummy = true;
    Handle<Map> double_map =
        MaybeNull(LookupElementsTransitionMap(FAST_DOUBLE_ELEMENTS, &dummy));
    // In the current implementation, if the DOUBLE map doesn't exist, the
    // FAST map can't exist either.
    if (double_map.is_null()) return Handle<Map>::null();
    Handle<Map> fast_map =
        MaybeNull(double_map->LookupElementsTransitionMap(FAST_ELEMENTS,
                                                          &dummy));
    if (!fast_map.is_null() && ContainsMap(candidates, fast_map)) {
      return fast_map;
    }
    if (ContainsMap(candidates, double_map)) return double_map;
  }
  return Handle<Map>::null();
}

static Map* GetElementsTransitionMapFromDescriptor(Object* descriptor_contents,
                                                   ElementsKind elements_kind) {
  if (descriptor_contents->IsMap()) {
    Map* map = Map::cast(descriptor_contents);
    if (map->elements_kind() == elements_kind) {
      return map;
    }
    return NULL;
  }

  FixedArray* map_array = FixedArray::cast(descriptor_contents);
  for (int i = 0; i < map_array->length(); ++i) {
    Object* current = map_array->get(i);
    // Skip undefined slots, they are sentinels for reclaimed maps.
    if (!current->IsUndefined()) {
      Map* current_map = Map::cast(map_array->get(i));
      if (current_map->elements_kind() == elements_kind) {
        return current_map;
      }
    }
  }

  return NULL;
}


static MaybeObject* AddElementsTransitionMapToDescriptor(
    Object* descriptor_contents,
    Map* new_map) {
  // Nothing was in the descriptor for an ELEMENTS_TRANSITION,
  // simply add the map.
  if (descriptor_contents == NULL) {
    return new_map;
  }

  // There was already a map in the descriptor, create a 2-element FixedArray
  // to contain the existing map plus the new one.
  FixedArray* new_array;
  Heap* heap = new_map->GetHeap();
  if (descriptor_contents->IsMap()) {
    // Must tenure, DescriptorArray expects no new-space objects.
    MaybeObject* maybe_new_array = heap->AllocateFixedArray(2, TENURED);
    if (!maybe_new_array->To<FixedArray>(&new_array)) {
      return maybe_new_array;
    }
    new_array->set(0, descriptor_contents);
    new_array->set(1, new_map);
    return new_array;
  }

  // The descriptor already contained a list of maps for different ElementKinds
  // of ELEMENTS_TRANSITION, first check the existing array for an undefined
  // slot, and if that's not available, create a FixedArray to hold the existing
  // maps plus the new one and fill it in.
  FixedArray* array = FixedArray::cast(descriptor_contents);
  for (int i = 0; i < array->length(); ++i) {
    if (array->get(i)->IsUndefined()) {
      array->set(i, new_map);
      return array;
    }
  }

  // Must tenure, DescriptorArray expects no new-space objects.
  MaybeObject* maybe_new_array =
      heap->AllocateFixedArray(array->length() + 1, TENURED);
  if (!maybe_new_array->To<FixedArray>(&new_array)) {
    return maybe_new_array;
  }
  int i = 0;
  while (i < array->length()) {
    new_array->set(i, array->get(i));
    ++i;
  }
  new_array->set(i, new_map);
  return new_array;
}


String* Map::elements_transition_sentinel_name() {
  return GetHeap()->empty_symbol();
}


Object* Map::GetDescriptorContents(String* sentinel_name,
                                   bool* safe_to_add_transition) {
  // Get the cached index for the descriptors lookup, or find and cache it.
  DescriptorArray* descriptors = instance_descriptors();
  DescriptorLookupCache* cache = GetIsolate()->descriptor_lookup_cache();
  int index = cache->Lookup(descriptors, sentinel_name);
  if (index == DescriptorLookupCache::kAbsent) {
    index = descriptors->Search(sentinel_name);
    cache->Update(descriptors, sentinel_name, index);
  }
  // If the transition already exists, return its descriptor.
  if (index != DescriptorArray::kNotFound) {
    PropertyDetails details(descriptors->GetDetails(index));
    if (details.type() == ELEMENTS_TRANSITION) {
      return descriptors->GetValue(index);
    } else {
      if (safe_to_add_transition != NULL) {
        *safe_to_add_transition = false;
      }
    }
  }
  return NULL;
}


Map* Map::LookupElementsTransitionMap(ElementsKind elements_kind,
                                      bool* safe_to_add_transition) {
  // Special case: indirect SMI->FAST transition (cf. comment in
  // AddElementsTransition()).
  if (this->elements_kind() == FAST_SMI_ONLY_ELEMENTS &&
      elements_kind == FAST_ELEMENTS) {
    Map* double_map = this->LookupElementsTransitionMap(FAST_DOUBLE_ELEMENTS,
                                                        safe_to_add_transition);
    if (double_map == NULL) return double_map;
    return double_map->LookupElementsTransitionMap(FAST_ELEMENTS,
                                                   safe_to_add_transition);
  }
  Object* descriptor_contents = GetDescriptorContents(
      elements_transition_sentinel_name(), safe_to_add_transition);
  if (descriptor_contents != NULL) {
    Map* maybe_transition_map =
        GetElementsTransitionMapFromDescriptor(descriptor_contents,
                                               elements_kind);
    ASSERT(maybe_transition_map == NULL || maybe_transition_map->IsMap());
    return maybe_transition_map;
  }
  return NULL;
}


MaybeObject* Map::AddElementsTransition(ElementsKind elements_kind,
                                        Map* transitioned_map) {
  // The map transition graph should be a tree, therefore the transition
  // from SMI to FAST elements is not done directly, but by going through
  // DOUBLE elements first.
  if (this->elements_kind() == FAST_SMI_ONLY_ELEMENTS &&
      elements_kind == FAST_ELEMENTS) {
    bool safe_to_add = true;
    Map* double_map = this->LookupElementsTransitionMap(
        FAST_DOUBLE_ELEMENTS, &safe_to_add);
    // This method is only called when safe_to_add_transition has been found
    // to be true earlier.
    ASSERT(safe_to_add);

    if (double_map == NULL) {
      MaybeObject* maybe_map = this->CopyDropTransitions();
      if (!maybe_map->To(&double_map)) return maybe_map;
      double_map->set_elements_kind(FAST_DOUBLE_ELEMENTS);
      MaybeObject* maybe_double_transition = this->AddElementsTransition(
          FAST_DOUBLE_ELEMENTS, double_map);
      if (maybe_double_transition->IsFailure()) return maybe_double_transition;
    }
    return double_map->AddElementsTransition(FAST_ELEMENTS, transitioned_map);
  }

  bool safe_to_add_transition = true;
  Object* descriptor_contents = GetDescriptorContents(
      elements_transition_sentinel_name(), &safe_to_add_transition);
  // This method is only called when safe_to_add_transition has been found
  // to be true earlier.
  ASSERT(safe_to_add_transition);
  MaybeObject* maybe_new_contents =
      AddElementsTransitionMapToDescriptor(descriptor_contents,
                                           transitioned_map);
  Object* new_contents;
  if (!maybe_new_contents->ToObject(&new_contents)) {
    return maybe_new_contents;
  }

  ElementsTransitionDescriptor desc(elements_transition_sentinel_name(),
                                    new_contents);
  Object* new_descriptors;
  MaybeObject* maybe_new_descriptors =
      instance_descriptors()->CopyInsert(&desc, KEEP_TRANSITIONS);
  if (!maybe_new_descriptors->ToObject(&new_descriptors)) {
    return maybe_new_descriptors;
  }
  set_instance_descriptors(DescriptorArray::cast(new_descriptors));
  return this;
}


Handle<Map> JSObject::GetElementsTransitionMap(Handle<JSObject> object,
                                               ElementsKind to_kind) {
  Isolate* isolate = object->GetIsolate();
  CALL_HEAP_FUNCTION(isolate,
                     object->GetElementsTransitionMap(isolate, to_kind),
                     Map);
}


MaybeObject* JSObject::GetElementsTransitionMapSlow(ElementsKind to_kind) {
  Map* current_map = map();
  ElementsKind from_kind = current_map->elements_kind();

  if (from_kind == to_kind) return current_map;

  // Only objects with FastProperties can have DescriptorArrays and can track
  // element-related maps. Also don't add descriptors to maps that are shared.
  bool safe_to_add_transition = HasFastProperties() &&
      !current_map->IsUndefined() &&
      !current_map->is_shared();

  // Prevent long chains of DICTIONARY -> FAST_ELEMENTS maps caused by objects
  // with elements that switch back and forth between dictionary and fast
  // element mode.
  if (from_kind == DICTIONARY_ELEMENTS && to_kind == FAST_ELEMENTS) {
    safe_to_add_transition = false;
  }

  if (safe_to_add_transition) {
    // It's only safe to manipulate the descriptor array if it would be
    // safe to add a transition.
    Map* maybe_transition_map = current_map->LookupElementsTransitionMap(
        to_kind, &safe_to_add_transition);
    if (maybe_transition_map != NULL) {
      return maybe_transition_map;
    }
  }

  Map* new_map = NULL;

  // No transition to an existing map for the given ElementsKind. Make a new
  // one.
  { MaybeObject* maybe_map = current_map->CopyDropTransitions();
    if (!maybe_map->To(&new_map)) return maybe_map;
  }

  new_map->set_elements_kind(to_kind);

  // Only remember the map transition if the object's map is NOT equal to the
  // global object_function's map and there is not an already existing
  // non-matching element transition.
  Context* global_context = GetIsolate()->context()->global_context();
  bool allow_map_transition = safe_to_add_transition &&
      (global_context->object_function()->map() != map());
  if (allow_map_transition) {
    MaybeObject* maybe_transition =
        current_map->AddElementsTransition(to_kind, new_map);
    if (maybe_transition->IsFailure()) return maybe_transition;
  }
  return new_map;
}


void JSObject::LocalLookupRealNamedProperty(String* name,
                                            LookupResult* result) {
  if (IsJSGlobalProxy()) {
    Object* proto = GetPrototype();
    if (proto->IsNull()) return result->NotFound();
    ASSERT(proto->IsJSGlobalObject());
    // A GlobalProxy's prototype should always be a proper JSObject.
    return JSObject::cast(proto)->LocalLookupRealNamedProperty(name, result);
  }

  if (HasFastProperties()) {
    LookupInDescriptor(name, result);
    if (result->IsFound()) {
      // A property, a map transition or a null descriptor was found.
      // We return all of these result types because
      // LocalLookupRealNamedProperty is used when setting properties
      // where map transitions and null descriptors are handled.
      ASSERT(result->holder() == this && result->type() != NORMAL);
      // Disallow caching for uninitialized constants. These can only
      // occur as fields.
      if (result->IsReadOnly() && result->type() == FIELD &&
          FastPropertyAt(result->GetFieldIndex())->IsTheHole()) {
        result->DisallowCaching();
      }
      return;
    }
  } else {
    int entry = property_dictionary()->FindEntry(name);
    if (entry != StringDictionary::kNotFound) {
      Object* value = property_dictionary()->ValueAt(entry);
      if (IsGlobalObject()) {
        PropertyDetails d = property_dictionary()->DetailsAt(entry);
        if (d.IsDeleted()) {
          result->NotFound();
          return;
        }
        value = JSGlobalPropertyCell::cast(value)->value();
      }
      // Make sure to disallow caching for uninitialized constants
      // found in the dictionary-mode objects.
      if (value->IsTheHole()) result->DisallowCaching();
      result->DictionaryResult(this, entry);
      return;
    }
  }
  result->NotFound();
}


void JSObject::LookupRealNamedProperty(String* name, LookupResult* result) {
  LocalLookupRealNamedProperty(name, result);
  if (result->IsProperty()) return;

  LookupRealNamedPropertyInPrototypes(name, result);
}


void JSObject::LookupRealNamedPropertyInPrototypes(String* name,
                                                   LookupResult* result) {
  Heap* heap = GetHeap();
  for (Object* pt = GetPrototype();
       pt != heap->null_value();
       pt = JSObject::cast(pt)->GetPrototype()) {
    JSObject::cast(pt)->LocalLookupRealNamedProperty(name, result);
    if (result->IsProperty() && (result->type() != INTERCEPTOR)) return;
  }
  result->NotFound();
}


// We only need to deal with CALLBACKS and INTERCEPTORS
MaybeObject* JSObject::SetPropertyWithFailedAccessCheck(
    LookupResult* result,
    String* name,
    Object* value,
    bool check_prototype,
    StrictModeFlag strict_mode) {
  if (check_prototype && !result->IsProperty()) {
    LookupCallbackSetterInPrototypes(name, result);
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
  Handle<Object> value_handle(value);
  isolate->ReportFailedAccessCheck(this, v8::ACCESS_SET);
  return *value_handle;
}


MaybeObject* JSReceiver::SetProperty(LookupResult* result,
                                     String* key,
                                     Object* value,
                                     PropertyAttributes attributes,
                                     StrictModeFlag strict_mode) {
  if (result->IsFound() && result->type() == HANDLER) {
    return result->proxy()->SetPropertyWithHandler(
        key, value, attributes, strict_mode);
  } else {
    return JSObject::cast(this)->SetPropertyForResult(
        result, key, value, attributes, strict_mode);
  }
}


bool JSProxy::HasPropertyWithHandler(String* name_raw) {
  Isolate* isolate = GetIsolate();
  HandleScope scope(isolate);
  Handle<Object> receiver(this);
  Handle<Object> name(name_raw);

  Handle<Object> args[] = { name };
  Handle<Object> result = CallTrap(
    "has", isolate->derived_has_trap(), ARRAY_SIZE(args), args);
  if (isolate->has_pending_exception()) return Failure::Exception();

  return result->ToBoolean()->IsTrue();
}


MUST_USE_RESULT MaybeObject* JSProxy::SetPropertyWithHandler(
    String* name_raw,
    Object* value_raw,
    PropertyAttributes attributes,
    StrictModeFlag strict_mode) {
  Isolate* isolate = GetIsolate();
  HandleScope scope(isolate);
  Handle<Object> receiver(this);
  Handle<Object> name(name_raw);
  Handle<Object> value(value_raw);

  Handle<Object> args[] = { receiver, name, value };
  CallTrap("set", isolate->derived_set_trap(), ARRAY_SIZE(args), args);
  if (isolate->has_pending_exception()) return Failure::Exception();

  return *value;
}


MUST_USE_RESULT MaybeObject* JSProxy::SetPropertyWithHandlerIfDefiningSetter(
    String* name_raw,
    Object* value_raw,
    PropertyAttributes attributes,
    StrictModeFlag strict_mode,
    bool* found) {
  *found = true;  // except where defined otherwise...
  Isolate* isolate = GetHeap()->isolate();
  Handle<JSProxy> proxy(this);
  Handle<Object> handler(this->handler());  // Trap might morph proxy.
  Handle<String> name(name_raw);
  Handle<Object> value(value_raw);
  Handle<Object> args[] = { name };
  Handle<Object> result = proxy->CallTrap(
      "getPropertyDescriptor", Handle<Object>(), ARRAY_SIZE(args), args);
  if (isolate->has_pending_exception()) return Failure::Exception();

  if (!result->IsUndefined()) {
    // The proxy handler cares about this property.
    // Check whether it is virtualized as an accessor.
    // Emulate [[GetProperty]] semantics for proxies.
    bool has_pending_exception;
    Handle<Object> argv[] = { result };
    Handle<Object> desc =
        Execution::Call(isolate->to_complete_property_descriptor(), result,
                        ARRAY_SIZE(argv), argv, &has_pending_exception);
    if (has_pending_exception) return Failure::Exception();

    Handle<String> conf_name =
        isolate->factory()->LookupAsciiSymbol("configurable_");
    Handle<Object> configurable(v8::internal::GetProperty(desc, conf_name));
    ASSERT(!isolate->has_pending_exception());
    if (configurable->IsFalse()) {
      Handle<String> trap =
          isolate->factory()->LookupAsciiSymbol("getPropertyDescriptor");
      Handle<Object> args[] = { handler, trap, name };
      Handle<Object> error = isolate->factory()->NewTypeError(
          "proxy_prop_not_configurable", HandleVector(args, ARRAY_SIZE(args)));
      return isolate->Throw(*error);
    }
    ASSERT(configurable->IsTrue());

    // Check for AccessorDescriptor.
    Handle<String> set_name = isolate->factory()->LookupAsciiSymbol("set_");
    Handle<Object> setter(v8::internal::GetProperty(desc, set_name));
    ASSERT(!isolate->has_pending_exception());
    if (!setter->IsUndefined()) {
      // We have a setter -- invoke it.
      // TODO(rossberg): nicer would be to cast to some JSCallable here...
      return proxy->SetPropertyWithDefinedSetter(
          JSReceiver::cast(*setter), *value);
    } else {
      Handle<String> get_name = isolate->factory()->LookupAsciiSymbol("get_");
      Handle<Object> getter(v8::internal::GetProperty(desc, get_name));
      ASSERT(!isolate->has_pending_exception());
      if (!getter->IsUndefined()) {
        // We have a getter but no setter -- the property may not be
        // written. In strict mode, throw an error.
        if (strict_mode == kNonStrictMode) return *value;
        Handle<Object> args[] = { name, proxy };
        Handle<Object> error = isolate->factory()->NewTypeError(
            "no_setter_in_callback", HandleVector(args, ARRAY_SIZE(args)));
        return isolate->Throw(*error);
      }
    }
    // Fall-through.
  }

  // The proxy does not define the property as an accessor.
  *found = false;
  return *value;
}


MUST_USE_RESULT MaybeObject* JSProxy::DeletePropertyWithHandler(
    String* name_raw, DeleteMode mode) {
  Isolate* isolate = GetIsolate();
  HandleScope scope(isolate);
  Handle<Object> receiver(this);
  Handle<Object> name(name_raw);

  Handle<Object> args[] = { name };
  Handle<Object> result = CallTrap(
    "delete", Handle<Object>(), ARRAY_SIZE(args), args);
  if (isolate->has_pending_exception()) return Failure::Exception();

  Object* bool_result = result->ToBoolean();
  if (mode == STRICT_DELETION && bool_result == GetHeap()->false_value()) {
    Handle<String> trap_name = isolate->factory()->LookupAsciiSymbol("delete");
    Handle<Object> args[] = { Handle<Object>(handler()), trap_name };
    Handle<Object> error = isolate->factory()->NewTypeError(
        "handler_failed", HandleVector(args, ARRAY_SIZE(args)));
    isolate->Throw(*error);
    return Failure::Exception();
  }
  return bool_result;
}


MUST_USE_RESULT MaybeObject* JSProxy::DeleteElementWithHandler(
    uint32_t index,
    DeleteMode mode) {
  Isolate* isolate = GetIsolate();
  HandleScope scope(isolate);
  Handle<String> name = isolate->factory()->Uint32ToString(index);
  return JSProxy::DeletePropertyWithHandler(*name, mode);
}


MUST_USE_RESULT PropertyAttributes JSProxy::GetPropertyAttributeWithHandler(
    JSReceiver* receiver_raw,
    String* name_raw) {
  Isolate* isolate = GetIsolate();
  HandleScope scope(isolate);
  Handle<JSProxy> proxy(this);
  Handle<Object> handler(this->handler());  // Trap might morph proxy.
  Handle<JSReceiver> receiver(receiver_raw);
  Handle<Object> name(name_raw);

  Handle<Object> args[] = { name };
  Handle<Object> result = CallTrap(
    "getPropertyDescriptor", Handle<Object>(), ARRAY_SIZE(args), args);
  if (isolate->has_pending_exception()) return NONE;

  if (result->IsUndefined()) return ABSENT;

  bool has_pending_exception;
  Handle<Object> argv[] = { result };
  Handle<Object> desc =
      Execution::Call(isolate->to_complete_property_descriptor(), result,
                      ARRAY_SIZE(argv), argv, &has_pending_exception);
  if (has_pending_exception) return NONE;

  // Convert result to PropertyAttributes.
  Handle<String> enum_n = isolate->factory()->LookupAsciiSymbol("enumerable");
  Handle<Object> enumerable(v8::internal::GetProperty(desc, enum_n));
  if (isolate->has_pending_exception()) return NONE;
  Handle<String> conf_n = isolate->factory()->LookupAsciiSymbol("configurable");
  Handle<Object> configurable(v8::internal::GetProperty(desc, conf_n));
  if (isolate->has_pending_exception()) return NONE;
  Handle<String> writ_n = isolate->factory()->LookupAsciiSymbol("writable");
  Handle<Object> writable(v8::internal::GetProperty(desc, writ_n));
  if (isolate->has_pending_exception()) return NONE;

  if (configurable->IsFalse()) {
    Handle<String> trap =
        isolate->factory()->LookupAsciiSymbol("getPropertyDescriptor");
    Handle<Object> args[] = { handler, trap, name };
    Handle<Object> error = isolate->factory()->NewTypeError(
        "proxy_prop_not_configurable", HandleVector(args, ARRAY_SIZE(args)));
    isolate->Throw(*error);
    return NONE;
  }

  int attributes = NONE;
  if (enumerable->ToBoolean()->IsFalse()) attributes |= DONT_ENUM;
  if (configurable->ToBoolean()->IsFalse()) attributes |= DONT_DELETE;
  if (writable->ToBoolean()->IsFalse()) attributes |= READ_ONLY;
  return static_cast<PropertyAttributes>(attributes);
}


MUST_USE_RESULT PropertyAttributes JSProxy::GetElementAttributeWithHandler(
    JSReceiver* receiver,
    uint32_t index) {
  Isolate* isolate = GetIsolate();
  HandleScope scope(isolate);
  Handle<String> name = isolate->factory()->Uint32ToString(index);
  return GetPropertyAttributeWithHandler(receiver, *name);
}


void JSProxy::Fix() {
  Isolate* isolate = GetIsolate();
  HandleScope scope(isolate);
  Handle<JSProxy> self(this);

  // Save identity hash.
  MaybeObject* maybe_hash = GetIdentityHash(OMIT_CREATION);

  if (IsJSFunctionProxy()) {
    isolate->factory()->BecomeJSFunction(self);
    // Code will be set on the JavaScript side.
  } else {
    isolate->factory()->BecomeJSObject(self);
  }
  ASSERT(self->IsJSObject());

  // Inherit identity, if it was present.
  Object* hash;
  if (maybe_hash->To<Object>(&hash) && hash->IsSmi()) {
    Handle<JSObject> new_self(JSObject::cast(*self));
    isolate->factory()->SetIdentityHash(new_self, hash);
  }
}


MUST_USE_RESULT Handle<Object> JSProxy::CallTrap(const char* name,
                                                 Handle<Object> derived,
                                                 int argc,
                                                 Handle<Object> argv[]) {
  Isolate* isolate = GetIsolate();
  Handle<Object> handler(this->handler());

  Handle<String> trap_name = isolate->factory()->LookupAsciiSymbol(name);
  Handle<Object> trap(v8::internal::GetProperty(handler, trap_name));
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
  return Execution::Call(trap, handler, argc, argv, &threw);
}


MaybeObject* JSObject::SetPropertyForResult(LookupResult* result,
                                            String* name,
                                            Object* value,
                                            PropertyAttributes attributes,
                                            StrictModeFlag strict_mode) {
  Heap* heap = GetHeap();
  // Make sure that the top context does not change when doing callbacks or
  // interceptor calls.
  AssertNoContextChange ncc;

  // Optimization for 2-byte strings often used as keys in a decompression
  // dictionary.  We make these short keys into symbols to avoid constantly
  // reallocating them.
  if (!name->IsSymbol() && name->length() <= 2) {
    Object* symbol_version;
    { MaybeObject* maybe_symbol_version = heap->LookupSymbol(name);
      if (maybe_symbol_version->ToObject(&symbol_version)) {
        name = String::cast(symbol_version);
      }
    }
  }

  // Check access rights if needed.
  if (IsAccessCheckNeeded()) {
    if (!heap->isolate()->MayNamedAccess(this, name, v8::ACCESS_SET)) {
      return SetPropertyWithFailedAccessCheck(
          result, name, value, true, strict_mode);
    }
  }

  if (IsJSGlobalProxy()) {
    Object* proto = GetPrototype();
    if (proto->IsNull()) return value;
    ASSERT(proto->IsJSGlobalObject());
    return JSObject::cast(proto)->SetPropertyForResult(
        result, name, value, attributes, strict_mode);
  }

  if (!result->IsProperty() && !IsJSContextExtensionObject()) {
    bool found = false;
    MaybeObject* result_object;
    result_object = SetPropertyWithCallbackSetterInPrototypes(name,
                                                              value,
                                                              attributes,
                                                              &found,
                                                              strict_mode);
    if (found) return result_object;
  }

  // At this point, no GC should have happened, as this would invalidate
  // 'result', which we cannot handlify!

  if (!result->IsFound()) {
    // Neither properties nor transitions found.
    return AddProperty(name, value, attributes, strict_mode);
  }
  if (result->IsReadOnly() && result->IsProperty()) {
    if (strict_mode == kStrictMode) {
      Handle<JSObject> self(this);
      Handle<String> hname(name);
      Handle<Object> args[] = { hname, self };
      return heap->isolate()->Throw(*heap->isolate()->factory()->NewTypeError(
          "strict_read_only_property", HandleVector(args, ARRAY_SIZE(args))));
    } else {
      return value;
    }
  }
  // This is a real property that is not read-only, or it is a
  // transition or null descriptor and there are no setters in the prototypes.
  switch (result->type()) {
    case NORMAL:
      return SetNormalizedProperty(result, value);
    case FIELD:
      return FastPropertyAtPut(result->GetFieldIndex(), value);
    case MAP_TRANSITION:
      if (attributes == result->GetAttributes()) {
        // Only use map transition if the attributes match.
        return AddFastPropertyUsingMap(result->GetTransitionMap(),
                                       name,
                                       value);
      }
      return ConvertDescriptorToField(name, value, attributes);
    case CONSTANT_FUNCTION:
      // Only replace the function if necessary.
      if (value == result->GetConstantFunction()) return value;
      // Preserve the attributes of this existing property.
      attributes = result->GetAttributes();
      return ConvertDescriptorToField(name, value, attributes);
    case CALLBACKS:
      return SetPropertyWithCallback(result->GetCallbackObject(),
                                     name,
                                     value,
                                     result->holder(),
                                     strict_mode);
    case INTERCEPTOR:
      return SetPropertyWithInterceptor(name, value, attributes, strict_mode);
    case CONSTANT_TRANSITION: {
      // If the same constant function is being added we can simply
      // transition to the target map.
      Map* target_map = result->GetTransitionMap();
      DescriptorArray* target_descriptors = target_map->instance_descriptors();
      int number = target_descriptors->SearchWithCache(name);
      ASSERT(number != DescriptorArray::kNotFound);
      ASSERT(target_descriptors->GetType(number) == CONSTANT_FUNCTION);
      JSFunction* function =
          JSFunction::cast(target_descriptors->GetValue(number));
      if (value == function) {
        set_map(target_map);
        return value;
      }
      // Otherwise, replace with a MAP_TRANSITION to a new map with a
      // FIELD, even if the value is a constant function.
      return ConvertDescriptorToFieldAndMapTransition(name, value, attributes);
    }
    case NULL_DESCRIPTOR:
    case ELEMENTS_TRANSITION:
      return ConvertDescriptorToFieldAndMapTransition(name, value, attributes);
    case HANDLER:
      UNREACHABLE();
      return value;
  }
  UNREACHABLE();  // keep the compiler happy
  return value;
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
    Handle<String> key,
    Handle<Object> value,
    PropertyAttributes attributes) {
  CALL_HEAP_FUNCTION(
    object->GetIsolate(),
    object->SetLocalPropertyIgnoreAttributes(*key, *value, attributes),
    Object);
}


MaybeObject* JSObject::SetLocalPropertyIgnoreAttributes(
    String* name,
    Object* value,
    PropertyAttributes attributes) {

  // Make sure that the top context does not change when doing callbacks or
  // interceptor calls.
  AssertNoContextChange ncc;
  Isolate* isolate = GetIsolate();
  LookupResult result(isolate);
  LocalLookup(name, &result);
  // Check access rights if needed.
  if (IsAccessCheckNeeded()) {
    if (!isolate->MayNamedAccess(this, name, v8::ACCESS_SET)) {
      return SetPropertyWithFailedAccessCheck(&result,
                                              name,
                                              value,
                                              false,
                                              kNonStrictMode);
    }
  }

  if (IsJSGlobalProxy()) {
    Object* proto = GetPrototype();
    if (proto->IsNull()) return value;
    ASSERT(proto->IsJSGlobalObject());
    return JSObject::cast(proto)->SetLocalPropertyIgnoreAttributes(
        name,
        value,
        attributes);
  }

  // Check for accessor in prototype chain removed here in clone.
  if (!result.IsFound()) {
    // Neither properties nor transitions found.
    return AddProperty(name, value, attributes, kNonStrictMode);
  }

  PropertyDetails details = PropertyDetails(attributes, NORMAL);

  // Check of IsReadOnly removed from here in clone.
  switch (result.type()) {
    case NORMAL:
      return SetNormalizedProperty(name, value, details);
    case FIELD:
      return FastPropertyAtPut(result.GetFieldIndex(), value);
    case MAP_TRANSITION:
      if (attributes == result.GetAttributes()) {
        // Only use map transition if the attributes match.
        return AddFastPropertyUsingMap(result.GetTransitionMap(),
                                       name,
                                       value);
      }
      return ConvertDescriptorToField(name, value, attributes);
    case CONSTANT_FUNCTION:
      // Only replace the function if necessary.
      if (value == result.GetConstantFunction()) return value;
      // Preserve the attributes of this existing property.
      attributes = result.GetAttributes();
      return ConvertDescriptorToField(name, value, attributes);
    case CALLBACKS:
    case INTERCEPTOR:
      // Override callback in clone
      return ConvertDescriptorToField(name, value, attributes);
    case CONSTANT_TRANSITION:
      // Replace with a MAP_TRANSITION to a new map with a FIELD, even
      // if the value is a function.
      return ConvertDescriptorToFieldAndMapTransition(name, value, attributes);
    case NULL_DESCRIPTOR:
    case ELEMENTS_TRANSITION:
      return ConvertDescriptorToFieldAndMapTransition(name, value, attributes);
    case HANDLER:
      UNREACHABLE();
      return value;
  }
  UNREACHABLE();  // keep the compiler happy
  return value;
}


PropertyAttributes JSObject::GetPropertyAttributePostInterceptor(
      JSObject* receiver,
      String* name,
      bool continue_search) {
  // Check local property, ignore interceptor.
  LookupResult result(GetIsolate());
  LocalLookupRealNamedProperty(name, &result);
  if (result.IsProperty()) return result.GetAttributes();

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
      String* name,
      bool continue_search) {
  Isolate* isolate = GetIsolate();

  // Make sure that the top context does not change when doing
  // callbacks or interceptor calls.
  AssertNoContextChange ncc;

  HandleScope scope(isolate);
  Handle<InterceptorInfo> interceptor(GetNamedInterceptor());
  Handle<JSObject> receiver_handle(receiver);
  Handle<JSObject> holder_handle(this);
  Handle<String> name_handle(name);
  CustomArguments args(isolate, interceptor->data(), receiver, this);
  v8::AccessorInfo info(args.end());
  if (!interceptor->query()->IsUndefined()) {
    v8::NamedPropertyQuery query =
        v8::ToCData<v8::NamedPropertyQuery>(interceptor->query());
    LOG(isolate,
        ApiNamedPropertyAccess("interceptor-named-has", *holder_handle, name));
    v8::Handle<v8::Integer> result;
    {
      // Leaving JavaScript.
      VMState state(isolate, EXTERNAL);
      result = query(v8::Utils::ToLocal(name_handle), info);
    }
    if (!result.IsEmpty()) {
      ASSERT(result->IsInt32());
      return static_cast<PropertyAttributes>(result->Int32Value());
    }
  } else if (!interceptor->getter()->IsUndefined()) {
    v8::NamedPropertyGetter getter =
        v8::ToCData<v8::NamedPropertyGetter>(interceptor->getter());
    LOG(isolate,
        ApiNamedPropertyAccess("interceptor-named-get-has", this, name));
    v8::Handle<v8::Value> result;
    {
      // Leaving JavaScript.
      VMState state(isolate, EXTERNAL);
      result = getter(v8::Utils::ToLocal(name_handle), info);
    }
    if (!result.IsEmpty()) return DONT_ENUM;
  }
  return holder_handle->GetPropertyAttributePostInterceptor(*receiver_handle,
                                                            *name_handle,
                                                            continue_search);
}


PropertyAttributes JSReceiver::GetPropertyAttributeWithReceiver(
      JSReceiver* receiver,
      String* key) {
  uint32_t index = 0;
  if (IsJSObject() && key->AsArrayIndex(&index)) {
    return JSObject::cast(this)->HasElementWithReceiver(receiver, index)
        ? NONE : ABSENT;
  }
  // Named property.
  LookupResult result(GetIsolate());
  Lookup(key, &result);
  return GetPropertyAttribute(receiver, &result, key, true);
}


PropertyAttributes JSReceiver::GetPropertyAttribute(JSReceiver* receiver,
                                                    LookupResult* result,
                                                    String* name,
                                                    bool continue_search) {
  // Check access rights if needed.
  if (IsAccessCheckNeeded()) {
    JSObject* this_obj = JSObject::cast(this);
    Heap* heap = GetHeap();
    if (!heap->isolate()->MayNamedAccess(this_obj, name, v8::ACCESS_HAS)) {
      return this_obj->GetPropertyAttributeWithFailedAccessCheck(
          receiver, result, name, continue_search);
    }
  }
  if (result->IsProperty()) {
    switch (result->type()) {
      case NORMAL:  // fall through
      case FIELD:
      case CONSTANT_FUNCTION:
      case CALLBACKS:
        return result->GetAttributes();
      case HANDLER: {
        return JSProxy::cast(result->proxy())->GetPropertyAttributeWithHandler(
            receiver, name);
      }
      case INTERCEPTOR:
        return result->holder()->GetPropertyAttributeWithInterceptor(
            JSObject::cast(receiver), name, continue_search);
      default:
        UNREACHABLE();
    }
  }
  return ABSENT;
}


PropertyAttributes JSReceiver::GetLocalPropertyAttribute(String* name) {
  // Check whether the name is an array index.
  uint32_t index = 0;
  if (IsJSObject() && name->AsArrayIndex(&index)) {
    if (JSObject::cast(this)->HasLocalElement(index)) return NONE;
    return ABSENT;
  }
  // Named property.
  LookupResult result(GetIsolate());
  LocalLookup(name, &result);
  return GetPropertyAttribute(this, &result, name, false);
}


MaybeObject* NormalizedMapCache::Get(JSObject* obj,
                                     PropertyNormalizationMode mode) {
  Isolate* isolate = obj->GetIsolate();
  Map* fast = obj->map();
  int index = fast->Hash() % kEntries;
  Object* result = get(index);
  if (result->IsMap() &&
      Map::cast(result)->EquivalentToForNormalization(fast, mode)) {
#ifdef DEBUG
    if (FLAG_verify_heap) {
      Map::cast(result)->SharedMapVerify();
    }
    if (FLAG_enable_slow_asserts) {
      // The cached map should match newly created normalized map bit-by-bit.
      Object* fresh;
      { MaybeObject* maybe_fresh =
            fast->CopyNormalized(mode, SHARED_NORMALIZED_MAP);
        if (maybe_fresh->ToObject(&fresh)) {
          ASSERT(memcmp(Map::cast(fresh)->address(),
                        Map::cast(result)->address(),
                        Map::kSize) == 0);
        }
      }
    }
#endif
    return result;
  }

  { MaybeObject* maybe_result =
        fast->CopyNormalized(mode, SHARED_NORMALIZED_MAP);
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }
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


void JSObject::UpdateMapCodeCache(Handle<JSObject> object,
                                  Handle<String> name,
                                  Handle<Code> code) {
  Isolate* isolate = object->GetIsolate();
  CALL_HEAP_FUNCTION_VOID(isolate,
                          object->UpdateMapCodeCache(*name, *code));
}


MaybeObject* JSObject::UpdateMapCodeCache(String* name, Code* code) {
  if (map()->is_shared()) {
    // Fast case maps are never marked as shared.
    ASSERT(!HasFastProperties());
    // Replace the map with an identical copy that can be safely modified.
    Object* obj;
    { MaybeObject* maybe_obj = map()->CopyNormalized(KEEP_INOBJECT_PROPERTIES,
                                                     UNIQUE_NORMALIZED_MAP);
      if (!maybe_obj->ToObject(&obj)) return maybe_obj;
    }
    GetIsolate()->counters()->normalized_maps()->Increment();

    set_map(Map::cast(obj));
  }
  return map()->UpdateCodeCache(name, code);
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
  int property_count = map_of_this->NumberOfDescribedProperties();
  if (expected_additional_properties > 0) {
    property_count += expected_additional_properties;
  } else {
    property_count += 2;  // Make space for two more properties.
  }
  StringDictionary* dictionary;
  { MaybeObject* maybe_dictionary = StringDictionary::Allocate(property_count);
    if (!maybe_dictionary->To(&dictionary)) return maybe_dictionary;
  }

  DescriptorArray* descs = map_of_this->instance_descriptors();
  for (int i = 0; i < descs->number_of_descriptors(); i++) {
    PropertyDetails details(descs->GetDetails(i));
    switch (details.type()) {
      case CONSTANT_FUNCTION: {
        PropertyDetails d =
            PropertyDetails(details.attributes(), NORMAL, details.index());
        Object* value = descs->GetConstantFunction(i);
        MaybeObject* maybe_dictionary =
            dictionary->Add(descs->GetKey(i), value, d);
        if (!maybe_dictionary->To(&dictionary)) return maybe_dictionary;
        break;
      }
      case FIELD: {
        PropertyDetails d =
            PropertyDetails(details.attributes(), NORMAL, details.index());
        Object* value = FastPropertyAt(descs->GetFieldIndex(i));
        MaybeObject* maybe_dictionary =
            dictionary->Add(descs->GetKey(i), value, d);
        if (!maybe_dictionary->To(&dictionary)) return maybe_dictionary;
        break;
      }
      case CALLBACKS: {
        if (!descs->IsProperty(i)) break;
        Object* value = descs->GetCallbacksObject(i);
        if (value->IsAccessorPair()) {
          MaybeObject* maybe_copy =
              AccessorPair::cast(value)->CopyWithoutTransitions();
          if (!maybe_copy->To(&value)) return maybe_copy;
        }
        MaybeObject* maybe_dictionary =
            dictionary->Add(descs->GetKey(i), value, details);
        if (!maybe_dictionary->To(&dictionary)) return maybe_dictionary;
        break;
      }
      case MAP_TRANSITION:
      case CONSTANT_TRANSITION:
      case NULL_DESCRIPTOR:
      case INTERCEPTOR:
      case ELEMENTS_TRANSITION:
        break;
      case HANDLER:
      case NORMAL:
        UNREACHABLE();
        break;
    }
  }

  Heap* current_heap = GetHeap();

  // Copy the next enumeration index from instance descriptor.
  int index = map_of_this->instance_descriptors()->NextEnumerationIndex();
  dictionary->SetNextEnumerationIndex(index);

  Map* new_map;
  { MaybeObject* maybe_map =
        current_heap->isolate()->context()->global_context()->
        normalized_map_cache()->Get(this, mode);
    if (!maybe_map->To(&new_map)) return maybe_map;
  }

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
  new_map->clear_instance_descriptors();

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

  ASSERT(HasFastElements() ||
         HasFastSmiOnlyElements() ||
         HasFastDoubleElements() ||
         HasFastArgumentsElements());
  // Compute the effective length and allocate a new backing store.
  int length = IsJSArray()
      ? Smi::cast(JSArray::cast(this)->length())->value()
      : array->length();
  int old_capacity = 0;
  int used_elements = 0;
  GetElementsCapacityAndUsage(&old_capacity, &used_elements);
  SeededNumberDictionary* dictionary = NULL;
  { Object* object;
    MaybeObject* maybe = SeededNumberDictionary::Allocate(used_elements);
    if (!maybe->ToObject(&object)) return maybe;
    dictionary = SeededNumberDictionary::cast(object);
  }

  // Copy the elements to the new backing store.
  bool has_double_elements = array->IsFixedDoubleArray();
  for (int i = 0; i < length; i++) {
    Object* value = NULL;
    if (has_double_elements) {
      FixedDoubleArray* double_array = FixedDoubleArray::cast(array);
      if (double_array->is_the_hole(i)) {
        value = GetIsolate()->heap()->the_hole_value();
      } else {
        // Objects must be allocated in the old object space, since the
        // overall number of HeapNumbers needed for the conversion might
        // exceed the capacity of new space, and we would fail repeatedly
        // trying to convert the FixedDoubleArray.
        MaybeObject* maybe_value_object =
            GetHeap()->AllocateHeapNumber(double_array->get_scalar(i), TENURED);
        if (!maybe_value_object->ToObject(&value)) return maybe_value_object;
      }
    } else {
      ASSERT(old_map->has_fast_elements() ||
             old_map->has_fast_smi_only_elements());
      value = FixedArray::cast(array)->get(i);
    }
    PropertyDetails details = PropertyDetails(NONE, NORMAL);
    if (!value->IsTheHole()) {
      Object* result;
      MaybeObject* maybe_result =
          dictionary->AddNumberEntry(i, value, details);
      if (!maybe_result->ToObject(&result)) return maybe_result;
      dictionary = SeededNumberDictionary::cast(result);
    }
  }

  // Switch to using the dictionary as the backing storage for elements.
  if (is_arguments) {
    FixedArray::cast(elements())->set(1, dictionary);
  } else {
    // Set the new map first to satify the elements type assert in
    // set_elements().
    Object* new_map;
    MaybeObject* maybe = GetElementsTransitionMap(GetIsolate(),
                                                  DICTIONARY_ELEMENTS);
    if (!maybe->ToObject(&new_map)) return maybe;
    set_map(Map::cast(new_map));
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
    hash_value = V8::RandomPrivate(isolate) & Smi::kMaxValue;
    attempts++;
  } while (hash_value == 0 && attempts < 30);
  hash_value = hash_value != 0 ? hash_value : 1;  // never return 0

  return Smi::FromInt(hash_value);
}


MaybeObject* JSObject::SetIdentityHash(Object* hash, CreationFlag flag) {
  MaybeObject* maybe = SetHiddenProperty(GetHeap()->identity_hash_symbol(),
                                         hash);
  if (maybe->IsFailure()) return maybe;
  return this;
}


int JSObject::GetIdentityHash(Handle<JSObject> obj) {
  CALL_AND_RETRY(obj->GetIsolate(),
                 obj->GetIdentityHash(ALLOW_CREATION),
                 return Smi::cast(__object__)->value(),
                 return 0);
}


MaybeObject* JSObject::GetIdentityHash(CreationFlag flag) {
  Object* stored_value = GetHiddenProperty(GetHeap()->identity_hash_symbol());
  if (stored_value->IsSmi()) return stored_value;

  // Do not generate permanent identity hash code if not requested.
  if (flag == OMIT_CREATION) return GetHeap()->undefined_value();

  Smi* hash = GenerateIdentityHash();
  MaybeObject* result = SetHiddenProperty(GetHeap()->identity_hash_symbol(),
                                          hash);
  if (result->IsFailure()) return result;
  if (result->ToObjectUnchecked()->IsUndefined()) {
    // Trying to get hash of detached proxy.
    return Smi::FromInt(0);
  }
  return hash;
}


MaybeObject* JSProxy::GetIdentityHash(CreationFlag flag) {
  Object* hash = this->hash();
  if (!hash->IsSmi() && flag == ALLOW_CREATION) {
    hash = GenerateIdentityHash();
    set_hash(hash);
  }
  return hash;
}


Object* JSObject::GetHiddenProperty(String* key) {
  if (IsJSGlobalProxy()) {
    // For a proxy, use the prototype as target object.
    Object* proxy_parent = GetPrototype();
    // If the proxy is detached, return undefined.
    if (proxy_parent->IsNull()) return GetHeap()->undefined_value();
    ASSERT(proxy_parent->IsJSGlobalObject());
    return JSObject::cast(proxy_parent)->GetHiddenProperty(key);
  }
  ASSERT(!IsJSGlobalProxy());
  MaybeObject* hidden_lookup = GetHiddenPropertiesDictionary(false);
  ASSERT(!hidden_lookup->IsFailure());  // No failure when passing false as arg.
  if (hidden_lookup->ToObjectUnchecked()->IsUndefined()) {
    return GetHeap()->undefined_value();
  }
  StringDictionary* dictionary =
      StringDictionary::cast(hidden_lookup->ToObjectUnchecked());
  int entry = dictionary->FindEntry(key);
  if (entry == StringDictionary::kNotFound) return GetHeap()->undefined_value();
  return dictionary->ValueAt(entry);
}


Handle<Object> JSObject::SetHiddenProperty(Handle<JSObject> obj,
                                 Handle<String> key,
                                 Handle<Object> value) {
  CALL_HEAP_FUNCTION(obj->GetIsolate(),
                     obj->SetHiddenProperty(*key, *value),
                     Object);
}


MaybeObject* JSObject::SetHiddenProperty(String* key, Object* value) {
  if (IsJSGlobalProxy()) {
    // For a proxy, use the prototype as target object.
    Object* proxy_parent = GetPrototype();
    // If the proxy is detached, return undefined.
    if (proxy_parent->IsNull()) return GetHeap()->undefined_value();
    ASSERT(proxy_parent->IsJSGlobalObject());
    return JSObject::cast(proxy_parent)->SetHiddenProperty(key, value);
  }
  ASSERT(!IsJSGlobalProxy());
  MaybeObject* hidden_lookup = GetHiddenPropertiesDictionary(true);
  StringDictionary* dictionary;
  if (!hidden_lookup->To<StringDictionary>(&dictionary)) return hidden_lookup;

  // If it was found, check if the key is already in the dictionary.
  int entry = dictionary->FindEntry(key);
  if (entry != StringDictionary::kNotFound) {
    // If key was found, just update the value.
    dictionary->ValueAtPut(entry, value);
    return this;
  }
  // Key was not already in the dictionary, so add the entry.
  MaybeObject* insert_result = dictionary->Add(key,
                                               value,
                                               PropertyDetails(NONE, NORMAL));
  StringDictionary* new_dict;
  if (!insert_result->To<StringDictionary>(&new_dict)) return insert_result;
  if (new_dict != dictionary) {
    // If adding the key expanded the dictionary (i.e., Add returned a new
    // dictionary), store it back to the object.
    MaybeObject* store_result = SetHiddenPropertiesDictionary(new_dict);
    if (store_result->IsFailure()) return store_result;
  }
  // Return this to mark success.
  return this;
}


void JSObject::DeleteHiddenProperty(String* key) {
  if (IsJSGlobalProxy()) {
    // For a proxy, use the prototype as target object.
    Object* proxy_parent = GetPrototype();
    // If the proxy is detached, return immediately.
    if (proxy_parent->IsNull()) return;
    ASSERT(proxy_parent->IsJSGlobalObject());
    JSObject::cast(proxy_parent)->DeleteHiddenProperty(key);
    return;
  }
  MaybeObject* hidden_lookup = GetHiddenPropertiesDictionary(false);
  ASSERT(!hidden_lookup->IsFailure());  // No failure when passing false as arg.
  if (hidden_lookup->ToObjectUnchecked()->IsUndefined()) return;
  StringDictionary* dictionary =
      StringDictionary::cast(hidden_lookup->ToObjectUnchecked());
  int entry = dictionary->FindEntry(key);
  if (entry == StringDictionary::kNotFound) {
    // Key wasn't in dictionary. Deletion is a success.
    return;
  }
  // Key was in the dictionary. Remove it.
  dictionary->DeleteProperty(entry, JSReceiver::FORCE_DELETION);
}


bool JSObject::HasHiddenProperties() {
  return GetPropertyAttributePostInterceptor(this,
                                             GetHeap()->hidden_symbol(),
                                             false) != ABSENT;
}


MaybeObject* JSObject::GetHiddenPropertiesDictionary(bool create_if_absent) {
  ASSERT(!IsJSGlobalProxy());
  if (HasFastProperties()) {
    // If the object has fast properties, check whether the first slot
    // in the descriptor array matches the hidden symbol. Since the
    // hidden symbols hash code is zero (and no other string has hash
    // code zero) it will always occupy the first entry if present.
    DescriptorArray* descriptors = this->map()->instance_descriptors();
    if ((descriptors->number_of_descriptors() > 0) &&
        (descriptors->GetKey(0) == GetHeap()->hidden_symbol())) {
      if (descriptors->GetType(0) == FIELD) {
        Object* hidden_store =
            this->FastPropertyAt(descriptors->GetFieldIndex(0));
        return StringDictionary::cast(hidden_store);
      } else {
        ASSERT(descriptors->GetType(0) == NULL_DESCRIPTOR ||
               descriptors->GetType(0) == MAP_TRANSITION);
      }
    }
  } else {
    PropertyAttributes attributes;
    // You can't install a getter on a property indexed by the hidden symbol,
    // so we can be sure that GetLocalPropertyPostInterceptor returns a real
    // object.
    Object* lookup =
        GetLocalPropertyPostInterceptor(this,
                                        GetHeap()->hidden_symbol(),
                                        &attributes)->ToObjectUnchecked();
    if (!lookup->IsUndefined()) {
      return StringDictionary::cast(lookup);
    }
  }
  if (!create_if_absent) return GetHeap()->undefined_value();
  const int kInitialSize = 5;
  MaybeObject* dict_alloc = StringDictionary::Allocate(kInitialSize);
  StringDictionary* dictionary;
  if (!dict_alloc->To<StringDictionary>(&dictionary)) return dict_alloc;
  MaybeObject* store_result =
      SetPropertyPostInterceptor(GetHeap()->hidden_symbol(),
                                 dictionary,
                                 DONT_ENUM,
                                 kNonStrictMode);
  if (store_result->IsFailure()) return store_result;
  return dictionary;
}


MaybeObject* JSObject::SetHiddenPropertiesDictionary(
    StringDictionary* dictionary) {
  ASSERT(!IsJSGlobalProxy());
  ASSERT(HasHiddenProperties());
  if (HasFastProperties()) {
    // If the object has fast properties, check whether the first slot
    // in the descriptor array matches the hidden symbol. Since the
    // hidden symbols hash code is zero (and no other string has hash
    // code zero) it will always occupy the first entry if present.
    DescriptorArray* descriptors = this->map()->instance_descriptors();
    if ((descriptors->number_of_descriptors() > 0) &&
        (descriptors->GetKey(0) == GetHeap()->hidden_symbol())) {
      if (descriptors->GetType(0) == FIELD) {
        this->FastPropertyAtPut(descriptors->GetFieldIndex(0), dictionary);
        return this;
      } else {
        ASSERT(descriptors->GetType(0) == NULL_DESCRIPTOR ||
               descriptors->GetType(0) == MAP_TRANSITION);
      }
    }
  }
  MaybeObject* store_result =
      SetPropertyPostInterceptor(GetHeap()->hidden_symbol(),
                                 dictionary,
                                 DONT_ENUM,
                                 kNonStrictMode);
  if (store_result->IsFailure()) return store_result;
  return this;
}


MaybeObject* JSObject::DeletePropertyPostInterceptor(String* name,
                                                     DeleteMode mode) {
  // Check local property, ignore interceptor.
  LookupResult result(GetIsolate());
  LocalLookupRealNamedProperty(name, &result);
  if (!result.IsProperty()) return GetHeap()->true_value();

  // Normalize object if needed.
  Object* obj;
  { MaybeObject* maybe_obj = NormalizeProperties(CLEAR_INOBJECT_PROPERTIES, 0);
    if (!maybe_obj->ToObject(&obj)) return maybe_obj;
  }

  return DeleteNormalizedProperty(name, mode);
}


MaybeObject* JSObject::DeletePropertyWithInterceptor(String* name) {
  Isolate* isolate = GetIsolate();
  HandleScope scope(isolate);
  Handle<InterceptorInfo> interceptor(GetNamedInterceptor());
  Handle<String> name_handle(name);
  Handle<JSObject> this_handle(this);
  if (!interceptor->deleter()->IsUndefined()) {
    v8::NamedPropertyDeleter deleter =
        v8::ToCData<v8::NamedPropertyDeleter>(interceptor->deleter());
    LOG(isolate,
        ApiNamedPropertyAccess("interceptor-named-delete", *this_handle, name));
    CustomArguments args(isolate, interceptor->data(), this, this);
    v8::AccessorInfo info(args.end());
    v8::Handle<v8::Boolean> result;
    {
      // Leaving JavaScript.
      VMState state(isolate, EXTERNAL);
      result = deleter(v8::Utils::ToLocal(name_handle), info);
    }
    RETURN_IF_SCHEDULED_EXCEPTION(isolate);
    if (!result.IsEmpty()) {
      ASSERT(result->IsBoolean());
      return *v8::Utils::OpenHandle(*result);
    }
  }
  MaybeObject* raw_result =
      this_handle->DeletePropertyPostInterceptor(*name_handle, NORMAL_DELETION);
  RETURN_IF_SCHEDULED_EXCEPTION(isolate);
  return raw_result;
}


MaybeObject* JSObject::DeleteElementWithInterceptor(uint32_t index) {
  Isolate* isolate = GetIsolate();
  Heap* heap = isolate->heap();
  // Make sure that the top context does not change when doing
  // callbacks or interceptor calls.
  AssertNoContextChange ncc;
  HandleScope scope(isolate);
  Handle<InterceptorInfo> interceptor(GetIndexedInterceptor());
  if (interceptor->deleter()->IsUndefined()) return heap->false_value();
  v8::IndexedPropertyDeleter deleter =
      v8::ToCData<v8::IndexedPropertyDeleter>(interceptor->deleter());
  Handle<JSObject> this_handle(this);
  LOG(isolate,
      ApiIndexedPropertyAccess("interceptor-indexed-delete", this, index));
  CustomArguments args(isolate, interceptor->data(), this, this);
  v8::AccessorInfo info(args.end());
  v8::Handle<v8::Boolean> result;
  {
    // Leaving JavaScript.
    VMState state(isolate, EXTERNAL);
    result = deleter(index, info);
  }
  RETURN_IF_SCHEDULED_EXCEPTION(isolate);
  if (!result.IsEmpty()) {
    ASSERT(result->IsBoolean());
    return *v8::Utils::OpenHandle(*result);
  }
  MaybeObject* raw_result = this_handle->GetElementsAccessor()->Delete(
      *this_handle,
      index,
      NORMAL_DELETION);
  RETURN_IF_SCHEDULED_EXCEPTION(isolate);
  return raw_result;
}


Handle<Object> JSObject::DeleteElement(Handle<JSObject> obj,
                                       uint32_t index) {
  CALL_HEAP_FUNCTION(obj->GetIsolate(),
                     obj->DeleteElement(index, JSObject::NORMAL_DELETION),
                     Object);
}


MaybeObject* JSObject::DeleteElement(uint32_t index, DeleteMode mode) {
  Isolate* isolate = GetIsolate();
  // Check access rights if needed.
  if (IsAccessCheckNeeded() &&
      !isolate->MayIndexedAccess(this, index, v8::ACCESS_DELETE)) {
    isolate->ReportFailedAccessCheck(this, v8::ACCESS_DELETE);
    return isolate->heap()->false_value();
  }

  if (IsJSGlobalProxy()) {
    Object* proto = GetPrototype();
    if (proto->IsNull()) return isolate->heap()->false_value();
    ASSERT(proto->IsJSGlobalObject());
    return JSGlobalObject::cast(proto)->DeleteElement(index, mode);
  }

  if (HasIndexedInterceptor()) {
    // Skip interceptor if forcing deletion.
    if (mode != FORCE_DELETION) {
      return DeleteElementWithInterceptor(index);
    }
    mode = JSReceiver::FORCE_DELETION;
  }

  return GetElementsAccessor()->Delete(this, index, mode);
}


Handle<Object> JSObject::DeleteProperty(Handle<JSObject> obj,
                              Handle<String> prop) {
  CALL_HEAP_FUNCTION(obj->GetIsolate(),
                     obj->DeleteProperty(*prop, JSObject::NORMAL_DELETION),
                     Object);
}


MaybeObject* JSObject::DeleteProperty(String* name, DeleteMode mode) {
  Isolate* isolate = GetIsolate();
  // ECMA-262, 3rd, 8.6.2.5
  ASSERT(name->IsString());

  // Check access rights if needed.
  if (IsAccessCheckNeeded() &&
      !isolate->MayNamedAccess(this, name, v8::ACCESS_DELETE)) {
    isolate->ReportFailedAccessCheck(this, v8::ACCESS_DELETE);
    return isolate->heap()->false_value();
  }

  if (IsJSGlobalProxy()) {
    Object* proto = GetPrototype();
    if (proto->IsNull()) return isolate->heap()->false_value();
    ASSERT(proto->IsJSGlobalObject());
    return JSGlobalObject::cast(proto)->DeleteProperty(name, mode);
  }

  uint32_t index = 0;
  if (name->AsArrayIndex(&index)) {
    return DeleteElement(index, mode);
  } else {
    LookupResult result(isolate);
    LocalLookup(name, &result);
    if (!result.IsProperty()) return isolate->heap()->true_value();
    // Ignore attributes if forcing a deletion.
    if (result.IsDontDelete() && mode != FORCE_DELETION) {
      if (mode == STRICT_DELETION) {
        // Deleting a non-configurable property in strict mode.
        HandleScope scope(isolate);
        Handle<Object> args[2] = { Handle<Object>(name), Handle<Object>(this) };
        return isolate->Throw(*isolate->factory()->NewTypeError(
            "strict_delete_property", HandleVector(args, 2)));
      }
      return isolate->heap()->false_value();
    }
    // Check for interceptor.
    if (result.type() == INTERCEPTOR) {
      // Skip interceptor if forcing a deletion.
      if (mode == FORCE_DELETION) {
        return DeletePropertyPostInterceptor(name, mode);
      }
      return DeletePropertyWithInterceptor(name);
    }
    // Normalize object if needed.
    Object* obj;
    { MaybeObject* maybe_obj =
          NormalizeProperties(CLEAR_INOBJECT_PROPERTIES, 0);
      if (!maybe_obj->ToObject(&obj)) return maybe_obj;
    }
    // Make sure the properties are normalized before removing the entry.
    return DeleteNormalizedProperty(name, mode);
  }
}


MaybeObject* JSReceiver::DeleteElement(uint32_t index, DeleteMode mode) {
  if (IsJSProxy()) {
    return JSProxy::cast(this)->DeleteElementWithHandler(index, mode);
  }
  return JSObject::cast(this)->DeleteElement(index, mode);
}


MaybeObject* JSReceiver::DeleteProperty(String* name, DeleteMode mode) {
  if (IsJSProxy()) {
    return JSProxy::cast(this)->DeletePropertyWithHandler(name, mode);
  }
  return JSObject::cast(this)->DeleteProperty(name, mode);
}


bool JSObject::ReferencesObjectFromElements(FixedArray* elements,
                                            ElementsKind kind,
                                            Object* object) {
  ASSERT(kind == FAST_ELEMENTS ||
         kind == DICTIONARY_ELEMENTS);
  if (kind == FAST_ELEMENTS) {
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
  AssertNoAllocation no_alloc;

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
      // Raw pixels and external arrays do not reference other
      // objects.
      break;
    case FAST_SMI_ONLY_ELEMENTS:
      break;
    case FAST_ELEMENTS:
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
      kind = arguments->IsDictionary() ? DICTIONARY_ELEMENTS : FAST_ELEMENTS;
      if (ReferencesObjectFromElements(arguments, kind, obj)) return true;
      break;
    }
  }

  // For functions check the context.
  if (IsJSFunction()) {
    // Get the constructor function for arguments array.
    JSObject* arguments_boilerplate =
        heap->isolate()->context()->global_context()->
            arguments_boilerplate();
    JSFunction* arguments_function =
        JSFunction::cast(arguments_boilerplate->map()->constructor());

    // Get the context and don't check if it is the global context.
    JSFunction* f = JSFunction::cast(this);
    Context* context = f->context();
    if (context->IsGlobalContext()) {
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
    Handle<Object> object(this);
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
  Map* new_map;
  { MaybeObject* maybe = map()->CopyDropTransitions();
    if (!maybe->To<Map>(&new_map)) return maybe;
  }
  new_map->set_is_extensible(false);
  set_map(new_map);
  ASSERT(!map()->is_extensible());
  return new_map;
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
    if (!curr->map()->instance_descriptors()->HasEnumCache()) return false;
    ASSERT(!curr->HasNamedInterceptor());
    ASSERT(!curr->HasIndexedInterceptor());
    ASSERT(!curr->IsAccessCheckNeeded());
    if (curr->NumberOfEnumElements() > 0) return false;
    if (curr != this) {
      FixedArray* curr_fixed_array =
          FixedArray::cast(curr->map()->instance_descriptors()->GetEnumCache());
      if (curr_fixed_array->length() > 0) return false;
    }
  }
  return true;
}


int Map::NumberOfDescribedProperties(PropertyAttributes filter) {
  int result = 0;
  DescriptorArray* descs = instance_descriptors();
  for (int i = 0; i < descs->number_of_descriptors(); i++) {
    PropertyDetails details(descs->GetDetails(i));
    if (descs->IsProperty(i) && (details.attributes() & filter) == 0) {
      result++;
    }
  }
  return result;
}


int Map::PropertyIndexFor(String* name) {
  DescriptorArray* descs = instance_descriptors();
  for (int i = 0; i < descs->number_of_descriptors(); i++) {
    if (name->Equals(descs->GetKey(i)) && !descs->IsNullDescriptor(i)) {
      return descs->GetFieldIndex(i);
    }
  }
  return -1;
}


int Map::NextFreePropertyIndex() {
  int max_index = -1;
  DescriptorArray* descs = instance_descriptors();
  for (int i = 0; i < descs->number_of_descriptors(); i++) {
    if (descs->GetType(i) == FIELD) {
      int current_index = descs->GetFieldIndex(i);
      if (current_index > max_index) max_index = current_index;
    }
  }
  return max_index + 1;
}


AccessorDescriptor* Map::FindAccessor(String* name) {
  DescriptorArray* descs = instance_descriptors();
  for (int i = 0; i < descs->number_of_descriptors(); i++) {
    if (name->Equals(descs->GetKey(i)) && descs->GetType(i) == CALLBACKS) {
      return descs->GetCallbacks(i);
    }
  }
  return NULL;
}


void JSReceiver::LocalLookup(String* name, LookupResult* result) {
  ASSERT(name->IsString());

  Heap* heap = GetHeap();

  if (IsJSGlobalProxy()) {
    Object* proto = GetPrototype();
    if (proto->IsNull()) return result->NotFound();
    ASSERT(proto->IsJSGlobalObject());
    return JSReceiver::cast(proto)->LocalLookup(name, result);
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

  // Check __proto__ before interceptor.
  if (name->Equals(heap->Proto_symbol()) && !IsJSContextExtensionObject()) {
    result->ConstantResult(js_object);
    return;
  }

  // Check for lookup interceptor except when bootstrapping.
  if (js_object->HasNamedInterceptor() &&
      !heap->isolate()->bootstrapper()->IsActive()) {
    result->InterceptorResult(js_object);
    return;
  }

  js_object->LocalLookupRealNamedProperty(name, result);
}


void JSReceiver::Lookup(String* name, LookupResult* result) {
  // Ecma-262 3rd 8.6.2.4
  Heap* heap = GetHeap();
  for (Object* current = this;
       current != heap->null_value();
       current = JSObject::cast(current)->GetPrototype()) {
    JSReceiver::cast(current)->LocalLookup(name, result);
    if (result->IsProperty()) return;
  }
  result->NotFound();
}


// Search object and it's prototype chain for callback properties.
void JSObject::LookupCallback(String* name, LookupResult* result) {
  Heap* heap = GetHeap();
  for (Object* current = this;
       current != heap->null_value() && current->IsJSObject();
       current = JSObject::cast(current)->GetPrototype()) {
    JSObject::cast(current)->LocalLookupRealNamedProperty(name, result);
    if (result->IsFound() && result->type() == CALLBACKS) return;
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
        dictionary->DetailsAtPut(entry,
                                 PropertyDetails(attributes, CALLBACKS, index));
      }
      AccessorPair::cast(result)->SetComponents(getter, setter);
      return true;
    }
  }
  return false;
}


MaybeObject* JSObject::DefineElementAccessor(uint32_t index,
                                             Object* getter,
                                             Object* setter,
                                             PropertyAttributes attributes) {
  switch (GetElementsKind()) {
    case FAST_SMI_ONLY_ELEMENTS:
    case FAST_ELEMENTS:
    case FAST_DOUBLE_ELEMENTS:
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
      return GetHeap()->undefined_value();
    case DICTIONARY_ELEMENTS:
      if (UpdateGetterSetterInDictionary(element_dictionary(),
                                         index,
                                         getter,
                                         setter,
                                         attributes)) {
        return GetHeap()->undefined_value();
      }
      break;
    case NON_STRICT_ARGUMENTS_ELEMENTS: {
      // Ascertain whether we have read-only properties or an existing
      // getter/setter pair in an arguments elements dictionary backing
      // store.
      FixedArray* parameter_map = FixedArray::cast(elements());
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
                                             getter,
                                             setter,
                                             attributes)) {
            return GetHeap()->undefined_value();
          }
        }
      }
      break;
    }
  }

  AccessorPair* accessors;
  { MaybeObject* maybe_accessors = GetHeap()->AllocateAccessorPair();
    if (!maybe_accessors->To(&accessors)) return maybe_accessors;
  }
  accessors->SetComponents(getter, setter);

  return SetElementCallback(index, accessors, attributes);
}


MaybeObject* JSObject::DefinePropertyAccessor(String* name,
                                              Object* getter,
                                              Object* setter,
                                              PropertyAttributes attributes) {
  // Lookup the name.
  LookupResult result(GetHeap()->isolate());
  LocalLookupRealNamedProperty(name, &result);
  if (result.IsFound()) {
    if (result.type() == CALLBACKS) {
      ASSERT(!result.IsDontDelete());
      Object* obj = result.GetCallbackObject();
      // Need to preserve old getters/setters.
      if (obj->IsAccessorPair()) {
        AccessorPair* copy;
        { MaybeObject* maybe_copy =
              AccessorPair::cast(obj)->CopyWithoutTransitions();
          if (!maybe_copy->To(&copy)) return maybe_copy;
        }
        copy->SetComponents(getter, setter);
        // Use set to update attributes.
        return SetPropertyCallback(name, copy, attributes);
      }
    }
  }

  AccessorPair* accessors;
  { MaybeObject* maybe_accessors = GetHeap()->AllocateAccessorPair();
    if (!maybe_accessors->To(&accessors)) return maybe_accessors;
  }
  accessors->SetComponents(getter, setter);

  return SetPropertyCallback(name, accessors, attributes);
}


bool JSObject::CanSetCallback(String* name) {
  ASSERT(!IsAccessCheckNeeded() ||
         GetIsolate()->MayNamedAccess(this, name, v8::ACCESS_SET));

  // Check if there is an API defined callback object which prohibits
  // callback overwriting in this object or it's prototype chain.
  // This mechanism is needed for instance in a browser setting, where
  // certain accessors such as window.location should not be allowed
  // to be overwritten because allowing overwriting could potentially
  // cause security problems.
  LookupResult callback_result(GetIsolate());
  LookupCallback(name, &callback_result);
  if (callback_result.IsProperty()) {
    Object* obj = callback_result.GetCallbackObject();
    if (obj->IsAccessorInfo() &&
        AccessorInfo::cast(obj)->prohibits_overwriting()) {
      return false;
    }
  }

  return true;
}


MaybeObject* JSObject::SetElementCallback(uint32_t index,
                                          Object* structure,
                                          PropertyAttributes attributes) {
  PropertyDetails details = PropertyDetails(attributes, CALLBACKS);

  // Normalize elements to make this operation simple.
  SeededNumberDictionary* dictionary;
  { MaybeObject* maybe_dictionary = NormalizeElements();
    if (!maybe_dictionary->To(&dictionary)) return maybe_dictionary;
  }
  ASSERT(HasDictionaryElements() || HasDictionaryArgumentsElements());

  // Update the dictionary with the new CALLBACKS property.
  { MaybeObject* maybe_dictionary = dictionary->Set(index, structure, details);
    if (!maybe_dictionary->To(&dictionary)) return maybe_dictionary;
  }

  dictionary->set_requires_slow_elements();
  // Update the dictionary backing store on the object.
  if (elements()->map() == GetHeap()->non_strict_arguments_elements_map()) {
    // Also delete any parameter alias.
    //
    // TODO(kmillikin): when deleting the last parameter alias we could
    // switch to a direct backing store without the parameter map.  This
    // would allow GC of the context.
    FixedArray* parameter_map = FixedArray::cast(elements());
    if (index < static_cast<uint32_t>(parameter_map->length()) - 2) {
      parameter_map->set(index + 2, GetHeap()->the_hole_value());
    }
    parameter_map->set(1, dictionary);
  } else {
    set_elements(dictionary);
  }

  return GetHeap()->undefined_value();
}


MaybeObject* JSObject::SetPropertyCallback(String* name,
                                           Object* structure,
                                           PropertyAttributes attributes) {
  // Normalize object to make this operation simple.
  { MaybeObject* maybe_ok = NormalizeProperties(CLEAR_INOBJECT_PROPERTIES, 0);
    if (maybe_ok->IsFailure()) return maybe_ok;
  }

  // For the global object allocate a new map to invalidate the global inline
  // caches which have a global property cell reference directly in the code.
  if (IsGlobalObject()) {
    Map* new_map;
    { MaybeObject* maybe_new_map = map()->CopyDropDescriptors();
      if (!maybe_new_map->To(&new_map)) return maybe_new_map;
    }
    set_map(new_map);
    // When running crankshaft, changing the map is not enough. We
    // need to deoptimize all functions that rely on this global
    // object.
    Deoptimizer::DeoptimizeGlobalObject(this);
  }

  // Update the dictionary with the new CALLBACKS property.
  PropertyDetails details = PropertyDetails(attributes, CALLBACKS);
  { MaybeObject* maybe_ok = SetNormalizedProperty(name, structure, details);
    if (maybe_ok->IsFailure()) return maybe_ok;
  }

  return GetHeap()->undefined_value();
}


void JSObject::DefineAccessor(Handle<JSObject> object,
                              Handle<String> name,
                              Handle<Object> getter,
                              Handle<Object> setter,
                              PropertyAttributes attributes) {
  CALL_HEAP_FUNCTION_VOID(
      object->GetIsolate(),
      object->DefineAccessor(*name, *getter, *setter, attributes));
}

MaybeObject* JSObject::DefineAccessor(String* name,
                                      Object* getter,
                                      Object* setter,
                                      PropertyAttributes attributes) {
  Isolate* isolate = GetIsolate();
  // Check access rights if needed.
  if (IsAccessCheckNeeded() &&
      !isolate->MayNamedAccess(this, name, v8::ACCESS_SET)) {
    isolate->ReportFailedAccessCheck(this, v8::ACCESS_SET);
    return isolate->heap()->undefined_value();
  }

  if (IsJSGlobalProxy()) {
    Object* proto = GetPrototype();
    if (proto->IsNull()) return this;
    ASSERT(proto->IsJSGlobalObject());
    return JSObject::cast(proto)->DefineAccessor(
        name, getter, setter, attributes);
  }

  // Make sure that the top context does not change when doing callbacks or
  // interceptor calls.
  AssertNoContextChange ncc;

  // Try to flatten before operating on the string.
  name->TryFlatten();

  if (!CanSetCallback(name)) return isolate->heap()->undefined_value();

  uint32_t index = 0;
  return name->AsArrayIndex(&index) ?
      DefineElementAccessor(index, getter, setter, attributes) :
      DefinePropertyAccessor(name, getter, setter, attributes);
}


MaybeObject* JSObject::DefineAccessor(AccessorInfo* info) {
  Isolate* isolate = GetIsolate();
  String* name = String::cast(info->name());
  // Check access rights if needed.
  if (IsAccessCheckNeeded() &&
      !isolate->MayNamedAccess(this, name, v8::ACCESS_SET)) {
    isolate->ReportFailedAccessCheck(this, v8::ACCESS_SET);
    return isolate->heap()->undefined_value();
  }

  if (IsJSGlobalProxy()) {
    Object* proto = GetPrototype();
    if (proto->IsNull()) return this;
    ASSERT(proto->IsJSGlobalObject());
    return JSObject::cast(proto)->DefineAccessor(info);
  }

  // Make sure that the top context does not change when doing callbacks or
  // interceptor calls.
  AssertNoContextChange ncc;

  // Try to flatten before operating on the string.
  name->TryFlatten();

  if (!CanSetCallback(name)) {
    return isolate->heap()->undefined_value();
  }

  uint32_t index = 0;
  bool is_element = name->AsArrayIndex(&index);

  if (is_element) {
    if (IsJSArray()) return isolate->heap()->undefined_value();

    // Accessors overwrite previous callbacks (cf. with getters/setters).
    switch (GetElementsKind()) {
      case FAST_SMI_ONLY_ELEMENTS:
      case FAST_ELEMENTS:
      case FAST_DOUBLE_ELEMENTS:
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
        return isolate->heap()->undefined_value();
      case DICTIONARY_ELEMENTS:
        break;
      case NON_STRICT_ARGUMENTS_ELEMENTS:
        UNIMPLEMENTED();
        break;
    }

    { MaybeObject* maybe_ok =
          SetElementCallback(index, info, info->property_attributes());
      if (maybe_ok->IsFailure()) return maybe_ok;
    }
  } else {
    // Lookup the name.
    LookupResult result(isolate);
    LocalLookup(name, &result);
    // ES5 forbids turning a property into an accessor if it's not
    // configurable (that is IsDontDelete in ES3 and v8), see 8.6.1 (Table 5).
    if (result.IsProperty() && (result.IsReadOnly() || result.IsDontDelete())) {
      return isolate->heap()->undefined_value();
    }
    { MaybeObject* maybe_ok =
          SetPropertyCallback(name, info, info->property_attributes());
      if (maybe_ok->IsFailure()) return maybe_ok;
    }
  }

  return this;
}


Object* JSObject::LookupAccessor(String* name, AccessorComponent component) {
  Heap* heap = GetHeap();

  // Make sure that the top context does not change when doing callbacks or
  // interceptor calls.
  AssertNoContextChange ncc;

  // Check access rights if needed.
  if (IsAccessCheckNeeded() &&
      !heap->isolate()->MayNamedAccess(this, name, v8::ACCESS_HAS)) {
    heap->isolate()->ReportFailedAccessCheck(this, v8::ACCESS_HAS);
    return heap->undefined_value();
  }

  // Make the lookup and include prototypes.
  uint32_t index = 0;
  if (name->AsArrayIndex(&index)) {
    for (Object* obj = this;
         obj != heap->null_value();
         obj = JSObject::cast(obj)->GetPrototype()) {
      JSObject* js_object = JSObject::cast(obj);
      if (js_object->HasDictionaryElements()) {
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
         obj = JSObject::cast(obj)->GetPrototype()) {
      LookupResult result(heap->isolate());
      JSObject::cast(obj)->LocalLookup(name, &result);
      if (result.IsProperty()) {
        if (result.IsReadOnly()) return heap->undefined_value();
        if (result.type() == CALLBACKS) {
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
    DescriptorArray* descs = map()->instance_descriptors();
    for (int i = 0; i < descs->number_of_descriptors(); i++) {
      if (descs->GetType(i) == FIELD) {
        if (FastPropertyAt(descs->GetFieldIndex(i)) == value) {
          return descs->GetKey(i);
        }
      } else if (descs->GetType(i) == CONSTANT_FUNCTION) {
        if (descs->GetConstantFunction(i) == value) {
          return descs->GetKey(i);
        }
      }
    }
    return GetHeap()->undefined_value();
  } else {
    return property_dictionary()->SlowReverseLookup(value);
  }
}


MaybeObject* Map::CopyDropDescriptors() {
  Heap* heap = GetHeap();
  Object* result;
  { MaybeObject* maybe_result =
        heap->AllocateMap(instance_type(), instance_size());
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }
  Map::cast(result)->set_prototype(prototype());
  Map::cast(result)->set_constructor(constructor());
  // Don't copy descriptors, so map transitions always remain a forest.
  // If we retained the same descriptors we would have two maps
  // pointing to the same transition which is bad because the garbage
  // collector relies on being able to reverse pointers from transitions
  // to maps.  If properties need to be retained use CopyDropTransitions.
  Map::cast(result)->clear_instance_descriptors();
  // Please note instance_type and instance_size are set when allocated.
  Map::cast(result)->set_inobject_properties(inobject_properties());
  Map::cast(result)->set_unused_property_fields(unused_property_fields());

  // If the map has pre-allocated properties always start out with a descriptor
  // array describing these properties.
  if (pre_allocated_property_fields() > 0) {
    ASSERT(constructor()->IsJSFunction());
    JSFunction* ctor = JSFunction::cast(constructor());
    Object* descriptors;
    { MaybeObject* maybe_descriptors =
          ctor->initial_map()->instance_descriptors()->RemoveTransitions();
      if (!maybe_descriptors->ToObject(&descriptors)) return maybe_descriptors;
    }
    Map::cast(result)->set_instance_descriptors(
        DescriptorArray::cast(descriptors));
    Map::cast(result)->set_pre_allocated_property_fields(
        pre_allocated_property_fields());
  }
  Map::cast(result)->set_bit_field(bit_field());
  Map::cast(result)->set_bit_field2(bit_field2());
  Map::cast(result)->set_bit_field3(bit_field3());
  Map::cast(result)->set_is_shared(false);
  Map::cast(result)->ClearCodeCache(heap);
  return result;
}


MaybeObject* Map::CopyNormalized(PropertyNormalizationMode mode,
                                 NormalizedMapSharingMode sharing) {
  int new_instance_size = instance_size();
  if (mode == CLEAR_INOBJECT_PROPERTIES) {
    new_instance_size -= inobject_properties() * kPointerSize;
  }

  Object* result;
  { MaybeObject* maybe_result =
        GetHeap()->AllocateMap(instance_type(), new_instance_size);
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }

  if (mode != CLEAR_INOBJECT_PROPERTIES) {
    Map::cast(result)->set_inobject_properties(inobject_properties());
  }

  Map::cast(result)->set_prototype(prototype());
  Map::cast(result)->set_constructor(constructor());

  Map::cast(result)->set_bit_field(bit_field());
  Map::cast(result)->set_bit_field2(bit_field2());
  Map::cast(result)->set_bit_field3(bit_field3());

  Map::cast(result)->set_is_shared(sharing == SHARED_NORMALIZED_MAP);

#ifdef DEBUG
  if (FLAG_verify_heap && Map::cast(result)->is_shared()) {
    Map::cast(result)->SharedMapVerify();
  }
#endif

  return result;
}


MaybeObject* Map::CopyDropTransitions() {
  Object* new_map;
  { MaybeObject* maybe_new_map = CopyDropDescriptors();
    if (!maybe_new_map->ToObject(&new_map)) return maybe_new_map;
  }
  Object* descriptors;
  { MaybeObject* maybe_descriptors =
        instance_descriptors()->RemoveTransitions();
    if (!maybe_descriptors->ToObject(&descriptors)) return maybe_descriptors;
  }
  cast(new_map)->set_instance_descriptors(DescriptorArray::cast(descriptors));
  return new_map;
}

void Map::UpdateCodeCache(Handle<Map> map,
                          Handle<String> name,
                          Handle<Code> code) {
  Isolate* isolate = map->GetIsolate();
  CALL_HEAP_FUNCTION_VOID(isolate,
                          map->UpdateCodeCache(*name, *code));
}

MaybeObject* Map::UpdateCodeCache(String* name, Code* code) {
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


Object* Map::FindInCodeCache(String* name, Code::Flags flags) {
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


void Map::RemoveFromCodeCache(String* name, Code* code, int index) {
  // No GC is supposed to happen between a call to IndexInCodeCache and
  // RemoveFromCodeCache so the code cache must be there.
  ASSERT(!code_cache()->IsFixedArray());
  CodeCache::cast(code_cache())->RemoveByIndex(name, code, index);
}


// An iterator over all map transitions in an descriptor array, reusing the map
// field of the contens array while it is running.
class IntrusiveMapTransitionIterator {
 public:
  explicit IntrusiveMapTransitionIterator(DescriptorArray* descriptor_array)
      : descriptor_array_(descriptor_array) { }

  void Start() {
    ASSERT(!IsIterating());
    if (HasContentArray()) *ContentHeader() = Smi::FromInt(0);
  }

  bool IsIterating() {
    return HasContentArray() && (*ContentHeader())->IsSmi();
  }

  Map* Next() {
    ASSERT(IsIterating());
    FixedArray* contents = ContentArray();
    // Attention, tricky index manipulation ahead: Every entry in the contents
    // array consists of a value/details pair, so the index is typically even.
    // An exception is made for CALLBACKS entries: An even index means we look
    // at its getter, and an odd index means we look at its setter.
    int index = Smi::cast(*ContentHeader())->value();
    while (index < contents->length()) {
      PropertyDetails details(Smi::cast(contents->get(index | 1)));
      switch (details.type()) {
        case MAP_TRANSITION:
        case CONSTANT_TRANSITION:
        case ELEMENTS_TRANSITION:
          // We definitely have a map transition.
          *ContentHeader() = Smi::FromInt(index + 2);
          return static_cast<Map*>(contents->get(index));
        case CALLBACKS: {
          // We might have a map transition in a getter or in a setter.
          AccessorPair* accessors =
              static_cast<AccessorPair*>(contents->get(index & ~1));
          Object* accessor =
              ((index & 1) == 0) ? accessors->getter() : accessors->setter();
          index++;
          if (accessor->IsMap()) {
            *ContentHeader() = Smi::FromInt(index);
            return static_cast<Map*>(accessor);
          }
          break;
        }
        case NORMAL:
        case FIELD:
        case CONSTANT_FUNCTION:
        case HANDLER:
        case INTERCEPTOR:
        case NULL_DESCRIPTOR:
          // We definitely have no map transition.
          index += 2;
          break;
      }
    }
    *ContentHeader() = descriptor_array_->GetHeap()->fixed_array_map();
    return NULL;
  }

 private:
  bool HasContentArray() {
    return descriptor_array_-> length() > DescriptorArray::kContentArrayIndex;
  }

  FixedArray* ContentArray() {
    Object* array = descriptor_array_->get(DescriptorArray::kContentArrayIndex);
    return static_cast<FixedArray*>(array);
  }

  Object** ContentHeader() {
    return HeapObject::RawField(ContentArray(), DescriptorArray::kMapOffset);
  }

  DescriptorArray* descriptor_array_;
};


// An iterator over all prototype transitions, reusing the map field of the
// underlying array while it is running.
class IntrusivePrototypeTransitionIterator {
 public:
  explicit IntrusivePrototypeTransitionIterator(FixedArray* proto_trans)
      : proto_trans_(proto_trans) { }

  void Start() {
    ASSERT(!IsIterating());
    if (HasTransitions()) *Header() = Smi::FromInt(0);
  }

  bool IsIterating() {
    return HasTransitions() && (*Header())->IsSmi();
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
  bool HasTransitions() {
    return proto_trans_->length() >= Map::kProtoTransitionHeaderSize;
  }

  Object** Header() {
    return HeapObject::RawField(proto_trans_, FixedArray::kMapOffset);
  }

  int NumberOfTransitions() {
    Object* num = proto_trans_->get(Map::kProtoTransitionNumberOfEntriesOffset);
    return Smi::cast(num)->value();
  }

  Map* GetTransition(int transitionNumber) {
    return Map::cast(proto_trans_->get(IndexFor(transitionNumber)));
  }

  int IndexFor(int transitionNumber) {
    return Map::kProtoTransitionHeaderSize +
        Map::kProtoTransitionMapOffset +
        transitionNumber * Map::kProtoTransitionElementsPerEntry;
  }

  FixedArray* proto_trans_;
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
    IntrusiveMapTransitionIterator(instance_descriptors()).Start();
    IntrusivePrototypeTransitionIterator(
        unchecked_prototype_transitions()).Start();
  }

  // If we have an unvisited child map, return that one and advance. If we have
  // none, return NULL and reset any destroyed FixedArray maps.
  TraversableMap* ChildIteratorNext() {
    IntrusiveMapTransitionIterator descriptor_iterator(instance_descriptors());
    if (descriptor_iterator.IsIterating()) {
      Map* next = descriptor_iterator.Next();
      if (next != NULL) return static_cast<TraversableMap*>(next);
    }
    IntrusivePrototypeTransitionIterator
        proto_iterator(unchecked_prototype_transitions());
    if (proto_iterator.IsIterating()) {
      Map* next = proto_iterator.Next();
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


MaybeObject* CodeCache::Update(String* name, Code* code) {
  // The number of monomorphic stubs for normal load/store/call IC's can grow to
  // a large number and therefore they need to go into a hash table. They are
  // used to load global properties from cells.
  if (code->type() == NORMAL) {
    // Make sure that a hash table is allocated for the normal load code cache.
    if (normal_type_cache()->IsUndefined()) {
      Object* result;
      { MaybeObject* maybe_result =
            CodeCacheHashTable::Allocate(CodeCacheHashTable::kInitialSize);
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


MaybeObject* CodeCache::UpdateDefaultCache(String* name, Code* code) {
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
    if (name->Equals(String::cast(key))) {
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


MaybeObject* CodeCache::UpdateNormalTypeCache(String* name, Code* code) {
  // Adding a new entry can cause a new cache to be allocated.
  CodeCacheHashTable* cache = CodeCacheHashTable::cast(normal_type_cache());
  Object* new_cache;
  { MaybeObject* maybe_new_cache = cache->Put(name, code);
    if (!maybe_new_cache->ToObject(&new_cache)) return maybe_new_cache;
  }
  set_normal_type_cache(new_cache);
  return this;
}


Object* CodeCache::Lookup(String* name, Code::Flags flags) {
  if (Code::ExtractTypeFromFlags(flags) == NORMAL) {
    return LookupNormalTypeCache(name, flags);
  } else {
    return LookupDefaultCache(name, flags);
  }
}


Object* CodeCache::LookupDefaultCache(String* name, Code::Flags flags) {
  FixedArray* cache = default_cache();
  int length = cache->length();
  for (int i = 0; i < length; i += kCodeCacheEntrySize) {
    Object* key = cache->get(i + kCodeCacheEntryNameOffset);
    // Skip deleted elements.
    if (key->IsNull()) continue;
    if (key->IsUndefined()) return key;
    if (name->Equals(String::cast(key))) {
      Code* code = Code::cast(cache->get(i + kCodeCacheEntryCodeOffset));
      if (code->flags() == flags) {
        return code;
      }
    }
  }
  return GetHeap()->undefined_value();
}


Object* CodeCache::LookupNormalTypeCache(String* name, Code::Flags flags) {
  if (!normal_type_cache()->IsUndefined()) {
    CodeCacheHashTable* cache = CodeCacheHashTable::cast(normal_type_cache());
    return cache->Lookup(name, flags);
  } else {
    return GetHeap()->undefined_value();
  }
}


int CodeCache::GetIndex(Object* name, Code* code) {
  if (code->type() == NORMAL) {
    if (normal_type_cache()->IsUndefined()) return -1;
    CodeCacheHashTable* cache = CodeCacheHashTable::cast(normal_type_cache());
    return cache->GetIndex(String::cast(name), code->flags());
  }

  FixedArray* array = default_cache();
  int len = array->length();
  for (int i = 0; i < len; i += kCodeCacheEntrySize) {
    if (array->get(i + kCodeCacheEntryCodeOffset) == code) return i + 1;
  }
  return -1;
}


void CodeCache::RemoveByIndex(Object* name, Code* code, int index) {
  if (code->type() == NORMAL) {
    ASSERT(!normal_type_cache()->IsUndefined());
    CodeCacheHashTable* cache = CodeCacheHashTable::cast(normal_type_cache());
    ASSERT(cache->GetIndex(String::cast(name), code->flags()) == index);
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
  CodeCacheHashTableKey(String* name, Code::Flags flags)
      : name_(name), flags_(flags), code_(NULL) { }

  CodeCacheHashTableKey(String* name, Code* code)
      : name_(name),
        flags_(code->flags()),
        code_(code) { }


  bool IsMatch(Object* other) {
    if (!other->IsFixedArray()) return false;
    FixedArray* pair = FixedArray::cast(other);
    String* name = String::cast(pair->get(0));
    Code::Flags flags = Code::cast(pair->get(1))->flags();
    if (flags != flags_) {
      return false;
    }
    return name_->Equals(name);
  }

  static uint32_t NameFlagsHashHelper(String* name, Code::Flags flags) {
    return name->Hash() ^ flags;
  }

  uint32_t Hash() { return NameFlagsHashHelper(name_, flags_); }

  uint32_t HashForObject(Object* obj) {
    FixedArray* pair = FixedArray::cast(obj);
    String* name = String::cast(pair->get(0));
    Code* code = Code::cast(pair->get(1));
    return NameFlagsHashHelper(name, code->flags());
  }

  MUST_USE_RESULT MaybeObject* AsObject() {
    ASSERT(code_ != NULL);
    Object* obj;
    { MaybeObject* maybe_obj = code_->GetHeap()->AllocateFixedArray(2);
      if (!maybe_obj->ToObject(&obj)) return maybe_obj;
    }
    FixedArray* pair = FixedArray::cast(obj);
    pair->set(0, name_);
    pair->set(1, code_);
    return pair;
  }

 private:
  String* name_;
  Code::Flags flags_;
  // TODO(jkummerow): We should be able to get by without this.
  Code* code_;
};


Object* CodeCacheHashTable::Lookup(String* name, Code::Flags flags) {
  CodeCacheHashTableKey key(name, flags);
  int entry = FindEntry(&key);
  if (entry == kNotFound) return GetHeap()->undefined_value();
  return get(EntryToIndex(entry) + 1);
}


MaybeObject* CodeCacheHashTable::Put(String* name, Code* code) {
  CodeCacheHashTableKey key(name, code);
  Object* obj;
  { MaybeObject* maybe_obj = EnsureCapacity(1, &key);
    if (!maybe_obj->ToObject(&obj)) return maybe_obj;
  }

  // Don't use |this|, as the table might have grown.
  CodeCacheHashTable* cache = reinterpret_cast<CodeCacheHashTable*>(obj);

  int entry = cache->FindInsertionEntry(key.Hash());
  Object* k;
  { MaybeObject* maybe_k = key.AsObject();
    if (!maybe_k->ToObject(&k)) return maybe_k;
  }

  cache->set(EntryToIndex(entry), k);
  cache->set(EntryToIndex(entry) + 1, code);
  cache->ElementAdded();
  return cache;
}


int CodeCacheHashTable::GetIndex(String* name, Code::Flags flags) {
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
    return Handle<Object>(hash_table->Lookup(maps, flags));
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

  MUST_USE_RESULT MaybeObject* AsObject() {
    Object* obj;
    // The maps in |maps_| must be copied to a newly allocated FixedArray,
    // both because the referenced MapList is short-lived, and because C++
    // objects can't be stored in the heap anyway.
    { MaybeObject* maybe_obj =
        HEAP->AllocateUninitializedFixedArray(maps_->length() + 1);
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
  { MaybeObject* maybe_obj = key.AsObject();
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
      ASSERT(current->IsNumber() || current->IsString());
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
  if (!maybe_result->To<FixedArray>(&result)) return maybe_result;
#ifdef DEBUG
  if (FLAG_enable_slow_asserts) {
    for (int i = 0; i < result->length(); i++) {
      Object* current = result->get(i);
      ASSERT(current->IsNumber() || current->IsString());
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
  AssertNoAllocation no_gc;
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
  AssertNoAllocation no_gc;
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


MaybeObject* DescriptorArray::Allocate(int number_of_descriptors) {
  Heap* heap = Isolate::Current()->heap();
  if (number_of_descriptors == 0) {
    return heap->empty_descriptor_array();
  }
  // Allocate the array of keys.
  Object* array;
  { MaybeObject* maybe_array =
        heap->AllocateFixedArray(ToKeyIndex(number_of_descriptors));
    if (!maybe_array->ToObject(&array)) return maybe_array;
  }
  // Do not use DescriptorArray::cast on incomplete object.
  FixedArray* result = FixedArray::cast(array);

  // Allocate the content array and set it in the descriptor array.
  { MaybeObject* maybe_array =
        heap->AllocateFixedArray(number_of_descriptors << 1);
    if (!maybe_array->ToObject(&array)) return maybe_array;
  }
  result->set(kBitField3StorageIndex, Smi::FromInt(0));
  result->set(kContentArrayIndex, array);
  result->set(kEnumerationIndexIndex,
              Smi::FromInt(PropertyDetails::kInitialIndex));
  return result;
}


void DescriptorArray::SetEnumCache(FixedArray* bridge_storage,
                                   FixedArray* new_cache,
                                   Object* new_index_cache) {
  ASSERT(bridge_storage->length() >= kEnumCacheBridgeLength);
  ASSERT(new_index_cache->IsSmi() || new_index_cache->IsFixedArray());
  if (HasEnumCache()) {
    FixedArray::cast(get(kEnumerationIndexIndex))->
      set(kEnumCacheBridgeCacheIndex, new_cache);
    FixedArray::cast(get(kEnumerationIndexIndex))->
      set(kEnumCacheBridgeIndicesCacheIndex, new_index_cache);
  } else {
    if (IsEmpty()) return;  // Do nothing for empty descriptor array.
    FixedArray::cast(bridge_storage)->
      set(kEnumCacheBridgeCacheIndex, new_cache);
    FixedArray::cast(bridge_storage)->
      set(kEnumCacheBridgeIndicesCacheIndex, new_index_cache);
    NoWriteBarrierSet(FixedArray::cast(bridge_storage),
                      kEnumCacheBridgeEnumIndex,
                      get(kEnumerationIndexIndex));
    set(kEnumerationIndexIndex, bridge_storage);
  }
}


static bool InsertionPointFound(String* key1, String* key2) {
  return key1->Hash() > key2->Hash() || key1 == key2;
}


void DescriptorArray::CopyFrom(Handle<DescriptorArray> dst,
                               int dst_index,
                               Handle<DescriptorArray> src,
                               int src_index,
                               const WhitenessWitness& witness) {
  CALL_HEAP_FUNCTION_VOID(dst->GetIsolate(),
                          dst->CopyFrom(dst_index, *src, src_index, witness));
}


MaybeObject* DescriptorArray::CopyFrom(int dst_index,
                                       DescriptorArray* src,
                                       int src_index,
                                       const WhitenessWitness& witness) {
  Object* value = src->GetValue(src_index);
  PropertyDetails details(src->GetDetails(src_index));
  if (details.type() == CALLBACKS && value->IsAccessorPair()) {
    MaybeObject* maybe_copy =
        AccessorPair::cast(value)->CopyWithoutTransitions();
    if (!maybe_copy->To(&value)) return maybe_copy;
  }
  Descriptor desc(src->GetKey(src_index), value, details);
  Set(dst_index, &desc, witness);
  return this;
}


MaybeObject* DescriptorArray::CopyInsert(Descriptor* descriptor,
                                         TransitionFlag transition_flag) {
  // Transitions are only kept when inserting another transition.
  // This precondition is not required by this function's implementation, but
  // is currently required by the semantics of maps, so we check it.
  // Conversely, we filter after replacing, so replacing a transition and
  // removing all other transitions is not supported.
  bool remove_transitions = transition_flag == REMOVE_TRANSITIONS;
  ASSERT(remove_transitions == !descriptor->ContainsTransition());
  ASSERT(descriptor->GetDetails().type() != NULL_DESCRIPTOR);

  // Ensure the key is a symbol.
  { MaybeObject* maybe_result = descriptor->KeyToSymbol();
    if (maybe_result->IsFailure()) return maybe_result;
  }

  int new_size = 0;
  for (int i = 0; i < number_of_descriptors(); i++) {
    if (IsNullDescriptor(i)) continue;
    if (remove_transitions && IsTransitionOnly(i)) continue;
    new_size++;
  }

  // If key is in descriptor, we replace it in-place when filtering.
  // Count a null descriptor for key as inserted, not replaced.
  int index = Search(descriptor->GetKey());
  const bool replacing = (index != kNotFound);
  bool keep_enumeration_index = false;
  if (replacing) {
    // We are replacing an existing descriptor.  We keep the enumeration
    // index of a visible property.
    PropertyType t = PropertyDetails(GetDetails(index)).type();
    if (t == CONSTANT_FUNCTION ||
        t == FIELD ||
        t == CALLBACKS ||
        t == INTERCEPTOR) {
      keep_enumeration_index = true;
    } else if (remove_transitions) {
     // Replaced descriptor has been counted as removed if it is
     // a transition that will be replaced.  Adjust count in this case.
      ++new_size;
    }
  } else {
    ++new_size;
  }

  DescriptorArray* new_descriptors;
  { MaybeObject* maybe_result = Allocate(new_size);
    if (!maybe_result->To(&new_descriptors)) return maybe_result;
  }

  DescriptorArray::WhitenessWitness witness(new_descriptors);

  // Set the enumeration index in the descriptors and set the enumeration index
  // in the result.
  int enumeration_index = NextEnumerationIndex();
  if (!descriptor->ContainsTransition()) {
    if (keep_enumeration_index) {
      descriptor->SetEnumerationIndex(
          PropertyDetails(GetDetails(index)).index());
    } else {
      descriptor->SetEnumerationIndex(enumeration_index);
      ++enumeration_index;
    }
  }
  new_descriptors->SetNextEnumerationIndex(enumeration_index);

  // Copy the descriptors, filtering out transitions and null descriptors,
  // and inserting or replacing a descriptor.
  int to_index = 0;
  int insertion_index = -1;
  int from_index = 0;
  while (from_index < number_of_descriptors()) {
    if (insertion_index < 0 &&
        InsertionPointFound(GetKey(from_index), descriptor->GetKey())) {
      insertion_index = to_index++;
      if (replacing) from_index++;
    } else {
      if (!(IsNullDescriptor(from_index) ||
            (remove_transitions && IsTransitionOnly(from_index)))) {
        MaybeObject* copy_result =
            new_descriptors->CopyFrom(to_index++, this, from_index, witness);
        if (copy_result->IsFailure()) return copy_result;
      }
      from_index++;
    }
  }
  if (insertion_index < 0) insertion_index = to_index++;
  new_descriptors->Set(insertion_index, descriptor, witness);

  ASSERT(to_index == new_descriptors->number_of_descriptors());
  SLOW_ASSERT(new_descriptors->IsSortedNoDuplicates());

  return new_descriptors;
}


MaybeObject* DescriptorArray::RemoveTransitions() {
  // Allocate the new descriptor array.
  int new_number_of_descriptors = 0;
  for (int i = 0; i < number_of_descriptors(); i++) {
    if (IsProperty(i)) new_number_of_descriptors++;
  }
  DescriptorArray* new_descriptors;
  { MaybeObject* maybe_result = Allocate(new_number_of_descriptors);
    if (!maybe_result->To(&new_descriptors)) return maybe_result;
  }

  // Copy the content.
  DescriptorArray::WhitenessWitness witness(new_descriptors);
  int next_descriptor = 0;
  for (int i = 0; i < number_of_descriptors(); i++) {
    if (IsProperty(i)) {
      MaybeObject* copy_result =
          new_descriptors->CopyFrom(next_descriptor++, this, i, witness);
      if (copy_result->IsFailure()) return copy_result;
    }
  }
  ASSERT(next_descriptor == new_descriptors->number_of_descriptors());

  return new_descriptors;
}


void DescriptorArray::SortUnchecked(const WhitenessWitness& witness) {
  // In-place heap sort.
  int len = number_of_descriptors();

  // Bottom-up max-heap construction.
  // Index of the last node with children
  const int max_parent_index = (len / 2) - 1;
  for (int i = max_parent_index; i >= 0; --i) {
    int parent_index = i;
    const uint32_t parent_hash = GetKey(i)->Hash();
    while (parent_index <= max_parent_index) {
      int child_index = 2 * parent_index + 1;
      uint32_t child_hash = GetKey(child_index)->Hash();
      if (child_index + 1 < len) {
        uint32_t right_child_hash = GetKey(child_index + 1)->Hash();
        if (right_child_hash > child_hash) {
          child_index++;
          child_hash = right_child_hash;
        }
      }
      if (child_hash <= parent_hash) break;
      NoIncrementalWriteBarrierSwapDescriptors(parent_index, child_index);
      // Now element at child_index could be < its children.
      parent_index = child_index;  // parent_hash remains correct.
    }
  }

  // Extract elements and create sorted array.
  for (int i = len - 1; i > 0; --i) {
    // Put max element at the back of the array.
    NoIncrementalWriteBarrierSwapDescriptors(0, i);
    // Shift down the new top element.
    int parent_index = 0;
    const uint32_t parent_hash = GetKey(parent_index)->Hash();
    const int max_parent_index = (i / 2) - 1;
    while (parent_index <= max_parent_index) {
      int child_index = parent_index * 2 + 1;
      uint32_t child_hash = GetKey(child_index)->Hash();
      if (child_index + 1 < i) {
        uint32_t right_child_hash = GetKey(child_index + 1)->Hash();
        if (right_child_hash > child_hash) {
          child_index++;
          child_hash = right_child_hash;
        }
      }
      if (child_hash <= parent_hash) break;
      NoIncrementalWriteBarrierSwapDescriptors(parent_index, child_index);
      parent_index = child_index;
    }
  }
}


void DescriptorArray::Sort(const WhitenessWitness& witness) {
  SortUnchecked(witness);
  SLOW_ASSERT(IsSortedNoDuplicates());
}


int DescriptorArray::BinarySearch(String* name, int low, int high) {
  uint32_t hash = name->Hash();

  while (low <= high) {
    int mid = (low + high) / 2;
    String* mid_name = GetKey(mid);
    uint32_t mid_hash = mid_name->Hash();

    if (mid_hash > hash) {
      high = mid - 1;
      continue;
    }
    if (mid_hash < hash) {
      low = mid + 1;
      continue;
    }
    // Found an element with the same hash-code.
    ASSERT(hash == mid_hash);
    // There might be more, so we find the first one and
    // check them all to see if we have a match.
    if (name == mid_name  && !is_null_descriptor(mid)) return mid;
    while ((mid > low) && (GetKey(mid - 1)->Hash() == hash)) mid--;
    for (; (mid <= high) && (GetKey(mid)->Hash() == hash); mid++) {
      if (GetKey(mid)->Equals(name) && !is_null_descriptor(mid)) return mid;
    }
    break;
  }
  return kNotFound;
}


int DescriptorArray::LinearSearch(String* name, int len) {
  uint32_t hash = name->Hash();
  for (int number = 0; number < len; number++) {
    String* entry = GetKey(number);
    if ((entry->Hash() == hash) &&
        name->Equals(entry) &&
        !is_null_descriptor(number)) {
      return number;
    }
  }
  return kNotFound;
}


MaybeObject* AccessorPair::CopyWithoutTransitions() {
  Heap* heap = GetHeap();
  AccessorPair* copy;
  { MaybeObject* maybe_copy = heap->AllocateAccessorPair();
    if (!maybe_copy->To(&copy)) return maybe_copy;
  }
  copy->set_getter(getter()->IsMap() ? heap->the_hole_value() : getter());
  copy->set_setter(setter()->IsMap() ? heap->the_hole_value() : setter());
  return copy;
}


Object* AccessorPair::GetComponent(AccessorComponent component) {
    Object* accessor = (component == ACCESSOR_GETTER) ? getter() : setter();
    return accessor->IsTheHole() ? GetHeap()->undefined_value() : accessor;
}


MaybeObject* DeoptimizationInputData::Allocate(int deopt_entry_count,
                                               PretenureFlag pretenure) {
  ASSERT(deopt_entry_count > 0);
  return HEAP->AllocateFixedArray(LengthFor(deopt_entry_count),
                                  pretenure);
}


MaybeObject* DeoptimizationOutputData::Allocate(int number_of_deopt_points,
                                                PretenureFlag pretenure) {
  if (number_of_deopt_points == 0) return HEAP->empty_fixed_array();
  return HEAP->AllocateFixedArray(LengthOfFixedArray(number_of_deopt_points),
                                  pretenure);
}


#ifdef DEBUG
bool DescriptorArray::IsEqualTo(DescriptorArray* other) {
  if (IsEmpty()) return other->IsEmpty();
  if (other->IsEmpty()) return false;
  if (length() != other->length()) return false;
  for (int i = 0; i < length(); ++i) {
    if (get(i) != other->get(i) && i != kContentArrayIndex) return false;
  }
  return GetContentArray()->IsEqualTo(other->GetContentArray());
}
#endif


bool String::LooksValid() {
  if (!Isolate::Current()->heap()->Contains(this)) return false;
  return true;
}


String::FlatContent String::GetFlatContent() {
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
  if (shape.encoding_tag() == kAsciiStringTag) {
    const char* start;
    if (shape.representation_tag() == kSeqStringTag) {
      start = SeqAsciiString::cast(string)->GetChars();
    } else {
      start = ExternalAsciiString::cast(string)->GetChars();
    }
    return FlatContent(Vector<const char>(start + offset, length));
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
  Access<StringInputBuffer> buffer(
      heap->isolate()->objects_string_input_buffer());
  buffer->Reset(offset, this);
  int character_position = offset;
  int utf8_bytes = 0;
  int last = unibrow::Utf16::kNoPreviousCharacter;
  while (buffer->has_more() && character_position++ < offset + length) {
    uint16_t character = buffer->GetNext();
    utf8_bytes += unibrow::Utf8::Length(character, last);
    last = character;
  }

  if (length_return) {
    *length_return = utf8_bytes;
  }

  char* result = NewArray<char>(utf8_bytes + 1);

  // Convert the UTF-16 string to a UTF-8 buffer. Start at the specified offset.
  buffer->Rewind();
  buffer->Seek(offset);
  character_position = offset;
  int utf8_byte_position = 0;
  last = unibrow::Utf16::kNoPreviousCharacter;
  while (buffer->has_more() && character_position++ < offset + length) {
    uint16_t character = buffer->GetNext();
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
  ASSERT(!IsAsciiRepresentationUnderneath());
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

  Access<StringInputBuffer> buffer(
      heap->isolate()->objects_string_input_buffer());
  buffer->Reset(this);

  uc16* result = NewArray<uc16>(length() + 1);

  int i = 0;
  while (buffer->has_more()) {
    uint16_t character = buffer->GetNext();
    result[i++] = character;
  }
  result[i] = 0;
  return SmartArrayPointer<uc16>(result);
}


const uc16* SeqTwoByteString::SeqTwoByteStringGetData(unsigned start) {
  return reinterpret_cast<uc16*>(
      reinterpret_cast<char*>(this) - kHeapObjectTag + kHeaderSize) + start;
}


void SeqTwoByteString::SeqTwoByteStringReadBlockIntoBuffer(ReadBlockBuffer* rbb,
                                                           unsigned* offset_ptr,
                                                           unsigned max_chars) {
  unsigned chars_read = 0;
  unsigned offset = *offset_ptr;
  while (chars_read < max_chars) {
    uint16_t c = *reinterpret_cast<uint16_t*>(
        reinterpret_cast<char*>(this) -
            kHeapObjectTag + kHeaderSize + offset * kShortSize);
    if (c <= kMaxAsciiCharCode) {
      // Fast case for ASCII characters.   Cursor is an input output argument.
      if (!unibrow::CharacterStream::EncodeAsciiCharacter(c,
                                                          rbb->util_buffer,
                                                          rbb->capacity,
                                                          rbb->cursor)) {
        break;
      }
    } else {
      if (!unibrow::CharacterStream::EncodeNonAsciiCharacter(c,
                                                             rbb->util_buffer,
                                                             rbb->capacity,
                                                             rbb->cursor)) {
        break;
      }
    }
    offset++;
    chars_read++;
  }
  *offset_ptr = offset;
  rbb->remaining += chars_read;
}


const unibrow::byte* SeqAsciiString::SeqAsciiStringReadBlock(
    unsigned* remaining,
    unsigned* offset_ptr,
    unsigned max_chars) {
  const unibrow::byte* b = reinterpret_cast<unibrow::byte*>(this) -
      kHeapObjectTag + kHeaderSize + *offset_ptr * kCharSize;
  *remaining = max_chars;
  *offset_ptr += max_chars;
  return b;
}


// This will iterate unless the block of string data spans two 'halves' of
// a ConsString, in which case it will recurse.  Since the block of string
// data to be read has a maximum size this limits the maximum recursion
// depth to something sane.  Since C++ does not have tail call recursion
// elimination, the iteration must be explicit. Since this is not an
// -IntoBuffer method it can delegate to one of the efficient
// *AsciiStringReadBlock routines.
const unibrow::byte* ConsString::ConsStringReadBlock(ReadBlockBuffer* rbb,
                                                     unsigned* offset_ptr,
                                                     unsigned max_chars) {
  ConsString* current = this;
  unsigned offset = *offset_ptr;
  int offset_correction = 0;

  while (true) {
    String* left = current->first();
    unsigned left_length = (unsigned)left->length();
    if (left_length > offset &&
        (max_chars <= left_length - offset ||
         (rbb->capacity <= left_length - offset &&
          (max_chars = left_length - offset, true)))) {  // comma operator!
      // Left hand side only - iterate unless we have reached the bottom of
      // the cons tree.  The assignment on the left of the comma operator is
      // in order to make use of the fact that the -IntoBuffer routines can
      // produce at most 'capacity' characters.  This enables us to postpone
      // the point where we switch to the -IntoBuffer routines (below) in order
      // to maximize the chances of delegating a big chunk of work to the
      // efficient *AsciiStringReadBlock routines.
      if (StringShape(left).IsCons()) {
        current = ConsString::cast(left);
        continue;
      } else {
        const unibrow::byte* answer =
            String::ReadBlock(left, rbb, &offset, max_chars);
        *offset_ptr = offset + offset_correction;
        return answer;
      }
    } else if (left_length <= offset) {
      // Right hand side only - iterate unless we have reached the bottom of
      // the cons tree.
      String* right = current->second();
      offset -= left_length;
      offset_correction += left_length;
      if (StringShape(right).IsCons()) {
        current = ConsString::cast(right);
        continue;
      } else {
        const unibrow::byte* answer =
            String::ReadBlock(right, rbb, &offset, max_chars);
        *offset_ptr = offset + offset_correction;
        return answer;
      }
    } else {
      // The block to be read spans two sides of the ConsString, so we call the
      // -IntoBuffer version, which will recurse.  The -IntoBuffer methods
      // are able to assemble data from several part strings because they use
      // the util_buffer to store their data and never return direct pointers
      // to their storage.  We don't try to read more than the buffer capacity
      // here or we can get too much recursion.
      ASSERT(rbb->remaining == 0);
      ASSERT(rbb->cursor == 0);
      current->ConsStringReadBlockIntoBuffer(
          rbb,
          &offset,
          max_chars > rbb->capacity ? rbb->capacity : max_chars);
      *offset_ptr = offset + offset_correction;
      return rbb->util_buffer;
    }
  }
}


const unibrow::byte* ExternalAsciiString::ExternalAsciiStringReadBlock(
      unsigned* remaining,
      unsigned* offset_ptr,
      unsigned max_chars) {
  // Cast const char* to unibrow::byte* (signedness difference).
  const unibrow::byte* b =
      reinterpret_cast<const unibrow::byte*>(GetChars()) + *offset_ptr;
  *remaining = max_chars;
  *offset_ptr += max_chars;
  return b;
}


void ExternalTwoByteString::ExternalTwoByteStringReadBlockIntoBuffer(
      ReadBlockBuffer* rbb,
      unsigned* offset_ptr,
      unsigned max_chars) {
  unsigned chars_read = 0;
  unsigned offset = *offset_ptr;
  const uint16_t* data = GetChars();
  while (chars_read < max_chars) {
    uint16_t c = data[offset];
    if (c <= kMaxAsciiCharCode) {
      // Fast case for ASCII characters. Cursor is an input output argument.
      if (!unibrow::CharacterStream::EncodeAsciiCharacter(c,
                                                          rbb->util_buffer,
                                                          rbb->capacity,
                                                          rbb->cursor))
        break;
    } else {
      if (!unibrow::CharacterStream::EncodeNonAsciiCharacter(c,
                                                             rbb->util_buffer,
                                                             rbb->capacity,
                                                             rbb->cursor))
        break;
    }
    offset++;
    chars_read++;
  }
  *offset_ptr = offset;
  rbb->remaining += chars_read;
}


void SeqAsciiString::SeqAsciiStringReadBlockIntoBuffer(ReadBlockBuffer* rbb,
                                                 unsigned* offset_ptr,
                                                 unsigned max_chars) {
  unsigned capacity = rbb->capacity - rbb->cursor;
  if (max_chars > capacity) max_chars = capacity;
  memcpy(rbb->util_buffer + rbb->cursor,
         reinterpret_cast<char*>(this) - kHeapObjectTag + kHeaderSize +
             *offset_ptr * kCharSize,
         max_chars);
  rbb->remaining += max_chars;
  *offset_ptr += max_chars;
  rbb->cursor += max_chars;
}


void ExternalAsciiString::ExternalAsciiStringReadBlockIntoBuffer(
      ReadBlockBuffer* rbb,
      unsigned* offset_ptr,
      unsigned max_chars) {
  unsigned capacity = rbb->capacity - rbb->cursor;
  if (max_chars > capacity) max_chars = capacity;
  memcpy(rbb->util_buffer + rbb->cursor, GetChars() + *offset_ptr, max_chars);
  rbb->remaining += max_chars;
  *offset_ptr += max_chars;
  rbb->cursor += max_chars;
}


// This method determines the type of string involved and then copies
// a whole chunk of characters into a buffer, or returns a pointer to a buffer
// where they can be found.  The pointer is not necessarily valid across a GC
// (see AsciiStringReadBlock).
const unibrow::byte* String::ReadBlock(String* input,
                                       ReadBlockBuffer* rbb,
                                       unsigned* offset_ptr,
                                       unsigned max_chars) {
  ASSERT(*offset_ptr <= static_cast<unsigned>(input->length()));
  if (max_chars == 0) {
    rbb->remaining = 0;
    return NULL;
  }
  switch (StringShape(input).representation_tag()) {
    case kSeqStringTag:
      if (input->IsAsciiRepresentation()) {
        SeqAsciiString* str = SeqAsciiString::cast(input);
        return str->SeqAsciiStringReadBlock(&rbb->remaining,
                                            offset_ptr,
                                            max_chars);
      } else {
        SeqTwoByteString* str = SeqTwoByteString::cast(input);
        str->SeqTwoByteStringReadBlockIntoBuffer(rbb,
                                                 offset_ptr,
                                                 max_chars);
        return rbb->util_buffer;
      }
    case kConsStringTag:
      return ConsString::cast(input)->ConsStringReadBlock(rbb,
                                                          offset_ptr,
                                                          max_chars);
    case kExternalStringTag:
      if (input->IsAsciiRepresentation()) {
        return ExternalAsciiString::cast(input)->ExternalAsciiStringReadBlock(
            &rbb->remaining,
            offset_ptr,
            max_chars);
      } else {
        ExternalTwoByteString::cast(input)->
            ExternalTwoByteStringReadBlockIntoBuffer(rbb,
                                                     offset_ptr,
                                                     max_chars);
        return rbb->util_buffer;
      }
    case kSlicedStringTag:
      return SlicedString::cast(input)->SlicedStringReadBlock(rbb,
                                                              offset_ptr,
                                                              max_chars);
    default:
      break;
  }

  UNREACHABLE();
  return 0;
}


void Relocatable::PostGarbageCollectionProcessing() {
  Isolate* isolate = Isolate::Current();
  Relocatable* current = isolate->relocatable_top();
  while (current != NULL) {
    current->PostGarbageCollection();
    current = current->prev_;
  }
}


// Reserve space for statics needing saving and restoring.
int Relocatable::ArchiveSpacePerThread() {
  return sizeof(Isolate::Current()->relocatable_top());
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


void Relocatable::Iterate(ObjectVisitor* v) {
  Isolate* isolate = Isolate::Current();
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
  String::FlatContent content = str->GetFlatContent();
  ASSERT(content.IsFlat());
  is_ascii_ = content.IsAscii();
  if (is_ascii_) {
    start_ = content.ToAsciiVector().start();
  } else {
    start_ = content.ToUC16Vector().start();
  }
}


void StringInputBuffer::Seek(unsigned pos) {
  Reset(pos, input_);
}


void SafeStringInputBuffer::Seek(unsigned pos) {
  Reset(pos, input_);
}


// This method determines the type of string involved and then copies
// a whole chunk of characters into a buffer.  It can be used with strings
// that have been glued together to form a ConsString and which must cooperate
// to fill up a buffer.
void String::ReadBlockIntoBuffer(String* input,
                                 ReadBlockBuffer* rbb,
                                 unsigned* offset_ptr,
                                 unsigned max_chars) {
  ASSERT(*offset_ptr <= (unsigned)input->length());
  if (max_chars == 0) return;

  switch (StringShape(input).representation_tag()) {
    case kSeqStringTag:
      if (input->IsAsciiRepresentation()) {
        SeqAsciiString::cast(input)->SeqAsciiStringReadBlockIntoBuffer(rbb,
                                                                 offset_ptr,
                                                                 max_chars);
        return;
      } else {
        SeqTwoByteString::cast(input)->SeqTwoByteStringReadBlockIntoBuffer(rbb,
                                                                     offset_ptr,
                                                                     max_chars);
        return;
      }
    case kConsStringTag:
      ConsString::cast(input)->ConsStringReadBlockIntoBuffer(rbb,
                                                             offset_ptr,
                                                             max_chars);
      return;
    case kExternalStringTag:
      if (input->IsAsciiRepresentation()) {
        ExternalAsciiString::cast(input)->
            ExternalAsciiStringReadBlockIntoBuffer(rbb, offset_ptr, max_chars);
      } else {
        ExternalTwoByteString::cast(input)->
            ExternalTwoByteStringReadBlockIntoBuffer(rbb,
                                                     offset_ptr,
                                                     max_chars);
       }
       return;
    case kSlicedStringTag:
      SlicedString::cast(input)->SlicedStringReadBlockIntoBuffer(rbb,
                                                                 offset_ptr,
                                                                 max_chars);
      return;
    default:
      break;
  }

  UNREACHABLE();
  return;
}


const unibrow::byte* String::ReadBlock(String* input,
                                       unibrow::byte* util_buffer,
                                       unsigned capacity,
                                       unsigned* remaining,
                                       unsigned* offset_ptr) {
  ASSERT(*offset_ptr <= (unsigned)input->length());
  unsigned chars = input->length() - *offset_ptr;
  ReadBlockBuffer rbb(util_buffer, 0, capacity, 0);
  const unibrow::byte* answer = ReadBlock(input, &rbb, offset_ptr, chars);
  ASSERT(rbb.remaining <= static_cast<unsigned>(input->length()));
  *remaining = rbb.remaining;
  return answer;
}


const unibrow::byte* String::ReadBlock(String** raw_input,
                                       unibrow::byte* util_buffer,
                                       unsigned capacity,
                                       unsigned* remaining,
                                       unsigned* offset_ptr) {
  Handle<String> input(raw_input);
  ASSERT(*offset_ptr <= (unsigned)input->length());
  unsigned chars = input->length() - *offset_ptr;
  if (chars > capacity) chars = capacity;
  ReadBlockBuffer rbb(util_buffer, 0, capacity, 0);
  ReadBlockIntoBuffer(*input, &rbb, offset_ptr, chars);
  ASSERT(rbb.remaining <= static_cast<unsigned>(input->length()));
  *remaining = rbb.remaining;
  return rbb.util_buffer;
}


// This will iterate unless the block of string data spans two 'halves' of
// a ConsString, in which case it will recurse.  Since the block of string
// data to be read has a maximum size this limits the maximum recursion
// depth to something sane.  Since C++ does not have tail call recursion
// elimination, the iteration must be explicit.
void ConsString::ConsStringReadBlockIntoBuffer(ReadBlockBuffer* rbb,
                                               unsigned* offset_ptr,
                                               unsigned max_chars) {
  ConsString* current = this;
  unsigned offset = *offset_ptr;
  int offset_correction = 0;

  while (true) {
    String* left = current->first();
    unsigned left_length = (unsigned)left->length();
    if (left_length > offset &&
      max_chars <= left_length - offset) {
      // Left hand side only - iterate unless we have reached the bottom of
      // the cons tree.
      if (StringShape(left).IsCons()) {
        current = ConsString::cast(left);
        continue;
      } else {
        String::ReadBlockIntoBuffer(left, rbb, &offset, max_chars);
        *offset_ptr = offset + offset_correction;
        return;
      }
    } else if (left_length <= offset) {
      // Right hand side only - iterate unless we have reached the bottom of
      // the cons tree.
      offset -= left_length;
      offset_correction += left_length;
      String* right = current->second();
      if (StringShape(right).IsCons()) {
        current = ConsString::cast(right);
        continue;
      } else {
        String::ReadBlockIntoBuffer(right, rbb, &offset, max_chars);
        *offset_ptr = offset + offset_correction;
        return;
      }
    } else {
      // The block to be read spans two sides of the ConsString, so we recurse.
      // First recurse on the left.
      max_chars -= left_length - offset;
      String::ReadBlockIntoBuffer(left, rbb, &offset, left_length - offset);
      // We may have reached the max or there may not have been enough space
      // in the buffer for the characters in the left hand side.
      if (offset == left_length) {
        // Recurse on the right.
        String* right = String::cast(current->second());
        offset -= left_length;
        offset_correction += left_length;
        String::ReadBlockIntoBuffer(right, rbb, &offset, max_chars);
      }
      *offset_ptr = offset + offset_correction;
      return;
    }
  }
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


const unibrow::byte* SlicedString::SlicedStringReadBlock(
    ReadBlockBuffer* buffer, unsigned* offset_ptr, unsigned chars) {
  unsigned offset = this->offset();
  *offset_ptr += offset;
  const unibrow::byte* answer = String::ReadBlock(String::cast(parent()),
                                                  buffer, offset_ptr, chars);
  *offset_ptr -= offset;
  return answer;
}


void SlicedString::SlicedStringReadBlockIntoBuffer(
    ReadBlockBuffer* buffer, unsigned* offset_ptr, unsigned chars) {
  unsigned offset = this->offset();
  *offset_ptr += offset;
  String::ReadBlockIntoBuffer(String::cast(parent()),
                              buffer, offset_ptr, chars);
  *offset_ptr -= offset;
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
      case kAsciiStringTag | kExternalStringTag: {
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
      case kAsciiStringTag | kSeqStringTag: {
        CopyChars(sink,
                  SeqAsciiString::cast(source)->GetChars() + from,
                  to - from);
        return;
      }
      case kTwoByteStringTag | kSeqStringTag: {
        CopyChars(sink,
                  SeqTwoByteString::cast(source)->GetChars() + from,
                  to - from);
        return;
      }
      case kAsciiStringTag | kConsStringTag:
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
            } else if (second->IsSeqAsciiString()) {
              CopyChars(sink + boundary - from,
                        SeqAsciiString::cast(second)->GetChars(),
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
      case kAsciiStringTag | kSlicedStringTag:
      case kTwoByteStringTag | kSlicedStringTag: {
        SlicedString* slice = SlicedString::cast(source);
        unsigned offset = slice->offset();
        WriteToFlat(slice->parent(), sink, from + offset, to + offset);
        return;
      }
    }
  }
}


template <typename IteratorA, typename IteratorB>
static inline bool CompareStringContents(IteratorA* ia, IteratorB* ib) {
  // General slow case check.  We know that the ia and ib iterators
  // have the same length.
  while (ia->has_more()) {
    uint32_t ca = ia->GetNext();
    uint32_t cb = ib->GetNext();
    ASSERT(ca <= unibrow::Utf16::kMaxNonSurrogateCharCode);
    ASSERT(cb <= unibrow::Utf16::kMaxNonSurrogateCharCode);
    if (ca != cb)
      return false;
  }
  return true;
}


// Compares the contents of two strings by reading and comparing
// int-sized blocks of characters.
template <typename Char>
static inline bool CompareRawStringContents(Vector<Char> a, Vector<Char> b) {
  int length = a.length();
  ASSERT_EQ(length, b.length());
  const Char* pa = a.start();
  const Char* pb = b.start();
  int i = 0;
#ifndef V8_HOST_CAN_READ_UNALIGNED
  // If this architecture isn't comfortable reading unaligned ints
  // then we have to check that the strings are aligned before
  // comparing them blockwise.
  const int kAlignmentMask = sizeof(uint32_t) - 1;  // NOLINT
  uint32_t pa_addr = reinterpret_cast<uint32_t>(pa);
  uint32_t pb_addr = reinterpret_cast<uint32_t>(pb);
  if (((pa_addr & kAlignmentMask) | (pb_addr & kAlignmentMask)) == 0) {
#endif
    const int kStepSize = sizeof(int) / sizeof(Char);  // NOLINT
    int endpoint = length - kStepSize;
    // Compare blocks until we reach near the end of the string.
    for (; i <= endpoint; i += kStepSize) {
      uint32_t wa = *reinterpret_cast<const uint32_t*>(pa + i);
      uint32_t wb = *reinterpret_cast<const uint32_t*>(pb + i);
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


template <typename IteratorA>
static inline bool CompareStringContentsPartial(Isolate* isolate,
                                                IteratorA* ia,
                                                String* b) {
  String::FlatContent content = b->GetFlatContent();
  if (content.IsFlat()) {
    if (content.IsAscii()) {
      VectorIterator<char> ib(content.ToAsciiVector());
      return CompareStringContents(ia, &ib);
    } else {
      VectorIterator<uc16> ib(content.ToUC16Vector());
      return CompareStringContents(ia, &ib);
    }
  } else {
    isolate->objects_string_compare_buffer_b()->Reset(0, b);
    return CompareStringContents(ia,
                                 isolate->objects_string_compare_buffer_b());
  }
}


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

  if (StringShape(lhs).IsSequentialAscii() &&
      StringShape(rhs).IsSequentialAscii()) {
    const char* str1 = SeqAsciiString::cast(lhs)->GetChars();
    const char* str2 = SeqAsciiString::cast(rhs)->GetChars();
    return CompareRawStringContents(Vector<const char>(str1, len),
                                    Vector<const char>(str2, len));
  }

  Isolate* isolate = GetIsolate();
  String::FlatContent lhs_content = lhs->GetFlatContent();
  String::FlatContent rhs_content = rhs->GetFlatContent();
  if (lhs_content.IsFlat()) {
    if (lhs_content.IsAscii()) {
      Vector<const char> vec1 = lhs_content.ToAsciiVector();
      if (rhs_content.IsFlat()) {
        if (rhs_content.IsAscii()) {
          Vector<const char> vec2 = rhs_content.ToAsciiVector();
          return CompareRawStringContents(vec1, vec2);
        } else {
          VectorIterator<char> buf1(vec1);
          VectorIterator<uc16> ib(rhs_content.ToUC16Vector());
          return CompareStringContents(&buf1, &ib);
        }
      } else {
        VectorIterator<char> buf1(vec1);
        isolate->objects_string_compare_buffer_b()->Reset(0, rhs);
        return CompareStringContents(&buf1,
            isolate->objects_string_compare_buffer_b());
      }
    } else {
      Vector<const uc16> vec1 = lhs_content.ToUC16Vector();
      if (rhs_content.IsFlat()) {
        if (rhs_content.IsAscii()) {
          VectorIterator<uc16> buf1(vec1);
          VectorIterator<char> ib(rhs_content.ToAsciiVector());
          return CompareStringContents(&buf1, &ib);
        } else {
          Vector<const uc16> vec2(rhs_content.ToUC16Vector());
          return CompareRawStringContents(vec1, vec2);
        }
      } else {
        VectorIterator<uc16> buf1(vec1);
        isolate->objects_string_compare_buffer_b()->Reset(0, rhs);
        return CompareStringContents(&buf1,
            isolate->objects_string_compare_buffer_b());
      }
    }
  } else {
    isolate->objects_string_compare_buffer_a()->Reset(0, lhs);
    return CompareStringContentsPartial(isolate,
        isolate->objects_string_compare_buffer_a(), rhs);
  }
}


bool String::MarkAsUndetectable() {
  if (StringShape(this).IsSymbol()) return false;

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


bool String::IsEqualTo(Vector<const char> str) {
  Isolate* isolate = GetIsolate();
  int slen = length();
  Access<UnicodeCache::Utf8Decoder>
      decoder(isolate->unicode_cache()->utf8_decoder());
  decoder->Reset(str.start(), str.length());
  int i;
  for (i = 0; i < slen && decoder->has_more(); i++) {
    uint32_t r = decoder->GetNext();
    if (r > unibrow::Utf16::kMaxNonSurrogateCharCode) {
      if (i > slen - 1) return false;
      if (Get(i++) != unibrow::Utf16::LeadSurrogate(r)) return false;
      if (Get(i) != unibrow::Utf16::TrailSurrogate(r)) return false;
    } else {
      if (Get(i) != r) return false;
    }
  }
  return i == slen && !decoder->has_more();
}


bool String::IsAsciiEqualTo(Vector<const char> str) {
  int slen = length();
  if (str.length() != slen) return false;
  FlatContent content = GetFlatContent();
  if (content.IsAscii()) {
    return CompareChars(content.ToAsciiVector().start(),
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
  ASSERT(!HasHashCode());

  const int len = length();

  // Compute the hash code.
  uint32_t field = 0;
  if (StringShape(this).IsSequentialAscii()) {
    field = HashSequentialString(SeqAsciiString::cast(this)->GetChars(),
                                 len,
                                 GetHeap()->HashSeed());
  } else if (StringShape(this).IsSequentialTwoByte()) {
    field = HashSequentialString(SeqTwoByteString::cast(this)->GetChars(),
                                 len,
                                 GetHeap()->HashSeed());
  } else {
    StringInputBuffer buffer(this);
    field = ComputeHashField(&buffer, len, GetHeap()->HashSeed());
  }

  // Store the hash code in the object.
  set_hash_field(field);

  // Check the hash code is there.
  ASSERT(HasHashCode());
  uint32_t result = field >> kHashShift;
  ASSERT(result != 0);  // Ensure that the hash value of 0 is never computed.
  return result;
}


bool String::ComputeArrayIndex(unibrow::CharacterStream* buffer,
                               uint32_t* index,
                               int length) {
  if (length == 0 || length > kMaxArrayIndexSize) return false;
  uc32 ch = buffer->GetNext();

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
  while (buffer->has_more()) {
    d = buffer->GetNext() - '0';
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
    StringInputBuffer buffer(this);
    return ComputeArrayIndex(&buffer, index, length());
  }
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


void StringHasher::AddSurrogatePair(uc32 c) {
  uint16_t lead = unibrow::Utf16::LeadSurrogate(c);
  AddCharacter(lead);
  uint16_t trail = unibrow::Utf16::TrailSurrogate(c);
  AddCharacter(trail);
}


void StringHasher::AddSurrogatePairNoIndex(uc32 c) {
  uint16_t lead = unibrow::Utf16::LeadSurrogate(c);
  AddCharacterNoIndex(lead);
  uint16_t trail = unibrow::Utf16::TrailSurrogate(c);
  AddCharacterNoIndex(trail);
}


uint32_t StringHasher::GetHashField() {
  ASSERT(is_valid());
  if (length_ <= String::kMaxHashCalcLength) {
    if (is_array_index()) {
      return MakeArrayIndexHash(array_index(), length_);
    }
    return (GetHash() << String::kHashShift) | String::kIsNotArrayIndexMask;
  } else {
    return (length_ << String::kHashShift) | String::kIsNotArrayIndexMask;
  }
}


uint32_t String::ComputeHashField(unibrow::CharacterStream* buffer,
                                  int length,
                                  uint32_t seed) {
  StringHasher hasher(length, seed);

  // Very long strings have a trivial hash that doesn't inspect the
  // string contents.
  if (hasher.has_trivial_hash()) {
    return hasher.GetHashField();
  }

  // Do the iterative array index computation as long as there is a
  // chance this is an array index.
  while (buffer->has_more() && hasher.is_array_index()) {
    hasher.AddCharacter(buffer->GetNext());
  }

  // Process the remaining characters without updating the array
  // index.
  while (buffer->has_more()) {
    hasher.AddCharacterNoIndex(buffer->GetNext());
  }

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
    fprintf(file, "%c", Get(i));
  }
}


void Map::CreateOneBackPointer(Object* transition_target) {
  if (!transition_target->IsMap()) return;
  Map* target = Map::cast(transition_target);
#ifdef DEBUG
  // Verify target.
  Object* source_prototype = prototype();
  Object* target_prototype = target->prototype();
  ASSERT(source_prototype->IsJSReceiver() ||
         source_prototype->IsMap() ||
         source_prototype->IsNull());
  ASSERT(target_prototype->IsJSReceiver() ||
         target_prototype->IsNull());
  ASSERT(source_prototype->IsMap() ||
         source_prototype == target_prototype);
#endif
  // Point target back to source.  set_prototype() will not let us set
  // the prototype to a map, as we do here.
  *RawField(target, kPrototypeOffset) = this;
}


void Map::CreateBackPointers() {
  DescriptorArray* descriptors = instance_descriptors();
  for (int i = 0; i < descriptors->number_of_descriptors(); i++) {
    switch (descriptors->GetType(i)) {
      case MAP_TRANSITION:
      case CONSTANT_TRANSITION:
        CreateOneBackPointer(descriptors->GetValue(i));
        break;
      case ELEMENTS_TRANSITION: {
        Object* object = descriptors->GetValue(i);
        if (object->IsMap()) {
          CreateOneBackPointer(object);
        } else {
          FixedArray* array = FixedArray::cast(object);
          for (int i = 0; i < array->length(); ++i) {
            CreateOneBackPointer(array->get(i));
          }
        }
        break;
      }
      case CALLBACKS: {
        Object* object = descriptors->GetValue(i);
        if (object->IsAccessorPair()) {
          AccessorPair* accessors = AccessorPair::cast(object);
          CreateOneBackPointer(accessors->getter());
          CreateOneBackPointer(accessors->setter());
        }
        break;
      }
      case NORMAL:
      case FIELD:
      case CONSTANT_FUNCTION:
      case HANDLER:
      case INTERCEPTOR:
      case NULL_DESCRIPTOR:
        break;
    }
  }
}


bool Map::RestoreOneBackPointer(Object* object,
                                Object* real_prototype,
                                bool* keep_entry) {
  if (!object->IsMap()) return false;
  Map* map = Map::cast(object);
  if (Marking::MarkBitFrom(map).Get()) {
    *keep_entry = true;
    return false;
  }
  ASSERT(map->prototype() == this || map->prototype() == real_prototype);
  // Getter prototype() is read-only, set_prototype() has side effects.
  *RawField(map, Map::kPrototypeOffset) = real_prototype;
  return true;
}


void Map::ClearNonLiveTransitions(Heap* heap, Object* real_prototype) {
  DescriptorArray* d = DescriptorArray::cast(
      *RawField(this, Map::kInstanceDescriptorsOrBitField3Offset));
  if (d->IsEmpty()) return;
  Smi* NullDescriptorDetails =
    PropertyDetails(NONE, NULL_DESCRIPTOR).AsSmi();
  FixedArray* contents = FixedArray::cast(
      d->get(DescriptorArray::kContentArrayIndex));
  ASSERT(contents->length() >= 2);
  for (int i = 0; i < contents->length(); i += 2) {
    // If the pair (value, details) is a map transition, check if the target is
    // live. If not, null the descriptor. Also drop the back pointer for that
    // map transition, so that this map is not reached again by following a back
    // pointer from a non-live object.
    bool keep_entry = false;
    PropertyDetails details(Smi::cast(contents->get(i + 1)));
    switch (details.type()) {
      case MAP_TRANSITION:
      case CONSTANT_TRANSITION:
        RestoreOneBackPointer(contents->get(i), real_prototype, &keep_entry);
        break;
      case ELEMENTS_TRANSITION: {
        Object* object = contents->get(i);
        if (object->IsMap()) {
          RestoreOneBackPointer(object, real_prototype, &keep_entry);
        } else {
          FixedArray* array = FixedArray::cast(object);
          for (int j = 0; j < array->length(); ++j) {
            if (RestoreOneBackPointer(array->get(j),
                                      real_prototype,
                                      &keep_entry)) {
              array->set_undefined(j);
            }
          }
        }
        break;
      }
      case CALLBACKS: {
        Object* object = contents->get(i);
        if (object->IsAccessorPair()) {
          AccessorPair* accessors = AccessorPair::cast(object);
          if (RestoreOneBackPointer(accessors->getter(),
                                    real_prototype,
                                    &keep_entry)) {
            accessors->set_getter(heap->the_hole_value());
          }
          if (RestoreOneBackPointer(accessors->setter(),
                                    real_prototype,
                                    &keep_entry)) {
            accessors->set_setter(heap->the_hole_value());
          }
        } else {
          keep_entry = true;
        }
        break;
      }
      case NORMAL:
      case FIELD:
      case CONSTANT_FUNCTION:
      case HANDLER:
      case INTERCEPTOR:
      case NULL_DESCRIPTOR:
        keep_entry = true;
        break;
    }
    // Make sure that an entry containing only dead transitions gets collected.
    // What we *really* want to do here is removing this entry completely, but
    // for technical reasons we can't do this, so we zero it out instead.
    if (!keep_entry) {
      contents->set_unchecked(i + 1, NullDescriptorDetails);
      contents->set_null_unchecked(heap, i);
    }
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


bool Map::EquivalentToForNormalization(Map* other,
                                       PropertyNormalizationMode mode) {
  return
    constructor() == other->constructor() &&
    prototype() == other->prototype() &&
    inobject_properties() == ((mode == CLEAR_INOBJECT_PROPERTIES) ?
                              0 :
                              other->inobject_properties()) &&
    instance_type() == other->instance_type() &&
    bit_field() == other->bit_field() &&
    bit_field2() == other->bit_field2() &&
    (bit_field3() & ~(1<<Map::kIsShared)) ==
        (other->bit_field3() & ~(1<<Map::kIsShared));
}


void JSFunction::JSFunctionIterateBody(int object_size, ObjectVisitor* v) {
  // Iterate over all fields in the body but take care in dealing with
  // the code entry.
  IteratePointers(v, kPropertiesOffset, kCodeEntryOffset);
  v->VisitCodeEntry(this->address() + kCodeEntryOffset);
  IteratePointers(v, kCodeEntryOffset + kPointerSize, object_size);
}


void JSFunction::MarkForLazyRecompilation() {
  ASSERT(is_compiled() && !IsOptimized());
  ASSERT(shared()->allows_lazy_compilation() ||
         code()->optimizable());
  Builtins* builtins = GetIsolate()->builtins();
  ReplaceCode(builtins->builtin(Builtins::kLazyRecompile));
}


bool SharedFunctionInfo::EnsureCompiled(Handle<SharedFunctionInfo> shared,
                                        ClearExceptionFlag flag) {
  return shared->is_compiled() || CompileLazy(shared, flag);
}


static bool CompileLazyHelper(CompilationInfo* info,
                              ClearExceptionFlag flag) {
  // Compile the source information to a code object.
  ASSERT(info->IsOptimizing() || !info->shared_info()->is_compiled());
  ASSERT(!info->isolate()->has_pending_exception());
  bool result = Compiler::CompileLazy(info);
  ASSERT(result != Isolate::Current()->has_pending_exception());
  if (!result && flag == CLEAR_EXCEPTION) {
    info->isolate()->clear_pending_exception();
  }
  return result;
}


bool SharedFunctionInfo::CompileLazy(Handle<SharedFunctionInfo> shared,
                                     ClearExceptionFlag flag) {
  CompilationInfo info(shared);
  return CompileLazyHelper(&info, flag);
}


bool JSFunction::CompileLazy(Handle<JSFunction> function,
                             ClearExceptionFlag flag) {
  bool result = true;
  if (function->shared()->is_compiled()) {
    function->ReplaceCode(function->shared()->code());
    function->shared()->set_code_age(0);
  } else {
    CompilationInfo info(function);
    result = CompileLazyHelper(&info, flag);
    ASSERT(!result || function->is_compiled());
  }
  return result;
}


bool JSFunction::CompileOptimized(Handle<JSFunction> function,
                                  int osr_ast_id,
                                  ClearExceptionFlag flag) {
  CompilationInfo info(function);
  info.SetOptimizing(osr_ast_id);
  return CompileLazyHelper(&info, flag);
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


MaybeObject* JSFunction::SetInstancePrototype(Object* value) {
  ASSERT(value->IsJSReceiver());
  Heap* heap = GetHeap();
  if (has_initial_map()) {
    // If the function has allocated the initial map
    // replace it with a copy containing the new prototype.
    Map* new_map;
    MaybeObject* maybe_new_map = initial_map()->CopyDropTransitions();
    if (!maybe_new_map->To(&new_map)) return maybe_new_map;
    new_map->set_prototype(value);
    MaybeObject* maybe_object =
        set_initial_map_and_cache_transitions(new_map);
    if (maybe_object->IsFailure()) return maybe_object;
  } else {
    // Put the value in the initial map field until an initial map is
    // needed.  At that point, a new initial map is created and the
    // prototype is put into the initial map where it belongs.
    set_prototype_or_initial_map(value);
  }
  heap->ClearInstanceofCache();
  return value;
}


MaybeObject* JSFunction::SetPrototype(Object* value) {
  ASSERT(should_have_prototype());
  Object* construct_prototype = value;

  // If the value is not a JSReceiver, store the value in the map's
  // constructor field so it can be accessed.  Also, set the prototype
  // used for constructing objects to the original object prototype.
  // See ECMA-262 13.2.2.
  if (!value->IsJSReceiver()) {
    // Copy the map so this does not affect unrelated functions.
    // Remove map transitions because they point to maps with a
    // different prototype.
    Map* new_map;
    { MaybeObject* maybe_new_map = map()->CopyDropTransitions();
      if (!maybe_new_map->To(&new_map)) return maybe_new_map;
    }
    Heap* heap = new_map->GetHeap();
    set_map(new_map);
    new_map->set_constructor(value);
    new_map->set_non_instance_prototype(true);
    construct_prototype =
        heap->isolate()->context()->global_context()->
            initial_object_prototype();
  } else {
    map()->set_non_instance_prototype(false);
  }

  return SetInstancePrototype(construct_prototype);
}


Object* JSFunction::RemovePrototype() {
  Context* global_context = context()->global_context();
  Map* no_prototype_map = shared()->is_classic_mode()
      ? global_context->function_without_prototype_map()
      : global_context->strict_mode_function_without_prototype_map();

  if (map() == no_prototype_map) {
    // Be idempotent.
    return this;
  }

  ASSERT(map() == (shared()->is_classic_mode()
                   ? global_context->function_map()
                   : global_context->strict_mode_function_map()));

  set_map(no_prototype_map);
  set_prototype_or_initial_map(no_prototype_map->GetHeap()->the_hole_value());
  return this;
}


Object* JSFunction::SetInstanceClassName(String* name) {
  shared()->set_instance_class_name(name);
  return this;
}


void JSFunction::PrintName(FILE* out) {
  SmartArrayPointer<char> name = shared()->DebugName()->ToCString();
  PrintF(out, "%s", *name);
}


Context* JSFunction::GlobalContextFromLiterals(FixedArray* literals) {
  return Context::cast(literals->get(JSFunction::kLiteralGlobalContextIndex));
}


MaybeObject* Oddball::Initialize(const char* to_string,
                                 Object* to_number,
                                 byte kind) {
  String* symbol;
  { MaybeObject* maybe_symbol =
        Isolate::Current()->heap()->LookupAsciiSymbol(to_string);
    if (!maybe_symbol->To(&symbol)) return maybe_symbol;
  }
  set_to_string(symbol);
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


bool SharedFunctionInfo::CanGenerateInlineConstructor(Object* prototype) {
  // Check the basic conditions for generating inline constructor code.
  if (!FLAG_inline_new
      || !has_only_simple_this_property_assignments()
      || this_property_assignments_count() == 0) {
    return false;
  }

  // If the prototype is null inline constructors cause no problems.
  if (!prototype->IsJSObject()) {
    ASSERT(prototype->IsNull());
    return true;
  }

  Heap* heap = GetHeap();

  // Traverse the proposed prototype chain looking for setters for properties of
  // the same names as are set by the inline constructor.
  for (Object* obj = prototype;
       obj != heap->null_value();
       obj = obj->GetPrototype()) {
    JSObject* js_object = JSObject::cast(obj);
    for (int i = 0; i < this_property_assignments_count(); i++) {
      LookupResult result(heap->isolate());
      String* name = GetThisPropertyAssignmentName(i);
      js_object->LocalLookupRealNamedProperty(name, &result);
      if (result.IsFound() && result.type() == CALLBACKS) {
        return false;
      }
    }
  }

  return true;
}


void SharedFunctionInfo::ForbidInlineConstructor() {
  set_compiler_hints(BooleanBit::set(compiler_hints(),
                                     kHasOnlySimpleThisPropertyAssignments,
                                     false));
}


void SharedFunctionInfo::SetThisPropertyAssignmentsInfo(
    bool only_simple_this_property_assignments,
    FixedArray* assignments) {
  set_compiler_hints(BooleanBit::set(compiler_hints(),
                                     kHasOnlySimpleThisPropertyAssignments,
                                     only_simple_this_property_assignments));
  set_this_property_assignments(assignments);
  set_this_property_assignments_count(assignments->length() / 3);
}


void SharedFunctionInfo::ClearThisPropertyAssignmentsInfo() {
  Heap* heap = GetHeap();
  set_compiler_hints(BooleanBit::set(compiler_hints(),
                                     kHasOnlySimpleThisPropertyAssignments,
                                     false));
  set_this_property_assignments(heap->undefined_value());
  set_this_property_assignments_count(0);
}


String* SharedFunctionInfo::GetThisPropertyAssignmentName(int index) {
  Object* obj = this_property_assignments();
  ASSERT(obj->IsFixedArray());
  ASSERT(index < this_property_assignments_count());
  obj = FixedArray::cast(obj)->get(index * 3);
  ASSERT(obj->IsString());
  return String::cast(obj);
}


bool SharedFunctionInfo::IsThisPropertyAssignmentArgument(int index) {
  Object* obj = this_property_assignments();
  ASSERT(obj->IsFixedArray());
  ASSERT(index < this_property_assignments_count());
  obj = FixedArray::cast(obj)->get(index * 3 + 1);
  return Smi::cast(obj)->value() != -1;
}


int SharedFunctionInfo::GetThisPropertyAssignmentArgument(int index) {
  ASSERT(IsThisPropertyAssignmentArgument(index));
  Object* obj =
      FixedArray::cast(this_property_assignments())->get(index * 3 + 1);
  return Smi::cast(obj)->value();
}


Object* SharedFunctionInfo::GetThisPropertyAssignmentConstant(int index) {
  ASSERT(!IsThisPropertyAssignmentArgument(index));
  Object* obj =
      FixedArray::cast(this_property_assignments())->get(index * 3 + 2);
  return obj;
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
  AssertNoAllocation no_allocation;
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
    set_code(recompiled);
  }
  ASSERT(has_deoptimization_support());
}


void SharedFunctionInfo::DisableOptimization() {
  // Disable optimization for the shared function info and mark the
  // code as non-optimizable. The marker on the shared function info
  // is there because we flush non-optimized code thereby loosing the
  // non-optimizable information for the code. When the code is
  // regenerated and set on the shared function info it is marked as
  // non-optimizable if optimization is disabled for the shared
  // function info.
  set_optimization_disabled(true);
  // Code should be the lazy compilation stub or else unoptimized.  If the
  // latter, disable optimization for the code too.
  ASSERT(code()->kind() == Code::FUNCTION || code()->kind() == Code::BUILTIN);
  if (code()->kind() == Code::FUNCTION) {
    code()->set_optimizable(false);
  }
  if (FLAG_trace_opt) {
    PrintF("[disabled optimization for %s]\n", *DebugName()->ToCString());
  }
}


bool SharedFunctionInfo::VerifyBailoutId(int id) {
  ASSERT(id != AstNode::kNoNumber);
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
  set_initial_map(heap->raw_unchecked_undefined_value());
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
        opt_count() >= Compiler::kDefaultMaxOptCount) {
      // Re-enable optimizations if they were disabled due to opt_count limit.
      set_optimization_disabled(false);
      code()->set_optimizable(true);
    }
    set_opt_count(0);
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


void SharedFunctionInfo::SharedFunctionInfoIterateBody(ObjectVisitor* v) {
  v->VisitSharedFunctionInfo(this);
  SharedFunctionInfo::BodyDescriptor::IterateBody(this, v);
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


void ObjectVisitor::VisitCodeEntry(Address entry_address) {
  Object* code = Code::GetObjectFromEntryAddress(entry_address);
  Object* old_code = code;
  VisitPointer(&code);
  if (code != old_code) {
    Memory::Address_at(entry_address) = reinterpret_cast<Code*>(code)->entry();
  }
}


void ObjectVisitor::VisitGlobalPropertyCell(RelocInfo* rinfo) {
  ASSERT(rinfo->rmode() == RelocInfo::GLOBAL_PROPERTY_CELL);
  Object* cell = rinfo->target_cell();
  Object* old_cell = cell;
  VisitPointer(&cell);
  if (cell != old_cell) {
    rinfo->set_target_cell(reinterpret_cast<JSGlobalPropertyCell*>(cell));
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
  memmove(instruction_start(), desc.buffer, desc.instr_size);

  // copy reloc info
  memmove(relocation_start(),
          desc.buffer + desc.buffer_size - desc.reloc_size,
          desc.reloc_size);

  // unbox handles and relocate
  intptr_t delta = instruction_start() - desc.buffer;
  int mode_mask = RelocInfo::kCodeTargetMask |
                  RelocInfo::ModeMask(RelocInfo::EMBEDDED_OBJECT) |
                  RelocInfo::ModeMask(RelocInfo::GLOBAL_PROPERTY_CELL) |
                  RelocInfo::kApplyMask;
  Assembler* origin = desc.origin;  // Needed to find target_object on X64.
  for (RelocIterator it(this, mode_mask); !it.done(); it.next()) {
    RelocInfo::Mode mode = it.rinfo()->rmode();
    if (mode == RelocInfo::EMBEDDED_OBJECT) {
      Handle<Object> p = it.rinfo()->target_object_handle(origin);
      it.rinfo()->set_target_object(*p, SKIP_WRITE_BARRIER);
    } else if (mode == RelocInfo::GLOBAL_PROPERTY_CELL) {
      Handle<JSGlobalPropertyCell> cell  = it.rinfo()->target_cell_handle();
      it.rinfo()->set_target_cell(*cell, SKIP_WRITE_BARRIER);
    } else if (RelocInfo::IsCodeTarget(mode)) {
      // rewrite code handles in inline cache targets to direct
      // pointers to the first instruction in the code object
      Handle<Object> p = it.rinfo()->target_object_handle(origin);
      Code* code = Code::cast(*p);
      it.rinfo()->set_target_address(code->instruction_start(),
                                     SKIP_WRITE_BARRIER);
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


void Code::SetNoStackCheckTable() {
  // Indicate the absence of a stack-check table by a table start after the
  // end of the instructions.  Table start must be aligned, so round up.
  set_stack_check_table_offset(RoundUp(instruction_size(), kIntSize));
}


Map* Code::FindFirstMap() {
  ASSERT(is_inline_cache_stub());
  AssertNoAllocation no_allocation;
  int mask = RelocInfo::ModeMask(RelocInfo::EMBEDDED_OBJECT);
  for (RelocIterator it(this, mask); !it.done(); it.next()) {
    RelocInfo* info = it.rinfo();
    Object* object = info->target_object();
    if (object->IsMap()) return Map::cast(object);
  }
  return NULL;
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
      IC::Clear(info->pc());
    }
  }
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
           AstId(i)->value(),
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
          JSFunction* function =
              JSFunction::cast(LiteralArray()->get(function_id));
          unsigned height = iterator.Next();
          PrintF(out, "{ast_id=%d, function=", ast_id);
          function->PrintName(out);
          PrintF(out, ", height=%u}", height);
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

        case Translation::DUPLICATE:
          break;

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

        case Translation::ARGUMENTS_OBJECT:
          break;
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
           this->AstId(i)->value(),
           FullCodeGenerator::PcField::decode(pc_and_state),
           FullCodeGenerator::State2String(
               FullCodeGenerator::StateField::decode(pc_and_state)));
  }
}


// Identify kind of code.
const char* Code::Kind2String(Kind kind) {
  switch (kind) {
    case FUNCTION: return "FUNCTION";
    case OPTIMIZED_FUNCTION: return "OPTIMIZED_FUNCTION";
    case STUB: return "STUB";
    case BUILTIN: return "BUILTIN";
    case LOAD_IC: return "LOAD_IC";
    case KEYED_LOAD_IC: return "KEYED_LOAD_IC";
    case STORE_IC: return "STORE_IC";
    case KEYED_STORE_IC: return "KEYED_STORE_IC";
    case CALL_IC: return "CALL_IC";
    case KEYED_CALL_IC: return "KEYED_CALL_IC";
    case UNARY_OP_IC: return "UNARY_OP_IC";
    case BINARY_OP_IC: return "BINARY_OP_IC";
    case COMPARE_IC: return "COMPARE_IC";
    case TO_BOOLEAN_IC: return "TO_BOOLEAN_IC";
  }
  UNREACHABLE();
  return NULL;
}


const char* Code::ICState2String(InlineCacheState state) {
  switch (state) {
    case UNINITIALIZED: return "UNINITIALIZED";
    case PREMONOMORPHIC: return "PREMONOMORPHIC";
    case MONOMORPHIC: return "MONOMORPHIC";
    case MONOMORPHIC_PROTOTYPE_FAILURE: return "MONOMORPHIC_PROTOTYPE_FAILURE";
    case MEGAMORPHIC: return "MEGAMORPHIC";
    case DEBUG_BREAK: return "DEBUG_BREAK";
    case DEBUG_PREPARE_STEP_IN: return "DEBUG_PREPARE_STEP_IN";
  }
  UNREACHABLE();
  return NULL;
}


const char* Code::PropertyType2String(PropertyType type) {
  switch (type) {
    case NORMAL: return "NORMAL";
    case FIELD: return "FIELD";
    case CONSTANT_FUNCTION: return "CONSTANT_FUNCTION";
    case CALLBACKS: return "CALLBACKS";
    case HANDLER: return "HANDLER";
    case INTERCEPTOR: return "INTERCEPTOR";
    case MAP_TRANSITION: return "MAP_TRANSITION";
    case ELEMENTS_TRANSITION: return "ELEMENTS_TRANSITION";
    case CONSTANT_TRANSITION: return "CONSTANT_TRANSITION";
    case NULL_DESCRIPTOR: return "NULL_DESCRIPTOR";
  }
  UNREACHABLE();  // keep the compiler happy
  return NULL;
}


void Code::PrintExtraICState(FILE* out, Kind kind, ExtraICState extra) {
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
    PrintF(out, "extra_ic_state = %s\n", name);
  } else {
    PrintF(out, "extra_ic_state = %d\n", extra);
  }
}


void Code::Disassemble(const char* name, FILE* out) {
  PrintF(out, "kind = %s\n", Kind2String(kind()));
  if (is_inline_cache_stub()) {
    PrintF(out, "ic_state = %s\n", ICState2String(ic_state()));
    PrintExtraICState(out, kind(), extra_ic_state());
    if (ic_state() == MONOMORPHIC) {
      PrintF(out, "type = %s\n", PropertyType2String(type()));
    }
    if (is_call_stub() || is_keyed_call_stub()) {
      PrintF(out, "argc = %d\n", arguments_count());
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

  if (kind() == OPTIMIZED_FUNCTION) {
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
    unsigned offset = stack_check_table_offset();
    // If there is no stack check table, the "table start" will at or after
    // (due to alignment) the end of the instruction stream.
    if (static_cast<int>(offset) < instruction_size()) {
      unsigned* address =
          reinterpret_cast<unsigned*>(instruction_start() + offset);
      unsigned length = address[0];
      PrintF(out, "Stack checks (size = %u)\n", length);
      PrintF(out, "ast_id  pc_offset\n");
      for (unsigned i = 0; i < length; ++i) {
        unsigned index = (2 * i) + 1;
        PrintF(out, "%6u  %9u\n", address[index], address[index + 1]);
      }
      PrintF(out, "\n");
    }
  }

  PrintF("RelocInfo (size = %d)\n", relocation_size());
  for (RelocIterator it(this); !it.done(); it.next()) it.rinfo()->Print(out);
  PrintF(out, "\n");
}
#endif  // ENABLE_DISASSEMBLER


MaybeObject* JSObject::SetFastElementsCapacityAndLength(
    int capacity,
    int length,
    SetFastElementsCapacityMode set_capacity_mode) {
  Heap* heap = GetHeap();
  // We should never end in here with a pixel or external array.
  ASSERT(!HasExternalArrayElements());

  // Allocate a new fast elements backing store.
  FixedArray* new_elements;
  { MaybeObject* maybe = heap->AllocateFixedArrayWithHoles(capacity);
    if (!maybe->To(&new_elements)) return maybe;
  }

  // Find the new map to use for this object if there is a map change.
  Map* new_map = NULL;
  if (elements()->map() != heap->non_strict_arguments_elements_map()) {
    // The resized array has FAST_SMI_ONLY_ELEMENTS if the capacity mode forces
    // it, or if it's allowed and the old elements array contained only SMIs.
    bool has_fast_smi_only_elements =
        (set_capacity_mode == kForceSmiOnlyElements) ||
        ((set_capacity_mode == kAllowSmiOnlyElements) &&
         (elements()->map()->has_fast_smi_only_elements() ||
          elements() == heap->empty_fixed_array()));
    ElementsKind elements_kind = has_fast_smi_only_elements
        ? FAST_SMI_ONLY_ELEMENTS
        : FAST_ELEMENTS;
    MaybeObject* maybe = GetElementsTransitionMap(GetIsolate(), elements_kind);
    if (!maybe->To(&new_map)) return maybe;
  }

  FixedArrayBase* old_elements = elements();
  ElementsKind elements_kind = GetElementsKind();
  ElementsAccessor* accessor = ElementsAccessor::ForKind(elements_kind);
  ElementsKind to_kind = (elements_kind == FAST_SMI_ONLY_ELEMENTS)
      ? FAST_SMI_ONLY_ELEMENTS
      : FAST_ELEMENTS;
  //  int copy_size = Min(old_elements_raw->length(), new_elements->length());
  accessor->CopyElements(this, new_elements, to_kind);
  if (elements_kind != NON_STRICT_ARGUMENTS_ELEMENTS) {
    set_map_and_elements(new_map, new_elements);
  } else {
    FixedArray* parameter_map = FixedArray::cast(old_elements);
    parameter_map->set(1, new_elements);
  }

  if (FLAG_trace_elements_transitions) {
    PrintElementsTransition(stdout, elements_kind, old_elements,
                            GetElementsKind(), new_elements);
  }

  // Update the length if necessary.
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

  FixedArrayBase* elems;
  { MaybeObject* maybe_obj =
        heap->AllocateUninitializedFixedDoubleArray(capacity);
    if (!maybe_obj->To(&elems)) return maybe_obj;
  }

  Map* new_map;
  { MaybeObject* maybe_obj =
        GetElementsTransitionMap(heap->isolate(), FAST_DOUBLE_ELEMENTS);
    if (!maybe_obj->To(&new_map)) return maybe_obj;
  }

  FixedArrayBase* old_elements = elements();
  ElementsKind elements_kind = GetElementsKind();
  ElementsAccessor* accessor = ElementsAccessor::ForKind(elements_kind);
  accessor->CopyElements(this, elems, FAST_DOUBLE_ELEMENTS);
  if (elements_kind != NON_STRICT_ARGUMENTS_ELEMENTS) {
    set_map_and_elements(new_map, elems);
  } else {
    FixedArray* parameter_map = FixedArray::cast(old_elements);
    parameter_map->set(1, elems);
  }

  if (FLAG_trace_elements_transitions) {
    PrintElementsTransition(stdout, elements_kind, old_elements,
                            FAST_DOUBLE_ELEMENTS, elems);
  }

  if (IsJSArray()) {
    JSArray::cast(this)->set_length(Smi::FromInt(length));
  }

  return this;
}


MaybeObject* JSArray::Initialize(int capacity) {
  Heap* heap = GetHeap();
  ASSERT(capacity >= 0);
  set_length(Smi::FromInt(0));
  FixedArray* new_elements;
  if (capacity == 0) {
    new_elements = heap->empty_fixed_array();
  } else {
    MaybeObject* maybe_obj = heap->AllocateFixedArrayWithHoles(capacity);
    if (!maybe_obj->To(&new_elements)) return maybe_obj;
  }
  set_elements(new_elements);
  return this;
}


void JSArray::Expand(int required_size) {
  GetIsolate()->factory()->SetElementsCapacityAndLength(
      Handle<JSArray>(this), required_size, required_size);
}


MaybeObject* JSArray::SetElementsLength(Object* len) {
  // We should never end in here with a pixel or external array.
  ASSERT(AllowsSetElementsLength());
  return GetElementsAccessor()->SetLength(this, len);
}


Object* Map::GetPrototypeTransition(Object* prototype) {
  FixedArray* cache = prototype_transitions();
  int number_of_transitions = NumberOfProtoTransitions();
  const int proto_offset =
      kProtoTransitionHeaderSize + kProtoTransitionPrototypeOffset;
  const int map_offset = kProtoTransitionHeaderSize + kProtoTransitionMapOffset;
  const int step = kProtoTransitionElementsPerEntry;
  for (int i = 0; i < number_of_transitions; i++) {
    if (cache->get(proto_offset + i * step) == prototype) {
      Object* map = cache->get(map_offset + i * step);
      ASSERT(map->IsMap());
      return map;
    }
  }
  return NULL;
}


MaybeObject* Map::PutPrototypeTransition(Object* prototype, Map* map) {
  ASSERT(map->IsMap());
  ASSERT(HeapObject::cast(prototype)->map()->IsMap());
  // Don't cache prototype transition if this map is shared.
  if (is_shared() || !FLAG_cache_prototype_transitions) return this;

  FixedArray* cache = prototype_transitions();

  const int step = kProtoTransitionElementsPerEntry;
  const int header = kProtoTransitionHeaderSize;

  int capacity = (cache->length() - header) / step;

  int transitions = NumberOfProtoTransitions() + 1;

  if (transitions > capacity) {
    if (capacity > kMaxCachedPrototypeTransitions) return this;

    FixedArray* new_cache;
    // Grow array by factor 2 over and above what we need.
    { MaybeObject* maybe_cache =
          GetHeap()->AllocateFixedArray(transitions * 2 * step + header);
      if (!maybe_cache->To(&new_cache)) return maybe_cache;
    }

    for (int i = 0; i < capacity * step; i++) {
      new_cache->set(i + header, cache->get(i + header));
    }
    cache = new_cache;
    set_prototype_transitions(cache);
  }

  int last = transitions - 1;

  cache->set(header + last * step + kProtoTransitionPrototypeOffset, prototype);
  cache->set(header + last * step + kProtoTransitionMapOffset, map);
  SetNumberOfProtoTransitions(transitions);

  return cache;
}


MaybeObject* JSReceiver::SetPrototype(Object* value,
                                      bool skip_hidden_prototypes) {
#ifdef DEBUG
  int size = Size();
#endif

  Heap* heap = GetHeap();
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
  if (!this->map()->is_extensible()) {
    HandleScope scope(heap->isolate());
    Handle<Object> handle(this, heap->isolate());
    return heap->isolate()->Throw(
        *FACTORY->NewTypeError("non_extensible_proto",
                               HandleVector<Object>(&handle, 1)));
  }

  // Before we can set the prototype we need to be sure
  // prototype cycles are prevented.
  // It is sufficient to validate that the receiver is not in the new prototype
  // chain.
  for (Object* pt = value; pt != heap->null_value(); pt = pt->GetPrototype()) {
    if (JSReceiver::cast(pt) == this) {
      // Cycle detected.
      HandleScope scope(heap->isolate());
      return heap->isolate()->Throw(
          *FACTORY->NewError("cyclic_proto", HandleVector<Object>(NULL, 0)));
    }
  }

  JSReceiver* real_receiver = this;

  if (skip_hidden_prototypes) {
    // Find the first object in the chain whose prototype object is not
    // hidden and set the new prototype on that object.
    Object* current_proto = real_receiver->GetPrototype();
    while (current_proto->IsJSObject() &&
          JSReceiver::cast(current_proto)->map()->is_hidden_prototype()) {
      real_receiver = JSReceiver::cast(current_proto);
      current_proto = current_proto->GetPrototype();
    }
  }

  // Set the new prototype of the object.
  Map* map = real_receiver->map();

  // Nothing to do if prototype is already set.
  if (map->prototype() == value) return value;

  Object* new_map = map->GetPrototypeTransition(value);
  if (new_map == NULL) {
    { MaybeObject* maybe_new_map = map->CopyDropTransitions();
      if (!maybe_new_map->ToObject(&new_map)) return maybe_new_map;
    }

    { MaybeObject* maybe_new_cache =
          map->PutPrototypeTransition(value, Map::cast(new_map));
      if (maybe_new_cache->IsFailure()) return maybe_new_cache;
    }

    Map::cast(new_map)->set_prototype(value);
  }
  ASSERT(Map::cast(new_map)->prototype() == value);
  real_receiver->set_map(Map::cast(new_map));

  heap->ClearInstanceofCache();
  ASSERT(size == Size());
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


bool JSObject::HasElementWithInterceptor(JSReceiver* receiver, uint32_t index) {
  Isolate* isolate = GetIsolate();
  // Make sure that the top context does not change when doing
  // callbacks or interceptor calls.
  AssertNoContextChange ncc;
  HandleScope scope(isolate);
  Handle<InterceptorInfo> interceptor(GetIndexedInterceptor());
  Handle<JSReceiver> receiver_handle(receiver);
  Handle<JSObject> holder_handle(this);
  CustomArguments args(isolate, interceptor->data(), receiver, this);
  v8::AccessorInfo info(args.end());
  if (!interceptor->query()->IsUndefined()) {
    v8::IndexedPropertyQuery query =
        v8::ToCData<v8::IndexedPropertyQuery>(interceptor->query());
    LOG(isolate,
        ApiIndexedPropertyAccess("interceptor-indexed-has", this, index));
    v8::Handle<v8::Integer> result;
    {
      // Leaving JavaScript.
      VMState state(isolate, EXTERNAL);
      result = query(index, info);
    }
    if (!result.IsEmpty()) {
      ASSERT(result->IsInt32());
      return true;  // absence of property is signaled by empty handle.
    }
  } else if (!interceptor->getter()->IsUndefined()) {
    v8::IndexedPropertyGetter getter =
        v8::ToCData<v8::IndexedPropertyGetter>(interceptor->getter());
    LOG(isolate,
        ApiIndexedPropertyAccess("interceptor-indexed-has-get", this, index));
    v8::Handle<v8::Value> result;
    {
      // Leaving JavaScript.
      VMState state(isolate, EXTERNAL);
      result = getter(index, info);
    }
    if (!result.IsEmpty()) return true;
  }

  if (holder_handle->GetElementsAccessor()->HasElement(
          *receiver_handle, *holder_handle, index)) {
    return true;
  }

  if (holder_handle->IsStringObjectWithCharacterAt(index)) return true;
  Object* pt = holder_handle->GetPrototype();
  if (pt->IsJSProxy()) {
    // We need to follow the spec and simulate a call to [[GetOwnProperty]].
    return JSProxy::cast(pt)->GetElementAttributeWithHandler(
        receiver, index) != ABSENT;
  }
  if (pt->IsNull()) return false;
  return JSObject::cast(pt)->HasElementWithReceiver(*receiver_handle, index);
}


JSObject::LocalElementType JSObject::HasLocalElement(uint32_t index) {
  // Check access rights if needed.
  if (IsAccessCheckNeeded()) {
    Heap* heap = GetHeap();
    if (!heap->isolate()->MayIndexedAccess(this, index, v8::ACCESS_HAS)) {
      heap->isolate()->ReportFailedAccessCheck(this, v8::ACCESS_HAS);
      return UNDEFINED_ELEMENT;
    }
  }

  if (IsJSGlobalProxy()) {
    Object* proto = GetPrototype();
    if (proto->IsNull()) return UNDEFINED_ELEMENT;
    ASSERT(proto->IsJSGlobalObject());
    return JSObject::cast(proto)->HasLocalElement(index);
  }

  // Check for lookup interceptor
  if (HasIndexedInterceptor()) {
    return HasElementWithInterceptor(this, index) ? INTERCEPTED_ELEMENT
                                                  : UNDEFINED_ELEMENT;
  }

  // Handle [] on String objects.
  if (this->IsStringObjectWithCharacterAt(index)) {
    return STRING_CHARACTER_ELEMENT;
  }

  switch (GetElementsKind()) {
    case FAST_SMI_ONLY_ELEMENTS:
    case FAST_ELEMENTS: {
      uint32_t length = IsJSArray() ?
          static_cast<uint32_t>
              (Smi::cast(JSArray::cast(this)->length())->value()) :
          static_cast<uint32_t>(FixedArray::cast(elements())->length());
      if ((index < length) &&
          !FixedArray::cast(elements())->get(index)->IsTheHole()) {
        return FAST_ELEMENT;
      }
      break;
    }
    case FAST_DOUBLE_ELEMENTS: {
      uint32_t length = IsJSArray() ?
          static_cast<uint32_t>
              (Smi::cast(JSArray::cast(this)->length())->value()) :
          static_cast<uint32_t>(FixedDoubleArray::cast(elements())->length());
      if ((index < length) &&
          !FixedDoubleArray::cast(elements())->is_the_hole(index)) {
        return FAST_ELEMENT;
      }
      break;
    }
    case EXTERNAL_PIXEL_ELEMENTS: {
      ExternalPixelArray* pixels = ExternalPixelArray::cast(elements());
      if (index < static_cast<uint32_t>(pixels->length())) return FAST_ELEMENT;
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
      ExternalArray* array = ExternalArray::cast(elements());
      if (index < static_cast<uint32_t>(array->length())) return FAST_ELEMENT;
      break;
    }
    case DICTIONARY_ELEMENTS: {
      if (element_dictionary()->FindEntry(index) !=
          SeededNumberDictionary::kNotFound) {
        return DICTIONARY_ELEMENT;
      }
      break;
    }
    case NON_STRICT_ARGUMENTS_ELEMENTS: {
      // Aliased parameters and non-aliased elements in a fast backing store
      // behave as FAST_ELEMENT.  Non-aliased elements in a dictionary
      // backing store behave as DICTIONARY_ELEMENT.
      FixedArray* parameter_map = FixedArray::cast(elements());
      uint32_t length = parameter_map->length();
      Object* probe =
          index < (length - 2) ? parameter_map->get(index + 2) : NULL;
      if (probe != NULL && !probe->IsTheHole()) return FAST_ELEMENT;
      // If not aliased, check the arguments.
      FixedArray* arguments = FixedArray::cast(parameter_map->get(1));
      if (arguments->IsDictionary()) {
        SeededNumberDictionary* dictionary =
            SeededNumberDictionary::cast(arguments);
        if (dictionary->FindEntry(index) != SeededNumberDictionary::kNotFound) {
          return DICTIONARY_ELEMENT;
        }
      } else {
        length = arguments->length();
        probe = (index < length) ? arguments->get(index) : NULL;
        if (probe != NULL && !probe->IsTheHole()) return FAST_ELEMENT;
      }
      break;
    }
  }

  return UNDEFINED_ELEMENT;
}


bool JSObject::HasElementWithReceiver(JSReceiver* receiver, uint32_t index) {
  // Check access rights if needed.
  if (IsAccessCheckNeeded()) {
    Heap* heap = GetHeap();
    if (!heap->isolate()->MayIndexedAccess(this, index, v8::ACCESS_HAS)) {
      heap->isolate()->ReportFailedAccessCheck(this, v8::ACCESS_HAS);
      return false;
    }
  }

  // Check for lookup interceptor
  if (HasIndexedInterceptor()) {
    return HasElementWithInterceptor(receiver, index);
  }

  ElementsAccessor* accessor = GetElementsAccessor();
  if (accessor->HasElement(receiver, this, index)) {
    return true;
  }

  // Handle [] on String objects.
  if (this->IsStringObjectWithCharacterAt(index)) return true;

  Object* pt = GetPrototype();
  if (pt->IsNull()) return false;
  if (pt->IsJSProxy()) {
    // We need to follow the spec and simulate a call to [[GetOwnProperty]].
    return JSProxy::cast(pt)->GetElementAttributeWithHandler(
        receiver, index) != ABSENT;
  }
  return JSObject::cast(pt)->HasElementWithReceiver(receiver, index);
}


MaybeObject* JSObject::SetElementWithInterceptor(uint32_t index,
                                                 Object* value,
                                                 PropertyAttributes attributes,
                                                 StrictModeFlag strict_mode,
                                                 bool check_prototype,
                                                 SetPropertyMode set_mode) {
  Isolate* isolate = GetIsolate();
  // Make sure that the top context does not change when doing
  // callbacks or interceptor calls.
  AssertNoContextChange ncc;
  HandleScope scope(isolate);
  Handle<InterceptorInfo> interceptor(GetIndexedInterceptor());
  Handle<JSObject> this_handle(this);
  Handle<Object> value_handle(value, isolate);
  if (!interceptor->setter()->IsUndefined()) {
    v8::IndexedPropertySetter setter =
        v8::ToCData<v8::IndexedPropertySetter>(interceptor->setter());
    LOG(isolate,
        ApiIndexedPropertyAccess("interceptor-indexed-set", this, index));
    CustomArguments args(isolate, interceptor->data(), this, this);
    v8::AccessorInfo info(args.end());
    v8::Handle<v8::Value> result;
    {
      // Leaving JavaScript.
      VMState state(isolate, EXTERNAL);
      result = setter(index, v8::Utils::ToLocal(value_handle), info);
    }
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
  if (structure->IsAccessorInfo()) {
    Handle<AccessorInfo> data(AccessorInfo::cast(structure));
    Object* fun_obj = data->getter();
    v8::AccessorGetter call_fun = v8::ToCData<v8::AccessorGetter>(fun_obj);
    HandleScope scope(isolate);
    Handle<JSObject> self(JSObject::cast(receiver));
    Handle<JSObject> holder_handle(JSObject::cast(holder));
    Handle<Object> number = isolate->factory()->NewNumberFromUint(index);
    Handle<String> key = isolate->factory()->NumberToString(number);
    LOG(isolate, ApiNamedPropertyAccess("load", *self, *key));
    CustomArguments args(isolate, data->data(), *self, *holder_handle);
    v8::AccessorInfo info(args.end());
    v8::Handle<v8::Value> result;
    {
      // Leaving JavaScript.
      VMState state(isolate, EXTERNAL);
      result = call_fun(v8::Utils::ToLocal(key), info);
    }
    RETURN_IF_SCHEDULED_EXCEPTION(isolate);
    if (result.IsEmpty()) return isolate->heap()->undefined_value();
    return *v8::Utils::OpenHandle(*result);
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

  if (structure->IsAccessorInfo()) {
    // api style callbacks
    Handle<JSObject> self(this);
    Handle<JSObject> holder_handle(JSObject::cast(holder));
    Handle<AccessorInfo> data(AccessorInfo::cast(structure));
    Object* call_obj = data->setter();
    v8::AccessorSetter call_fun = v8::ToCData<v8::AccessorSetter>(call_obj);
    if (call_fun == NULL) return value;
    Handle<Object> number = isolate->factory()->NewNumberFromUint(index);
    Handle<String> key(isolate->factory()->NumberToString(number));
    LOG(isolate, ApiNamedPropertyAccess("store", *self, *key));
    CustomArguments args(isolate, data->data(), *self, *holder_handle);
    v8::AccessorInfo info(args.end());
    {
      // Leaving JavaScript.
      VMState state(isolate, EXTERNAL);
      call_fun(v8::Utils::ToLocal(key),
               v8::Utils::ToLocal(value_handle),
               info);
    }
    RETURN_IF_SCHEDULED_EXCEPTION(isolate);
    return *value_handle;
  }

  if (structure->IsAccessorPair()) {
    Handle<Object> setter(AccessorPair::cast(structure)->setter());
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
  ASSERT(HasFastTypeElements() ||
         HasFastArgumentsElements());

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
  if (IsJSArray()) {
    CHECK(JSArray::cast(this)->length()->ToArrayIndex(&array_length));
    if (index >= array_length) {
      must_update_array_length = true;
      array_length = index + 1;
    }
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
  if (HasFastSmiOnlyElements() && !value->IsSmi() && value->IsNumber()) {
    MaybeObject* maybe =
        SetFastDoubleElementsCapacityAndLength(new_capacity, array_length);
    if (maybe->IsFailure()) return maybe;
    FixedDoubleArray::cast(elements())->set(index, value->Number());
    return value;
  }
  // Change elements kind from SMI_ONLY to generic FAST if necessary.
  if (HasFastSmiOnlyElements() && !value->IsSmi()) {
    Map* new_map;
    { MaybeObject* maybe_new_map = GetElementsTransitionMap(GetIsolate(),
                                                            FAST_ELEMENTS);
      if (!maybe_new_map->To(&new_map)) return maybe_new_map;
    }
    set_map(new_map);
    if (FLAG_trace_elements_transitions) {
      PrintElementsTransition(stdout, FAST_SMI_ONLY_ELEMENTS, elements(),
                              FAST_ELEMENTS, elements());
    }
  }
  // Increase backing store capacity if that's been decided previously.
  if (new_capacity != capacity) {
    FixedArray* new_elements;
    SetFastElementsCapacityMode set_capacity_mode =
        value->IsSmi() && HasFastSmiOnlyElements()
            ? kAllowSmiOnlyElements
            : kDontAllowSmiOnlyElements;
    { MaybeObject* maybe =
          SetFastElementsCapacityAndLength(new_capacity,
                                           array_length,
                                           set_capacity_mode);
      if (!maybe->To(&new_elements)) return maybe;
    }
    new_elements->set(index, value);
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
                                            Object* value,
                                            PropertyAttributes attributes,
                                            StrictModeFlag strict_mode,
                                            bool check_prototype,
                                            SetPropertyMode set_mode) {
  ASSERT(HasDictionaryElements() || HasDictionaryArgumentsElements());
  Isolate* isolate = GetIsolate();
  Heap* heap = isolate->heap();

  // Insert element in the dictionary.
  FixedArray* elements = FixedArray::cast(this->elements());
  bool is_arguments =
      (elements->map() == heap->non_strict_arguments_elements_map());
  SeededNumberDictionary* dictionary = NULL;
  if (is_arguments) {
    dictionary = SeededNumberDictionary::cast(elements->get(1));
  } else {
    dictionary = SeededNumberDictionary::cast(elements);
  }

  int entry = dictionary->FindEntry(index);
  if (entry != SeededNumberDictionary::kNotFound) {
    Object* element = dictionary->ValueAt(entry);
    PropertyDetails details = dictionary->DetailsAt(entry);
    if (details.type() == CALLBACKS && set_mode == SET_PROPERTY) {
      return SetElementWithCallback(element, index, value, this, strict_mode);
    } else {
      dictionary->UpdateMaxNumberKey(index);
      // If a value has not been initialized we allow writing to it even if it
      // is read-only (a declared const that has not been initialized).  If a
      // value is being defined we skip attribute checks completely.
      if (set_mode == DEFINE_PROPERTY) {
        details = PropertyDetails(attributes, NORMAL, details.index());
        dictionary->DetailsAtPut(entry, details);
      } else if (details.IsReadOnly() && !element->IsTheHole()) {
        if (strict_mode == kNonStrictMode) {
          return isolate->heap()->undefined_value();
        } else {
          Handle<Object> holder(this);
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
        context->set(context_index, value);
        // For elements that are still writable we keep slow aliasing.
        if (!details.IsReadOnly()) value = element;
      }
      dictionary->ValueAtPut(entry, value);
    }
  } else {
    // Index not already used. Look for an accessor in the prototype chain.
    if (check_prototype) {
      bool found;
      MaybeObject* result =
          SetElementWithCallbackSetterInPrototypes(
              index, value, &found, strict_mode);
      if (found) return result;
    }
    // When we set the is_extensible flag to false we always force the
    // element into dictionary mode (and force them to stay there).
    if (!map()->is_extensible()) {
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
    PropertyDetails details = PropertyDetails(attributes, NORMAL);
    MaybeObject* maybe = dictionary->AddNumberEntry(index, value, details);
    if (!maybe->To(&new_dictionary)) return maybe;
    if (dictionary != SeededNumberDictionary::cast(new_dictionary)) {
      if (is_arguments) {
        elements->set(1, new_dictionary);
      } else {
        set_elements(new_dictionary);
      }
      dictionary = SeededNumberDictionary::cast(new_dictionary);
    }
  }

  // Update the array length if this JSObject is an array.
  if (IsJSArray()) {
    MaybeObject* result =
        JSArray::cast(this)->JSArrayUpdateLengthFromIndex(index, value);
    if (result->IsFailure()) return result;
  }

  // Attempt to put this object back in fast case.
  if (ShouldConvertToFastElements()) {
    uint32_t new_length = 0;
    if (IsJSArray()) {
      CHECK(JSArray::cast(this)->length()->ToArrayIndex(&new_length));
    } else {
      new_length = dictionary->max_number_key() + 1;
    }
    SetFastElementsCapacityMode set_capacity_mode = FLAG_smi_only_arrays
        ? kAllowSmiOnlyElements
        : kDontAllowSmiOnlyElements;
    bool has_smi_only_elements = false;
    bool should_convert_to_fast_double_elements =
        ShouldConvertToFastDoubleElements(&has_smi_only_elements);
    if (has_smi_only_elements) {
      set_capacity_mode = kForceSmiOnlyElements;
    }
    MaybeObject* result = should_convert_to_fast_double_elements
        ? SetFastDoubleElementsCapacityAndLength(new_length, new_length)
        : SetFastElementsCapacityAndLength(new_length,
                                           new_length,
                                           set_capacity_mode);
    if (result->IsFailure()) return result;
#ifdef DEBUG
    if (FLAG_trace_normalization) {
      PrintF("Object elements are fast case again:\n");
      Print();
    }
#endif
  }
  return value;
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
  if (!value->IsNumber()) {
    Object* obj;
    uint32_t length = elms_length;
    if (IsJSArray()) {
      CHECK(JSArray::cast(this)->length()->ToArrayIndex(&length));
    }
    MaybeObject* maybe_obj = SetFastElementsCapacityAndLength(
        elms_length,
        length,
        kDontAllowSmiOnlyElements);
    if (!maybe_obj->ToObject(&obj)) return maybe_obj;
    return SetFastElement(index,
                          value,
                          strict_mode,
                          check_prototype);
  }

  double double_value = value_is_smi
      ? static_cast<double>(Smi::cast(value)->value())
      : HeapNumber::cast(value)->value();

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
      Object* obj;
      { MaybeObject* maybe_obj =
            SetFastDoubleElementsCapacityAndLength(new_capacity,
                                                   index + 1);
        if (!maybe_obj->ToObject(&obj)) return maybe_obj;
      }
      FixedDoubleArray::cast(elements())->set(index, double_value);
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


MaybeObject* JSReceiver::SetElement(uint32_t index,
                                    Object* value,
                                    PropertyAttributes attributes,
                                    StrictModeFlag strict_mode,
                                    bool check_proto) {
  if (IsJSProxy()) {
    return JSProxy::cast(this)->SetElementWithHandler(
        index, value, strict_mode);
  } else {
    return JSObject::cast(this)->SetElement(
        index, value, attributes, strict_mode, check_proto);
  }
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
    if (!value->IsSmi() && !value->IsHeapNumber() && !value->IsUndefined()) {
      bool has_exception;
      Handle<Object> number = Execution::ToNumber(value, &has_exception);
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
                                  Object* value,
                                  PropertyAttributes attributes,
                                  StrictModeFlag strict_mode,
                                  bool check_prototype,
                                  SetPropertyMode set_mode) {
  // Check access rights if needed.
  if (IsAccessCheckNeeded()) {
    Heap* heap = GetHeap();
    if (!heap->isolate()->MayIndexedAccess(this, index, v8::ACCESS_SET)) {
      HandleScope scope(heap->isolate());
      Handle<Object> value_handle(value);
      heap->isolate()->ReportFailedAccessCheck(this, v8::ACCESS_SET);
      return *value_handle;
    }
  }

  if (IsJSGlobalProxy()) {
    Object* proto = GetPrototype();
    if (proto->IsNull()) return value;
    ASSERT(proto->IsJSGlobalObject());
    return JSObject::cast(proto)->SetElement(index,
                                             value,
                                             attributes,
                                             strict_mode,
                                             check_prototype,
                                             set_mode);
  }

  // Don't allow element properties to be redefined for external arrays.
  if (HasExternalArrayElements() && set_mode == DEFINE_PROPERTY) {
    Isolate* isolate = GetHeap()->isolate();
    Handle<Object> number = isolate->factory()->NewNumberFromUint(index);
    Handle<Object> args[] = { Handle<Object>(this), number };
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

  // Check for lookup interceptor
  if (HasIndexedInterceptor()) {
    return SetElementWithInterceptor(index,
                                     value,
                                     attributes,
                                     strict_mode,
                                     check_prototype,
                                     set_mode);
  }

  return SetElementWithoutInterceptor(index,
                                      value,
                                      attributes,
                                      strict_mode,
                                      check_prototype,
                                      set_mode);
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
  switch (GetElementsKind()) {
    case FAST_SMI_ONLY_ELEMENTS:
    case FAST_ELEMENTS:
      return SetFastElement(index, value, strict_mode, check_prototype);
    case FAST_DOUBLE_ELEMENTS:
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


MaybeObject* JSObject::TransitionElementsKind(ElementsKind to_kind) {
  ElementsKind from_kind = map()->elements_kind();

  Isolate* isolate = GetIsolate();
  if ((from_kind == FAST_SMI_ONLY_ELEMENTS ||
      elements() == isolate->heap()->empty_fixed_array()) &&
      to_kind == FAST_ELEMENTS) {
    ASSERT(from_kind != FAST_ELEMENTS);
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

  if (from_kind == FAST_SMI_ONLY_ELEMENTS &&
      to_kind == FAST_DOUBLE_ELEMENTS) {
    MaybeObject* maybe_result =
        SetFastDoubleElementsCapacityAndLength(capacity, length);
    if (maybe_result->IsFailure()) return maybe_result;
    return this;
  }

  if (from_kind == FAST_DOUBLE_ELEMENTS && to_kind == FAST_ELEMENTS) {
    MaybeObject* maybe_result = SetFastElementsCapacityAndLength(
        capacity, length, kDontAllowSmiOnlyElements);
    if (maybe_result->IsFailure()) return maybe_result;
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
  return
      (from_kind == FAST_SMI_ONLY_ELEMENTS &&
          (to_kind == FAST_DOUBLE_ELEMENTS || to_kind == FAST_ELEMENTS)) ||
      (from_kind == FAST_DOUBLE_ELEMENTS && to_kind == FAST_ELEMENTS);
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
  // Make sure that the top context does not change when doing
  // callbacks or interceptor calls.
  AssertNoContextChange ncc;
  HandleScope scope(isolate);
  Handle<InterceptorInfo> interceptor(GetIndexedInterceptor(), isolate);
  Handle<Object> this_handle(receiver, isolate);
  Handle<JSObject> holder_handle(this, isolate);
  if (!interceptor->getter()->IsUndefined()) {
    v8::IndexedPropertyGetter getter =
        v8::ToCData<v8::IndexedPropertyGetter>(interceptor->getter());
    LOG(isolate,
        ApiIndexedPropertyAccess("interceptor-indexed-get", this, index));
    CustomArguments args(isolate, interceptor->data(), receiver, this);
    v8::AccessorInfo info(args.end());
    v8::Handle<v8::Value> result;
    {
      // Leaving JavaScript.
      VMState state(isolate, EXTERNAL);
      result = getter(index, info);
    }
    RETURN_IF_SCHEDULED_EXCEPTION(isolate);
    if (!result.IsEmpty()) return *v8::Utils::OpenHandle(*result);
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
  return pt->GetElementWithReceiver(*this_handle, index);
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
    case FAST_SMI_ONLY_ELEMENTS:
    case FAST_ELEMENTS:
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
    case FAST_DOUBLE_ELEMENTS: {
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
  AssertNoAllocation no_gc;
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
    JSReceiver* receiver,
    String* name,
    PropertyAttributes* attributes) {
  // Check local property in holder, ignore interceptor.
  LookupResult result(GetIsolate());
  LocalLookupRealNamedProperty(name, &result);
  if (result.IsProperty()) {
    return GetProperty(receiver, &result, name, attributes);
  }
  // Continue searching via the prototype chain.
  Object* pt = GetPrototype();
  *attributes = ABSENT;
  if (pt->IsNull()) return GetHeap()->undefined_value();
  return pt->GetPropertyWithReceiver(receiver, name, attributes);
}


MaybeObject* JSObject::GetLocalPropertyPostInterceptor(
    JSReceiver* receiver,
    String* name,
    PropertyAttributes* attributes) {
  // Check local property in holder, ignore interceptor.
  LookupResult result(GetIsolate());
  LocalLookupRealNamedProperty(name, &result);
  if (result.IsProperty()) {
    return GetProperty(receiver, &result, name, attributes);
  }
  return GetHeap()->undefined_value();
}


MaybeObject* JSObject::GetPropertyWithInterceptor(
    JSReceiver* receiver,
    String* name,
    PropertyAttributes* attributes) {
  Isolate* isolate = GetIsolate();
  InterceptorInfo* interceptor = GetNamedInterceptor();
  HandleScope scope(isolate);
  Handle<JSReceiver> receiver_handle(receiver);
  Handle<JSObject> holder_handle(this);
  Handle<String> name_handle(name);

  if (!interceptor->getter()->IsUndefined()) {
    v8::NamedPropertyGetter getter =
        v8::ToCData<v8::NamedPropertyGetter>(interceptor->getter());
    LOG(isolate,
        ApiNamedPropertyAccess("interceptor-named-get", *holder_handle, name));
    CustomArguments args(isolate, interceptor->data(), receiver, this);
    v8::AccessorInfo info(args.end());
    v8::Handle<v8::Value> result;
    {
      // Leaving JavaScript.
      VMState state(isolate, EXTERNAL);
      result = getter(v8::Utils::ToLocal(name_handle), info);
    }
    RETURN_IF_SCHEDULED_EXCEPTION(isolate);
    if (!result.IsEmpty()) {
      *attributes = NONE;
      return *v8::Utils::OpenHandle(*result);
    }
  }

  MaybeObject* result = holder_handle->GetPropertyPostInterceptor(
      *receiver_handle,
      *name_handle,
      attributes);
  RETURN_IF_SCHEDULED_EXCEPTION(isolate);
  return result;
}


bool JSObject::HasRealNamedProperty(String* key) {
  // Check access rights if needed.
  Isolate* isolate = GetIsolate();
  if (IsAccessCheckNeeded()) {
    if (!isolate->MayNamedAccess(this, key, v8::ACCESS_HAS)) {
      isolate->ReportFailedAccessCheck(this, v8::ACCESS_HAS);
      return false;
    }
  }

  LookupResult result(isolate);
  LocalLookupRealNamedProperty(key, &result);
  return result.IsProperty() && (result.type() != INTERCEPTOR);
}


bool JSObject::HasRealElementProperty(uint32_t index) {
  // Check access rights if needed.
  if (IsAccessCheckNeeded()) {
    Heap* heap = GetHeap();
    if (!heap->isolate()->MayIndexedAccess(this, index, v8::ACCESS_HAS)) {
      heap->isolate()->ReportFailedAccessCheck(this, v8::ACCESS_HAS);
      return false;
    }
  }

  // Handle [] on String objects.
  if (this->IsStringObjectWithCharacterAt(index)) return true;

  switch (GetElementsKind()) {
    case FAST_SMI_ONLY_ELEMENTS:
    case FAST_ELEMENTS: {
      uint32_t length = IsJSArray() ?
          static_cast<uint32_t>(
              Smi::cast(JSArray::cast(this)->length())->value()) :
          static_cast<uint32_t>(FixedArray::cast(elements())->length());
      return (index < length) &&
          !FixedArray::cast(elements())->get(index)->IsTheHole();
    }
    case FAST_DOUBLE_ELEMENTS: {
      uint32_t length = IsJSArray() ?
          static_cast<uint32_t>(
              Smi::cast(JSArray::cast(this)->length())->value()) :
          static_cast<uint32_t>(FixedDoubleArray::cast(elements())->length());
      return (index < length) &&
          !FixedDoubleArray::cast(elements())->is_the_hole(index);
      break;
    }
    case EXTERNAL_PIXEL_ELEMENTS: {
      ExternalPixelArray* pixels = ExternalPixelArray::cast(elements());
      return index < static_cast<uint32_t>(pixels->length());
    }
    case EXTERNAL_BYTE_ELEMENTS:
    case EXTERNAL_UNSIGNED_BYTE_ELEMENTS:
    case EXTERNAL_SHORT_ELEMENTS:
    case EXTERNAL_UNSIGNED_SHORT_ELEMENTS:
    case EXTERNAL_INT_ELEMENTS:
    case EXTERNAL_UNSIGNED_INT_ELEMENTS:
    case EXTERNAL_FLOAT_ELEMENTS:
    case EXTERNAL_DOUBLE_ELEMENTS: {
      ExternalArray* array = ExternalArray::cast(elements());
      return index < static_cast<uint32_t>(array->length());
    }
    case DICTIONARY_ELEMENTS: {
      return element_dictionary()->FindEntry(index)
          != SeededNumberDictionary::kNotFound;
    }
    case NON_STRICT_ARGUMENTS_ELEMENTS:
      UNIMPLEMENTED();
      break;
  }
  // All possibilities have been handled above already.
  UNREACHABLE();
  return GetHeap()->null_value();
}


bool JSObject::HasRealNamedCallbackProperty(String* key) {
  // Check access rights if needed.
  Isolate* isolate = GetIsolate();
  if (IsAccessCheckNeeded()) {
    if (!isolate->MayNamedAccess(this, key, v8::ACCESS_HAS)) {
      isolate->ReportFailedAccessCheck(this, v8::ACCESS_HAS);
      return false;
    }
  }

  LookupResult result(isolate);
  LocalLookupRealNamedProperty(key, &result);
  return result.IsFound() && (result.type() == CALLBACKS);
}


int JSObject::NumberOfLocalProperties(PropertyAttributes filter) {
  return HasFastProperties() ?
      map()->NumberOfDescribedProperties(filter) :
      property_dictionary()->NumberOfElementsFilterAttributes(filter);
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
void JSObject::GetLocalPropertyNames(FixedArray* storage, int index) {
  ASSERT(storage->length() >= (NumberOfLocalProperties() - index));
  if (HasFastProperties()) {
    DescriptorArray* descs = map()->instance_descriptors();
    for (int i = 0; i < descs->number_of_descriptors(); i++) {
      if (descs->IsProperty(i)) storage->set(index++, descs->GetKey(i));
    }
    ASSERT(storage->length() >= index);
  } else {
    property_dictionary()->CopyKeysTo(storage,
                                      index,
                                      StringDictionary::UNSORTED);
  }
}


int JSObject::NumberOfLocalElements(PropertyAttributes filter) {
  return GetLocalElementKeys(NULL, filter);
}


int JSObject::NumberOfEnumElements() {
  // Fast case for objects with no elements.
  if (!IsJSValue() && HasFastElements()) {
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
    case FAST_SMI_ONLY_ELEMENTS:
    case FAST_ELEMENTS: {
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
    case FAST_DOUBLE_ELEMENTS: {
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

  Object* AsObject() { return string_; }

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

  MUST_USE_RESULT MaybeObject* AsObject() {
    Object* obj;
    { MaybeObject* maybe_obj = source_->GetHeap()->AllocateFixedArray(4);
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

  Object* AsObject() {
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

// Utf8SymbolKey carries a vector of chars as key.
class Utf8SymbolKey : public HashTableKey {
 public:
  explicit Utf8SymbolKey(Vector<const char> string, uint32_t seed)
      : string_(string), hash_field_(0), seed_(seed) { }

  bool IsMatch(Object* string) {
    return String::cast(string)->IsEqualTo(string_);
  }

  uint32_t Hash() {
    if (hash_field_ != 0) return hash_field_ >> String::kHashShift;
    unibrow::Utf8InputBuffer<> buffer(string_.start(),
                                      static_cast<unsigned>(string_.length()));
    chars_ = buffer.Utf16Length();
    hash_field_ = String::ComputeHashField(&buffer, chars_, seed_);
    uint32_t result = hash_field_ >> String::kHashShift;
    ASSERT(result != 0);  // Ensure that the hash value of 0 is never computed.
    return result;
  }

  uint32_t HashForObject(Object* other) {
    return String::cast(other)->Hash();
  }

  MaybeObject* AsObject() {
    if (hash_field_ == 0) Hash();
    return Isolate::Current()->heap()->AllocateSymbol(
        string_, chars_, hash_field_);
  }

  Vector<const char> string_;
  uint32_t hash_field_;
  int chars_;  // Caches the number of characters when computing the hash code.
  uint32_t seed_;
};


template <typename Char>
class SequentialSymbolKey : public HashTableKey {
 public:
  explicit SequentialSymbolKey(Vector<const Char> string, uint32_t seed)
      : string_(string), hash_field_(0), seed_(seed) { }

  uint32_t Hash() {
    StringHasher hasher(string_.length(), seed_);

    // Very long strings have a trivial hash that doesn't inspect the
    // string contents.
    if (hasher.has_trivial_hash()) {
      hash_field_ = hasher.GetHashField();
    } else {
      int i = 0;
      // Do the iterative array index computation as long as there is a
      // chance this is an array index.
      while (i < string_.length() && hasher.is_array_index()) {
        hasher.AddCharacter(static_cast<uc32>(string_[i]));
        i++;
      }

      // Process the remaining characters without updating the array
      // index.
      while (i < string_.length()) {
        hasher.AddCharacterNoIndex(static_cast<uc32>(string_[i]));
        i++;
      }
      hash_field_ = hasher.GetHashField();
    }

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



class AsciiSymbolKey : public SequentialSymbolKey<char> {
 public:
  AsciiSymbolKey(Vector<const char> str, uint32_t seed)
      : SequentialSymbolKey<char>(str, seed) { }

  bool IsMatch(Object* string) {
    return String::cast(string)->IsAsciiEqualTo(string_);
  }

  MaybeObject* AsObject() {
    if (hash_field_ == 0) Hash();
    return HEAP->AllocateAsciiSymbol(string_, hash_field_);
  }
};


class SubStringAsciiSymbolKey : public HashTableKey {
 public:
  explicit SubStringAsciiSymbolKey(Handle<SeqAsciiString> string,
                                   int from,
                                   int length,
                                   uint32_t seed)
      : string_(string), from_(from), length_(length), seed_(seed) { }

  uint32_t Hash() {
    ASSERT(length_ >= 0);
    ASSERT(from_ + length_ <= string_->length());
    StringHasher hasher(length_, string_->GetHeap()->HashSeed());

    // Very long strings have a trivial hash that doesn't inspect the
    // string contents.
    if (hasher.has_trivial_hash()) {
      hash_field_ = hasher.GetHashField();
    } else {
      int i = 0;
      // Do the iterative array index computation as long as there is a
      // chance this is an array index.
      while (i < length_ && hasher.is_array_index()) {
        hasher.AddCharacter(static_cast<uc32>(
            string_->SeqAsciiStringGet(i + from_)));
        i++;
      }

      // Process the remaining characters without updating the array
      // index.
      while (i < length_) {
        hasher.AddCharacterNoIndex(static_cast<uc32>(
            string_->SeqAsciiStringGet(i + from_)));
        i++;
      }
      hash_field_ = hasher.GetHashField();
    }

    uint32_t result = hash_field_ >> String::kHashShift;
    ASSERT(result != 0);  // Ensure that the hash value of 0 is never computed.
    return result;
  }


  uint32_t HashForObject(Object* other) {
    return String::cast(other)->Hash();
  }

  bool IsMatch(Object* string) {
    Vector<const char> chars(string_->GetChars() + from_, length_);
    return String::cast(string)->IsAsciiEqualTo(chars);
  }

  MaybeObject* AsObject() {
    if (hash_field_ == 0) Hash();
    Vector<const char> chars(string_->GetChars() + from_, length_);
    return HEAP->AllocateAsciiSymbol(chars, hash_field_);
  }

 private:
  Handle<SeqAsciiString> string_;
  int from_;
  int length_;
  uint32_t hash_field_;
  uint32_t seed_;
};


class TwoByteSymbolKey : public SequentialSymbolKey<uc16> {
 public:
  explicit TwoByteSymbolKey(Vector<const uc16> str, uint32_t seed)
      : SequentialSymbolKey<uc16>(str, seed) { }

  bool IsMatch(Object* string) {
    return String::cast(string)->IsTwoByteEqualTo(string_);
  }

  MaybeObject* AsObject() {
    if (hash_field_ == 0) Hash();
    return HEAP->AllocateTwoByteSymbol(string_, hash_field_);
  }
};


// SymbolKey carries a string/symbol object as key.
class SymbolKey : public HashTableKey {
 public:
  explicit SymbolKey(String* string)
      : string_(string) { }

  bool IsMatch(Object* string) {
    return String::cast(string)->Equals(string_);
  }

  uint32_t Hash() { return string_->Hash(); }

  uint32_t HashForObject(Object* other) {
    return String::cast(other)->Hash();
  }

  MaybeObject* AsObject() {
    // Attempt to flatten the string, so that symbols will most often
    // be flat strings.
    string_ = string_->TryFlattenGetString();
    Heap* heap = string_->GetHeap();
    // Transform string to symbol if possible.
    Map* map = heap->SymbolMapForString(string_);
    if (map != NULL) {
      string_->set_map_no_write_barrier(map);
      ASSERT(string_->IsSymbol());
      return string_;
    }
    // Otherwise allocate a new symbol.
    StringInputBuffer buffer(string_);
    return heap->AllocateInternalSymbol(&buffer,
                                        string_->length(),
                                        string_->hash_field());
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
MaybeObject* HashTable<Shape, Key>::Allocate(int at_least_space_for,
                                             PretenureFlag pretenure) {
  int capacity = ComputeCapacity(at_least_space_for);
  if (capacity > HashTable::kMaxCapacity) {
    return Failure::OutOfMemoryException();
  }

  Object* obj;
  { MaybeObject* maybe_obj = Isolate::Current()->heap()->
        AllocateHashTable(EntryToIndex(capacity), pretenure);
    if (!maybe_obj->ToObject(&obj)) return maybe_obj;
  }
  HashTable::cast(obj)->SetNumberOfElements(0);
  HashTable::cast(obj)->SetNumberOfDeletedElements(0);
  HashTable::cast(obj)->SetCapacity(capacity);
  return obj;
}


// Find entry for key otherwise return kNotFound.
int StringDictionary::FindEntry(String* key) {
  if (!key->IsSymbol()) {
    return HashTable<StringDictionaryShape, String*>::FindEntry(key);
  }

  // Optimized for symbol key. Knowledge of the key type allows:
  // 1. Move the check if the key is a symbol out of the loop.
  // 2. Avoid comparing hash codes in symbol to symbol comparison.
  // 3. Detect a case when a dictionary key is not a symbol but the key is.
  //    In case of positive result the dictionary key may be replaced by
  //    the symbol with minimal performance penalty. It gives a chance to
  //    perform further lookups in code stubs (and significant performance boost
  //    a certain style of code).

  // EnsureCapacity will guarantee the hash table is never full.
  uint32_t capacity = Capacity();
  uint32_t entry = FirstProbe(key->Hash(), capacity);
  uint32_t count = 1;

  while (true) {
    int index = EntryToIndex(entry);
    Object* element = get(index);
    if (element->IsUndefined()) break;  // Empty entry.
    if (key == element) return entry;
    if (!element->IsSymbol() &&
        !element->IsTheHole() &&
        String::cast(element)->Equals(key)) {
      // Replace a non-symbol key by the equivalent symbol for faster further
      // lookups.
      set(index, key);
      return entry;
    }
    ASSERT(element->IsTheHole() || !String::cast(element)->Equals(key));
    entry = NextProbe(entry, count++, capacity);
  }
  return kNotFound;
}


bool StringDictionary::ContainsTransition(int entry) {
  switch (DetailsAt(entry).type()) {
    case MAP_TRANSITION:
    case CONSTANT_TRANSITION:
    case ELEMENTS_TRANSITION:
      return true;
    case CALLBACKS: {
      Object* value = ValueAt(entry);
      if (!value->IsAccessorPair()) return false;
      AccessorPair* accessors = AccessorPair::cast(value);
      return accessors->getter()->IsMap() || accessors->setter()->IsMap();
    }
    case NORMAL:
    case FIELD:
    case CONSTANT_FUNCTION:
    case HANDLER:
    case INTERCEPTOR:
    case NULL_DESCRIPTOR:
      return false;
  }
  UNREACHABLE();  // Keep the compiler happy.
  return false;
}


template<typename Shape, typename Key>
MaybeObject* HashTable<Shape, Key>::Rehash(HashTable* new_table, Key key) {
  ASSERT(NumberOfElements() < new_table->Capacity());

  AssertNoAllocation no_gc;
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
        Allocate(nof * 2, pretenure ? TENURED : NOT_TENURED);
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
        Allocate(at_least_room_for, pretenure ? TENURED : NOT_TENURED);
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

template class HashTable<SymbolTableShape, HashTableKey*>;

template class HashTable<CompilationCacheShape, HashTableKey*>;

template class HashTable<MapCacheShape, HashTableKey*>;

template class HashTable<ObjectHashTableShape<1>, Object*>;

template class HashTable<ObjectHashTableShape<2>, Object*>;

template class Dictionary<StringDictionaryShape, String*>;

template class Dictionary<SeededNumberDictionaryShape, uint32_t>;

template class Dictionary<UnseededNumberDictionaryShape, uint32_t>;

template MaybeObject* Dictionary<SeededNumberDictionaryShape, uint32_t>::
    Allocate(int at_least_space_for);

template MaybeObject* Dictionary<UnseededNumberDictionaryShape, uint32_t>::
    Allocate(int at_least_space_for);

template MaybeObject* Dictionary<StringDictionaryShape, String*>::Allocate(
    int);

template MaybeObject* Dictionary<SeededNumberDictionaryShape, uint32_t>::AtPut(
    uint32_t, Object*);

template MaybeObject* Dictionary<UnseededNumberDictionaryShape, uint32_t>::
    AtPut(uint32_t, Object*);

template Object* Dictionary<SeededNumberDictionaryShape, uint32_t>::
    SlowReverseLookup(Object* value);

template Object* Dictionary<UnseededNumberDictionaryShape, uint32_t>::
    SlowReverseLookup(Object* value);

template Object* Dictionary<StringDictionaryShape, String*>::SlowReverseLookup(
    Object*);

template void Dictionary<SeededNumberDictionaryShape, uint32_t>::CopyKeysTo(
    FixedArray*,
    PropertyAttributes,
    Dictionary<SeededNumberDictionaryShape, uint32_t>::SortMode);

template Object* Dictionary<StringDictionaryShape, String*>::DeleteProperty(
    int, JSObject::DeleteMode);

template Object* Dictionary<SeededNumberDictionaryShape, uint32_t>::
    DeleteProperty(int, JSObject::DeleteMode);

template MaybeObject* Dictionary<StringDictionaryShape, String*>::Shrink(
    String*);

template MaybeObject* Dictionary<SeededNumberDictionaryShape, uint32_t>::Shrink(
    uint32_t);

template void Dictionary<StringDictionaryShape, String*>::CopyKeysTo(
    FixedArray*,
    int,
    Dictionary<StringDictionaryShape, String*>::SortMode);

template int
Dictionary<StringDictionaryShape, String*>::NumberOfElementsFilterAttributes(
    PropertyAttributes);

template MaybeObject* Dictionary<StringDictionaryShape, String*>::Add(
    String*, Object*, PropertyDetails);

template MaybeObject*
Dictionary<StringDictionaryShape, String*>::GenerateNewEnumerationIndices();

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

template MaybeObject* Dictionary<StringDictionaryShape, String*>::
    EnsureCapacity(int, String*);

template MaybeObject* Dictionary<SeededNumberDictionaryShape, uint32_t>::
    AddEntry(uint32_t, Object*, PropertyDetails, uint32_t);

template MaybeObject* Dictionary<UnseededNumberDictionaryShape, uint32_t>::
    AddEntry(uint32_t, Object*, PropertyDetails, uint32_t);

template MaybeObject* Dictionary<StringDictionaryShape, String*>::AddEntry(
    String*, Object*, PropertyDetails, uint32_t);

template
int Dictionary<SeededNumberDictionaryShape, uint32_t>::NumberOfEnumElements();

template
int Dictionary<StringDictionaryShape, String*>::NumberOfEnumElements();

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
        SeededNumberDictionary::Allocate(dict->NumberOfElements());
    if (!maybe_obj->ToObject(&obj)) return maybe_obj;
  }
  SeededNumberDictionary* new_dict = SeededNumberDictionary::cast(obj);

  AssertNoAllocation no_alloc;

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
      if (details.type() == CALLBACKS) {
        // Bail out and do the sorting of undefineds and array holes in JS.
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
  PropertyDetails no_details = PropertyDetails(NONE, NORMAL);
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
    { MaybeObject* maybe_obj = GetElementsTransitionMap(GetIsolate(),
                                                        FAST_ELEMENTS);
      if (!maybe_obj->ToObject(&obj)) return maybe_obj;
    }
    Map* new_map = Map::cast(obj);

    PretenureFlag tenure = heap->InNewSpace(this) ? NOT_TENURED: TENURED;
    Object* new_array;
    { MaybeObject* maybe_new_array =
          heap->AllocateFixedArray(dict->NumberOfElements(), tenure);
      if (!maybe_new_array->ToObject(&new_array)) return maybe_new_array;
    }
    FixedArray* fast_elements = FixedArray::cast(new_array);
    dict->CopyValuesTo(fast_elements);

    set_map(new_map);
    set_elements(fast_elements);
  } else if (HasExternalArrayElements()) {
    // External arrays cannot have holes or undefined elements.
    return Smi::FromInt(ExternalArray::cast(elements())->length());
  } else if (!HasFastDoubleElements()) {
    Object* obj;
    { MaybeObject* maybe_obj = EnsureWritableFastElements();
      if (!maybe_obj->ToObject(&obj)) return maybe_obj;
    }
  }
  ASSERT(HasFastTypeElements() || HasFastDoubleElements());

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
    AssertNoAllocation no_alloc;

    // Split elements into defined, undefined and the_hole, in that order.  Only
    // count locations for undefined and the hole, and fill them afterwards.
    WriteBarrierMode write_barrier = elements->GetWriteBarrierMode(no_alloc);
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
        clamped_value = static_cast<uint8_t>(double_value + 0.5);
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


JSGlobalPropertyCell* GlobalObject::GetPropertyCell(LookupResult* result) {
  ASSERT(!HasFastProperties());
  Object* value = property_dictionary()->ValueAt(result->GetDictionaryEntry());
  return JSGlobalPropertyCell::cast(value);
}


Handle<JSGlobalPropertyCell> GlobalObject::EnsurePropertyCell(
    Handle<GlobalObject> global,
    Handle<String> name) {
  Isolate* isolate = global->GetIsolate();
  CALL_HEAP_FUNCTION(isolate,
                     global->EnsurePropertyCell(*name),
                     JSGlobalPropertyCell);
}


MaybeObject* GlobalObject::EnsurePropertyCell(String* name) {
  ASSERT(!HasFastProperties());
  int entry = property_dictionary()->FindEntry(name);
  if (entry == StringDictionary::kNotFound) {
    Heap* heap = GetHeap();
    Object* cell;
    { MaybeObject* maybe_cell =
          heap->AllocateJSGlobalPropertyCell(heap->the_hole_value());
      if (!maybe_cell->ToObject(&cell)) return maybe_cell;
    }
    PropertyDetails details(NONE, NORMAL);
    details = details.AsDeleted();
    Object* dictionary;
    { MaybeObject* maybe_dictionary =
          property_dictionary()->Add(name, cell, details);
      if (!maybe_dictionary->ToObject(&dictionary)) return maybe_dictionary;
    }
    set_properties(StringDictionary::cast(dictionary));
    return cell;
  } else {
    Object* value = property_dictionary()->ValueAt(entry);
    ASSERT(value->IsJSGlobalPropertyCell());
    return value;
  }
}


MaybeObject* SymbolTable::LookupString(String* string, Object** s) {
  SymbolKey key(string);
  return LookupKey(&key, s);
}


// This class is used for looking up two character strings in the symbol table.
// If we don't have a hit we don't want to waste much time so we unroll the
// string hash calculation loop here for speed.  Doesn't work if the two
// characters form a decimal integer, since such strings have a different hash
// algorithm.
class TwoCharHashTableKey : public HashTableKey {
 public:
  TwoCharHashTableKey(uint32_t c1, uint32_t c2, uint32_t seed)
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
    if ((hash & String::kHashBitMask) == 0) hash = String::kZeroHash;
#ifdef DEBUG
    StringHasher hasher(2, seed);
    hasher.AddCharacter(c1);
    hasher.AddCharacter(c2);
    // If this assert fails then we failed to reproduce the two-character
    // version of the string hashing algorithm above.  One reason could be
    // that we were passed two digits as characters, since the hash
    // algorithm is different in that case.
    ASSERT_EQ(static_cast<int>(hasher.GetHash()), static_cast<int>(hash));
#endif
    hash_ = hash;
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

  Object* AsObject() {
    // The TwoCharHashTableKey is only used for looking in the symbol
    // table, not for adding to it.
    UNREACHABLE();
    return NULL;
  }

 private:
  uint32_t c1_;
  uint32_t c2_;
  uint32_t hash_;
};


bool SymbolTable::LookupSymbolIfExists(String* string, String** symbol) {
  SymbolKey key(string);
  int entry = FindEntry(&key);
  if (entry == kNotFound) {
    return false;
  } else {
    String* result = String::cast(KeyAt(entry));
    ASSERT(StringShape(result).IsSymbol());
    *symbol = result;
    return true;
  }
}


bool SymbolTable::LookupTwoCharsSymbolIfExists(uint32_t c1,
                                               uint32_t c2,
                                               String** symbol) {
  TwoCharHashTableKey key(c1, c2, GetHeap()->HashSeed());
  int entry = FindEntry(&key);
  if (entry == kNotFound) {
    return false;
  } else {
    String* result = String::cast(KeyAt(entry));
    ASSERT(StringShape(result).IsSymbol());
    *symbol = result;
    return true;
  }
}


MaybeObject* SymbolTable::LookupSymbol(Vector<const char> str,
                                       Object** s) {
  Utf8SymbolKey key(str, GetHeap()->HashSeed());
  return LookupKey(&key, s);
}


MaybeObject* SymbolTable::LookupAsciiSymbol(Vector<const char> str,
                                            Object** s) {
  AsciiSymbolKey key(str, GetHeap()->HashSeed());
  return LookupKey(&key, s);
}


MaybeObject* SymbolTable::LookupSubStringAsciiSymbol(Handle<SeqAsciiString> str,
                                                     int from,
                                                     int length,
                                                     Object** s) {
  SubStringAsciiSymbolKey key(str, from, length, GetHeap()->HashSeed());
  return LookupKey(&key, s);
}


MaybeObject* SymbolTable::LookupTwoByteSymbol(Vector<const uc16> str,
                                              Object** s) {
  TwoByteSymbolKey key(str, GetHeap()->HashSeed());
  return LookupKey(&key, s);
}

MaybeObject* SymbolTable::LookupKey(HashTableKey* key, Object** s) {
  int entry = FindEntry(key);

  // Symbol already in table.
  if (entry != kNotFound) {
    *s = KeyAt(entry);
    return this;
  }

  // Adding new symbol. Grow table if needed.
  Object* obj;
  { MaybeObject* maybe_obj = EnsureCapacity(1, key);
    if (!maybe_obj->ToObject(&obj)) return maybe_obj;
  }

  // Create symbol object.
  Object* symbol;
  { MaybeObject* maybe_symbol = key->AsObject();
    if (!maybe_symbol->ToObject(&symbol)) return maybe_symbol;
  }

  // If the symbol table grew as part of EnsureCapacity, obj is not
  // the current symbol table and therefore we cannot use
  // SymbolTable::cast here.
  SymbolTable* table = reinterpret_cast<SymbolTable*>(obj);

  // Add the new symbol and return it along with the symbol table.
  entry = table->FindInsertionEntry(key->Hash());
  table->set(EntryToIndex(entry), symbol);
  table->ElementAdded();
  *s = symbol;
  return table;
}


Object* CompilationCacheTable::Lookup(String* src) {
  StringKey key(src);
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


MaybeObject* CompilationCacheTable::Put(String* src, Object* value) {
  StringKey key(src);
  Object* obj;
  { MaybeObject* maybe_obj = EnsureCapacity(1, &key);
    if (!maybe_obj->ToObject(&obj)) return maybe_obj;
  }

  CompilationCacheTable* cache =
      reinterpret_cast<CompilationCacheTable*>(obj);
  int entry = cache->FindInsertionEntry(key.Hash());
  cache->set(EntryToIndex(entry), src);
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
  { MaybeObject* maybe_k = key.AsObject();
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


// SymbolsKey used for HashTable where key is array of symbols.
class SymbolsKey : public HashTableKey {
 public:
  explicit SymbolsKey(FixedArray* symbols) : symbols_(symbols) { }

  bool IsMatch(Object* symbols) {
    FixedArray* o = FixedArray::cast(symbols);
    int len = symbols_->length();
    if (o->length() != len) return false;
    for (int i = 0; i < len; i++) {
      if (o->get(i) != symbols_->get(i)) return false;
    }
    return true;
  }

  uint32_t Hash() { return HashForObject(symbols_); }

  uint32_t HashForObject(Object* obj) {
    FixedArray* symbols = FixedArray::cast(obj);
    int len = symbols->length();
    uint32_t hash = 0;
    for (int i = 0; i < len; i++) {
      hash ^= String::cast(symbols->get(i))->Hash();
    }
    return hash;
  }

  Object* AsObject() { return symbols_; }

 private:
  FixedArray* symbols_;
};


Object* MapCache::Lookup(FixedArray* array) {
  SymbolsKey key(array);
  int entry = FindEntry(&key);
  if (entry == kNotFound) return GetHeap()->undefined_value();
  return get(EntryToIndex(entry) + 1);
}


MaybeObject* MapCache::Put(FixedArray* array, Map* value) {
  SymbolsKey key(array);
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
MaybeObject* Dictionary<Shape, Key>::Allocate(int at_least_space_for) {
  Object* obj;
  { MaybeObject* maybe_obj =
        HashTable<Shape, Key>::Allocate(at_least_space_for);
    if (!maybe_obj->ToObject(&obj)) return maybe_obj;
  }
  // Initialize the next enumeration index.
  Dictionary<Shape, Key>::cast(obj)->
      SetNextEnumerationIndex(PropertyDetails::kInitialIndex);
  return obj;
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
      enumeration_order->set(pos++, Smi::FromInt(DetailsAt(i).index()));
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
      PropertyDetails new_details =
          PropertyDetails(details.attributes(), details.type(), enum_index);
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
  { MaybeObject* maybe_k = Shape::AsObject(key);
    if (!maybe_k->ToObject(&k)) return maybe_k;
  }
  PropertyDetails details = PropertyDetails(NONE, NORMAL);

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
  { MaybeObject* maybe_k = Shape::AsObject(key);
    if (!maybe_k->ToObject(&k)) return maybe_k;
  }

  uint32_t entry = Dictionary<Shape, Key>::FindInsertionEntry(hash);
  // Insert element at empty or deleted entry
  if (!details.IsDeleted() && details.index() == 0 && Shape::kIsEnumerable) {
    // Assign an enumeration index to the property and update
    // SetNextEnumerationIndex.
    int index = NextEnumerationIndex();
    details = PropertyDetails(details.attributes(), details.type(), index);
    SetNextEnumerationIndex(index + 1);
  }
  SetEntry(entry, k, value, details);
  ASSERT((Dictionary<Shape, Key>::KeyAt(entry)->IsNumber()
          || Dictionary<Shape, Key>::KeyAt(entry)->IsString()));
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
  return Add(key, value, PropertyDetails(NONE, NORMAL));
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
                            DetailsAt(entry).index());
  MaybeObject* maybe_object_key = SeededNumberDictionaryShape::AsObject(key);
  Object* object_key;
  if (!maybe_object_key->ToObject(&object_key)) return maybe_object_key;
  SetEntry(entry, object_key, value, details);
  return this;
}


MaybeObject* UnseededNumberDictionary::Set(uint32_t key,
                                           Object* value) {
  int entry = FindEntry(key);
  if (entry == kNotFound) return AddNumberEntry(key, value);
  MaybeObject* maybe_object_key = UnseededNumberDictionaryShape::AsObject(key);
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
    if (HashTable<Shape, Key>::IsKey(k)) {
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


void StringDictionary::CopyEnumKeysTo(FixedArray* storage,
                                      FixedArray* sort_array) {
  ASSERT(storage->length() >= NumberOfEnumElements());
  int capacity = Capacity();
  int index = 0;
  for (int i = 0; i < capacity; i++) {
     Object* k = KeyAt(i);
     if (IsKey(k)) {
       PropertyDetails details = DetailsAt(i);
       if (details.IsDeleted() || details.IsDontEnum()) continue;
       storage->set(index, k);
       sort_array->set(index, Smi::FromInt(details.index()));
       index++;
     }
  }
  storage->SortPairs(sort_array, sort_array->length());
  ASSERT(storage->length() >= index);
}


template<typename Shape, typename Key>
void Dictionary<Shape, Key>::CopyKeysTo(
    FixedArray* storage,
    int index,
    typename Dictionary<Shape, Key>::SortMode sort_mode) {
  ASSERT(storage->length() >= NumberOfElementsFilterAttributes(
      static_cast<PropertyAttributes>(NONE)));
  int capacity = HashTable<Shape, Key>::Capacity();
  for (int i = 0; i < capacity; i++) {
    Object* k = HashTable<Shape, Key>::KeyAt(i);
    if (HashTable<Shape, Key>::IsKey(k)) {
      PropertyDetails details = DetailsAt(i);
      if (details.IsDeleted()) continue;
      storage->set(index++, k);
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
      if (e->IsJSGlobalPropertyCell()) {
        e = JSGlobalPropertyCell::cast(e)->value();
      }
      if (e == value) return k;
    }
  }
  Heap* heap = Dictionary<Shape, Key>::GetHeap();
  return heap->undefined_value();
}


MaybeObject* StringDictionary::TransformPropertiesToFastFor(
    JSObject* obj, int unused_property_fields) {
  // Make sure we preserve dictionary representation if there are too many
  // descriptors.
  if (NumberOfElements() > DescriptorArray::kMaxNumberOfDescriptors) return obj;

  // Figure out if it is necessary to generate new enumeration indices.
  int max_enumeration_index =
      NextEnumerationIndex() +
          (DescriptorArray::kMaxNumberOfDescriptors -
           NumberOfElements());
  if (!PropertyDetails::IsValidIndex(max_enumeration_index)) {
    Object* result;
    { MaybeObject* maybe_result = GenerateNewEnumerationIndices();
      if (!maybe_result->ToObject(&result)) return maybe_result;
    }
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
      if (type == NORMAL &&
          (!value->IsJSFunction() || heap->InNewSpace(value))) {
        number_of_fields += 1;
      }
    }
  }

  // Allocate the instance descriptor.
  DescriptorArray* descriptors;
  { MaybeObject* maybe_descriptors =
        DescriptorArray::Allocate(instance_descriptor_length);
    if (!maybe_descriptors->To<DescriptorArray>(&descriptors)) {
      return maybe_descriptors;
    }
  }

  DescriptorArray::WhitenessWitness witness(descriptors);

  int inobject_props = obj->map()->inobject_properties();
  int number_of_allocated_fields =
      number_of_fields + unused_property_fields - inobject_props;
  if (number_of_allocated_fields < 0) {
    // There is enough inobject space for all fields (including unused).
    number_of_allocated_fields = 0;
    unused_property_fields = inobject_props - number_of_fields;
  }

  // Allocate the fixed array for the fields.
  Object* fields;
  { MaybeObject* maybe_fields =
        heap->AllocateFixedArray(number_of_allocated_fields);
    if (!maybe_fields->ToObject(&fields)) return maybe_fields;
  }

  // Fill in the instance descriptor and the fields.
  int next_descriptor = 0;
  int current_offset = 0;
  for (int i = 0; i < capacity; i++) {
    Object* k = KeyAt(i);
    if (IsKey(k)) {
      Object* value = ValueAt(i);
      // Ensure the key is a symbol before writing into the instance descriptor.
      Object* key;
      { MaybeObject* maybe_key = heap->LookupSymbol(String::cast(k));
        if (!maybe_key->ToObject(&key)) return maybe_key;
      }
      PropertyDetails details = DetailsAt(i);
      PropertyType type = details.type();

      if (value->IsJSFunction() && !heap->InNewSpace(value)) {
        ConstantFunctionDescriptor d(String::cast(key),
                                     JSFunction::cast(value),
                                     details.attributes(),
                                     details.index());
        descriptors->Set(next_descriptor++, &d, witness);
      } else if (type == NORMAL) {
        if (current_offset < inobject_props) {
          obj->InObjectPropertyAtPut(current_offset,
                                     value,
                                     UPDATE_WRITE_BARRIER);
        } else {
          int offset = current_offset - inobject_props;
          FixedArray::cast(fields)->set(offset, value);
        }
        FieldDescriptor d(String::cast(key),
                          current_offset++,
                          details.attributes(),
                          details.index());
        descriptors->Set(next_descriptor++, &d, witness);
      } else if (type == CALLBACKS) {
        if (value->IsAccessorPair()) {
          MaybeObject* maybe_copy =
              AccessorPair::cast(value)->CopyWithoutTransitions();
          if (!maybe_copy->To(&value)) return maybe_copy;
        }
        CallbacksDescriptor d(String::cast(key),
                              value,
                              details.attributes(),
                              details.index());
        descriptors->Set(next_descriptor++, &d, witness);
      } else {
        UNREACHABLE();
      }
    }
  }
  ASSERT(current_offset == number_of_fields);

  descriptors->Sort(witness);
  // Allocate new map.
  Object* new_map;
  { MaybeObject* maybe_new_map = obj->map()->CopyDropDescriptors();
    if (!maybe_new_map->ToObject(&new_map)) return maybe_new_map;
  }

  // Transform the object.
  obj->set_map(Map::cast(new_map));
  obj->map()->set_instance_descriptors(descriptors);
  obj->map()->set_unused_property_fields(unused_property_fields);

  obj->set_properties(FixedArray::cast(fields));
  ASSERT(obj->IsJSObject());

  descriptors->SetNextEnumerationIndex(NextEnumerationIndex());
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
      return GetHeap()->undefined_value();
    }
  }
  int entry = FindEntry(key);
  if (entry == kNotFound) return GetHeap()->undefined_value();
  return get(EntryToIndex(entry) + 1);
}


MaybeObject* ObjectHashTable::Put(Object* key, Object* value) {
  ASSERT(IsKey(key));

  // Make sure the key object has an identity hash code.
  int hash;
  { MaybeObject* maybe_hash = key->GetHash(ALLOW_CREATION);
    if (maybe_hash->IsFailure()) return maybe_hash;
    hash = Smi::cast(maybe_hash->ToObjectUnchecked())->value();
  }
  int entry = FindEntry(key);

  // Check whether to perform removal operation.
  if (value->IsUndefined()) {
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
  Handle<Object> break_point_info(debug_info->GetBreakPointInfo(code_position));
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
  Isolate* isolate = Isolate::Current();
  Handle<Object> break_point_info(debug_info->GetBreakPointInfo(code_position));
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
  Isolate* isolate = Isolate::Current();
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
  // If there was no break point objects before just set it.
  if (break_point_info->break_point_objects()->IsUndefined()) {
    break_point_info->set_break_point_objects(*break_point_object);
    return;
  }
  // If the break point object is the same as before just ignore.
  if (break_point_info->break_point_objects() == *break_point_object) return;
  // If there was one break point object before replace with array.
  if (!break_point_info->break_point_objects()->IsFixedArray()) {
    Handle<FixedArray> array = FACTORY->NewFixedArray(2);
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
      FACTORY->NewFixedArray(old_array->length() + 1);
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


MaybeObject* JSDate::GetField(Object* object, Smi* index) {
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
  if (isnan(time)) return GetIsolate()->heap()->nan_value();

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

  if (isnan(value)) return GetIsolate()->heap()->nan_value();

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

} }  // namespace v8::internal
