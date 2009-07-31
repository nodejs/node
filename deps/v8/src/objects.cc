// Copyright 2006-2009 the V8 project authors. All rights reserved.
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
#include "bootstrapper.h"
#include "debug.h"
#include "execution.h"
#include "objects-inl.h"
#include "macro-assembler.h"
#include "scanner.h"
#include "scopeinfo.h"
#include "string-stream.h"

#ifdef ENABLE_DISASSEMBLER
#include "disassembler.h"
#endif


namespace v8 {
namespace internal {

// Getters and setters are stored in a fixed array property.  These are
// constants for their indices.
const int kGetterIndex = 0;
const int kSetterIndex = 1;

bool Object::IsInstanceOf(FunctionTemplateInfo* expected) {
  // There is a constraint on the object; check
  if (!this->IsJSObject()) return false;
  // Fetch the constructor function of the object
  Object* cons_obj = JSObject::cast(this)->map()->constructor();
  if (!cons_obj->IsJSFunction()) return false;
  JSFunction* fun = JSFunction::cast(cons_obj);
  // Iterate through the chain of inheriting function templates to
  // see if the required one occurs.
  for (Object* type = fun->shared()->function_data();
       type->IsFunctionTemplateInfo();
       type = FunctionTemplateInfo::cast(type)->parent_template()) {
    if (type == expected) return true;
  }
  // Didn't find the required type in the inheritance chain.
  return false;
}


static Object* CreateJSValue(JSFunction* constructor, Object* value) {
  Object* result = Heap::AllocateJSObject(constructor);
  if (result->IsFailure()) return result;
  JSValue::cast(result)->set_value(value);
  return result;
}


Object* Object::ToObject(Context* global_context) {
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


Object* Object::ToObject() {
  Context* global_context = Top::context()->global_context();
  if (IsJSObject()) {
    return this;
  } else if (IsNumber()) {
    return CreateJSValue(global_context->number_function(), this);
  } else if (IsBoolean()) {
    return CreateJSValue(global_context->boolean_function(), this);
  } else if (IsString()) {
    return CreateJSValue(global_context->string_function(), this);
  }

  // Throw a type error.
  return Failure::InternalError();
}


Object* Object::ToBoolean() {
  if (IsTrue()) return Heap::true_value();
  if (IsFalse()) return Heap::false_value();
  if (IsSmi()) {
    return Heap::ToBoolean(Smi::cast(this)->value() != 0);
  }
  if (IsUndefined() || IsNull()) return Heap::false_value();
  // Undetectable object is false
  if (IsUndetectableObject()) {
    return Heap::false_value();
  }
  if (IsString()) {
    return Heap::ToBoolean(String::cast(this)->length() != 0);
  }
  if (IsHeapNumber()) {
    return HeapNumber::cast(this)->HeapNumberToBoolean();
  }
  return Heap::true_value();
}


void Object::Lookup(String* name, LookupResult* result) {
  if (IsJSObject()) return JSObject::cast(this)->Lookup(name, result);
  Object* holder = NULL;
  Context* global_context = Top::context()->global_context();
  if (IsString()) {
    holder = global_context->string_function()->instance_prototype();
  } else if (IsNumber()) {
    holder = global_context->number_function()->instance_prototype();
  } else if (IsBoolean()) {
    holder = global_context->boolean_function()->instance_prototype();
  }
  ASSERT(holder != NULL);  // Cannot handle null or undefined.
  JSObject::cast(holder)->Lookup(name, result);
}


Object* Object::GetPropertyWithReceiver(Object* receiver,
                                        String* name,
                                        PropertyAttributes* attributes) {
  LookupResult result;
  Lookup(name, &result);
  Object* value = GetProperty(receiver, &result, name, attributes);
  ASSERT(*attributes <= ABSENT);
  return value;
}


Object* Object::GetPropertyWithCallback(Object* receiver,
                                        Object* structure,
                                        String* name,
                                        Object* holder) {
  // To accommodate both the old and the new api we switch on the
  // data structure used to store the callbacks.  Eventually proxy
  // callbacks should be phased out.
  if (structure->IsProxy()) {
    AccessorDescriptor* callback =
        reinterpret_cast<AccessorDescriptor*>(Proxy::cast(structure)->proxy());
    Object* value = (callback->getter)(receiver, callback->data);
    RETURN_IF_SCHEDULED_EXCEPTION();
    return value;
  }

  // api style callbacks.
  if (structure->IsAccessorInfo()) {
    AccessorInfo* data = AccessorInfo::cast(structure);
    Object* fun_obj = data->getter();
    v8::AccessorGetter call_fun = v8::ToCData<v8::AccessorGetter>(fun_obj);
    HandleScope scope;
    Handle<JSObject> self(JSObject::cast(receiver));
    Handle<JSObject> holder_handle(JSObject::cast(holder));
    Handle<String> key(name);
    Handle<Object> fun_data(data->data());
    LOG(ApiNamedPropertyAccess("load", *self, name));
    v8::AccessorInfo info(v8::Utils::ToLocal(self),
                          v8::Utils::ToLocal(fun_data),
                          v8::Utils::ToLocal(holder_handle));
    v8::Handle<v8::Value> result;
    {
      // Leaving JavaScript.
      VMState state(EXTERNAL);
      result = call_fun(v8::Utils::ToLocal(key), info);
    }
    RETURN_IF_SCHEDULED_EXCEPTION();
    if (result.IsEmpty()) return Heap::undefined_value();
    return *v8::Utils::OpenHandle(*result);
  }

  // __defineGetter__ callback
  if (structure->IsFixedArray()) {
    Object* getter = FixedArray::cast(structure)->get(kGetterIndex);
    if (getter->IsJSFunction()) {
      return Object::GetPropertyWithDefinedGetter(receiver,
                                                  JSFunction::cast(getter));
    }
    // Getter is not a function.
    return Heap::undefined_value();
  }

  UNREACHABLE();
  return 0;
}


Object* Object::GetPropertyWithDefinedGetter(Object* receiver,
                                             JSFunction* getter) {
  HandleScope scope;
  Handle<JSFunction> fun(JSFunction::cast(getter));
  Handle<Object> self(receiver);
#ifdef ENABLE_DEBUGGER_SUPPORT
  // Handle stepping into a getter if step into is active.
  if (Debug::StepInActive()) {
    Debug::HandleStepIn(fun, Handle<Object>::null(), 0, false);
  }
#endif
  bool has_pending_exception;
  Handle<Object> result =
      Execution::Call(fun, self, 0, NULL, &has_pending_exception);
  // Check for pending exception and return the result.
  if (has_pending_exception) return Failure::Exception();
  return *result;
}


// Only deal with CALLBACKS and INTERCEPTOR
Object* JSObject::GetPropertyWithFailedAccessCheck(
    Object* receiver,
    LookupResult* result,
    String* name,
    PropertyAttributes* attributes) {
  if (result->IsValid()) {
    switch (result->type()) {
      case CALLBACKS: {
        // Only allow API accessors.
        Object* obj = result->GetCallbackObject();
        if (obj->IsAccessorInfo()) {
          AccessorInfo* info = AccessorInfo::cast(obj);
          if (info->all_can_read()) {
            *attributes = result->GetAttributes();
            return GetPropertyWithCallback(receiver,
                                           result->GetCallbackObject(),
                                           name,
                                           result->holder());
          }
        }
        break;
      }
      case NORMAL:
      case FIELD:
      case CONSTANT_FUNCTION: {
        // Search ALL_CAN_READ accessors in prototype chain.
        LookupResult r;
        result->holder()->LookupRealNamedPropertyInPrototypes(name, &r);
        if (r.IsValid()) {
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
        LookupResult r;
        result->holder()->LookupRealNamedProperty(name, &r);
        if (r.IsValid()) {
          return GetPropertyWithFailedAccessCheck(receiver,
                                                  &r,
                                                  name,
                                                  attributes);
        }
      }
      default: {
        break;
      }
    }
  }

  // No accessible property found.
  *attributes = ABSENT;
  Top::ReportFailedAccessCheck(this, v8::ACCESS_GET);
  return Heap::undefined_value();
}


PropertyAttributes JSObject::GetPropertyAttributeWithFailedAccessCheck(
    Object* receiver,
    LookupResult* result,
    String* name,
    bool continue_search) {
  if (result->IsValid()) {
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
        LookupResult r;
        result->holder()->LookupRealNamedPropertyInPrototypes(name, &r);
        if (r.IsValid()) {
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
        LookupResult r;
        if (continue_search) {
          result->holder()->LookupRealNamedProperty(name, &r);
        } else {
          result->holder()->LocalLookupRealNamedProperty(name, &r);
        }
        if (r.IsValid()) {
          return GetPropertyAttributeWithFailedAccessCheck(receiver,
                                                           &r,
                                                           name,
                                                           continue_search);
        }
        break;
      }

      default: {
        break;
      }
    }
  }

  Top::ReportFailedAccessCheck(this, v8::ACCESS_HAS);
  return ABSENT;
}


Object* JSObject::GetLazyProperty(Object* receiver,
                                  LookupResult* result,
                                  String* name,
                                  PropertyAttributes* attributes) {
  HandleScope scope;
  Handle<Object> this_handle(this);
  Handle<Object> receiver_handle(receiver);
  Handle<String> name_handle(name);
  bool pending_exception;
  LoadLazy(Handle<JSObject>(JSObject::cast(result->GetLazyValue())),
           &pending_exception);
  if (pending_exception) return Failure::Exception();
  return this_handle->GetPropertyWithReceiver(*receiver_handle,
                                              *name_handle,
                                              attributes);
}


Object* JSObject::SetLazyProperty(LookupResult* result,
                                  String* name,
                                  Object* value,
                                  PropertyAttributes attributes) {
  ASSERT(!IsJSGlobalProxy());
  HandleScope scope;
  Handle<JSObject> this_handle(this);
  Handle<String> name_handle(name);
  Handle<Object> value_handle(value);
  bool pending_exception;
  LoadLazy(Handle<JSObject>(JSObject::cast(result->GetLazyValue())),
           &pending_exception);
  if (pending_exception) return Failure::Exception();
  return this_handle->SetProperty(*name_handle, *value_handle, attributes);
}


Object* JSObject::DeleteLazyProperty(LookupResult* result,
                                     String* name,
                                     DeleteMode mode) {
  HandleScope scope;
  Handle<JSObject> this_handle(this);
  Handle<String> name_handle(name);
  bool pending_exception;
  LoadLazy(Handle<JSObject>(JSObject::cast(result->GetLazyValue())),
           &pending_exception);
  if (pending_exception) return Failure::Exception();
  return this_handle->DeleteProperty(*name_handle, mode);
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


Object* JSObject::SetNormalizedProperty(String* name,
                                        Object* value,
                                        PropertyDetails details) {
  ASSERT(!HasFastProperties());
  int entry = property_dictionary()->FindEntry(name);
  if (entry == StringDictionary::kNotFound) {
    Object* store_value = value;
    if (IsGlobalObject()) {
      store_value = Heap::AllocateJSGlobalPropertyCell(value);
      if (store_value->IsFailure()) return store_value;
    }
    Object* dict = property_dictionary()->Add(name, store_value, details);
    if (dict->IsFailure()) return dict;
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


Object* JSObject::DeleteNormalizedProperty(String* name, DeleteMode mode) {
  ASSERT(!HasFastProperties());
  StringDictionary* dictionary = property_dictionary();
  int entry = dictionary->FindEntry(name);
  if (entry != StringDictionary::kNotFound) {
    // If we have a global object set the cell to the hole.
    if (IsGlobalObject()) {
      PropertyDetails details = dictionary->DetailsAt(entry);
      if (details.IsDontDelete()) {
        if (mode != FORCE_DELETION) return Heap::false_value();
        // When forced to delete global properties, we have to make a
        // map change to invalidate any ICs that think they can load
        // from the DontDelete cell without checking if it contains
        // the hole value.
        Object* new_map = map()->CopyDropDescriptors();
        if (new_map->IsFailure()) return new_map;
        set_map(Map::cast(new_map));
      }
      JSGlobalPropertyCell* cell =
          JSGlobalPropertyCell::cast(dictionary->ValueAt(entry));
      cell->set_value(Heap::the_hole_value());
      dictionary->DetailsAtPut(entry, details.AsDeleted());
    } else {
      return dictionary->DeleteProperty(entry, mode);
    }
  }
  return Heap::true_value();
}


Object* Object::GetProperty(Object* receiver,
                            LookupResult* result,
                            String* name,
                            PropertyAttributes* attributes) {
  // Make sure that the top context does not change when doing
  // callbacks or interceptor calls.
  AssertNoContextChange ncc;

  // Traverse the prototype chain from the current object (this) to
  // the holder and check for access rights. This avoid traversing the
  // objects more than once in case of interceptors, because the
  // holder will always be the interceptor holder and the search may
  // only continue with a current object just after the interceptor
  // holder in the prototype chain.
  Object* last = result->IsValid() ? result->holder() : Heap::null_value();
  for (Object* current = this; true; current = current->GetPrototype()) {
    if (current->IsAccessCheckNeeded()) {
      // Check if we're allowed to read from the current object. Note
      // that even though we may not actually end up loading the named
      // property from the current object, we still check that we have
      // access to it.
      JSObject* checked = JSObject::cast(current);
      if (!Top::MayNamedAccess(checked, name, v8::ACCESS_GET)) {
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

  if (!result->IsProperty()) {
    *attributes = ABSENT;
    return Heap::undefined_value();
  }
  *attributes = result->GetAttributes();
  if (!result->IsLoaded()) {
    return JSObject::cast(this)->GetLazyProperty(receiver,
                                                 result,
                                                 name,
                                                 attributes);
  }
  Object* value;
  JSObject* holder = result->holder();
  switch (result->type()) {
    case NORMAL:
      value = holder->GetNormalizedProperty(result);
      ASSERT(!value->IsTheHole() || result->IsReadOnly());
      return value->IsTheHole() ? Heap::undefined_value() : value;
    case FIELD:
      value = holder->FastPropertyAt(result->GetFieldIndex());
      ASSERT(!value->IsTheHole() || result->IsReadOnly());
      return value->IsTheHole() ? Heap::undefined_value() : value;
    case CONSTANT_FUNCTION:
      return result->GetConstantFunction();
    case CALLBACKS:
      return GetPropertyWithCallback(receiver,
                                     result->GetCallbackObject(),
                                     name,
                                     holder);
    case INTERCEPTOR: {
      JSObject* recvr = JSObject::cast(receiver);
      return holder->GetPropertyWithInterceptor(recvr, name, attributes);
    }
    default:
      UNREACHABLE();
      return NULL;
  }
}


Object* Object::GetElementWithReceiver(Object* receiver, uint32_t index) {
  // Non-JS objects do not have integer indexed properties.
  if (!IsJSObject()) return Heap::undefined_value();
  return JSObject::cast(this)->GetElementWithReceiver(JSObject::cast(receiver),
                                                      index);
}


Object* Object::GetPrototype() {
  // The object is either a number, a string, a boolean, or a real JS object.
  if (IsJSObject()) return JSObject::cast(this)->map()->prototype();
  Context* context = Top::context()->global_context();

  if (IsNumber()) return context->number_function()->instance_prototype();
  if (IsString()) return context->string_function()->instance_prototype();
  if (IsBoolean()) {
    return context->boolean_function()->instance_prototype();
  } else {
    return Heap::null_value();
  }
}


void Object::ShortPrint() {
  HeapStringAllocator allocator;
  StringStream accumulator(&allocator);
  ShortPrint(&accumulator);
  accumulator.OutputToStdOut();
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


void Smi::SmiPrint() {
  PrintF("%d", value());
}


void Smi::SmiPrint(StringStream* accumulator) {
  accumulator->Add("%d", value());
}


void Failure::FailurePrint(StringStream* accumulator) {
  accumulator->Add("Failure(%d)", value());
}


void Failure::FailurePrint() {
  PrintF("Failure(%d)", value());
}


Failure* Failure::RetryAfterGC(int requested_bytes, AllocationSpace space) {
  ASSERT((space & ~kSpaceTagMask) == 0);
  // TODO(X64): Stop using Smi validation for non-smi checks, even if they
  // happen to be identical at the moment.

  int requested = requested_bytes >> kObjectAlignmentBits;
  int value = (requested << kSpaceTagSize) | space;
  // We can't very well allocate a heap number in this situation, and if the
  // requested memory is so large it seems reasonable to say that this is an
  // out of memory situation.  This fixes a crash in
  // js1_5/Regress/regress-303213.js.
  if (value >> kSpaceTagSize != requested ||
      !Smi::IsValid(value) ||
      value != ((value << kFailureTypeTagSize) >> kFailureTypeTagSize) ||
      !Smi::IsValid(value << kFailureTypeTagSize)) {
    Top::context()->mark_out_of_memory();
    return Failure::OutOfMemoryException();
  }
  return Construct(RETRY_AFTER_GC, value);
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


Object* String::TryFlatten() {
#ifdef DEBUG
  // Do not attempt to flatten in debug mode when allocation is not
  // allowed.  This is to avoid an assertion failure when allocating.
  // Flattening strings is the only case where we always allow
  // allocation because no GC is performed if the allocation fails.
  if (!Heap::IsAllocationAllowed()) return this;
#endif

  switch (StringShape(this).representation_tag()) {
    case kSlicedStringTag: {
      SlicedString* ss = SlicedString::cast(this);
      // The SlicedString constructor should ensure that there are no
      // SlicedStrings that are constructed directly on top of other
      // SlicedStrings.
      String* buf = ss->buffer();
      ASSERT(!buf->IsSlicedString());
      Object* ok = buf->TryFlatten();
      if (ok->IsFailure()) return ok;
      // Under certain circumstances (TryFlattenIfNotFlat fails in
      // String::Slice) we can have a cons string under a slice.
      // In this case we need to get the flat string out of the cons!
      if (StringShape(String::cast(ok)).IsCons()) {
        ss->set_buffer(ConsString::cast(ok)->first());
      }
      return this;
    }
    case kConsStringTag: {
      ConsString* cs = ConsString::cast(this);
      if (cs->second()->length() == 0) {
        return this;
      }
      // There's little point in putting the flat string in new space if the
      // cons string is in old space.  It can never get GCed until there is
      // an old space GC.
      PretenureFlag tenure = Heap::InNewSpace(this) ? NOT_TENURED : TENURED;
      int len = length();
      Object* object;
      String* result;
      if (IsAsciiRepresentation()) {
        object = Heap::AllocateRawAsciiString(len, tenure);
        if (object->IsFailure()) return object;
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
        object = Heap::AllocateRawTwoByteString(len, tenure);
        if (object->IsFailure()) return object;
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
      cs->set_second(Heap::empty_string());
      return this;
    }
    default:
      return this;
  }
}


bool String::MakeExternal(v8::String::ExternalStringResource* resource) {
#ifdef DEBUG
  {  // NOLINT (presubmit.py gets confused about if and braces)
    // Assert that the resource and the string are equivalent.
    ASSERT(static_cast<size_t>(this->length()) == resource->length());
    SmartPointer<uc16> smart_chars = this->ToWideCString();
    ASSERT(memcmp(*smart_chars,
                  resource->data(),
                  resource->length() * sizeof(**smart_chars)) == 0);
  }
#endif  // DEBUG

  int size = this->Size();  // Byte size of the original string.
  if (size < ExternalString::kSize) {
    // The string is too small to fit an external String in its place. This can
    // only happen for zero length strings.
    return false;
  }
  ASSERT(size >= ExternalString::kSize);
  bool is_symbol = this->IsSymbol();
  int length = this->length();

  // Morph the object to an external string by adjusting the map and
  // reinitializing the fields.
  this->set_map(ExternalTwoByteString::StringMap(length));
  ExternalTwoByteString* self = ExternalTwoByteString::cast(this);
  self->set_length(length);
  self->set_resource(resource);
  // Additionally make the object into an external symbol if the original string
  // was a symbol to start with.
  if (is_symbol) {
    self->Hash();  // Force regeneration of the hash value.
    // Now morph this external string into a external symbol.
    self->set_map(ExternalTwoByteString::SymbolMap(length));
  }

  // Fill the remainder of the string with dead wood.
  int new_size = this->Size();  // Byte size of the external String object.
  Heap::CreateFillerObjectAt(this->address() + new_size, size - new_size);
  return true;
}


bool String::MakeExternal(v8::String::ExternalAsciiStringResource* resource) {
#ifdef DEBUG
  {  // NOLINT (presubmit.py gets confused about if and braces)
    // Assert that the resource and the string are equivalent.
    ASSERT(static_cast<size_t>(this->length()) == resource->length());
    SmartPointer<char> smart_chars = this->ToCString();
    ASSERT(memcmp(*smart_chars,
                  resource->data(),
                  resource->length()*sizeof(**smart_chars)) == 0);
  }
#endif  // DEBUG

  int size = this->Size();  // Byte size of the original string.
  if (size < ExternalString::kSize) {
    // The string is too small to fit an external String in its place. This can
    // only happen for zero length strings.
    return false;
  }
  ASSERT(size >= ExternalString::kSize);
  bool is_symbol = this->IsSymbol();
  int length = this->length();

  // Morph the object to an external string by adjusting the map and
  // reinitializing the fields.
  this->set_map(ExternalAsciiString::StringMap(length));
  ExternalAsciiString* self = ExternalAsciiString::cast(this);
  self->set_length(length);
  self->set_resource(resource);
  // Additionally make the object into an external symbol if the original string
  // was a symbol to start with.
  if (is_symbol) {
    self->Hash();  // Force regeneration of the hash value.
    // Now morph this external string into a external symbol.
    self->set_map(ExternalAsciiString::SymbolMap(length));
  }

  // Fill the remainder of the string with dead wood.
  int new_size = this->Size();  // Byte size of the external String object.
  Heap::CreateFillerObjectAt(this->address() + new_size, size - new_size);
  return true;
}


void String::StringShortPrint(StringStream* accumulator) {
  int len = length();
  if (len > kMaxMediumStringSize) {
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
      accumulator->Add("<JS array[%u]>", static_cast<uint32_t>(length));
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
      Object* constructor = map()->constructor();
      bool printed = false;
      if (constructor->IsHeapObject() &&
          !Heap::Contains(HeapObject::cast(constructor))) {
        accumulator->Add("!!!INVALID CONSTRUCTOR!!!");
      } else {
        bool global_object = IsJSGlobalProxy();
        if (constructor->IsJSFunction()) {
          if (!Heap::Contains(JSFunction::cast(constructor)->shared())) {
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
                accumulator->Put('>');
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


void HeapObject::HeapObjectShortPrint(StringStream* accumulator) {
  // if (!Heap::InNewSpace(this)) PrintF("*", this);
  if (!Heap::Contains(this)) {
    accumulator->Add("!!!INVALID POINTER!!!");
    return;
  }
  if (!Heap::Contains(map())) {
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
      accumulator->Add("<Map>");
      break;
    case FIXED_ARRAY_TYPE:
      accumulator->Add("<FixedArray[%u]>", FixedArray::cast(this)->length());
      break;
    case BYTE_ARRAY_TYPE:
      accumulator->Add("<ByteArray[%u]>", ByteArray::cast(this)->length());
      break;
    case PIXEL_ARRAY_TYPE:
      accumulator->Add("<PixelArray[%u]>", PixelArray::cast(this)->length());
      break;
    case SHARED_FUNCTION_INFO_TYPE:
      accumulator->Add("<SharedFunctionInfo>");
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
    case PROXY_TYPE:
      accumulator->Add("<Proxy>");
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


int HeapObject::SlowSizeFromMap(Map* map) {
  // Avoid calling functions such as FixedArray::cast during GC, which
  // read map pointer of this object again.
  InstanceType instance_type = map->instance_type();
  uint32_t type = static_cast<uint32_t>(instance_type);

  if (instance_type < FIRST_NONSTRING_TYPE
      && (StringShape(instance_type).IsSequential())) {
    if ((type & kStringEncodingMask) == kAsciiStringTag) {
      SeqAsciiString* seq_ascii_this = reinterpret_cast<SeqAsciiString*>(this);
      return seq_ascii_this->SeqAsciiStringSize(instance_type);
    } else {
      SeqTwoByteString* self = reinterpret_cast<SeqTwoByteString*>(this);
      return self->SeqTwoByteStringSize(instance_type);
    }
  }

  switch (instance_type) {
    case FIXED_ARRAY_TYPE:
      return reinterpret_cast<FixedArray*>(this)->FixedArraySize();
    case BYTE_ARRAY_TYPE:
      return reinterpret_cast<ByteArray*>(this)->ByteArraySize();
    case CODE_TYPE:
      return reinterpret_cast<Code*>(this)->CodeSize();
    case MAP_TYPE:
      return Map::kSize;
    default:
      return map->instance_size();
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
        reinterpret_cast<ConsString*>(this)->ConsStringIterateBody(v);
        break;
      case kSlicedStringTag:
        reinterpret_cast<SlicedString*>(this)->SlicedStringIterateBody(v);
        break;
    }
    return;
  }

  switch (type) {
    case FIXED_ARRAY_TYPE:
      reinterpret_cast<FixedArray*>(this)->FixedArrayIterateBody(v);
      break;
    case JS_OBJECT_TYPE:
    case JS_CONTEXT_EXTENSION_OBJECT_TYPE:
    case JS_VALUE_TYPE:
    case JS_ARRAY_TYPE:
    case JS_REGEXP_TYPE:
    case JS_FUNCTION_TYPE:
    case JS_GLOBAL_PROXY_TYPE:
    case JS_GLOBAL_OBJECT_TYPE:
    case JS_BUILTINS_OBJECT_TYPE:
      reinterpret_cast<JSObject*>(this)->JSObjectIterateBody(object_size, v);
      break;
    case ODDBALL_TYPE:
      reinterpret_cast<Oddball*>(this)->OddballIterateBody(v);
      break;
    case PROXY_TYPE:
      reinterpret_cast<Proxy*>(this)->ProxyIterateBody(v);
      break;
    case MAP_TYPE:
      reinterpret_cast<Map*>(this)->MapIterateBody(v);
      break;
    case CODE_TYPE:
      reinterpret_cast<Code*>(this)->CodeIterateBody(v);
      break;
    case JS_GLOBAL_PROPERTY_CELL_TYPE:
      reinterpret_cast<JSGlobalPropertyCell*>(this)
          ->JSGlobalPropertyCellIterateBody(v);
      break;
    case HEAP_NUMBER_TYPE:
    case FILLER_TYPE:
    case BYTE_ARRAY_TYPE:
    case PIXEL_ARRAY_TYPE:
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
      IterateStructBody(object_size, v);
      break;
    default:
      PrintF("Unknown type: %d\n", type);
      UNREACHABLE();
  }
}


void HeapObject::IterateStructBody(int object_size, ObjectVisitor* v) {
  IteratePointers(v, HeapObject::kHeaderSize, object_size);
}


Object* HeapNumber::HeapNumberToBoolean() {
  // NaN, +0, and -0 should return the false object
  switch (fpclassify(value())) {
    case FP_NAN:  // fall through
    case FP_ZERO: return Heap::false_value();
    default: return Heap::true_value();
  }
}


void HeapNumber::HeapNumberPrint() {
  PrintF("%.16g", Number());
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


String* JSObject::class_name() {
  if (IsJSFunction()) return Heap::function_class_symbol();
  if (map()->constructor()->IsJSFunction()) {
    JSFunction* constructor = JSFunction::cast(map()->constructor());
    return String::cast(constructor->shared()->instance_class_name());
  }
  // If the constructor is not present, return "Object".
  return Heap::Object_symbol();
}


void JSObject::JSObjectIterateBody(int object_size, ObjectVisitor* v) {
  // Iterate over all fields in the body. Assumes all are Object*.
  IteratePointers(v, kPropertiesOffset, object_size);
}


Object* JSObject::AddFastPropertyUsingMap(Map* new_map,
                                          String* name,
                                          Object* value) {
  int index = new_map->PropertyIndexFor(name);
  if (map()->unused_property_fields() == 0) {
    ASSERT(map()->unused_property_fields() == 0);
    int new_unused = new_map->unused_property_fields();
    Object* values =
        properties()->CopySize(properties()->length() + new_unused + 1);
    if (values->IsFailure()) return values;
    set_properties(FixedArray::cast(values));
  }
  set_map(new_map);
  return FastPropertyAtPut(index, value);
}


Object* JSObject::AddFastProperty(String* name,
                                  Object* value,
                                  PropertyAttributes attributes) {
  // Normalize the object if the name is an actual string (not the
  // hidden symbols) and is not a real identifier.
  StringInputBuffer buffer(name);
  if (!Scanner::IsIdentifier(&buffer) && name != Heap::hidden_symbol()) {
    Object* obj = NormalizeProperties(CLEAR_INOBJECT_PROPERTIES, 0);
    if (obj->IsFailure()) return obj;
    return AddSlowProperty(name, value, attributes);
  }

  DescriptorArray* old_descriptors = map()->instance_descriptors();
  // Compute the new index for new field.
  int index = map()->NextFreePropertyIndex();

  // Allocate new instance descriptors with (name, index) added
  FieldDescriptor new_field(name, index, attributes);
  Object* new_descriptors =
      old_descriptors->CopyInsert(&new_field, REMOVE_TRANSITIONS);
  if (new_descriptors->IsFailure()) return new_descriptors;

  // Only allow map transition if the object's map is NOT equal to the
  // global object_function's map and there is not a transition for name.
  bool allow_map_transition =
        !old_descriptors->Contains(name) &&
        (Top::context()->global_context()->object_function()->map() != map());

  ASSERT(index < map()->inobject_properties() ||
         (index - map()->inobject_properties()) < properties()->length() ||
         map()->unused_property_fields() == 0);
  // Allocate a new map for the object.
  Object* r = map()->CopyDropDescriptors();
  if (r->IsFailure()) return r;
  Map* new_map = Map::cast(r);
  if (allow_map_transition) {
    // Allocate new instance descriptors for the old map with map transition.
    MapTransitionDescriptor d(name, Map::cast(new_map), attributes);
    Object* r = old_descriptors->CopyInsert(&d, KEEP_TRANSITIONS);
    if (r->IsFailure()) return r;
    old_descriptors = DescriptorArray::cast(r);
  }

  if (map()->unused_property_fields() == 0) {
    if (properties()->length() > kMaxFastProperties) {
      Object* obj = NormalizeProperties(CLEAR_INOBJECT_PROPERTIES, 0);
      if (obj->IsFailure()) return obj;
      return AddSlowProperty(name, value, attributes);
    }
    // Make room for the new value
    Object* values =
        properties()->CopySize(properties()->length() + kFieldsAdded);
    if (values->IsFailure()) return values;
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


Object* JSObject::AddConstantFunctionProperty(String* name,
                                              JSFunction* function,
                                              PropertyAttributes attributes) {
  // Allocate new instance descriptors with (name, function) added
  ConstantFunctionDescriptor d(name, function, attributes);
  Object* new_descriptors =
      map()->instance_descriptors()->CopyInsert(&d, REMOVE_TRANSITIONS);
  if (new_descriptors->IsFailure()) return new_descriptors;

  // Allocate a new map for the object.
  Object* new_map = map()->CopyDropDescriptors();
  if (new_map->IsFailure()) return new_map;

  DescriptorArray* descriptors = DescriptorArray::cast(new_descriptors);
  Map::cast(new_map)->set_instance_descriptors(descriptors);
  Map* old_map = map();
  set_map(Map::cast(new_map));

  // If the old map is the global object map (from new Object()),
  // then transitions are not added to it, so we are done.
  if (old_map == Top::context()->global_context()->object_function()->map()) {
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
  ConstTransitionDescriptor mark(name);
  new_descriptors =
      old_map->instance_descriptors()->CopyInsert(&mark, KEEP_TRANSITIONS);
  if (new_descriptors->IsFailure()) {
    return function;  // We have accomplished the main goal, so return success.
  }
  old_map->set_instance_descriptors(DescriptorArray::cast(new_descriptors));

  return function;
}


// Add property in slow mode
Object* JSObject::AddSlowProperty(String* name,
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
    store_value = Heap::AllocateJSGlobalPropertyCell(value);
    if (store_value->IsFailure()) return store_value;
    JSGlobalPropertyCell::cast(store_value)->set_value(value);
  }
  PropertyDetails details = PropertyDetails(attributes, NORMAL);
  Object* result = dict->Add(name, store_value, details);
  if (result->IsFailure()) return result;
  if (dict != result) set_properties(StringDictionary::cast(result));
  return value;
}


Object* JSObject::AddProperty(String* name,
                              Object* value,
                              PropertyAttributes attributes) {
  ASSERT(!IsJSGlobalProxy());
  if (HasFastProperties()) {
    // Ensure the descriptor array does not get too big.
    if (map()->instance_descriptors()->number_of_descriptors() <
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
      Object* obj = NormalizeProperties(CLEAR_INOBJECT_PROPERTIES, 0);
      if (obj->IsFailure()) return obj;
    }
  }
  return AddSlowProperty(name, value, attributes);
}


Object* JSObject::SetPropertyPostInterceptor(String* name,
                                             Object* value,
                                             PropertyAttributes attributes) {
  // Check local property, ignore interceptor.
  LookupResult result;
  LocalLookupRealNamedProperty(name, &result);
  if (result.IsValid()) return SetProperty(&result, name, value, attributes);
  // Add real property.
  return AddProperty(name, value, attributes);
}


Object* JSObject::ReplaceSlowProperty(String* name,
                                       Object* value,
                                       PropertyAttributes attributes) {
  StringDictionary* dictionary = property_dictionary();
  int old_index = dictionary->FindEntry(name);
  int new_enumeration_index = 0;  // 0 means "Use the next available index."
  if (old_index != -1) {
    // All calls to ReplaceSlowProperty have had all transitions removed.
    ASSERT(!dictionary->DetailsAt(old_index).IsTransition());
    new_enumeration_index = dictionary->DetailsAt(old_index).index();
  }

  PropertyDetails new_details(attributes, NORMAL, new_enumeration_index);
  return SetNormalizedProperty(name, value, new_details);
}

Object* JSObject::ConvertDescriptorToFieldAndMapTransition(
    String* name,
    Object* new_value,
    PropertyAttributes attributes) {
  Map* old_map = map();
  Object* result = ConvertDescriptorToField(name, new_value, attributes);
  if (result->IsFailure()) return result;
  // If we get to this point we have succeeded - do not return failure
  // after this point.  Later stuff is optional.
  if (!HasFastProperties()) {
    return result;
  }
  // Do not add transitions to the map of "new Object()".
  if (map() == Top::context()->global_context()->object_function()->map()) {
    return result;
  }

  MapTransitionDescriptor transition(name,
                                     map(),
                                     attributes);
  Object* new_descriptors =
      old_map->instance_descriptors()->
          CopyInsert(&transition, KEEP_TRANSITIONS);
  if (new_descriptors->IsFailure()) return result;  // Yes, return _result_.
  old_map->set_instance_descriptors(DescriptorArray::cast(new_descriptors));
  return result;
}


Object* JSObject::ConvertDescriptorToField(String* name,
                                           Object* new_value,
                                           PropertyAttributes attributes) {
  if (map()->unused_property_fields() == 0 &&
      properties()->length() > kMaxFastProperties) {
    Object* obj = NormalizeProperties(CLEAR_INOBJECT_PROPERTIES, 0);
    if (obj->IsFailure()) return obj;
    return ReplaceSlowProperty(name, new_value, attributes);
  }

  int index = map()->NextFreePropertyIndex();
  FieldDescriptor new_field(name, index, attributes);
  // Make a new DescriptorArray replacing an entry with FieldDescriptor.
  Object* descriptors_unchecked = map()->instance_descriptors()->
      CopyInsert(&new_field, REMOVE_TRANSITIONS);
  if (descriptors_unchecked->IsFailure()) return descriptors_unchecked;
  DescriptorArray* new_descriptors =
      DescriptorArray::cast(descriptors_unchecked);

  // Make a new map for the object.
  Object* new_map_unchecked = map()->CopyDropDescriptors();
  if (new_map_unchecked->IsFailure()) return new_map_unchecked;
  Map* new_map = Map::cast(new_map_unchecked);
  new_map->set_instance_descriptors(new_descriptors);

  // Make new properties array if necessary.
  FixedArray* new_properties = 0;  // Will always be NULL or a valid pointer.
  int new_unused_property_fields = map()->unused_property_fields() - 1;
  if (map()->unused_property_fields() == 0) {
     new_unused_property_fields = kFieldsAdded - 1;
     Object* new_properties_unchecked =
        properties()->CopySize(properties()->length() + kFieldsAdded);
    if (new_properties_unchecked->IsFailure()) return new_properties_unchecked;
    new_properties = FixedArray::cast(new_properties_unchecked);
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



Object* JSObject::SetPropertyWithInterceptor(String* name,
                                             Object* value,
                                             PropertyAttributes attributes) {
  HandleScope scope;
  Handle<JSObject> this_handle(this);
  Handle<String> name_handle(name);
  Handle<Object> value_handle(value);
  Handle<InterceptorInfo> interceptor(GetNamedInterceptor());
  if (!interceptor->setter()->IsUndefined()) {
    Handle<Object> data_handle(interceptor->data());
    LOG(ApiNamedPropertyAccess("interceptor-named-set", this, name));
    v8::AccessorInfo info(v8::Utils::ToLocal(this_handle),
                          v8::Utils::ToLocal(data_handle),
                          v8::Utils::ToLocal(this_handle));
    v8::NamedPropertySetter setter =
        v8::ToCData<v8::NamedPropertySetter>(interceptor->setter());
    v8::Handle<v8::Value> result;
    {
      // Leaving JavaScript.
      VMState state(EXTERNAL);
      Handle<Object> value_unhole(value->IsTheHole() ?
                                  Heap::undefined_value() :
                                  value);
      result = setter(v8::Utils::ToLocal(name_handle),
                      v8::Utils::ToLocal(value_unhole),
                      info);
    }
    RETURN_IF_SCHEDULED_EXCEPTION();
    if (!result.IsEmpty()) return *value_handle;
  }
  Object* raw_result = this_handle->SetPropertyPostInterceptor(*name_handle,
                                                               *value_handle,
                                                               attributes);
  RETURN_IF_SCHEDULED_EXCEPTION();
  return raw_result;
}


Object* JSObject::SetProperty(String* name,
                              Object* value,
                              PropertyAttributes attributes) {
  LookupResult result;
  LocalLookup(name, &result);
  return SetProperty(&result, name, value, attributes);
}


Object* JSObject::SetPropertyWithCallback(Object* structure,
                                          String* name,
                                          Object* value,
                                          JSObject* holder) {
  HandleScope scope;

  // We should never get here to initialize a const with the hole
  // value since a const declaration would conflict with the setter.
  ASSERT(!value->IsTheHole());
  Handle<Object> value_handle(value);

  // To accommodate both the old and the new api we switch on the
  // data structure used to store the callbacks.  Eventually proxy
  // callbacks should be phased out.
  if (structure->IsProxy()) {
    AccessorDescriptor* callback =
        reinterpret_cast<AccessorDescriptor*>(Proxy::cast(structure)->proxy());
    Object* obj = (callback->setter)(this,  value, callback->data);
    RETURN_IF_SCHEDULED_EXCEPTION();
    if (obj->IsFailure()) return obj;
    return *value_handle;
  }

  if (structure->IsAccessorInfo()) {
    // api style callbacks
    AccessorInfo* data = AccessorInfo::cast(structure);
    Object* call_obj = data->setter();
    v8::AccessorSetter call_fun = v8::ToCData<v8::AccessorSetter>(call_obj);
    if (call_fun == NULL) return value;
    Handle<JSObject> self(this);
    Handle<JSObject> holder_handle(JSObject::cast(holder));
    Handle<String> key(name);
    Handle<Object> fun_data(data->data());
    LOG(ApiNamedPropertyAccess("store", this, name));
    v8::AccessorInfo info(v8::Utils::ToLocal(self),
                          v8::Utils::ToLocal(fun_data),
                          v8::Utils::ToLocal(holder_handle));
    {
      // Leaving JavaScript.
      VMState state(EXTERNAL);
      call_fun(v8::Utils::ToLocal(key),
               v8::Utils::ToLocal(value_handle),
               info);
    }
    RETURN_IF_SCHEDULED_EXCEPTION();
    return *value_handle;
  }

  if (structure->IsFixedArray()) {
    Object* setter = FixedArray::cast(structure)->get(kSetterIndex);
    if (setter->IsJSFunction()) {
     return SetPropertyWithDefinedSetter(JSFunction::cast(setter), value);
    } else {
      Handle<String> key(name);
      Handle<Object> holder_handle(holder);
      Handle<Object> args[2] = { key, holder_handle };
      return Top::Throw(*Factory::NewTypeError("no_setter_in_callback",
                                               HandleVector(args, 2)));
    }
  }

  UNREACHABLE();
  return 0;
}


Object* JSObject::SetPropertyWithDefinedSetter(JSFunction* setter,
                                               Object* value) {
  Handle<Object> value_handle(value);
  Handle<JSFunction> fun(JSFunction::cast(setter));
  Handle<JSObject> self(this);
#ifdef ENABLE_DEBUGGER_SUPPORT
  // Handle stepping into a setter if step into is active.
  if (Debug::StepInActive()) {
    Debug::HandleStepIn(fun, Handle<Object>::null(), 0, false);
  }
#endif
  bool has_pending_exception;
  Object** argv[] = { value_handle.location() };
  Execution::Call(fun, self, 1, argv, &has_pending_exception);
  // Check for pending exception and return the result.
  if (has_pending_exception) return Failure::Exception();
  return *value_handle;
}


void JSObject::LookupCallbackSetterInPrototypes(String* name,
                                                LookupResult* result) {
  for (Object* pt = GetPrototype();
       pt != Heap::null_value();
       pt = pt->GetPrototype()) {
    JSObject::cast(pt)->LocalLookupRealNamedProperty(name, result);
    if (result->IsValid()) {
      if (!result->IsTransitionType() && result->IsReadOnly()) {
        result->NotFound();
        return;
      }
      if (result->type() == CALLBACKS) {
        return;
      }
    }
  }
  result->NotFound();
}


Object* JSObject::LookupCallbackSetterInPrototypes(uint32_t index) {
  for (Object* pt = GetPrototype();
       pt != Heap::null_value();
       pt = pt->GetPrototype()) {
    if (!JSObject::cast(pt)->HasDictionaryElements()) {
        continue;
    }
    NumberDictionary* dictionary = JSObject::cast(pt)->element_dictionary();
    int entry = dictionary->FindEntry(index);
    if (entry != NumberDictionary::kNotFound) {
      Object* element = dictionary->ValueAt(entry);
      PropertyDetails details = dictionary->DetailsAt(entry);
      if (details.type() == CALLBACKS) {
        // Only accessors allowed as elements.
        return FixedArray::cast(element)->get(kSetterIndex);
      }
    }
  }
  return Heap::undefined_value();
}


void JSObject::LookupInDescriptor(String* name, LookupResult* result) {
  DescriptorArray* descriptors = map()->instance_descriptors();
  int number = DescriptorLookupCache::Lookup(descriptors, name);
  if (number == DescriptorLookupCache::kAbsent) {
    number = descriptors->Search(name);
    DescriptorLookupCache::Update(descriptors, name, number);
  }
  if (number != DescriptorArray::kNotFound) {
    result->DescriptorResult(this, descriptors->GetDetails(number), number);
  } else {
    result->NotFound();
  }
}


void JSObject::LocalLookupRealNamedProperty(String* name,
                                            LookupResult* result) {
  if (IsJSGlobalProxy()) {
    Object* proto = GetPrototype();
    if (proto->IsNull()) return result->NotFound();
    ASSERT(proto->IsJSGlobalObject());
    return JSObject::cast(proto)->LocalLookupRealNamedProperty(name, result);
  }

  if (HasFastProperties()) {
    LookupInDescriptor(name, result);
    if (result->IsValid()) {
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
        ASSERT(result->IsLoaded());
      }
      // Make sure to disallow caching for uninitialized constants
      // found in the dictionary-mode objects.
      if (value->IsTheHole()) result->DisallowCaching();
      result->DictionaryResult(this, entry);
      return;
    }
    // Slow case object skipped during lookup. Do not use inline caching.
    if (!IsGlobalObject()) result->DisallowCaching();
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
  for (Object* pt = GetPrototype();
       pt != Heap::null_value();
       pt = JSObject::cast(pt)->GetPrototype()) {
    JSObject::cast(pt)->LocalLookupRealNamedProperty(name, result);
    if (result->IsValid()) {
      switch (result->type()) {
        case NORMAL:
        case FIELD:
        case CONSTANT_FUNCTION:
        case CALLBACKS:
          return;
        default: break;
      }
    }
  }
  result->NotFound();
}


// We only need to deal with CALLBACKS and INTERCEPTORS
Object* JSObject::SetPropertyWithFailedAccessCheck(LookupResult* result,
                                                   String* name,
                                                   Object* value) {
  if (!result->IsProperty()) {
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
                                             result->holder());
            }
          }
          break;
        }
        case INTERCEPTOR: {
          // Try lookup real named properties. Note that only property can be
          // set is callbacks marked as ALL_CAN_WRITE on the prototype chain.
          LookupResult r;
          LookupRealNamedProperty(name, &r);
          if (r.IsProperty()) {
            return SetPropertyWithFailedAccessCheck(&r, name, value);
          }
          break;
        }
        default: {
          break;
        }
      }
    }
  }

  Top::ReportFailedAccessCheck(this, v8::ACCESS_SET);
  return value;
}


Object* JSObject::SetProperty(LookupResult* result,
                              String* name,
                              Object* value,
                              PropertyAttributes attributes) {
  // Make sure that the top context does not change when doing callbacks or
  // interceptor calls.
  AssertNoContextChange ncc;

  // Check access rights if needed.
  if (IsAccessCheckNeeded()
      && !Top::MayNamedAccess(this, name, v8::ACCESS_SET)) {
    return SetPropertyWithFailedAccessCheck(result, name, value);
  }

  if (IsJSGlobalProxy()) {
    Object* proto = GetPrototype();
    if (proto->IsNull()) return value;
    ASSERT(proto->IsJSGlobalObject());
    return JSObject::cast(proto)->SetProperty(result, name, value, attributes);
  }

  if (!result->IsProperty() && !IsJSContextExtensionObject()) {
    // We could not find a local property so let's check whether there is an
    // accessor that wants to handle the property.
    LookupResult accessor_result;
    LookupCallbackSetterInPrototypes(name, &accessor_result);
    if (accessor_result.IsValid()) {
      return SetPropertyWithCallback(accessor_result.GetCallbackObject(),
                                     name,
                                     value,
                                     accessor_result.holder());
    }
  }
  if (result->IsNotFound()) {
    return AddProperty(name, value, attributes);
  }
  if (!result->IsLoaded()) {
    return SetLazyProperty(result, name, value, attributes);
  }
  if (result->IsReadOnly() && result->IsProperty()) return value;
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
                                     result->holder());
    case INTERCEPTOR:
      return SetPropertyWithInterceptor(name, value, attributes);
    case CONSTANT_TRANSITION:
      // Replace with a MAP_TRANSITION to a new map with a FIELD, even
      // if the value is a function.
      return ConvertDescriptorToFieldAndMapTransition(name, value, attributes);
    case NULL_DESCRIPTOR:
      return ConvertDescriptorToFieldAndMapTransition(name, value, attributes);
    default:
      UNREACHABLE();
  }
  UNREACHABLE();
  return value;
}


// Set a real local property, even if it is READ_ONLY.  If the property is not
// present, add it with attributes NONE.  This code is an exact clone of
// SetProperty, with the check for IsReadOnly and the check for a
// callback setter removed.  The two lines looking up the LookupResult
// result are also added.  If one of the functions is changed, the other
// should be.
Object* JSObject::IgnoreAttributesAndSetLocalProperty(
    String* name,
    Object* value,
    PropertyAttributes attributes) {
  // Make sure that the top context does not change when doing callbacks or
  // interceptor calls.
  AssertNoContextChange ncc;
  // ADDED TO CLONE
  LookupResult result_struct;
  LocalLookup(name, &result_struct);
  LookupResult* result = &result_struct;
  // END ADDED TO CLONE
  // Check access rights if needed.
  if (IsAccessCheckNeeded()
    && !Top::MayNamedAccess(this, name, v8::ACCESS_SET)) {
    return SetPropertyWithFailedAccessCheck(result, name, value);
  }

  if (IsJSGlobalProxy()) {
    Object* proto = GetPrototype();
    if (proto->IsNull()) return value;
    ASSERT(proto->IsJSGlobalObject());
    return JSObject::cast(proto)->IgnoreAttributesAndSetLocalProperty(
        name,
        value,
        attributes);
  }

  // Check for accessor in prototype chain removed here in clone.
  if (result->IsNotFound()) {
    return AddProperty(name, value, attributes);
  }
  if (!result->IsLoaded()) {
    return SetLazyProperty(result, name, value, attributes);
  }
  // Check of IsReadOnly removed from here in clone.
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
    case INTERCEPTOR:
      // Override callback in clone
      return ConvertDescriptorToField(name, value, attributes);
    case CONSTANT_TRANSITION:
      // Replace with a MAP_TRANSITION to a new map with a FIELD, even
      // if the value is a function.
      return ConvertDescriptorToFieldAndMapTransition(name, value, attributes);
    case NULL_DESCRIPTOR:
      return ConvertDescriptorToFieldAndMapTransition(name, value, attributes);
    default:
      UNREACHABLE();
  }
  UNREACHABLE();
  return value;
}


PropertyAttributes JSObject::GetPropertyAttributePostInterceptor(
      JSObject* receiver,
      String* name,
      bool continue_search) {
  // Check local property, ignore interceptor.
  LookupResult result;
  LocalLookupRealNamedProperty(name, &result);
  if (result.IsProperty()) return result.GetAttributes();

  if (continue_search) {
    // Continue searching via the prototype chain.
    Object* pt = GetPrototype();
    if (pt != Heap::null_value()) {
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
  // Make sure that the top context does not change when doing
  // callbacks or interceptor calls.
  AssertNoContextChange ncc;

  HandleScope scope;
  Handle<InterceptorInfo> interceptor(GetNamedInterceptor());
  Handle<JSObject> receiver_handle(receiver);
  Handle<JSObject> holder_handle(this);
  Handle<String> name_handle(name);
  Handle<Object> data_handle(interceptor->data());
  v8::AccessorInfo info(v8::Utils::ToLocal(receiver_handle),
                        v8::Utils::ToLocal(data_handle),
                        v8::Utils::ToLocal(holder_handle));
  if (!interceptor->query()->IsUndefined()) {
    v8::NamedPropertyQuery query =
        v8::ToCData<v8::NamedPropertyQuery>(interceptor->query());
    LOG(ApiNamedPropertyAccess("interceptor-named-has", *holder_handle, name));
    v8::Handle<v8::Boolean> result;
    {
      // Leaving JavaScript.
      VMState state(EXTERNAL);
      result = query(v8::Utils::ToLocal(name_handle), info);
    }
    if (!result.IsEmpty()) {
      // Convert the boolean result to a property attribute
      // specification.
      return result->IsTrue() ? NONE : ABSENT;
    }
  } else if (!interceptor->getter()->IsUndefined()) {
    v8::NamedPropertyGetter getter =
        v8::ToCData<v8::NamedPropertyGetter>(interceptor->getter());
    LOG(ApiNamedPropertyAccess("interceptor-named-get-has", this, name));
    v8::Handle<v8::Value> result;
    {
      // Leaving JavaScript.
      VMState state(EXTERNAL);
      result = getter(v8::Utils::ToLocal(name_handle), info);
    }
    if (!result.IsEmpty()) return NONE;
  }
  return holder_handle->GetPropertyAttributePostInterceptor(*receiver_handle,
                                                            *name_handle,
                                                            continue_search);
}


PropertyAttributes JSObject::GetPropertyAttributeWithReceiver(
      JSObject* receiver,
      String* key) {
  uint32_t index = 0;
  if (key->AsArrayIndex(&index)) {
    if (HasElementWithReceiver(receiver, index)) return NONE;
    return ABSENT;
  }
  // Named property.
  LookupResult result;
  Lookup(key, &result);
  return GetPropertyAttribute(receiver, &result, key, true);
}


PropertyAttributes JSObject::GetPropertyAttribute(JSObject* receiver,
                                                  LookupResult* result,
                                                  String* name,
                                                  bool continue_search) {
  // Check access rights if needed.
  if (IsAccessCheckNeeded() &&
      !Top::MayNamedAccess(this, name, v8::ACCESS_HAS)) {
    return GetPropertyAttributeWithFailedAccessCheck(receiver,
                                                     result,
                                                     name,
                                                     continue_search);
  }
  if (result->IsValid()) {
    switch (result->type()) {
      case NORMAL:  // fall through
      case FIELD:
      case CONSTANT_FUNCTION:
      case CALLBACKS:
        return result->GetAttributes();
      case INTERCEPTOR:
        return result->holder()->
          GetPropertyAttributeWithInterceptor(receiver, name, continue_search);
      case MAP_TRANSITION:
      case CONSTANT_TRANSITION:
      case NULL_DESCRIPTOR:
        return ABSENT;
      default:
        UNREACHABLE();
        break;
    }
  }
  return ABSENT;
}


PropertyAttributes JSObject::GetLocalPropertyAttribute(String* name) {
  // Check whether the name is an array index.
  uint32_t index = 0;
  if (name->AsArrayIndex(&index)) {
    if (HasLocalElement(index)) return NONE;
    return ABSENT;
  }
  // Named property.
  LookupResult result;
  LocalLookup(name, &result);
  return GetPropertyAttribute(this, &result, name, false);
}


Object* JSObject::NormalizeProperties(PropertyNormalizationMode mode,
                                      int expected_additional_properties) {
  if (!HasFastProperties()) return this;

  // The global object is always normalized.
  ASSERT(!IsGlobalObject());

  // Allocate new content.
  int property_count = map()->NumberOfDescribedProperties();
  if (expected_additional_properties > 0) {
    property_count += expected_additional_properties;
  } else {
    property_count += 2;  // Make space for two more properties.
  }
  Object* obj =
      StringDictionary::Allocate(property_count * 2);
  if (obj->IsFailure()) return obj;
  StringDictionary* dictionary = StringDictionary::cast(obj);

  DescriptorArray* descs = map()->instance_descriptors();
  for (int i = 0; i < descs->number_of_descriptors(); i++) {
    PropertyDetails details = descs->GetDetails(i);
    switch (details.type()) {
      case CONSTANT_FUNCTION: {
        PropertyDetails d =
            PropertyDetails(details.attributes(), NORMAL, details.index());
        Object* value = descs->GetConstantFunction(i);
        Object* result = dictionary->Add(descs->GetKey(i), value, d);
        if (result->IsFailure()) return result;
        dictionary = StringDictionary::cast(result);
        break;
      }
      case FIELD: {
        PropertyDetails d =
            PropertyDetails(details.attributes(), NORMAL, details.index());
        Object* value = FastPropertyAt(descs->GetFieldIndex(i));
        Object* result = dictionary->Add(descs->GetKey(i), value, d);
        if (result->IsFailure()) return result;
        dictionary = StringDictionary::cast(result);
        break;
      }
      case CALLBACKS: {
        PropertyDetails d =
            PropertyDetails(details.attributes(), CALLBACKS, details.index());
        Object* value = descs->GetCallbacksObject(i);
        Object* result = dictionary->Add(descs->GetKey(i), value, d);
        if (result->IsFailure()) return result;
        dictionary = StringDictionary::cast(result);
        break;
      }
      case MAP_TRANSITION:
      case CONSTANT_TRANSITION:
      case NULL_DESCRIPTOR:
      case INTERCEPTOR:
        break;
      default:
        UNREACHABLE();
    }
  }

  // Copy the next enumeration index from instance descriptor.
  int index = map()->instance_descriptors()->NextEnumerationIndex();
  dictionary->SetNextEnumerationIndex(index);

  // Allocate new map.
  obj = map()->CopyDropDescriptors();
  if (obj->IsFailure()) return obj;
  Map* new_map = Map::cast(obj);

  // Clear inobject properties if needed by adjusting the instance size and
  // putting in a filler object instead of the inobject properties.
  if (mode == CLEAR_INOBJECT_PROPERTIES && map()->inobject_properties() > 0) {
    int instance_size_delta = map()->inobject_properties() * kPointerSize;
    int new_instance_size = map()->instance_size() - instance_size_delta;
    new_map->set_inobject_properties(0);
    new_map->set_instance_size(new_instance_size);
    Heap::CreateFillerObjectAt(this->address() + new_instance_size,
                               instance_size_delta);
  }
  new_map->set_unused_property_fields(0);

  // We have now successfully allocated all the necessary objects.
  // Changes can now be made with the guarantee that all of them take effect.
  set_map(new_map);
  map()->set_instance_descriptors(Heap::empty_descriptor_array());

  set_properties(dictionary);

  Counters::props_to_dictionary.Increment();

#ifdef DEBUG
  if (FLAG_trace_normalization) {
    PrintF("Object properties have been normalized:\n");
    Print();
  }
#endif
  return this;
}


Object* JSObject::TransformToFastProperties(int unused_property_fields) {
  if (HasFastProperties()) return this;
  ASSERT(!IsGlobalObject());
  return property_dictionary()->
      TransformPropertiesToFastFor(this, unused_property_fields);
}


Object* JSObject::NormalizeElements() {
  ASSERT(!HasPixelElements());
  if (HasDictionaryElements()) return this;

  // Get number of entries.
  FixedArray* array = FixedArray::cast(elements());

  // Compute the effective length.
  int length = IsJSArray() ?
               Smi::cast(JSArray::cast(this)->length())->value() :
               array->length();
  Object* obj = NumberDictionary::Allocate(length);
  if (obj->IsFailure()) return obj;
  NumberDictionary* dictionary = NumberDictionary::cast(obj);
  // Copy entries.
  for (int i = 0; i < length; i++) {
    Object* value = array->get(i);
    if (!value->IsTheHole()) {
      PropertyDetails details = PropertyDetails(NONE, NORMAL);
      Object* result = dictionary->AddNumberEntry(i, array->get(i), details);
      if (result->IsFailure()) return result;
      dictionary = NumberDictionary::cast(result);
    }
  }
  // Switch to using the dictionary as the backing storage for elements.
  set_elements(dictionary);

  Counters::elements_to_dictionary.Increment();

#ifdef DEBUG
  if (FLAG_trace_normalization) {
    PrintF("Object elements have been normalized:\n");
    Print();
  }
#endif

  return this;
}


Object* JSObject::DeletePropertyPostInterceptor(String* name, DeleteMode mode) {
  // Check local property, ignore interceptor.
  LookupResult result;
  LocalLookupRealNamedProperty(name, &result);
  if (!result.IsValid()) return Heap::true_value();

  // Normalize object if needed.
  Object* obj = NormalizeProperties(CLEAR_INOBJECT_PROPERTIES, 0);
  if (obj->IsFailure()) return obj;

  return DeleteNormalizedProperty(name, mode);
}


Object* JSObject::DeletePropertyWithInterceptor(String* name) {
  HandleScope scope;
  Handle<InterceptorInfo> interceptor(GetNamedInterceptor());
  Handle<String> name_handle(name);
  Handle<JSObject> this_handle(this);
  if (!interceptor->deleter()->IsUndefined()) {
    v8::NamedPropertyDeleter deleter =
        v8::ToCData<v8::NamedPropertyDeleter>(interceptor->deleter());
    Handle<Object> data_handle(interceptor->data());
    LOG(ApiNamedPropertyAccess("interceptor-named-delete", *this_handle, name));
    v8::AccessorInfo info(v8::Utils::ToLocal(this_handle),
                          v8::Utils::ToLocal(data_handle),
                          v8::Utils::ToLocal(this_handle));
    v8::Handle<v8::Boolean> result;
    {
      // Leaving JavaScript.
      VMState state(EXTERNAL);
      result = deleter(v8::Utils::ToLocal(name_handle), info);
    }
    RETURN_IF_SCHEDULED_EXCEPTION();
    if (!result.IsEmpty()) {
      ASSERT(result->IsBoolean());
      return *v8::Utils::OpenHandle(*result);
    }
  }
  Object* raw_result =
      this_handle->DeletePropertyPostInterceptor(*name_handle, NORMAL_DELETION);
  RETURN_IF_SCHEDULED_EXCEPTION();
  return raw_result;
}


Object* JSObject::DeleteElementPostInterceptor(uint32_t index,
                                               DeleteMode mode) {
  ASSERT(!HasPixelElements());
  switch (GetElementsKind()) {
    case FAST_ELEMENTS: {
      uint32_t length = IsJSArray() ?
      static_cast<uint32_t>(Smi::cast(JSArray::cast(this)->length())->value()) :
      static_cast<uint32_t>(FixedArray::cast(elements())->length());
      if (index < length) {
        FixedArray::cast(elements())->set_the_hole(index);
      }
      break;
    }
    case DICTIONARY_ELEMENTS: {
      NumberDictionary* dictionary = element_dictionary();
      int entry = dictionary->FindEntry(index);
      if (entry != NumberDictionary::kNotFound) {
        return dictionary->DeleteProperty(entry, mode);
      }
      break;
    }
    default:
      UNREACHABLE();
      break;
  }
  return Heap::true_value();
}


Object* JSObject::DeleteElementWithInterceptor(uint32_t index) {
  // Make sure that the top context does not change when doing
  // callbacks or interceptor calls.
  AssertNoContextChange ncc;
  HandleScope scope;
  Handle<InterceptorInfo> interceptor(GetIndexedInterceptor());
  if (interceptor->deleter()->IsUndefined()) return Heap::false_value();
  v8::IndexedPropertyDeleter deleter =
      v8::ToCData<v8::IndexedPropertyDeleter>(interceptor->deleter());
  Handle<JSObject> this_handle(this);
  Handle<Object> data_handle(interceptor->data());
  LOG(ApiIndexedPropertyAccess("interceptor-indexed-delete", this, index));
  v8::AccessorInfo info(v8::Utils::ToLocal(this_handle),
                        v8::Utils::ToLocal(data_handle),
                        v8::Utils::ToLocal(this_handle));
  v8::Handle<v8::Boolean> result;
  {
    // Leaving JavaScript.
    VMState state(EXTERNAL);
    result = deleter(index, info);
  }
  RETURN_IF_SCHEDULED_EXCEPTION();
  if (!result.IsEmpty()) {
    ASSERT(result->IsBoolean());
    return *v8::Utils::OpenHandle(*result);
  }
  Object* raw_result =
      this_handle->DeleteElementPostInterceptor(index, NORMAL_DELETION);
  RETURN_IF_SCHEDULED_EXCEPTION();
  return raw_result;
}


Object* JSObject::DeleteElement(uint32_t index, DeleteMode mode) {
  // Check access rights if needed.
  if (IsAccessCheckNeeded() &&
      !Top::MayIndexedAccess(this, index, v8::ACCESS_DELETE)) {
    Top::ReportFailedAccessCheck(this, v8::ACCESS_DELETE);
    return Heap::false_value();
  }

  if (IsJSGlobalProxy()) {
    Object* proto = GetPrototype();
    if (proto->IsNull()) return Heap::false_value();
    ASSERT(proto->IsJSGlobalObject());
    return JSGlobalObject::cast(proto)->DeleteElement(index, mode);
  }

  if (HasIndexedInterceptor()) {
    // Skip interceptor if forcing deletion.
    if (mode == FORCE_DELETION) {
      return DeleteElementPostInterceptor(index, mode);
    }
    return DeleteElementWithInterceptor(index);
  }

  switch (GetElementsKind()) {
    case FAST_ELEMENTS: {
      uint32_t length = IsJSArray() ?
      static_cast<uint32_t>(Smi::cast(JSArray::cast(this)->length())->value()) :
      static_cast<uint32_t>(FixedArray::cast(elements())->length());
      if (index < length) {
        FixedArray::cast(elements())->set_the_hole(index);
      }
      break;
    }
    case PIXEL_ELEMENTS: {
      // Pixel elements cannot be deleted. Just silently ignore here.
      break;
    }
    case DICTIONARY_ELEMENTS: {
      NumberDictionary* dictionary = element_dictionary();
      int entry = dictionary->FindEntry(index);
      if (entry != NumberDictionary::kNotFound) {
        return dictionary->DeleteProperty(entry, mode);
      }
      break;
    }
    default:
      UNREACHABLE();
      break;
  }
  return Heap::true_value();
}


Object* JSObject::DeleteProperty(String* name, DeleteMode mode) {
  // ECMA-262, 3rd, 8.6.2.5
  ASSERT(name->IsString());

  // Check access rights if needed.
  if (IsAccessCheckNeeded() &&
      !Top::MayNamedAccess(this, name, v8::ACCESS_DELETE)) {
    Top::ReportFailedAccessCheck(this, v8::ACCESS_DELETE);
    return Heap::false_value();
  }

  if (IsJSGlobalProxy()) {
    Object* proto = GetPrototype();
    if (proto->IsNull()) return Heap::false_value();
    ASSERT(proto->IsJSGlobalObject());
    return JSGlobalObject::cast(proto)->DeleteProperty(name, mode);
  }

  uint32_t index = 0;
  if (name->AsArrayIndex(&index)) {
    return DeleteElement(index, mode);
  } else {
    LookupResult result;
    LocalLookup(name, &result);
    if (!result.IsValid()) return Heap::true_value();
    // Ignore attributes if forcing a deletion.
    if (result.IsDontDelete() && mode != FORCE_DELETION) {
      return Heap::false_value();
    }
    // Check for interceptor.
    if (result.type() == INTERCEPTOR) {
      // Skip interceptor if forcing a deletion.
      if (mode == FORCE_DELETION) {
        return DeletePropertyPostInterceptor(name, mode);
      }
      return DeletePropertyWithInterceptor(name);
    }
    if (!result.IsLoaded()) {
      return JSObject::cast(this)->DeleteLazyProperty(&result,
                                                      name,
                                                      mode);
    }
    // Normalize object if needed.
    Object* obj = NormalizeProperties(CLEAR_INOBJECT_PROPERTIES, 0);
    if (obj->IsFailure()) return obj;
    // Make sure the properties are normalized before removing the entry.
    return DeleteNormalizedProperty(name, mode);
  }
}


// Check whether this object references another object.
bool JSObject::ReferencesObject(Object* obj) {
  AssertNoAllocation no_alloc;

  // Is the object the constructor for this object?
  if (map()->constructor() == obj) {
    return true;
  }

  // Is the object the prototype for this object?
  if (map()->prototype() == obj) {
    return true;
  }

  // Check if the object is among the named properties.
  Object* key = SlowReverseLookup(obj);
  if (key != Heap::undefined_value()) {
    return true;
  }

  // Check if the object is among the indexed properties.
  switch (GetElementsKind()) {
    case PIXEL_ELEMENTS:
      // Raw pixels do not reference other objects.
      break;
    case FAST_ELEMENTS: {
      int length = IsJSArray() ?
          Smi::cast(JSArray::cast(this)->length())->value() :
          FixedArray::cast(elements())->length();
      for (int i = 0; i < length; i++) {
        Object* element = FixedArray::cast(elements())->get(i);
        if (!element->IsTheHole() && element == obj) {
          return true;
        }
      }
      break;
    }
    case DICTIONARY_ELEMENTS: {
      key = element_dictionary()->SlowReverseLookup(obj);
      if (key != Heap::undefined_value()) {
        return true;
      }
      break;
    }
    default:
      UNREACHABLE();
      break;
  }

  // For functions check the context. Boilerplate functions do
  // not have to be traversed since they have no real context.
  if (IsJSFunction() && !JSFunction::cast(this)->IsBoilerplate()) {
    // Get the constructor function for arguments array.
    JSObject* arguments_boilerplate =
        Top::context()->global_context()->arguments_boilerplate();
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

    // Check the context extension if any.
    if (context->has_extension()) {
      return context->extension()->ReferencesObject(obj);
    }
  }

  // No references to object.
  return false;
}


// Tests for the fast common case for property enumeration:
// - this object has an enum cache
// - this object has no elements
// - no prototype has enumerable properties/elements
// - neither this object nor any prototype has interceptors
bool JSObject::IsSimpleEnum() {
  JSObject* arguments_boilerplate =
      Top::context()->global_context()->arguments_boilerplate();
  JSFunction* arguments_function =
      JSFunction::cast(arguments_boilerplate->map()->constructor());
  if (IsAccessCheckNeeded()) return false;
  if (map()->constructor() == arguments_function) return false;

  for (Object* o = this;
       o != Heap::null_value();
       o = JSObject::cast(o)->GetPrototype()) {
    JSObject* curr = JSObject::cast(o);
    if (!curr->HasFastProperties()) return false;
    if (!curr->map()->instance_descriptors()->HasEnumCache()) return false;
    if (curr->NumberOfEnumElements() > 0) return false;
    if (curr->HasNamedInterceptor()) return false;
    if (curr->HasIndexedInterceptor()) return false;
    if (curr != this) {
      FixedArray* curr_fixed_array =
          FixedArray::cast(curr->map()->instance_descriptors()->GetEnumCache());
      if (curr_fixed_array->length() > 0) {
        return false;
      }
    }
  }
  return true;
}


int Map::NumberOfDescribedProperties() {
  int result = 0;
  DescriptorArray* descs = instance_descriptors();
  for (int i = 0; i < descs->number_of_descriptors(); i++) {
    if (descs->IsProperty(i)) result++;
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


void JSObject::LocalLookup(String* name, LookupResult* result) {
  ASSERT(name->IsString());

  if (IsJSGlobalProxy()) {
    Object* proto = GetPrototype();
    if (proto->IsNull()) return result->NotFound();
    ASSERT(proto->IsJSGlobalObject());
    return JSObject::cast(proto)->LocalLookup(name, result);
  }

  // Do not use inline caching if the object is a non-global object
  // that requires access checks.
  if (!IsJSGlobalProxy() && IsAccessCheckNeeded()) {
    result->DisallowCaching();
  }

  // Check __proto__ before interceptor.
  if (name->Equals(Heap::Proto_symbol()) && !IsJSContextExtensionObject()) {
    result->ConstantResult(this);
    return;
  }

  // Check for lookup interceptor except when bootstrapping.
  if (HasNamedInterceptor() && !Bootstrapper::IsActive()) {
    result->InterceptorResult(this);
    return;
  }

  LocalLookupRealNamedProperty(name, result);
}


void JSObject::Lookup(String* name, LookupResult* result) {
  // Ecma-262 3rd 8.6.2.4
  for (Object* current = this;
       current != Heap::null_value();
       current = JSObject::cast(current)->GetPrototype()) {
    JSObject::cast(current)->LocalLookup(name, result);
    if (result->IsValid() && !result->IsTransitionType()) return;
  }
  result->NotFound();
}


// Search object and it's prototype chain for callback properties.
void JSObject::LookupCallback(String* name, LookupResult* result) {
  for (Object* current = this;
       current != Heap::null_value();
       current = JSObject::cast(current)->GetPrototype()) {
    JSObject::cast(current)->LocalLookupRealNamedProperty(name, result);
    if (result->IsValid() && result->type() == CALLBACKS) return;
  }
  result->NotFound();
}


Object* JSObject::DefineGetterSetter(String* name,
                                     PropertyAttributes attributes) {
  // Make sure that the top context does not change when doing callbacks or
  // interceptor calls.
  AssertNoContextChange ncc;

  // Check access rights if needed.
  if (IsAccessCheckNeeded() &&
      !Top::MayNamedAccess(this, name, v8::ACCESS_SET)) {
    Top::ReportFailedAccessCheck(this, v8::ACCESS_SET);
    return Heap::undefined_value();
  }

  // Try to flatten before operating on the string.
  name->TryFlattenIfNotFlat();

  // Check if there is an API defined callback object which prohibits
  // callback overwriting in this object or it's prototype chain.
  // This mechanism is needed for instance in a browser setting, where
  // certain accessors such as window.location should not be allowed
  // to be overwritten because allowing overwriting could potentially
  // cause security problems.
  LookupResult callback_result;
  LookupCallback(name, &callback_result);
  if (callback_result.IsValid()) {
    Object* obj = callback_result.GetCallbackObject();
    if (obj->IsAccessorInfo() &&
        AccessorInfo::cast(obj)->prohibits_overwriting()) {
      return Heap::undefined_value();
    }
  }

  uint32_t index;
  bool is_element = name->AsArrayIndex(&index);
  if (is_element && IsJSArray()) return Heap::undefined_value();

  if (is_element) {
    switch (GetElementsKind()) {
      case FAST_ELEMENTS:
        break;
      case PIXEL_ELEMENTS:
        // Ignore getters and setters on pixel elements.
        return Heap::undefined_value();
      case DICTIONARY_ELEMENTS: {
        // Lookup the index.
        NumberDictionary* dictionary = element_dictionary();
        int entry = dictionary->FindEntry(index);
        if (entry != NumberDictionary::kNotFound) {
          Object* result = dictionary->ValueAt(entry);
          PropertyDetails details = dictionary->DetailsAt(entry);
          if (details.IsReadOnly()) return Heap::undefined_value();
          if (details.type() == CALLBACKS) {
            // Only accessors allowed as elements.
            ASSERT(result->IsFixedArray());
            return result;
          }
        }
        break;
      }
      default:
        UNREACHABLE();
        break;
    }
  } else {
    // Lookup the name.
    LookupResult result;
    LocalLookup(name, &result);
    if (result.IsValid()) {
      if (result.IsReadOnly()) return Heap::undefined_value();
      if (result.type() == CALLBACKS) {
        Object* obj = result.GetCallbackObject();
        if (obj->IsFixedArray()) return obj;
      }
    }
  }

  // Allocate the fixed array to hold getter and setter.
  Object* structure = Heap::AllocateFixedArray(2, TENURED);
  if (structure->IsFailure()) return structure;
  PropertyDetails details = PropertyDetails(attributes, CALLBACKS);

  if (is_element) {
    // Normalize object to make this operation simple.
    Object* ok = NormalizeElements();
    if (ok->IsFailure()) return ok;

    // Update the dictionary with the new CALLBACKS property.
    Object* dict =
        element_dictionary()->Set(index, structure, details);
    if (dict->IsFailure()) return dict;

    // If name is an index we need to stay in slow case.
    NumberDictionary* elements = NumberDictionary::cast(dict);
    elements->set_requires_slow_elements();
    // Set the potential new dictionary on the object.
    set_elements(NumberDictionary::cast(dict));
  } else {
    // Normalize object to make this operation simple.
    Object* ok = NormalizeProperties(CLEAR_INOBJECT_PROPERTIES, 0);
    if (ok->IsFailure()) return ok;

    // For the global object allocate a new map to invalidate the global inline
    // caches which have a global property cell reference directly in the code.
    if (IsGlobalObject()) {
      Object* new_map = map()->CopyDropDescriptors();
      if (new_map->IsFailure()) return new_map;
      set_map(Map::cast(new_map));
    }

    // Update the dictionary with the new CALLBACKS property.
    return SetNormalizedProperty(name, structure, details);
  }

  return structure;
}


Object* JSObject::DefineAccessor(String* name, bool is_getter, JSFunction* fun,
                                 PropertyAttributes attributes) {
  // Check access rights if needed.
  if (IsAccessCheckNeeded() &&
      !Top::MayNamedAccess(this, name, v8::ACCESS_HAS)) {
    Top::ReportFailedAccessCheck(this, v8::ACCESS_HAS);
    return Heap::undefined_value();
  }

  if (IsJSGlobalProxy()) {
    Object* proto = GetPrototype();
    if (proto->IsNull()) return this;
    ASSERT(proto->IsJSGlobalObject());
    return JSObject::cast(proto)->DefineAccessor(name, is_getter,
                                                 fun, attributes);
  }

  Object* array = DefineGetterSetter(name, attributes);
  if (array->IsFailure() || array->IsUndefined()) return array;
  FixedArray::cast(array)->set(is_getter ? 0 : 1, fun);
  return this;
}


Object* JSObject::LookupAccessor(String* name, bool is_getter) {
  // Make sure that the top context does not change when doing callbacks or
  // interceptor calls.
  AssertNoContextChange ncc;

  // Check access rights if needed.
  if (IsAccessCheckNeeded() &&
      !Top::MayNamedAccess(this, name, v8::ACCESS_HAS)) {
    Top::ReportFailedAccessCheck(this, v8::ACCESS_HAS);
    return Heap::undefined_value();
  }

  // Make the lookup and include prototypes.
  int accessor_index = is_getter ? kGetterIndex : kSetterIndex;
  uint32_t index;
  if (name->AsArrayIndex(&index)) {
    for (Object* obj = this;
         obj != Heap::null_value();
         obj = JSObject::cast(obj)->GetPrototype()) {
      JSObject* js_object = JSObject::cast(obj);
      if (js_object->HasDictionaryElements()) {
        NumberDictionary* dictionary = js_object->element_dictionary();
        int entry = dictionary->FindEntry(index);
        if (entry != NumberDictionary::kNotFound) {
          Object* element = dictionary->ValueAt(entry);
          PropertyDetails details = dictionary->DetailsAt(entry);
          if (details.type() == CALLBACKS) {
            // Only accessors allowed as elements.
            return FixedArray::cast(element)->get(accessor_index);
          }
        }
      }
    }
  } else {
    for (Object* obj = this;
         obj != Heap::null_value();
         obj = JSObject::cast(obj)->GetPrototype()) {
      LookupResult result;
      JSObject::cast(obj)->LocalLookup(name, &result);
      if (result.IsValid()) {
        if (result.IsReadOnly()) return Heap::undefined_value();
        if (result.type() == CALLBACKS) {
          Object* obj = result.GetCallbackObject();
          if (obj->IsFixedArray()) {
            return FixedArray::cast(obj)->get(accessor_index);
          }
        }
      }
    }
  }
  return Heap::undefined_value();
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
    return Heap::undefined_value();
  } else {
    return property_dictionary()->SlowReverseLookup(value);
  }
}


Object* Map::CopyDropDescriptors() {
  Object* result = Heap::AllocateMap(instance_type(), instance_size());
  if (result->IsFailure()) return result;
  Map::cast(result)->set_prototype(prototype());
  Map::cast(result)->set_constructor(constructor());
  // Don't copy descriptors, so map transitions always remain a forest.
  // If we retained the same descriptors we would have two maps
  // pointing to the same transition which is bad because the garbage
  // collector relies on being able to reverse pointers from transitions
  // to maps.  If properties need to be retained use CopyDropTransitions.
  Map::cast(result)->set_instance_descriptors(Heap::empty_descriptor_array());
  // Please note instance_type and instance_size are set when allocated.
  Map::cast(result)->set_inobject_properties(inobject_properties());
  Map::cast(result)->set_unused_property_fields(unused_property_fields());
  Map::cast(result)->set_bit_field(bit_field());
  Map::cast(result)->set_bit_field2(bit_field2());
  Map::cast(result)->ClearCodeCache();
  return result;
}


Object* Map::CopyDropTransitions() {
  Object* new_map = CopyDropDescriptors();
  if (new_map->IsFailure()) return new_map;
  Object* descriptors = instance_descriptors()->RemoveTransitions();
  if (descriptors->IsFailure()) return descriptors;
  cast(new_map)->set_instance_descriptors(DescriptorArray::cast(descriptors));
  return cast(new_map);
}


Object* Map::UpdateCodeCache(String* name, Code* code) {
  ASSERT(code->ic_state() == MONOMORPHIC);
  FixedArray* cache = code_cache();

  // When updating the code cache we disregard the type encoded in the
  // flags. This allows call constant stubs to overwrite call field
  // stubs, etc.
  Code::Flags flags = Code::RemoveTypeFromFlags(code->flags());

  // First check whether we can update existing code cache without
  // extending it.
  int length = cache->length();
  int deleted_index = -1;
  for (int i = 0; i < length; i += 2) {
    Object* key = cache->get(i);
    if (key->IsNull()) {
      if (deleted_index < 0) deleted_index = i;
      continue;
    }
    if (key->IsUndefined()) {
      if (deleted_index >= 0) i = deleted_index;
      cache->set(i + 0, name);
      cache->set(i + 1, code);
      return this;
    }
    if (name->Equals(String::cast(key))) {
      Code::Flags found = Code::cast(cache->get(i + 1))->flags();
      if (Code::RemoveTypeFromFlags(found) == flags) {
        cache->set(i + 1, code);
        return this;
      }
    }
  }

  // Reached the end of the code cache.  If there were deleted
  // elements, reuse the space for the first of them.
  if (deleted_index >= 0) {
    cache->set(deleted_index + 0, name);
    cache->set(deleted_index + 1, code);
    return this;
  }

  // Extend the code cache with some new entries (at least one).
  int new_length = length + ((length >> 1) & ~1) + 2;
  ASSERT((new_length & 1) == 0);  // must be a multiple of two
  Object* result = cache->CopySize(new_length);
  if (result->IsFailure()) return result;

  // Add the (name, code) pair to the new cache.
  cache = FixedArray::cast(result);
  cache->set(length + 0, name);
  cache->set(length + 1, code);
  set_code_cache(cache);
  return this;
}


Object* Map::FindInCodeCache(String* name, Code::Flags flags) {
  FixedArray* cache = code_cache();
  int length = cache->length();
  for (int i = 0; i < length; i += 2) {
    Object* key = cache->get(i);
    // Skip deleted elements.
    if (key->IsNull()) continue;
    if (key->IsUndefined()) return key;
    if (name->Equals(String::cast(key))) {
      Code* code = Code::cast(cache->get(i + 1));
      if (code->flags() == flags) return code;
    }
  }
  return Heap::undefined_value();
}


int Map::IndexInCodeCache(Code* code) {
  FixedArray* array = code_cache();
  int len = array->length();
  for (int i = 0; i < len; i += 2) {
    if (array->get(i + 1) == code) return i + 1;
  }
  return -1;
}


void Map::RemoveFromCodeCache(int index) {
  FixedArray* array = code_cache();
  ASSERT(array->length() >= index && array->get(index)->IsCode());
  // Use null instead of undefined for deleted elements to distinguish
  // deleted elements from unused elements.  This distinction is used
  // when looking up in the cache and when updating the cache.
  array->set_null(index - 1);  // key
  array->set_null(index);  // code
}


void FixedArray::FixedArrayIterateBody(ObjectVisitor* v) {
  IteratePointers(v, kHeaderSize, kHeaderSize + length() * kPointerSize);
}


static bool HasKey(FixedArray* array, Object* key) {
  int len0 = array->length();
  for (int i = 0; i < len0; i++) {
    Object* element = array->get(i);
    if (element->IsSmi() && key->IsSmi() && (element == key)) return true;
    if (element->IsString() &&
        key->IsString() && String::cast(element)->Equals(String::cast(key))) {
      return true;
    }
  }
  return false;
}


Object* FixedArray::AddKeysFromJSArray(JSArray* array) {
  ASSERT(!array->HasPixelElements());
  switch (array->GetElementsKind()) {
    case JSObject::FAST_ELEMENTS:
      return UnionOfKeys(FixedArray::cast(array->elements()));
    case JSObject::DICTIONARY_ELEMENTS: {
      NumberDictionary* dict = array->element_dictionary();
      int size = dict->NumberOfElements();

      // Allocate a temporary fixed array.
      Object* object = Heap::AllocateFixedArray(size);
      if (object->IsFailure()) return object;
      FixedArray* key_array = FixedArray::cast(object);

      int capacity = dict->Capacity();
      int pos = 0;
      // Copy the elements from the JSArray to the temporary fixed array.
      for (int i = 0; i < capacity; i++) {
        if (dict->IsKey(dict->KeyAt(i))) {
          key_array->set(pos++, dict->ValueAt(i));
        }
      }
      // Compute the union of this and the temporary fixed array.
      return UnionOfKeys(key_array);
    }
    default:
      UNREACHABLE();
  }
  UNREACHABLE();
  return Heap::null_value();  // Failure case needs to "return" a value.
}


Object* FixedArray::UnionOfKeys(FixedArray* other) {
  int len0 = length();
  int len1 = other->length();
  // Optimize if either is empty.
  if (len0 == 0) return other;
  if (len1 == 0) return this;

  // Compute how many elements are not in this.
  int extra = 0;
  for (int y = 0; y < len1; y++) {
    Object* value = other->get(y);
    if (!value->IsTheHole() && !HasKey(this, value)) extra++;
  }

  if (extra == 0) return this;

  // Allocate the result
  Object* obj = Heap::AllocateFixedArray(len0 + extra);
  if (obj->IsFailure()) return obj;
  // Fill in the content
  FixedArray* result = FixedArray::cast(obj);
  WriteBarrierMode mode = result->GetWriteBarrierMode();
  for (int i = 0; i < len0; i++) {
    result->set(i, get(i), mode);
  }
  // Fill in the extra keys.
  int index = 0;
  for (int y = 0; y < len1; y++) {
    Object* value = other->get(y);
    if (!value->IsTheHole() && !HasKey(this, value)) {
      result->set(len0 + index, other->get(y), mode);
      index++;
    }
  }
  ASSERT(extra == index);
  return result;
}


Object* FixedArray::CopySize(int new_length) {
  if (new_length == 0) return Heap::empty_fixed_array();
  Object* obj = Heap::AllocateFixedArray(new_length);
  if (obj->IsFailure()) return obj;
  FixedArray* result = FixedArray::cast(obj);
  // Copy the content
  int len = length();
  if (new_length < len) len = new_length;
  result->set_map(map());
  WriteBarrierMode mode = result->GetWriteBarrierMode();
  for (int i = 0; i < len; i++) {
    result->set(i, get(i), mode);
  }
  return result;
}


void FixedArray::CopyTo(int pos, FixedArray* dest, int dest_pos, int len) {
  WriteBarrierMode mode = dest->GetWriteBarrierMode();
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


Object* DescriptorArray::Allocate(int number_of_descriptors) {
  if (number_of_descriptors == 0) {
    return Heap::empty_descriptor_array();
  }
  // Allocate the array of keys.
  Object* array = Heap::AllocateFixedArray(ToKeyIndex(number_of_descriptors));
  if (array->IsFailure()) return array;
  // Do not use DescriptorArray::cast on incomplete object.
  FixedArray* result = FixedArray::cast(array);

  // Allocate the content array and set it in the descriptor array.
  array = Heap::AllocateFixedArray(number_of_descriptors << 1);
  if (array->IsFailure()) return array;
  result->set(kContentArrayIndex, array);
  result->set(kEnumerationIndexIndex,
              Smi::FromInt(PropertyDetails::kInitialIndex),
              SKIP_WRITE_BARRIER);
  return result;
}


void DescriptorArray::SetEnumCache(FixedArray* bridge_storage,
                                   FixedArray* new_cache) {
  ASSERT(bridge_storage->length() >= kEnumCacheBridgeLength);
  if (HasEnumCache()) {
    FixedArray::cast(get(kEnumerationIndexIndex))->
      set(kEnumCacheBridgeCacheIndex, new_cache);
  } else {
    if (IsEmpty()) return;  // Do nothing for empty descriptor array.
    FixedArray::cast(bridge_storage)->
      set(kEnumCacheBridgeCacheIndex, new_cache);
    fast_set(FixedArray::cast(bridge_storage),
             kEnumCacheBridgeEnumIndex,
             get(kEnumerationIndexIndex));
    set(kEnumerationIndexIndex, bridge_storage);
  }
}


Object* DescriptorArray::CopyInsert(Descriptor* descriptor,
                                    TransitionFlag transition_flag) {
  // Transitions are only kept when inserting another transition.
  // This precondition is not required by this function's implementation, but
  // is currently required by the semantics of maps, so we check it.
  // Conversely, we filter after replacing, so replacing a transition and
  // removing all other transitions is not supported.
  bool remove_transitions = transition_flag == REMOVE_TRANSITIONS;
  ASSERT(remove_transitions == !descriptor->GetDetails().IsTransition());
  ASSERT(descriptor->GetDetails().type() != NULL_DESCRIPTOR);

  // Ensure the key is a symbol.
  Object* result = descriptor->KeyToSymbol();
  if (result->IsFailure()) return result;

  int transitions = 0;
  int null_descriptors = 0;
  if (remove_transitions) {
    for (int i = 0; i < number_of_descriptors(); i++) {
      if (IsTransition(i)) transitions++;
      if (IsNullDescriptor(i)) null_descriptors++;
    }
  } else {
    for (int i = 0; i < number_of_descriptors(); i++) {
      if (IsNullDescriptor(i)) null_descriptors++;
    }
  }
  int new_size = number_of_descriptors() - transitions - null_descriptors;

  // If key is in descriptor, we replace it in-place when filtering.
  // Count a null descriptor for key as inserted, not replaced.
  int index = Search(descriptor->GetKey());
  const bool inserting = (index == kNotFound);
  const bool replacing = !inserting;
  bool keep_enumeration_index = false;
  if (inserting) {
    ++new_size;
  }
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
  }
  result = Allocate(new_size);
  if (result->IsFailure()) return result;
  DescriptorArray* new_descriptors = DescriptorArray::cast(result);
  // Set the enumeration index in the descriptors and set the enumeration index
  // in the result.
  int enumeration_index = NextEnumerationIndex();
  if (!descriptor->GetDetails().IsTransition()) {
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
  uint32_t descriptor_hash = descriptor->GetKey()->Hash();
  int from_index = 0;
  int to_index = 0;

  for (; from_index < number_of_descriptors(); from_index++) {
    String* key = GetKey(from_index);
    if (key->Hash() > descriptor_hash || key == descriptor->GetKey()) {
      break;
    }
    if (IsNullDescriptor(from_index)) continue;
    if (remove_transitions && IsTransition(from_index)) continue;
    new_descriptors->CopyFrom(to_index++, this, from_index);
  }

  new_descriptors->Set(to_index++, descriptor);
  if (replacing) from_index++;

  for (; from_index < number_of_descriptors(); from_index++) {
    if (IsNullDescriptor(from_index)) continue;
    if (remove_transitions && IsTransition(from_index)) continue;
    new_descriptors->CopyFrom(to_index++, this, from_index);
  }

  ASSERT(to_index == new_descriptors->number_of_descriptors());
  SLOW_ASSERT(new_descriptors->IsSortedNoDuplicates());

  return new_descriptors;
}


Object* DescriptorArray::RemoveTransitions() {
  // Remove all transitions and null descriptors. Return a copy of the array
  // with all transitions removed, or a Failure object if the new array could
  // not be allocated.

  // Compute the size of the map transition entries to be removed.
  int num_removed = 0;
  for (int i = 0; i < number_of_descriptors(); i++) {
    if (!IsProperty(i)) num_removed++;
  }

  // Allocate the new descriptor array.
  Object* result = Allocate(number_of_descriptors() - num_removed);
  if (result->IsFailure()) return result;
  DescriptorArray* new_descriptors = DescriptorArray::cast(result);

  // Copy the content.
  int next_descriptor = 0;
  for (int i = 0; i < number_of_descriptors(); i++) {
    if (IsProperty(i)) new_descriptors->CopyFrom(next_descriptor++, this, i);
  }
  ASSERT(next_descriptor == new_descriptors->number_of_descriptors());

  return new_descriptors;
}


void DescriptorArray::Sort() {
  // In-place heap sort.
  int len = number_of_descriptors();

  // Bottom-up max-heap construction.
  for (int i = 1; i < len; ++i) {
    int child_index = i;
    while (child_index > 0) {
      int parent_index = ((child_index + 1) >> 1) - 1;
      uint32_t parent_hash = GetKey(parent_index)->Hash();
      uint32_t child_hash = GetKey(child_index)->Hash();
      if (parent_hash < child_hash) {
        Swap(parent_index, child_index);
      } else {
        break;
      }
      child_index = parent_index;
    }
  }

  // Extract elements and create sorted array.
  for (int i = len - 1; i > 0; --i) {
    // Put max element at the back of the array.
    Swap(0, i);
    // Sift down the new top element.
    int parent_index = 0;
    while (true) {
      int child_index = ((parent_index + 1) << 1) - 1;
      if (child_index >= i) break;
      uint32_t child1_hash = GetKey(child_index)->Hash();
      uint32_t child2_hash = GetKey(child_index + 1)->Hash();
      uint32_t parent_hash = GetKey(parent_index)->Hash();
      if (child_index + 1 >= i || child1_hash > child2_hash) {
        if (parent_hash > child1_hash) break;
        Swap(parent_index, child_index);
        parent_index = child_index;
      } else {
        if (parent_hash > child2_hash) break;
        Swap(parent_index, child_index + 1);
        parent_index = child_index + 1;
      }
    }
  }

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


static StaticResource<StringInputBuffer> string_input_buffer;


bool String::LooksValid() {
  if (!Heap::Contains(this)) return false;
  return true;
}


int String::Utf8Length() {
  if (IsAsciiRepresentation()) return length();
  // Attempt to flatten before accessing the string.  It probably
  // doesn't make Utf8Length faster, but it is very likely that
  // the string will be accessed later (for example by WriteUtf8)
  // so it's still a good idea.
  TryFlattenIfNotFlat();
  Access<StringInputBuffer> buffer(&string_input_buffer);
  buffer->Reset(0, this);
  int result = 0;
  while (buffer->has_more())
    result += unibrow::Utf8::Length(buffer->GetNext());
  return result;
}


Vector<const char> String::ToAsciiVector() {
  ASSERT(IsAsciiRepresentation());
  ASSERT(IsFlat());

  int offset = 0;
  int length = this->length();
  StringRepresentationTag string_tag = StringShape(this).representation_tag();
  String* string = this;
  if (string_tag == kSlicedStringTag) {
    SlicedString* sliced = SlicedString::cast(string);
    offset += sliced->start();
    string = sliced->buffer();
    string_tag = StringShape(string).representation_tag();
  } else if (string_tag == kConsStringTag) {
    ConsString* cons = ConsString::cast(string);
    ASSERT(cons->second()->length() == 0);
    string = cons->first();
    string_tag = StringShape(string).representation_tag();
  }
  if (string_tag == kSeqStringTag) {
    SeqAsciiString* seq = SeqAsciiString::cast(string);
    char* start = seq->GetChars();
    return Vector<const char>(start + offset, length);
  }
  ASSERT(string_tag == kExternalStringTag);
  ExternalAsciiString* ext = ExternalAsciiString::cast(string);
  const char* start = ext->resource()->data();
  return Vector<const char>(start + offset, length);
}


Vector<const uc16> String::ToUC16Vector() {
  ASSERT(IsTwoByteRepresentation());
  ASSERT(IsFlat());

  int offset = 0;
  int length = this->length();
  StringRepresentationTag string_tag = StringShape(this).representation_tag();
  String* string = this;
  if (string_tag == kSlicedStringTag) {
    SlicedString* sliced = SlicedString::cast(string);
    offset += sliced->start();
    string = String::cast(sliced->buffer());
    string_tag = StringShape(string).representation_tag();
  } else if (string_tag == kConsStringTag) {
    ConsString* cons = ConsString::cast(string);
    ASSERT(cons->second()->length() == 0);
    string = cons->first();
    string_tag = StringShape(string).representation_tag();
  }
  if (string_tag == kSeqStringTag) {
    SeqTwoByteString* seq = SeqTwoByteString::cast(string);
    return Vector<const uc16>(seq->GetChars() + offset, length);
  }
  ASSERT(string_tag == kExternalStringTag);
  ExternalTwoByteString* ext = ExternalTwoByteString::cast(string);
  const uc16* start =
      reinterpret_cast<const uc16*>(ext->resource()->data());
  return Vector<const uc16>(start + offset, length);
}


SmartPointer<char> String::ToCString(AllowNullsFlag allow_nulls,
                                     RobustnessFlag robust_flag,
                                     int offset,
                                     int length,
                                     int* length_return) {
  ASSERT(NativeAllocationChecker::allocation_allowed());
  if (robust_flag == ROBUST_STRING_TRAVERSAL && !LooksValid()) {
    return SmartPointer<char>(NULL);
  }

  // Negative length means the to the end of the string.
  if (length < 0) length = kMaxInt - offset;

  // Compute the size of the UTF-8 string. Start at the specified offset.
  Access<StringInputBuffer> buffer(&string_input_buffer);
  buffer->Reset(offset, this);
  int character_position = offset;
  int utf8_bytes = 0;
  while (buffer->has_more()) {
    uint16_t character = buffer->GetNext();
    if (character_position < offset + length) {
      utf8_bytes += unibrow::Utf8::Length(character);
    }
    character_position++;
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
  while (buffer->has_more()) {
    uint16_t character = buffer->GetNext();
    if (character_position < offset + length) {
      if (allow_nulls == DISALLOW_NULLS && character == 0) {
        character = ' ';
      }
      utf8_byte_position +=
          unibrow::Utf8::Encode(result + utf8_byte_position, character);
    }
    character_position++;
  }
  result[utf8_byte_position] = 0;
  return SmartPointer<char>(result);
}


SmartPointer<char> String::ToCString(AllowNullsFlag allow_nulls,
                                     RobustnessFlag robust_flag,
                                     int* length_return) {
  return ToCString(allow_nulls, robust_flag, 0, -1, length_return);
}


const uc16* String::GetTwoByteData() {
  return GetTwoByteData(0);
}


const uc16* String::GetTwoByteData(unsigned start) {
  ASSERT(!IsAsciiRepresentation());
  switch (StringShape(this).representation_tag()) {
    case kSeqStringTag:
      return SeqTwoByteString::cast(this)->SeqTwoByteStringGetData(start);
    case kExternalStringTag:
      return ExternalTwoByteString::cast(this)->
        ExternalTwoByteStringGetData(start);
    case kSlicedStringTag: {
      SlicedString* sliced_string = SlicedString::cast(this);
      String* buffer = sliced_string->buffer();
      if (StringShape(buffer).IsCons()) {
        ConsString* cs = ConsString::cast(buffer);
        // Flattened string.
        ASSERT(cs->second()->length() == 0);
        buffer = cs->first();
      }
      return buffer->GetTwoByteData(start + sliced_string->start());
    }
    case kConsStringTag:
      UNREACHABLE();
      return NULL;
  }
  UNREACHABLE();
  return NULL;
}


SmartPointer<uc16> String::ToWideCString(RobustnessFlag robust_flag) {
  ASSERT(NativeAllocationChecker::allocation_allowed());

  if (robust_flag == ROBUST_STRING_TRAVERSAL && !LooksValid()) {
    return SmartPointer<uc16>();
  }

  Access<StringInputBuffer> buffer(&string_input_buffer);
  buffer->Reset(this);

  uc16* result = NewArray<uc16>(length() + 1);

  int i = 0;
  while (buffer->has_more()) {
    uint16_t character = buffer->GetNext();
    result[i++] = character;
  }
  result[i] = 0;
  return SmartPointer<uc16>(result);
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


const unibrow::byte* SlicedString::SlicedStringReadBlock(ReadBlockBuffer* rbb,
                                                         unsigned* offset_ptr,
                                                         unsigned max_chars) {
  String* backing = buffer();
  unsigned offset = start() + *offset_ptr;
  unsigned length = backing->length();
  if (max_chars > length - offset) {
    max_chars = length - offset;
  }
  const unibrow::byte* answer =
      String::ReadBlock(backing, rbb, &offset, max_chars);
  *offset_ptr = offset - start();
  return answer;
}


uint16_t ExternalAsciiString::ExternalAsciiStringGet(int index) {
  ASSERT(index >= 0 && index < length());
  return resource()->data()[index];
}


const unibrow::byte* ExternalAsciiString::ExternalAsciiStringReadBlock(
      unsigned* remaining,
      unsigned* offset_ptr,
      unsigned max_chars) {
  // Cast const char* to unibrow::byte* (signedness difference).
  const unibrow::byte* b =
      reinterpret_cast<const unibrow::byte*>(resource()->data()) + *offset_ptr;
  *remaining = max_chars;
  *offset_ptr += max_chars;
  return b;
}


const uc16* ExternalTwoByteString::ExternalTwoByteStringGetData(
      unsigned start) {
  return resource()->data() + start;
}


uint16_t ExternalTwoByteString::ExternalTwoByteStringGet(int index) {
  ASSERT(index >= 0 && index < length());
  return resource()->data()[index];
}


void ExternalTwoByteString::ExternalTwoByteStringReadBlockIntoBuffer(
      ReadBlockBuffer* rbb,
      unsigned* offset_ptr,
      unsigned max_chars) {
  unsigned chars_read = 0;
  unsigned offset = *offset_ptr;
  const uint16_t* data = resource()->data();
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
  memcpy(rbb->util_buffer + rbb->cursor,
         resource()->data() + *offset_ptr,
         max_chars);
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
    case kSlicedStringTag:
      return SlicedString::cast(input)->SlicedStringReadBlock(rbb,
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
    default:
      break;
  }

  UNREACHABLE();
  return 0;
}


FlatStringReader* FlatStringReader::top_ = NULL;


FlatStringReader::FlatStringReader(Handle<String> str)
    : str_(str.location()),
      length_(str->length()),
      prev_(top_) {
  top_ = this;
  RefreshState();
}


FlatStringReader::FlatStringReader(Vector<const char> input)
    : str_(NULL),
      is_ascii_(true),
      length_(input.length()),
      start_(input.start()),
      prev_(top_) {
  top_ = this;
}


FlatStringReader::~FlatStringReader() {
  ASSERT_EQ(top_, this);
  top_ = prev_;
}


void FlatStringReader::RefreshState() {
  if (str_ == NULL) return;
  Handle<String> str(str_);
  ASSERT(str->IsFlat());
  is_ascii_ = str->IsAsciiRepresentation();
  if (is_ascii_) {
    start_ = str->ToAsciiVector().start();
  } else {
    start_ = str->ToUC16Vector().start();
  }
}


void FlatStringReader::PostGarbageCollectionProcessing() {
  FlatStringReader* current = top_;
  while (current != NULL) {
    current->RefreshState();
    current = current->prev_;
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
    case kSlicedStringTag:
      SlicedString::cast(input)->SlicedStringReadBlockIntoBuffer(rbb,
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


void SlicedString::SlicedStringReadBlockIntoBuffer(ReadBlockBuffer* rbb,
                                                   unsigned* offset_ptr,
                                                   unsigned max_chars) {
  String* backing = buffer();
  unsigned offset = start() + *offset_ptr;
  unsigned length = backing->length();
  if (max_chars > length - offset) {
    max_chars = length - offset;
  }
  String::ReadBlockIntoBuffer(backing, rbb, &offset, max_chars);
  *offset_ptr = offset - start();
}


void ConsString::ConsStringIterateBody(ObjectVisitor* v) {
  IteratePointers(v, kFirstOffset, kSecondOffset + kPointerSize);
}


void JSGlobalPropertyCell::JSGlobalPropertyCellIterateBody(ObjectVisitor* v) {
  IteratePointers(v, kValueOffset, kValueOffset + kPointerSize);
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
                  ExternalAsciiString::cast(source)->resource()->data() + from,
                  to - from);
        return;
      }
      case kTwoByteStringTag | kExternalStringTag: {
        const uc16* data =
            ExternalTwoByteString::cast(source)->resource()->data();
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
      case kAsciiStringTag | kSlicedStringTag:
      case kTwoByteStringTag | kSlicedStringTag: {
        SlicedString* sliced_string = SlicedString::cast(source);
        int start = sliced_string->start();
        from += start;
        to += start;
        source = String::cast(sliced_string->buffer());
        break;
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
            WriteToFlat(second,
                        sink + boundary - from,
                        0,
                        to - boundary);
            to = boundary;
          }
          source = first;
        }
        break;
      }
    }
  }
}


void SlicedString::SlicedStringIterateBody(ObjectVisitor* v) {
  IteratePointer(v, kBufferOffset);
}


uint16_t SlicedString::SlicedStringGet(int index) {
  ASSERT(index >= 0 && index < this->length());
  // Delegate to the buffer string.
  String* underlying = buffer();
  return underlying->Get(start() + index);
}


template <typename IteratorA, typename IteratorB>
static inline bool CompareStringContents(IteratorA* ia, IteratorB* ib) {
  // General slow case check.  We know that the ia and ib iterators
  // have the same length.
  while (ia->has_more()) {
    uc32 ca = ia->GetNext();
    uc32 cb = ib->GetNext();
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


static StringInputBuffer string_compare_buffer_b;


template <typename IteratorA>
static inline bool CompareStringContentsPartial(IteratorA* ia, String* b) {
  if (b->IsFlat()) {
    if (b->IsAsciiRepresentation()) {
      VectorIterator<char> ib(b->ToAsciiVector());
      return CompareStringContents(ia, &ib);
    } else {
      VectorIterator<uc16> ib(b->ToUC16Vector());
      return CompareStringContents(ia, &ib);
    }
  } else {
    string_compare_buffer_b.Reset(0, b);
    return CompareStringContents(ia, &string_compare_buffer_b);
  }
}


static StringInputBuffer string_compare_buffer_a;


bool String::SlowEquals(String* other) {
  // Fast check: negative check with lengths.
  int len = length();
  if (len != other->length()) return false;
  if (len == 0) return true;

  // Fast check: if hash code is computed for both strings
  // a fast negative check can be performed.
  if (HasHashCode() && other->HasHashCode()) {
    if (Hash() != other->Hash()) return false;
  }

  if (StringShape(this).IsSequentialAscii() &&
      StringShape(other).IsSequentialAscii()) {
    const char* str1 = SeqAsciiString::cast(this)->GetChars();
    const char* str2 = SeqAsciiString::cast(other)->GetChars();
    return CompareRawStringContents(Vector<const char>(str1, len),
                                    Vector<const char>(str2, len));
  }

  if (this->IsFlat()) {
    if (IsAsciiRepresentation()) {
      Vector<const char> vec1 = this->ToAsciiVector();
      if (other->IsFlat()) {
        if (other->IsAsciiRepresentation()) {
          Vector<const char> vec2 = other->ToAsciiVector();
          return CompareRawStringContents(vec1, vec2);
        } else {
          VectorIterator<char> buf1(vec1);
          VectorIterator<uc16> ib(other->ToUC16Vector());
          return CompareStringContents(&buf1, &ib);
        }
      } else {
        VectorIterator<char> buf1(vec1);
        string_compare_buffer_b.Reset(0, other);
        return CompareStringContents(&buf1, &string_compare_buffer_b);
      }
    } else {
      Vector<const uc16> vec1 = this->ToUC16Vector();
      if (other->IsFlat()) {
        if (other->IsAsciiRepresentation()) {
          VectorIterator<uc16> buf1(vec1);
          VectorIterator<char> ib(other->ToAsciiVector());
          return CompareStringContents(&buf1, &ib);
        } else {
          Vector<const uc16> vec2(other->ToUC16Vector());
          return CompareRawStringContents(vec1, vec2);
        }
      } else {
        VectorIterator<uc16> buf1(vec1);
        string_compare_buffer_b.Reset(0, other);
        return CompareStringContents(&buf1, &string_compare_buffer_b);
      }
    }
  } else {
    string_compare_buffer_a.Reset(0, this);
    return CompareStringContentsPartial(&string_compare_buffer_a, other);
  }
}


bool String::MarkAsUndetectable() {
  if (StringShape(this).IsSymbol()) return false;

  Map* map = this->map();
  if (map == Heap::short_string_map()) {
    this->set_map(Heap::undetectable_short_string_map());
    return true;
  } else if (map == Heap::medium_string_map()) {
    this->set_map(Heap::undetectable_medium_string_map());
    return true;
  } else if (map == Heap::long_string_map()) {
    this->set_map(Heap::undetectable_long_string_map());
    return true;
  } else if (map == Heap::short_ascii_string_map()) {
    this->set_map(Heap::undetectable_short_ascii_string_map());
    return true;
  } else if (map == Heap::medium_ascii_string_map()) {
    this->set_map(Heap::undetectable_medium_ascii_string_map());
    return true;
  } else if (map == Heap::long_ascii_string_map()) {
    this->set_map(Heap::undetectable_long_ascii_string_map());
    return true;
  }
  // Rest cannot be marked as undetectable
  return false;
}


bool String::IsEqualTo(Vector<const char> str) {
  int slen = length();
  Access<Scanner::Utf8Decoder> decoder(Scanner::utf8_decoder());
  decoder->Reset(str.start(), str.length());
  int i;
  for (i = 0; i < slen && decoder->has_more(); i++) {
    uc32 r = decoder->GetNext();
    if (Get(i) != r) return false;
  }
  return i == slen && !decoder->has_more();
}


uint32_t String::ComputeAndSetHash() {
  // Should only be called if hash code has not yet been computed.
  ASSERT(!(length_field() & kHashComputedMask));

  // Compute the hash code.
  StringInputBuffer buffer(this);
  uint32_t field = ComputeLengthAndHashField(&buffer, length());

  // Store the hash code in the object.
  set_length_field(field);

  // Check the hash code is there.
  ASSERT(length_field() & kHashComputedMask);
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
    uint32_t field = length_field();
    if ((field & kIsArrayIndexMask) == 0) return false;
    *index = (field & ((1 << kShortLengthShift) - 1)) >> kLongLengthShift;
    return true;
  } else {
    StringInputBuffer buffer(this);
    return ComputeArrayIndex(&buffer, index, length());
  }
}


static inline uint32_t HashField(uint32_t hash, bool is_array_index) {
  uint32_t result =
      (hash << String::kLongLengthShift) | String::kHashComputedMask;
  if (is_array_index) result |= String::kIsArrayIndexMask;
  return result;
}


uint32_t StringHasher::GetHashField() {
  ASSERT(is_valid());
  if (length_ <= String::kMaxShortStringSize) {
    uint32_t payload;
    if (is_array_index()) {
      payload = v8::internal::HashField(array_index(), true);
    } else {
      payload = v8::internal::HashField(GetHash(), false);
    }
    return (payload & ((1 << String::kShortLengthShift) - 1)) |
           (length_ << String::kShortLengthShift);
  } else if (length_ <= String::kMaxMediumStringSize) {
    uint32_t payload = v8::internal::HashField(GetHash(), false);
    return (payload & ((1 << String::kMediumLengthShift) - 1)) |
           (length_ << String::kMediumLengthShift);
  } else {
    return v8::internal::HashField(length_, false);
  }
}


uint32_t String::ComputeLengthAndHashField(unibrow::CharacterStream* buffer,
                                           int length) {
  StringHasher hasher(length);

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


Object* String::Slice(int start, int end) {
  if (start == 0 && end == length()) return this;
  if (StringShape(this).representation_tag() == kSlicedStringTag) {
    // Translate slices of a SlicedString into slices of the
    // underlying string buffer.
    SlicedString* str = SlicedString::cast(this);
    String* buf = str->buffer();
    return Heap::AllocateSlicedString(buf,
                                      str->start() + start,
                                      str->start() + end);
  }
  Object* result = Heap::AllocateSlicedString(this, start, end);
  if (result->IsFailure()) {
    return result;
  }
  // Due to the way we retry after GC on allocation failure we are not allowed
  // to fail on allocation after this point.  This is the one-allocation rule.

  // Try to flatten a cons string that is under the sliced string.
  // This is to avoid memory leaks and possible stack overflows caused by
  // building 'towers' of sliced strings on cons strings.
  // This may fail due to an allocation failure (when a GC is needed), but it
  // will succeed often enough to avoid the problem.  We only have to do this
  // if Heap::AllocateSlicedString actually returned a SlicedString.  It will
  // return flat strings for small slices for efficiency reasons.
  String* answer = String::cast(result);
  if (StringShape(answer).IsSliced() &&
      StringShape(this).representation_tag() == kConsStringTag) {
    TryFlatten();
    // If the flatten succeeded we might as well make the sliced string point
    // to the flat string rather than the cons string.
    String* second = ConsString::cast(this)->second();
    if (second->length() == 0) {
      SlicedString::cast(answer)->set_buffer(ConsString::cast(this)->first());
    }
  }
  return answer;
}


void String::PrintOn(FILE* file) {
  int length = this->length();
  for (int i = 0; i < length; i++) {
    fprintf(file, "%c", Get(i));
  }
}


void Map::CreateBackPointers() {
  DescriptorArray* descriptors = instance_descriptors();
  for (int i = 0; i < descriptors->number_of_descriptors(); i++) {
    if (descriptors->GetType(i) == MAP_TRANSITION) {
      // Get target.
      Map* target = Map::cast(descriptors->GetValue(i));
#ifdef DEBUG
      // Verify target.
      Object* source_prototype = prototype();
      Object* target_prototype = target->prototype();
      ASSERT(source_prototype->IsJSObject() ||
             source_prototype->IsMap() ||
             source_prototype->IsNull());
      ASSERT(target_prototype->IsJSObject() ||
             target_prototype->IsNull());
      ASSERT(source_prototype->IsMap() ||
             source_prototype == target_prototype);
#endif
      // Point target back to source.  set_prototype() will not let us set
      // the prototype to a map, as we do here.
      *RawField(target, kPrototypeOffset) = this;
    }
  }
}


void Map::ClearNonLiveTransitions(Object* real_prototype) {
  // Live DescriptorArray objects will be marked, so we must use
  // low-level accessors to get and modify their data.
  DescriptorArray* d = reinterpret_cast<DescriptorArray*>(
      *RawField(this, Map::kInstanceDescriptorsOffset));
  if (d == Heap::raw_unchecked_empty_descriptor_array()) return;
  Smi* NullDescriptorDetails =
    PropertyDetails(NONE, NULL_DESCRIPTOR).AsSmi();
  FixedArray* contents = reinterpret_cast<FixedArray*>(
      d->get(DescriptorArray::kContentArrayIndex));
  ASSERT(contents->length() >= 2);
  for (int i = 0; i < contents->length(); i += 2) {
    // If the pair (value, details) is a map transition,
    // check if the target is live.  If not, null the descriptor.
    // Also drop the back pointer for that map transition, so that this
    // map is not reached again by following a back pointer from a
    // non-live object.
    PropertyDetails details(Smi::cast(contents->get(i + 1)));
    if (details.type() == MAP_TRANSITION) {
      Map* target = reinterpret_cast<Map*>(contents->get(i));
      ASSERT(target->IsHeapObject());
      if (!target->IsMarked()) {
        ASSERT(target->IsMap());
        contents->set(i + 1, NullDescriptorDetails, SKIP_WRITE_BARRIER);
        contents->set(i, Heap::null_value(), SKIP_WRITE_BARRIER);
        ASSERT(target->prototype() == this ||
               target->prototype() == real_prototype);
        // Getter prototype() is read-only, set_prototype() has side effects.
        *RawField(target, Map::kPrototypeOffset) = real_prototype;
      }
    }
  }
}


void Map::MapIterateBody(ObjectVisitor* v) {
  // Assumes all Object* members are contiguously allocated!
  IteratePointers(v, kPrototypeOffset, kCodeCacheOffset + kPointerSize);
}


Object* JSFunction::SetInstancePrototype(Object* value) {
  ASSERT(value->IsJSObject());

  if (has_initial_map()) {
    initial_map()->set_prototype(value);
  } else {
    // Put the value in the initial map field until an initial map is
    // needed.  At that point, a new initial map is created and the
    // prototype is put into the initial map where it belongs.
    set_prototype_or_initial_map(value);
  }
  return value;
}



Object* JSFunction::SetPrototype(Object* value) {
  Object* construct_prototype = value;

  // If the value is not a JSObject, store the value in the map's
  // constructor field so it can be accessed.  Also, set the prototype
  // used for constructing objects to the original object prototype.
  // See ECMA-262 13.2.2.
  if (!value->IsJSObject()) {
    // Copy the map so this does not affect unrelated functions.
    // Remove map transitions because they point to maps with a
    // different prototype.
    Object* new_map = map()->CopyDropTransitions();
    if (new_map->IsFailure()) return new_map;
    set_map(Map::cast(new_map));
    map()->set_constructor(value);
    map()->set_non_instance_prototype(true);
    construct_prototype =
        Top::context()->global_context()->initial_object_prototype();
  } else {
    map()->set_non_instance_prototype(false);
  }

  return SetInstancePrototype(construct_prototype);
}


Object* JSFunction::SetInstanceClassName(String* name) {
  shared()->set_instance_class_name(name);
  return this;
}


Context* JSFunction::GlobalContextFromLiterals(FixedArray* literals) {
  return Context::cast(literals->get(JSFunction::kLiteralGlobalContextIndex));
}


void Oddball::OddballIterateBody(ObjectVisitor* v) {
  // Assumes all Object* members are contiguously allocated!
  IteratePointers(v, kToStringOffset, kToNumberOffset + kPointerSize);
}


Object* Oddball::Initialize(const char* to_string, Object* to_number) {
  Object* symbol = Heap::LookupAsciiSymbol(to_string);
  if (symbol->IsFailure()) return symbol;
  set_to_string(String::cast(symbol));
  set_to_number(to_number);
  return this;
}


bool SharedFunctionInfo::HasSourceCode() {
  return !script()->IsUndefined() &&
         !Script::cast(script())->source()->IsUndefined();
}


Object* SharedFunctionInfo::GetSourceCode() {
  HandleScope scope;
  if (script()->IsUndefined()) return Heap::undefined_value();
  Object* source = Script::cast(script())->source();
  if (source->IsUndefined()) return Heap::undefined_value();
  return *SubString(Handle<String>(String::cast(source)),
                    start_position(), end_position());
}


// Support function for printing the source code to a StringStream
// without any allocation in the heap.
void SharedFunctionInfo::SourceCodePrint(StringStream* accumulator,
                                         int max_length) {
  // For some native functions there is no source.
  if (script()->IsUndefined() ||
      Script::cast(script())->source()->IsUndefined()) {
    accumulator->Add("<No Source>");
    return;
  }

  // Get the slice of the source for this function.
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
  if (len > max_length) {
    accumulator->Put(script_source,
                     start_position(),
                     start_position() + max_length);
    accumulator->Add("...\n");
  } else {
    accumulator->Put(script_source, start_position(), end_position());
  }
}


void SharedFunctionInfo::SharedFunctionInfoIterateBody(ObjectVisitor* v) {
  IteratePointers(v, kNameOffset, kConstructStubOffset + kPointerSize);
  IteratePointers(v, kInstanceClassNameOffset, kScriptOffset + kPointerSize);
  IteratePointers(v, kDebugInfoOffset, kInferredNameOffset + kPointerSize);
}


void ObjectVisitor::BeginCodeIteration(Code* code) {
  ASSERT(code->ic_flag() == Code::IC_TARGET_IS_OBJECT);
}


void ObjectVisitor::VisitCodeTarget(RelocInfo* rinfo) {
  ASSERT(RelocInfo::IsCodeTarget(rinfo->rmode()));
  VisitPointer(rinfo->target_object_address());
}


void ObjectVisitor::VisitDebugTarget(RelocInfo* rinfo) {
  ASSERT(RelocInfo::IsJSReturn(rinfo->rmode()) && rinfo->IsCallInstruction());
  VisitPointer(rinfo->call_object_address());
}


// Convert relocatable targets from address to code object address. This is
// mainly IC call targets but for debugging straight-line code can be replaced
// with a call instruction which also has to be relocated.
void Code::ConvertICTargetsFromAddressToObject() {
  ASSERT(ic_flag() == IC_TARGET_IS_ADDRESS);

  for (RelocIterator it(this, RelocInfo::kCodeTargetMask);
       !it.done(); it.next()) {
    Address ic_addr = it.rinfo()->target_address();
    ASSERT(ic_addr != NULL);
    HeapObject* code = HeapObject::FromAddress(ic_addr - Code::kHeaderSize);
    ASSERT(code->IsHeapObject());
    it.rinfo()->set_target_object(code);
  }

#ifdef ENABLE_DEBUGGER_SUPPORT
  if (Debug::has_break_points()) {
    for (RelocIterator it(this, RelocInfo::ModeMask(RelocInfo::JS_RETURN));
         !it.done();
         it.next()) {
      if (it.rinfo()->IsCallInstruction()) {
        Address addr = it.rinfo()->call_address();
        ASSERT(addr != NULL);
        HeapObject* code = HeapObject::FromAddress(addr - Code::kHeaderSize);
        ASSERT(code->IsHeapObject());
        it.rinfo()->set_call_object(code);
      }
    }
  }
#endif
  set_ic_flag(IC_TARGET_IS_OBJECT);
}


void Code::CodeIterateBody(ObjectVisitor* v) {
  v->BeginCodeIteration(this);

  int mode_mask = RelocInfo::kCodeTargetMask |
                  RelocInfo::ModeMask(RelocInfo::EMBEDDED_OBJECT) |
                  RelocInfo::ModeMask(RelocInfo::EXTERNAL_REFERENCE) |
                  RelocInfo::ModeMask(RelocInfo::JS_RETURN) |
                  RelocInfo::ModeMask(RelocInfo::RUNTIME_ENTRY);

  for (RelocIterator it(this, mode_mask); !it.done(); it.next()) {
    RelocInfo::Mode rmode = it.rinfo()->rmode();
    if (rmode == RelocInfo::EMBEDDED_OBJECT) {
      v->VisitPointer(it.rinfo()->target_object_address());
    } else if (RelocInfo::IsCodeTarget(rmode)) {
      v->VisitCodeTarget(it.rinfo());
    } else if (rmode == RelocInfo::EXTERNAL_REFERENCE) {
      v->VisitExternalReference(it.rinfo()->target_reference_address());
#ifdef ENABLE_DEBUGGER_SUPPORT
    } else if (Debug::has_break_points() &&
               RelocInfo::IsJSReturn(rmode) &&
               it.rinfo()->IsCallInstruction()) {
      v->VisitDebugTarget(it.rinfo());
#endif
    } else if (rmode == RelocInfo::RUNTIME_ENTRY) {
      v->VisitRuntimeEntry(it.rinfo());
    }
  }

  ScopeInfo<>::IterateScopeInfo(this, v);

  v->EndCodeIteration(this);
}


void Code::ConvertICTargetsFromObjectToAddress() {
  ASSERT(ic_flag() == IC_TARGET_IS_OBJECT);

  for (RelocIterator it(this, RelocInfo::kCodeTargetMask);
       !it.done(); it.next()) {
    // We cannot use the safe cast (Code::cast) here, because we may be in
    // the middle of relocating old objects during GC and the map pointer in
    // the code object may be mangled
    Code* code = reinterpret_cast<Code*>(it.rinfo()->target_object());
    ASSERT((code != NULL) && code->IsHeapObject());
    it.rinfo()->set_target_address(code->instruction_start());
  }

#ifdef ENABLE_DEBUGGER_SUPPORT
  if (Debug::has_break_points()) {
    for (RelocIterator it(this, RelocInfo::ModeMask(RelocInfo::JS_RETURN));
         !it.done();
         it.next()) {
      if (it.rinfo()->IsCallInstruction()) {
        Code* code = reinterpret_cast<Code*>(it.rinfo()->call_object());
        ASSERT((code != NULL) && code->IsHeapObject());
        it.rinfo()->set_call_address(code->instruction_start());
      }
    }
  }
#endif
  set_ic_flag(IC_TARGET_IS_ADDRESS);
}


void Code::Relocate(int delta) {
  for (RelocIterator it(this, RelocInfo::kApplyMask); !it.done(); it.next()) {
    it.rinfo()->apply(delta);
  }
  CPU::FlushICache(instruction_start(), instruction_size());
}


void Code::CopyFrom(const CodeDesc& desc) {
  // copy code
  memmove(instruction_start(), desc.buffer, desc.instr_size);

  // fill gap with zero bytes
  { byte* p = instruction_start() + desc.instr_size;
    byte* q = relocation_start();
    while (p < q) {
      *p++ = 0;
    }
  }

  // copy reloc info
  memmove(relocation_start(),
          desc.buffer + desc.buffer_size - desc.reloc_size,
          desc.reloc_size);

  // unbox handles and relocate
  int delta = instruction_start() - desc.buffer;
  int mode_mask = RelocInfo::kCodeTargetMask |
                  RelocInfo::ModeMask(RelocInfo::EMBEDDED_OBJECT) |
                  RelocInfo::kApplyMask;
  for (RelocIterator it(this, mode_mask); !it.done(); it.next()) {
    RelocInfo::Mode mode = it.rinfo()->rmode();
    if (mode == RelocInfo::EMBEDDED_OBJECT) {
      Object** p = reinterpret_cast<Object**>(it.rinfo()->target_object());
      it.rinfo()->set_target_object(*p);
    } else if (RelocInfo::IsCodeTarget(mode)) {
      // rewrite code handles in inline cache targets to direct
      // pointers to the first instruction in the code object
      Object** p = reinterpret_cast<Object**>(it.rinfo()->target_object());
      Code* code = Code::cast(*p);
      it.rinfo()->set_target_address(code->instruction_start());
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
      int dist = pc - it.rinfo()->pc();
      int pos = it.rinfo()->data();
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
      int p = it.rinfo()->data();
      if (statement_position < p && p <= position) {
        statement_position = p;
      }
    }
    it.next();
  }
  return statement_position;
}


#ifdef ENABLE_DISASSEMBLER
// Identify kind of code.
const char* Code::Kind2String(Kind kind) {
  switch (kind) {
    case FUNCTION: return "FUNCTION";
    case STUB: return "STUB";
    case BUILTIN: return "BUILTIN";
    case LOAD_IC: return "LOAD_IC";
    case KEYED_LOAD_IC: return "KEYED_LOAD_IC";
    case STORE_IC: return "STORE_IC";
    case KEYED_STORE_IC: return "KEYED_STORE_IC";
    case CALL_IC: return "CALL_IC";
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
    case INTERCEPTOR: return "INTERCEPTOR";
    case MAP_TRANSITION: return "MAP_TRANSITION";
    case CONSTANT_TRANSITION: return "CONSTANT_TRANSITION";
    case NULL_DESCRIPTOR: return "NULL_DESCRIPTOR";
  }
  UNREACHABLE();
  return NULL;
}

void Code::Disassemble(const char* name) {
  PrintF("kind = %s\n", Kind2String(kind()));
  if (is_inline_cache_stub()) {
    PrintF("ic_state = %s\n", ICState2String(ic_state()));
    PrintF("ic_in_loop = %d\n", ic_in_loop() == IN_LOOP);
    if (ic_state() == MONOMORPHIC) {
      PrintF("type = %s\n", PropertyType2String(type()));
    }
  }
  if ((name != NULL) && (name[0] != '\0')) {
    PrintF("name = %s\n", name);
  }

  PrintF("Instructions (size = %d)\n", instruction_size());
  Disassembler::Decode(NULL, this);
  PrintF("\n");

  PrintF("RelocInfo (size = %d)\n", relocation_size());
  for (RelocIterator it(this); !it.done(); it.next())
    it.rinfo()->Print();
  PrintF("\n");
}
#endif  // ENABLE_DISASSEMBLER


void JSObject::SetFastElements(FixedArray* elems) {
  // We should never end in here with a pixel array.
  ASSERT(!HasPixelElements());
#ifdef DEBUG
  // Check the provided array is filled with the_hole.
  uint32_t len = static_cast<uint32_t>(elems->length());
  for (uint32_t i = 0; i < len; i++) ASSERT(elems->get(i)->IsTheHole());
#endif
  WriteBarrierMode mode = elems->GetWriteBarrierMode();
  switch (GetElementsKind()) {
    case FAST_ELEMENTS: {
      FixedArray* old_elements = FixedArray::cast(elements());
      uint32_t old_length = static_cast<uint32_t>(old_elements->length());
      // Fill out the new array with this content and array holes.
      for (uint32_t i = 0; i < old_length; i++) {
        elems->set(i, old_elements->get(i), mode);
      }
      break;
    }
    case DICTIONARY_ELEMENTS: {
      NumberDictionary* dictionary = NumberDictionary::cast(elements());
      for (int i = 0; i < dictionary->Capacity(); i++) {
        Object* key = dictionary->KeyAt(i);
        if (key->IsNumber()) {
          uint32_t entry = static_cast<uint32_t>(key->Number());
          elems->set(entry, dictionary->ValueAt(i), mode);
        }
      }
      break;
    }
    default:
      UNREACHABLE();
      break;
  }
  set_elements(elems);
}


Object* JSObject::SetSlowElements(Object* len) {
  // We should never end in here with a pixel array.
  ASSERT(!HasPixelElements());

  uint32_t new_length = static_cast<uint32_t>(len->Number());

  switch (GetElementsKind()) {
    case FAST_ELEMENTS: {
      // Make sure we never try to shrink dense arrays into sparse arrays.
      ASSERT(static_cast<uint32_t>(FixedArray::cast(elements())->length()) <=
                                   new_length);
      Object* obj = NormalizeElements();
      if (obj->IsFailure()) return obj;

      // Update length for JSArrays.
      if (IsJSArray()) JSArray::cast(this)->set_length(len);
      break;
    }
    case DICTIONARY_ELEMENTS: {
      if (IsJSArray()) {
        uint32_t old_length =
        static_cast<uint32_t>(JSArray::cast(this)->length()->Number());
        element_dictionary()->RemoveNumberEntries(new_length, old_length),
        JSArray::cast(this)->set_length(len);
      }
      break;
    }
    default:
      UNREACHABLE();
      break;
  }
  return this;
}


Object* JSArray::Initialize(int capacity) {
  ASSERT(capacity >= 0);
  set_length(Smi::FromInt(0), SKIP_WRITE_BARRIER);
  FixedArray* new_elements;
  if (capacity == 0) {
    new_elements = Heap::empty_fixed_array();
  } else {
    Object* obj = Heap::AllocateFixedArrayWithHoles(capacity);
    if (obj->IsFailure()) return obj;
    new_elements = FixedArray::cast(obj);
  }
  set_elements(new_elements);
  return this;
}


void JSArray::Expand(int required_size) {
  Handle<JSArray> self(this);
  Handle<FixedArray> old_backing(FixedArray::cast(elements()));
  int old_size = old_backing->length();
  // Doubling in size would be overkill, but leave some slack to avoid
  // constantly growing.
  int new_size = required_size + (required_size >> 3);
  Handle<FixedArray> new_backing = Factory::NewFixedArray(new_size);
  // Can't use this any more now because we may have had a GC!
  for (int i = 0; i < old_size; i++) new_backing->set(i, old_backing->get(i));
  self->SetContent(*new_backing);
}


// Computes the new capacity when expanding the elements of a JSObject.
static int NewElementsCapacity(int old_capacity) {
  // (old_capacity + 50%) + 16
  return old_capacity + (old_capacity >> 1) + 16;
}


static Object* ArrayLengthRangeError() {
  HandleScope scope;
  return Top::Throw(*Factory::NewRangeError("invalid_array_length",
                                            HandleVector<Object>(NULL, 0)));
}


Object* JSObject::SetElementsLength(Object* len) {
  // We should never end in here with a pixel array.
  ASSERT(!HasPixelElements());

  Object* smi_length = len->ToSmi();
  if (smi_length->IsSmi()) {
    int value = Smi::cast(smi_length)->value();
    if (value < 0) return ArrayLengthRangeError();
    switch (GetElementsKind()) {
      case FAST_ELEMENTS: {
        int old_capacity = FixedArray::cast(elements())->length();
        if (value <= old_capacity) {
          if (IsJSArray()) {
            int old_length = FastD2I(JSArray::cast(this)->length()->Number());
            // NOTE: We may be able to optimize this by removing the
            // last part of the elements backing storage array and
            // setting the capacity to the new size.
            for (int i = value; i < old_length; i++) {
              FixedArray::cast(elements())->set_the_hole(i);
            }
            JSArray::cast(this)->set_length(smi_length, SKIP_WRITE_BARRIER);
          }
          return this;
        }
        int min = NewElementsCapacity(old_capacity);
        int new_capacity = value > min ? value : min;
        if (new_capacity <= kMaxFastElementsLength ||
            !ShouldConvertToSlowElements(new_capacity)) {
          Object* obj = Heap::AllocateFixedArrayWithHoles(new_capacity);
          if (obj->IsFailure()) return obj;
          if (IsJSArray()) JSArray::cast(this)->set_length(smi_length,
                                                           SKIP_WRITE_BARRIER);
          SetFastElements(FixedArray::cast(obj));
          return this;
        }
        break;
      }
      case DICTIONARY_ELEMENTS: {
        if (IsJSArray()) {
          if (value == 0) {
            // If the length of a slow array is reset to zero, we clear
            // the array and flush backing storage. This has the added
            // benefit that the array returns to fast mode.
            initialize_elements();
          } else {
            // Remove deleted elements.
            uint32_t old_length =
            static_cast<uint32_t>(JSArray::cast(this)->length()->Number());
            element_dictionary()->RemoveNumberEntries(value, old_length);
          }
          JSArray::cast(this)->set_length(smi_length, SKIP_WRITE_BARRIER);
        }
        return this;
      }
      default:
        UNREACHABLE();
        break;
    }
  }

  // General slow case.
  if (len->IsNumber()) {
    uint32_t length;
    if (Array::IndexFromObject(len, &length)) {
      return SetSlowElements(len);
    } else {
      return ArrayLengthRangeError();
    }
  }

  // len is not a number so make the array size one and
  // set only element to len.
  Object* obj = Heap::AllocateFixedArray(1);
  if (obj->IsFailure()) return obj;
  FixedArray::cast(obj)->set(0, len);
  if (IsJSArray()) JSArray::cast(this)->set_length(Smi::FromInt(1),
                                                   SKIP_WRITE_BARRIER);
  set_elements(FixedArray::cast(obj));
  return this;
}


bool JSObject::HasElementPostInterceptor(JSObject* receiver, uint32_t index) {
  switch (GetElementsKind()) {
    case FAST_ELEMENTS: {
      uint32_t length = IsJSArray() ?
          static_cast<uint32_t>
              (Smi::cast(JSArray::cast(this)->length())->value()) :
          static_cast<uint32_t>(FixedArray::cast(elements())->length());
      if ((index < length) &&
          !FixedArray::cast(elements())->get(index)->IsTheHole()) {
        return true;
      }
      break;
    }
    case PIXEL_ELEMENTS: {
      // TODO(iposva): Add testcase.
      PixelArray* pixels = PixelArray::cast(elements());
      if (index < static_cast<uint32_t>(pixels->length())) {
        return true;
      }
      break;
    }
    case DICTIONARY_ELEMENTS: {
      if (element_dictionary()->FindEntry(index)
          != NumberDictionary::kNotFound) {
        return true;
      }
      break;
    }
    default:
      UNREACHABLE();
      break;
  }

  // Handle [] on String objects.
  if (this->IsStringObjectWithCharacterAt(index)) return true;

  Object* pt = GetPrototype();
  if (pt == Heap::null_value()) return false;
  return JSObject::cast(pt)->HasElementWithReceiver(receiver, index);
}


bool JSObject::HasElementWithInterceptor(JSObject* receiver, uint32_t index) {
  // Make sure that the top context does not change when doing
  // callbacks or interceptor calls.
  AssertNoContextChange ncc;
  HandleScope scope;
  Handle<InterceptorInfo> interceptor(GetIndexedInterceptor());
  Handle<JSObject> receiver_handle(receiver);
  Handle<JSObject> holder_handle(this);
  Handle<Object> data_handle(interceptor->data());
  v8::AccessorInfo info(v8::Utils::ToLocal(receiver_handle),
                        v8::Utils::ToLocal(data_handle),
                        v8::Utils::ToLocal(holder_handle));
  if (!interceptor->query()->IsUndefined()) {
    v8::IndexedPropertyQuery query =
        v8::ToCData<v8::IndexedPropertyQuery>(interceptor->query());
    LOG(ApiIndexedPropertyAccess("interceptor-indexed-has", this, index));
    v8::Handle<v8::Boolean> result;
    {
      // Leaving JavaScript.
      VMState state(EXTERNAL);
      result = query(index, info);
    }
    if (!result.IsEmpty()) return result->IsTrue();
  } else if (!interceptor->getter()->IsUndefined()) {
    v8::IndexedPropertyGetter getter =
        v8::ToCData<v8::IndexedPropertyGetter>(interceptor->getter());
    LOG(ApiIndexedPropertyAccess("interceptor-indexed-has-get", this, index));
    v8::Handle<v8::Value> result;
    {
      // Leaving JavaScript.
      VMState state(EXTERNAL);
      result = getter(index, info);
    }
    if (!result.IsEmpty()) return true;
  }
  return holder_handle->HasElementPostInterceptor(*receiver_handle, index);
}


bool JSObject::HasLocalElement(uint32_t index) {
  // Check access rights if needed.
  if (IsAccessCheckNeeded() &&
      !Top::MayIndexedAccess(this, index, v8::ACCESS_HAS)) {
    Top::ReportFailedAccessCheck(this, v8::ACCESS_HAS);
    return false;
  }

  // Check for lookup interceptor
  if (HasIndexedInterceptor()) {
    return HasElementWithInterceptor(this, index);
  }

  // Handle [] on String objects.
  if (this->IsStringObjectWithCharacterAt(index)) return true;

  switch (GetElementsKind()) {
    case FAST_ELEMENTS: {
      uint32_t length = IsJSArray() ?
          static_cast<uint32_t>
              (Smi::cast(JSArray::cast(this)->length())->value()) :
          static_cast<uint32_t>(FixedArray::cast(elements())->length());
      return (index < length) &&
          !FixedArray::cast(elements())->get(index)->IsTheHole();
    }
    case PIXEL_ELEMENTS: {
      PixelArray* pixels = PixelArray::cast(elements());
      return (index < static_cast<uint32_t>(pixels->length()));
    }
    case DICTIONARY_ELEMENTS: {
      return element_dictionary()->FindEntry(index)
          != NumberDictionary::kNotFound;
    }
    default:
      UNREACHABLE();
      break;
  }
  UNREACHABLE();
  return Heap::null_value();
}


bool JSObject::HasElementWithReceiver(JSObject* receiver, uint32_t index) {
  // Check access rights if needed.
  if (IsAccessCheckNeeded() &&
      !Top::MayIndexedAccess(this, index, v8::ACCESS_HAS)) {
    Top::ReportFailedAccessCheck(this, v8::ACCESS_HAS);
    return false;
  }

  // Check for lookup interceptor
  if (HasIndexedInterceptor()) {
    return HasElementWithInterceptor(receiver, index);
  }

  switch (GetElementsKind()) {
    case FAST_ELEMENTS: {
      uint32_t length = IsJSArray() ?
          static_cast<uint32_t>
              (Smi::cast(JSArray::cast(this)->length())->value()) :
          static_cast<uint32_t>(FixedArray::cast(elements())->length());
      if ((index < length) &&
          !FixedArray::cast(elements())->get(index)->IsTheHole()) return true;
      break;
    }
    case PIXEL_ELEMENTS: {
      PixelArray* pixels = PixelArray::cast(elements());
      if (index < static_cast<uint32_t>(pixels->length())) {
        return true;
      }
      break;
    }
    case DICTIONARY_ELEMENTS: {
      if (element_dictionary()->FindEntry(index)
          != NumberDictionary::kNotFound) {
        return true;
      }
      break;
    }
    default:
      UNREACHABLE();
      break;
  }

  // Handle [] on String objects.
  if (this->IsStringObjectWithCharacterAt(index)) return true;

  Object* pt = GetPrototype();
  if (pt == Heap::null_value()) return false;
  return JSObject::cast(pt)->HasElementWithReceiver(receiver, index);
}


Object* JSObject::SetElementWithInterceptor(uint32_t index, Object* value) {
  // Make sure that the top context does not change when doing
  // callbacks or interceptor calls.
  AssertNoContextChange ncc;
  HandleScope scope;
  Handle<InterceptorInfo> interceptor(GetIndexedInterceptor());
  Handle<JSObject> this_handle(this);
  Handle<Object> value_handle(value);
  if (!interceptor->setter()->IsUndefined()) {
    v8::IndexedPropertySetter setter =
        v8::ToCData<v8::IndexedPropertySetter>(interceptor->setter());
    Handle<Object> data_handle(interceptor->data());
    LOG(ApiIndexedPropertyAccess("interceptor-indexed-set", this, index));
    v8::AccessorInfo info(v8::Utils::ToLocal(this_handle),
                          v8::Utils::ToLocal(data_handle),
                          v8::Utils::ToLocal(this_handle));
    v8::Handle<v8::Value> result;
    {
      // Leaving JavaScript.
      VMState state(EXTERNAL);
      result = setter(index, v8::Utils::ToLocal(value_handle), info);
    }
    RETURN_IF_SCHEDULED_EXCEPTION();
    if (!result.IsEmpty()) return *value_handle;
  }
  Object* raw_result =
      this_handle->SetElementWithoutInterceptor(index, *value_handle);
  RETURN_IF_SCHEDULED_EXCEPTION();
  return raw_result;
}


// Adding n elements in fast case is O(n*n).
// Note: revisit design to have dual undefined values to capture absent
// elements.
Object* JSObject::SetFastElement(uint32_t index, Object* value) {
  ASSERT(HasFastElements());

  FixedArray* elms = FixedArray::cast(elements());
  uint32_t elms_length = static_cast<uint32_t>(elms->length());

  if (!IsJSArray() && (index >= elms_length || elms->get(index)->IsTheHole())) {
    Object* setter = LookupCallbackSetterInPrototypes(index);
    if (setter->IsJSFunction()) {
      return SetPropertyWithDefinedSetter(JSFunction::cast(setter), value);
    }
  }

  // Check whether there is extra space in fixed array..
  if (index < elms_length) {
    elms->set(index, value);
    if (IsJSArray()) {
      // Update the length of the array if needed.
      uint32_t array_length = 0;
      CHECK(Array::IndexFromObject(JSArray::cast(this)->length(),
                                   &array_length));
      if (index >= array_length) {
        JSArray::cast(this)->set_length(Smi::FromInt(index + 1),
                                        SKIP_WRITE_BARRIER);
      }
    }
    return value;
  }

  // Allow gap in fast case.
  if ((index - elms_length) < kMaxGap) {
    // Try allocating extra space.
    int new_capacity = NewElementsCapacity(index+1);
    if (new_capacity <= kMaxFastElementsLength ||
        !ShouldConvertToSlowElements(new_capacity)) {
      ASSERT(static_cast<uint32_t>(new_capacity) > index);
      Object* obj = Heap::AllocateFixedArrayWithHoles(new_capacity);
      if (obj->IsFailure()) return obj;
      SetFastElements(FixedArray::cast(obj));
      if (IsJSArray()) JSArray::cast(this)->set_length(Smi::FromInt(index + 1),
                                                       SKIP_WRITE_BARRIER);
      FixedArray::cast(elements())->set(index, value);
      return value;
    }
  }

  // Otherwise default to slow case.
  Object* obj = NormalizeElements();
  if (obj->IsFailure()) return obj;
  ASSERT(HasDictionaryElements());
  return SetElement(index, value);
}

Object* JSObject::SetElement(uint32_t index, Object* value) {
  // Check access rights if needed.
  if (IsAccessCheckNeeded() &&
      !Top::MayIndexedAccess(this, index, v8::ACCESS_SET)) {
    Top::ReportFailedAccessCheck(this, v8::ACCESS_SET);
    return value;
  }

  if (IsJSGlobalProxy()) {
    Object* proto = GetPrototype();
    if (proto->IsNull()) return value;
    ASSERT(proto->IsJSGlobalObject());
    return JSObject::cast(proto)->SetElement(index, value);
  }

  // Check for lookup interceptor
  if (HasIndexedInterceptor()) {
    return SetElementWithInterceptor(index, value);
  }

  return SetElementWithoutInterceptor(index, value);
}


Object* JSObject::SetElementWithoutInterceptor(uint32_t index, Object* value) {
  switch (GetElementsKind()) {
    case FAST_ELEMENTS:
      // Fast case.
      return SetFastElement(index, value);
    case PIXEL_ELEMENTS: {
      PixelArray* pixels = PixelArray::cast(elements());
      return pixels->SetValue(index, value);
    }
    case DICTIONARY_ELEMENTS: {
      // Insert element in the dictionary.
      FixedArray* elms = FixedArray::cast(elements());
      NumberDictionary* dictionary = NumberDictionary::cast(elms);

      int entry = dictionary->FindEntry(index);
      if (entry != NumberDictionary::kNotFound) {
        Object* element = dictionary->ValueAt(entry);
        PropertyDetails details = dictionary->DetailsAt(entry);
        if (details.type() == CALLBACKS) {
          // Only accessors allowed as elements.
          FixedArray* structure = FixedArray::cast(element);
          if (structure->get(kSetterIndex)->IsJSFunction()) {
            JSFunction* setter = JSFunction::cast(structure->get(kSetterIndex));
            return SetPropertyWithDefinedSetter(setter, value);
          } else {
            Handle<Object> self(this);
            Handle<Object> key(Factory::NewNumberFromUint(index));
            Handle<Object> args[2] = { key, self };
            return Top::Throw(*Factory::NewTypeError("no_setter_in_callback",
                                                     HandleVector(args, 2)));
          }
        } else {
          dictionary->UpdateMaxNumberKey(index);
          dictionary->ValueAtPut(entry, value);
        }
      } else {
        // Index not already used. Look for an accessor in the prototype chain.
        if (!IsJSArray()) {
          Object* setter = LookupCallbackSetterInPrototypes(index);
          if (setter->IsJSFunction()) {
            return SetPropertyWithDefinedSetter(JSFunction::cast(setter),
                                                value);
          }
        }
        Object* result = dictionary->AtNumberPut(index, value);
        if (result->IsFailure()) return result;
        if (elms != FixedArray::cast(result)) {
          set_elements(FixedArray::cast(result));
        }
      }

      // Update the array length if this JSObject is an array.
      if (IsJSArray()) {
        JSArray* array = JSArray::cast(this);
        Object* return_value = array->JSArrayUpdateLengthFromIndex(index,
                                                                   value);
        if (return_value->IsFailure()) return return_value;
      }

      // Attempt to put this object back in fast case.
      if (ShouldConvertToFastElements()) {
        uint32_t new_length = 0;
        if (IsJSArray()) {
          CHECK(Array::IndexFromObject(JSArray::cast(this)->length(),
                                       &new_length));
          JSArray::cast(this)->set_length(Smi::FromInt(new_length));
        } else {
          new_length = NumberDictionary::cast(elements())->max_number_key() + 1;
        }
        Object* obj = Heap::AllocateFixedArrayWithHoles(new_length);
        if (obj->IsFailure()) return obj;
        SetFastElements(FixedArray::cast(obj));
#ifdef DEBUG
        if (FLAG_trace_normalization) {
          PrintF("Object elements are fast case again:\n");
          Print();
        }
#endif
      }

      return value;
    }
    default:
      UNREACHABLE();
      break;
  }
  // All possible cases have been handled above. Add a return to avoid the
  // complaints from the compiler.
  UNREACHABLE();
  return Heap::null_value();
}


Object* JSArray::JSArrayUpdateLengthFromIndex(uint32_t index, Object* value) {
  uint32_t old_len = 0;
  CHECK(Array::IndexFromObject(length(), &old_len));
  // Check to see if we need to update the length. For now, we make
  // sure that the length stays within 32-bits (unsigned).
  if (index >= old_len && index != 0xffffffff) {
    Object* len =
        Heap::NumberFromDouble(static_cast<double>(index) + 1);
    if (len->IsFailure()) return len;
    set_length(len);
  }
  return value;
}


Object* JSObject::GetElementPostInterceptor(JSObject* receiver,
                                            uint32_t index) {
  // Get element works for both JSObject and JSArray since
  // JSArray::length cannot change.
  switch (GetElementsKind()) {
    case FAST_ELEMENTS: {
      FixedArray* elms = FixedArray::cast(elements());
      if (index < static_cast<uint32_t>(elms->length())) {
        Object* value = elms->get(index);
        if (!value->IsTheHole()) return value;
      }
      break;
    }
    case PIXEL_ELEMENTS: {
      // TODO(iposva): Add testcase and implement.
      UNIMPLEMENTED();
      break;
    }
    case DICTIONARY_ELEMENTS: {
      NumberDictionary* dictionary = element_dictionary();
      int entry = dictionary->FindEntry(index);
      if (entry != NumberDictionary::kNotFound) {
        Object* element = dictionary->ValueAt(entry);
        PropertyDetails details = dictionary->DetailsAt(entry);
        if (details.type() == CALLBACKS) {
          // Only accessors allowed as elements.
          FixedArray* structure = FixedArray::cast(element);
          Object* getter = structure->get(kGetterIndex);
          if (getter->IsJSFunction()) {
            return GetPropertyWithDefinedGetter(receiver,
                                                JSFunction::cast(getter));
          } else {
            // Getter is not a function.
            return Heap::undefined_value();
          }
        }
        return element;
      }
      break;
    }
    default:
      UNREACHABLE();
      break;
  }

  // Continue searching via the prototype chain.
  Object* pt = GetPrototype();
  if (pt == Heap::null_value()) return Heap::undefined_value();
  return pt->GetElementWithReceiver(receiver, index);
}


Object* JSObject::GetElementWithInterceptor(JSObject* receiver,
                                            uint32_t index) {
  // Make sure that the top context does not change when doing
  // callbacks or interceptor calls.
  AssertNoContextChange ncc;
  HandleScope scope;
  Handle<InterceptorInfo> interceptor(GetIndexedInterceptor());
  Handle<JSObject> this_handle(receiver);
  Handle<JSObject> holder_handle(this);

  if (!interceptor->getter()->IsUndefined()) {
    Handle<Object> data_handle(interceptor->data());
    v8::IndexedPropertyGetter getter =
        v8::ToCData<v8::IndexedPropertyGetter>(interceptor->getter());
    LOG(ApiIndexedPropertyAccess("interceptor-indexed-get", this, index));
    v8::AccessorInfo info(v8::Utils::ToLocal(this_handle),
                          v8::Utils::ToLocal(data_handle),
                          v8::Utils::ToLocal(holder_handle));
    v8::Handle<v8::Value> result;
    {
      // Leaving JavaScript.
      VMState state(EXTERNAL);
      result = getter(index, info);
    }
    RETURN_IF_SCHEDULED_EXCEPTION();
    if (!result.IsEmpty()) return *v8::Utils::OpenHandle(*result);
  }

  Object* raw_result =
      holder_handle->GetElementPostInterceptor(*this_handle, index);
  RETURN_IF_SCHEDULED_EXCEPTION();
  return raw_result;
}


Object* JSObject::GetElementWithReceiver(JSObject* receiver, uint32_t index) {
  // Check access rights if needed.
  if (IsAccessCheckNeeded() &&
      !Top::MayIndexedAccess(this, index, v8::ACCESS_GET)) {
    Top::ReportFailedAccessCheck(this, v8::ACCESS_GET);
    return Heap::undefined_value();
  }

  if (HasIndexedInterceptor()) {
    return GetElementWithInterceptor(receiver, index);
  }

  // Get element works for both JSObject and JSArray since
  // JSArray::length cannot change.
  switch (GetElementsKind()) {
    case FAST_ELEMENTS: {
      FixedArray* elms = FixedArray::cast(elements());
      if (index < static_cast<uint32_t>(elms->length())) {
        Object* value = elms->get(index);
        if (!value->IsTheHole()) return value;
      }
      break;
    }
    case PIXEL_ELEMENTS: {
      PixelArray* pixels = PixelArray::cast(elements());
      if (index < static_cast<uint32_t>(pixels->length())) {
        uint8_t value = pixels->get(index);
        return Smi::FromInt(value);
      }
      break;
    }
    case DICTIONARY_ELEMENTS: {
      NumberDictionary* dictionary = element_dictionary();
      int entry = dictionary->FindEntry(index);
      if (entry != NumberDictionary::kNotFound) {
        Object* element = dictionary->ValueAt(entry);
        PropertyDetails details = dictionary->DetailsAt(entry);
        if (details.type() == CALLBACKS) {
          // Only accessors allowed as elements.
          FixedArray* structure = FixedArray::cast(element);
          Object* getter = structure->get(kGetterIndex);
          if (getter->IsJSFunction()) {
            return GetPropertyWithDefinedGetter(receiver,
                                                JSFunction::cast(getter));
          } else {
            // Getter is not a function.
            return Heap::undefined_value();
          }
        }
        return element;
      }
      break;
    }
  }

  Object* pt = GetPrototype();
  if (pt == Heap::null_value()) return Heap::undefined_value();
  return pt->GetElementWithReceiver(receiver, index);
}


bool JSObject::HasDenseElements() {
  int capacity = 0;
  int number_of_elements = 0;

  switch (GetElementsKind()) {
    case FAST_ELEMENTS: {
      FixedArray* elms = FixedArray::cast(elements());
      capacity = elms->length();
      for (int i = 0; i < capacity; i++) {
        if (!elms->get(i)->IsTheHole()) number_of_elements++;
      }
      break;
    }
    case PIXEL_ELEMENTS: {
      return true;
    }
    case DICTIONARY_ELEMENTS: {
      NumberDictionary* dictionary = NumberDictionary::cast(elements());
      capacity = dictionary->Capacity();
      number_of_elements = dictionary->NumberOfElements();
      break;
    }
    default:
      UNREACHABLE();
      break;
  }

  if (capacity == 0) return true;
  return (number_of_elements > (capacity / 2));
}


bool JSObject::ShouldConvertToSlowElements(int new_capacity) {
  ASSERT(HasFastElements());
  // Keep the array in fast case if the current backing storage is
  // almost filled and if the new capacity is no more than twice the
  // old capacity.
  int elements_length = FixedArray::cast(elements())->length();
  return !HasDenseElements() || ((new_capacity / 2) > elements_length);
}


bool JSObject::ShouldConvertToFastElements() {
  ASSERT(HasDictionaryElements());
  NumberDictionary* dictionary = NumberDictionary::cast(elements());
  // If the elements are sparse, we should not go back to fast case.
  if (!HasDenseElements()) return false;
  // If an element has been added at a very high index in the elements
  // dictionary, we cannot go back to fast case.
  if (dictionary->requires_slow_elements()) return false;
  // An object requiring access checks is never allowed to have fast
  // elements.  If it had fast elements we would skip security checks.
  if (IsAccessCheckNeeded()) return false;
  // If the dictionary backing storage takes up roughly half as much
  // space as a fast-case backing storage would the array should have
  // fast elements.
  uint32_t length = 0;
  if (IsJSArray()) {
    CHECK(Array::IndexFromObject(JSArray::cast(this)->length(), &length));
  } else {
    length = dictionary->max_number_key();
  }
  return static_cast<uint32_t>(dictionary->Capacity()) >=
      (length / (2 * NumberDictionary::kEntrySize));
}


// Certain compilers request function template instantiation when they
// see the definition of the other template functions in the
// class. This requires us to have the template functions put
// together, so even though this function belongs in objects-debug.cc,
// we keep it here instead to satisfy certain compilers.
#ifdef DEBUG
template<typename Shape, typename Key>
void Dictionary<Shape, Key>::Print() {
  int capacity = HashTable<Shape, Key>::Capacity();
  for (int i = 0; i < capacity; i++) {
    Object* k = HashTable<Shape, Key>::KeyAt(i);
    if (HashTable<Shape, Key>::IsKey(k)) {
      PrintF(" ");
      if (k->IsString()) {
        String::cast(k)->StringPrint();
      } else {
        k->ShortPrint();
      }
      PrintF(": ");
      ValueAt(i)->ShortPrint();
      PrintF("\n");
    }
  }
}
#endif


template<typename Shape, typename Key>
void Dictionary<Shape, Key>::CopyValuesTo(FixedArray* elements) {
  int pos = 0;
  int capacity = HashTable<Shape, Key>::Capacity();
  WriteBarrierMode mode = elements->GetWriteBarrierMode();
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
  Object* template_info = constructor->shared()->function_data();
  Object* result =
      FunctionTemplateInfo::cast(template_info)->named_property_handler();
  return InterceptorInfo::cast(result);
}


InterceptorInfo* JSObject::GetIndexedInterceptor() {
  ASSERT(map()->has_indexed_interceptor());
  JSFunction* constructor = JSFunction::cast(map()->constructor());
  Object* template_info = constructor->shared()->function_data();
  Object* result =
      FunctionTemplateInfo::cast(template_info)->indexed_property_handler();
  return InterceptorInfo::cast(result);
}


Object* JSObject::GetPropertyPostInterceptor(JSObject* receiver,
                                             String* name,
                                             PropertyAttributes* attributes) {
  // Check local property in holder, ignore interceptor.
  LookupResult result;
  LocalLookupRealNamedProperty(name, &result);
  if (result.IsValid()) return GetProperty(receiver, &result, name, attributes);
  // Continue searching via the prototype chain.
  Object* pt = GetPrototype();
  *attributes = ABSENT;
  if (pt == Heap::null_value()) return Heap::undefined_value();
  return pt->GetPropertyWithReceiver(receiver, name, attributes);
}


Object* JSObject::GetPropertyWithInterceptor(
    JSObject* receiver,
    String* name,
    PropertyAttributes* attributes) {
  InterceptorInfo* interceptor = GetNamedInterceptor();
  HandleScope scope;
  Handle<JSObject> receiver_handle(receiver);
  Handle<JSObject> holder_handle(this);
  Handle<String> name_handle(name);
  Handle<Object> data_handle(interceptor->data());

  if (!interceptor->getter()->IsUndefined()) {
    v8::NamedPropertyGetter getter =
        v8::ToCData<v8::NamedPropertyGetter>(interceptor->getter());
    LOG(ApiNamedPropertyAccess("interceptor-named-get", *holder_handle, name));
    v8::AccessorInfo info(v8::Utils::ToLocal(receiver_handle),
                          v8::Utils::ToLocal(data_handle),
                          v8::Utils::ToLocal(holder_handle));
    v8::Handle<v8::Value> result;
    {
      // Leaving JavaScript.
      VMState state(EXTERNAL);
      result = getter(v8::Utils::ToLocal(name_handle), info);
    }
    RETURN_IF_SCHEDULED_EXCEPTION();
    if (!result.IsEmpty()) {
      *attributes = NONE;
      return *v8::Utils::OpenHandle(*result);
    }
  }

  Object* result = holder_handle->GetPropertyPostInterceptor(
      *receiver_handle,
      *name_handle,
      attributes);
  RETURN_IF_SCHEDULED_EXCEPTION();
  return result;
}


bool JSObject::HasRealNamedProperty(String* key) {
  // Check access rights if needed.
  if (IsAccessCheckNeeded() &&
      !Top::MayNamedAccess(this, key, v8::ACCESS_HAS)) {
    Top::ReportFailedAccessCheck(this, v8::ACCESS_HAS);
    return false;
  }

  LookupResult result;
  LocalLookupRealNamedProperty(key, &result);
  if (result.IsValid()) {
    switch (result.type()) {
      case NORMAL:    // fall through.
      case FIELD:     // fall through.
      case CALLBACKS:  // fall through.
      case CONSTANT_FUNCTION:
        return true;
      case INTERCEPTOR:
      case MAP_TRANSITION:
      case CONSTANT_TRANSITION:
      case NULL_DESCRIPTOR:
        return false;
      default:
        UNREACHABLE();
    }
  }

  return false;
}


bool JSObject::HasRealElementProperty(uint32_t index) {
  // Check access rights if needed.
  if (IsAccessCheckNeeded() &&
      !Top::MayIndexedAccess(this, index, v8::ACCESS_HAS)) {
    Top::ReportFailedAccessCheck(this, v8::ACCESS_HAS);
    return false;
  }

  // Handle [] on String objects.
  if (this->IsStringObjectWithCharacterAt(index)) return true;

  switch (GetElementsKind()) {
    case FAST_ELEMENTS: {
      uint32_t length = IsJSArray() ?
          static_cast<uint32_t>(
              Smi::cast(JSArray::cast(this)->length())->value()) :
          static_cast<uint32_t>(FixedArray::cast(elements())->length());
      return (index < length) &&
          !FixedArray::cast(elements())->get(index)->IsTheHole();
    }
    case PIXEL_ELEMENTS: {
      PixelArray* pixels = PixelArray::cast(elements());
      return index < static_cast<uint32_t>(pixels->length());
    }
    case DICTIONARY_ELEMENTS: {
      return element_dictionary()->FindEntry(index)
          != NumberDictionary::kNotFound;
    }
    default:
      UNREACHABLE();
      break;
  }
  // All possibilities have been handled above already.
  UNREACHABLE();
  return Heap::null_value();
}


bool JSObject::HasRealNamedCallbackProperty(String* key) {
  // Check access rights if needed.
  if (IsAccessCheckNeeded() &&
      !Top::MayNamedAccess(this, key, v8::ACCESS_HAS)) {
    Top::ReportFailedAccessCheck(this, v8::ACCESS_HAS);
    return false;
  }

  LookupResult result;
  LocalLookupRealNamedProperty(key, &result);
  return result.IsValid() && (result.type() == CALLBACKS);
}


int JSObject::NumberOfLocalProperties(PropertyAttributes filter) {
  if (HasFastProperties()) {
    DescriptorArray* descs = map()->instance_descriptors();
    int result = 0;
    for (int i = 0; i < descs->number_of_descriptors(); i++) {
      PropertyDetails details = descs->GetDetails(i);
      if (details.IsProperty() && (details.attributes() & filter) == 0) {
        result++;
      }
    }
    return result;
  } else {
    return property_dictionary()->NumberOfElementsFilterAttributes(filter);
  }
}


int JSObject::NumberOfEnumProperties() {
  return NumberOfLocalProperties(static_cast<PropertyAttributes>(DONT_ENUM));
}


void FixedArray::SwapPairs(FixedArray* numbers, int i, int j) {
  Object* temp = get(i);
  set(i, get(j));
  set(j, temp);
  if (this != numbers) {
    temp = numbers->get(i);
    numbers->set(i, numbers->get(j));
    numbers->set(j, temp);
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
  ASSERT(storage->length() >= (NumberOfLocalProperties(NONE) - index));
  if (HasFastProperties()) {
    DescriptorArray* descs = map()->instance_descriptors();
    for (int i = 0; i < descs->number_of_descriptors(); i++) {
      if (descs->IsProperty(i)) storage->set(index++, descs->GetKey(i));
    }
    ASSERT(storage->length() >= index);
  } else {
    property_dictionary()->CopyKeysTo(storage);
  }
}


int JSObject::NumberOfLocalElements(PropertyAttributes filter) {
  return GetLocalElementKeys(NULL, filter);
}


int JSObject::NumberOfEnumElements() {
  return NumberOfLocalElements(static_cast<PropertyAttributes>(DONT_ENUM));
}


int JSObject::GetLocalElementKeys(FixedArray* storage,
                                  PropertyAttributes filter) {
  int counter = 0;
  switch (GetElementsKind()) {
    case FAST_ELEMENTS: {
      int length = IsJSArray() ?
          Smi::cast(JSArray::cast(this)->length())->value() :
          FixedArray::cast(elements())->length();
      for (int i = 0; i < length; i++) {
        if (!FixedArray::cast(elements())->get(i)->IsTheHole()) {
          if (storage != NULL) {
            storage->set(counter, Smi::FromInt(i), SKIP_WRITE_BARRIER);
          }
          counter++;
        }
      }
      ASSERT(!storage || storage->length() >= counter);
      break;
    }
    case PIXEL_ELEMENTS: {
      int length = PixelArray::cast(elements())->length();
      while (counter < length) {
        if (storage != NULL) {
          storage->set(counter, Smi::FromInt(counter), SKIP_WRITE_BARRIER);
        }
        counter++;
      }
      ASSERT(!storage || storage->length() >= counter);
      break;
    }
    case DICTIONARY_ELEMENTS: {
      if (storage != NULL) {
        element_dictionary()->CopyKeysTo(storage, filter);
      }
      counter = element_dictionary()->NumberOfElementsFilterAttributes(filter);
      break;
    }
    default:
      UNREACHABLE();
      break;
  }

  if (this->IsJSValue()) {
    Object* val = JSValue::cast(this)->value();
    if (val->IsString()) {
      String* str = String::cast(val);
      if (storage) {
        for (int i = 0; i < str->length(); i++) {
          storage->set(counter + i, Smi::FromInt(i), SKIP_WRITE_BARRIER);
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


bool NumberDictionaryShape::IsMatch(uint32_t key, Object* other) {
  ASSERT(other->IsNumber());
  return key == static_cast<uint32_t>(other->Number());
}


uint32_t NumberDictionaryShape::Hash(uint32_t key) {
  return ComputeIntegerHash(key);
}


uint32_t NumberDictionaryShape::HashForObject(uint32_t key, Object* other) {
  ASSERT(other->IsNumber());
  return ComputeIntegerHash(static_cast<uint32_t>(other->Number()));
}


Object* NumberDictionaryShape::AsObject(uint32_t key) {
  return Heap::NumberFromUint32(key);
}


bool StringDictionaryShape::IsMatch(String* key, Object* other) {
  // We know that all entries in a hash table had their hash keys created.
  // Use that knowledge to have fast failure.
  if (key->Hash() != String::cast(other)->Hash()) return false;
  return key->Equals(String::cast(other));
}


uint32_t StringDictionaryShape::Hash(String* key) {
  return key->Hash();
}


uint32_t StringDictionaryShape::HashForObject(String* key, Object* other) {
  return String::cast(other)->Hash();
}


Object* StringDictionaryShape::AsObject(String* key) {
  return key;
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
  StringSharedKey(String* source, SharedFunctionInfo* shared)
      : source_(source), shared_(shared) { }

  bool IsMatch(Object* other) {
    if (!other->IsFixedArray()) return false;
    FixedArray* pair = FixedArray::cast(other);
    SharedFunctionInfo* shared = SharedFunctionInfo::cast(pair->get(0));
    if (shared != shared_) return false;
    String* source = String::cast(pair->get(1));
    return source->Equals(source_);
  }

  static uint32_t StringSharedHashHelper(String* source,
                                         SharedFunctionInfo* shared) {
    uint32_t hash = source->Hash();
    if (shared->HasSourceCode()) {
      // Instead of using the SharedFunctionInfo pointer in the hash
      // code computation, we use a combination of the hash of the
      // script source code and the start and end positions.  We do
      // this to ensure that the cache entries can survive garbage
      // collection.
      Script* script = Script::cast(shared->script());
      hash ^= String::cast(script->source())->Hash();
      hash += shared->start_position();
    }
    return hash;
  }

  uint32_t Hash() {
    return StringSharedHashHelper(source_, shared_);
  }

  uint32_t HashForObject(Object* obj) {
    FixedArray* pair = FixedArray::cast(obj);
    SharedFunctionInfo* shared = SharedFunctionInfo::cast(pair->get(0));
    String* source = String::cast(pair->get(1));
    return StringSharedHashHelper(source, shared);
  }

  Object* AsObject() {
    Object* obj = Heap::AllocateFixedArray(2);
    if (obj->IsFailure()) return obj;
    FixedArray* pair = FixedArray::cast(obj);
    pair->set(0, shared_);
    pair->set(1, source_);
    return pair;
  }

 private:
  String* source_;
  SharedFunctionInfo* shared_;
};


// RegExpKey carries the source and flags of a regular expression as key.
class RegExpKey : public HashTableKey {
 public:
  RegExpKey(String* string, JSRegExp::Flags flags)
      : string_(string),
        flags_(Smi::FromInt(flags.value())) { }

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
  explicit Utf8SymbolKey(Vector<const char> string)
      : string_(string), length_field_(0) { }

  bool IsMatch(Object* string) {
    return String::cast(string)->IsEqualTo(string_);
  }

  uint32_t Hash() {
    if (length_field_ != 0) return length_field_ >> String::kHashShift;
    unibrow::Utf8InputBuffer<> buffer(string_.start(),
                                      static_cast<unsigned>(string_.length()));
    chars_ = buffer.Length();
    length_field_ = String::ComputeLengthAndHashField(&buffer, chars_);
    uint32_t result = length_field_ >> String::kHashShift;
    ASSERT(result != 0);  // Ensure that the hash value of 0 is never computed.
    return result;
  }

  uint32_t HashForObject(Object* other) {
    return String::cast(other)->Hash();
  }

  Object* AsObject() {
    if (length_field_ == 0) Hash();
    return Heap::AllocateSymbol(string_, chars_, length_field_);
  }

  Vector<const char> string_;
  uint32_t length_field_;
  int chars_;  // Caches the number of characters when computing the hash code.
};


// SymbolKey carries a string/symbol object as key.
class SymbolKey : public HashTableKey {
 public:
  explicit SymbolKey(String* string) : string_(string) { }

  bool IsMatch(Object* string) {
    return String::cast(string)->Equals(string_);
  }

  uint32_t Hash() { return string_->Hash(); }

  uint32_t HashForObject(Object* other) {
    return String::cast(other)->Hash();
  }

  Object* AsObject() {
    // If the string is a cons string, attempt to flatten it so that
    // symbols will most often be flat strings.
    if (StringShape(string_).IsCons()) {
      ConsString* cons_string = ConsString::cast(string_);
      cons_string->TryFlatten();
      if (cons_string->second()->length() == 0) {
        string_ = cons_string->first();
      }
    }
    // Transform string to symbol if possible.
    Map* map = Heap::SymbolMapForString(string_);
    if (map != NULL) {
      string_->set_map(map);
      ASSERT(string_->IsSymbol());
      return string_;
    }
    // Otherwise allocate a new symbol.
    StringInputBuffer buffer(string_);
    return Heap::AllocateInternalSymbol(&buffer,
                                        string_->length(),
                                        string_->length_field());
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
Object* HashTable<Shape, Key>::Allocate(
    int at_least_space_for) {
  int capacity = RoundUpToPowerOf2(at_least_space_for);
  if (capacity < 4) capacity = 4;  // Guarantee min capacity.
  Object* obj = Heap::AllocateHashTable(EntryToIndex(capacity));
  if (!obj->IsFailure()) {
    HashTable::cast(obj)->SetNumberOfElements(0);
    HashTable::cast(obj)->SetCapacity(capacity);
  }
  return obj;
}



// Find entry for key otherwise return -1.
template<typename Shape, typename Key>
int HashTable<Shape, Key>::FindEntry(Key key) {
  uint32_t nof = NumberOfElements();
  if (nof == 0) return kNotFound;  // Bail out if empty.

  uint32_t capacity = Capacity();
  uint32_t hash = Shape::Hash(key);
  uint32_t entry = GetProbe(hash, 0, capacity);

  Object* element = KeyAt(entry);
  uint32_t passed_elements = 0;
  if (!element->IsNull()) {
    if (!element->IsUndefined() && Shape::IsMatch(key, element)) return entry;
    if (++passed_elements == nof) return kNotFound;
  }
  for (uint32_t i = 1; !element->IsUndefined(); i++) {
    entry = GetProbe(hash, i, capacity);
    element = KeyAt(entry);
    if (!element->IsNull()) {
      if (!element->IsUndefined() && Shape::IsMatch(key, element)) return entry;
      if (++passed_elements == nof) return kNotFound;
    }
  }
  return kNotFound;
}


template<typename Shape, typename Key>
Object* HashTable<Shape, Key>::EnsureCapacity(int n, Key key) {
  int capacity = Capacity();
  int nof = NumberOfElements() + n;
  // Make sure 50% is free
  if (nof + (nof >> 1) <= capacity) return this;

  Object* obj = Allocate(nof * 2);
  if (obj->IsFailure()) return obj;
  HashTable* table = HashTable::cast(obj);
  WriteBarrierMode mode = table->GetWriteBarrierMode();

  // Copy prefix to new array.
  for (int i = kPrefixStartIndex;
       i < kPrefixStartIndex + Shape::kPrefixSize;
       i++) {
    table->set(i, get(i), mode);
  }
  // Rehash the elements.
  for (int i = 0; i < capacity; i++) {
    uint32_t from_index = EntryToIndex(i);
    Object* k = get(from_index);
    if (IsKey(k)) {
      uint32_t hash = Shape::HashForObject(key, k);
      uint32_t insertion_index =
          EntryToIndex(table->FindInsertionEntry(hash));
      for (int j = 0; j < Shape::kEntrySize; j++) {
        table->set(insertion_index + j, get(from_index + j), mode);
      }
    }
  }
  table->SetNumberOfElements(NumberOfElements());
  return table;
}


template<typename Shape, typename Key>
uint32_t HashTable<Shape, Key>::FindInsertionEntry(uint32_t hash) {
  uint32_t capacity = Capacity();
  uint32_t entry = GetProbe(hash, 0, capacity);
  Object* element = KeyAt(entry);

  for (uint32_t i = 1; !(element->IsUndefined() || element->IsNull()); i++) {
    entry = GetProbe(hash, i, capacity);
    element = KeyAt(entry);
  }

  return entry;
}

// Force instantiation of template instances class.
// Please note this list is compiler dependent.

template class HashTable<SymbolTableShape, HashTableKey*>;

template class HashTable<CompilationCacheShape, HashTableKey*>;

template class HashTable<MapCacheShape, HashTableKey*>;

template class Dictionary<StringDictionaryShape, String*>;

template class Dictionary<NumberDictionaryShape, uint32_t>;

template Object* Dictionary<NumberDictionaryShape, uint32_t>::Allocate(
    int);

template Object* Dictionary<StringDictionaryShape, String*>::Allocate(
    int);

template Object* Dictionary<NumberDictionaryShape, uint32_t>::AtPut(
    uint32_t, Object*);

template Object* Dictionary<NumberDictionaryShape, uint32_t>::SlowReverseLookup(
    Object*);

template Object* Dictionary<StringDictionaryShape, String*>::SlowReverseLookup(
    Object*);

template void Dictionary<NumberDictionaryShape, uint32_t>::CopyKeysTo(
    FixedArray*, PropertyAttributes);

template Object* Dictionary<StringDictionaryShape, String*>::DeleteProperty(
    int, JSObject::DeleteMode);

template Object* Dictionary<NumberDictionaryShape, uint32_t>::DeleteProperty(
    int, JSObject::DeleteMode);

template void Dictionary<StringDictionaryShape, String*>::CopyKeysTo(
    FixedArray*);

template int
Dictionary<StringDictionaryShape, String*>::NumberOfElementsFilterAttributes(
    PropertyAttributes);

template Object* Dictionary<StringDictionaryShape, String*>::Add(
    String*, Object*, PropertyDetails);

template Object*
Dictionary<StringDictionaryShape, String*>::GenerateNewEnumerationIndices();

template int
Dictionary<NumberDictionaryShape, uint32_t>::NumberOfElementsFilterAttributes(
    PropertyAttributes);

template Object* Dictionary<NumberDictionaryShape, uint32_t>::Add(
    uint32_t, Object*, PropertyDetails);

template Object* Dictionary<NumberDictionaryShape, uint32_t>::EnsureCapacity(
    int, uint32_t);

template Object* Dictionary<StringDictionaryShape, String*>::EnsureCapacity(
    int, String*);

template Object* Dictionary<NumberDictionaryShape, uint32_t>::AddEntry(
    uint32_t, Object*, PropertyDetails, uint32_t);

template Object* Dictionary<StringDictionaryShape, String*>::AddEntry(
    String*, Object*, PropertyDetails, uint32_t);

template
int Dictionary<NumberDictionaryShape, uint32_t>::NumberOfEnumElements();

template
int Dictionary<StringDictionaryShape, String*>::NumberOfEnumElements();

// Collates undefined and unexisting elements below limit from position
// zero of the elements. The object stays in Dictionary mode.
Object* JSObject::PrepareSlowElementsForSort(uint32_t limit) {
  ASSERT(HasDictionaryElements());
  // Must stay in dictionary mode, either because of requires_slow_elements,
  // or because we are not going to sort (and therefore compact) all of the
  // elements.
  NumberDictionary* dict = element_dictionary();
  HeapNumber* result_double = NULL;
  if (limit > static_cast<uint32_t>(Smi::kMaxValue)) {
    // Allocate space for result before we start mutating the object.
    Object* new_double = Heap::AllocateHeapNumber(0.0);
    if (new_double->IsFailure()) return new_double;
    result_double = HeapNumber::cast(new_double);
  }

  int capacity = dict->Capacity();
  Object* obj = NumberDictionary::Allocate(dict->Capacity());
  if (obj->IsFailure()) return obj;
  NumberDictionary* new_dict = NumberDictionary::cast(obj);

  AssertNoAllocation no_alloc;

  uint32_t pos = 0;
  uint32_t undefs = 0;
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
      if (key < limit) {
        if (value->IsUndefined()) {
          undefs++;
        } else {
          new_dict->AddNumberEntry(pos, value, details);
          pos++;
        }
      } else {
        new_dict->AddNumberEntry(key, value, details);
      }
    }
  }

  uint32_t result = pos;
  PropertyDetails no_details = PropertyDetails(NONE, NORMAL);
  while (undefs > 0) {
    new_dict->AddNumberEntry(pos, Heap::undefined_value(), no_details);
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
Object* JSObject::PrepareElementsForSort(uint32_t limit) {
  ASSERT(!HasPixelElements());

  if (HasDictionaryElements()) {
    // Convert to fast elements containing only the existing properties.
    // Ordering is irrelevant, since we are going to sort anyway.
    NumberDictionary* dict = element_dictionary();
    if (IsJSArray() || dict->requires_slow_elements() ||
        dict->max_number_key() >= limit) {
      return PrepareSlowElementsForSort(limit);
    }
    // Convert to fast elements.

    PretenureFlag tenure = Heap::InNewSpace(this) ? NOT_TENURED: TENURED;
    Object* new_array =
        Heap::AllocateFixedArray(dict->NumberOfElements(), tenure);
    if (new_array->IsFailure()) {
      return new_array;
    }
    FixedArray* fast_elements = FixedArray::cast(new_array);
    dict->CopyValuesTo(fast_elements);
    set_elements(fast_elements);
  }
  ASSERT(HasFastElements());

  // Collect holes at the end, undefined before that and the rest at the
  // start, and return the number of non-hole, non-undefined values.

  FixedArray* elements = FixedArray::cast(this->elements());
  uint32_t elements_length = static_cast<uint32_t>(elements->length());
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
    Object* new_double = Heap::AllocateHeapNumber(0.0);
    if (new_double->IsFailure()) return new_double;
    result_double = HeapNumber::cast(new_double);
  }

  AssertNoAllocation no_alloc;

  // Split elements into defined, undefined and the_hole, in that order.
  // Only count locations for undefined and the hole, and fill them afterwards.
  WriteBarrierMode write_barrier = elements->GetWriteBarrierMode();
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
  uint32_t result = undefs;
  while (undefs < holes) {
    elements->set_undefined(undefs);
    undefs++;
  }
  while (holes < limit) {
    elements->set_the_hole(holes);
    holes++;
  }

  if (result <= static_cast<uint32_t>(Smi::kMaxValue)) {
    return Smi::FromInt(static_cast<int>(result));
  }
  ASSERT_NE(NULL, result_double);
  result_double->set_value(static_cast<double>(result));
  return result_double;
}


Object* PixelArray::SetValue(uint32_t index, Object* value) {
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


Object* GlobalObject::GetPropertyCell(LookupResult* result) {
  ASSERT(!HasFastProperties());
  Object* value = property_dictionary()->ValueAt(result->GetDictionaryEntry());
  ASSERT(value->IsJSGlobalPropertyCell());
  return value;
}


Object* GlobalObject::EnsurePropertyCell(String* name) {
  ASSERT(!HasFastProperties());
  int entry = property_dictionary()->FindEntry(name);
  if (entry == StringDictionary::kNotFound) {
    Object* cell = Heap::AllocateJSGlobalPropertyCell(Heap::the_hole_value());
    if (cell->IsFailure()) return cell;
    PropertyDetails details(NONE, NORMAL);
    details = details.AsDeleted();
    Object* dictionary = property_dictionary()->Add(name, cell, details);
    if (dictionary->IsFailure()) return dictionary;
    set_properties(StringDictionary::cast(dictionary));
    return cell;
  } else {
    Object* value = property_dictionary()->ValueAt(entry);
    ASSERT(value->IsJSGlobalPropertyCell());
    return value;
  }
}


Object* SymbolTable::LookupString(String* string, Object** s) {
  SymbolKey key(string);
  return LookupKey(&key, s);
}


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


Object* SymbolTable::LookupSymbol(Vector<const char> str, Object** s) {
  Utf8SymbolKey key(str);
  return LookupKey(&key, s);
}


Object* SymbolTable::LookupKey(HashTableKey* key, Object** s) {
  int entry = FindEntry(key);

  // Symbol already in table.
  if (entry != kNotFound) {
    *s = KeyAt(entry);
    return this;
  }

  // Adding new symbol. Grow table if needed.
  Object* obj = EnsureCapacity(1, key);
  if (obj->IsFailure()) return obj;

  // Create symbol object.
  Object* symbol = key->AsObject();
  if (symbol->IsFailure()) return symbol;

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
  if (entry == kNotFound) return Heap::undefined_value();
  return get(EntryToIndex(entry) + 1);
}


Object* CompilationCacheTable::LookupEval(String* src, Context* context) {
  StringSharedKey key(src, context->closure()->shared());
  int entry = FindEntry(&key);
  if (entry == kNotFound) return Heap::undefined_value();
  return get(EntryToIndex(entry) + 1);
}


Object* CompilationCacheTable::LookupRegExp(String* src,
                                            JSRegExp::Flags flags) {
  RegExpKey key(src, flags);
  int entry = FindEntry(&key);
  if (entry == kNotFound) return Heap::undefined_value();
  return get(EntryToIndex(entry) + 1);
}


Object* CompilationCacheTable::Put(String* src, Object* value) {
  StringKey key(src);
  Object* obj = EnsureCapacity(1, &key);
  if (obj->IsFailure()) return obj;

  CompilationCacheTable* cache =
      reinterpret_cast<CompilationCacheTable*>(obj);
  int entry = cache->FindInsertionEntry(key.Hash());
  cache->set(EntryToIndex(entry), src);
  cache->set(EntryToIndex(entry) + 1, value);
  cache->ElementAdded();
  return cache;
}


Object* CompilationCacheTable::PutEval(String* src,
                                       Context* context,
                                       Object* value) {
  StringSharedKey key(src, context->closure()->shared());
  Object* obj = EnsureCapacity(1, &key);
  if (obj->IsFailure()) return obj;

  CompilationCacheTable* cache =
      reinterpret_cast<CompilationCacheTable*>(obj);
  int entry = cache->FindInsertionEntry(key.Hash());

  Object* k = key.AsObject();
  if (k->IsFailure()) return k;

  cache->set(EntryToIndex(entry), k);
  cache->set(EntryToIndex(entry) + 1, value);
  cache->ElementAdded();
  return cache;
}


Object* CompilationCacheTable::PutRegExp(String* src,
                                         JSRegExp::Flags flags,
                                         FixedArray* value) {
  RegExpKey key(src, flags);
  Object* obj = EnsureCapacity(1, &key);
  if (obj->IsFailure()) return obj;

  CompilationCacheTable* cache =
      reinterpret_cast<CompilationCacheTable*>(obj);
  int entry = cache->FindInsertionEntry(key.Hash());
  cache->set(EntryToIndex(entry), value);
  cache->set(EntryToIndex(entry) + 1, value);
  cache->ElementAdded();
  return cache;
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
  if (entry == kNotFound) return Heap::undefined_value();
  return get(EntryToIndex(entry) + 1);
}


Object* MapCache::Put(FixedArray* array, Map* value) {
  SymbolsKey key(array);
  Object* obj = EnsureCapacity(1, &key);
  if (obj->IsFailure()) return obj;

  MapCache* cache = reinterpret_cast<MapCache*>(obj);
  int entry = cache->FindInsertionEntry(key.Hash());
  cache->set(EntryToIndex(entry), array);
  cache->set(EntryToIndex(entry) + 1, value);
  cache->ElementAdded();
  return cache;
}


template<typename Shape, typename Key>
Object* Dictionary<Shape, Key>::Allocate(int at_least_space_for) {
  Object* obj = HashTable<Shape, Key>::Allocate(at_least_space_for);
  // Initialize the next enumeration index.
  if (!obj->IsFailure()) {
    Dictionary<Shape, Key>::cast(obj)->
        SetNextEnumerationIndex(PropertyDetails::kInitialIndex);
  }
  return obj;
}


template<typename Shape, typename Key>
Object* Dictionary<Shape, Key>::GenerateNewEnumerationIndices() {
  int length = HashTable<Shape, Key>::NumberOfElements();

  // Allocate and initialize iteration order array.
  Object* obj = Heap::AllocateFixedArray(length);
  if (obj->IsFailure()) return obj;
  FixedArray* iteration_order = FixedArray::cast(obj);
  for (int i = 0; i < length; i++) {
    iteration_order->set(i, Smi::FromInt(i), SKIP_WRITE_BARRIER);
  }

  // Allocate array with enumeration order.
  obj = Heap::AllocateFixedArray(length);
  if (obj->IsFailure()) return obj;
  FixedArray* enumeration_order = FixedArray::cast(obj);

  // Fill the enumeration order array with property details.
  int capacity = HashTable<Shape, Key>::Capacity();
  int pos = 0;
  for (int i = 0; i < capacity; i++) {
    if (Dictionary<Shape, Key>::IsKey(Dictionary<Shape, Key>::KeyAt(i))) {
      enumeration_order->set(pos++,
                             Smi::FromInt(DetailsAt(i).index()),
                             SKIP_WRITE_BARRIER);
    }
  }

  // Sort the arrays wrt. enumeration order.
  iteration_order->SortPairs(enumeration_order, enumeration_order->length());

  // Overwrite the enumeration_order with the enumeration indices.
  for (int i = 0; i < length; i++) {
    int index = Smi::cast(iteration_order->get(i))->value();
    int enum_index = PropertyDetails::kInitialIndex + i;
    enumeration_order->set(index,
                           Smi::FromInt(enum_index),
                           SKIP_WRITE_BARRIER);
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
Object* Dictionary<Shape, Key>::EnsureCapacity(int n, Key key) {
  // Check whether there are enough enumeration indices to add n elements.
  if (Shape::kIsEnumerable &&
      !PropertyDetails::IsValidIndex(NextEnumerationIndex() + n)) {
    // If not, we generate new indices for the properties.
    Object* result = GenerateNewEnumerationIndices();
    if (result->IsFailure()) return result;
  }
  return HashTable<Shape, Key>::EnsureCapacity(n, key);
}


void NumberDictionary::RemoveNumberEntries(uint32_t from, uint32_t to) {
  // Do nothing if the interval [from, to) is empty.
  if (from >= to) return;

  int removed_entries = 0;
  Object* sentinel = Heap::null_value();
  int capacity = Capacity();
  for (int i = 0; i < capacity; i++) {
    Object* key = KeyAt(i);
    if (key->IsNumber()) {
      uint32_t number = static_cast<uint32_t>(key->Number());
      if (from <= number && number < to) {
        SetEntry(i, sentinel, sentinel, Smi::FromInt(0));
        removed_entries++;
      }
    }
  }

  // Update the number of elements.
  SetNumberOfElements(NumberOfElements() - removed_entries);
}


template<typename Shape, typename Key>
Object* Dictionary<Shape, Key>::DeleteProperty(int entry,
                                               JSObject::DeleteMode mode) {
  PropertyDetails details = DetailsAt(entry);
  // Ignore attributes if forcing a deletion.
  if (details.IsDontDelete() && mode == JSObject::NORMAL_DELETION) {
    return Heap::false_value();
  }
  SetEntry(entry, Heap::null_value(), Heap::null_value(), Smi::FromInt(0));
  HashTable<Shape, Key>::ElementRemoved();
  return Heap::true_value();
}


template<typename Shape, typename Key>
Object* Dictionary<Shape, Key>::AtPut(Key key, Object* value) {
  int entry = FindEntry(key);

  // If the entry is present set the value;
  if (entry != Dictionary<Shape, Key>::kNotFound) {
    ValueAtPut(entry, value);
    return this;
  }

  // Check whether the dictionary should be extended.
  Object* obj = EnsureCapacity(1, key);
  if (obj->IsFailure()) return obj;

  Object* k = Shape::AsObject(key);
  if (k->IsFailure()) return k;
  PropertyDetails details = PropertyDetails(NONE, NORMAL);
  return Dictionary<Shape, Key>::cast(obj)->
      AddEntry(key, value, details, Shape::Hash(key));
}


template<typename Shape, typename Key>
Object* Dictionary<Shape, Key>::Add(Key key,
                                    Object* value,
                                    PropertyDetails details) {
  // Valdate key is absent.
  SLOW_ASSERT((FindEntry(key) == Dictionary<Shape, Key>::kNotFound));
  // Check whether the dictionary should be extended.
  Object* obj = EnsureCapacity(1, key);
  if (obj->IsFailure()) return obj;
  return Dictionary<Shape, Key>::cast(obj)->
      AddEntry(key, value, details, Shape::Hash(key));
}


// Add a key, value pair to the dictionary.
template<typename Shape, typename Key>
Object* Dictionary<Shape, Key>::AddEntry(Key key,
                                         Object* value,
                                         PropertyDetails details,
                                         uint32_t hash) {
  // Compute the key object.
  Object* k = Shape::AsObject(key);
  if (k->IsFailure()) return k;

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


void NumberDictionary::UpdateMaxNumberKey(uint32_t key) {
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
                    Smi::FromInt(key << kRequiresSlowElementsTagSize),
                    SKIP_WRITE_BARRIER);
  }
}


Object* NumberDictionary::AddNumberEntry(uint32_t key,
                                         Object* value,
                                         PropertyDetails details) {
  UpdateMaxNumberKey(key);
  SLOW_ASSERT(FindEntry(key) == kNotFound);
  return Add(key, value, details);
}


Object* NumberDictionary::AtNumberPut(uint32_t key, Object* value) {
  UpdateMaxNumberKey(key);
  return AtPut(key, value);
}


Object* NumberDictionary::Set(uint32_t key,
                              Object* value,
                              PropertyDetails details) {
  int entry = FindEntry(key);
  if (entry == kNotFound) return AddNumberEntry(key, value, details);
  // Preserve enumeration index.
  details = PropertyDetails(details.attributes(),
                            details.type(),
                            DetailsAt(entry).index());
  SetEntry(entry, NumberDictionaryShape::AsObject(key), value, details);
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
void Dictionary<Shape, Key>::CopyKeysTo(FixedArray* storage,
                                        PropertyAttributes filter) {
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
  storage->SortPairs(storage, index);
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
       sort_array->set(index,
                       Smi::FromInt(details.index()),
                       SKIP_WRITE_BARRIER);
       index++;
     }
  }
  storage->SortPairs(sort_array, sort_array->length());
  ASSERT(storage->length() >= index);
}


template<typename Shape, typename Key>
void Dictionary<Shape, Key>::CopyKeysTo(FixedArray* storage) {
  ASSERT(storage->length() >= NumberOfElementsFilterAttributes(
      static_cast<PropertyAttributes>(NONE)));
  int capacity = HashTable<Shape, Key>::Capacity();
  int index = 0;
  for (int i = 0; i < capacity; i++) {
    Object* k = HashTable<Shape, Key>::KeyAt(i);
    if (HashTable<Shape, Key>::IsKey(k)) {
      PropertyDetails details = DetailsAt(i);
      if (details.IsDeleted()) continue;
      storage->set(index++, k);
    }
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
  return Heap::undefined_value();
}


Object* StringDictionary::TransformPropertiesToFastFor(
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
    Object* result = GenerateNewEnumerationIndices();
    if (result->IsFailure()) return result;
  }

  int instance_descriptor_length = 0;
  int number_of_fields = 0;

  // Compute the length of the instance descriptor.
  int capacity = Capacity();
  for (int i = 0; i < capacity; i++) {
    Object* k = KeyAt(i);
    if (IsKey(k)) {
      Object* value = ValueAt(i);
      PropertyType type = DetailsAt(i).type();
      ASSERT(type != FIELD);
      instance_descriptor_length++;
      if (type == NORMAL && !value->IsJSFunction()) number_of_fields += 1;
    }
  }

  // Allocate the instance descriptor.
  Object* descriptors_unchecked =
      DescriptorArray::Allocate(instance_descriptor_length);
  if (descriptors_unchecked->IsFailure()) return descriptors_unchecked;
  DescriptorArray* descriptors = DescriptorArray::cast(descriptors_unchecked);

  int inobject_props = obj->map()->inobject_properties();
  int number_of_allocated_fields =
      number_of_fields + unused_property_fields - inobject_props;

  // Allocate the fixed array for the fields.
  Object* fields = Heap::AllocateFixedArray(number_of_allocated_fields);
  if (fields->IsFailure()) return fields;

  // Fill in the instance descriptor and the fields.
  int next_descriptor = 0;
  int current_offset = 0;
  for (int i = 0; i < capacity; i++) {
    Object* k = KeyAt(i);
    if (IsKey(k)) {
      Object* value = ValueAt(i);
      // Ensure the key is a symbol before writing into the instance descriptor.
      Object* key = Heap::LookupSymbol(String::cast(k));
      if (key->IsFailure()) return key;
      PropertyDetails details = DetailsAt(i);
      PropertyType type = details.type();

      if (value->IsJSFunction()) {
        ConstantFunctionDescriptor d(String::cast(key),
                                     JSFunction::cast(value),
                                     details.attributes(),
                                     details.index());
        descriptors->Set(next_descriptor++, &d);
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
        descriptors->Set(next_descriptor++, &d);
      } else if (type == CALLBACKS) {
        CallbacksDescriptor d(String::cast(key),
                              value,
                              details.attributes(),
                              details.index());
        descriptors->Set(next_descriptor++, &d);
      } else {
        UNREACHABLE();
      }
    }
  }
  ASSERT(current_offset == number_of_fields);

  descriptors->Sort();
  // Allocate new map.
  Object* new_map = obj->map()->CopyDropDescriptors();
  if (new_map->IsFailure()) return new_map;

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
  if (index == kNoBreakPointInfo) return Heap::undefined_value();
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
    debug_info->set_break_points(*Factory::NewFixedArray(
        old_break_points->length() +
            Debug::kEstimatedNofBreakPointsInFunction));
    Handle<FixedArray> new_break_points =
        Handle<FixedArray>(FixedArray::cast(debug_info->break_points()));
    for (int i = 0; i < old_break_points->length(); i++) {
      new_break_points->set(i, old_break_points->get(i));
    }
    index = old_break_points->length();
  }
  ASSERT(index != kNoBreakPointInfo);

  // Allocate new BreakPointInfo object and set the break point.
  Handle<BreakPointInfo> new_break_point_info =
      Handle<BreakPointInfo>::cast(Factory::NewStruct(BREAK_POINT_INFO_TYPE));
  new_break_point_info->set_code_position(Smi::FromInt(code_position));
  new_break_point_info->set_source_position(Smi::FromInt(source_position));
  new_break_point_info->
      set_statement_position(Smi::FromInt(statement_position));
  new_break_point_info->set_break_point_objects(Heap::undefined_value());
  BreakPointInfo::SetBreakPoint(new_break_point_info, break_point_object);
  debug_info->break_points()->set(index, *new_break_point_info);
}


// Get the break point objects for a code position.
Object* DebugInfo::GetBreakPointObjects(int code_position) {
  Object* break_point_info = GetBreakPointInfo(code_position);
  if (break_point_info->IsUndefined()) {
    return Heap::undefined_value();
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
  if (debug_info->break_points()->IsUndefined()) return Heap::undefined_value();
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
  return Heap::undefined_value();
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
  // If there are no break points just ignore.
  if (break_point_info->break_point_objects()->IsUndefined()) return;
  // If there is a single break point clear it if it is the same.
  if (!break_point_info->break_point_objects()->IsFixedArray()) {
    if (break_point_info->break_point_objects() == *break_point_object) {
      break_point_info->set_break_point_objects(Heap::undefined_value());
    }
    return;
  }
  // If there are multiple break points shrink the array
  ASSERT(break_point_info->break_point_objects()->IsFixedArray());
  Handle<FixedArray> old_array =
      Handle<FixedArray>(
          FixedArray::cast(break_point_info->break_point_objects()));
  Handle<FixedArray> new_array =
      Factory::NewFixedArray(old_array->length() - 1);
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
    Handle<FixedArray> array = Factory::NewFixedArray(2);
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
      Factory::NewFixedArray(old_array->length() + 1);
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
  // Single beak point.
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
  // Single beak point.
  if (!break_point_objects()->IsFixedArray()) return 1;
  // Multiple break points.
  return FixedArray::cast(break_point_objects())->length();
}
#endif


} }  // namespace v8::internal
