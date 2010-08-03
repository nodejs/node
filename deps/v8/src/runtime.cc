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

#include <stdlib.h>

#include "v8.h"

#include "accessors.h"
#include "api.h"
#include "arguments.h"
#include "codegen.h"
#include "compiler.h"
#include "cpu.h"
#include "dateparser-inl.h"
#include "debug.h"
#include "execution.h"
#include "jsregexp.h"
#include "liveedit.h"
#include "parser.h"
#include "platform.h"
#include "runtime.h"
#include "scopeinfo.h"
#include "smart-pointer.h"
#include "stub-cache.h"
#include "v8threads.h"

namespace v8 {
namespace internal {


#define RUNTIME_ASSERT(value) \
  if (!(value)) return Top::ThrowIllegalOperation();

// Cast the given object to a value of the specified type and store
// it in a variable with the given name.  If the object is not of the
// expected type call IllegalOperation and return.
#define CONVERT_CHECKED(Type, name, obj)                             \
  RUNTIME_ASSERT(obj->Is##Type());                                   \
  Type* name = Type::cast(obj);

#define CONVERT_ARG_CHECKED(Type, name, index)                       \
  RUNTIME_ASSERT(args[index]->Is##Type());                           \
  Handle<Type> name = args.at<Type>(index);

// Cast the given object to a boolean and store it in a variable with
// the given name.  If the object is not a boolean call IllegalOperation
// and return.
#define CONVERT_BOOLEAN_CHECKED(name, obj)                            \
  RUNTIME_ASSERT(obj->IsBoolean());                                   \
  bool name = (obj)->IsTrue();

// Cast the given object to a Smi and store its value in an int variable
// with the given name.  If the object is not a Smi call IllegalOperation
// and return.
#define CONVERT_SMI_CHECKED(name, obj)                            \
  RUNTIME_ASSERT(obj->IsSmi());                                   \
  int name = Smi::cast(obj)->value();

// Cast the given object to a double and store it in a variable with
// the given name.  If the object is not a number (as opposed to
// the number not-a-number) call IllegalOperation and return.
#define CONVERT_DOUBLE_CHECKED(name, obj)                            \
  RUNTIME_ASSERT(obj->IsNumber());                                   \
  double name = (obj)->Number();

// Call the specified converter on the object *comand store the result in
// a variable of the specified type with the given name.  If the
// object is not a Number call IllegalOperation and return.
#define CONVERT_NUMBER_CHECKED(type, name, Type, obj)                \
  RUNTIME_ASSERT(obj->IsNumber());                                   \
  type name = NumberTo##Type(obj);

// Non-reentrant string buffer for efficient general use in this file.
static StaticResource<StringInputBuffer> runtime_string_input_buffer;


static Object* DeepCopyBoilerplate(JSObject* boilerplate) {
  StackLimitCheck check;
  if (check.HasOverflowed()) return Top::StackOverflow();

  Object* result = Heap::CopyJSObject(boilerplate);
  if (result->IsFailure()) return result;
  JSObject* copy = JSObject::cast(result);

  // Deep copy local properties.
  if (copy->HasFastProperties()) {
    FixedArray* properties = copy->properties();
    for (int i = 0; i < properties->length(); i++) {
      Object* value = properties->get(i);
      if (value->IsJSObject()) {
        JSObject* js_object = JSObject::cast(value);
        result = DeepCopyBoilerplate(js_object);
        if (result->IsFailure()) return result;
        properties->set(i, result);
      }
    }
    int nof = copy->map()->inobject_properties();
    for (int i = 0; i < nof; i++) {
      Object* value = copy->InObjectPropertyAt(i);
      if (value->IsJSObject()) {
        JSObject* js_object = JSObject::cast(value);
        result = DeepCopyBoilerplate(js_object);
        if (result->IsFailure()) return result;
        copy->InObjectPropertyAtPut(i, result);
      }
    }
  } else {
    result = Heap::AllocateFixedArray(copy->NumberOfLocalProperties(NONE));
    if (result->IsFailure()) return result;
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
      Object* value = copy->GetProperty(key_string, &attributes);
      ASSERT(!value->IsFailure());
      if (value->IsJSObject()) {
        JSObject* js_object = JSObject::cast(value);
        result = DeepCopyBoilerplate(js_object);
        if (result->IsFailure()) return result;
        result = copy->SetProperty(key_string, result, NONE);
        if (result->IsFailure()) return result;
      }
    }
  }

  // Deep copy local elements.
  // Pixel elements cannot be created using an object literal.
  ASSERT(!copy->HasPixelElements() && !copy->HasExternalArrayElements());
  switch (copy->GetElementsKind()) {
    case JSObject::FAST_ELEMENTS: {
      FixedArray* elements = FixedArray::cast(copy->elements());
      for (int i = 0; i < elements->length(); i++) {
        Object* value = elements->get(i);
        if (value->IsJSObject()) {
          JSObject* js_object = JSObject::cast(value);
          result = DeepCopyBoilerplate(js_object);
          if (result->IsFailure()) return result;
          elements->set(i, result);
        }
      }
      break;
    }
    case JSObject::DICTIONARY_ELEMENTS: {
      NumberDictionary* element_dictionary = copy->element_dictionary();
      int capacity = element_dictionary->Capacity();
      for (int i = 0; i < capacity; i++) {
        Object* k = element_dictionary->KeyAt(i);
        if (element_dictionary->IsKey(k)) {
          Object* value = element_dictionary->ValueAt(i);
          if (value->IsJSObject()) {
            JSObject* js_object = JSObject::cast(value);
            result = DeepCopyBoilerplate(js_object);
            if (result->IsFailure()) return result;
            element_dictionary->ValueAtPut(i, result);
          }
        }
      }
      break;
    }
    default:
      UNREACHABLE();
      break;
  }
  return copy;
}


static Object* Runtime_CloneLiteralBoilerplate(Arguments args) {
  CONVERT_CHECKED(JSObject, boilerplate, args[0]);
  return DeepCopyBoilerplate(boilerplate);
}


static Object* Runtime_CloneShallowLiteralBoilerplate(Arguments args) {
  CONVERT_CHECKED(JSObject, boilerplate, args[0]);
  return Heap::CopyJSObject(boilerplate);
}


static Handle<Map> ComputeObjectLiteralMap(
    Handle<Context> context,
    Handle<FixedArray> constant_properties,
    bool* is_result_from_cache) {
  int properties_length = constant_properties->length();
  int number_of_properties = properties_length / 2;
  if (FLAG_canonicalize_object_literal_maps) {
    // Check that there are only symbols and array indices among keys.
    int number_of_symbol_keys = 0;
    for (int p = 0; p != properties_length; p += 2) {
      Object* key = constant_properties->get(p);
      uint32_t element_index = 0;
      if (key->IsSymbol()) {
        number_of_symbol_keys++;
      } else if (key->ToArrayIndex(&element_index)) {
        // An index key does not require space in the property backing store.
        number_of_properties--;
      } else {
        // Bail out as a non-symbol non-index key makes caching impossible.
        // ASSERT to make sure that the if condition after the loop is false.
        ASSERT(number_of_symbol_keys != number_of_properties);
        break;
      }
    }
    // If we only have symbols and array indices among keys then we can
    // use the map cache in the global context.
    const int kMaxKeys = 10;
    if ((number_of_symbol_keys == number_of_properties) &&
        (number_of_symbol_keys < kMaxKeys)) {
      // Create the fixed array with the key.
      Handle<FixedArray> keys = Factory::NewFixedArray(number_of_symbol_keys);
      if (number_of_symbol_keys > 0) {
        int index = 0;
        for (int p = 0; p < properties_length; p += 2) {
          Object* key = constant_properties->get(p);
          if (key->IsSymbol()) {
            keys->set(index++, key);
          }
        }
        ASSERT(index == number_of_symbol_keys);
      }
      *is_result_from_cache = true;
      return Factory::ObjectLiteralMapFromCache(context, keys);
    }
  }
  *is_result_from_cache = false;
  return Factory::CopyMap(
      Handle<Map>(context->object_function()->initial_map()),
      number_of_properties);
}


static Handle<Object> CreateLiteralBoilerplate(
    Handle<FixedArray> literals,
    Handle<FixedArray> constant_properties);


static Handle<Object> CreateObjectLiteralBoilerplate(
    Handle<FixedArray> literals,
    Handle<FixedArray> constant_properties,
    bool should_have_fast_elements) {
  // Get the global context from the literals array.  This is the
  // context in which the function was created and we use the object
  // function from this context to create the object literal.  We do
  // not use the object function from the current global context
  // because this might be the object function from another context
  // which we should not have access to.
  Handle<Context> context =
      Handle<Context>(JSFunction::GlobalContextFromLiterals(*literals));

  bool is_result_from_cache;
  Handle<Map> map = ComputeObjectLiteralMap(context,
                                            constant_properties,
                                            &is_result_from_cache);

  Handle<JSObject> boilerplate = Factory::NewJSObjectFromMap(map);

  // Normalize the elements of the boilerplate to save space if needed.
  if (!should_have_fast_elements) NormalizeElements(boilerplate);

  {  // Add the constant properties to the boilerplate.
    int length = constant_properties->length();
    OptimizedObjectForAddingMultipleProperties opt(boilerplate,
                                                   length / 2,
                                                   !is_result_from_cache);
    for (int index = 0; index < length; index +=2) {
      Handle<Object> key(constant_properties->get(index+0));
      Handle<Object> value(constant_properties->get(index+1));
      if (value->IsFixedArray()) {
        // The value contains the constant_properties of a
        // simple object literal.
        Handle<FixedArray> array = Handle<FixedArray>::cast(value);
        value = CreateLiteralBoilerplate(literals, array);
        if (value.is_null()) return value;
      }
      Handle<Object> result;
      uint32_t element_index = 0;
      if (key->IsSymbol()) {
        // If key is a symbol it is not an array element.
        Handle<String> name(String::cast(*key));
        ASSERT(!name->AsArrayIndex(&element_index));
        result = SetProperty(boilerplate, name, value, NONE);
      } else if (key->ToArrayIndex(&element_index)) {
        // Array index (uint32).
        result = SetElement(boilerplate, element_index, value);
      } else {
        // Non-uint32 number.
        ASSERT(key->IsNumber());
        double num = key->Number();
        char arr[100];
        Vector<char> buffer(arr, ARRAY_SIZE(arr));
        const char* str = DoubleToCString(num, buffer);
        Handle<String> name = Factory::NewStringFromAscii(CStrVector(str));
        result = SetProperty(boilerplate, name, value, NONE);
      }
      // If setting the property on the boilerplate throws an
      // exception, the exception is converted to an empty handle in
      // the handle based operations.  In that case, we need to
      // convert back to an exception.
      if (result.is_null()) return result;
    }
  }

  return boilerplate;
}


static Handle<Object> CreateArrayLiteralBoilerplate(
    Handle<FixedArray> literals,
    Handle<FixedArray> elements) {
  // Create the JSArray.
  Handle<JSFunction> constructor(
      JSFunction::GlobalContextFromLiterals(*literals)->array_function());
  Handle<Object> object = Factory::NewJSObject(constructor);

  Handle<Object> copied_elements = Factory::CopyFixedArray(elements);

  Handle<FixedArray> content = Handle<FixedArray>::cast(copied_elements);
  for (int i = 0; i < content->length(); i++) {
    if (content->get(i)->IsFixedArray()) {
      // The value contains the constant_properties of a
      // simple object literal.
      Handle<FixedArray> fa(FixedArray::cast(content->get(i)));
      Handle<Object> result =
        CreateLiteralBoilerplate(literals, fa);
      if (result.is_null()) return result;
      content->set(i, *result);
    }
  }

  // Set the elements.
  Handle<JSArray>::cast(object)->SetContent(*content);
  return object;
}


static Handle<Object> CreateLiteralBoilerplate(
    Handle<FixedArray> literals,
    Handle<FixedArray> array) {
  Handle<FixedArray> elements = CompileTimeValue::GetElements(array);
  switch (CompileTimeValue::GetType(array)) {
    case CompileTimeValue::OBJECT_LITERAL_FAST_ELEMENTS:
      return CreateObjectLiteralBoilerplate(literals, elements, true);
    case CompileTimeValue::OBJECT_LITERAL_SLOW_ELEMENTS:
      return CreateObjectLiteralBoilerplate(literals, elements, false);
    case CompileTimeValue::ARRAY_LITERAL:
      return CreateArrayLiteralBoilerplate(literals, elements);
    default:
      UNREACHABLE();
      return Handle<Object>::null();
  }
}


static Object* Runtime_CreateArrayLiteralBoilerplate(Arguments args) {
  // Takes a FixedArray of elements containing the literal elements of
  // the array literal and produces JSArray with those elements.
  // Additionally takes the literals array of the surrounding function
  // which contains the context from which to get the Array function
  // to use for creating the array literal.
  HandleScope scope;
  ASSERT(args.length() == 3);
  CONVERT_ARG_CHECKED(FixedArray, literals, 0);
  CONVERT_SMI_CHECKED(literals_index, args[1]);
  CONVERT_ARG_CHECKED(FixedArray, elements, 2);

  Handle<Object> object = CreateArrayLiteralBoilerplate(literals, elements);
  if (object.is_null()) return Failure::Exception();

  // Update the functions literal and return the boilerplate.
  literals->set(literals_index, *object);
  return *object;
}


static Object* Runtime_CreateObjectLiteral(Arguments args) {
  HandleScope scope;
  ASSERT(args.length() == 4);
  CONVERT_ARG_CHECKED(FixedArray, literals, 0);
  CONVERT_SMI_CHECKED(literals_index, args[1]);
  CONVERT_ARG_CHECKED(FixedArray, constant_properties, 2);
  CONVERT_SMI_CHECKED(fast_elements, args[3]);
  bool should_have_fast_elements = fast_elements == 1;

  // Check if boilerplate exists. If not, create it first.
  Handle<Object> boilerplate(literals->get(literals_index));
  if (*boilerplate == Heap::undefined_value()) {
    boilerplate = CreateObjectLiteralBoilerplate(literals,
                                                 constant_properties,
                                                 should_have_fast_elements);
    if (boilerplate.is_null()) return Failure::Exception();
    // Update the functions literal and return the boilerplate.
    literals->set(literals_index, *boilerplate);
  }
  return DeepCopyBoilerplate(JSObject::cast(*boilerplate));
}


static Object* Runtime_CreateObjectLiteralShallow(Arguments args) {
  HandleScope scope;
  ASSERT(args.length() == 4);
  CONVERT_ARG_CHECKED(FixedArray, literals, 0);
  CONVERT_SMI_CHECKED(literals_index, args[1]);
  CONVERT_ARG_CHECKED(FixedArray, constant_properties, 2);
  CONVERT_SMI_CHECKED(fast_elements, args[3]);
  bool should_have_fast_elements = fast_elements == 1;

  // Check if boilerplate exists. If not, create it first.
  Handle<Object> boilerplate(literals->get(literals_index));
  if (*boilerplate == Heap::undefined_value()) {
    boilerplate = CreateObjectLiteralBoilerplate(literals,
                                                 constant_properties,
                                                 should_have_fast_elements);
    if (boilerplate.is_null()) return Failure::Exception();
    // Update the functions literal and return the boilerplate.
    literals->set(literals_index, *boilerplate);
  }
  return Heap::CopyJSObject(JSObject::cast(*boilerplate));
}


static Object* Runtime_CreateArrayLiteral(Arguments args) {
  HandleScope scope;
  ASSERT(args.length() == 3);
  CONVERT_ARG_CHECKED(FixedArray, literals, 0);
  CONVERT_SMI_CHECKED(literals_index, args[1]);
  CONVERT_ARG_CHECKED(FixedArray, elements, 2);

  // Check if boilerplate exists. If not, create it first.
  Handle<Object> boilerplate(literals->get(literals_index));
  if (*boilerplate == Heap::undefined_value()) {
    boilerplate = CreateArrayLiteralBoilerplate(literals, elements);
    if (boilerplate.is_null()) return Failure::Exception();
    // Update the functions literal and return the boilerplate.
    literals->set(literals_index, *boilerplate);
  }
  return DeepCopyBoilerplate(JSObject::cast(*boilerplate));
}


static Object* Runtime_CreateArrayLiteralShallow(Arguments args) {
  HandleScope scope;
  ASSERT(args.length() == 3);
  CONVERT_ARG_CHECKED(FixedArray, literals, 0);
  CONVERT_SMI_CHECKED(literals_index, args[1]);
  CONVERT_ARG_CHECKED(FixedArray, elements, 2);

  // Check if boilerplate exists. If not, create it first.
  Handle<Object> boilerplate(literals->get(literals_index));
  if (*boilerplate == Heap::undefined_value()) {
    boilerplate = CreateArrayLiteralBoilerplate(literals, elements);
    if (boilerplate.is_null()) return Failure::Exception();
    // Update the functions literal and return the boilerplate.
    literals->set(literals_index, *boilerplate);
  }
  return Heap::CopyJSObject(JSObject::cast(*boilerplate));
}


static Object* Runtime_CreateCatchExtensionObject(Arguments args) {
  ASSERT(args.length() == 2);
  CONVERT_CHECKED(String, key, args[0]);
  Object* value = args[1];
  // Create a catch context extension object.
  JSFunction* constructor =
      Top::context()->global_context()->context_extension_function();
  Object* object = Heap::AllocateJSObject(constructor);
  if (object->IsFailure()) return object;
  // Assign the exception value to the catch variable and make sure
  // that the catch variable is DontDelete.
  value = JSObject::cast(object)->SetProperty(key, value, DONT_DELETE);
  if (value->IsFailure()) return value;
  return object;
}


static Object* Runtime_ClassOf(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);
  Object* obj = args[0];
  if (!obj->IsJSObject()) return Heap::null_value();
  return JSObject::cast(obj)->class_name();
}


static Object* Runtime_IsInPrototypeChain(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);
  // See ECMA-262, section 15.3.5.3, page 88 (steps 5 - 8).
  Object* O = args[0];
  Object* V = args[1];
  while (true) {
    Object* prototype = V->GetPrototype();
    if (prototype->IsNull()) return Heap::false_value();
    if (O == prototype) return Heap::true_value();
    V = prototype;
  }
}


// Inserts an object as the hidden prototype of another object.
static Object* Runtime_SetHiddenPrototype(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);
  CONVERT_CHECKED(JSObject, jsobject, args[0]);
  CONVERT_CHECKED(JSObject, proto, args[1]);

  // Sanity checks.  The old prototype (that we are replacing) could
  // theoretically be null, but if it is not null then check that we
  // didn't already install a hidden prototype here.
  RUNTIME_ASSERT(!jsobject->GetPrototype()->IsHeapObject() ||
    !HeapObject::cast(jsobject->GetPrototype())->map()->is_hidden_prototype());
  RUNTIME_ASSERT(!proto->map()->is_hidden_prototype());

  // Allocate up front before we start altering state in case we get a GC.
  Object* map_or_failure = proto->map()->CopyDropTransitions();
  if (map_or_failure->IsFailure()) return map_or_failure;
  Map* new_proto_map = Map::cast(map_or_failure);

  map_or_failure = jsobject->map()->CopyDropTransitions();
  if (map_or_failure->IsFailure()) return map_or_failure;
  Map* new_map = Map::cast(map_or_failure);

  // Set proto's prototype to be the old prototype of the object.
  new_proto_map->set_prototype(jsobject->GetPrototype());
  proto->set_map(new_proto_map);
  new_proto_map->set_is_hidden_prototype();

  // Set the object's prototype to proto.
  new_map->set_prototype(proto);
  jsobject->set_map(new_map);

  return Heap::undefined_value();
}


static Object* Runtime_IsConstructCall(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 0);
  JavaScriptFrameIterator it;
  return Heap::ToBoolean(it.frame()->IsConstructor());
}


// Recursively traverses hidden prototypes if property is not found
static void GetOwnPropertyImplementation(JSObject* obj,
                                         String* name,
                                         LookupResult* result) {
  obj->LocalLookupRealNamedProperty(name, result);

  if (!result->IsProperty()) {
    Object* proto = obj->GetPrototype();
    if (proto->IsJSObject() &&
      JSObject::cast(proto)->map()->is_hidden_prototype())
      GetOwnPropertyImplementation(JSObject::cast(proto),
                                   name, result);
  }
}


// Enumerator used as indices into the array returned from GetOwnProperty
enum PropertyDescriptorIndices {
  IS_ACCESSOR_INDEX,
  VALUE_INDEX,
  GETTER_INDEX,
  SETTER_INDEX,
  WRITABLE_INDEX,
  ENUMERABLE_INDEX,
  CONFIGURABLE_INDEX,
  DESCRIPTOR_SIZE
};

// Returns an array with the property description:
//  if args[1] is not a property on args[0]
//          returns undefined
//  if args[1] is a data property on args[0]
//         [false, value, Writeable, Enumerable, Configurable]
//  if args[1] is an accessor on args[0]
//         [true, GetFunction, SetFunction, Enumerable, Configurable]
static Object* Runtime_GetOwnProperty(Arguments args) {
  ASSERT(args.length() == 2);
  HandleScope scope;
  Handle<FixedArray> elms = Factory::NewFixedArray(DESCRIPTOR_SIZE);
  Handle<JSArray> desc = Factory::NewJSArrayWithElements(elms);
  LookupResult result;
  CONVERT_CHECKED(JSObject, obj, args[0]);
  CONVERT_CHECKED(String, name, args[1]);

  // This could be an element.
  uint32_t index;
  if (name->AsArrayIndex(&index)) {
    if (!obj->HasLocalElement(index)) {
      return Heap::undefined_value();
    }

    // Special handling of string objects according to ECMAScript 5 15.5.5.2.
    // Note that this might be a string object with elements other than the
    // actual string value. This is covered by the subsequent cases.
    if (obj->IsStringObjectWithCharacterAt(index)) {
      JSValue* js_value = JSValue::cast(obj);
      String* str = String::cast(js_value->value());
      elms->set(IS_ACCESSOR_INDEX, Heap::false_value());
      elms->set(VALUE_INDEX, str->SubString(index, index+1));
      elms->set(WRITABLE_INDEX, Heap::false_value());
      elms->set(ENUMERABLE_INDEX,  Heap::false_value());
      elms->set(CONFIGURABLE_INDEX, Heap::false_value());
      return *desc;
    }

    // This can potentially be an element in the elements dictionary or
    // a fast element.
    if (obj->HasDictionaryElements()) {
      NumberDictionary* dictionary = obj->element_dictionary();
      int entry = dictionary->FindEntry(index);
      PropertyDetails details = dictionary->DetailsAt(entry);
      elms->set(IS_ACCESSOR_INDEX, Heap::false_value());
      elms->set(VALUE_INDEX, dictionary->ValueAt(entry));
      elms->set(WRITABLE_INDEX, Heap::ToBoolean(!details.IsReadOnly()));
      elms->set(ENUMERABLE_INDEX, Heap::ToBoolean(!details.IsDontEnum()));
      elms->set(CONFIGURABLE_INDEX, Heap::ToBoolean(!details.IsDontDelete()));
      return *desc;
    } else {
      // Elements that are stored as array elements always has:
      // writable: true, configurable: true, enumerable: true.
      elms->set(IS_ACCESSOR_INDEX, Heap::false_value());
      elms->set(VALUE_INDEX, obj->GetElement(index));
      elms->set(WRITABLE_INDEX, Heap::true_value());
      elms->set(ENUMERABLE_INDEX,  Heap::true_value());
      elms->set(CONFIGURABLE_INDEX, Heap::true_value());
      return *desc;
    }
  }

  // Use recursive implementation to also traverse hidden prototypes
  GetOwnPropertyImplementation(obj, name, &result);

  if (!result.IsProperty()) {
    return Heap::undefined_value();
  }
  if (result.type() == CALLBACKS) {
    Object* structure = result.GetCallbackObject();
    if (structure->IsProxy() || structure->IsAccessorInfo()) {
      // Property that is internally implemented as a callback or
      // an API defined callback.
      Object* value = obj->GetPropertyWithCallback(
          obj, structure, name, result.holder());
      elms->set(IS_ACCESSOR_INDEX, Heap::false_value());
      elms->set(VALUE_INDEX, value);
      elms->set(WRITABLE_INDEX, Heap::ToBoolean(!result.IsReadOnly()));
    } else if (structure->IsFixedArray()) {
      // __defineGetter__/__defineSetter__ callback.
      elms->set(IS_ACCESSOR_INDEX, Heap::true_value());
      elms->set(GETTER_INDEX, FixedArray::cast(structure)->get(0));
      elms->set(SETTER_INDEX, FixedArray::cast(structure)->get(1));
    } else {
      return Heap::undefined_value();
    }
  } else {
    elms->set(IS_ACCESSOR_INDEX, Heap::false_value());
    elms->set(VALUE_INDEX, result.GetLazyValue());
    elms->set(WRITABLE_INDEX, Heap::ToBoolean(!result.IsReadOnly()));
  }

  elms->set(ENUMERABLE_INDEX, Heap::ToBoolean(!result.IsDontEnum()));
  elms->set(CONFIGURABLE_INDEX, Heap::ToBoolean(!result.IsDontDelete()));
  return *desc;
}


static Object* Runtime_PreventExtensions(Arguments args) {
  ASSERT(args.length() == 1);
  CONVERT_CHECKED(JSObject, obj, args[0]);
  return obj->PreventExtensions();
}

static Object* Runtime_IsExtensible(Arguments args) {
  ASSERT(args.length() == 1);
  CONVERT_CHECKED(JSObject, obj, args[0]);
  return obj->map()->is_extensible() ?  Heap::true_value()
                                     : Heap::false_value();
}


static Object* Runtime_RegExpCompile(Arguments args) {
  HandleScope scope;
  ASSERT(args.length() == 3);
  CONVERT_ARG_CHECKED(JSRegExp, re, 0);
  CONVERT_ARG_CHECKED(String, pattern, 1);
  CONVERT_ARG_CHECKED(String, flags, 2);
  Handle<Object> result = RegExpImpl::Compile(re, pattern, flags);
  if (result.is_null()) return Failure::Exception();
  return *result;
}


static Object* Runtime_CreateApiFunction(Arguments args) {
  HandleScope scope;
  ASSERT(args.length() == 1);
  CONVERT_ARG_CHECKED(FunctionTemplateInfo, data, 0);
  return *Factory::CreateApiFunction(data);
}


static Object* Runtime_IsTemplate(Arguments args) {
  ASSERT(args.length() == 1);
  Object* arg = args[0];
  bool result = arg->IsObjectTemplateInfo() || arg->IsFunctionTemplateInfo();
  return Heap::ToBoolean(result);
}


static Object* Runtime_GetTemplateField(Arguments args) {
  ASSERT(args.length() == 2);
  CONVERT_CHECKED(HeapObject, templ, args[0]);
  CONVERT_CHECKED(Smi, field, args[1]);
  int index = field->value();
  int offset = index * kPointerSize + HeapObject::kHeaderSize;
  InstanceType type = templ->map()->instance_type();
  RUNTIME_ASSERT(type ==  FUNCTION_TEMPLATE_INFO_TYPE ||
                 type ==  OBJECT_TEMPLATE_INFO_TYPE);
  RUNTIME_ASSERT(offset > 0);
  if (type == FUNCTION_TEMPLATE_INFO_TYPE) {
    RUNTIME_ASSERT(offset < FunctionTemplateInfo::kSize);
  } else {
    RUNTIME_ASSERT(offset < ObjectTemplateInfo::kSize);
  }
  return *HeapObject::RawField(templ, offset);
}


static Object* Runtime_DisableAccessChecks(Arguments args) {
  ASSERT(args.length() == 1);
  CONVERT_CHECKED(HeapObject, object, args[0]);
  Map* old_map = object->map();
  bool needs_access_checks = old_map->is_access_check_needed();
  if (needs_access_checks) {
    // Copy map so it won't interfere constructor's initial map.
    Object* new_map = old_map->CopyDropTransitions();
    if (new_map->IsFailure()) return new_map;

    Map::cast(new_map)->set_is_access_check_needed(false);
    object->set_map(Map::cast(new_map));
  }
  return needs_access_checks ? Heap::true_value() : Heap::false_value();
}


static Object* Runtime_EnableAccessChecks(Arguments args) {
  ASSERT(args.length() == 1);
  CONVERT_CHECKED(HeapObject, object, args[0]);
  Map* old_map = object->map();
  if (!old_map->is_access_check_needed()) {
    // Copy map so it won't interfere constructor's initial map.
    Object* new_map = old_map->CopyDropTransitions();
    if (new_map->IsFailure()) return new_map;

    Map::cast(new_map)->set_is_access_check_needed(true);
    object->set_map(Map::cast(new_map));
  }
  return Heap::undefined_value();
}


static Object* ThrowRedeclarationError(const char* type, Handle<String> name) {
  HandleScope scope;
  Handle<Object> type_handle = Factory::NewStringFromAscii(CStrVector(type));
  Handle<Object> args[2] = { type_handle, name };
  Handle<Object> error =
      Factory::NewTypeError("redeclaration", HandleVector(args, 2));
  return Top::Throw(*error);
}


static Object* Runtime_DeclareGlobals(Arguments args) {
  HandleScope scope;
  Handle<GlobalObject> global = Handle<GlobalObject>(Top::context()->global());

  Handle<Context> context = args.at<Context>(0);
  CONVERT_ARG_CHECKED(FixedArray, pairs, 1);
  bool is_eval = Smi::cast(args[2])->value() == 1;

  // Compute the property attributes. According to ECMA-262, section
  // 13, page 71, the property must be read-only and
  // non-deletable. However, neither SpiderMonkey nor KJS creates the
  // property as read-only, so we don't either.
  PropertyAttributes base = is_eval ? NONE : DONT_DELETE;

  // Traverse the name/value pairs and set the properties.
  int length = pairs->length();
  for (int i = 0; i < length; i += 2) {
    HandleScope scope;
    Handle<String> name(String::cast(pairs->get(i)));
    Handle<Object> value(pairs->get(i + 1));

    // We have to declare a global const property. To capture we only
    // assign to it when evaluating the assignment for "const x =
    // <expr>" the initial value is the hole.
    bool is_const_property = value->IsTheHole();

    if (value->IsUndefined() || is_const_property) {
      // Lookup the property in the global object, and don't set the
      // value of the variable if the property is already there.
      LookupResult lookup;
      global->Lookup(*name, &lookup);
      if (lookup.IsProperty()) {
        // Determine if the property is local by comparing the holder
        // against the global object. The information will be used to
        // avoid throwing re-declaration errors when declaring
        // variables or constants that exist in the prototype chain.
        bool is_local = (*global == lookup.holder());
        // Get the property attributes and determine if the property is
        // read-only.
        PropertyAttributes attributes = global->GetPropertyAttribute(*name);
        bool is_read_only = (attributes & READ_ONLY) != 0;
        if (lookup.type() == INTERCEPTOR) {
          // If the interceptor says the property is there, we
          // just return undefined without overwriting the property.
          // Otherwise, we continue to setting the property.
          if (attributes != ABSENT) {
            // Check if the existing property conflicts with regards to const.
            if (is_local && (is_read_only || is_const_property)) {
              const char* type = (is_read_only) ? "const" : "var";
              return ThrowRedeclarationError(type, name);
            };
            // The property already exists without conflicting: Go to
            // the next declaration.
            continue;
          }
          // Fall-through and introduce the absent property by using
          // SetProperty.
        } else {
          if (is_local && (is_read_only || is_const_property)) {
            const char* type = (is_read_only) ? "const" : "var";
            return ThrowRedeclarationError(type, name);
          }
          // The property already exists without conflicting: Go to
          // the next declaration.
          continue;
        }
      }
    } else {
      // Copy the function and update its context. Use it as value.
      Handle<SharedFunctionInfo> shared =
          Handle<SharedFunctionInfo>::cast(value);
      Handle<JSFunction> function =
          Factory::NewFunctionFromSharedFunctionInfo(shared, context, TENURED);
      value = function;
    }

    LookupResult lookup;
    global->LocalLookup(*name, &lookup);

    PropertyAttributes attributes = is_const_property
        ? static_cast<PropertyAttributes>(base | READ_ONLY)
        : base;

    if (lookup.IsProperty()) {
      // There's a local property that we need to overwrite because
      // we're either declaring a function or there's an interceptor
      // that claims the property is absent.

      // Check for conflicting re-declarations. We cannot have
      // conflicting types in case of intercepted properties because
      // they are absent.
      if (lookup.type() != INTERCEPTOR &&
          (lookup.IsReadOnly() || is_const_property)) {
        const char* type = (lookup.IsReadOnly()) ? "const" : "var";
        return ThrowRedeclarationError(type, name);
      }
      SetProperty(global, name, value, attributes);
    } else {
      // If a property with this name does not already exist on the
      // global object add the property locally.  We take special
      // precautions to always add it as a local property even in case
      // of callbacks in the prototype chain (this rules out using
      // SetProperty).  Also, we must use the handle-based version to
      // avoid GC issues.
      IgnoreAttributesAndSetLocalProperty(global, name, value, attributes);
    }
  }

  return Heap::undefined_value();
}


static Object* Runtime_DeclareContextSlot(Arguments args) {
  HandleScope scope;
  ASSERT(args.length() == 4);

  CONVERT_ARG_CHECKED(Context, context, 0);
  Handle<String> name(String::cast(args[1]));
  PropertyAttributes mode =
      static_cast<PropertyAttributes>(Smi::cast(args[2])->value());
  ASSERT(mode == READ_ONLY || mode == NONE);
  Handle<Object> initial_value(args[3]);

  // Declarations are always done in the function context.
  context = Handle<Context>(context->fcontext());

  int index;
  PropertyAttributes attributes;
  ContextLookupFlags flags = DONT_FOLLOW_CHAINS;
  Handle<Object> holder =
      context->Lookup(name, flags, &index, &attributes);

  if (attributes != ABSENT) {
    // The name was declared before; check for conflicting
    // re-declarations: This is similar to the code in parser.cc in
    // the AstBuildingParser::Declare function.
    if (((attributes & READ_ONLY) != 0) || (mode == READ_ONLY)) {
      // Functions are not read-only.
      ASSERT(mode != READ_ONLY || initial_value->IsTheHole());
      const char* type = ((attributes & READ_ONLY) != 0) ? "const" : "var";
      return ThrowRedeclarationError(type, name);
    }

    // Initialize it if necessary.
    if (*initial_value != NULL) {
      if (index >= 0) {
        // The variable or constant context slot should always be in
        // the function context or the arguments object.
        if (holder->IsContext()) {
          ASSERT(holder.is_identical_to(context));
          if (((attributes & READ_ONLY) == 0) ||
              context->get(index)->IsTheHole()) {
            context->set(index, *initial_value);
          }
        } else {
          Handle<JSObject>::cast(holder)->SetElement(index, *initial_value);
        }
      } else {
        // Slow case: The property is not in the FixedArray part of the context.
        Handle<JSObject> context_ext = Handle<JSObject>::cast(holder);
        SetProperty(context_ext, name, initial_value, mode);
      }
    }

  } else {
    // The property is not in the function context. It needs to be
    // "declared" in the function context's extension context, or in the
    // global context.
    Handle<JSObject> context_ext;
    if (context->has_extension()) {
      // The function context's extension context exists - use it.
      context_ext = Handle<JSObject>(context->extension());
    } else {
      // The function context's extension context does not exists - allocate
      // it.
      context_ext = Factory::NewJSObject(Top::context_extension_function());
      // And store it in the extension slot.
      context->set_extension(*context_ext);
    }
    ASSERT(*context_ext != NULL);

    // Declare the property by setting it to the initial value if provided,
    // or undefined, and use the correct mode (e.g. READ_ONLY attribute for
    // constant declarations).
    ASSERT(!context_ext->HasLocalProperty(*name));
    Handle<Object> value(Heap::undefined_value());
    if (*initial_value != NULL) value = initial_value;
    SetProperty(context_ext, name, value, mode);
    ASSERT(context_ext->GetLocalPropertyAttribute(*name) == mode);
  }

  return Heap::undefined_value();
}


static Object* Runtime_InitializeVarGlobal(Arguments args) {
  NoHandleAllocation nha;

  // Determine if we need to assign to the variable if it already
  // exists (based on the number of arguments).
  RUNTIME_ASSERT(args.length() == 1 || args.length() == 2);
  bool assign = args.length() == 2;

  CONVERT_ARG_CHECKED(String, name, 0);
  GlobalObject* global = Top::context()->global();

  // According to ECMA-262, section 12.2, page 62, the property must
  // not be deletable.
  PropertyAttributes attributes = DONT_DELETE;

  // Lookup the property locally in the global object. If it isn't
  // there, there is a property with this name in the prototype chain.
  // We follow Safari and Firefox behavior and only set the property
  // locally if there is an explicit initialization value that we have
  // to assign to the property. When adding the property we take
  // special precautions to always add it as a local property even in
  // case of callbacks in the prototype chain (this rules out using
  // SetProperty).  We have IgnoreAttributesAndSetLocalProperty for
  // this.
  // Note that objects can have hidden prototypes, so we need to traverse
  // the whole chain of hidden prototypes to do a 'local' lookup.
  JSObject* real_holder = global;
  LookupResult lookup;
  while (true) {
    real_holder->LocalLookup(*name, &lookup);
    if (lookup.IsProperty()) {
      // Determine if this is a redeclaration of something read-only.
      if (lookup.IsReadOnly()) {
        // If we found readonly property on one of hidden prototypes,
        // just shadow it.
        if (real_holder != Top::context()->global()) break;
        return ThrowRedeclarationError("const", name);
      }

      // Determine if this is a redeclaration of an intercepted read-only
      // property and figure out if the property exists at all.
      bool found = true;
      PropertyType type = lookup.type();
      if (type == INTERCEPTOR) {
        HandleScope handle_scope;
        Handle<JSObject> holder(real_holder);
        PropertyAttributes intercepted = holder->GetPropertyAttribute(*name);
        real_holder = *holder;
        if (intercepted == ABSENT) {
          // The interceptor claims the property isn't there. We need to
          // make sure to introduce it.
          found = false;
        } else if ((intercepted & READ_ONLY) != 0) {
          // The property is present, but read-only. Since we're trying to
          // overwrite it with a variable declaration we must throw a
          // re-declaration error.  However if we found readonly property
          // on one of hidden prototypes, just shadow it.
          if (real_holder != Top::context()->global()) break;
          return ThrowRedeclarationError("const", name);
        }
      }

      if (found && !assign) {
        // The global property is there and we're not assigning any value
        // to it. Just return.
        return Heap::undefined_value();
      }

      // Assign the value (or undefined) to the property.
      Object* value = (assign) ? args[1] : Heap::undefined_value();
      return real_holder->SetProperty(&lookup, *name, value, attributes);
    }

    Object* proto = real_holder->GetPrototype();
    if (!proto->IsJSObject())
      break;

    if (!JSObject::cast(proto)->map()->is_hidden_prototype())
      break;

    real_holder = JSObject::cast(proto);
  }

  global = Top::context()->global();
  if (assign) {
    return global->IgnoreAttributesAndSetLocalProperty(*name,
                                                       args[1],
                                                       attributes);
  }
  return Heap::undefined_value();
}


static Object* Runtime_InitializeConstGlobal(Arguments args) {
  // All constants are declared with an initial value. The name
  // of the constant is the first argument and the initial value
  // is the second.
  RUNTIME_ASSERT(args.length() == 2);
  CONVERT_ARG_CHECKED(String, name, 0);
  Handle<Object> value = args.at<Object>(1);

  // Get the current global object from top.
  GlobalObject* global = Top::context()->global();

  // According to ECMA-262, section 12.2, page 62, the property must
  // not be deletable. Since it's a const, it must be READ_ONLY too.
  PropertyAttributes attributes =
      static_cast<PropertyAttributes>(DONT_DELETE | READ_ONLY);

  // Lookup the property locally in the global object. If it isn't
  // there, we add the property and take special precautions to always
  // add it as a local property even in case of callbacks in the
  // prototype chain (this rules out using SetProperty).
  // We use IgnoreAttributesAndSetLocalProperty instead
  LookupResult lookup;
  global->LocalLookup(*name, &lookup);
  if (!lookup.IsProperty()) {
    return global->IgnoreAttributesAndSetLocalProperty(*name,
                                                       *value,
                                                       attributes);
  }

  // Determine if this is a redeclaration of something not
  // read-only. In case the result is hidden behind an interceptor we
  // need to ask it for the property attributes.
  if (!lookup.IsReadOnly()) {
    if (lookup.type() != INTERCEPTOR) {
      return ThrowRedeclarationError("var", name);
    }

    PropertyAttributes intercepted = global->GetPropertyAttribute(*name);

    // Throw re-declaration error if the intercepted property is present
    // but not read-only.
    if (intercepted != ABSENT && (intercepted & READ_ONLY) == 0) {
      return ThrowRedeclarationError("var", name);
    }

    // Restore global object from context (in case of GC) and continue
    // with setting the value because the property is either absent or
    // read-only. We also have to do redo the lookup.
    global = Top::context()->global();

    // BUG 1213579: Handle the case where we have to set a read-only
    // property through an interceptor and only do it if it's
    // uninitialized, e.g. the hole. Nirk...
    global->SetProperty(*name, *value, attributes);
    return *value;
  }

  // Set the value, but only we're assigning the initial value to a
  // constant. For now, we determine this by checking if the
  // current value is the hole.
  PropertyType type = lookup.type();
  if (type == FIELD) {
    FixedArray* properties = global->properties();
    int index = lookup.GetFieldIndex();
    if (properties->get(index)->IsTheHole()) {
      properties->set(index, *value);
    }
  } else if (type == NORMAL) {
    if (global->GetNormalizedProperty(&lookup)->IsTheHole()) {
      global->SetNormalizedProperty(&lookup, *value);
    }
  } else {
    // Ignore re-initialization of constants that have already been
    // assigned a function value.
    ASSERT(lookup.IsReadOnly() && type == CONSTANT_FUNCTION);
  }

  // Use the set value as the result of the operation.
  return *value;
}


static Object* Runtime_InitializeConstContextSlot(Arguments args) {
  HandleScope scope;
  ASSERT(args.length() == 3);

  Handle<Object> value(args[0]);
  ASSERT(!value->IsTheHole());
  CONVERT_ARG_CHECKED(Context, context, 1);
  Handle<String> name(String::cast(args[2]));

  // Initializations are always done in the function context.
  context = Handle<Context>(context->fcontext());

  int index;
  PropertyAttributes attributes;
  ContextLookupFlags flags = FOLLOW_CHAINS;
  Handle<Object> holder =
      context->Lookup(name, flags, &index, &attributes);

  // In most situations, the property introduced by the const
  // declaration should be present in the context extension object.
  // However, because declaration and initialization are separate, the
  // property might have been deleted (if it was introduced by eval)
  // before we reach the initialization point.
  //
  // Example:
  //
  //    function f() { eval("delete x; const x;"); }
  //
  // In that case, the initialization behaves like a normal assignment
  // to property 'x'.
  if (index >= 0) {
    // Property was found in a context.
    if (holder->IsContext()) {
      // The holder cannot be the function context.  If it is, there
      // should have been a const redeclaration error when declaring
      // the const property.
      ASSERT(!holder.is_identical_to(context));
      if ((attributes & READ_ONLY) == 0) {
        Handle<Context>::cast(holder)->set(index, *value);
      }
    } else {
      // The holder is an arguments object.
      ASSERT((attributes & READ_ONLY) == 0);
      Handle<JSObject>::cast(holder)->SetElement(index, *value);
    }
    return *value;
  }

  // The property could not be found, we introduce it in the global
  // context.
  if (attributes == ABSENT) {
    Handle<JSObject> global = Handle<JSObject>(Top::context()->global());
    SetProperty(global, name, value, NONE);
    return *value;
  }

  // The property was present in a context extension object.
  Handle<JSObject> context_ext = Handle<JSObject>::cast(holder);

  if (*context_ext == context->extension()) {
    // This is the property that was introduced by the const
    // declaration.  Set it if it hasn't been set before.  NOTE: We
    // cannot use GetProperty() to get the current value as it
    // 'unholes' the value.
    LookupResult lookup;
    context_ext->LocalLookupRealNamedProperty(*name, &lookup);
    ASSERT(lookup.IsProperty());  // the property was declared
    ASSERT(lookup.IsReadOnly());  // and it was declared as read-only

    PropertyType type = lookup.type();
    if (type == FIELD) {
      FixedArray* properties = context_ext->properties();
      int index = lookup.GetFieldIndex();
      if (properties->get(index)->IsTheHole()) {
        properties->set(index, *value);
      }
    } else if (type == NORMAL) {
      if (context_ext->GetNormalizedProperty(&lookup)->IsTheHole()) {
        context_ext->SetNormalizedProperty(&lookup, *value);
      }
    } else {
      // We should not reach here. Any real, named property should be
      // either a field or a dictionary slot.
      UNREACHABLE();
    }
  } else {
    // The property was found in a different context extension object.
    // Set it if it is not a read-only property.
    if ((attributes & READ_ONLY) == 0) {
      Handle<Object> set = SetProperty(context_ext, name, value, attributes);
      // Setting a property might throw an exception.  Exceptions
      // are converted to empty handles in handle operations.  We
      // need to convert back to exceptions here.
      if (set.is_null()) {
        ASSERT(Top::has_pending_exception());
        return Failure::Exception();
      }
    }
  }

  return *value;
}


static Object* Runtime_OptimizeObjectForAddingMultipleProperties(
    Arguments args) {
  HandleScope scope;
  ASSERT(args.length() == 2);
  CONVERT_ARG_CHECKED(JSObject, object, 0);
  CONVERT_SMI_CHECKED(properties, args[1]);
  if (object->HasFastProperties()) {
    NormalizeProperties(object, KEEP_INOBJECT_PROPERTIES, properties);
  }
  return *object;
}


static Object* Runtime_RegExpExec(Arguments args) {
  HandleScope scope;
  ASSERT(args.length() == 4);
  CONVERT_ARG_CHECKED(JSRegExp, regexp, 0);
  CONVERT_ARG_CHECKED(String, subject, 1);
  // Due to the way the JS calls are constructed this must be less than the
  // length of a string, i.e. it is always a Smi.  We check anyway for security.
  CONVERT_SMI_CHECKED(index, args[2]);
  CONVERT_ARG_CHECKED(JSArray, last_match_info, 3);
  RUNTIME_ASSERT(last_match_info->HasFastElements());
  RUNTIME_ASSERT(index >= 0);
  RUNTIME_ASSERT(index <= subject->length());
  Counters::regexp_entry_runtime.Increment();
  Handle<Object> result = RegExpImpl::Exec(regexp,
                                           subject,
                                           index,
                                           last_match_info);
  if (result.is_null()) return Failure::Exception();
  return *result;
}


static Object* Runtime_RegExpConstructResult(Arguments args) {
  ASSERT(args.length() == 3);
  CONVERT_SMI_CHECKED(elements_count, args[0]);
  if (elements_count > JSArray::kMaxFastElementsLength) {
    return Top::ThrowIllegalOperation();
  }
  Object* new_object = Heap::AllocateFixedArrayWithHoles(elements_count);
  if (new_object->IsFailure()) return new_object;
  FixedArray* elements = FixedArray::cast(new_object);
  new_object = Heap::AllocateRaw(JSRegExpResult::kSize,
                                 NEW_SPACE,
                                 OLD_POINTER_SPACE);
  if (new_object->IsFailure()) return new_object;
  {
    AssertNoAllocation no_gc;
    HandleScope scope;
    reinterpret_cast<HeapObject*>(new_object)->
        set_map(Top::global_context()->regexp_result_map());
  }
  JSArray* array = JSArray::cast(new_object);
  array->set_properties(Heap::empty_fixed_array());
  array->set_elements(elements);
  array->set_length(Smi::FromInt(elements_count));
  // Write in-object properties after the length of the array.
  array->InObjectPropertyAtPut(JSRegExpResult::kIndexIndex, args[1]);
  array->InObjectPropertyAtPut(JSRegExpResult::kInputIndex, args[2]);
  return array;
}


static Object* Runtime_RegExpInitializeObject(Arguments args) {
  AssertNoAllocation no_alloc;
  ASSERT(args.length() == 5);
  CONVERT_CHECKED(JSRegExp, regexp, args[0]);
  CONVERT_CHECKED(String, source, args[1]);

  Object* global = args[2];
  if (!global->IsTrue()) global = Heap::false_value();

  Object* ignoreCase = args[3];
  if (!ignoreCase->IsTrue()) ignoreCase = Heap::false_value();

  Object* multiline = args[4];
  if (!multiline->IsTrue()) multiline = Heap::false_value();

  Map* map = regexp->map();
  Object* constructor = map->constructor();
  if (constructor->IsJSFunction() &&
      JSFunction::cast(constructor)->initial_map() == map) {
    // If we still have the original map, set in-object properties directly.
    regexp->InObjectPropertyAtPut(JSRegExp::kSourceFieldIndex, source);
    // TODO(lrn): Consider skipping write barrier on booleans as well.
    // Both true and false should be in oldspace at all times.
    regexp->InObjectPropertyAtPut(JSRegExp::kGlobalFieldIndex, global);
    regexp->InObjectPropertyAtPut(JSRegExp::kIgnoreCaseFieldIndex, ignoreCase);
    regexp->InObjectPropertyAtPut(JSRegExp::kMultilineFieldIndex, multiline);
    regexp->InObjectPropertyAtPut(JSRegExp::kLastIndexFieldIndex,
                                  Smi::FromInt(0),
                                  SKIP_WRITE_BARRIER);
    return regexp;
  }

  // Map has changed, so use generic, but slower, method.
  PropertyAttributes final =
      static_cast<PropertyAttributes>(READ_ONLY | DONT_ENUM | DONT_DELETE);
  PropertyAttributes writable =
      static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE);
  regexp->IgnoreAttributesAndSetLocalProperty(Heap::source_symbol(),
                                              source,
                                              final);
  regexp->IgnoreAttributesAndSetLocalProperty(Heap::global_symbol(),
                                              global,
                                              final);
  regexp->IgnoreAttributesAndSetLocalProperty(Heap::ignore_case_symbol(),
                                              ignoreCase,
                                              final);
  regexp->IgnoreAttributesAndSetLocalProperty(Heap::multiline_symbol(),
                                              multiline,
                                              final);
  regexp->IgnoreAttributesAndSetLocalProperty(Heap::last_index_symbol(),
                                              Smi::FromInt(0),
                                              writable);
  return regexp;
}


static Object* Runtime_FinishArrayPrototypeSetup(Arguments args) {
  HandleScope scope;
  ASSERT(args.length() == 1);
  CONVERT_ARG_CHECKED(JSArray, prototype, 0);
  // This is necessary to enable fast checks for absence of elements
  // on Array.prototype and below.
  prototype->set_elements(Heap::empty_fixed_array());
  return Smi::FromInt(0);
}


static Handle<JSFunction> InstallBuiltin(Handle<JSObject> holder,
                                         const char* name,
                                         Builtins::Name builtin_name) {
  Handle<String> key = Factory::LookupAsciiSymbol(name);
  Handle<Code> code(Builtins::builtin(builtin_name));
  Handle<JSFunction> optimized = Factory::NewFunction(key,
                                                      JS_OBJECT_TYPE,
                                                      JSObject::kHeaderSize,
                                                      code,
                                                      false);
  optimized->shared()->DontAdaptArguments();
  SetProperty(holder, key, optimized, NONE);
  return optimized;
}


static Object* Runtime_SpecialArrayFunctions(Arguments args) {
  HandleScope scope;
  ASSERT(args.length() == 1);
  CONVERT_ARG_CHECKED(JSObject, holder, 0);

  InstallBuiltin(holder, "pop", Builtins::ArrayPop);
  InstallBuiltin(holder, "push", Builtins::ArrayPush);
  InstallBuiltin(holder, "shift", Builtins::ArrayShift);
  InstallBuiltin(holder, "unshift", Builtins::ArrayUnshift);
  InstallBuiltin(holder, "slice", Builtins::ArraySlice);
  InstallBuiltin(holder, "splice", Builtins::ArraySplice);
  InstallBuiltin(holder, "concat", Builtins::ArrayConcat);

  return *holder;
}


static Object* Runtime_GetGlobalReceiver(Arguments args) {
  // Returns a real global receiver, not one of builtins object.
  Context* global_context = Top::context()->global()->global_context();
  return global_context->global()->global_receiver();
}


static Object* Runtime_MaterializeRegExpLiteral(Arguments args) {
  HandleScope scope;
  ASSERT(args.length() == 4);
  CONVERT_ARG_CHECKED(FixedArray, literals, 0);
  int index = Smi::cast(args[1])->value();
  Handle<String> pattern = args.at<String>(2);
  Handle<String> flags = args.at<String>(3);

  // Get the RegExp function from the context in the literals array.
  // This is the RegExp function from the context in which the
  // function was created.  We do not use the RegExp function from the
  // current global context because this might be the RegExp function
  // from another context which we should not have access to.
  Handle<JSFunction> constructor =
      Handle<JSFunction>(
          JSFunction::GlobalContextFromLiterals(*literals)->regexp_function());
  // Compute the regular expression literal.
  bool has_pending_exception;
  Handle<Object> regexp =
      RegExpImpl::CreateRegExpLiteral(constructor, pattern, flags,
                                      &has_pending_exception);
  if (has_pending_exception) {
    ASSERT(Top::has_pending_exception());
    return Failure::Exception();
  }
  literals->set(index, *regexp);
  return *regexp;
}


static Object* Runtime_FunctionGetName(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);

  CONVERT_CHECKED(JSFunction, f, args[0]);
  return f->shared()->name();
}


static Object* Runtime_FunctionSetName(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);

  CONVERT_CHECKED(JSFunction, f, args[0]);
  CONVERT_CHECKED(String, name, args[1]);
  f->shared()->set_name(name);
  return Heap::undefined_value();
}


static Object* Runtime_FunctionRemovePrototype(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);

  CONVERT_CHECKED(JSFunction, f, args[0]);
  Object* obj = f->RemovePrototype();
  if (obj->IsFailure()) return obj;

  return Heap::undefined_value();
}


static Object* Runtime_FunctionGetScript(Arguments args) {
  HandleScope scope;
  ASSERT(args.length() == 1);

  CONVERT_CHECKED(JSFunction, fun, args[0]);
  Handle<Object> script = Handle<Object>(fun->shared()->script());
  if (!script->IsScript()) return Heap::undefined_value();

  return *GetScriptWrapper(Handle<Script>::cast(script));
}


static Object* Runtime_FunctionGetSourceCode(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);

  CONVERT_CHECKED(JSFunction, f, args[0]);
  return f->shared()->GetSourceCode();
}


static Object* Runtime_FunctionGetScriptSourcePosition(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);

  CONVERT_CHECKED(JSFunction, fun, args[0]);
  int pos = fun->shared()->start_position();
  return Smi::FromInt(pos);
}


static Object* Runtime_FunctionGetPositionForOffset(Arguments args) {
  ASSERT(args.length() == 2);

  CONVERT_CHECKED(JSFunction, fun, args[0]);
  CONVERT_NUMBER_CHECKED(int, offset, Int32, args[1]);

  Code* code = fun->code();
  RUNTIME_ASSERT(0 <= offset && offset < code->Size());

  Address pc = code->address() + offset;
  return Smi::FromInt(fun->code()->SourcePosition(pc));
}



static Object* Runtime_FunctionSetInstanceClassName(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);

  CONVERT_CHECKED(JSFunction, fun, args[0]);
  CONVERT_CHECKED(String, name, args[1]);
  fun->SetInstanceClassName(name);
  return Heap::undefined_value();
}


static Object* Runtime_FunctionSetLength(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);

  CONVERT_CHECKED(JSFunction, fun, args[0]);
  CONVERT_CHECKED(Smi, length, args[1]);
  fun->shared()->set_length(length->value());
  return length;
}


static Object* Runtime_FunctionSetPrototype(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);

  CONVERT_CHECKED(JSFunction, fun, args[0]);
  ASSERT(fun->should_have_prototype());
  Object* obj = Accessors::FunctionSetPrototype(fun, args[1], NULL);
  if (obj->IsFailure()) return obj;
  return args[0];  // return TOS
}


static Object* Runtime_FunctionIsAPIFunction(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);

  CONVERT_CHECKED(JSFunction, f, args[0]);
  return f->shared()->IsApiFunction() ? Heap::true_value()
                                      : Heap::false_value();
}

static Object* Runtime_FunctionIsBuiltin(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);

  CONVERT_CHECKED(JSFunction, f, args[0]);
  return f->IsBuiltin() ? Heap::true_value() : Heap::false_value();
}


static Object* Runtime_SetCode(Arguments args) {
  HandleScope scope;
  ASSERT(args.length() == 2);

  CONVERT_ARG_CHECKED(JSFunction, target, 0);
  Handle<Object> code = args.at<Object>(1);

  Handle<Context> context(target->context());

  if (!code->IsNull()) {
    RUNTIME_ASSERT(code->IsJSFunction());
    Handle<JSFunction> fun = Handle<JSFunction>::cast(code);
    Handle<SharedFunctionInfo> shared(fun->shared());
    SetExpectedNofProperties(target, shared->expected_nof_properties());

    if (!EnsureCompiled(shared, KEEP_EXCEPTION)) {
      return Failure::Exception();
    }
    // Set the code, scope info, formal parameter count,
    // and the length of the target function.
    target->set_code(fun->code());
    target->shared()->set_scope_info(shared->scope_info());
    target->shared()->set_length(shared->length());
    target->shared()->set_formal_parameter_count(
        shared->formal_parameter_count());
    // Set the source code of the target function to undefined.
    // SetCode is only used for built-in constructors like String,
    // Array, and Object, and some web code
    // doesn't like seeing source code for constructors.
    target->shared()->set_script(Heap::undefined_value());
    // Clear the optimization hints related to the compiled code as these are no
    // longer valid when the code is overwritten.
    target->shared()->ClearThisPropertyAssignmentsInfo();
    context = Handle<Context>(fun->context());

    // Make sure we get a fresh copy of the literal vector to avoid
    // cross context contamination.
    int number_of_literals = fun->NumberOfLiterals();
    Handle<FixedArray> literals =
        Factory::NewFixedArray(number_of_literals, TENURED);
    if (number_of_literals > 0) {
      // Insert the object, regexp and array functions in the literals
      // array prefix.  These are the functions that will be used when
      // creating object, regexp and array literals.
      literals->set(JSFunction::kLiteralGlobalContextIndex,
                    context->global_context());
    }
    // It's okay to skip the write barrier here because the literals
    // are guaranteed to be in old space.
    target->set_literals(*literals, SKIP_WRITE_BARRIER);
  }

  target->set_context(*context);
  return *target;
}


static Object* CharFromCode(Object* char_code) {
  uint32_t code;
  if (char_code->ToArrayIndex(&code)) {
    if (code <= 0xffff) {
      return Heap::LookupSingleCharacterStringFromCode(code);
    }
  }
  return Heap::empty_string();
}


static Object* Runtime_StringCharCodeAt(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);

  CONVERT_CHECKED(String, subject, args[0]);
  Object* index = args[1];
  RUNTIME_ASSERT(index->IsNumber());

  uint32_t i = 0;
  if (index->IsSmi()) {
    int value = Smi::cast(index)->value();
    if (value < 0) return Heap::nan_value();
    i = value;
  } else {
    ASSERT(index->IsHeapNumber());
    double value = HeapNumber::cast(index)->value();
    i = static_cast<uint32_t>(DoubleToInteger(value));
  }

  // Flatten the string.  If someone wants to get a char at an index
  // in a cons string, it is likely that more indices will be
  // accessed.
  Object* flat = subject->TryFlatten();
  if (flat->IsFailure()) return flat;
  subject = String::cast(flat);

  if (i >= static_cast<uint32_t>(subject->length())) {
    return Heap::nan_value();
  }

  return Smi::FromInt(subject->Get(i));
}


static Object* Runtime_CharFromCode(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);
  return CharFromCode(args[0]);
}


class FixedArrayBuilder {
 public:
  explicit FixedArrayBuilder(int initial_capacity)
      : array_(Factory::NewFixedArrayWithHoles(initial_capacity)),
        length_(0) {
    // Require a non-zero initial size. Ensures that doubling the size to
    // extend the array will work.
    ASSERT(initial_capacity > 0);
  }

  explicit FixedArrayBuilder(Handle<FixedArray> backing_store)
      : array_(backing_store),
        length_(0) {
    // Require a non-zero initial size. Ensures that doubling the size to
    // extend the array will work.
    ASSERT(backing_store->length() > 0);
  }

  bool HasCapacity(int elements) {
    int length = array_->length();
    int required_length = length_ + elements;
    return (length >= required_length);
  }

  void EnsureCapacity(int elements) {
    int length = array_->length();
    int required_length = length_ + elements;
    if (length < required_length) {
      int new_length = length;
      do {
        new_length *= 2;
      } while (new_length < required_length);
      Handle<FixedArray> extended_array =
          Factory::NewFixedArrayWithHoles(new_length);
      array_->CopyTo(0, *extended_array, 0, length_);
      array_ = extended_array;
    }
  }

  void Add(Object* value) {
    ASSERT(length_ < capacity());
    array_->set(length_, value);
    length_++;
  }

  void Add(Smi* value) {
    ASSERT(length_ < capacity());
    array_->set(length_, value);
    length_++;
  }

  Handle<FixedArray> array() {
    return array_;
  }

  int length() {
    return length_;
  }

  int capacity() {
    return array_->length();
  }

  Handle<JSArray> ToJSArray() {
    Handle<JSArray> result_array = Factory::NewJSArrayWithElements(array_);
    result_array->set_length(Smi::FromInt(length_));
    return result_array;
  }

  Handle<JSArray> ToJSArray(Handle<JSArray> target_array) {
    target_array->set_elements(*array_);
    target_array->set_length(Smi::FromInt(length_));
    return target_array;
  }

 private:
  Handle<FixedArray> array_;
  int length_;
};


// Forward declarations.
const int kStringBuilderConcatHelperLengthBits = 11;
const int kStringBuilderConcatHelperPositionBits = 19;

template <typename schar>
static inline void StringBuilderConcatHelper(String*,
                                             schar*,
                                             FixedArray*,
                                             int);

typedef BitField<int, 0, kStringBuilderConcatHelperLengthBits>
    StringBuilderSubstringLength;
typedef BitField<int,
                 kStringBuilderConcatHelperLengthBits,
                 kStringBuilderConcatHelperPositionBits>
    StringBuilderSubstringPosition;


class ReplacementStringBuilder {
 public:
  ReplacementStringBuilder(Handle<String> subject, int estimated_part_count)
      : array_builder_(estimated_part_count),
        subject_(subject),
        character_count_(0),
        is_ascii_(subject->IsAsciiRepresentation()) {
    // Require a non-zero initial size. Ensures that doubling the size to
    // extend the array will work.
    ASSERT(estimated_part_count > 0);
  }

  static inline void AddSubjectSlice(FixedArrayBuilder* builder,
                                     int from,
                                     int to) {
    ASSERT(from >= 0);
    int length = to - from;
    ASSERT(length > 0);
    if (StringBuilderSubstringLength::is_valid(length) &&
        StringBuilderSubstringPosition::is_valid(from)) {
      int encoded_slice = StringBuilderSubstringLength::encode(length) |
          StringBuilderSubstringPosition::encode(from);
      builder->Add(Smi::FromInt(encoded_slice));
    } else {
      // Otherwise encode as two smis.
      builder->Add(Smi::FromInt(-length));
      builder->Add(Smi::FromInt(from));
    }
  }


  void EnsureCapacity(int elements) {
    array_builder_.EnsureCapacity(elements);
  }


  void AddSubjectSlice(int from, int to) {
    AddSubjectSlice(&array_builder_, from, to);
    IncrementCharacterCount(to - from);
  }


  void AddString(Handle<String> string) {
    int length = string->length();
    ASSERT(length > 0);
    AddElement(*string);
    if (!string->IsAsciiRepresentation()) {
      is_ascii_ = false;
    }
    IncrementCharacterCount(length);
  }


  Handle<String> ToString() {
    if (array_builder_.length() == 0) {
      return Factory::empty_string();
    }

    Handle<String> joined_string;
    if (is_ascii_) {
      joined_string = NewRawAsciiString(character_count_);
      AssertNoAllocation no_alloc;
      SeqAsciiString* seq = SeqAsciiString::cast(*joined_string);
      char* char_buffer = seq->GetChars();
      StringBuilderConcatHelper(*subject_,
                                char_buffer,
                                *array_builder_.array(),
                                array_builder_.length());
    } else {
      // Non-ASCII.
      joined_string = NewRawTwoByteString(character_count_);
      AssertNoAllocation no_alloc;
      SeqTwoByteString* seq = SeqTwoByteString::cast(*joined_string);
      uc16* char_buffer = seq->GetChars();
      StringBuilderConcatHelper(*subject_,
                                char_buffer,
                                *array_builder_.array(),
                                array_builder_.length());
    }
    return joined_string;
  }


  void IncrementCharacterCount(int by) {
    if (character_count_ > String::kMaxLength - by) {
      V8::FatalProcessOutOfMemory("String.replace result too large.");
    }
    character_count_ += by;
  }

  Handle<JSArray> GetParts() {
    Handle<JSArray> result =
        Factory::NewJSArrayWithElements(array_builder_.array());
    result->set_length(Smi::FromInt(array_builder_.length()));
    return result;
  }

 private:
  Handle<String> NewRawAsciiString(int size) {
    CALL_HEAP_FUNCTION(Heap::AllocateRawAsciiString(size), String);
  }


  Handle<String> NewRawTwoByteString(int size) {
    CALL_HEAP_FUNCTION(Heap::AllocateRawTwoByteString(size), String);
  }


  void AddElement(Object* element) {
    ASSERT(element->IsSmi() || element->IsString());
    ASSERT(array_builder_.capacity() > array_builder_.length());
    array_builder_.Add(element);
  }

  FixedArrayBuilder array_builder_;
  Handle<String> subject_;
  int character_count_;
  bool is_ascii_;
};


class CompiledReplacement {
 public:
  CompiledReplacement()
      : parts_(1), replacement_substrings_(0) {}

  void Compile(Handle<String> replacement,
               int capture_count,
               int subject_length);

  void Apply(ReplacementStringBuilder* builder,
             int match_from,
             int match_to,
             Handle<JSArray> last_match_info);

  // Number of distinct parts of the replacement pattern.
  int parts() {
    return parts_.length();
  }
 private:
  enum PartType {
    SUBJECT_PREFIX = 1,
    SUBJECT_SUFFIX,
    SUBJECT_CAPTURE,
    REPLACEMENT_SUBSTRING,
    REPLACEMENT_STRING,

    NUMBER_OF_PART_TYPES
  };

  struct ReplacementPart {
    static inline ReplacementPart SubjectMatch() {
      return ReplacementPart(SUBJECT_CAPTURE, 0);
    }
    static inline ReplacementPart SubjectCapture(int capture_index) {
      return ReplacementPart(SUBJECT_CAPTURE, capture_index);
    }
    static inline ReplacementPart SubjectPrefix() {
      return ReplacementPart(SUBJECT_PREFIX, 0);
    }
    static inline ReplacementPart SubjectSuffix(int subject_length) {
      return ReplacementPart(SUBJECT_SUFFIX, subject_length);
    }
    static inline ReplacementPart ReplacementString() {
      return ReplacementPart(REPLACEMENT_STRING, 0);
    }
    static inline ReplacementPart ReplacementSubString(int from, int to) {
      ASSERT(from >= 0);
      ASSERT(to > from);
      return ReplacementPart(-from, to);
    }

    // If tag <= 0 then it is the negation of a start index of a substring of
    // the replacement pattern, otherwise it's a value from PartType.
    ReplacementPart(int tag, int data)
        : tag(tag), data(data) {
      // Must be non-positive or a PartType value.
      ASSERT(tag < NUMBER_OF_PART_TYPES);
    }
    // Either a value of PartType or a non-positive number that is
    // the negation of an index into the replacement string.
    int tag;
    // The data value's interpretation depends on the value of tag:
    // tag == SUBJECT_PREFIX ||
    // tag == SUBJECT_SUFFIX:  data is unused.
    // tag == SUBJECT_CAPTURE: data is the number of the capture.
    // tag == REPLACEMENT_SUBSTRING ||
    // tag == REPLACEMENT_STRING:    data is index into array of substrings
    //                               of the replacement string.
    // tag <= 0: Temporary representation of the substring of the replacement
    //           string ranging over -tag .. data.
    //           Is replaced by REPLACEMENT_{SUB,}STRING when we create the
    //           substring objects.
    int data;
  };

  template<typename Char>
  static void ParseReplacementPattern(ZoneList<ReplacementPart>* parts,
                                      Vector<Char> characters,
                                      int capture_count,
                                      int subject_length) {
    int length = characters.length();
    int last = 0;
    for (int i = 0; i < length; i++) {
      Char c = characters[i];
      if (c == '$') {
        int next_index = i + 1;
        if (next_index == length) {  // No next character!
          break;
        }
        Char c2 = characters[next_index];
        switch (c2) {
        case '$':
          if (i > last) {
            // There is a substring before. Include the first "$".
            parts->Add(ReplacementPart::ReplacementSubString(last, next_index));
            last = next_index + 1;  // Continue after the second "$".
          } else {
            // Let the next substring start with the second "$".
            last = next_index;
          }
          i = next_index;
          break;
        case '`':
          if (i > last) {
            parts->Add(ReplacementPart::ReplacementSubString(last, i));
          }
          parts->Add(ReplacementPart::SubjectPrefix());
          i = next_index;
          last = i + 1;
          break;
        case '\'':
          if (i > last) {
            parts->Add(ReplacementPart::ReplacementSubString(last, i));
          }
          parts->Add(ReplacementPart::SubjectSuffix(subject_length));
          i = next_index;
          last = i + 1;
          break;
        case '&':
          if (i > last) {
            parts->Add(ReplacementPart::ReplacementSubString(last, i));
          }
          parts->Add(ReplacementPart::SubjectMatch());
          i = next_index;
          last = i + 1;
          break;
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9': {
          int capture_ref = c2 - '0';
          if (capture_ref > capture_count) {
            i = next_index;
            continue;
          }
          int second_digit_index = next_index + 1;
          if (second_digit_index < length) {
            // Peek ahead to see if we have two digits.
            Char c3 = characters[second_digit_index];
            if ('0' <= c3 && c3 <= '9') {  // Double digits.
              int double_digit_ref = capture_ref * 10 + c3 - '0';
              if (double_digit_ref <= capture_count) {
                next_index = second_digit_index;
                capture_ref = double_digit_ref;
              }
            }
          }
          if (capture_ref > 0) {
            if (i > last) {
              parts->Add(ReplacementPart::ReplacementSubString(last, i));
            }
            ASSERT(capture_ref <= capture_count);
            parts->Add(ReplacementPart::SubjectCapture(capture_ref));
            last = next_index + 1;
          }
          i = next_index;
          break;
        }
        default:
          i = next_index;
          break;
        }
      }
    }
    if (length > last) {
      if (last == 0) {
        parts->Add(ReplacementPart::ReplacementString());
      } else {
        parts->Add(ReplacementPart::ReplacementSubString(last, length));
      }
    }
  }

  ZoneList<ReplacementPart> parts_;
  ZoneList<Handle<String> > replacement_substrings_;
};


void CompiledReplacement::Compile(Handle<String> replacement,
                                  int capture_count,
                                  int subject_length) {
  ASSERT(replacement->IsFlat());
  if (replacement->IsAsciiRepresentation()) {
    AssertNoAllocation no_alloc;
    ParseReplacementPattern(&parts_,
                            replacement->ToAsciiVector(),
                            capture_count,
                            subject_length);
  } else {
    ASSERT(replacement->IsTwoByteRepresentation());
    AssertNoAllocation no_alloc;

    ParseReplacementPattern(&parts_,
                            replacement->ToUC16Vector(),
                            capture_count,
                            subject_length);
  }
  // Find substrings of replacement string and create them as String objects.
  int substring_index = 0;
  for (int i = 0, n = parts_.length(); i < n; i++) {
    int tag = parts_[i].tag;
    if (tag <= 0) {  // A replacement string slice.
      int from = -tag;
      int to = parts_[i].data;
      replacement_substrings_.Add(Factory::NewSubString(replacement, from, to));
      parts_[i].tag = REPLACEMENT_SUBSTRING;
      parts_[i].data = substring_index;
      substring_index++;
    } else if (tag == REPLACEMENT_STRING) {
      replacement_substrings_.Add(replacement);
      parts_[i].data = substring_index;
      substring_index++;
    }
  }
}


void CompiledReplacement::Apply(ReplacementStringBuilder* builder,
                                int match_from,
                                int match_to,
                                Handle<JSArray> last_match_info) {
  for (int i = 0, n = parts_.length(); i < n; i++) {
    ReplacementPart part = parts_[i];
    switch (part.tag) {
      case SUBJECT_PREFIX:
        if (match_from > 0) builder->AddSubjectSlice(0, match_from);
        break;
      case SUBJECT_SUFFIX: {
        int subject_length = part.data;
        if (match_to < subject_length) {
          builder->AddSubjectSlice(match_to, subject_length);
        }
        break;
      }
      case SUBJECT_CAPTURE: {
        int capture = part.data;
        FixedArray* match_info = FixedArray::cast(last_match_info->elements());
        int from = RegExpImpl::GetCapture(match_info, capture * 2);
        int to = RegExpImpl::GetCapture(match_info, capture * 2 + 1);
        if (from >= 0 && to > from) {
          builder->AddSubjectSlice(from, to);
        }
        break;
      }
      case REPLACEMENT_SUBSTRING:
      case REPLACEMENT_STRING:
        builder->AddString(replacement_substrings_[part.data]);
        break;
      default:
        UNREACHABLE();
    }
  }
}



static Object* StringReplaceRegExpWithString(String* subject,
                                             JSRegExp* regexp,
                                             String* replacement,
                                             JSArray* last_match_info) {
  ASSERT(subject->IsFlat());
  ASSERT(replacement->IsFlat());

  HandleScope handles;

  int length = subject->length();
  Handle<String> subject_handle(subject);
  Handle<JSRegExp> regexp_handle(regexp);
  Handle<String> replacement_handle(replacement);
  Handle<JSArray> last_match_info_handle(last_match_info);
  Handle<Object> match = RegExpImpl::Exec(regexp_handle,
                                          subject_handle,
                                          0,
                                          last_match_info_handle);
  if (match.is_null()) {
    return Failure::Exception();
  }
  if (match->IsNull()) {
    return *subject_handle;
  }

  int capture_count = regexp_handle->CaptureCount();

  // CompiledReplacement uses zone allocation.
  CompilationZoneScope zone(DELETE_ON_EXIT);
  CompiledReplacement compiled_replacement;
  compiled_replacement.Compile(replacement_handle,
                               capture_count,
                               length);

  bool is_global = regexp_handle->GetFlags().is_global();

  // Guessing the number of parts that the final result string is built
  // from. Global regexps can match any number of times, so we guess
  // conservatively.
  int expected_parts =
      (compiled_replacement.parts() + 1) * (is_global ? 4 : 1) + 1;
  ReplacementStringBuilder builder(subject_handle, expected_parts);

  // Index of end of last match.
  int prev = 0;

  // Number of parts added by compiled replacement plus preceeding
  // string and possibly suffix after last match.  It is possible for
  // all components to use two elements when encoded as two smis.
  const int parts_added_per_loop = 2 * (compiled_replacement.parts() + 2);
  bool matched = true;
  do {
    ASSERT(last_match_info_handle->HasFastElements());
    // Increase the capacity of the builder before entering local handle-scope,
    // so its internal buffer can safely allocate a new handle if it grows.
    builder.EnsureCapacity(parts_added_per_loop);

    HandleScope loop_scope;
    int start, end;
    {
      AssertNoAllocation match_info_array_is_not_in_a_handle;
      FixedArray* match_info_array =
          FixedArray::cast(last_match_info_handle->elements());

      ASSERT_EQ(capture_count * 2 + 2,
                RegExpImpl::GetLastCaptureCount(match_info_array));
      start = RegExpImpl::GetCapture(match_info_array, 0);
      end = RegExpImpl::GetCapture(match_info_array, 1);
    }

    if (prev < start) {
      builder.AddSubjectSlice(prev, start);
    }
    compiled_replacement.Apply(&builder,
                               start,
                               end,
                               last_match_info_handle);
    prev = end;

    // Only continue checking for global regexps.
    if (!is_global) break;

    // Continue from where the match ended, unless it was an empty match.
    int next = end;
    if (start == end) {
      next = end + 1;
      if (next > length) break;
    }

    match = RegExpImpl::Exec(regexp_handle,
                             subject_handle,
                             next,
                             last_match_info_handle);
    if (match.is_null()) {
      return Failure::Exception();
    }
    matched = !match->IsNull();
  } while (matched);

  if (prev < length) {
    builder.AddSubjectSlice(prev, length);
  }

  return *(builder.ToString());
}


template <typename ResultSeqString>
static Object* StringReplaceRegExpWithEmptyString(String* subject,
                                                  JSRegExp* regexp,
                                                  JSArray* last_match_info) {
  ASSERT(subject->IsFlat());

  HandleScope handles;

  Handle<String> subject_handle(subject);
  Handle<JSRegExp> regexp_handle(regexp);
  Handle<JSArray> last_match_info_handle(last_match_info);
  Handle<Object> match = RegExpImpl::Exec(regexp_handle,
                                          subject_handle,
                                          0,
                                          last_match_info_handle);
  if (match.is_null()) return Failure::Exception();
  if (match->IsNull()) return *subject_handle;

  ASSERT(last_match_info_handle->HasFastElements());

  HandleScope loop_scope;
  int start, end;
  {
    AssertNoAllocation match_info_array_is_not_in_a_handle;
    FixedArray* match_info_array =
        FixedArray::cast(last_match_info_handle->elements());

    start = RegExpImpl::GetCapture(match_info_array, 0);
    end = RegExpImpl::GetCapture(match_info_array, 1);
  }

  int length = subject->length();
  int new_length = length - (end - start);
  if (new_length == 0) {
    return Heap::empty_string();
  }
  Handle<ResultSeqString> answer;
  if (ResultSeqString::kHasAsciiEncoding) {
    answer =
        Handle<ResultSeqString>::cast(Factory::NewRawAsciiString(new_length));
  } else {
    answer =
        Handle<ResultSeqString>::cast(Factory::NewRawTwoByteString(new_length));
  }

  // If the regexp isn't global, only match once.
  if (!regexp_handle->GetFlags().is_global()) {
    if (start > 0) {
      String::WriteToFlat(*subject_handle,
                          answer->GetChars(),
                          0,
                          start);
    }
    if (end < length) {
      String::WriteToFlat(*subject_handle,
                          answer->GetChars() + start,
                          end,
                          length);
    }
    return *answer;
  }

  int prev = 0;  // Index of end of last match.
  int next = 0;  // Start of next search (prev unless last match was empty).
  int position = 0;

  do {
    if (prev < start) {
      // Add substring subject[prev;start] to answer string.
      String::WriteToFlat(*subject_handle,
                          answer->GetChars() + position,
                          prev,
                          start);
      position += start - prev;
    }
    prev = end;
    next = end;
    // Continue from where the match ended, unless it was an empty match.
    if (start == end) {
      next++;
      if (next > length) break;
    }
    match = RegExpImpl::Exec(regexp_handle,
                             subject_handle,
                             next,
                             last_match_info_handle);
    if (match.is_null()) return Failure::Exception();
    if (match->IsNull()) break;

    ASSERT(last_match_info_handle->HasFastElements());
    HandleScope loop_scope;
    {
      AssertNoAllocation match_info_array_is_not_in_a_handle;
      FixedArray* match_info_array =
          FixedArray::cast(last_match_info_handle->elements());
      start = RegExpImpl::GetCapture(match_info_array, 0);
      end = RegExpImpl::GetCapture(match_info_array, 1);
    }
  } while (true);

  if (prev < length) {
    // Add substring subject[prev;length] to answer string.
    String::WriteToFlat(*subject_handle,
                        answer->GetChars() + position,
                        prev,
                        length);
    position += length - prev;
  }

  if (position == 0) {
    return Heap::empty_string();
  }

  // Shorten string and fill
  int string_size = ResultSeqString::SizeFor(position);
  int allocated_string_size = ResultSeqString::SizeFor(new_length);
  int delta = allocated_string_size - string_size;

  answer->set_length(position);
  if (delta == 0) return *answer;

  Address end_of_string = answer->address() + string_size;
  Heap::CreateFillerObjectAt(end_of_string, delta);

  return *answer;
}


static Object* Runtime_StringReplaceRegExpWithString(Arguments args) {
  ASSERT(args.length() == 4);

  CONVERT_CHECKED(String, subject, args[0]);
  if (!subject->IsFlat()) {
    Object* flat_subject = subject->TryFlatten();
    if (flat_subject->IsFailure()) {
      return flat_subject;
    }
    subject = String::cast(flat_subject);
  }

  CONVERT_CHECKED(String, replacement, args[2]);
  if (!replacement->IsFlat()) {
    Object* flat_replacement = replacement->TryFlatten();
    if (flat_replacement->IsFailure()) {
      return flat_replacement;
    }
    replacement = String::cast(flat_replacement);
  }

  CONVERT_CHECKED(JSRegExp, regexp, args[1]);
  CONVERT_CHECKED(JSArray, last_match_info, args[3]);

  ASSERT(last_match_info->HasFastElements());

  if (replacement->length() == 0) {
    if (subject->HasOnlyAsciiChars()) {
      return StringReplaceRegExpWithEmptyString<SeqAsciiString>(
          subject, regexp, last_match_info);
    } else {
      return StringReplaceRegExpWithEmptyString<SeqTwoByteString>(
          subject, regexp, last_match_info);
    }
  }

  return StringReplaceRegExpWithString(subject,
                                       regexp,
                                       replacement,
                                       last_match_info);
}


// Cap on the maximal shift in the Boyer-Moore implementation. By setting a
// limit, we can fix the size of tables.
static const int kBMMaxShift = 0xff;
// Reduce alphabet to this size.
static const int kBMAlphabetSize = 0x100;
// For patterns below this length, the skip length of Boyer-Moore is too short
// to compensate for the algorithmic overhead compared to simple brute force.
static const int kBMMinPatternLength = 5;

// Holds the two buffers used by Boyer-Moore string search's Good Suffix
// shift. Only allows the last kBMMaxShift characters of the needle
// to be indexed.
class BMGoodSuffixBuffers {
 public:
  BMGoodSuffixBuffers() {}
  inline void init(int needle_length) {
    ASSERT(needle_length > 1);
    int start = needle_length < kBMMaxShift ? 0 : needle_length - kBMMaxShift;
    int len = needle_length - start;
    biased_suffixes_ = suffixes_ - start;
    biased_good_suffix_shift_ = good_suffix_shift_ - start;
    for (int i = 0; i <= len; i++) {
      good_suffix_shift_[i] = len;
    }
  }
  inline int& suffix(int index) {
    ASSERT(biased_suffixes_ + index >= suffixes_);
    return biased_suffixes_[index];
  }
  inline int& shift(int index) {
    ASSERT(biased_good_suffix_shift_ + index >= good_suffix_shift_);
    return biased_good_suffix_shift_[index];
  }
 private:
  int suffixes_[kBMMaxShift + 1];
  int good_suffix_shift_[kBMMaxShift + 1];
  int* biased_suffixes_;
  int* biased_good_suffix_shift_;
  DISALLOW_COPY_AND_ASSIGN(BMGoodSuffixBuffers);
};

// buffers reused by BoyerMoore
static int bad_char_occurrence[kBMAlphabetSize];
static BMGoodSuffixBuffers bmgs_buffers;

// State of the string match tables.
// SIMPLE: No usable content in the buffers.
// BOYER_MOORE_HORSPOOL: The bad_char_occurences table has been populated.
// BOYER_MOORE: The bmgs_buffers tables have also been populated.
// Whenever starting with a new needle, one should call InitializeStringSearch
// to determine which search strategy to use, and in the case of a long-needle
// strategy, the call also initializes the algorithm to SIMPLE.
enum StringSearchAlgorithm { SIMPLE_SEARCH, BOYER_MOORE_HORSPOOL, BOYER_MOORE };
static StringSearchAlgorithm algorithm;


// Compute the bad-char table for Boyer-Moore in the static buffer.
template <typename pchar>
static void BoyerMoorePopulateBadCharTable(Vector<const pchar> pattern) {
  // Only preprocess at most kBMMaxShift last characters of pattern.
  int start = pattern.length() < kBMMaxShift ? 0
                                             : pattern.length() - kBMMaxShift;
  // Run forwards to populate bad_char_table, so that *last* instance
  // of character equivalence class is the one registered.
  // Notice: Doesn't include the last character.
  int table_size = (sizeof(pchar) == 1) ? String::kMaxAsciiCharCode + 1
                                        : kBMAlphabetSize;
  if (start == 0) {  // All patterns less than kBMMaxShift in length.
    memset(bad_char_occurrence, -1, table_size * sizeof(*bad_char_occurrence));
  } else {
    for (int i = 0; i < table_size; i++) {
      bad_char_occurrence[i] = start - 1;
    }
  }
  for (int i = start; i < pattern.length() - 1; i++) {
    pchar c = pattern[i];
    int bucket = (sizeof(pchar) ==1) ? c : c % kBMAlphabetSize;
    bad_char_occurrence[bucket] = i;
  }
}


template <typename pchar>
static void BoyerMoorePopulateGoodSuffixTable(Vector<const pchar> pattern) {
  int m = pattern.length();
  int start = m < kBMMaxShift ? 0 : m - kBMMaxShift;
  int len = m - start;
  // Compute Good Suffix tables.
  bmgs_buffers.init(m);

  bmgs_buffers.shift(m-1) = 1;
  bmgs_buffers.suffix(m) = m + 1;
  pchar last_char = pattern[m - 1];
  int suffix = m + 1;
  for (int i = m; i > start;) {
    for (pchar c = pattern[i - 1]; suffix <= m && c != pattern[suffix - 1];) {
      if (bmgs_buffers.shift(suffix) == len) {
        bmgs_buffers.shift(suffix) = suffix - i;
      }
      suffix = bmgs_buffers.suffix(suffix);
    }
    i--;
    suffix--;
    bmgs_buffers.suffix(i) = suffix;
    if (suffix == m) {
      // No suffix to extend, so we check against last_char only.
      while (i > start && pattern[i - 1] != last_char) {
        if (bmgs_buffers.shift(m) == len) {
          bmgs_buffers.shift(m) = m - i;
        }
        i--;
        bmgs_buffers.suffix(i) = m;
      }
      if (i > start) {
        i--;
        suffix--;
        bmgs_buffers.suffix(i) = suffix;
      }
    }
  }
  if (suffix < m) {
    for (int i = start; i <= m; i++) {
      if (bmgs_buffers.shift(i) == len) {
        bmgs_buffers.shift(i) = suffix - start;
      }
      if (i == suffix) {
        suffix = bmgs_buffers.suffix(suffix);
      }
    }
  }
}


template <typename schar, typename pchar>
static inline int CharOccurrence(int char_code) {
  if (sizeof(schar) == 1) {
    return bad_char_occurrence[char_code];
  }
  if (sizeof(pchar) == 1) {
    if (char_code > String::kMaxAsciiCharCode) {
      return -1;
    }
    return bad_char_occurrence[char_code];
  }
  return bad_char_occurrence[char_code % kBMAlphabetSize];
}


// Restricted simplified Boyer-Moore string matching.
// Uses only the bad-shift table of Boyer-Moore and only uses it
// for the character compared to the last character of the needle.
template <typename schar, typename pchar>
static int BoyerMooreHorspool(Vector<const schar> subject,
                              Vector<const pchar> pattern,
                              int start_index,
                              bool* complete) {
  ASSERT(algorithm <= BOYER_MOORE_HORSPOOL);
  int n = subject.length();
  int m = pattern.length();

  int badness = -m;

  // How bad we are doing without a good-suffix table.
  int idx;  // No matches found prior to this index.
  pchar last_char = pattern[m - 1];
  int last_char_shift = m - 1 - CharOccurrence<schar, pchar>(last_char);
  // Perform search
  for (idx = start_index; idx <= n - m;) {
    int j = m - 1;
    int c;
    while (last_char != (c = subject[idx + j])) {
      int bc_occ = CharOccurrence<schar, pchar>(c);
      int shift = j - bc_occ;
      idx += shift;
      badness += 1 - shift;  // at most zero, so badness cannot increase.
      if (idx > n - m) {
        *complete = true;
        return -1;
      }
    }
    j--;
    while (j >= 0 && pattern[j] == (subject[idx + j])) j--;
    if (j < 0) {
      *complete = true;
      return idx;
    } else {
      idx += last_char_shift;
      // Badness increases by the number of characters we have
      // checked, and decreases by the number of characters we
      // can skip by shifting. It's a measure of how we are doing
      // compared to reading each character exactly once.
      badness += (m - j) - last_char_shift;
      if (badness > 0) {
        *complete = false;
        return idx;
      }
    }
  }
  *complete = true;
  return -1;
}


template <typename schar, typename pchar>
static int BoyerMooreIndexOf(Vector<const schar> subject,
                             Vector<const pchar> pattern,
                             int idx) {
  ASSERT(algorithm <= BOYER_MOORE);
  int n = subject.length();
  int m = pattern.length();
  // Only preprocess at most kBMMaxShift last characters of pattern.
  int start = m < kBMMaxShift ? 0 : m - kBMMaxShift;

  pchar last_char = pattern[m - 1];
  // Continue search from i.
  while (idx <= n - m) {
    int j = m - 1;
    schar c;
    while (last_char != (c = subject[idx + j])) {
      int shift = j - CharOccurrence<schar, pchar>(c);
      idx += shift;
      if (idx > n - m) {
        return -1;
      }
    }
    while (j >= 0 && pattern[j] == (c = subject[idx + j])) j--;
    if (j < 0) {
      return idx;
    } else if (j < start) {
      // we have matched more than our tables allow us to be smart about.
      // Fall back on BMH shift.
      idx += m - 1 - CharOccurrence<schar, pchar>(last_char);
    } else {
      int gs_shift = bmgs_buffers.shift(j + 1);       // Good suffix shift.
      int bc_occ = CharOccurrence<schar, pchar>(c);
      int shift = j - bc_occ;                         // Bad-char shift.
      if (gs_shift > shift) {
        shift = gs_shift;
      }
      idx += shift;
    }
  }

  return -1;
}


template <typename schar>
static inline int SingleCharIndexOf(Vector<const schar> string,
                                    schar pattern_char,
                                    int start_index) {
  if (sizeof(schar) == 1) {
    const schar* pos = reinterpret_cast<const schar*>(
        memchr(string.start() + start_index,
               pattern_char,
               string.length() - start_index));
    if (pos == NULL) return -1;
    return static_cast<int>(pos - string.start());
  }
  for (int i = start_index, n = string.length(); i < n; i++) {
    if (pattern_char == string[i]) {
      return i;
    }
  }
  return -1;
}


template <typename schar>
static int SingleCharLastIndexOf(Vector<const schar> string,
                                 schar pattern_char,
                                 int start_index) {
  for (int i = start_index; i >= 0; i--) {
    if (pattern_char == string[i]) {
      return i;
    }
  }
  return -1;
}


// Trivial string search for shorter strings.
// On return, if "complete" is set to true, the return value is the
// final result of searching for the patter in the subject.
// If "complete" is set to false, the return value is the index where
// further checking should start, i.e., it's guaranteed that the pattern
// does not occur at a position prior to the returned index.
template <typename pchar, typename schar>
static int SimpleIndexOf(Vector<const schar> subject,
                         Vector<const pchar> pattern,
                         int idx,
                         bool* complete) {
  // Badness is a count of how much work we have done.  When we have
  // done enough work we decide it's probably worth switching to a better
  // algorithm.
  int badness = -10 - (pattern.length() << 2);

  // We know our pattern is at least 2 characters, we cache the first so
  // the common case of the first character not matching is faster.
  pchar pattern_first_char = pattern[0];
  for (int i = idx, n = subject.length() - pattern.length(); i <= n; i++) {
    badness++;
    if (badness > 0) {
      *complete = false;
      return i;
    }
    if (sizeof(schar) == 1 && sizeof(pchar) == 1) {
      const schar* pos = reinterpret_cast<const schar*>(
          memchr(subject.start() + i,
                 pattern_first_char,
                 n - i + 1));
      if (pos == NULL) {
        *complete = true;
        return -1;
      }
      i = static_cast<int>(pos - subject.start());
    } else {
      if (subject[i] != pattern_first_char) continue;
    }
    int j = 1;
    do {
      if (pattern[j] != subject[i+j]) {
        break;
      }
      j++;
    } while (j < pattern.length());
    if (j == pattern.length()) {
      *complete = true;
      return i;
    }
    badness += j;
  }
  *complete = true;
  return -1;
}

// Simple indexOf that never bails out. For short patterns only.
template <typename pchar, typename schar>
static int SimpleIndexOf(Vector<const schar> subject,
                         Vector<const pchar> pattern,
                         int idx) {
  pchar pattern_first_char = pattern[0];
  for (int i = idx, n = subject.length() - pattern.length(); i <= n; i++) {
    if (sizeof(schar) == 1 && sizeof(pchar) == 1) {
      const schar* pos = reinterpret_cast<const schar*>(
          memchr(subject.start() + i,
                 pattern_first_char,
                 n - i + 1));
      if (pos == NULL) return -1;
      i = static_cast<int>(pos - subject.start());
    } else {
      if (subject[i] != pattern_first_char) continue;
    }
    int j = 1;
    do {
      if (pattern[j] != subject[i+j]) {
        break;
      }
      j++;
    } while (j < pattern.length());
    if (j == pattern.length()) {
      return i;
    }
  }
  return -1;
}


// Strategy for searching for a string in another string.
enum StringSearchStrategy { SEARCH_FAIL, SEARCH_SHORT, SEARCH_LONG };


template <typename pchar>
static inline StringSearchStrategy InitializeStringSearch(
    Vector<const pchar> pat, bool ascii_subject) {
  ASSERT(pat.length() > 1);
  // We have an ASCII haystack and a non-ASCII needle. Check if there
  // really is a non-ASCII character in the needle and bail out if there
  // is.
  if (ascii_subject && sizeof(pchar) > 1) {
    for (int i = 0; i < pat.length(); i++) {
      uc16 c = pat[i];
      if (c > String::kMaxAsciiCharCode) {
        return SEARCH_FAIL;
      }
    }
  }
  if (pat.length() < kBMMinPatternLength) {
    return SEARCH_SHORT;
  }
  algorithm = SIMPLE_SEARCH;
  return SEARCH_LONG;
}


// Dispatch long needle searches to different algorithms.
template <typename schar, typename pchar>
static int ComplexIndexOf(Vector<const schar> sub,
                          Vector<const pchar> pat,
                          int start_index) {
  ASSERT(pat.length() >= kBMMinPatternLength);
  // Try algorithms in order of increasing setup cost and expected performance.
  bool complete;
  int idx = start_index;
  switch (algorithm) {
    case SIMPLE_SEARCH:
      idx = SimpleIndexOf(sub, pat, idx, &complete);
      if (complete) return idx;
      BoyerMoorePopulateBadCharTable(pat);
      algorithm = BOYER_MOORE_HORSPOOL;
      // FALLTHROUGH.
    case BOYER_MOORE_HORSPOOL:
      idx = BoyerMooreHorspool(sub, pat, idx, &complete);
      if (complete) return idx;
      // Build the Good Suffix table and continue searching.
      BoyerMoorePopulateGoodSuffixTable(pat);
      algorithm = BOYER_MOORE;
      // FALLTHROUGH.
    case BOYER_MOORE:
      return BoyerMooreIndexOf(sub, pat, idx);
  }
  UNREACHABLE();
  return -1;
}


// Dispatch to different search strategies for a single search.
// If searching multiple times on the same needle, the search
// strategy should only be computed once and then dispatch to different
// loops.
template <typename schar, typename pchar>
static int StringSearch(Vector<const schar> sub,
                        Vector<const pchar> pat,
                        int start_index) {
  bool ascii_subject = (sizeof(schar) == 1);
  StringSearchStrategy strategy = InitializeStringSearch(pat, ascii_subject);
  switch (strategy) {
    case SEARCH_FAIL: return -1;
    case SEARCH_SHORT: return SimpleIndexOf(sub, pat, start_index);
    case SEARCH_LONG: return ComplexIndexOf(sub, pat, start_index);
  }
  UNREACHABLE();
  return -1;
}


// Perform string match of pattern on subject, starting at start index.
// Caller must ensure that 0 <= start_index <= sub->length(),
// and should check that pat->length() + start_index <= sub->length()
int Runtime::StringMatch(Handle<String> sub,
                         Handle<String> pat,
                         int start_index) {
  ASSERT(0 <= start_index);
  ASSERT(start_index <= sub->length());

  int pattern_length = pat->length();
  if (pattern_length == 0) return start_index;

  int subject_length = sub->length();
  if (start_index + pattern_length > subject_length) return -1;

  if (!sub->IsFlat()) {
    FlattenString(sub);
  }

  // Searching for one specific character is common.  For one
  // character patterns linear search is necessary, so any smart
  // algorithm is unnecessary overhead.
  if (pattern_length == 1) {
    AssertNoAllocation no_heap_allocation;  // ensure vectors stay valid
    String* seq_sub = *sub;
    if (seq_sub->IsConsString()) {
      seq_sub = ConsString::cast(seq_sub)->first();
    }
    if (seq_sub->IsAsciiRepresentation()) {
      uc16 pchar = pat->Get(0);
      if (pchar > String::kMaxAsciiCharCode) {
        return -1;
      }
      Vector<const char> ascii_vector =
          seq_sub->ToAsciiVector().SubVector(start_index, subject_length);
      const void* pos = memchr(ascii_vector.start(),
                               static_cast<const char>(pchar),
                               static_cast<size_t>(ascii_vector.length()));
      if (pos == NULL) {
        return -1;
      }
      return static_cast<int>(reinterpret_cast<const char*>(pos)
          - ascii_vector.start() + start_index);
    }
    return SingleCharIndexOf(seq_sub->ToUC16Vector(),
                             pat->Get(0),
                             start_index);
  }

  if (!pat->IsFlat()) {
    FlattenString(pat);
  }

  AssertNoAllocation no_heap_allocation;  // ensure vectors stay valid
  // Extract flattened substrings of cons strings before determining asciiness.
  String* seq_sub = *sub;
  if (seq_sub->IsConsString()) {
    seq_sub = ConsString::cast(seq_sub)->first();
  }
  String* seq_pat = *pat;
  if (seq_pat->IsConsString()) {
    seq_pat = ConsString::cast(seq_pat)->first();
  }

  // dispatch on type of strings
  if (seq_pat->IsAsciiRepresentation()) {
    Vector<const char> pat_vector = seq_pat->ToAsciiVector();
    if (seq_sub->IsAsciiRepresentation()) {
      return StringSearch(seq_sub->ToAsciiVector(), pat_vector, start_index);
    }
    return StringSearch(seq_sub->ToUC16Vector(), pat_vector, start_index);
  }
  Vector<const uc16> pat_vector = seq_pat->ToUC16Vector();
  if (seq_sub->IsAsciiRepresentation()) {
    return StringSearch(seq_sub->ToAsciiVector(), pat_vector, start_index);
  }
  return StringSearch(seq_sub->ToUC16Vector(), pat_vector, start_index);
}


static Object* Runtime_StringIndexOf(Arguments args) {
  HandleScope scope;  // create a new handle scope
  ASSERT(args.length() == 3);

  CONVERT_ARG_CHECKED(String, sub, 0);
  CONVERT_ARG_CHECKED(String, pat, 1);

  Object* index = args[2];
  uint32_t start_index;
  if (!index->ToArrayIndex(&start_index)) return Smi::FromInt(-1);

  RUNTIME_ASSERT(start_index <= static_cast<uint32_t>(sub->length()));
  int position = Runtime::StringMatch(sub, pat, start_index);
  return Smi::FromInt(position);
}


template <typename schar, typename pchar>
static int StringMatchBackwards(Vector<const schar> sub,
                                Vector<const pchar> pat,
                                int idx) {
  ASSERT(pat.length() >= 1);
  ASSERT(idx + pat.length() <= sub.length());

  if (sizeof(schar) == 1 && sizeof(pchar) > 1) {
    for (int i = 0; i < pat.length(); i++) {
      uc16 c = pat[i];
      if (c > String::kMaxAsciiCharCode) {
        return -1;
      }
    }
  }

  pchar pattern_first_char = pat[0];
  for (int i = idx; i >= 0; i--) {
    if (sub[i] != pattern_first_char) continue;
    int j = 1;
    while (j < pat.length()) {
      if (pat[j] != sub[i+j]) {
        break;
      }
      j++;
    }
    if (j == pat.length()) {
      return i;
    }
  }
  return -1;
}

static Object* Runtime_StringLastIndexOf(Arguments args) {
  HandleScope scope;  // create a new handle scope
  ASSERT(args.length() == 3);

  CONVERT_ARG_CHECKED(String, sub, 0);
  CONVERT_ARG_CHECKED(String, pat, 1);

  Object* index = args[2];
  uint32_t start_index;
  if (!index->ToArrayIndex(&start_index)) return Smi::FromInt(-1);

  uint32_t pat_length = pat->length();
  uint32_t sub_length = sub->length();

  if (start_index + pat_length > sub_length) {
    start_index = sub_length - pat_length;
  }

  if (pat_length == 0) {
    return Smi::FromInt(start_index);
  }

  if (!sub->IsFlat()) {
    FlattenString(sub);
  }

  if (pat_length == 1) {
    AssertNoAllocation no_heap_allocation;  // ensure vectors stay valid
    if (sub->IsAsciiRepresentation()) {
      uc16 pchar = pat->Get(0);
      if (pchar > String::kMaxAsciiCharCode) {
        return Smi::FromInt(-1);
      }
      return Smi::FromInt(SingleCharLastIndexOf(sub->ToAsciiVector(),
                                                static_cast<char>(pat->Get(0)),
                                                start_index));
    } else {
      return Smi::FromInt(SingleCharLastIndexOf(sub->ToUC16Vector(),
                                                pat->Get(0),
                                                start_index));
    }
  }

  if (!pat->IsFlat()) {
    FlattenString(pat);
  }

  AssertNoAllocation no_heap_allocation;  // ensure vectors stay valid

  int position = -1;

  if (pat->IsAsciiRepresentation()) {
    Vector<const char> pat_vector = pat->ToAsciiVector();
    if (sub->IsAsciiRepresentation()) {
      position = StringMatchBackwards(sub->ToAsciiVector(),
                                      pat_vector,
                                      start_index);
    } else {
      position = StringMatchBackwards(sub->ToUC16Vector(),
                                      pat_vector,
                                      start_index);
    }
  } else {
    Vector<const uc16> pat_vector = pat->ToUC16Vector();
    if (sub->IsAsciiRepresentation()) {
      position = StringMatchBackwards(sub->ToAsciiVector(),
                                      pat_vector,
                                      start_index);
    } else {
      position = StringMatchBackwards(sub->ToUC16Vector(),
                                      pat_vector,
                                      start_index);
    }
  }

  return Smi::FromInt(position);
}


static Object* Runtime_StringLocaleCompare(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);

  CONVERT_CHECKED(String, str1, args[0]);
  CONVERT_CHECKED(String, str2, args[1]);

  if (str1 == str2) return Smi::FromInt(0);  // Equal.
  int str1_length = str1->length();
  int str2_length = str2->length();

  // Decide trivial cases without flattening.
  if (str1_length == 0) {
    if (str2_length == 0) return Smi::FromInt(0);  // Equal.
    return Smi::FromInt(-str2_length);
  } else {
    if (str2_length == 0) return Smi::FromInt(str1_length);
  }

  int end = str1_length < str2_length ? str1_length : str2_length;

  // No need to flatten if we are going to find the answer on the first
  // character.  At this point we know there is at least one character
  // in each string, due to the trivial case handling above.
  int d = str1->Get(0) - str2->Get(0);
  if (d != 0) return Smi::FromInt(d);

  str1->TryFlatten();
  str2->TryFlatten();

  static StringInputBuffer buf1;
  static StringInputBuffer buf2;

  buf1.Reset(str1);
  buf2.Reset(str2);

  for (int i = 0; i < end; i++) {
    uint16_t char1 = buf1.GetNext();
    uint16_t char2 = buf2.GetNext();
    if (char1 != char2) return Smi::FromInt(char1 - char2);
  }

  return Smi::FromInt(str1_length - str2_length);
}


static Object* Runtime_SubString(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 3);

  CONVERT_CHECKED(String, value, args[0]);
  Object* from = args[1];
  Object* to = args[2];
  int start, end;
  // We have a fast integer-only case here to avoid a conversion to double in
  // the common case where from and to are Smis.
  if (from->IsSmi() && to->IsSmi()) {
    start = Smi::cast(from)->value();
    end = Smi::cast(to)->value();
  } else {
    CONVERT_DOUBLE_CHECKED(from_number, from);
    CONVERT_DOUBLE_CHECKED(to_number, to);
    start = FastD2I(from_number);
    end = FastD2I(to_number);
  }
  RUNTIME_ASSERT(end >= start);
  RUNTIME_ASSERT(start >= 0);
  RUNTIME_ASSERT(end <= value->length());
  Counters::sub_string_runtime.Increment();
  return value->SubString(start, end);
}


static Object* Runtime_StringMatch(Arguments args) {
  ASSERT_EQ(3, args.length());

  CONVERT_ARG_CHECKED(String, subject, 0);
  CONVERT_ARG_CHECKED(JSRegExp, regexp, 1);
  CONVERT_ARG_CHECKED(JSArray, regexp_info, 2);
  HandleScope handles;

  Handle<Object> match = RegExpImpl::Exec(regexp, subject, 0, regexp_info);

  if (match.is_null()) {
    return Failure::Exception();
  }
  if (match->IsNull()) {
    return Heap::null_value();
  }
  int length = subject->length();

  CompilationZoneScope zone_space(DELETE_ON_EXIT);
  ZoneList<int> offsets(8);
  do {
    int start;
    int end;
    {
      AssertNoAllocation no_alloc;
      FixedArray* elements = FixedArray::cast(regexp_info->elements());
      start = Smi::cast(elements->get(RegExpImpl::kFirstCapture))->value();
      end = Smi::cast(elements->get(RegExpImpl::kFirstCapture + 1))->value();
    }
    offsets.Add(start);
    offsets.Add(end);
    int index = start < end ? end : end + 1;
    if (index > length) break;
    match = RegExpImpl::Exec(regexp, subject, index, regexp_info);
    if (match.is_null()) {
      return Failure::Exception();
    }
  } while (!match->IsNull());
  int matches = offsets.length() / 2;
  Handle<FixedArray> elements = Factory::NewFixedArray(matches);
  for (int i = 0; i < matches ; i++) {
    int from = offsets.at(i * 2);
    int to = offsets.at(i * 2 + 1);
    elements->set(i, *Factory::NewSubString(subject, from, to));
  }
  Handle<JSArray> result = Factory::NewJSArrayWithElements(elements);
  result->set_length(Smi::FromInt(matches));
  return *result;
}


// Two smis before and after the match, for very long strings.
const int kMaxBuilderEntriesPerRegExpMatch = 5;


static void SetLastMatchInfoNoCaptures(Handle<String> subject,
                                       Handle<JSArray> last_match_info,
                                       int match_start,
                                       int match_end) {
  // Fill last_match_info with a single capture.
  last_match_info->EnsureSize(2 + RegExpImpl::kLastMatchOverhead);
  AssertNoAllocation no_gc;
  FixedArray* elements = FixedArray::cast(last_match_info->elements());
  RegExpImpl::SetLastCaptureCount(elements, 2);
  RegExpImpl::SetLastInput(elements, *subject);
  RegExpImpl::SetLastSubject(elements, *subject);
  RegExpImpl::SetCapture(elements, 0, match_start);
  RegExpImpl::SetCapture(elements, 1, match_end);
}


template <typename schar>
static bool SearchCharMultiple(Vector<schar> subject,
                               String* pattern,
                               schar pattern_char,
                               FixedArrayBuilder* builder,
                               int* match_pos) {
  // Position of last match.
  int pos = *match_pos;
  int subject_length = subject.length();
  while (pos < subject_length) {
    int match_end = pos + 1;
    if (!builder->HasCapacity(kMaxBuilderEntriesPerRegExpMatch)) {
      *match_pos = pos;
      return false;
    }
    int new_pos = SingleCharIndexOf(subject, pattern_char, match_end);
    if (new_pos >= 0) {
      // Match has been found.
      if (new_pos > match_end) {
        ReplacementStringBuilder::AddSubjectSlice(builder, match_end, new_pos);
      }
      pos = new_pos;
      builder->Add(pattern);
    } else {
      break;
    }
  }
  if (pos + 1 < subject_length) {
    ReplacementStringBuilder::AddSubjectSlice(builder, pos + 1, subject_length);
  }
  *match_pos = pos;
  return true;
}


static bool SearchCharMultiple(Handle<String> subject,
                               Handle<String> pattern,
                               Handle<JSArray> last_match_info,
                               FixedArrayBuilder* builder) {
  ASSERT(subject->IsFlat());
  ASSERT_EQ(1, pattern->length());
  uc16 pattern_char = pattern->Get(0);
  // Treating position before first as initial "previous match position".
  int match_pos = -1;

  for (;;) {  // Break when search complete.
    builder->EnsureCapacity(kMaxBuilderEntriesPerRegExpMatch);
    AssertNoAllocation no_gc;
    if (subject->IsAsciiRepresentation()) {
      if (pattern_char > String::kMaxAsciiCharCode) {
        break;
      }
      Vector<const char> subject_vector = subject->ToAsciiVector();
      char pattern_ascii_char = static_cast<char>(pattern_char);
      bool complete = SearchCharMultiple<const char>(subject_vector,
                                                     *pattern,
                                                     pattern_ascii_char,
                                                     builder,
                                                     &match_pos);
      if (complete) break;
    } else {
      Vector<const uc16> subject_vector = subject->ToUC16Vector();
      bool complete = SearchCharMultiple<const uc16>(subject_vector,
                                                     *pattern,
                                                     pattern_char,
                                                     builder,
                                                     &match_pos);
      if (complete) break;
    }
  }

  if (match_pos >= 0) {
    SetLastMatchInfoNoCaptures(subject,
                               last_match_info,
                               match_pos,
                               match_pos + 1);
    return true;
  }
  return false;  // No matches at all.
}


template <typename schar, typename pchar>
static bool SearchStringMultiple(Vector<schar> subject,
                                 String* pattern,
                                 Vector<pchar> pattern_string,
                                 FixedArrayBuilder* builder,
                                 int* match_pos) {
  int pos = *match_pos;
  int subject_length = subject.length();
  int pattern_length = pattern_string.length();
  int max_search_start = subject_length - pattern_length;
  bool is_ascii = (sizeof(schar) == 1);
  StringSearchStrategy strategy =
      InitializeStringSearch(pattern_string, is_ascii);
  switch (strategy) {
    case SEARCH_FAIL: break;
    case SEARCH_SHORT:
      while (pos <= max_search_start) {
        if (!builder->HasCapacity(kMaxBuilderEntriesPerRegExpMatch)) {
          *match_pos = pos;
          return false;
        }
        // Position of end of previous match.
        int match_end = pos + pattern_length;
        int new_pos = SimpleIndexOf(subject, pattern_string, match_end);
        if (new_pos >= 0) {
          // A match.
          if (new_pos > match_end) {
            ReplacementStringBuilder::AddSubjectSlice(builder,
                                                      match_end,
                                                      new_pos);
          }
          pos = new_pos;
          builder->Add(pattern);
        } else {
          break;
        }
      }
      break;
    case SEARCH_LONG:
      while (pos  <= max_search_start) {
        if (!builder->HasCapacity(kMaxBuilderEntriesPerRegExpMatch)) {
          *match_pos = pos;
          return false;
        }
        int match_end = pos + pattern_length;
        int new_pos = ComplexIndexOf(subject, pattern_string, match_end);
        if (new_pos >= 0) {
          // A match has been found.
          if (new_pos > match_end) {
            ReplacementStringBuilder::AddSubjectSlice(builder,
                                                      match_end,
                                                      new_pos);
          }
          pos = new_pos;
          builder->Add(pattern);
        } else {
         break;
        }
      }
      break;
  }
  if (pos < max_search_start) {
    ReplacementStringBuilder::AddSubjectSlice(builder,
                                              pos + pattern_length,
                                              subject_length);
  }
  *match_pos = pos;
  return true;
}


static bool SearchStringMultiple(Handle<String> subject,
                                 Handle<String> pattern,
                                 Handle<JSArray> last_match_info,
                                 FixedArrayBuilder* builder) {
  ASSERT(subject->IsFlat());
  ASSERT(pattern->IsFlat());
  ASSERT(pattern->length() > 1);

  // Treating as if a previous match was before first character.
  int match_pos = -pattern->length();

  for (;;) {  // Break when search complete.
    builder->EnsureCapacity(kMaxBuilderEntriesPerRegExpMatch);
    AssertNoAllocation no_gc;
    if (subject->IsAsciiRepresentation()) {
      Vector<const char> subject_vector = subject->ToAsciiVector();
      if (pattern->IsAsciiRepresentation()) {
        if (SearchStringMultiple(subject_vector,
                                 *pattern,
                                 pattern->ToAsciiVector(),
                                 builder,
                                 &match_pos)) break;
      } else {
        if (SearchStringMultiple(subject_vector,
                                 *pattern,
                                 pattern->ToUC16Vector(),
                                 builder,
                                 &match_pos)) break;
      }
    } else {
      Vector<const uc16> subject_vector = subject->ToUC16Vector();
      if (pattern->IsAsciiRepresentation()) {
        if (SearchStringMultiple(subject_vector,
                                 *pattern,
                                 pattern->ToAsciiVector(),
                                 builder,
                                 &match_pos)) break;
      } else {
        if (SearchStringMultiple(subject_vector,
                                 *pattern,
                                 pattern->ToUC16Vector(),
                                 builder,
                                 &match_pos)) break;
      }
    }
  }

  if (match_pos >= 0) {
    SetLastMatchInfoNoCaptures(subject,
                               last_match_info,
                               match_pos,
                               match_pos + pattern->length());
    return true;
  }
  return false;  // No matches at all.
}


static RegExpImpl::IrregexpResult SearchRegExpNoCaptureMultiple(
    Handle<String> subject,
    Handle<JSRegExp> regexp,
    Handle<JSArray> last_match_array,
    FixedArrayBuilder* builder) {
  ASSERT(subject->IsFlat());
  int match_start = -1;
  int match_end = 0;
  int pos = 0;
  int required_registers = RegExpImpl::IrregexpPrepare(regexp, subject);
  if (required_registers < 0) return RegExpImpl::RE_EXCEPTION;

  OffsetsVector registers(required_registers);
  Vector<int> register_vector(registers.vector(), registers.length());
  int subject_length = subject->length();

  for (;;) {  // Break on failure, return on exception.
    RegExpImpl::IrregexpResult result =
        RegExpImpl::IrregexpExecOnce(regexp,
                                     subject,
                                     pos,
                                     register_vector);
    if (result == RegExpImpl::RE_SUCCESS) {
      match_start = register_vector[0];
      builder->EnsureCapacity(kMaxBuilderEntriesPerRegExpMatch);
      if (match_end < match_start) {
        ReplacementStringBuilder::AddSubjectSlice(builder,
                                                  match_end,
                                                  match_start);
      }
      match_end = register_vector[1];
      HandleScope loop_scope;
      builder->Add(*Factory::NewSubString(subject, match_start, match_end));
      if (match_start != match_end) {
        pos = match_end;
      } else {
        pos = match_end + 1;
        if (pos > subject_length) break;
      }
    } else if (result == RegExpImpl::RE_FAILURE) {
      break;
    } else {
      ASSERT_EQ(result, RegExpImpl::RE_EXCEPTION);
      return result;
    }
  }

  if (match_start >= 0) {
    if (match_end < subject_length) {
      ReplacementStringBuilder::AddSubjectSlice(builder,
                                                match_end,
                                                subject_length);
    }
    SetLastMatchInfoNoCaptures(subject,
                               last_match_array,
                               match_start,
                               match_end);
    return RegExpImpl::RE_SUCCESS;
  } else {
    return RegExpImpl::RE_FAILURE;  // No matches at all.
  }
}


static RegExpImpl::IrregexpResult SearchRegExpMultiple(
    Handle<String> subject,
    Handle<JSRegExp> regexp,
    Handle<JSArray> last_match_array,
    FixedArrayBuilder* builder) {

  ASSERT(subject->IsFlat());
  int required_registers = RegExpImpl::IrregexpPrepare(regexp, subject);
  if (required_registers < 0) return RegExpImpl::RE_EXCEPTION;

  OffsetsVector registers(required_registers);
  Vector<int> register_vector(registers.vector(), registers.length());

  RegExpImpl::IrregexpResult result =
      RegExpImpl::IrregexpExecOnce(regexp,
                                   subject,
                                   0,
                                   register_vector);

  int capture_count = regexp->CaptureCount();
  int subject_length = subject->length();

  // Position to search from.
  int pos = 0;
  // End of previous match. Differs from pos if match was empty.
  int match_end = 0;
  if (result == RegExpImpl::RE_SUCCESS) {
    // Need to keep a copy of the previous match for creating last_match_info
    // at the end, so we have two vectors that we swap between.
    OffsetsVector registers2(required_registers);
    Vector<int> prev_register_vector(registers2.vector(), registers2.length());

    do {
      int match_start = register_vector[0];
      builder->EnsureCapacity(kMaxBuilderEntriesPerRegExpMatch);
      if (match_end < match_start) {
        ReplacementStringBuilder::AddSubjectSlice(builder,
                                                  match_end,
                                                  match_start);
      }
      match_end = register_vector[1];

      {
        // Avoid accumulating new handles inside loop.
        HandleScope temp_scope;
        // Arguments array to replace function is match, captures, index and
        // subject, i.e., 3 + capture count in total.
        Handle<FixedArray> elements = Factory::NewFixedArray(3 + capture_count);
        elements->set(0, *Factory::NewSubString(subject,
                                                match_start,
                                                match_end));
        for (int i = 1; i <= capture_count; i++) {
          int start = register_vector[i * 2];
          if (start >= 0) {
            int end = register_vector[i * 2 + 1];
            ASSERT(start <= end);
            Handle<String> substring = Factory::NewSubString(subject,
                                                             start,
                                                             end);
            elements->set(i, *substring);
          } else {
            ASSERT(register_vector[i * 2 + 1] < 0);
            elements->set(i, Heap::undefined_value());
          }
        }
        elements->set(capture_count + 1, Smi::FromInt(match_start));
        elements->set(capture_count + 2, *subject);
        builder->Add(*Factory::NewJSArrayWithElements(elements));
      }
      // Swap register vectors, so the last successful match is in
      // prev_register_vector.
      Vector<int> tmp = prev_register_vector;
      prev_register_vector = register_vector;
      register_vector = tmp;

      if (match_end > match_start) {
        pos = match_end;
      } else {
        pos = match_end + 1;
        if (pos > subject_length) {
          break;
        }
      }

      result = RegExpImpl::IrregexpExecOnce(regexp,
                                            subject,
                                            pos,
                                            register_vector);
    } while (result == RegExpImpl::RE_SUCCESS);

    if (result != RegExpImpl::RE_EXCEPTION) {
      // Finished matching, with at least one match.
      if (match_end < subject_length) {
        ReplacementStringBuilder::AddSubjectSlice(builder,
                                                  match_end,
                                                  subject_length);
      }

      int last_match_capture_count = (capture_count + 1) * 2;
      int last_match_array_size =
          last_match_capture_count + RegExpImpl::kLastMatchOverhead;
      last_match_array->EnsureSize(last_match_array_size);
      AssertNoAllocation no_gc;
      FixedArray* elements = FixedArray::cast(last_match_array->elements());
      RegExpImpl::SetLastCaptureCount(elements, last_match_capture_count);
      RegExpImpl::SetLastSubject(elements, *subject);
      RegExpImpl::SetLastInput(elements, *subject);
      for (int i = 0; i < last_match_capture_count; i++) {
        RegExpImpl::SetCapture(elements, i, prev_register_vector[i]);
      }
      return RegExpImpl::RE_SUCCESS;
    }
  }
  // No matches at all, return failure or exception result directly.
  return result;
}


static Object* Runtime_RegExpExecMultiple(Arguments args) {
  ASSERT(args.length() == 4);
  HandleScope handles;

  CONVERT_ARG_CHECKED(String, subject, 1);
  if (!subject->IsFlat()) { FlattenString(subject); }
  CONVERT_ARG_CHECKED(JSRegExp, regexp, 0);
  CONVERT_ARG_CHECKED(JSArray, last_match_info, 2);
  CONVERT_ARG_CHECKED(JSArray, result_array, 3);

  ASSERT(last_match_info->HasFastElements());
  ASSERT(regexp->GetFlags().is_global());
  Handle<FixedArray> result_elements;
  if (result_array->HasFastElements()) {
    result_elements =
        Handle<FixedArray>(FixedArray::cast(result_array->elements()));
  } else {
    result_elements = Factory::NewFixedArrayWithHoles(16);
  }
  FixedArrayBuilder builder(result_elements);

  if (regexp->TypeTag() == JSRegExp::ATOM) {
    Handle<String> pattern(
        String::cast(regexp->DataAt(JSRegExp::kAtomPatternIndex)));
    int pattern_length = pattern->length();
    if (pattern_length == 1) {
      if (SearchCharMultiple(subject, pattern, last_match_info, &builder)) {
        return *builder.ToJSArray(result_array);
      }
      return Heap::null_value();
    }

    if (!pattern->IsFlat()) FlattenString(pattern);
    if (SearchStringMultiple(subject, pattern, last_match_info, &builder)) {
      return *builder.ToJSArray(result_array);
    }
    return Heap::null_value();
  }

  ASSERT_EQ(regexp->TypeTag(), JSRegExp::IRREGEXP);

  RegExpImpl::IrregexpResult result;
  if (regexp->CaptureCount() == 0) {
    result = SearchRegExpNoCaptureMultiple(subject,
                                           regexp,
                                           last_match_info,
                                           &builder);
  } else {
    result = SearchRegExpMultiple(subject, regexp, last_match_info, &builder);
  }
  if (result == RegExpImpl::RE_SUCCESS) return *builder.ToJSArray(result_array);
  if (result == RegExpImpl::RE_FAILURE) return Heap::null_value();
  ASSERT_EQ(result, RegExpImpl::RE_EXCEPTION);
  return Failure::Exception();
}


static Object* Runtime_NumberToRadixString(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);

  // Fast case where the result is a one character string.
  if (args[0]->IsSmi() && args[1]->IsSmi()) {
    int value = Smi::cast(args[0])->value();
    int radix = Smi::cast(args[1])->value();
    if (value >= 0 && value < radix) {
      RUNTIME_ASSERT(radix <= 36);
      // Character array used for conversion.
      static const char kCharTable[] = "0123456789abcdefghijklmnopqrstuvwxyz";
      return Heap::LookupSingleCharacterStringFromCode(kCharTable[value]);
    }
  }

  // Slow case.
  CONVERT_DOUBLE_CHECKED(value, args[0]);
  if (isnan(value)) {
    return Heap::AllocateStringFromAscii(CStrVector("NaN"));
  }
  if (isinf(value)) {
    if (value < 0) {
      return Heap::AllocateStringFromAscii(CStrVector("-Infinity"));
    }
    return Heap::AllocateStringFromAscii(CStrVector("Infinity"));
  }
  CONVERT_DOUBLE_CHECKED(radix_number, args[1]);
  int radix = FastD2I(radix_number);
  RUNTIME_ASSERT(2 <= radix && radix <= 36);
  char* str = DoubleToRadixCString(value, radix);
  Object* result = Heap::AllocateStringFromAscii(CStrVector(str));
  DeleteArray(str);
  return result;
}


static Object* Runtime_NumberToFixed(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);

  CONVERT_DOUBLE_CHECKED(value, args[0]);
  if (isnan(value)) {
    return Heap::AllocateStringFromAscii(CStrVector("NaN"));
  }
  if (isinf(value)) {
    if (value < 0) {
      return Heap::AllocateStringFromAscii(CStrVector("-Infinity"));
    }
    return Heap::AllocateStringFromAscii(CStrVector("Infinity"));
  }
  CONVERT_DOUBLE_CHECKED(f_number, args[1]);
  int f = FastD2I(f_number);
  RUNTIME_ASSERT(f >= 0);
  char* str = DoubleToFixedCString(value, f);
  Object* res = Heap::AllocateStringFromAscii(CStrVector(str));
  DeleteArray(str);
  return res;
}


static Object* Runtime_NumberToExponential(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);

  CONVERT_DOUBLE_CHECKED(value, args[0]);
  if (isnan(value)) {
    return Heap::AllocateStringFromAscii(CStrVector("NaN"));
  }
  if (isinf(value)) {
    if (value < 0) {
      return Heap::AllocateStringFromAscii(CStrVector("-Infinity"));
    }
    return Heap::AllocateStringFromAscii(CStrVector("Infinity"));
  }
  CONVERT_DOUBLE_CHECKED(f_number, args[1]);
  int f = FastD2I(f_number);
  RUNTIME_ASSERT(f >= -1 && f <= 20);
  char* str = DoubleToExponentialCString(value, f);
  Object* res = Heap::AllocateStringFromAscii(CStrVector(str));
  DeleteArray(str);
  return res;
}


static Object* Runtime_NumberToPrecision(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);

  CONVERT_DOUBLE_CHECKED(value, args[0]);
  if (isnan(value)) {
    return Heap::AllocateStringFromAscii(CStrVector("NaN"));
  }
  if (isinf(value)) {
    if (value < 0) {
      return Heap::AllocateStringFromAscii(CStrVector("-Infinity"));
    }
    return Heap::AllocateStringFromAscii(CStrVector("Infinity"));
  }
  CONVERT_DOUBLE_CHECKED(f_number, args[1]);
  int f = FastD2I(f_number);
  RUNTIME_ASSERT(f >= 1 && f <= 21);
  char* str = DoubleToPrecisionCString(value, f);
  Object* res = Heap::AllocateStringFromAscii(CStrVector(str));
  DeleteArray(str);
  return res;
}


// Returns a single character string where first character equals
// string->Get(index).
static Handle<Object> GetCharAt(Handle<String> string, uint32_t index) {
  if (index < static_cast<uint32_t>(string->length())) {
    string->TryFlatten();
    return LookupSingleCharacterStringFromCode(
        string->Get(index));
  }
  return Execution::CharAt(string, index);
}


Object* Runtime::GetElementOrCharAt(Handle<Object> object, uint32_t index) {
  // Handle [] indexing on Strings
  if (object->IsString()) {
    Handle<Object> result = GetCharAt(Handle<String>::cast(object), index);
    if (!result->IsUndefined()) return *result;
  }

  // Handle [] indexing on String objects
  if (object->IsStringObjectWithCharacterAt(index)) {
    Handle<JSValue> js_value = Handle<JSValue>::cast(object);
    Handle<Object> result =
        GetCharAt(Handle<String>(String::cast(js_value->value())), index);
    if (!result->IsUndefined()) return *result;
  }

  if (object->IsString() || object->IsNumber() || object->IsBoolean()) {
    Handle<Object> prototype = GetPrototype(object);
    return prototype->GetElement(index);
  }

  return GetElement(object, index);
}


Object* Runtime::GetElement(Handle<Object> object, uint32_t index) {
  return object->GetElement(index);
}


Object* Runtime::GetObjectProperty(Handle<Object> object, Handle<Object> key) {
  HandleScope scope;

  if (object->IsUndefined() || object->IsNull()) {
    Handle<Object> args[2] = { key, object };
    Handle<Object> error =
        Factory::NewTypeError("non_object_property_load",
                              HandleVector(args, 2));
    return Top::Throw(*error);
  }

  // Check if the given key is an array index.
  uint32_t index;
  if (key->ToArrayIndex(&index)) {
    return GetElementOrCharAt(object, index);
  }

  // Convert the key to a string - possibly by calling back into JavaScript.
  Handle<String> name;
  if (key->IsString()) {
    name = Handle<String>::cast(key);
  } else {
    bool has_pending_exception = false;
    Handle<Object> converted =
        Execution::ToString(key, &has_pending_exception);
    if (has_pending_exception) return Failure::Exception();
    name = Handle<String>::cast(converted);
  }

  // Check if the name is trivially convertible to an index and get
  // the element if so.
  if (name->AsArrayIndex(&index)) {
    return GetElementOrCharAt(object, index);
  } else {
    PropertyAttributes attr;
    return object->GetProperty(*name, &attr);
  }
}


static Object* Runtime_GetProperty(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);

  Handle<Object> object = args.at<Object>(0);
  Handle<Object> key = args.at<Object>(1);

  return Runtime::GetObjectProperty(object, key);
}


// KeyedStringGetProperty is called from KeyedLoadIC::GenerateGeneric.
static Object* Runtime_KeyedGetProperty(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);

  // Fast cases for getting named properties of the receiver JSObject
  // itself.
  //
  // The global proxy objects has to be excluded since LocalLookup on
  // the global proxy object can return a valid result even though the
  // global proxy object never has properties.  This is the case
  // because the global proxy object forwards everything to its hidden
  // prototype including local lookups.
  //
  // Additionally, we need to make sure that we do not cache results
  // for objects that require access checks.
  if (args[0]->IsJSObject() &&
      !args[0]->IsJSGlobalProxy() &&
      !args[0]->IsAccessCheckNeeded() &&
      args[1]->IsString()) {
    JSObject* receiver = JSObject::cast(args[0]);
    String* key = String::cast(args[1]);
    if (receiver->HasFastProperties()) {
      // Attempt to use lookup cache.
      Map* receiver_map = receiver->map();
      int offset = KeyedLookupCache::Lookup(receiver_map, key);
      if (offset != -1) {
        Object* value = receiver->FastPropertyAt(offset);
        return value->IsTheHole() ? Heap::undefined_value() : value;
      }
      // Lookup cache miss.  Perform lookup and update the cache if appropriate.
      LookupResult result;
      receiver->LocalLookup(key, &result);
      if (result.IsProperty() && result.type() == FIELD) {
        int offset = result.GetFieldIndex();
        KeyedLookupCache::Update(receiver_map, key, offset);
        return receiver->FastPropertyAt(offset);
      }
    } else {
      // Attempt dictionary lookup.
      StringDictionary* dictionary = receiver->property_dictionary();
      int entry = dictionary->FindEntry(key);
      if ((entry != StringDictionary::kNotFound) &&
          (dictionary->DetailsAt(entry).type() == NORMAL)) {
        Object* value = dictionary->ValueAt(entry);
        if (!receiver->IsGlobalObject()) return value;
        value = JSGlobalPropertyCell::cast(value)->value();
        if (!value->IsTheHole()) return value;
        // If value is the hole do the general lookup.
      }
    }
  } else if (args[0]->IsString() && args[1]->IsSmi()) {
    // Fast case for string indexing using [] with a smi index.
    HandleScope scope;
    Handle<String> str = args.at<String>(0);
    int index = Smi::cast(args[1])->value();
    Handle<Object> result = GetCharAt(str, index);
    return *result;
  }

  // Fall back to GetObjectProperty.
  return Runtime::GetObjectProperty(args.at<Object>(0),
                                    args.at<Object>(1));
}


static Object* Runtime_DefineOrRedefineAccessorProperty(Arguments args) {
  ASSERT(args.length() == 5);
  HandleScope scope;
  CONVERT_ARG_CHECKED(JSObject, obj, 0);
  CONVERT_CHECKED(String, name, args[1]);
  CONVERT_CHECKED(Smi, flag_setter, args[2]);
  CONVERT_CHECKED(JSFunction, fun, args[3]);
  CONVERT_CHECKED(Smi, flag_attr, args[4]);
  int unchecked = flag_attr->value();
  RUNTIME_ASSERT((unchecked & ~(READ_ONLY | DONT_ENUM | DONT_DELETE)) == 0);
  RUNTIME_ASSERT(!obj->IsNull());
  LookupResult result;
  obj->LocalLookupRealNamedProperty(name, &result);

  PropertyAttributes attr = static_cast<PropertyAttributes>(unchecked);
  // If an existing property is either FIELD, NORMAL or CONSTANT_FUNCTION
  // delete it to avoid running into trouble in DefineAccessor, which
  // handles this incorrectly if the property is readonly (does nothing)
  if (result.IsProperty() &&
      (result.type() == FIELD || result.type() == NORMAL
       || result.type() == CONSTANT_FUNCTION)) {
    obj->DeleteProperty(name, JSObject::NORMAL_DELETION);
  }
  return obj->DefineAccessor(name, flag_setter->value() == 0, fun, attr);
}

static Object* Runtime_DefineOrRedefineDataProperty(Arguments args) {
  ASSERT(args.length() == 4);
  HandleScope scope;
  CONVERT_ARG_CHECKED(JSObject, js_object, 0);
  CONVERT_ARG_CHECKED(String, name, 1);
  Handle<Object> obj_value = args.at<Object>(2);

  CONVERT_CHECKED(Smi, flag, args[3]);
  int unchecked = flag->value();
  RUNTIME_ASSERT((unchecked & ~(READ_ONLY | DONT_ENUM | DONT_DELETE)) == 0);

  PropertyAttributes attr = static_cast<PropertyAttributes>(unchecked);

  // Check if this is an element.
  uint32_t index;
  bool is_element = name->AsArrayIndex(&index);

  // Special case for elements if any of the flags are true.
  // If elements are in fast case we always implicitly assume that:
  // DONT_DELETE: false, DONT_ENUM: false, READ_ONLY: false.
  if (((unchecked & (DONT_DELETE | DONT_ENUM | READ_ONLY)) != 0) &&
      is_element) {
    // Normalize the elements to enable attributes on the property.
    js_object->NormalizeElements();
    NumberDictionary* dictionary = js_object->element_dictionary();
    // Make sure that we never go back to fast case.
    dictionary->set_requires_slow_elements();
    PropertyDetails details = PropertyDetails(attr, NORMAL);
    dictionary->Set(index, *obj_value, details);
  }

  LookupResult result;
  js_object->LocalLookupRealNamedProperty(*name, &result);

  // Take special care when attributes are different and there is already
  // a property. For simplicity we normalize the property which enables us
  // to not worry about changing the instance_descriptor and creating a new
  // map. The current version of SetObjectProperty does not handle attributes
  // correctly in the case where a property is a field and is reset with
  // new attributes.
  if (result.IsProperty() && attr != result.GetAttributes()) {
    // New attributes - normalize to avoid writing to instance descriptor
    js_object->NormalizeProperties(CLEAR_INOBJECT_PROPERTIES, 0);
    // Use IgnoreAttributes version since a readonly property may be
    // overridden and SetProperty does not allow this.
    return js_object->IgnoreAttributesAndSetLocalProperty(*name,
                                                          *obj_value,
                                                          attr);
  }

  return Runtime::SetObjectProperty(js_object, name, obj_value, attr);
}


Object* Runtime::SetObjectProperty(Handle<Object> object,
                                   Handle<Object> key,
                                   Handle<Object> value,
                                   PropertyAttributes attr) {
  HandleScope scope;

  if (object->IsUndefined() || object->IsNull()) {
    Handle<Object> args[2] = { key, object };
    Handle<Object> error =
        Factory::NewTypeError("non_object_property_store",
                              HandleVector(args, 2));
    return Top::Throw(*error);
  }

  // If the object isn't a JavaScript object, we ignore the store.
  if (!object->IsJSObject()) return *value;

  Handle<JSObject> js_object = Handle<JSObject>::cast(object);

  // Check if the given key is an array index.
  uint32_t index;
  if (key->ToArrayIndex(&index)) {
    // In Firefox/SpiderMonkey, Safari and Opera you can access the characters
    // of a string using [] notation.  We need to support this too in
    // JavaScript.
    // In the case of a String object we just need to redirect the assignment to
    // the underlying string if the index is in range.  Since the underlying
    // string does nothing with the assignment then we can ignore such
    // assignments.
    if (js_object->IsStringObjectWithCharacterAt(index)) {
      return *value;
    }

    Handle<Object> result = SetElement(js_object, index, value);
    if (result.is_null()) return Failure::Exception();
    return *value;
  }

  if (key->IsString()) {
    Handle<Object> result;
    if (Handle<String>::cast(key)->AsArrayIndex(&index)) {
      result = SetElement(js_object, index, value);
    } else {
      Handle<String> key_string = Handle<String>::cast(key);
      key_string->TryFlatten();
      result = SetProperty(js_object, key_string, value, attr);
    }
    if (result.is_null()) return Failure::Exception();
    return *value;
  }

  // Call-back into JavaScript to convert the key to a string.
  bool has_pending_exception = false;
  Handle<Object> converted = Execution::ToString(key, &has_pending_exception);
  if (has_pending_exception) return Failure::Exception();
  Handle<String> name = Handle<String>::cast(converted);

  if (name->AsArrayIndex(&index)) {
    return js_object->SetElement(index, *value);
  } else {
    return js_object->SetProperty(*name, *value, attr);
  }
}


Object* Runtime::ForceSetObjectProperty(Handle<JSObject> js_object,
                                        Handle<Object> key,
                                        Handle<Object> value,
                                        PropertyAttributes attr) {
  HandleScope scope;

  // Check if the given key is an array index.
  uint32_t index;
  if (key->ToArrayIndex(&index)) {
    // In Firefox/SpiderMonkey, Safari and Opera you can access the characters
    // of a string using [] notation.  We need to support this too in
    // JavaScript.
    // In the case of a String object we just need to redirect the assignment to
    // the underlying string if the index is in range.  Since the underlying
    // string does nothing with the assignment then we can ignore such
    // assignments.
    if (js_object->IsStringObjectWithCharacterAt(index)) {
      return *value;
    }

    return js_object->SetElement(index, *value);
  }

  if (key->IsString()) {
    if (Handle<String>::cast(key)->AsArrayIndex(&index)) {
      return js_object->SetElement(index, *value);
    } else {
      Handle<String> key_string = Handle<String>::cast(key);
      key_string->TryFlatten();
      return js_object->IgnoreAttributesAndSetLocalProperty(*key_string,
                                                            *value,
                                                            attr);
    }
  }

  // Call-back into JavaScript to convert the key to a string.
  bool has_pending_exception = false;
  Handle<Object> converted = Execution::ToString(key, &has_pending_exception);
  if (has_pending_exception) return Failure::Exception();
  Handle<String> name = Handle<String>::cast(converted);

  if (name->AsArrayIndex(&index)) {
    return js_object->SetElement(index, *value);
  } else {
    return js_object->IgnoreAttributesAndSetLocalProperty(*name, *value, attr);
  }
}


Object* Runtime::ForceDeleteObjectProperty(Handle<JSObject> js_object,
                                           Handle<Object> key) {
  HandleScope scope;

  // Check if the given key is an array index.
  uint32_t index;
  if (key->ToArrayIndex(&index)) {
    // In Firefox/SpiderMonkey, Safari and Opera you can access the
    // characters of a string using [] notation.  In the case of a
    // String object we just need to redirect the deletion to the
    // underlying string if the index is in range.  Since the
    // underlying string does nothing with the deletion, we can ignore
    // such deletions.
    if (js_object->IsStringObjectWithCharacterAt(index)) {
      return Heap::true_value();
    }

    return js_object->DeleteElement(index, JSObject::FORCE_DELETION);
  }

  Handle<String> key_string;
  if (key->IsString()) {
    key_string = Handle<String>::cast(key);
  } else {
    // Call-back into JavaScript to convert the key to a string.
    bool has_pending_exception = false;
    Handle<Object> converted = Execution::ToString(key, &has_pending_exception);
    if (has_pending_exception) return Failure::Exception();
    key_string = Handle<String>::cast(converted);
  }

  key_string->TryFlatten();
  return js_object->DeleteProperty(*key_string, JSObject::FORCE_DELETION);
}


static Object* Runtime_SetProperty(Arguments args) {
  NoHandleAllocation ha;
  RUNTIME_ASSERT(args.length() == 3 || args.length() == 4);

  Handle<Object> object = args.at<Object>(0);
  Handle<Object> key = args.at<Object>(1);
  Handle<Object> value = args.at<Object>(2);

  // Compute attributes.
  PropertyAttributes attributes = NONE;
  if (args.length() == 4) {
    CONVERT_CHECKED(Smi, value_obj, args[3]);
    int unchecked_value = value_obj->value();
    // Only attribute bits should be set.
    RUNTIME_ASSERT(
        (unchecked_value & ~(READ_ONLY | DONT_ENUM | DONT_DELETE)) == 0);
    attributes = static_cast<PropertyAttributes>(unchecked_value);
  }
  return Runtime::SetObjectProperty(object, key, value, attributes);
}


// Set a local property, even if it is READ_ONLY.  If the property does not
// exist, it will be added with attributes NONE.
static Object* Runtime_IgnoreAttributesAndSetProperty(Arguments args) {
  NoHandleAllocation ha;
  RUNTIME_ASSERT(args.length() == 3 || args.length() == 4);
  CONVERT_CHECKED(JSObject, object, args[0]);
  CONVERT_CHECKED(String, name, args[1]);
  // Compute attributes.
  PropertyAttributes attributes = NONE;
  if (args.length() == 4) {
    CONVERT_CHECKED(Smi, value_obj, args[3]);
    int unchecked_value = value_obj->value();
    // Only attribute bits should be set.
    RUNTIME_ASSERT(
        (unchecked_value & ~(READ_ONLY | DONT_ENUM | DONT_DELETE)) == 0);
    attributes = static_cast<PropertyAttributes>(unchecked_value);
  }

  return object->
      IgnoreAttributesAndSetLocalProperty(name, args[2], attributes);
}


static Object* Runtime_DeleteProperty(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);

  CONVERT_CHECKED(JSObject, object, args[0]);
  CONVERT_CHECKED(String, key, args[1]);
  return object->DeleteProperty(key, JSObject::NORMAL_DELETION);
}


static Object* HasLocalPropertyImplementation(Handle<JSObject> object,
                                              Handle<String> key) {
  if (object->HasLocalProperty(*key)) return Heap::true_value();
  // Handle hidden prototypes.  If there's a hidden prototype above this thing
  // then we have to check it for properties, because they are supposed to
  // look like they are on this object.
  Handle<Object> proto(object->GetPrototype());
  if (proto->IsJSObject() &&
      Handle<JSObject>::cast(proto)->map()->is_hidden_prototype()) {
    return HasLocalPropertyImplementation(Handle<JSObject>::cast(proto), key);
  }
  return Heap::false_value();
}


static Object* Runtime_HasLocalProperty(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);
  CONVERT_CHECKED(String, key, args[1]);

  Object* obj = args[0];
  // Only JS objects can have properties.
  if (obj->IsJSObject()) {
    JSObject* object = JSObject::cast(obj);
    // Fast case - no interceptors.
    if (object->HasRealNamedProperty(key)) return Heap::true_value();
    // Slow case.  Either it's not there or we have an interceptor.  We should
    // have handles for this kind of deal.
    HandleScope scope;
    return HasLocalPropertyImplementation(Handle<JSObject>(object),
                                          Handle<String>(key));
  } else if (obj->IsString()) {
    // Well, there is one exception:  Handle [] on strings.
    uint32_t index;
    if (key->AsArrayIndex(&index)) {
      String* string = String::cast(obj);
      if (index < static_cast<uint32_t>(string->length()))
        return Heap::true_value();
    }
  }
  return Heap::false_value();
}


static Object* Runtime_HasProperty(Arguments args) {
  NoHandleAllocation na;
  ASSERT(args.length() == 2);

  // Only JS objects can have properties.
  if (args[0]->IsJSObject()) {
    JSObject* object = JSObject::cast(args[0]);
    CONVERT_CHECKED(String, key, args[1]);
    if (object->HasProperty(key)) return Heap::true_value();
  }
  return Heap::false_value();
}


static Object* Runtime_HasElement(Arguments args) {
  NoHandleAllocation na;
  ASSERT(args.length() == 2);

  // Only JS objects can have elements.
  if (args[0]->IsJSObject()) {
    JSObject* object = JSObject::cast(args[0]);
    CONVERT_CHECKED(Smi, index_obj, args[1]);
    uint32_t index = index_obj->value();
    if (object->HasElement(index)) return Heap::true_value();
  }
  return Heap::false_value();
}


static Object* Runtime_IsPropertyEnumerable(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);

  CONVERT_CHECKED(JSObject, object, args[0]);
  CONVERT_CHECKED(String, key, args[1]);

  uint32_t index;
  if (key->AsArrayIndex(&index)) {
    return Heap::ToBoolean(object->HasElement(index));
  }

  PropertyAttributes att = object->GetLocalPropertyAttribute(key);
  return Heap::ToBoolean(att != ABSENT && (att & DONT_ENUM) == 0);
}


static Object* Runtime_GetPropertyNames(Arguments args) {
  HandleScope scope;
  ASSERT(args.length() == 1);
  CONVERT_ARG_CHECKED(JSObject, object, 0);
  return *GetKeysFor(object);
}


// Returns either a FixedArray as Runtime_GetPropertyNames,
// or, if the given object has an enum cache that contains
// all enumerable properties of the object and its prototypes
// have none, the map of the object. This is used to speed up
// the check for deletions during a for-in.
static Object* Runtime_GetPropertyNamesFast(Arguments args) {
  ASSERT(args.length() == 1);

  CONVERT_CHECKED(JSObject, raw_object, args[0]);

  if (raw_object->IsSimpleEnum()) return raw_object->map();

  HandleScope scope;
  Handle<JSObject> object(raw_object);
  Handle<FixedArray> content = GetKeysInFixedArrayFor(object,
                                                      INCLUDE_PROTOS);

  // Test again, since cache may have been built by preceding call.
  if (object->IsSimpleEnum()) return object->map();

  return *content;
}


// Find the length of the prototype chain that is to to handled as one. If a
// prototype object is hidden it is to be viewed as part of the the object it
// is prototype for.
static int LocalPrototypeChainLength(JSObject* obj) {
  int count = 1;
  Object* proto = obj->GetPrototype();
  while (proto->IsJSObject() &&
         JSObject::cast(proto)->map()->is_hidden_prototype()) {
    count++;
    proto = JSObject::cast(proto)->GetPrototype();
  }
  return count;
}


// Return the names of the local named properties.
// args[0]: object
static Object* Runtime_GetLocalPropertyNames(Arguments args) {
  HandleScope scope;
  ASSERT(args.length() == 1);
  if (!args[0]->IsJSObject()) {
    return Heap::undefined_value();
  }
  CONVERT_ARG_CHECKED(JSObject, obj, 0);

  // Skip the global proxy as it has no properties and always delegates to the
  // real global object.
  if (obj->IsJSGlobalProxy()) {
    // Only collect names if access is permitted.
    if (obj->IsAccessCheckNeeded() &&
        !Top::MayNamedAccess(*obj, Heap::undefined_value(), v8::ACCESS_KEYS)) {
      Top::ReportFailedAccessCheck(*obj, v8::ACCESS_KEYS);
      return *Factory::NewJSArray(0);
    }
    obj = Handle<JSObject>(JSObject::cast(obj->GetPrototype()));
  }

  // Find the number of objects making up this.
  int length = LocalPrototypeChainLength(*obj);

  // Find the number of local properties for each of the objects.
  ScopedVector<int> local_property_count(length);
  int total_property_count = 0;
  Handle<JSObject> jsproto = obj;
  for (int i = 0; i < length; i++) {
    // Only collect names if access is permitted.
    if (jsproto->IsAccessCheckNeeded() &&
        !Top::MayNamedAccess(*jsproto,
                             Heap::undefined_value(),
                             v8::ACCESS_KEYS)) {
      Top::ReportFailedAccessCheck(*jsproto, v8::ACCESS_KEYS);
      return *Factory::NewJSArray(0);
    }
    int n;
    n = jsproto->NumberOfLocalProperties(static_cast<PropertyAttributes>(NONE));
    local_property_count[i] = n;
    total_property_count += n;
    if (i < length - 1) {
      jsproto = Handle<JSObject>(JSObject::cast(jsproto->GetPrototype()));
    }
  }

  // Allocate an array with storage for all the property names.
  Handle<FixedArray> names = Factory::NewFixedArray(total_property_count);

  // Get the property names.
  jsproto = obj;
  int proto_with_hidden_properties = 0;
  for (int i = 0; i < length; i++) {
    jsproto->GetLocalPropertyNames(*names,
                                   i == 0 ? 0 : local_property_count[i - 1]);
    if (!GetHiddenProperties(jsproto, false)->IsUndefined()) {
      proto_with_hidden_properties++;
    }
    if (i < length - 1) {
      jsproto = Handle<JSObject>(JSObject::cast(jsproto->GetPrototype()));
    }
  }

  // Filter out name of hidden propeties object.
  if (proto_with_hidden_properties > 0) {
    Handle<FixedArray> old_names = names;
    names = Factory::NewFixedArray(
        names->length() - proto_with_hidden_properties);
    int dest_pos = 0;
    for (int i = 0; i < total_property_count; i++) {
      Object* name = old_names->get(i);
      if (name == Heap::hidden_symbol()) {
        continue;
      }
      names->set(dest_pos++, name);
    }
  }

  return *Factory::NewJSArrayWithElements(names);
}


// Return the names of the local indexed properties.
// args[0]: object
static Object* Runtime_GetLocalElementNames(Arguments args) {
  HandleScope scope;
  ASSERT(args.length() == 1);
  if (!args[0]->IsJSObject()) {
    return Heap::undefined_value();
  }
  CONVERT_ARG_CHECKED(JSObject, obj, 0);

  int n = obj->NumberOfLocalElements(static_cast<PropertyAttributes>(NONE));
  Handle<FixedArray> names = Factory::NewFixedArray(n);
  obj->GetLocalElementKeys(*names, static_cast<PropertyAttributes>(NONE));
  return *Factory::NewJSArrayWithElements(names);
}


// Return information on whether an object has a named or indexed interceptor.
// args[0]: object
static Object* Runtime_GetInterceptorInfo(Arguments args) {
  HandleScope scope;
  ASSERT(args.length() == 1);
  if (!args[0]->IsJSObject()) {
    return Smi::FromInt(0);
  }
  CONVERT_ARG_CHECKED(JSObject, obj, 0);

  int result = 0;
  if (obj->HasNamedInterceptor()) result |= 2;
  if (obj->HasIndexedInterceptor()) result |= 1;

  return Smi::FromInt(result);
}


// Return property names from named interceptor.
// args[0]: object
static Object* Runtime_GetNamedInterceptorPropertyNames(Arguments args) {
  HandleScope scope;
  ASSERT(args.length() == 1);
  CONVERT_ARG_CHECKED(JSObject, obj, 0);

  if (obj->HasNamedInterceptor()) {
    v8::Handle<v8::Array> result = GetKeysForNamedInterceptor(obj, obj);
    if (!result.IsEmpty()) return *v8::Utils::OpenHandle(*result);
  }
  return Heap::undefined_value();
}


// Return element names from indexed interceptor.
// args[0]: object
static Object* Runtime_GetIndexedInterceptorElementNames(Arguments args) {
  HandleScope scope;
  ASSERT(args.length() == 1);
  CONVERT_ARG_CHECKED(JSObject, obj, 0);

  if (obj->HasIndexedInterceptor()) {
    v8::Handle<v8::Array> result = GetKeysForIndexedInterceptor(obj, obj);
    if (!result.IsEmpty()) return *v8::Utils::OpenHandle(*result);
  }
  return Heap::undefined_value();
}


static Object* Runtime_LocalKeys(Arguments args) {
  ASSERT_EQ(args.length(), 1);
  CONVERT_CHECKED(JSObject, raw_object, args[0]);
  HandleScope scope;
  Handle<JSObject> object(raw_object);
  Handle<FixedArray> contents = GetKeysInFixedArrayFor(object,
                                                       LOCAL_ONLY);
  // Some fast paths through GetKeysInFixedArrayFor reuse a cached
  // property array and since the result is mutable we have to create
  // a fresh clone on each invocation.
  int length = contents->length();
  Handle<FixedArray> copy = Factory::NewFixedArray(length);
  for (int i = 0; i < length; i++) {
    Object* entry = contents->get(i);
    if (entry->IsString()) {
      copy->set(i, entry);
    } else {
      ASSERT(entry->IsNumber());
      HandleScope scope;
      Handle<Object> entry_handle(entry);
      Handle<Object> entry_str = Factory::NumberToString(entry_handle);
      copy->set(i, *entry_str);
    }
  }
  return *Factory::NewJSArrayWithElements(copy);
}


static Object* Runtime_GetArgumentsProperty(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);

  // Compute the frame holding the arguments.
  JavaScriptFrameIterator it;
  it.AdvanceToArgumentsFrame();
  JavaScriptFrame* frame = it.frame();

  // Get the actual number of provided arguments.
  const uint32_t n = frame->GetProvidedParametersCount();

  // Try to convert the key to an index. If successful and within
  // index return the the argument from the frame.
  uint32_t index;
  if (args[0]->ToArrayIndex(&index) && index < n) {
    return frame->GetParameter(index);
  }

  // Convert the key to a string.
  HandleScope scope;
  bool exception = false;
  Handle<Object> converted =
      Execution::ToString(args.at<Object>(0), &exception);
  if (exception) return Failure::Exception();
  Handle<String> key = Handle<String>::cast(converted);

  // Try to convert the string key into an array index.
  if (key->AsArrayIndex(&index)) {
    if (index < n) {
      return frame->GetParameter(index);
    } else {
      return Top::initial_object_prototype()->GetElement(index);
    }
  }

  // Handle special arguments properties.
  if (key->Equals(Heap::length_symbol())) return Smi::FromInt(n);
  if (key->Equals(Heap::callee_symbol())) return frame->function();

  // Lookup in the initial Object.prototype object.
  return Top::initial_object_prototype()->GetProperty(*key);
}


static Object* Runtime_ToFastProperties(Arguments args) {
  HandleScope scope;

  ASSERT(args.length() == 1);
  Handle<Object> object = args.at<Object>(0);
  if (object->IsJSObject()) {
    Handle<JSObject> js_object = Handle<JSObject>::cast(object);
    if (!js_object->HasFastProperties() && !js_object->IsGlobalObject()) {
      js_object->TransformToFastProperties(0);
    }
  }
  return *object;
}


static Object* Runtime_ToSlowProperties(Arguments args) {
  HandleScope scope;

  ASSERT(args.length() == 1);
  Handle<Object> object = args.at<Object>(0);
  if (object->IsJSObject()) {
    Handle<JSObject> js_object = Handle<JSObject>::cast(object);
    js_object->NormalizeProperties(CLEAR_INOBJECT_PROPERTIES, 0);
  }
  return *object;
}


static Object* Runtime_ToBool(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);

  return args[0]->ToBoolean();
}


// Returns the type string of a value; see ECMA-262, 11.4.3 (p 47).
// Possible optimizations: put the type string into the oddballs.
static Object* Runtime_Typeof(Arguments args) {
  NoHandleAllocation ha;

  Object* obj = args[0];
  if (obj->IsNumber()) return Heap::number_symbol();
  HeapObject* heap_obj = HeapObject::cast(obj);

  // typeof an undetectable object is 'undefined'
  if (heap_obj->map()->is_undetectable()) return Heap::undefined_symbol();

  InstanceType instance_type = heap_obj->map()->instance_type();
  if (instance_type < FIRST_NONSTRING_TYPE) {
    return Heap::string_symbol();
  }

  switch (instance_type) {
    case ODDBALL_TYPE:
      if (heap_obj->IsTrue() || heap_obj->IsFalse()) {
        return Heap::boolean_symbol();
      }
      if (heap_obj->IsNull()) {
        return Heap::object_symbol();
      }
      ASSERT(heap_obj->IsUndefined());
      return Heap::undefined_symbol();
    case JS_FUNCTION_TYPE: case JS_REGEXP_TYPE:
      return Heap::function_symbol();
    default:
      // For any kind of object not handled above, the spec rule for
      // host objects gives that it is okay to return "object"
      return Heap::object_symbol();
  }
}


static bool AreDigits(const char*s, int from, int to) {
  for (int i = from; i < to; i++) {
    if (s[i] < '0' || s[i] > '9') return false;
  }

  return true;
}


static int ParseDecimalInteger(const char*s, int from, int to) {
  ASSERT(to - from < 10);  // Overflow is not possible.
  ASSERT(from < to);
  int d = s[from] - '0';

  for (int i = from + 1; i < to; i++) {
    d = 10 * d + (s[i] - '0');
  }

  return d;
}


static Object* Runtime_StringToNumber(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);
  CONVERT_CHECKED(String, subject, args[0]);
  subject->TryFlatten();

  // Fast case: short integer or some sorts of junk values.
  int len = subject->length();
  if (subject->IsSeqAsciiString()) {
    if (len == 0) return Smi::FromInt(0);

    char const* data = SeqAsciiString::cast(subject)->GetChars();
    bool minus = (data[0] == '-');
    int start_pos = (minus ? 1 : 0);

    if (start_pos == len) {
      return Heap::nan_value();
    } else if (data[start_pos] > '9') {
      // Fast check for a junk value. A valid string may start from a
      // whitespace, a sign ('+' or '-'), the decimal point, a decimal digit or
      // the 'I' character ('Infinity'). All of that have codes not greater than
      // '9' except 'I'.
      if (data[start_pos] != 'I') {
        return Heap::nan_value();
      }
    } else if (len - start_pos < 10 && AreDigits(data, start_pos, len)) {
      // The maximal/minimal smi has 10 digits. If the string has less digits we
      // know it will fit into the smi-data type.
      int d = ParseDecimalInteger(data, start_pos, len);
      if (minus) {
        if (d == 0) return Heap::minus_zero_value();
        d = -d;
      }
      return Smi::FromInt(d);
    }
  }

  // Slower case.
  return Heap::NumberFromDouble(StringToDouble(subject, ALLOW_HEX));
}


static Object* Runtime_StringFromCharCodeArray(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);

  CONVERT_CHECKED(JSArray, codes, args[0]);
  int length = Smi::cast(codes->length())->value();

  // Check if the string can be ASCII.
  int i;
  for (i = 0; i < length; i++) {
    Object* element = codes->GetElement(i);
    CONVERT_NUMBER_CHECKED(int, chr, Int32, element);
    if ((chr & 0xffff) > String::kMaxAsciiCharCode)
      break;
  }

  Object* object = NULL;
  if (i == length) {  // The string is ASCII.
    object = Heap::AllocateRawAsciiString(length);
  } else {  // The string is not ASCII.
    object = Heap::AllocateRawTwoByteString(length);
  }

  if (object->IsFailure()) return object;
  String* result = String::cast(object);
  for (int i = 0; i < length; i++) {
    Object* element = codes->GetElement(i);
    CONVERT_NUMBER_CHECKED(int, chr, Int32, element);
    result->Set(i, chr & 0xffff);
  }
  return result;
}


// kNotEscaped is generated by the following:
//
// #!/bin/perl
// for (my $i = 0; $i < 256; $i++) {
//   print "\n" if $i % 16 == 0;
//   my $c = chr($i);
//   my $escaped = 1;
//   $escaped = 0 if $c =~ m#[A-Za-z0-9@*_+./-]#;
//   print $escaped ? "0, " : "1, ";
// }


static bool IsNotEscaped(uint16_t character) {
  // Only for 8 bit characters, the rest are always escaped (in a different way)
  ASSERT(character < 256);
  static const char kNotEscaped[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1,
    0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  };
  return kNotEscaped[character] != 0;
}


static Object* Runtime_URIEscape(Arguments args) {
  const char hex_chars[] = "0123456789ABCDEF";
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);
  CONVERT_CHECKED(String, source, args[0]);

  source->TryFlatten();

  int escaped_length = 0;
  int length = source->length();
  {
    Access<StringInputBuffer> buffer(&runtime_string_input_buffer);
    buffer->Reset(source);
    while (buffer->has_more()) {
      uint16_t character = buffer->GetNext();
      if (character >= 256) {
        escaped_length += 6;
      } else if (IsNotEscaped(character)) {
        escaped_length++;
      } else {
        escaped_length += 3;
      }
      // We don't allow strings that are longer than a maximal length.
      ASSERT(String::kMaxLength < 0x7fffffff - 6);  // Cannot overflow.
      if (escaped_length > String::kMaxLength) {
        Top::context()->mark_out_of_memory();
        return Failure::OutOfMemoryException();
      }
    }
  }
  // No length change implies no change.  Return original string if no change.
  if (escaped_length == length) {
    return source;
  }
  Object* o = Heap::AllocateRawAsciiString(escaped_length);
  if (o->IsFailure()) return o;
  String* destination = String::cast(o);
  int dest_position = 0;

  Access<StringInputBuffer> buffer(&runtime_string_input_buffer);
  buffer->Rewind();
  while (buffer->has_more()) {
    uint16_t chr = buffer->GetNext();
    if (chr >= 256) {
      destination->Set(dest_position, '%');
      destination->Set(dest_position+1, 'u');
      destination->Set(dest_position+2, hex_chars[chr >> 12]);
      destination->Set(dest_position+3, hex_chars[(chr >> 8) & 0xf]);
      destination->Set(dest_position+4, hex_chars[(chr >> 4) & 0xf]);
      destination->Set(dest_position+5, hex_chars[chr & 0xf]);
      dest_position += 6;
    } else if (IsNotEscaped(chr)) {
      destination->Set(dest_position, chr);
      dest_position++;
    } else {
      destination->Set(dest_position, '%');
      destination->Set(dest_position+1, hex_chars[chr >> 4]);
      destination->Set(dest_position+2, hex_chars[chr & 0xf]);
      dest_position += 3;
    }
  }
  return destination;
}


static inline int TwoDigitHex(uint16_t character1, uint16_t character2) {
  static const signed char kHexValue['g'] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    0,  1,  2,   3,  4,  5,  6,  7,  8,  9, -1, -1, -1, -1, -1, -1,
    -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, 10, 11, 12, 13, 14, 15 };

  if (character1 > 'f') return -1;
  int hi = kHexValue[character1];
  if (hi == -1) return -1;
  if (character2 > 'f') return -1;
  int lo = kHexValue[character2];
  if (lo == -1) return -1;
  return (hi << 4) + lo;
}


static inline int Unescape(String* source,
                           int i,
                           int length,
                           int* step) {
  uint16_t character = source->Get(i);
  int32_t hi = 0;
  int32_t lo = 0;
  if (character == '%' &&
      i <= length - 6 &&
      source->Get(i + 1) == 'u' &&
      (hi = TwoDigitHex(source->Get(i + 2),
                        source->Get(i + 3))) != -1 &&
      (lo = TwoDigitHex(source->Get(i + 4),
                        source->Get(i + 5))) != -1) {
    *step = 6;
    return (hi << 8) + lo;
  } else if (character == '%' &&
      i <= length - 3 &&
      (lo = TwoDigitHex(source->Get(i + 1),
                        source->Get(i + 2))) != -1) {
    *step = 3;
    return lo;
  } else {
    *step = 1;
    return character;
  }
}


static Object* Runtime_URIUnescape(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);
  CONVERT_CHECKED(String, source, args[0]);

  source->TryFlatten();

  bool ascii = true;
  int length = source->length();

  int unescaped_length = 0;
  for (int i = 0; i < length; unescaped_length++) {
    int step;
    if (Unescape(source, i, length, &step) > String::kMaxAsciiCharCode) {
      ascii = false;
    }
    i += step;
  }

  // No length change implies no change.  Return original string if no change.
  if (unescaped_length == length)
    return source;

  Object* o = ascii ?
              Heap::AllocateRawAsciiString(unescaped_length) :
              Heap::AllocateRawTwoByteString(unescaped_length);
  if (o->IsFailure()) return o;
  String* destination = String::cast(o);

  int dest_position = 0;
  for (int i = 0; i < length; dest_position++) {
    int step;
    destination->Set(dest_position, Unescape(source, i, length, &step));
    i += step;
  }
  return destination;
}


static Object* Runtime_StringParseInt(Arguments args) {
  NoHandleAllocation ha;

  CONVERT_CHECKED(String, s, args[0]);
  CONVERT_SMI_CHECKED(radix, args[1]);

  s->TryFlatten();

  RUNTIME_ASSERT(radix == 0 || (2 <= radix && radix <= 36));
  double value = StringToInt(s, radix);
  return Heap::NumberFromDouble(value);
  return Heap::nan_value();
}


static Object* Runtime_StringParseFloat(Arguments args) {
  NoHandleAllocation ha;
  CONVERT_CHECKED(String, str, args[0]);

  // ECMA-262 section 15.1.2.3, empty string is NaN
  double value = StringToDouble(str, ALLOW_TRAILING_JUNK, OS::nan_value());

  // Create a number object from the value.
  return Heap::NumberFromDouble(value);
}


static unibrow::Mapping<unibrow::ToUppercase, 128> to_upper_mapping;
static unibrow::Mapping<unibrow::ToLowercase, 128> to_lower_mapping;


template <class Converter>
static Object* ConvertCaseHelper(String* s,
                                 int length,
                                 int input_string_length,
                                 unibrow::Mapping<Converter, 128>* mapping) {
  // We try this twice, once with the assumption that the result is no longer
  // than the input and, if that assumption breaks, again with the exact
  // length.  This may not be pretty, but it is nicer than what was here before
  // and I hereby claim my vaffel-is.
  //
  // Allocate the resulting string.
  //
  // NOTE: This assumes that the upper/lower case of an ascii
  // character is also ascii.  This is currently the case, but it
  // might break in the future if we implement more context and locale
  // dependent upper/lower conversions.
  Object* o = s->IsAsciiRepresentation()
      ? Heap::AllocateRawAsciiString(length)
      : Heap::AllocateRawTwoByteString(length);
  if (o->IsFailure()) return o;
  String* result = String::cast(o);
  bool has_changed_character = false;

  // Convert all characters to upper case, assuming that they will fit
  // in the buffer
  Access<StringInputBuffer> buffer(&runtime_string_input_buffer);
  buffer->Reset(s);
  unibrow::uchar chars[Converter::kMaxWidth];
  // We can assume that the string is not empty
  uc32 current = buffer->GetNext();
  for (int i = 0; i < length;) {
    bool has_next = buffer->has_more();
    uc32 next = has_next ? buffer->GetNext() : 0;
    int char_length = mapping->get(current, next, chars);
    if (char_length == 0) {
      // The case conversion of this character is the character itself.
      result->Set(i, current);
      i++;
    } else if (char_length == 1) {
      // Common case: converting the letter resulted in one character.
      ASSERT(static_cast<uc32>(chars[0]) != current);
      result->Set(i, chars[0]);
      has_changed_character = true;
      i++;
    } else if (length == input_string_length) {
      // We've assumed that the result would be as long as the
      // input but here is a character that converts to several
      // characters.  No matter, we calculate the exact length
      // of the result and try the whole thing again.
      //
      // Note that this leaves room for optimization.  We could just
      // memcpy what we already have to the result string.  Also,
      // the result string is the last object allocated we could
      // "realloc" it and probably, in the vast majority of cases,
      // extend the existing string to be able to hold the full
      // result.
      int next_length = 0;
      if (has_next) {
        next_length = mapping->get(next, 0, chars);
        if (next_length == 0) next_length = 1;
      }
      int current_length = i + char_length + next_length;
      while (buffer->has_more()) {
        current = buffer->GetNext();
        // NOTE: we use 0 as the next character here because, while
        // the next character may affect what a character converts to,
        // it does not in any case affect the length of what it convert
        // to.
        int char_length = mapping->get(current, 0, chars);
        if (char_length == 0) char_length = 1;
        current_length += char_length;
        if (current_length > Smi::kMaxValue) {
          Top::context()->mark_out_of_memory();
          return Failure::OutOfMemoryException();
        }
      }
      // Try again with the real length.
      return Smi::FromInt(current_length);
    } else {
      for (int j = 0; j < char_length; j++) {
        result->Set(i, chars[j]);
        i++;
      }
      has_changed_character = true;
    }
    current = next;
  }
  if (has_changed_character) {
    return result;
  } else {
    // If we didn't actually change anything in doing the conversion
    // we simple return the result and let the converted string
    // become garbage; there is no reason to keep two identical strings
    // alive.
    return s;
  }
}


namespace {

struct ToLowerTraits {
  typedef unibrow::ToLowercase UnibrowConverter;

  static bool ConvertAscii(char* dst, char* src, int length) {
    bool changed = false;
    for (int i = 0; i < length; ++i) {
      char c = src[i];
      if ('A' <= c && c <= 'Z') {
        c += ('a' - 'A');
        changed = true;
      }
      dst[i] = c;
    }
    return changed;
  }
};


struct ToUpperTraits {
  typedef unibrow::ToUppercase UnibrowConverter;

  static bool ConvertAscii(char* dst, char* src, int length) {
    bool changed = false;
    for (int i = 0; i < length; ++i) {
      char c = src[i];
      if ('a' <= c && c <= 'z') {
        c -= ('a' - 'A');
        changed = true;
      }
      dst[i] = c;
    }
    return changed;
  }
};

}  // namespace


template <typename ConvertTraits>
static Object* ConvertCase(
    Arguments args,
    unibrow::Mapping<typename ConvertTraits::UnibrowConverter, 128>* mapping) {
  NoHandleAllocation ha;
  CONVERT_CHECKED(String, s, args[0]);
  s = s->TryFlattenGetString();

  const int length = s->length();
  // Assume that the string is not empty; we need this assumption later
  if (length == 0) return s;

  // Simpler handling of ascii strings.
  //
  // NOTE: This assumes that the upper/lower case of an ascii
  // character is also ascii.  This is currently the case, but it
  // might break in the future if we implement more context and locale
  // dependent upper/lower conversions.
  if (s->IsSeqAsciiString()) {
    Object* o = Heap::AllocateRawAsciiString(length);
    if (o->IsFailure()) return o;
    SeqAsciiString* result = SeqAsciiString::cast(o);
    bool has_changed_character = ConvertTraits::ConvertAscii(
        result->GetChars(), SeqAsciiString::cast(s)->GetChars(), length);
    return has_changed_character ? result : s;
  }

  Object* answer = ConvertCaseHelper(s, length, length, mapping);
  if (answer->IsSmi()) {
    // Retry with correct length.
    answer = ConvertCaseHelper(s, Smi::cast(answer)->value(), length, mapping);
  }
  return answer;  // This may be a failure.
}


static Object* Runtime_StringToLowerCase(Arguments args) {
  return ConvertCase<ToLowerTraits>(args, &to_lower_mapping);
}


static Object* Runtime_StringToUpperCase(Arguments args) {
  return ConvertCase<ToUpperTraits>(args, &to_upper_mapping);
}


static inline bool IsTrimWhiteSpace(unibrow::uchar c) {
  return unibrow::WhiteSpace::Is(c) || c == 0x200b;
}


static Object* Runtime_StringTrim(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 3);

  CONVERT_CHECKED(String, s, args[0]);
  CONVERT_BOOLEAN_CHECKED(trimLeft, args[1]);
  CONVERT_BOOLEAN_CHECKED(trimRight, args[2]);

  s->TryFlatten();
  int length = s->length();

  int left = 0;
  if (trimLeft) {
    while (left < length && IsTrimWhiteSpace(s->Get(left))) {
      left++;
    }
  }

  int right = length;
  if (trimRight) {
    while (right > left && IsTrimWhiteSpace(s->Get(right - 1))) {
      right--;
    }
  }
  return s->SubString(left, right);
}


template <typename schar, typename pchar>
void FindStringIndices(Vector<const schar> subject,
                       Vector<const pchar> pattern,
                       ZoneList<int>* indices,
                       unsigned int limit) {
  ASSERT(limit > 0);
  // Collect indices of pattern in subject, and the end-of-string index.
  // Stop after finding at most limit values.
  StringSearchStrategy strategy =
      InitializeStringSearch(pattern, sizeof(schar) == 1);
  switch (strategy) {
    case SEARCH_FAIL: return;
    case SEARCH_SHORT: {
      int pattern_length = pattern.length();
      int index = 0;
      while (limit > 0) {
        index = SimpleIndexOf(subject, pattern, index);
        if (index < 0) return;
        indices->Add(index);
        index += pattern_length;
        limit--;
      }
      return;
    }
    case SEARCH_LONG: {
      int pattern_length = pattern.length();
      int index = 0;
      while (limit > 0) {
        index = ComplexIndexOf(subject, pattern, index);
        if (index < 0) return;
        indices->Add(index);
        index += pattern_length;
        limit--;
      }
      return;
    }
    default:
      UNREACHABLE();
      return;
  }
}

template <typename schar>
inline void FindCharIndices(Vector<const schar> subject,
                            const schar pattern_char,
                            ZoneList<int>* indices,
                            unsigned int limit) {
  // Collect indices of pattern_char in subject, and the end-of-string index.
  // Stop after finding at most limit values.
  int index = 0;
  while (limit > 0) {
    index = SingleCharIndexOf(subject, pattern_char, index);
    if (index < 0) return;
    indices->Add(index);
    index++;
    limit--;
  }
}


static Object* Runtime_StringSplit(Arguments args) {
  ASSERT(args.length() == 3);
  HandleScope handle_scope;
  CONVERT_ARG_CHECKED(String, subject, 0);
  CONVERT_ARG_CHECKED(String, pattern, 1);
  CONVERT_NUMBER_CHECKED(uint32_t, limit, Uint32, args[2]);

  int subject_length = subject->length();
  int pattern_length = pattern->length();
  RUNTIME_ASSERT(pattern_length > 0);

  // The limit can be very large (0xffffffffu), but since the pattern
  // isn't empty, we can never create more parts than ~half the length
  // of the subject.

  if (!subject->IsFlat()) FlattenString(subject);

  static const int kMaxInitialListCapacity = 16;

  ZoneScope scope(DELETE_ON_EXIT);

  // Find (up to limit) indices of separator and end-of-string in subject
  int initial_capacity = Min<uint32_t>(kMaxInitialListCapacity, limit);
  ZoneList<int> indices(initial_capacity);
  if (pattern_length == 1) {
    // Special case, go directly to fast single-character split.
    AssertNoAllocation nogc;
    uc16 pattern_char = pattern->Get(0);
    if (subject->IsTwoByteRepresentation()) {
      FindCharIndices(subject->ToUC16Vector(), pattern_char,
                      &indices,
                      limit);
    } else if (pattern_char <= String::kMaxAsciiCharCode) {
      FindCharIndices(subject->ToAsciiVector(),
                      static_cast<char>(pattern_char),
                      &indices,
                      limit);
    }
  } else {
    if (!pattern->IsFlat()) FlattenString(pattern);
    AssertNoAllocation nogc;
    if (subject->IsAsciiRepresentation()) {
      Vector<const char> subject_vector = subject->ToAsciiVector();
      if (pattern->IsAsciiRepresentation()) {
        FindStringIndices(subject_vector,
                          pattern->ToAsciiVector(),
                          &indices,
                          limit);
      } else {
        FindStringIndices(subject_vector,
                          pattern->ToUC16Vector(),
                          &indices,
                          limit);
      }
    } else {
      Vector<const uc16> subject_vector = subject->ToUC16Vector();
      if (pattern->IsAsciiRepresentation()) {
        FindStringIndices(subject_vector,
                          pattern->ToAsciiVector(),
                          &indices,
                          limit);
      } else {
        FindStringIndices(subject_vector,
                          pattern->ToUC16Vector(),
                          &indices,
                          limit);
      }
    }
  }
  if (static_cast<uint32_t>(indices.length()) < limit) {
    indices.Add(subject_length);
  }
  // The list indices now contains the end of each part to create.


  // Create JSArray of substrings separated by separator.
  int part_count = indices.length();

  Handle<JSArray> result = Factory::NewJSArray(part_count);
  result->set_length(Smi::FromInt(part_count));

  ASSERT(result->HasFastElements());

  if (part_count == 1 && indices.at(0) == subject_length) {
    FixedArray::cast(result->elements())->set(0, *subject);
    return *result;
  }

  Handle<FixedArray> elements(FixedArray::cast(result->elements()));
  int part_start = 0;
  for (int i = 0; i < part_count; i++) {
    HandleScope local_loop_handle;
    int part_end = indices.at(i);
    Handle<String> substring =
        Factory::NewSubString(subject, part_start, part_end);
    elements->set(i, *substring);
    part_start = part_end + pattern_length;
  }

  return *result;
}


// Copies ascii characters to the given fixed array looking up
// one-char strings in the cache. Gives up on the first char that is
// not in the cache and fills the remainder with smi zeros. Returns
// the length of the successfully copied prefix.
static int CopyCachedAsciiCharsToArray(const char* chars,
                                       FixedArray* elements,
                                       int length) {
  AssertNoAllocation nogc;
  FixedArray* ascii_cache = Heap::single_character_string_cache();
  Object* undefined = Heap::undefined_value();
  int i;
  for (i = 0; i < length; ++i) {
    Object* value = ascii_cache->get(chars[i]);
    if (value == undefined) break;
    ASSERT(!Heap::InNewSpace(value));
    elements->set(i, value, SKIP_WRITE_BARRIER);
  }
  if (i < length) {
    ASSERT(Smi::FromInt(0) == 0);
    memset(elements->data_start() + i, 0, kPointerSize * (length - i));
  }
#ifdef DEBUG
  for (int j = 0; j < length; ++j) {
    Object* element = elements->get(j);
    ASSERT(element == Smi::FromInt(0) ||
           (element->IsString() && String::cast(element)->LooksValid()));
  }
#endif
  return i;
}


// Converts a String to JSArray.
// For example, "foo" => ["f", "o", "o"].
static Object* Runtime_StringToArray(Arguments args) {
  HandleScope scope;
  ASSERT(args.length() == 1);
  CONVERT_ARG_CHECKED(String, s, 0);

  s->TryFlatten();
  const int length = s->length();

  Handle<FixedArray> elements;
  if (s->IsFlat() && s->IsAsciiRepresentation()) {
    Object* obj = Heap::AllocateUninitializedFixedArray(length);
    if (obj->IsFailure()) return obj;
    elements = Handle<FixedArray>(FixedArray::cast(obj));

    Vector<const char> chars = s->ToAsciiVector();
    // Note, this will initialize all elements (not only the prefix)
    // to prevent GC from seeing partially initialized array.
    int num_copied_from_cache = CopyCachedAsciiCharsToArray(chars.start(),
                                                            *elements,
                                                            length);

    for (int i = num_copied_from_cache; i < length; ++i) {
      elements->set(i, *LookupSingleCharacterStringFromCode(chars[i]));
    }
  } else {
    elements = Factory::NewFixedArray(length);
    for (int i = 0; i < length; ++i) {
      elements->set(i, *LookupSingleCharacterStringFromCode(s->Get(i)));
    }
  }

#ifdef DEBUG
  for (int i = 0; i < length; ++i) {
    ASSERT(String::cast(elements->get(i))->length() == 1);
  }
#endif

  return *Factory::NewJSArrayWithElements(elements);
}


bool Runtime::IsUpperCaseChar(uint16_t ch) {
  unibrow::uchar chars[unibrow::ToUppercase::kMaxWidth];
  int char_length = to_upper_mapping.get(ch, 0, chars);
  return char_length == 0;
}


static Object* Runtime_NumberToString(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);

  Object* number = args[0];
  RUNTIME_ASSERT(number->IsNumber());

  return Heap::NumberToString(number);
}


static Object* Runtime_NumberToStringSkipCache(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);

  Object* number = args[0];
  RUNTIME_ASSERT(number->IsNumber());

  return Heap::NumberToString(number, false);
}


static Object* Runtime_NumberToInteger(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);

  CONVERT_DOUBLE_CHECKED(number, args[0]);

  // We do not include 0 so that we don't have to treat +0 / -0 cases.
  if (number > 0 && number <= Smi::kMaxValue) {
    return Smi::FromInt(static_cast<int>(number));
  }
  return Heap::NumberFromDouble(DoubleToInteger(number));
}


static Object* Runtime_NumberToIntegerMapMinusZero(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);

  CONVERT_DOUBLE_CHECKED(number, args[0]);

  // We do not include 0 so that we don't have to treat +0 / -0 cases.
  if (number > 0 && number <= Smi::kMaxValue) {
    return Smi::FromInt(static_cast<int>(number));
  }

  double double_value = DoubleToInteger(number);
  // Map both -0 and +0 to +0.
  if (double_value == 0) double_value = 0;

  return Heap::NumberFromDouble(double_value);
}


static Object* Runtime_NumberToJSUint32(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);

  CONVERT_NUMBER_CHECKED(int32_t, number, Uint32, args[0]);
  return Heap::NumberFromUint32(number);
}


static Object* Runtime_NumberToJSInt32(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);

  CONVERT_DOUBLE_CHECKED(number, args[0]);

  // We do not include 0 so that we don't have to treat +0 / -0 cases.
  if (number > 0 && number <= Smi::kMaxValue) {
    return Smi::FromInt(static_cast<int>(number));
  }
  return Heap::NumberFromInt32(DoubleToInt32(number));
}


// Converts a Number to a Smi, if possible. Returns NaN if the number is not
// a small integer.
static Object* Runtime_NumberToSmi(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);

  Object* obj = args[0];
  if (obj->IsSmi()) {
    return obj;
  }
  if (obj->IsHeapNumber()) {
    double value = HeapNumber::cast(obj)->value();
    int int_value = FastD2I(value);
    if (value == FastI2D(int_value) && Smi::IsValid(int_value)) {
      return Smi::FromInt(int_value);
    }
  }
  return Heap::nan_value();
}


static Object* Runtime_NumberAdd(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);

  CONVERT_DOUBLE_CHECKED(x, args[0]);
  CONVERT_DOUBLE_CHECKED(y, args[1]);
  return Heap::NumberFromDouble(x + y);
}


static Object* Runtime_NumberSub(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);

  CONVERT_DOUBLE_CHECKED(x, args[0]);
  CONVERT_DOUBLE_CHECKED(y, args[1]);
  return Heap::NumberFromDouble(x - y);
}


static Object* Runtime_NumberMul(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);

  CONVERT_DOUBLE_CHECKED(x, args[0]);
  CONVERT_DOUBLE_CHECKED(y, args[1]);
  return Heap::NumberFromDouble(x * y);
}


static Object* Runtime_NumberUnaryMinus(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);

  CONVERT_DOUBLE_CHECKED(x, args[0]);
  return Heap::NumberFromDouble(-x);
}


static Object* Runtime_NumberAlloc(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 0);

  return Heap::NumberFromDouble(9876543210.0);
}


static Object* Runtime_NumberDiv(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);

  CONVERT_DOUBLE_CHECKED(x, args[0]);
  CONVERT_DOUBLE_CHECKED(y, args[1]);
  return Heap::NumberFromDouble(x / y);
}


static Object* Runtime_NumberMod(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);

  CONVERT_DOUBLE_CHECKED(x, args[0]);
  CONVERT_DOUBLE_CHECKED(y, args[1]);

  x = modulo(x, y);
  // NumberFromDouble may return a Smi instead of a Number object
  return Heap::NumberFromDouble(x);
}


static Object* Runtime_StringAdd(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);
  CONVERT_CHECKED(String, str1, args[0]);
  CONVERT_CHECKED(String, str2, args[1]);
  Counters::string_add_runtime.Increment();
  return Heap::AllocateConsString(str1, str2);
}


template <typename sinkchar>
static inline void StringBuilderConcatHelper(String* special,
                                             sinkchar* sink,
                                             FixedArray* fixed_array,
                                             int array_length) {
  int position = 0;
  for (int i = 0; i < array_length; i++) {
    Object* element = fixed_array->get(i);
    if (element->IsSmi()) {
      // Smi encoding of position and length.
      int encoded_slice = Smi::cast(element)->value();
      int pos;
      int len;
      if (encoded_slice > 0) {
        // Position and length encoded in one smi.
        pos = StringBuilderSubstringPosition::decode(encoded_slice);
        len = StringBuilderSubstringLength::decode(encoded_slice);
      } else {
        // Position and length encoded in two smis.
        Object* obj = fixed_array->get(++i);
        ASSERT(obj->IsSmi());
        pos = Smi::cast(obj)->value();
        len = -encoded_slice;
      }
      String::WriteToFlat(special,
                          sink + position,
                          pos,
                          pos + len);
      position += len;
    } else {
      String* string = String::cast(element);
      int element_length = string->length();
      String::WriteToFlat(string, sink + position, 0, element_length);
      position += element_length;
    }
  }
}


static Object* Runtime_StringBuilderConcat(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 3);
  CONVERT_CHECKED(JSArray, array, args[0]);
  if (!args[1]->IsSmi()) {
    Top::context()->mark_out_of_memory();
    return Failure::OutOfMemoryException();
  }
  int array_length = Smi::cast(args[1])->value();
  CONVERT_CHECKED(String, special, args[2]);

  // This assumption is used by the slice encoding in one or two smis.
  ASSERT(Smi::kMaxValue >= String::kMaxLength);

  int special_length = special->length();
  if (!array->HasFastElements()) {
    return Top::Throw(Heap::illegal_argument_symbol());
  }
  FixedArray* fixed_array = FixedArray::cast(array->elements());
  if (fixed_array->length() < array_length) {
    array_length = fixed_array->length();
  }

  if (array_length == 0) {
    return Heap::empty_string();
  } else if (array_length == 1) {
    Object* first = fixed_array->get(0);
    if (first->IsString()) return first;
  }

  bool ascii = special->HasOnlyAsciiChars();
  int position = 0;
  for (int i = 0; i < array_length; i++) {
    int increment = 0;
    Object* elt = fixed_array->get(i);
    if (elt->IsSmi()) {
      // Smi encoding of position and length.
      int smi_value = Smi::cast(elt)->value();
      int pos;
      int len;
      if (smi_value > 0) {
        // Position and length encoded in one smi.
        pos = StringBuilderSubstringPosition::decode(smi_value);
        len = StringBuilderSubstringLength::decode(smi_value);
      } else {
        // Position and length encoded in two smis.
        len = -smi_value;
        // Get the position and check that it is a positive smi.
        i++;
        if (i >= array_length) {
          return Top::Throw(Heap::illegal_argument_symbol());
        }
        Object* next_smi = fixed_array->get(i);
        if (!next_smi->IsSmi()) {
          return Top::Throw(Heap::illegal_argument_symbol());
        }
        pos = Smi::cast(next_smi)->value();
        if (pos < 0) {
          return Top::Throw(Heap::illegal_argument_symbol());
        }
      }
      ASSERT(pos >= 0);
      ASSERT(len >= 0);
      if (pos > special_length || len > special_length - pos) {
        return Top::Throw(Heap::illegal_argument_symbol());
      }
      increment = len;
    } else if (elt->IsString()) {
      String* element = String::cast(elt);
      int element_length = element->length();
      increment = element_length;
      if (ascii && !element->HasOnlyAsciiChars()) {
        ascii = false;
      }
    } else {
      return Top::Throw(Heap::illegal_argument_symbol());
    }
    if (increment > String::kMaxLength - position) {
      Top::context()->mark_out_of_memory();
      return Failure::OutOfMemoryException();
    }
    position += increment;
  }

  int length = position;
  Object* object;

  if (ascii) {
    object = Heap::AllocateRawAsciiString(length);
    if (object->IsFailure()) return object;
    SeqAsciiString* answer = SeqAsciiString::cast(object);
    StringBuilderConcatHelper(special,
                              answer->GetChars(),
                              fixed_array,
                              array_length);
    return answer;
  } else {
    object = Heap::AllocateRawTwoByteString(length);
    if (object->IsFailure()) return object;
    SeqTwoByteString* answer = SeqTwoByteString::cast(object);
    StringBuilderConcatHelper(special,
                              answer->GetChars(),
                              fixed_array,
                              array_length);
    return answer;
  }
}


static Object* Runtime_NumberOr(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);

  CONVERT_NUMBER_CHECKED(int32_t, x, Int32, args[0]);
  CONVERT_NUMBER_CHECKED(int32_t, y, Int32, args[1]);
  return Heap::NumberFromInt32(x | y);
}


static Object* Runtime_NumberAnd(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);

  CONVERT_NUMBER_CHECKED(int32_t, x, Int32, args[0]);
  CONVERT_NUMBER_CHECKED(int32_t, y, Int32, args[1]);
  return Heap::NumberFromInt32(x & y);
}


static Object* Runtime_NumberXor(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);

  CONVERT_NUMBER_CHECKED(int32_t, x, Int32, args[0]);
  CONVERT_NUMBER_CHECKED(int32_t, y, Int32, args[1]);
  return Heap::NumberFromInt32(x ^ y);
}


static Object* Runtime_NumberNot(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);

  CONVERT_NUMBER_CHECKED(int32_t, x, Int32, args[0]);
  return Heap::NumberFromInt32(~x);
}


static Object* Runtime_NumberShl(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);

  CONVERT_NUMBER_CHECKED(int32_t, x, Int32, args[0]);
  CONVERT_NUMBER_CHECKED(int32_t, y, Int32, args[1]);
  return Heap::NumberFromInt32(x << (y & 0x1f));
}


static Object* Runtime_NumberShr(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);

  CONVERT_NUMBER_CHECKED(uint32_t, x, Uint32, args[0]);
  CONVERT_NUMBER_CHECKED(int32_t, y, Int32, args[1]);
  return Heap::NumberFromUint32(x >> (y & 0x1f));
}


static Object* Runtime_NumberSar(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);

  CONVERT_NUMBER_CHECKED(int32_t, x, Int32, args[0]);
  CONVERT_NUMBER_CHECKED(int32_t, y, Int32, args[1]);
  return Heap::NumberFromInt32(ArithmeticShiftRight(x, y & 0x1f));
}


static Object* Runtime_NumberEquals(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);

  CONVERT_DOUBLE_CHECKED(x, args[0]);
  CONVERT_DOUBLE_CHECKED(y, args[1]);
  if (isnan(x)) return Smi::FromInt(NOT_EQUAL);
  if (isnan(y)) return Smi::FromInt(NOT_EQUAL);
  if (x == y) return Smi::FromInt(EQUAL);
  Object* result;
  if ((fpclassify(x) == FP_ZERO) && (fpclassify(y) == FP_ZERO)) {
    result = Smi::FromInt(EQUAL);
  } else {
    result = Smi::FromInt(NOT_EQUAL);
  }
  return result;
}


static Object* Runtime_StringEquals(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);

  CONVERT_CHECKED(String, x, args[0]);
  CONVERT_CHECKED(String, y, args[1]);

  bool not_equal = !x->Equals(y);
  // This is slightly convoluted because the value that signifies
  // equality is 0 and inequality is 1 so we have to negate the result
  // from String::Equals.
  ASSERT(not_equal == 0 || not_equal == 1);
  STATIC_CHECK(EQUAL == 0);
  STATIC_CHECK(NOT_EQUAL == 1);
  return Smi::FromInt(not_equal);
}


static Object* Runtime_NumberCompare(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 3);

  CONVERT_DOUBLE_CHECKED(x, args[0]);
  CONVERT_DOUBLE_CHECKED(y, args[1]);
  if (isnan(x) || isnan(y)) return args[2];
  if (x == y) return Smi::FromInt(EQUAL);
  if (isless(x, y)) return Smi::FromInt(LESS);
  return Smi::FromInt(GREATER);
}


// Compare two Smis as if they were converted to strings and then
// compared lexicographically.
static Object* Runtime_SmiLexicographicCompare(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);

  // Arrays for the individual characters of the two Smis.  Smis are
  // 31 bit integers and 10 decimal digits are therefore enough.
  static int x_elms[10];
  static int y_elms[10];

  // Extract the integer values from the Smis.
  CONVERT_CHECKED(Smi, x, args[0]);
  CONVERT_CHECKED(Smi, y, args[1]);
  int x_value = x->value();
  int y_value = y->value();

  // If the integers are equal so are the string representations.
  if (x_value == y_value) return Smi::FromInt(EQUAL);

  // If one of the integers are zero the normal integer order is the
  // same as the lexicographic order of the string representations.
  if (x_value == 0 || y_value == 0) return Smi::FromInt(x_value - y_value);

  // If only one of the integers is negative the negative number is
  // smallest because the char code of '-' is less than the char code
  // of any digit.  Otherwise, we make both values positive.
  if (x_value < 0 || y_value < 0) {
    if (y_value >= 0) return Smi::FromInt(LESS);
    if (x_value >= 0) return Smi::FromInt(GREATER);
    x_value = -x_value;
    y_value = -y_value;
  }

  // Convert the integers to arrays of their decimal digits.
  int x_index = 0;
  int y_index = 0;
  while (x_value > 0) {
    x_elms[x_index++] = x_value % 10;
    x_value /= 10;
  }
  while (y_value > 0) {
    y_elms[y_index++] = y_value % 10;
    y_value /= 10;
  }

  // Loop through the arrays of decimal digits finding the first place
  // where they differ.
  while (--x_index >= 0 && --y_index >= 0) {
    int diff = x_elms[x_index] - y_elms[y_index];
    if (diff != 0) return Smi::FromInt(diff);
  }

  // If one array is a suffix of the other array, the longest array is
  // the representation of the largest of the Smis in the
  // lexicographic ordering.
  return Smi::FromInt(x_index - y_index);
}


static Object* StringInputBufferCompare(String* x, String* y) {
  static StringInputBuffer bufx;
  static StringInputBuffer bufy;
  bufx.Reset(x);
  bufy.Reset(y);
  while (bufx.has_more() && bufy.has_more()) {
    int d = bufx.GetNext() - bufy.GetNext();
    if (d < 0) return Smi::FromInt(LESS);
    else if (d > 0) return Smi::FromInt(GREATER);
  }

  // x is (non-trivial) prefix of y:
  if (bufy.has_more()) return Smi::FromInt(LESS);
  // y is prefix of x:
  return Smi::FromInt(bufx.has_more() ? GREATER : EQUAL);
}


static Object* FlatStringCompare(String* x, String* y) {
  ASSERT(x->IsFlat());
  ASSERT(y->IsFlat());
  Object* equal_prefix_result = Smi::FromInt(EQUAL);
  int prefix_length = x->length();
  if (y->length() < prefix_length) {
    prefix_length = y->length();
    equal_prefix_result = Smi::FromInt(GREATER);
  } else if (y->length() > prefix_length) {
    equal_prefix_result = Smi::FromInt(LESS);
  }
  int r;
  if (x->IsAsciiRepresentation()) {
    Vector<const char> x_chars = x->ToAsciiVector();
    if (y->IsAsciiRepresentation()) {
      Vector<const char> y_chars = y->ToAsciiVector();
      r = CompareChars(x_chars.start(), y_chars.start(), prefix_length);
    } else {
      Vector<const uc16> y_chars = y->ToUC16Vector();
      r = CompareChars(x_chars.start(), y_chars.start(), prefix_length);
    }
  } else {
    Vector<const uc16> x_chars = x->ToUC16Vector();
    if (y->IsAsciiRepresentation()) {
      Vector<const char> y_chars = y->ToAsciiVector();
      r = CompareChars(x_chars.start(), y_chars.start(), prefix_length);
    } else {
      Vector<const uc16> y_chars = y->ToUC16Vector();
      r = CompareChars(x_chars.start(), y_chars.start(), prefix_length);
    }
  }
  Object* result;
  if (r == 0) {
    result = equal_prefix_result;
  } else {
    result = (r < 0) ? Smi::FromInt(LESS) : Smi::FromInt(GREATER);
  }
  ASSERT(result == StringInputBufferCompare(x, y));
  return result;
}


static Object* Runtime_StringCompare(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);

  CONVERT_CHECKED(String, x, args[0]);
  CONVERT_CHECKED(String, y, args[1]);

  Counters::string_compare_runtime.Increment();

  // A few fast case tests before we flatten.
  if (x == y) return Smi::FromInt(EQUAL);
  if (y->length() == 0) {
    if (x->length() == 0) return Smi::FromInt(EQUAL);
    return Smi::FromInt(GREATER);
  } else if (x->length() == 0) {
    return Smi::FromInt(LESS);
  }

  int d = x->Get(0) - y->Get(0);
  if (d < 0) return Smi::FromInt(LESS);
  else if (d > 0) return Smi::FromInt(GREATER);

  Object* obj = Heap::PrepareForCompare(x);
  if (obj->IsFailure()) return obj;
  obj = Heap::PrepareForCompare(y);
  if (obj->IsFailure()) return obj;

  return (x->IsFlat() && y->IsFlat()) ? FlatStringCompare(x, y)
                                      : StringInputBufferCompare(x, y);
}


static Object* Runtime_Math_acos(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);
  Counters::math_acos.Increment();

  CONVERT_DOUBLE_CHECKED(x, args[0]);
  return TranscendentalCache::Get(TranscendentalCache::ACOS, x);
}


static Object* Runtime_Math_asin(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);
  Counters::math_asin.Increment();

  CONVERT_DOUBLE_CHECKED(x, args[0]);
  return TranscendentalCache::Get(TranscendentalCache::ASIN, x);
}


static Object* Runtime_Math_atan(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);
  Counters::math_atan.Increment();

  CONVERT_DOUBLE_CHECKED(x, args[0]);
  return TranscendentalCache::Get(TranscendentalCache::ATAN, x);
}


static Object* Runtime_Math_atan2(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);
  Counters::math_atan2.Increment();

  CONVERT_DOUBLE_CHECKED(x, args[0]);
  CONVERT_DOUBLE_CHECKED(y, args[1]);
  double result;
  if (isinf(x) && isinf(y)) {
    // Make sure that the result in case of two infinite arguments
    // is a multiple of Pi / 4. The sign of the result is determined
    // by the first argument (x) and the sign of the second argument
    // determines the multiplier: one or three.
    static double kPiDividedBy4 = 0.78539816339744830962;
    int multiplier = (x < 0) ? -1 : 1;
    if (y < 0) multiplier *= 3;
    result = multiplier * kPiDividedBy4;
  } else {
    result = atan2(x, y);
  }
  return Heap::AllocateHeapNumber(result);
}


static Object* Runtime_Math_ceil(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);
  Counters::math_ceil.Increment();

  CONVERT_DOUBLE_CHECKED(x, args[0]);
  return Heap::NumberFromDouble(ceiling(x));
}


static Object* Runtime_Math_cos(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);
  Counters::math_cos.Increment();

  CONVERT_DOUBLE_CHECKED(x, args[0]);
  return TranscendentalCache::Get(TranscendentalCache::COS, x);
}


static Object* Runtime_Math_exp(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);
  Counters::math_exp.Increment();

  CONVERT_DOUBLE_CHECKED(x, args[0]);
  return TranscendentalCache::Get(TranscendentalCache::EXP, x);
}


static Object* Runtime_Math_floor(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);
  Counters::math_floor.Increment();

  CONVERT_DOUBLE_CHECKED(x, args[0]);
  return Heap::NumberFromDouble(floor(x));
}


static Object* Runtime_Math_log(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);
  Counters::math_log.Increment();

  CONVERT_DOUBLE_CHECKED(x, args[0]);
  return TranscendentalCache::Get(TranscendentalCache::LOG, x);
}


// Helper function to compute x^y, where y is known to be an
// integer. Uses binary decomposition to limit the number of
// multiplications; see the discussion in "Hacker's Delight" by Henry
// S. Warren, Jr., figure 11-6, page 213.
static double powi(double x, int y) {
  ASSERT(y != kMinInt);
  unsigned n = (y < 0) ? -y : y;
  double m = x;
  double p = 1;
  while (true) {
    if ((n & 1) != 0) p *= m;
    n >>= 1;
    if (n == 0) {
      if (y < 0) {
        // Unfortunately, we have to be careful when p has reached
        // infinity in the computation, because sometimes the higher
        // internal precision in the pow() implementation would have
        // given us a finite p. This happens very rarely.
        double result = 1.0 / p;
        return (result == 0 && isinf(p))
            ? pow(x, static_cast<double>(y))  // Avoid pow(double, int).
            : result;
      } else {
        return p;
      }
    }
    m *= m;
  }
}


static Object* Runtime_Math_pow(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);
  Counters::math_pow.Increment();

  CONVERT_DOUBLE_CHECKED(x, args[0]);

  // If the second argument is a smi, it is much faster to call the
  // custom powi() function than the generic pow().
  if (args[1]->IsSmi()) {
    int y = Smi::cast(args[1])->value();
    return Heap::NumberFromDouble(powi(x, y));
  }

  CONVERT_DOUBLE_CHECKED(y, args[1]);

  if (!isinf(x)) {
    if (y == 0.5) {
      // It's not uncommon to use Math.pow(x, 0.5) to compute the
      // square root of a number. To speed up such computations, we
      // explictly check for this case and use the sqrt() function
      // which is faster than pow().
      return Heap::AllocateHeapNumber(sqrt(x));
    } else if (y == -0.5) {
      // Optimized using Math.pow(x, -0.5) == 1 / Math.pow(x, 0.5).
      return Heap::AllocateHeapNumber(1.0 / sqrt(x));
    }
  }

  if (y == 0) {
    return Smi::FromInt(1);
  } else if (isnan(y) || ((x == 1 || x == -1) && isinf(y))) {
    return Heap::nan_value();
  } else {
    return Heap::AllocateHeapNumber(pow(x, y));
  }
}

// Fast version of Math.pow if we know that y is not an integer and
// y is not -0.5 or 0.5. Used as slowcase from codegen.
static Object* Runtime_Math_pow_cfunction(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);
  CONVERT_DOUBLE_CHECKED(x, args[0]);
  CONVERT_DOUBLE_CHECKED(y, args[1]);
  if (y == 0) {
      return Smi::FromInt(1);
  } else if (isnan(y) || ((x == 1 || x == -1) && isinf(y))) {
      return Heap::nan_value();
  } else {
      return Heap::AllocateHeapNumber(pow(x, y));
  }
}


static Object* Runtime_RoundNumber(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);
  Counters::math_round.Increment();

  if (!args[0]->IsHeapNumber()) {
    // Must be smi. Return the argument unchanged for all the other types
    // to make fuzz-natives test happy.
    return args[0];
  }

  HeapNumber* number = reinterpret_cast<HeapNumber*>(args[0]);

  double value = number->value();
  int exponent = number->get_exponent();
  int sign = number->get_sign();

  // We compare with kSmiValueSize - 3 because (2^30 - 0.1) has exponent 29 and
  // should be rounded to 2^30, which is not smi.
  if (!sign && exponent <= kSmiValueSize - 3) {
    return Smi::FromInt(static_cast<int>(value + 0.5));
  }

  // If the magnitude is big enough, there's no place for fraction part. If we
  // try to add 0.5 to this number, 1.0 will be added instead.
  if (exponent >= 52) {
    return number;
  }

  if (sign && value >= -0.5) return Heap::minus_zero_value();

  // Do not call NumberFromDouble() to avoid extra checks.
  return Heap::AllocateHeapNumber(floor(value + 0.5));
}


static Object* Runtime_Math_sin(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);
  Counters::math_sin.Increment();

  CONVERT_DOUBLE_CHECKED(x, args[0]);
  return TranscendentalCache::Get(TranscendentalCache::SIN, x);
}


static Object* Runtime_Math_sqrt(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);
  Counters::math_sqrt.Increment();

  CONVERT_DOUBLE_CHECKED(x, args[0]);
  return Heap::AllocateHeapNumber(sqrt(x));
}


static Object* Runtime_Math_tan(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);
  Counters::math_tan.Increment();

  CONVERT_DOUBLE_CHECKED(x, args[0]);
  return TranscendentalCache::Get(TranscendentalCache::TAN, x);
}


static int MakeDay(int year, int month, int day) {
  static const int day_from_month[] = {0, 31, 59, 90, 120, 151,
                                       181, 212, 243, 273, 304, 334};
  static const int day_from_month_leap[] = {0, 31, 60, 91, 121, 152,
                                            182, 213, 244, 274, 305, 335};

  year += month / 12;
  month %= 12;
  if (month < 0) {
    year--;
    month += 12;
  }

  ASSERT(month >= 0);
  ASSERT(month < 12);

  // year_delta is an arbitrary number such that:
  // a) year_delta = -1 (mod 400)
  // b) year + year_delta > 0 for years in the range defined by
  //    ECMA 262 - 15.9.1.1, i.e. upto 100,000,000 days on either side of
  //    Jan 1 1970. This is required so that we don't run into integer
  //    division of negative numbers.
  // c) there shouldn't be an overflow for 32-bit integers in the following
  //    operations.
  static const int year_delta = 399999;
  static const int base_day = 365 * (1970 + year_delta) +
                              (1970 + year_delta) / 4 -
                              (1970 + year_delta) / 100 +
                              (1970 + year_delta) / 400;

  int year1 = year + year_delta;
  int day_from_year = 365 * year1 +
                      year1 / 4 -
                      year1 / 100 +
                      year1 / 400 -
                      base_day;

  if (year % 4 || (year % 100 == 0 && year % 400 != 0)) {
    return day_from_year + day_from_month[month] + day - 1;
  }

  return day_from_year + day_from_month_leap[month] + day - 1;
}


static Object* Runtime_DateMakeDay(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 3);

  CONVERT_SMI_CHECKED(year, args[0]);
  CONVERT_SMI_CHECKED(month, args[1]);
  CONVERT_SMI_CHECKED(date, args[2]);

  return Smi::FromInt(MakeDay(year, month, date));
}


static const int kDays4Years[] = {0, 365, 2 * 365, 3 * 365 + 1};
static const int kDaysIn4Years = 4 * 365 + 1;
static const int kDaysIn100Years = 25 * kDaysIn4Years - 1;
static const int kDaysIn400Years = 4 * kDaysIn100Years + 1;
static const int kDays1970to2000 = 30 * 365 + 7;
static const int kDaysOffset = 1000 * kDaysIn400Years + 5 * kDaysIn400Years -
                               kDays1970to2000;
static const int kYearsOffset = 400000;

static const char kDayInYear[] = {
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30, 31,

      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30, 31,

      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30, 31,

      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
      22, 23, 24, 25, 26, 27, 28, 29, 30, 31};

static const char kMonthInYear[] = {
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1,
      2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
      2, 2, 2, 2, 2, 2,
      3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
      3, 3, 3, 3, 3,
      4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
      4, 4, 4, 4, 4, 4,
      5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
      5, 5, 5, 5, 5,
      6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
      6, 6, 6, 6, 6, 6,
      7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
      7, 7, 7, 7, 7, 7,
      8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
      8, 8, 8, 8, 8,
      9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
      9, 9, 9, 9, 9, 9,
      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
      11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
      11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,

      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1,
      2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
      2, 2, 2, 2, 2, 2,
      3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
      3, 3, 3, 3, 3,
      4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
      4, 4, 4, 4, 4, 4,
      5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
      5, 5, 5, 5, 5,
      6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
      6, 6, 6, 6, 6, 6,
      7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
      7, 7, 7, 7, 7, 7,
      8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
      8, 8, 8, 8, 8,
      9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
      9, 9, 9, 9, 9, 9,
      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
      11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
      11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,

      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1,
      2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
      2, 2, 2, 2, 2, 2,
      3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
      3, 3, 3, 3, 3,
      4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
      4, 4, 4, 4, 4, 4,
      5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
      5, 5, 5, 5, 5,
      6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
      6, 6, 6, 6, 6, 6,
      7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
      7, 7, 7, 7, 7, 7,
      8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
      8, 8, 8, 8, 8,
      9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
      9, 9, 9, 9, 9, 9,
      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
      11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
      11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,

      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1,
      2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
      2, 2, 2, 2, 2, 2,
      3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
      3, 3, 3, 3, 3,
      4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
      4, 4, 4, 4, 4, 4,
      5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
      5, 5, 5, 5, 5,
      6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
      6, 6, 6, 6, 6, 6,
      7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
      7, 7, 7, 7, 7, 7,
      8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
      8, 8, 8, 8, 8,
      9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
      9, 9, 9, 9, 9, 9,
      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
      11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
      11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11};


// This function works for dates from 1970 to 2099.
static inline void DateYMDFromTimeAfter1970(int date,
                                            int& year, int& month, int& day) {
#ifdef DEBUG
  int save_date = date;  // Need this for ASSERT in the end.
#endif

  year = 1970 + (4 * date + 2) / kDaysIn4Years;
  date %= kDaysIn4Years;

  month = kMonthInYear[date];
  day = kDayInYear[date];

  ASSERT(MakeDay(year, month, day) == save_date);
}


static inline void DateYMDFromTimeSlow(int date,
                                       int& year, int& month, int& day) {
#ifdef DEBUG
  int save_date = date;  // Need this for ASSERT in the end.
#endif

  date += kDaysOffset;
  year = 400 * (date / kDaysIn400Years) - kYearsOffset;
  date %= kDaysIn400Years;

  ASSERT(MakeDay(year, 0, 1) + date == save_date);

  date--;
  int yd1 = date / kDaysIn100Years;
  date %= kDaysIn100Years;
  year += 100 * yd1;

  date++;
  int yd2 = date / kDaysIn4Years;
  date %= kDaysIn4Years;
  year += 4 * yd2;

  date--;
  int yd3 = date / 365;
  date %= 365;
  year += yd3;

  bool is_leap = (!yd1 || yd2) && !yd3;

  ASSERT(date >= -1);
  ASSERT(is_leap || (date >= 0));
  ASSERT((date < 365) || (is_leap && (date < 366)));
  ASSERT(is_leap == ((year % 4 == 0) && (year % 100 || (year % 400 == 0))));
  ASSERT(is_leap || ((MakeDay(year, 0, 1) + date) == save_date));
  ASSERT(!is_leap || ((MakeDay(year, 0, 1) + date + 1) == save_date));

  if (is_leap) {
    day = kDayInYear[2*365 + 1 + date];
    month = kMonthInYear[2*365 + 1 + date];
  } else {
    day = kDayInYear[date];
    month = kMonthInYear[date];
  }

  ASSERT(MakeDay(year, month, day) == save_date);
}


static inline void DateYMDFromTime(int date,
                                   int& year, int& month, int& day) {
  if (date >= 0 && date < 32 * kDaysIn4Years) {
    DateYMDFromTimeAfter1970(date, year, month, day);
  } else {
    DateYMDFromTimeSlow(date, year, month, day);
  }
}


static Object* Runtime_DateYMDFromTime(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);

  CONVERT_DOUBLE_CHECKED(t, args[0]);
  CONVERT_CHECKED(JSArray, res_array, args[1]);

  int year, month, day;
  DateYMDFromTime(static_cast<int>(floor(t / 86400000)), year, month, day);

  res_array->SetElement(0, Smi::FromInt(year));
  res_array->SetElement(1, Smi::FromInt(month));
  res_array->SetElement(2, Smi::FromInt(day));

  return Heap::undefined_value();
}


static Object* Runtime_NewArgumentsFast(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 3);

  JSFunction* callee = JSFunction::cast(args[0]);
  Object** parameters = reinterpret_cast<Object**>(args[1]);
  const int length = Smi::cast(args[2])->value();

  Object* result = Heap::AllocateArgumentsObject(callee, length);
  if (result->IsFailure()) return result;
  // Allocate the elements if needed.
  if (length > 0) {
    // Allocate the fixed array.
    Object* obj = Heap::AllocateRawFixedArray(length);
    if (obj->IsFailure()) return obj;

    AssertNoAllocation no_gc;
    FixedArray* array = reinterpret_cast<FixedArray*>(obj);
    array->set_map(Heap::fixed_array_map());
    array->set_length(length);

    WriteBarrierMode mode = array->GetWriteBarrierMode(no_gc);
    for (int i = 0; i < length; i++) {
      array->set(i, *--parameters, mode);
    }
    JSObject::cast(result)->set_elements(FixedArray::cast(obj));
  }
  return result;
}


static Object* Runtime_NewClosure(Arguments args) {
  HandleScope scope;
  ASSERT(args.length() == 2);
  CONVERT_ARG_CHECKED(Context, context, 0);
  CONVERT_ARG_CHECKED(SharedFunctionInfo, shared, 1);

  PretenureFlag pretenure = (context->global_context() == *context)
      ? TENURED       // Allocate global closures in old space.
      : NOT_TENURED;  // Allocate local closures in new space.
  Handle<JSFunction> result =
      Factory::NewFunctionFromSharedFunctionInfo(shared, context, pretenure);
  return *result;
}

static Object* Runtime_NewObjectFromBound(Arguments args) {
  HandleScope scope;
  ASSERT(args.length() == 2);
  CONVERT_ARG_CHECKED(JSFunction, function, 0);
  CONVERT_ARG_CHECKED(JSArray, params, 1);

  RUNTIME_ASSERT(params->HasFastElements());
  FixedArray* fixed = FixedArray::cast(params->elements());

  int fixed_length = Smi::cast(params->length())->value();
  SmartPointer<Object**> param_data(NewArray<Object**>(fixed_length));
  for (int i = 0; i < fixed_length; i++) {
    Handle<Object> val = Handle<Object>(fixed->get(i));
    param_data[i] = val.location();
  }

  bool exception = false;
  Handle<Object> result = Execution::New(
      function, fixed_length, *param_data, &exception);
  if (exception) {
      return Failure::Exception();
  }
  ASSERT(!result.is_null());
  return *result;
}


static Code* ComputeConstructStub(Handle<JSFunction> function) {
  Handle<Object> prototype = Factory::null_value();
  if (function->has_instance_prototype()) {
    prototype = Handle<Object>(function->instance_prototype());
  }
  if (function->shared()->CanGenerateInlineConstructor(*prototype)) {
    ConstructStubCompiler compiler;
    Object* code = compiler.CompileConstructStub(function->shared());
    if (code->IsFailure()) {
      return Builtins::builtin(Builtins::JSConstructStubGeneric);
    }
    return Code::cast(code);
  }

  return function->shared()->construct_stub();
}


static Object* Runtime_NewObject(Arguments args) {
  HandleScope scope;
  ASSERT(args.length() == 1);

  Handle<Object> constructor = args.at<Object>(0);

  // If the constructor isn't a proper function we throw a type error.
  if (!constructor->IsJSFunction()) {
    Vector< Handle<Object> > arguments = HandleVector(&constructor, 1);
    Handle<Object> type_error =
        Factory::NewTypeError("not_constructor", arguments);
    return Top::Throw(*type_error);
  }

  Handle<JSFunction> function = Handle<JSFunction>::cast(constructor);

  // If function should not have prototype, construction is not allowed. In this
  // case generated code bailouts here, since function has no initial_map.
  if (!function->should_have_prototype()) {
    Vector< Handle<Object> > arguments = HandleVector(&constructor, 1);
    Handle<Object> type_error =
        Factory::NewTypeError("not_constructor", arguments);
    return Top::Throw(*type_error);
  }

#ifdef ENABLE_DEBUGGER_SUPPORT
  // Handle stepping into constructors if step into is active.
  if (Debug::StepInActive()) {
    Debug::HandleStepIn(function, Handle<Object>::null(), 0, true);
  }
#endif

  if (function->has_initial_map()) {
    if (function->initial_map()->instance_type() == JS_FUNCTION_TYPE) {
      // The 'Function' function ignores the receiver object when
      // called using 'new' and creates a new JSFunction object that
      // is returned.  The receiver object is only used for error
      // reporting if an error occurs when constructing the new
      // JSFunction. Factory::NewJSObject() should not be used to
      // allocate JSFunctions since it does not properly initialize
      // the shared part of the function. Since the receiver is
      // ignored anyway, we use the global object as the receiver
      // instead of a new JSFunction object. This way, errors are
      // reported the same way whether or not 'Function' is called
      // using 'new'.
      return Top::context()->global();
    }
  }

  // The function should be compiled for the optimization hints to be available.
  Handle<SharedFunctionInfo> shared(function->shared());
  EnsureCompiled(shared, CLEAR_EXCEPTION);

  bool first_allocation = !function->has_initial_map();
  Handle<JSObject> result = Factory::NewJSObject(function);
  if (first_allocation) {
    Handle<Code> stub = Handle<Code>(
        ComputeConstructStub(Handle<JSFunction>(function)));
    shared->set_construct_stub(*stub);
  }

  Counters::constructed_objects.Increment();
  Counters::constructed_objects_runtime.Increment();

  return *result;
}


static Object* Runtime_LazyCompile(Arguments args) {
  HandleScope scope;
  ASSERT(args.length() == 1);

  Handle<JSFunction> function = args.at<JSFunction>(0);
#ifdef DEBUG
  if (FLAG_trace_lazy) {
    PrintF("[lazy: ");
    function->shared()->name()->Print();
    PrintF("]\n");
  }
#endif

  // Compile the target function.  Here we compile using CompileLazyInLoop in
  // order to get the optimized version.  This helps code like delta-blue
  // that calls performance-critical routines through constructors.  A
  // constructor call doesn't use a CallIC, it uses a LoadIC followed by a
  // direct call.  Since the in-loop tracking takes place through CallICs
  // this means that things called through constructors are never known to
  // be in loops.  We compile them as if they are in loops here just in case.
  ASSERT(!function->is_compiled());
  if (!CompileLazyInLoop(function, Handle<Object>::null(), KEEP_EXCEPTION)) {
    return Failure::Exception();
  }

  return function->code();
}


static Object* Runtime_GetFunctionDelegate(Arguments args) {
  HandleScope scope;
  ASSERT(args.length() == 1);
  RUNTIME_ASSERT(!args[0]->IsJSFunction());
  return *Execution::GetFunctionDelegate(args.at<Object>(0));
}


static Object* Runtime_GetConstructorDelegate(Arguments args) {
  HandleScope scope;
  ASSERT(args.length() == 1);
  RUNTIME_ASSERT(!args[0]->IsJSFunction());
  return *Execution::GetConstructorDelegate(args.at<Object>(0));
}


static Object* Runtime_NewContext(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);

  CONVERT_CHECKED(JSFunction, function, args[0]);
  int length = function->shared()->scope_info()->NumberOfContextSlots();
  Object* result = Heap::AllocateFunctionContext(length, function);
  if (result->IsFailure()) return result;

  Top::set_context(Context::cast(result));

  return result;  // non-failure
}

static Object* PushContextHelper(Object* object, bool is_catch_context) {
  // Convert the object to a proper JavaScript object.
  Object* js_object = object;
  if (!js_object->IsJSObject()) {
    js_object = js_object->ToObject();
    if (js_object->IsFailure()) {
      if (!Failure::cast(js_object)->IsInternalError()) return js_object;
      HandleScope scope;
      Handle<Object> handle(object);
      Handle<Object> result =
          Factory::NewTypeError("with_expression", HandleVector(&handle, 1));
      return Top::Throw(*result);
    }
  }

  Object* result =
      Heap::AllocateWithContext(Top::context(),
                                JSObject::cast(js_object),
                                is_catch_context);
  if (result->IsFailure()) return result;

  Context* context = Context::cast(result);
  Top::set_context(context);

  return result;
}


static Object* Runtime_PushContext(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);
  return PushContextHelper(args[0], false);
}


static Object* Runtime_PushCatchContext(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);
  return PushContextHelper(args[0], true);
}


static Object* Runtime_LookupContext(Arguments args) {
  HandleScope scope;
  ASSERT(args.length() == 2);

  CONVERT_ARG_CHECKED(Context, context, 0);
  CONVERT_ARG_CHECKED(String, name, 1);

  int index;
  PropertyAttributes attributes;
  ContextLookupFlags flags = FOLLOW_CHAINS;
  Handle<Object> holder =
      context->Lookup(name, flags, &index, &attributes);

  if (index < 0 && !holder.is_null()) {
    ASSERT(holder->IsJSObject());
    return *holder;
  }

  // No intermediate context found. Use global object by default.
  return Top::context()->global();
}


// A mechanism to return a pair of Object pointers in registers (if possible).
// How this is achieved is calling convention-dependent.
// All currently supported x86 compiles uses calling conventions that are cdecl
// variants where a 64-bit value is returned in two 32-bit registers
// (edx:eax on ia32, r1:r0 on ARM).
// In AMD-64 calling convention a struct of two pointers is returned in rdx:rax.
// In Win64 calling convention, a struct of two pointers is returned in memory,
// allocated by the caller, and passed as a pointer in a hidden first parameter.
#ifdef V8_HOST_ARCH_64_BIT
struct ObjectPair {
  Object* x;
  Object* y;
};

static inline ObjectPair MakePair(Object* x, Object* y) {
  ObjectPair result = {x, y};
  // Pointers x and y returned in rax and rdx, in AMD-x64-abi.
  // In Win64 they are assigned to a hidden first argument.
  return result;
}
#else
typedef uint64_t ObjectPair;
static inline ObjectPair MakePair(Object* x, Object* y) {
  return reinterpret_cast<uint32_t>(x) |
      (reinterpret_cast<ObjectPair>(y) << 32);
}
#endif


static inline Object* Unhole(Object* x, PropertyAttributes attributes) {
  ASSERT(!x->IsTheHole() || (attributes & READ_ONLY) != 0);
  USE(attributes);
  return x->IsTheHole() ? Heap::undefined_value() : x;
}


static JSObject* ComputeReceiverForNonGlobal(JSObject* holder) {
  ASSERT(!holder->IsGlobalObject());
  Context* top = Top::context();
  // Get the context extension function.
  JSFunction* context_extension_function =
      top->global_context()->context_extension_function();
  // If the holder isn't a context extension object, we just return it
  // as the receiver. This allows arguments objects to be used as
  // receivers, but only if they are put in the context scope chain
  // explicitly via a with-statement.
  Object* constructor = holder->map()->constructor();
  if (constructor != context_extension_function) return holder;
  // Fall back to using the global object as the receiver if the
  // property turns out to be a local variable allocated in a context
  // extension object - introduced via eval.
  return top->global()->global_receiver();
}


static ObjectPair LoadContextSlotHelper(Arguments args, bool throw_error) {
  HandleScope scope;
  ASSERT_EQ(2, args.length());

  if (!args[0]->IsContext() || !args[1]->IsString()) {
    return MakePair(Top::ThrowIllegalOperation(), NULL);
  }
  Handle<Context> context = args.at<Context>(0);
  Handle<String> name = args.at<String>(1);

  int index;
  PropertyAttributes attributes;
  ContextLookupFlags flags = FOLLOW_CHAINS;
  Handle<Object> holder =
      context->Lookup(name, flags, &index, &attributes);

  // If the index is non-negative, the slot has been found in a local
  // variable or a parameter. Read it from the context object or the
  // arguments object.
  if (index >= 0) {
    // If the "property" we were looking for is a local variable or an
    // argument in a context, the receiver is the global object; see
    // ECMA-262, 3rd., 10.1.6 and 10.2.3.
    JSObject* receiver = Top::context()->global()->global_receiver();
    Object* value = (holder->IsContext())
        ? Context::cast(*holder)->get(index)
        : JSObject::cast(*holder)->GetElement(index);
    return MakePair(Unhole(value, attributes), receiver);
  }

  // If the holder is found, we read the property from it.
  if (!holder.is_null() && holder->IsJSObject()) {
    ASSERT(Handle<JSObject>::cast(holder)->HasProperty(*name));
    JSObject* object = JSObject::cast(*holder);
    JSObject* receiver;
    if (object->IsGlobalObject()) {
      receiver = GlobalObject::cast(object)->global_receiver();
    } else if (context->is_exception_holder(*holder)) {
      receiver = Top::context()->global()->global_receiver();
    } else {
      receiver = ComputeReceiverForNonGlobal(object);
    }
    // No need to unhole the value here. This is taken care of by the
    // GetProperty function.
    Object* value = object->GetProperty(*name);
    return MakePair(value, receiver);
  }

  if (throw_error) {
    // The property doesn't exist - throw exception.
    Handle<Object> reference_error =
        Factory::NewReferenceError("not_defined", HandleVector(&name, 1));
    return MakePair(Top::Throw(*reference_error), NULL);
  } else {
    // The property doesn't exist - return undefined
    return MakePair(Heap::undefined_value(), Heap::undefined_value());
  }
}


static ObjectPair Runtime_LoadContextSlot(Arguments args) {
  return LoadContextSlotHelper(args, true);
}


static ObjectPair Runtime_LoadContextSlotNoReferenceError(Arguments args) {
  return LoadContextSlotHelper(args, false);
}


static Object* Runtime_StoreContextSlot(Arguments args) {
  HandleScope scope;
  ASSERT(args.length() == 3);

  Handle<Object> value(args[0]);
  CONVERT_ARG_CHECKED(Context, context, 1);
  CONVERT_ARG_CHECKED(String, name, 2);

  int index;
  PropertyAttributes attributes;
  ContextLookupFlags flags = FOLLOW_CHAINS;
  Handle<Object> holder =
      context->Lookup(name, flags, &index, &attributes);

  if (index >= 0) {
    if (holder->IsContext()) {
      // Ignore if read_only variable.
      if ((attributes & READ_ONLY) == 0) {
        Handle<Context>::cast(holder)->set(index, *value);
      }
    } else {
      ASSERT((attributes & READ_ONLY) == 0);
      Object* result =
          Handle<JSObject>::cast(holder)->SetElement(index, *value);
      USE(result);
      ASSERT(!result->IsFailure());
    }
    return *value;
  }

  // Slow case: The property is not in a FixedArray context.
  // It is either in an JSObject extension context or it was not found.
  Handle<JSObject> context_ext;

  if (!holder.is_null()) {
    // The property exists in the extension context.
    context_ext = Handle<JSObject>::cast(holder);
  } else {
    // The property was not found. It needs to be stored in the global context.
    ASSERT(attributes == ABSENT);
    attributes = NONE;
    context_ext = Handle<JSObject>(Top::context()->global());
  }

  // Set the property, but ignore if read_only variable on the context
  // extension object itself.
  if ((attributes & READ_ONLY) == 0 ||
      (context_ext->GetLocalPropertyAttribute(*name) == ABSENT)) {
    Handle<Object> set = SetProperty(context_ext, name, value, attributes);
    if (set.is_null()) {
      // Failure::Exception is converted to a null handle in the
      // handle-based methods such as SetProperty.  We therefore need
      // to convert null handles back to exceptions.
      ASSERT(Top::has_pending_exception());
      return Failure::Exception();
    }
  }
  return *value;
}


static Object* Runtime_Throw(Arguments args) {
  HandleScope scope;
  ASSERT(args.length() == 1);

  return Top::Throw(args[0]);
}


static Object* Runtime_ReThrow(Arguments args) {
  HandleScope scope;
  ASSERT(args.length() == 1);

  return Top::ReThrow(args[0]);
}


static Object* Runtime_PromoteScheduledException(Arguments args) {
  ASSERT_EQ(0, args.length());
  return Top::PromoteScheduledException();
}


static Object* Runtime_ThrowReferenceError(Arguments args) {
  HandleScope scope;
  ASSERT(args.length() == 1);

  Handle<Object> name(args[0]);
  Handle<Object> reference_error =
    Factory::NewReferenceError("not_defined", HandleVector(&name, 1));
  return Top::Throw(*reference_error);
}


static Object* Runtime_StackOverflow(Arguments args) {
  NoHandleAllocation na;
  return Top::StackOverflow();
}


static Object* Runtime_StackGuard(Arguments args) {
  ASSERT(args.length() == 1);

  // First check if this is a real stack overflow.
  if (StackGuard::IsStackOverflow()) {
    return Runtime_StackOverflow(args);
  }

  return Execution::HandleStackGuardInterrupt();
}


// NOTE: These PrintXXX functions are defined for all builds (not just
// DEBUG builds) because we may want to be able to trace function
// calls in all modes.
static void PrintString(String* str) {
  // not uncommon to have empty strings
  if (str->length() > 0) {
    SmartPointer<char> s =
        str->ToCString(DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL);
    PrintF("%s", *s);
  }
}


static void PrintObject(Object* obj) {
  if (obj->IsSmi()) {
    PrintF("%d", Smi::cast(obj)->value());
  } else if (obj->IsString() || obj->IsSymbol()) {
    PrintString(String::cast(obj));
  } else if (obj->IsNumber()) {
    PrintF("%g", obj->Number());
  } else if (obj->IsFailure()) {
    PrintF("<failure>");
  } else if (obj->IsUndefined()) {
    PrintF("<undefined>");
  } else if (obj->IsNull()) {
    PrintF("<null>");
  } else if (obj->IsTrue()) {
    PrintF("<true>");
  } else if (obj->IsFalse()) {
    PrintF("<false>");
  } else {
    PrintF("%p", obj);
  }
}


static int StackSize() {
  int n = 0;
  for (JavaScriptFrameIterator it; !it.done(); it.Advance()) n++;
  return n;
}


static void PrintTransition(Object* result) {
  // indentation
  { const int nmax = 80;
    int n = StackSize();
    if (n <= nmax)
      PrintF("%4d:%*s", n, n, "");
    else
      PrintF("%4d:%*s", n, nmax, "...");
  }

  if (result == NULL) {
    // constructor calls
    JavaScriptFrameIterator it;
    JavaScriptFrame* frame = it.frame();
    if (frame->IsConstructor()) PrintF("new ");
    // function name
    Object* fun = frame->function();
    if (fun->IsJSFunction()) {
      PrintObject(JSFunction::cast(fun)->shared()->name());
    } else {
      PrintObject(fun);
    }
    // function arguments
    // (we are intentionally only printing the actually
    // supplied parameters, not all parameters required)
    PrintF("(this=");
    PrintObject(frame->receiver());
    const int length = frame->GetProvidedParametersCount();
    for (int i = 0; i < length; i++) {
      PrintF(", ");
      PrintObject(frame->GetParameter(i));
    }
    PrintF(") {\n");

  } else {
    // function result
    PrintF("} -> ");
    PrintObject(result);
    PrintF("\n");
  }
}


static Object* Runtime_TraceEnter(Arguments args) {
  ASSERT(args.length() == 0);
  NoHandleAllocation ha;
  PrintTransition(NULL);
  return Heap::undefined_value();
}


static Object* Runtime_TraceExit(Arguments args) {
  NoHandleAllocation ha;
  PrintTransition(args[0]);
  return args[0];  // return TOS
}


static Object* Runtime_DebugPrint(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);

#ifdef DEBUG
  if (args[0]->IsString()) {
    // If we have a string, assume it's a code "marker"
    // and print some interesting cpu debugging info.
    JavaScriptFrameIterator it;
    JavaScriptFrame* frame = it.frame();
    PrintF("fp = %p, sp = %p, caller_sp = %p: ",
           frame->fp(), frame->sp(), frame->caller_sp());
  } else {
    PrintF("DebugPrint: ");
  }
  args[0]->Print();
  if (args[0]->IsHeapObject()) {
    PrintF("\n");
    HeapObject::cast(args[0])->map()->Print();
  }
#else
  // ShortPrint is available in release mode. Print is not.
  args[0]->ShortPrint();
#endif
  PrintF("\n");
  Flush();

  return args[0];  // return TOS
}


static Object* Runtime_DebugTrace(Arguments args) {
  ASSERT(args.length() == 0);
  NoHandleAllocation ha;
  Top::PrintStack();
  return Heap::undefined_value();
}


static Object* Runtime_DateCurrentTime(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 0);

  // According to ECMA-262, section 15.9.1, page 117, the precision of
  // the number in a Date object representing a particular instant in
  // time is milliseconds. Therefore, we floor the result of getting
  // the OS time.
  double millis = floor(OS::TimeCurrentMillis());
  return Heap::NumberFromDouble(millis);
}


static Object* Runtime_DateParseString(Arguments args) {
  HandleScope scope;
  ASSERT(args.length() == 2);

  CONVERT_ARG_CHECKED(String, str, 0);
  FlattenString(str);

  CONVERT_ARG_CHECKED(JSArray, output, 1);
  RUNTIME_ASSERT(output->HasFastElements());

  AssertNoAllocation no_allocation;

  FixedArray* output_array = FixedArray::cast(output->elements());
  RUNTIME_ASSERT(output_array->length() >= DateParser::OUTPUT_SIZE);
  bool result;
  if (str->IsAsciiRepresentation()) {
    result = DateParser::Parse(str->ToAsciiVector(), output_array);
  } else {
    ASSERT(str->IsTwoByteRepresentation());
    result = DateParser::Parse(str->ToUC16Vector(), output_array);
  }

  if (result) {
    return *output;
  } else {
    return Heap::null_value();
  }
}


static Object* Runtime_DateLocalTimezone(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);

  CONVERT_DOUBLE_CHECKED(x, args[0]);
  const char* zone = OS::LocalTimezone(x);
  return Heap::AllocateStringFromUtf8(CStrVector(zone));
}


static Object* Runtime_DateLocalTimeOffset(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 0);

  return Heap::NumberFromDouble(OS::LocalTimeOffset());
}


static Object* Runtime_DateDaylightSavingsOffset(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);

  CONVERT_DOUBLE_CHECKED(x, args[0]);
  return Heap::NumberFromDouble(OS::DaylightSavingsOffset(x));
}


static Object* Runtime_GlobalReceiver(Arguments args) {
  ASSERT(args.length() == 1);
  Object* global = args[0];
  if (!global->IsJSGlobalObject()) return Heap::null_value();
  return JSGlobalObject::cast(global)->global_receiver();
}


static Object* Runtime_CompileString(Arguments args) {
  HandleScope scope;
  ASSERT_EQ(2, args.length());
  CONVERT_ARG_CHECKED(String, source, 0);
  CONVERT_ARG_CHECKED(Oddball, is_json, 1)

  // Compile source string in the global context.
  Handle<Context> context(Top::context()->global_context());
  Compiler::ValidationState validate = (is_json->IsTrue())
    ? Compiler::VALIDATE_JSON : Compiler::DONT_VALIDATE_JSON;
  Handle<SharedFunctionInfo> shared = Compiler::CompileEval(source,
                                                            context,
                                                            true,
                                                            validate);
  if (shared.is_null()) return Failure::Exception();
  Handle<JSFunction> fun =
      Factory::NewFunctionFromSharedFunctionInfo(shared, context, NOT_TENURED);
  return *fun;
}


static ObjectPair CompileGlobalEval(Handle<String> source,
                                    Handle<Object> receiver) {
  // Deal with a normal eval call with a string argument. Compile it
  // and return the compiled function bound in the local context.
  Handle<SharedFunctionInfo> shared = Compiler::CompileEval(
      source,
      Handle<Context>(Top::context()),
      Top::context()->IsGlobalContext(),
      Compiler::DONT_VALIDATE_JSON);
  if (shared.is_null()) return MakePair(Failure::Exception(), NULL);
  Handle<JSFunction> compiled = Factory::NewFunctionFromSharedFunctionInfo(
      shared,
      Handle<Context>(Top::context()),
      NOT_TENURED);
  return MakePair(*compiled, *receiver);
}


static ObjectPair Runtime_ResolvePossiblyDirectEval(Arguments args) {
  ASSERT(args.length() == 3);
  if (!args[0]->IsJSFunction()) {
    return MakePair(Top::ThrowIllegalOperation(), NULL);
  }

  HandleScope scope;
  Handle<JSFunction> callee = args.at<JSFunction>(0);
  Handle<Object> receiver;  // Will be overwritten.

  // Compute the calling context.
  Handle<Context> context = Handle<Context>(Top::context());
#ifdef DEBUG
  // Make sure Top::context() agrees with the old code that traversed
  // the stack frames to compute the context.
  StackFrameLocator locator;
  JavaScriptFrame* frame = locator.FindJavaScriptFrame(0);
  ASSERT(Context::cast(frame->context()) == *context);
#endif

  // Find where the 'eval' symbol is bound. It is unaliased only if
  // it is bound in the global context.
  int index = -1;
  PropertyAttributes attributes = ABSENT;
  while (true) {
    receiver = context->Lookup(Factory::eval_symbol(), FOLLOW_PROTOTYPE_CHAIN,
                               &index, &attributes);
    // Stop search when eval is found or when the global context is
    // reached.
    if (attributes != ABSENT || context->IsGlobalContext()) break;
    if (context->is_function_context()) {
      context = Handle<Context>(Context::cast(context->closure()->context()));
    } else {
      context = Handle<Context>(context->previous());
    }
  }

  // If eval could not be resolved, it has been deleted and we need to
  // throw a reference error.
  if (attributes == ABSENT) {
    Handle<Object> name = Factory::eval_symbol();
    Handle<Object> reference_error =
        Factory::NewReferenceError("not_defined", HandleVector(&name, 1));
    return MakePair(Top::Throw(*reference_error), NULL);
  }

  if (!context->IsGlobalContext()) {
    // 'eval' is not bound in the global context. Just call the function
    // with the given arguments. This is not necessarily the global eval.
    if (receiver->IsContext()) {
      context = Handle<Context>::cast(receiver);
      receiver = Handle<Object>(context->get(index));
    } else if (receiver->IsJSContextExtensionObject()) {
      receiver = Handle<JSObject>(Top::context()->global()->global_receiver());
    }
    return MakePair(*callee, *receiver);
  }

  // 'eval' is bound in the global context, but it may have been overwritten.
  // Compare it to the builtin 'GlobalEval' function to make sure.
  if (*callee != Top::global_context()->global_eval_fun() ||
      !args[1]->IsString()) {
    return MakePair(*callee, Top::context()->global()->global_receiver());
  }

  return CompileGlobalEval(args.at<String>(1), args.at<Object>(2));
}


static ObjectPair Runtime_ResolvePossiblyDirectEvalNoLookup(Arguments args) {
  ASSERT(args.length() == 3);
  if (!args[0]->IsJSFunction()) {
    return MakePair(Top::ThrowIllegalOperation(), NULL);
  }

  HandleScope scope;
  Handle<JSFunction> callee = args.at<JSFunction>(0);

  // 'eval' is bound in the global context, but it may have been overwritten.
  // Compare it to the builtin 'GlobalEval' function to make sure.
  if (*callee != Top::global_context()->global_eval_fun() ||
      !args[1]->IsString()) {
    return MakePair(*callee, Top::context()->global()->global_receiver());
  }

  return CompileGlobalEval(args.at<String>(1), args.at<Object>(2));
}


static Object* Runtime_SetNewFunctionAttributes(Arguments args) {
  // This utility adjusts the property attributes for newly created Function
  // object ("new Function(...)") by changing the map.
  // All it does is changing the prototype property to enumerable
  // as specified in ECMA262, 15.3.5.2.
  HandleScope scope;
  ASSERT(args.length() == 1);
  CONVERT_ARG_CHECKED(JSFunction, func, 0);
  ASSERT(func->map()->instance_type() ==
         Top::function_instance_map()->instance_type());
  ASSERT(func->map()->instance_size() ==
         Top::function_instance_map()->instance_size());
  func->set_map(*Top::function_instance_map());
  return *func;
}


// Push an array unto an array of arrays if it is not already in the
// array.  Returns true if the element was pushed on the stack and
// false otherwise.
static Object* Runtime_PushIfAbsent(Arguments args) {
  ASSERT(args.length() == 2);
  CONVERT_CHECKED(JSArray, array, args[0]);
  CONVERT_CHECKED(JSArray, element, args[1]);
  RUNTIME_ASSERT(array->HasFastElements());
  int length = Smi::cast(array->length())->value();
  FixedArray* elements = FixedArray::cast(array->elements());
  for (int i = 0; i < length; i++) {
    if (elements->get(i) == element) return Heap::false_value();
  }
  Object* obj = array->SetFastElement(length, element);
  if (obj->IsFailure()) return obj;
  return Heap::true_value();
}


/**
 * A simple visitor visits every element of Array's.
 * The backend storage can be a fixed array for fast elements case,
 * or a dictionary for sparse array. Since Dictionary is a subtype
 * of FixedArray, the class can be used by both fast and slow cases.
 * The second parameter of the constructor, fast_elements, specifies
 * whether the storage is a FixedArray or Dictionary.
 *
 * An index limit is used to deal with the situation that a result array
 * length overflows 32-bit non-negative integer.
 */
class ArrayConcatVisitor {
 public:
  ArrayConcatVisitor(Handle<FixedArray> storage,
                     uint32_t index_limit,
                     bool fast_elements) :
      storage_(storage), index_limit_(index_limit),
      index_offset_(0), fast_elements_(fast_elements) { }

  void visit(uint32_t i, Handle<Object> elm) {
    if (i >= index_limit_ - index_offset_) return;
    uint32_t index = index_offset_ + i;

    if (fast_elements_) {
      ASSERT(index < static_cast<uint32_t>(storage_->length()));
      storage_->set(index, *elm);

    } else {
      Handle<NumberDictionary> dict = Handle<NumberDictionary>::cast(storage_);
      Handle<NumberDictionary> result =
          Factory::DictionaryAtNumberPut(dict, index, elm);
      if (!result.is_identical_to(dict))
        storage_ = result;
    }
  }

  void increase_index_offset(uint32_t delta) {
    if (index_limit_ - index_offset_ < delta) {
      index_offset_ = index_limit_;
    } else {
      index_offset_ += delta;
    }
  }

  Handle<FixedArray> storage() { return storage_; }

 private:
  Handle<FixedArray> storage_;
  // Limit on the accepted indices. Elements with indices larger than the
  // limit are ignored by the visitor.
  uint32_t index_limit_;
  // Index after last seen index. Always less than or equal to index_limit_.
  uint32_t index_offset_;
  const bool fast_elements_;
};


template<class ExternalArrayClass, class ElementType>
static uint32_t IterateExternalArrayElements(Handle<JSObject> receiver,
                                             bool elements_are_ints,
                                             bool elements_are_guaranteed_smis,
                                             uint32_t range,
                                             ArrayConcatVisitor* visitor) {
  Handle<ExternalArrayClass> array(
      ExternalArrayClass::cast(receiver->elements()));
  uint32_t len = Min(static_cast<uint32_t>(array->length()), range);

  if (visitor != NULL) {
    if (elements_are_ints) {
      if (elements_are_guaranteed_smis) {
        for (uint32_t j = 0; j < len; j++) {
          Handle<Smi> e(Smi::FromInt(static_cast<int>(array->get(j))));
          visitor->visit(j, e);
        }
      } else {
        for (uint32_t j = 0; j < len; j++) {
          int64_t val = static_cast<int64_t>(array->get(j));
          if (Smi::IsValid(static_cast<intptr_t>(val))) {
            Handle<Smi> e(Smi::FromInt(static_cast<int>(val)));
            visitor->visit(j, e);
          } else {
            Handle<Object> e(
                Heap::AllocateHeapNumber(static_cast<ElementType>(val)));
            visitor->visit(j, e);
          }
        }
      }
    } else {
      for (uint32_t j = 0; j < len; j++) {
        Handle<Object> e(Heap::AllocateHeapNumber(array->get(j)));
        visitor->visit(j, e);
      }
    }
  }

  return len;
}

/**
 * A helper function that visits elements of a JSObject. Only elements
 * whose index between 0 and range (exclusive) are visited.
 *
 * If the third parameter, visitor, is not NULL, the visitor is called
 * with parameters, 'visitor_index_offset + element index' and the element.
 *
 * It returns the number of visisted elements.
 */
static uint32_t IterateElements(Handle<JSObject> receiver,
                                uint32_t range,
                                ArrayConcatVisitor* visitor) {
  uint32_t num_of_elements = 0;

  switch (receiver->GetElementsKind()) {
    case JSObject::FAST_ELEMENTS: {
      Handle<FixedArray> elements(FixedArray::cast(receiver->elements()));
      uint32_t len = elements->length();
      if (range < len) {
        len = range;
      }

      for (uint32_t j = 0; j < len; j++) {
        Handle<Object> e(elements->get(j));
        if (!e->IsTheHole()) {
          num_of_elements++;
          if (visitor) {
            visitor->visit(j, e);
          }
        }
      }
      break;
    }
    case JSObject::PIXEL_ELEMENTS: {
      Handle<PixelArray> pixels(PixelArray::cast(receiver->elements()));
      uint32_t len = pixels->length();
      if (range < len) {
        len = range;
      }

      for (uint32_t j = 0; j < len; j++) {
        num_of_elements++;
        if (visitor != NULL) {
          Handle<Smi> e(Smi::FromInt(pixels->get(j)));
          visitor->visit(j, e);
        }
      }
      break;
    }
    case JSObject::EXTERNAL_BYTE_ELEMENTS: {
      num_of_elements =
          IterateExternalArrayElements<ExternalByteArray, int8_t>(
              receiver, true, true, range, visitor);
      break;
    }
    case JSObject::EXTERNAL_UNSIGNED_BYTE_ELEMENTS: {
      num_of_elements =
          IterateExternalArrayElements<ExternalUnsignedByteArray, uint8_t>(
              receiver, true, true, range, visitor);
      break;
    }
    case JSObject::EXTERNAL_SHORT_ELEMENTS: {
      num_of_elements =
          IterateExternalArrayElements<ExternalShortArray, int16_t>(
              receiver, true, true, range, visitor);
      break;
    }
    case JSObject::EXTERNAL_UNSIGNED_SHORT_ELEMENTS: {
      num_of_elements =
          IterateExternalArrayElements<ExternalUnsignedShortArray, uint16_t>(
              receiver, true, true, range, visitor);
      break;
    }
    case JSObject::EXTERNAL_INT_ELEMENTS: {
      num_of_elements =
          IterateExternalArrayElements<ExternalIntArray, int32_t>(
              receiver, true, false, range, visitor);
      break;
    }
    case JSObject::EXTERNAL_UNSIGNED_INT_ELEMENTS: {
      num_of_elements =
          IterateExternalArrayElements<ExternalUnsignedIntArray, uint32_t>(
              receiver, true, false, range, visitor);
      break;
    }
    case JSObject::EXTERNAL_FLOAT_ELEMENTS: {
      num_of_elements =
          IterateExternalArrayElements<ExternalFloatArray, float>(
              receiver, false, false, range, visitor);
      break;
    }
    case JSObject::DICTIONARY_ELEMENTS: {
      Handle<NumberDictionary> dict(receiver->element_dictionary());
      uint32_t capacity = dict->Capacity();
      for (uint32_t j = 0; j < capacity; j++) {
        Handle<Object> k(dict->KeyAt(j));
        if (dict->IsKey(*k)) {
          ASSERT(k->IsNumber());
          uint32_t index = static_cast<uint32_t>(k->Number());
          if (index < range) {
            num_of_elements++;
            if (visitor) {
              visitor->visit(index, Handle<Object>(dict->ValueAt(j)));
            }
          }
        }
      }
      break;
    }
    default:
      UNREACHABLE();
      break;
  }

  return num_of_elements;
}


/**
 * A helper function that visits elements of an Array object, and elements
 * on its prototypes.
 *
 * Elements on prototypes are visited first, and only elements whose indices
 * less than Array length are visited.
 *
 * If a ArrayConcatVisitor object is given, the visitor is called with
 * parameters, element's index + visitor_index_offset and the element.
 *
 * The returned number of elements is an upper bound on the actual number
 * of elements added. If the same element occurs in more than one object
 * in the array's prototype chain, it will be counted more than once, but
 * will only occur once in the result.
 */
static uint32_t IterateArrayAndPrototypeElements(Handle<JSArray> array,
                                                 ArrayConcatVisitor* visitor) {
  uint32_t range = static_cast<uint32_t>(array->length()->Number());
  Handle<Object> obj = array;

  static const int kEstimatedPrototypes = 3;
  List< Handle<JSObject> > objects(kEstimatedPrototypes);

  // Visit prototype first. If an element on the prototype is shadowed by
  // the inheritor using the same index, the ArrayConcatVisitor visits
  // the prototype element before the shadowing element.
  // The visitor can simply overwrite the old value by new value using
  // the same index.  This follows Array::concat semantics.
  while (!obj->IsNull()) {
    objects.Add(Handle<JSObject>::cast(obj));
    obj = Handle<Object>(obj->GetPrototype());
  }

  uint32_t nof_elements = 0;
  for (int i = objects.length() - 1; i >= 0; i--) {
    Handle<JSObject> obj = objects[i];
    uint32_t encountered_elements =
        IterateElements(Handle<JSObject>::cast(obj), range, visitor);

    if (encountered_elements > JSObject::kMaxElementCount - nof_elements) {
      nof_elements = JSObject::kMaxElementCount;
    } else {
      nof_elements += encountered_elements;
    }
  }

  return nof_elements;
}


/**
 * A helper function of Runtime_ArrayConcat.
 *
 * The first argument is an Array of arrays and objects. It is the
 * same as the arguments array of Array::concat JS function.
 *
 * If an argument is an Array object, the function visits array
 * elements.  If an argument is not an Array object, the function
 * visits the object as if it is an one-element array.
 *
 * If the result array index overflows 32-bit unsigned integer, the rounded
 * non-negative number is used as new length. For example, if one
 * array length is 2^32 - 1, second array length is 1, the
 * concatenated array length is 0.
 * TODO(lrn) Change length behavior to ECMAScript 5 specification (length
 * is one more than the last array index to get a value assigned).
 */
static uint32_t IterateArguments(Handle<JSArray> arguments,
                                 ArrayConcatVisitor* visitor) {
  uint32_t visited_elements = 0;
  uint32_t num_of_args = static_cast<uint32_t>(arguments->length()->Number());

  for (uint32_t i = 0; i < num_of_args; i++) {
    Handle<Object> obj(arguments->GetElement(i));
    if (obj->IsJSArray()) {
      Handle<JSArray> array = Handle<JSArray>::cast(obj);
      uint32_t len = static_cast<uint32_t>(array->length()->Number());
      uint32_t nof_elements =
          IterateArrayAndPrototypeElements(array, visitor);
      // Total elements of array and its prototype chain can be more than
      // the array length, but ArrayConcat can only concatenate at most
      // the array length number of elements. We use the length as an estimate
      // for the actual number of elements added.
      uint32_t added_elements = (nof_elements > len) ? len : nof_elements;
      if (JSArray::kMaxElementCount - visited_elements < added_elements) {
        visited_elements = JSArray::kMaxElementCount;
      } else {
        visited_elements += added_elements;
      }
      if (visitor) visitor->increase_index_offset(len);
    } else {
      if (visitor) {
        visitor->visit(0, obj);
        visitor->increase_index_offset(1);
      }
      if (visited_elements < JSArray::kMaxElementCount) {
        visited_elements++;
      }
    }
  }
  return visited_elements;
}


/**
 * Array::concat implementation.
 * See ECMAScript 262, 15.4.4.4.
 * TODO(lrn): Fix non-compliance for very large concatenations and update to
 * following the ECMAScript 5 specification.
 */
static Object* Runtime_ArrayConcat(Arguments args) {
  ASSERT(args.length() == 1);
  HandleScope handle_scope;

  CONVERT_CHECKED(JSArray, arg_arrays, args[0]);
  Handle<JSArray> arguments(arg_arrays);

  // Pass 1: estimate the number of elements of the result
  // (it could be more than real numbers if prototype has elements).
  uint32_t result_length = 0;
  uint32_t num_of_args = static_cast<uint32_t>(arguments->length()->Number());

  { AssertNoAllocation nogc;
    for (uint32_t i = 0; i < num_of_args; i++) {
      Object* obj = arguments->GetElement(i);
      uint32_t length_estimate;
      if (obj->IsJSArray()) {
        length_estimate =
            static_cast<uint32_t>(JSArray::cast(obj)->length()->Number());
      } else {
        length_estimate = 1;
      }
      if (JSObject::kMaxElementCount - result_length < length_estimate) {
        result_length = JSObject::kMaxElementCount;
        break;
      }
      result_length += length_estimate;
    }
  }

  // Allocate an empty array, will set length and content later.
  Handle<JSArray> result = Factory::NewJSArray(0);

  uint32_t estimate_nof_elements = IterateArguments(arguments, NULL);
  // If estimated number of elements is more than half of length, a
  // fixed array (fast case) is more time and space-efficient than a
  // dictionary.
  bool fast_case = (estimate_nof_elements * 2) >= result_length;

  Handle<FixedArray> storage;
  if (fast_case) {
    // The backing storage array must have non-existing elements to
    // preserve holes across concat operations.
    storage = Factory::NewFixedArrayWithHoles(result_length);
    result->set_map(*Factory::GetFastElementsMap(Handle<Map>(result->map())));
  } else {
    // TODO(126): move 25% pre-allocation logic into Dictionary::Allocate
    uint32_t at_least_space_for = estimate_nof_elements +
                                  (estimate_nof_elements >> 2);
    storage = Handle<FixedArray>::cast(
                  Factory::NewNumberDictionary(at_least_space_for));
    result->set_map(*Factory::GetSlowElementsMap(Handle<Map>(result->map())));
  }

  Handle<Object> len = Factory::NewNumber(static_cast<double>(result_length));

  ArrayConcatVisitor visitor(storage, result_length, fast_case);

  IterateArguments(arguments, &visitor);

  result->set_length(*len);
  // Please note the storage might have changed in the visitor.
  result->set_elements(*visitor.storage());

  return *result;
}


// This will not allocate (flatten the string), but it may run
// very slowly for very deeply nested ConsStrings.  For debugging use only.
static Object* Runtime_GlobalPrint(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);

  CONVERT_CHECKED(String, string, args[0]);
  StringInputBuffer buffer(string);
  while (buffer.has_more()) {
    uint16_t character = buffer.GetNext();
    PrintF("%c", character);
  }
  return string;
}

// Moves all own elements of an object, that are below a limit, to positions
// starting at zero. All undefined values are placed after non-undefined values,
// and are followed by non-existing element. Does not change the length
// property.
// Returns the number of non-undefined elements collected.
static Object* Runtime_RemoveArrayHoles(Arguments args) {
  ASSERT(args.length() == 2);
  CONVERT_CHECKED(JSObject, object, args[0]);
  CONVERT_NUMBER_CHECKED(uint32_t, limit, Uint32, args[1]);
  return object->PrepareElementsForSort(limit);
}


// Move contents of argument 0 (an array) to argument 1 (an array)
static Object* Runtime_MoveArrayContents(Arguments args) {
  ASSERT(args.length() == 2);
  CONVERT_CHECKED(JSArray, from, args[0]);
  CONVERT_CHECKED(JSArray, to, args[1]);
  HeapObject* new_elements = from->elements();
  Object* new_map;
  if (new_elements->map() == Heap::fixed_array_map()) {
    new_map = to->map()->GetFastElementsMap();
  } else {
    new_map = to->map()->GetSlowElementsMap();
  }
  if (new_map->IsFailure()) return new_map;
  to->set_map(Map::cast(new_map));
  to->set_elements(new_elements);
  to->set_length(from->length());
  Object* obj = from->ResetElements();
  if (obj->IsFailure()) return obj;
  from->set_length(Smi::FromInt(0));
  return to;
}


// How many elements does this array have?
static Object* Runtime_EstimateNumberOfElements(Arguments args) {
  ASSERT(args.length() == 1);
  CONVERT_CHECKED(JSArray, array, args[0]);
  HeapObject* elements = array->elements();
  if (elements->IsDictionary()) {
    return Smi::FromInt(NumberDictionary::cast(elements)->NumberOfElements());
  } else {
    return array->length();
  }
}


static Object* Runtime_SwapElements(Arguments args) {
  HandleScope handle_scope;

  ASSERT_EQ(3, args.length());

  CONVERT_ARG_CHECKED(JSObject, object, 0);
  Handle<Object> key1 = args.at<Object>(1);
  Handle<Object> key2 = args.at<Object>(2);

  uint32_t index1, index2;
  if (!key1->ToArrayIndex(&index1)
      || !key2->ToArrayIndex(&index2)) {
    return Top::ThrowIllegalOperation();
  }

  Handle<JSObject> jsobject = Handle<JSObject>::cast(object);
  Handle<Object> tmp1 = GetElement(jsobject, index1);
  Handle<Object> tmp2 = GetElement(jsobject, index2);

  SetElement(jsobject, index1, tmp2);
  SetElement(jsobject, index2, tmp1);

  return Heap::undefined_value();
}


// Returns an array that tells you where in the [0, length) interval an array
// might have elements.  Can either return keys or intervals.  Keys can have
// gaps in (undefined).  Intervals can also span over some undefined keys.
static Object* Runtime_GetArrayKeys(Arguments args) {
  ASSERT(args.length() == 2);
  HandleScope scope;
  CONVERT_ARG_CHECKED(JSObject, array, 0);
  CONVERT_NUMBER_CHECKED(uint32_t, length, Uint32, args[1]);
  if (array->elements()->IsDictionary()) {
    // Create an array and get all the keys into it, then remove all the
    // keys that are not integers in the range 0 to length-1.
    Handle<FixedArray> keys = GetKeysInFixedArrayFor(array, INCLUDE_PROTOS);
    int keys_length = keys->length();
    for (int i = 0; i < keys_length; i++) {
      Object* key = keys->get(i);
      uint32_t index;
      if (!key->ToArrayIndex(&index) || index >= length) {
        // Zap invalid keys.
        keys->set_undefined(i);
      }
    }
    return *Factory::NewJSArrayWithElements(keys);
  } else {
    ASSERT(array->HasFastElements());
    Handle<FixedArray> single_interval = Factory::NewFixedArray(2);
    // -1 means start of array.
    single_interval->set(0, Smi::FromInt(-1));
    uint32_t actual_length =
        static_cast<uint32_t>(FixedArray::cast(array->elements())->length());
    uint32_t min_length = actual_length < length ? actual_length : length;
    Handle<Object> length_object =
        Factory::NewNumber(static_cast<double>(min_length));
    single_interval->set(1, *length_object);
    return *Factory::NewJSArrayWithElements(single_interval);
  }
}


// DefineAccessor takes an optional final argument which is the
// property attributes (eg, DONT_ENUM, DONT_DELETE).  IMPORTANT: due
// to the way accessors are implemented, it is set for both the getter
// and setter on the first call to DefineAccessor and ignored on
// subsequent calls.
static Object* Runtime_DefineAccessor(Arguments args) {
  RUNTIME_ASSERT(args.length() == 4 || args.length() == 5);
  // Compute attributes.
  PropertyAttributes attributes = NONE;
  if (args.length() == 5) {
    CONVERT_CHECKED(Smi, attrs, args[4]);
    int value = attrs->value();
    // Only attribute bits should be set.
    ASSERT((value & ~(READ_ONLY | DONT_ENUM | DONT_DELETE)) == 0);
    attributes = static_cast<PropertyAttributes>(value);
  }

  CONVERT_CHECKED(JSObject, obj, args[0]);
  CONVERT_CHECKED(String, name, args[1]);
  CONVERT_CHECKED(Smi, flag, args[2]);
  CONVERT_CHECKED(JSFunction, fun, args[3]);
  return obj->DefineAccessor(name, flag->value() == 0, fun, attributes);
}


static Object* Runtime_LookupAccessor(Arguments args) {
  ASSERT(args.length() == 3);
  CONVERT_CHECKED(JSObject, obj, args[0]);
  CONVERT_CHECKED(String, name, args[1]);
  CONVERT_CHECKED(Smi, flag, args[2]);
  return obj->LookupAccessor(name, flag->value() == 0);
}


#ifdef ENABLE_DEBUGGER_SUPPORT
static Object* Runtime_DebugBreak(Arguments args) {
  ASSERT(args.length() == 0);
  return Execution::DebugBreakHelper();
}


// Helper functions for wrapping and unwrapping stack frame ids.
static Smi* WrapFrameId(StackFrame::Id id) {
  ASSERT(IsAligned(OffsetFrom(id), static_cast<intptr_t>(4)));
  return Smi::FromInt(id >> 2);
}


static StackFrame::Id UnwrapFrameId(Smi* wrapped) {
  return static_cast<StackFrame::Id>(wrapped->value() << 2);
}


// Adds a JavaScript function as a debug event listener.
// args[0]: debug event listener function to set or null or undefined for
//          clearing the event listener function
// args[1]: object supplied during callback
static Object* Runtime_SetDebugEventListener(Arguments args) {
  ASSERT(args.length() == 2);
  RUNTIME_ASSERT(args[0]->IsJSFunction() ||
                 args[0]->IsUndefined() ||
                 args[0]->IsNull());
  Handle<Object> callback = args.at<Object>(0);
  Handle<Object> data = args.at<Object>(1);
  Debugger::SetEventListener(callback, data);

  return Heap::undefined_value();
}


static Object* Runtime_Break(Arguments args) {
  ASSERT(args.length() == 0);
  StackGuard::DebugBreak();
  return Heap::undefined_value();
}


static Object* DebugLookupResultValue(Object* receiver, String* name,
                                      LookupResult* result,
                                      bool* caught_exception) {
  Object* value;
  switch (result->type()) {
    case NORMAL:
      value = result->holder()->GetNormalizedProperty(result);
      if (value->IsTheHole()) {
        return Heap::undefined_value();
      }
      return value;
    case FIELD:
      value =
          JSObject::cast(
              result->holder())->FastPropertyAt(result->GetFieldIndex());
      if (value->IsTheHole()) {
        return Heap::undefined_value();
      }
      return value;
    case CONSTANT_FUNCTION:
      return result->GetConstantFunction();
    case CALLBACKS: {
      Object* structure = result->GetCallbackObject();
      if (structure->IsProxy() || structure->IsAccessorInfo()) {
        value = receiver->GetPropertyWithCallback(
            receiver, structure, name, result->holder());
        if (value->IsException()) {
          value = Top::pending_exception();
          Top::clear_pending_exception();
          if (caught_exception != NULL) {
            *caught_exception = true;
          }
        }
        return value;
      } else {
        return Heap::undefined_value();
      }
    }
    case INTERCEPTOR:
    case MAP_TRANSITION:
    case CONSTANT_TRANSITION:
    case NULL_DESCRIPTOR:
      return Heap::undefined_value();
    default:
      UNREACHABLE();
  }
  UNREACHABLE();
  return Heap::undefined_value();
}


// Get debugger related details for an object property.
// args[0]: object holding property
// args[1]: name of the property
//
// The array returned contains the following information:
// 0: Property value
// 1: Property details
// 2: Property value is exception
// 3: Getter function if defined
// 4: Setter function if defined
// Items 2-4 are only filled if the property has either a getter or a setter
// defined through __defineGetter__ and/or __defineSetter__.
static Object* Runtime_DebugGetPropertyDetails(Arguments args) {
  HandleScope scope;

  ASSERT(args.length() == 2);

  CONVERT_ARG_CHECKED(JSObject, obj, 0);
  CONVERT_ARG_CHECKED(String, name, 1);

  // Make sure to set the current context to the context before the debugger was
  // entered (if the debugger is entered). The reason for switching context here
  // is that for some property lookups (accessors and interceptors) callbacks
  // into the embedding application can occour, and the embedding application
  // could have the assumption that its own global context is the current
  // context and not some internal debugger context.
  SaveContext save;
  if (Debug::InDebugger()) {
    Top::set_context(*Debug::debugger_entry()->GetContext());
  }

  // Skip the global proxy as it has no properties and always delegates to the
  // real global object.
  if (obj->IsJSGlobalProxy()) {
    obj = Handle<JSObject>(JSObject::cast(obj->GetPrototype()));
  }


  // Check if the name is trivially convertible to an index and get the element
  // if so.
  uint32_t index;
  if (name->AsArrayIndex(&index)) {
    Handle<FixedArray> details = Factory::NewFixedArray(2);
    details->set(0, Runtime::GetElementOrCharAt(obj, index));
    details->set(1, PropertyDetails(NONE, NORMAL).AsSmi());
    return *Factory::NewJSArrayWithElements(details);
  }

  // Find the number of objects making up this.
  int length = LocalPrototypeChainLength(*obj);

  // Try local lookup on each of the objects.
  Handle<JSObject> jsproto = obj;
  for (int i = 0; i < length; i++) {
    LookupResult result;
    jsproto->LocalLookup(*name, &result);
    if (result.IsProperty()) {
      // LookupResult is not GC safe as it holds raw object pointers.
      // GC can happen later in this code so put the required fields into
      // local variables using handles when required for later use.
      PropertyType result_type = result.type();
      Handle<Object> result_callback_obj;
      if (result_type == CALLBACKS) {
        result_callback_obj = Handle<Object>(result.GetCallbackObject());
      }
      Smi* property_details = result.GetPropertyDetails().AsSmi();
      // DebugLookupResultValue can cause GC so details from LookupResult needs
      // to be copied to handles before this.
      bool caught_exception = false;
      Object* raw_value = DebugLookupResultValue(*obj, *name, &result,
                                                 &caught_exception);
      if (raw_value->IsFailure()) return raw_value;
      Handle<Object> value(raw_value);

      // If the callback object is a fixed array then it contains JavaScript
      // getter and/or setter.
      bool hasJavaScriptAccessors = result_type == CALLBACKS &&
                                    result_callback_obj->IsFixedArray();
      Handle<FixedArray> details =
          Factory::NewFixedArray(hasJavaScriptAccessors ? 5 : 2);
      details->set(0, *value);
      details->set(1, property_details);
      if (hasJavaScriptAccessors) {
        details->set(2,
                     caught_exception ? Heap::true_value()
                                      : Heap::false_value());
        details->set(3, FixedArray::cast(*result_callback_obj)->get(0));
        details->set(4, FixedArray::cast(*result_callback_obj)->get(1));
      }

      return *Factory::NewJSArrayWithElements(details);
    }
    if (i < length - 1) {
      jsproto = Handle<JSObject>(JSObject::cast(jsproto->GetPrototype()));
    }
  }

  return Heap::undefined_value();
}


static Object* Runtime_DebugGetProperty(Arguments args) {
  HandleScope scope;

  ASSERT(args.length() == 2);

  CONVERT_ARG_CHECKED(JSObject, obj, 0);
  CONVERT_ARG_CHECKED(String, name, 1);

  LookupResult result;
  obj->Lookup(*name, &result);
  if (result.IsProperty()) {
    return DebugLookupResultValue(*obj, *name, &result, NULL);
  }
  return Heap::undefined_value();
}


// Return the property type calculated from the property details.
// args[0]: smi with property details.
static Object* Runtime_DebugPropertyTypeFromDetails(Arguments args) {
  ASSERT(args.length() == 1);
  CONVERT_CHECKED(Smi, details, args[0]);
  PropertyType type = PropertyDetails(details).type();
  return Smi::FromInt(static_cast<int>(type));
}


// Return the property attribute calculated from the property details.
// args[0]: smi with property details.
static Object* Runtime_DebugPropertyAttributesFromDetails(Arguments args) {
  ASSERT(args.length() == 1);
  CONVERT_CHECKED(Smi, details, args[0]);
  PropertyAttributes attributes = PropertyDetails(details).attributes();
  return Smi::FromInt(static_cast<int>(attributes));
}


// Return the property insertion index calculated from the property details.
// args[0]: smi with property details.
static Object* Runtime_DebugPropertyIndexFromDetails(Arguments args) {
  ASSERT(args.length() == 1);
  CONVERT_CHECKED(Smi, details, args[0]);
  int index = PropertyDetails(details).index();
  return Smi::FromInt(index);
}


// Return property value from named interceptor.
// args[0]: object
// args[1]: property name
static Object* Runtime_DebugNamedInterceptorPropertyValue(Arguments args) {
  HandleScope scope;
  ASSERT(args.length() == 2);
  CONVERT_ARG_CHECKED(JSObject, obj, 0);
  RUNTIME_ASSERT(obj->HasNamedInterceptor());
  CONVERT_ARG_CHECKED(String, name, 1);

  PropertyAttributes attributes;
  return obj->GetPropertyWithInterceptor(*obj, *name, &attributes);
}


// Return element value from indexed interceptor.
// args[0]: object
// args[1]: index
static Object* Runtime_DebugIndexedInterceptorElementValue(Arguments args) {
  HandleScope scope;
  ASSERT(args.length() == 2);
  CONVERT_ARG_CHECKED(JSObject, obj, 0);
  RUNTIME_ASSERT(obj->HasIndexedInterceptor());
  CONVERT_NUMBER_CHECKED(uint32_t, index, Uint32, args[1]);

  return obj->GetElementWithInterceptor(*obj, index);
}


static Object* Runtime_CheckExecutionState(Arguments args) {
  ASSERT(args.length() >= 1);
  CONVERT_NUMBER_CHECKED(int, break_id, Int32, args[0]);
  // Check that the break id is valid.
  if (Debug::break_id() == 0 || break_id != Debug::break_id()) {
    return Top::Throw(Heap::illegal_execution_state_symbol());
  }

  return Heap::true_value();
}


static Object* Runtime_GetFrameCount(Arguments args) {
  HandleScope scope;
  ASSERT(args.length() == 1);

  // Check arguments.
  Object* result = Runtime_CheckExecutionState(args);
  if (result->IsFailure()) return result;

  // Count all frames which are relevant to debugging stack trace.
  int n = 0;
  StackFrame::Id id = Debug::break_frame_id();
  if (id == StackFrame::NO_ID) {
    // If there is no JavaScript stack frame count is 0.
    return Smi::FromInt(0);
  }
  for (JavaScriptFrameIterator it(id); !it.done(); it.Advance()) n++;
  return Smi::FromInt(n);
}


static const int kFrameDetailsFrameIdIndex = 0;
static const int kFrameDetailsReceiverIndex = 1;
static const int kFrameDetailsFunctionIndex = 2;
static const int kFrameDetailsArgumentCountIndex = 3;
static const int kFrameDetailsLocalCountIndex = 4;
static const int kFrameDetailsSourcePositionIndex = 5;
static const int kFrameDetailsConstructCallIndex = 6;
static const int kFrameDetailsAtReturnIndex = 7;
static const int kFrameDetailsDebuggerFrameIndex = 8;
static const int kFrameDetailsFirstDynamicIndex = 9;

// Return an array with frame details
// args[0]: number: break id
// args[1]: number: frame index
//
// The array returned contains the following information:
// 0: Frame id
// 1: Receiver
// 2: Function
// 3: Argument count
// 4: Local count
// 5: Source position
// 6: Constructor call
// 7: Is at return
// 8: Debugger frame
// Arguments name, value
// Locals name, value
// Return value if any
static Object* Runtime_GetFrameDetails(Arguments args) {
  HandleScope scope;
  ASSERT(args.length() == 2);

  // Check arguments.
  Object* check = Runtime_CheckExecutionState(args);
  if (check->IsFailure()) return check;
  CONVERT_NUMBER_CHECKED(int, index, Int32, args[1]);

  // Find the relevant frame with the requested index.
  StackFrame::Id id = Debug::break_frame_id();
  if (id == StackFrame::NO_ID) {
    // If there are no JavaScript stack frames return undefined.
    return Heap::undefined_value();
  }
  int count = 0;
  JavaScriptFrameIterator it(id);
  for (; !it.done(); it.Advance()) {
    if (count == index) break;
    count++;
  }
  if (it.done()) return Heap::undefined_value();

  // Traverse the saved contexts chain to find the active context for the
  // selected frame.
  SaveContext* save = Top::save_context();
  while (save != NULL && !save->below(it.frame())) {
    save = save->prev();
  }
  ASSERT(save != NULL);

  // Get the frame id.
  Handle<Object> frame_id(WrapFrameId(it.frame()->id()));

  // Find source position.
  int position = it.frame()->code()->SourcePosition(it.frame()->pc());

  // Check for constructor frame.
  bool constructor = it.frame()->IsConstructor();

  // Get scope info and read from it for local variable information.
  Handle<JSFunction> function(JSFunction::cast(it.frame()->function()));
  Handle<SerializedScopeInfo> scope_info(function->shared()->scope_info());
  ScopeInfo<> info(*scope_info);

  // Get the context.
  Handle<Context> context(Context::cast(it.frame()->context()));

  // Get the locals names and values into a temporary array.
  //
  // TODO(1240907): Hide compiler-introduced stack variables
  // (e.g. .result)?  For users of the debugger, they will probably be
  // confusing.
  Handle<FixedArray> locals = Factory::NewFixedArray(info.NumberOfLocals() * 2);
  for (int i = 0; i < info.NumberOfLocals(); i++) {
    // Name of the local.
    locals->set(i * 2, *info.LocalName(i));

    // Fetch the value of the local - either from the stack or from a
    // heap-allocated context.
    if (i < info.number_of_stack_slots()) {
      locals->set(i * 2 + 1, it.frame()->GetExpression(i));
    } else {
      Handle<String> name = info.LocalName(i);
      // Traverse the context chain to the function context as all local
      // variables stored in the context will be on the function context.
      while (!context->is_function_context()) {
        context = Handle<Context>(context->previous());
      }
      ASSERT(context->is_function_context());
      locals->set(i * 2 + 1,
                  context->get(scope_info->ContextSlotIndex(*name, NULL)));
    }
  }

  // Check whether this frame is positioned at return.
  int at_return = (index == 0) ? Debug::IsBreakAtReturn(it.frame()) : false;

  // If positioned just before return find the value to be returned and add it
  // to the frame information.
  Handle<Object> return_value = Factory::undefined_value();
  if (at_return) {
    StackFrameIterator it2;
    Address internal_frame_sp = NULL;
    while (!it2.done()) {
      if (it2.frame()->is_internal()) {
        internal_frame_sp = it2.frame()->sp();
      } else {
        if (it2.frame()->is_java_script()) {
          if (it2.frame()->id() == it.frame()->id()) {
            // The internal frame just before the JavaScript frame contains the
            // value to return on top. A debug break at return will create an
            // internal frame to store the return value (eax/rax/r0) before
            // entering the debug break exit frame.
            if (internal_frame_sp != NULL) {
              return_value =
                  Handle<Object>(Memory::Object_at(internal_frame_sp));
              break;
            }
          }
        }

        // Indicate that the previous frame was not an internal frame.
        internal_frame_sp = NULL;
      }
      it2.Advance();
    }
  }

  // Now advance to the arguments adapter frame (if any). It contains all
  // the provided parameters whereas the function frame always have the number
  // of arguments matching the functions parameters. The rest of the
  // information (except for what is collected above) is the same.
  it.AdvanceToArgumentsFrame();

  // Find the number of arguments to fill. At least fill the number of
  // parameters for the function and fill more if more parameters are provided.
  int argument_count = info.number_of_parameters();
  if (argument_count < it.frame()->GetProvidedParametersCount()) {
    argument_count = it.frame()->GetProvidedParametersCount();
  }

  // Calculate the size of the result.
  int details_size = kFrameDetailsFirstDynamicIndex +
                     2 * (argument_count + info.NumberOfLocals()) +
                     (at_return ? 1 : 0);
  Handle<FixedArray> details = Factory::NewFixedArray(details_size);

  // Add the frame id.
  details->set(kFrameDetailsFrameIdIndex, *frame_id);

  // Add the function (same as in function frame).
  details->set(kFrameDetailsFunctionIndex, it.frame()->function());

  // Add the arguments count.
  details->set(kFrameDetailsArgumentCountIndex, Smi::FromInt(argument_count));

  // Add the locals count
  details->set(kFrameDetailsLocalCountIndex,
               Smi::FromInt(info.NumberOfLocals()));

  // Add the source position.
  if (position != RelocInfo::kNoPosition) {
    details->set(kFrameDetailsSourcePositionIndex, Smi::FromInt(position));
  } else {
    details->set(kFrameDetailsSourcePositionIndex, Heap::undefined_value());
  }

  // Add the constructor information.
  details->set(kFrameDetailsConstructCallIndex, Heap::ToBoolean(constructor));

  // Add the at return information.
  details->set(kFrameDetailsAtReturnIndex, Heap::ToBoolean(at_return));

  // Add information on whether this frame is invoked in the debugger context.
  details->set(kFrameDetailsDebuggerFrameIndex,
               Heap::ToBoolean(*save->context() == *Debug::debug_context()));

  // Fill the dynamic part.
  int details_index = kFrameDetailsFirstDynamicIndex;

  // Add arguments name and value.
  for (int i = 0; i < argument_count; i++) {
    // Name of the argument.
    if (i < info.number_of_parameters()) {
      details->set(details_index++, *info.parameter_name(i));
    } else {
      details->set(details_index++, Heap::undefined_value());
    }

    // Parameter value.
    if (i < it.frame()->GetProvidedParametersCount()) {
      details->set(details_index++, it.frame()->GetParameter(i));
    } else {
      details->set(details_index++, Heap::undefined_value());
    }
  }

  // Add locals name and value from the temporary copy from the function frame.
  for (int i = 0; i < info.NumberOfLocals() * 2; i++) {
    details->set(details_index++, locals->get(i));
  }

  // Add the value being returned.
  if (at_return) {
    details->set(details_index++, *return_value);
  }

  // Add the receiver (same as in function frame).
  // THIS MUST BE DONE LAST SINCE WE MIGHT ADVANCE
  // THE FRAME ITERATOR TO WRAP THE RECEIVER.
  Handle<Object> receiver(it.frame()->receiver());
  if (!receiver->IsJSObject()) {
    // If the receiver is NOT a JSObject we have hit an optimization
    // where a value object is not converted into a wrapped JS objects.
    // To hide this optimization from the debugger, we wrap the receiver
    // by creating correct wrapper object based on the calling frame's
    // global context.
    it.Advance();
    Handle<Context> calling_frames_global_context(
        Context::cast(Context::cast(it.frame()->context())->global_context()));
    receiver = Factory::ToObject(receiver, calling_frames_global_context);
  }
  details->set(kFrameDetailsReceiverIndex, *receiver);

  ASSERT_EQ(details_size, details_index);
  return *Factory::NewJSArrayWithElements(details);
}


// Copy all the context locals into an object used to materialize a scope.
static void CopyContextLocalsToScopeObject(
    Handle<SerializedScopeInfo> serialized_scope_info,
    ScopeInfo<>& scope_info,
    Handle<Context> context,
    Handle<JSObject> scope_object) {
  // Fill all context locals to the context extension.
  for (int i = Context::MIN_CONTEXT_SLOTS;
       i < scope_info.number_of_context_slots();
       i++) {
    int context_index = serialized_scope_info->ContextSlotIndex(
        *scope_info.context_slot_name(i), NULL);

    // Don't include the arguments shadow (.arguments) context variable.
    if (*scope_info.context_slot_name(i) != Heap::arguments_shadow_symbol()) {
      SetProperty(scope_object,
                  scope_info.context_slot_name(i),
                  Handle<Object>(context->get(context_index)), NONE);
    }
  }
}


// Create a plain JSObject which materializes the local scope for the specified
// frame.
static Handle<JSObject> MaterializeLocalScope(JavaScriptFrame* frame) {
  Handle<JSFunction> function(JSFunction::cast(frame->function()));
  Handle<SharedFunctionInfo> shared(function->shared());
  Handle<SerializedScopeInfo> serialized_scope_info(shared->scope_info());
  ScopeInfo<> scope_info(*serialized_scope_info);

  // Allocate and initialize a JSObject with all the arguments, stack locals
  // heap locals and extension properties of the debugged function.
  Handle<JSObject> local_scope = Factory::NewJSObject(Top::object_function());

  // First fill all parameters.
  for (int i = 0; i < scope_info.number_of_parameters(); ++i) {
    SetProperty(local_scope,
                scope_info.parameter_name(i),
                Handle<Object>(frame->GetParameter(i)), NONE);
  }

  // Second fill all stack locals.
  for (int i = 0; i < scope_info.number_of_stack_slots(); i++) {
    SetProperty(local_scope,
                scope_info.stack_slot_name(i),
                Handle<Object>(frame->GetExpression(i)), NONE);
  }

  // Third fill all context locals.
  Handle<Context> frame_context(Context::cast(frame->context()));
  Handle<Context> function_context(frame_context->fcontext());
  CopyContextLocalsToScopeObject(serialized_scope_info, scope_info,
                                 function_context, local_scope);

  // Finally copy any properties from the function context extension. This will
  // be variables introduced by eval.
  if (function_context->closure() == *function) {
    if (function_context->has_extension() &&
        !function_context->IsGlobalContext()) {
      Handle<JSObject> ext(JSObject::cast(function_context->extension()));
      Handle<FixedArray> keys = GetKeysInFixedArrayFor(ext, INCLUDE_PROTOS);
      for (int i = 0; i < keys->length(); i++) {
        // Names of variables introduced by eval are strings.
        ASSERT(keys->get(i)->IsString());
        Handle<String> key(String::cast(keys->get(i)));
        SetProperty(local_scope, key, GetProperty(ext, key), NONE);
      }
    }
  }
  return local_scope;
}


// Create a plain JSObject which materializes the closure content for the
// context.
static Handle<JSObject> MaterializeClosure(Handle<Context> context) {
  ASSERT(context->is_function_context());

  Handle<SharedFunctionInfo> shared(context->closure()->shared());
  Handle<SerializedScopeInfo> serialized_scope_info(shared->scope_info());
  ScopeInfo<> scope_info(*serialized_scope_info);

  // Allocate and initialize a JSObject with all the content of theis function
  // closure.
  Handle<JSObject> closure_scope = Factory::NewJSObject(Top::object_function());

  // Check whether the arguments shadow object exists.
  int arguments_shadow_index =
      shared->scope_info()->ContextSlotIndex(Heap::arguments_shadow_symbol(),
                                             NULL);
  if (arguments_shadow_index >= 0) {
    // In this case all the arguments are available in the arguments shadow
    // object.
    Handle<JSObject> arguments_shadow(
        JSObject::cast(context->get(arguments_shadow_index)));
    for (int i = 0; i < scope_info.number_of_parameters(); ++i) {
      SetProperty(closure_scope,
                  scope_info.parameter_name(i),
                  Handle<Object>(arguments_shadow->GetElement(i)), NONE);
    }
  }

  // Fill all context locals to the context extension.
  CopyContextLocalsToScopeObject(serialized_scope_info, scope_info,
                                 context, closure_scope);

  // Finally copy any properties from the function context extension. This will
  // be variables introduced by eval.
  if (context->has_extension()) {
    Handle<JSObject> ext(JSObject::cast(context->extension()));
    Handle<FixedArray> keys = GetKeysInFixedArrayFor(ext, INCLUDE_PROTOS);
    for (int i = 0; i < keys->length(); i++) {
      // Names of variables introduced by eval are strings.
      ASSERT(keys->get(i)->IsString());
      Handle<String> key(String::cast(keys->get(i)));
      SetProperty(closure_scope, key, GetProperty(ext, key), NONE);
    }
  }

  return closure_scope;
}


// Iterate over the actual scopes visible from a stack frame. All scopes are
// backed by an actual context except the local scope, which is inserted
// "artifically" in the context chain.
class ScopeIterator {
 public:
  enum ScopeType {
    ScopeTypeGlobal = 0,
    ScopeTypeLocal,
    ScopeTypeWith,
    ScopeTypeClosure,
    // Every catch block contains an implicit with block (its parameter is
    // a JSContextExtensionObject) that extends current scope with a variable
    // holding exception object. Such with blocks are treated as scopes of their
    // own type.
    ScopeTypeCatch
  };

  explicit ScopeIterator(JavaScriptFrame* frame)
    : frame_(frame),
      function_(JSFunction::cast(frame->function())),
      context_(Context::cast(frame->context())),
      local_done_(false),
      at_local_(false) {

    // Check whether the first scope is actually a local scope.
    if (context_->IsGlobalContext()) {
      // If there is a stack slot for .result then this local scope has been
      // created for evaluating top level code and it is not a real local scope.
      // Checking for the existence of .result seems fragile, but the scope info
      // saved with the code object does not otherwise have that information.
      int index = function_->shared()->scope_info()->
          StackSlotIndex(Heap::result_symbol());
      at_local_ = index < 0;
    } else if (context_->is_function_context()) {
      at_local_ = true;
    }
  }

  // More scopes?
  bool Done() { return context_.is_null(); }

  // Move to the next scope.
  void Next() {
    // If at a local scope mark the local scope as passed.
    if (at_local_) {
      at_local_ = false;
      local_done_ = true;

      // If the current context is not associated with the local scope the
      // current context is the next real scope, so don't move to the next
      // context in this case.
      if (context_->closure() != *function_) {
        return;
      }
    }

    // The global scope is always the last in the chain.
    if (context_->IsGlobalContext()) {
      context_ = Handle<Context>();
      return;
    }

    // Move to the next context.
    if (context_->is_function_context()) {
      context_ = Handle<Context>(Context::cast(context_->closure()->context()));
    } else {
      context_ = Handle<Context>(context_->previous());
    }

    // If passing the local scope indicate that the current scope is now the
    // local scope.
    if (!local_done_ &&
        (context_->IsGlobalContext() || (context_->is_function_context()))) {
      at_local_ = true;
    }
  }

  // Return the type of the current scope.
  int Type() {
    if (at_local_) {
      return ScopeTypeLocal;
    }
    if (context_->IsGlobalContext()) {
      ASSERT(context_->global()->IsGlobalObject());
      return ScopeTypeGlobal;
    }
    if (context_->is_function_context()) {
      return ScopeTypeClosure;
    }
    ASSERT(context_->has_extension());
    // Current scope is either an explicit with statement or a with statement
    // implicitely generated for a catch block.
    // If the extension object here is a JSContextExtensionObject then
    // current with statement is one frome a catch block otherwise it's a
    // regular with statement.
    if (context_->extension()->IsJSContextExtensionObject()) {
      return ScopeTypeCatch;
    }
    return ScopeTypeWith;
  }

  // Return the JavaScript object with the content of the current scope.
  Handle<JSObject> ScopeObject() {
    switch (Type()) {
      case ScopeIterator::ScopeTypeGlobal:
        return Handle<JSObject>(CurrentContext()->global());
        break;
      case ScopeIterator::ScopeTypeLocal:
        // Materialize the content of the local scope into a JSObject.
        return MaterializeLocalScope(frame_);
        break;
      case ScopeIterator::ScopeTypeWith:
      case ScopeIterator::ScopeTypeCatch:
        // Return the with object.
        return Handle<JSObject>(CurrentContext()->extension());
        break;
      case ScopeIterator::ScopeTypeClosure:
        // Materialize the content of the closure scope into a JSObject.
        return MaterializeClosure(CurrentContext());
        break;
    }
    UNREACHABLE();
    return Handle<JSObject>();
  }

  // Return the context for this scope. For the local context there might not
  // be an actual context.
  Handle<Context> CurrentContext() {
    if (at_local_ && context_->closure() != *function_) {
      return Handle<Context>();
    }
    return context_;
  }

#ifdef DEBUG
  // Debug print of the content of the current scope.
  void DebugPrint() {
    switch (Type()) {
      case ScopeIterator::ScopeTypeGlobal:
        PrintF("Global:\n");
        CurrentContext()->Print();
        break;

      case ScopeIterator::ScopeTypeLocal: {
        PrintF("Local:\n");
        ScopeInfo<> scope_info(function_->shared()->scope_info());
        scope_info.Print();
        if (!CurrentContext().is_null()) {
          CurrentContext()->Print();
          if (CurrentContext()->has_extension()) {
            Handle<JSObject> extension =
                Handle<JSObject>(CurrentContext()->extension());
            if (extension->IsJSContextExtensionObject()) {
              extension->Print();
            }
          }
        }
        break;
      }

      case ScopeIterator::ScopeTypeWith: {
        PrintF("With:\n");
        Handle<JSObject> extension =
            Handle<JSObject>(CurrentContext()->extension());
        extension->Print();
        break;
      }

      case ScopeIterator::ScopeTypeCatch: {
        PrintF("Catch:\n");
        Handle<JSObject> extension =
            Handle<JSObject>(CurrentContext()->extension());
        extension->Print();
        break;
      }

      case ScopeIterator::ScopeTypeClosure: {
        PrintF("Closure:\n");
        CurrentContext()->Print();
        if (CurrentContext()->has_extension()) {
          Handle<JSObject> extension =
              Handle<JSObject>(CurrentContext()->extension());
          if (extension->IsJSContextExtensionObject()) {
            extension->Print();
          }
        }
        break;
      }

      default:
        UNREACHABLE();
    }
    PrintF("\n");
  }
#endif

 private:
  JavaScriptFrame* frame_;
  Handle<JSFunction> function_;
  Handle<Context> context_;
  bool local_done_;
  bool at_local_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ScopeIterator);
};


static Object* Runtime_GetScopeCount(Arguments args) {
  HandleScope scope;
  ASSERT(args.length() == 2);

  // Check arguments.
  Object* check = Runtime_CheckExecutionState(args);
  if (check->IsFailure()) return check;
  CONVERT_CHECKED(Smi, wrapped_id, args[1]);

  // Get the frame where the debugging is performed.
  StackFrame::Id id = UnwrapFrameId(wrapped_id);
  JavaScriptFrameIterator it(id);
  JavaScriptFrame* frame = it.frame();

  // Count the visible scopes.
  int n = 0;
  for (ScopeIterator it(frame); !it.Done(); it.Next()) {
    n++;
  }

  return Smi::FromInt(n);
}


static const int kScopeDetailsTypeIndex = 0;
static const int kScopeDetailsObjectIndex = 1;
static const int kScopeDetailsSize = 2;

// Return an array with scope details
// args[0]: number: break id
// args[1]: number: frame index
// args[2]: number: scope index
//
// The array returned contains the following information:
// 0: Scope type
// 1: Scope object
static Object* Runtime_GetScopeDetails(Arguments args) {
  HandleScope scope;
  ASSERT(args.length() == 3);

  // Check arguments.
  Object* check = Runtime_CheckExecutionState(args);
  if (check->IsFailure()) return check;
  CONVERT_CHECKED(Smi, wrapped_id, args[1]);
  CONVERT_NUMBER_CHECKED(int, index, Int32, args[2]);

  // Get the frame where the debugging is performed.
  StackFrame::Id id = UnwrapFrameId(wrapped_id);
  JavaScriptFrameIterator frame_it(id);
  JavaScriptFrame* frame = frame_it.frame();

  // Find the requested scope.
  int n = 0;
  ScopeIterator it(frame);
  for (; !it.Done() && n < index; it.Next()) {
    n++;
  }
  if (it.Done()) {
    return Heap::undefined_value();
  }

  // Calculate the size of the result.
  int details_size = kScopeDetailsSize;
  Handle<FixedArray> details = Factory::NewFixedArray(details_size);

  // Fill in scope details.
  details->set(kScopeDetailsTypeIndex, Smi::FromInt(it.Type()));
  details->set(kScopeDetailsObjectIndex, *it.ScopeObject());

  return *Factory::NewJSArrayWithElements(details);
}


static Object* Runtime_DebugPrintScopes(Arguments args) {
  HandleScope scope;
  ASSERT(args.length() == 0);

#ifdef DEBUG
  // Print the scopes for the top frame.
  StackFrameLocator locator;
  JavaScriptFrame* frame = locator.FindJavaScriptFrame(0);
  for (ScopeIterator it(frame); !it.Done(); it.Next()) {
    it.DebugPrint();
  }
#endif
  return Heap::undefined_value();
}


static Object* Runtime_GetCFrames(Arguments args) {
  HandleScope scope;
  ASSERT(args.length() == 1);
  Object* result = Runtime_CheckExecutionState(args);
  if (result->IsFailure()) return result;

#if V8_HOST_ARCH_64_BIT
  UNIMPLEMENTED();
  return Heap::undefined_value();
#else

  static const int kMaxCFramesSize = 200;
  ScopedVector<OS::StackFrame> frames(kMaxCFramesSize);
  int frames_count = OS::StackWalk(frames);
  if (frames_count == OS::kStackWalkError) {
    return Heap::undefined_value();
  }

  Handle<String> address_str = Factory::LookupAsciiSymbol("address");
  Handle<String> text_str = Factory::LookupAsciiSymbol("text");
  Handle<FixedArray> frames_array = Factory::NewFixedArray(frames_count);
  for (int i = 0; i < frames_count; i++) {
    Handle<JSObject> frame_value = Factory::NewJSObject(Top::object_function());
    frame_value->SetProperty(
        *address_str,
        *Factory::NewNumberFromInt(reinterpret_cast<int>(frames[i].address)),
        NONE);

    // Get the stack walk text for this frame.
    Handle<String> frame_text;
    int frame_text_length = StrLength(frames[i].text);
    if (frame_text_length > 0) {
      Vector<const char> str(frames[i].text, frame_text_length);
      frame_text = Factory::NewStringFromAscii(str);
    }

    if (!frame_text.is_null()) {
      frame_value->SetProperty(*text_str, *frame_text, NONE);
    }

    frames_array->set(i, *frame_value);
  }
  return *Factory::NewJSArrayWithElements(frames_array);
#endif  // V8_HOST_ARCH_64_BIT
}


static Object* Runtime_GetThreadCount(Arguments args) {
  HandleScope scope;
  ASSERT(args.length() == 1);

  // Check arguments.
  Object* result = Runtime_CheckExecutionState(args);
  if (result->IsFailure()) return result;

  // Count all archived V8 threads.
  int n = 0;
  for (ThreadState* thread = ThreadState::FirstInUse();
       thread != NULL;
       thread = thread->Next()) {
    n++;
  }

  // Total number of threads is current thread and archived threads.
  return Smi::FromInt(n + 1);
}


static const int kThreadDetailsCurrentThreadIndex = 0;
static const int kThreadDetailsThreadIdIndex = 1;
static const int kThreadDetailsSize = 2;

// Return an array with thread details
// args[0]: number: break id
// args[1]: number: thread index
//
// The array returned contains the following information:
// 0: Is current thread?
// 1: Thread id
static Object* Runtime_GetThreadDetails(Arguments args) {
  HandleScope scope;
  ASSERT(args.length() == 2);

  // Check arguments.
  Object* check = Runtime_CheckExecutionState(args);
  if (check->IsFailure()) return check;
  CONVERT_NUMBER_CHECKED(int, index, Int32, args[1]);

  // Allocate array for result.
  Handle<FixedArray> details = Factory::NewFixedArray(kThreadDetailsSize);

  // Thread index 0 is current thread.
  if (index == 0) {
    // Fill the details.
    details->set(kThreadDetailsCurrentThreadIndex, Heap::true_value());
    details->set(kThreadDetailsThreadIdIndex,
                 Smi::FromInt(ThreadManager::CurrentId()));
  } else {
    // Find the thread with the requested index.
    int n = 1;
    ThreadState* thread = ThreadState::FirstInUse();
    while (index != n && thread != NULL) {
      thread = thread->Next();
      n++;
    }
    if (thread == NULL) {
      return Heap::undefined_value();
    }

    // Fill the details.
    details->set(kThreadDetailsCurrentThreadIndex, Heap::false_value());
    details->set(kThreadDetailsThreadIdIndex, Smi::FromInt(thread->id()));
  }

  // Convert to JS array and return.
  return *Factory::NewJSArrayWithElements(details);
}


// Sets the disable break state
// args[0]: disable break state
static Object* Runtime_SetDisableBreak(Arguments args) {
  HandleScope scope;
  ASSERT(args.length() == 1);
  CONVERT_BOOLEAN_CHECKED(disable_break, args[0]);
  Debug::set_disable_break(disable_break);
  return  Heap::undefined_value();
}


static Object* Runtime_GetBreakLocations(Arguments args) {
  HandleScope scope;
  ASSERT(args.length() == 1);

  CONVERT_ARG_CHECKED(JSFunction, fun, 0);
  Handle<SharedFunctionInfo> shared(fun->shared());
  // Find the number of break points
  Handle<Object> break_locations = Debug::GetSourceBreakLocations(shared);
  if (break_locations->IsUndefined()) return Heap::undefined_value();
  // Return array as JS array
  return *Factory::NewJSArrayWithElements(
      Handle<FixedArray>::cast(break_locations));
}


// Set a break point in a function
// args[0]: function
// args[1]: number: break source position (within the function source)
// args[2]: number: break point object
static Object* Runtime_SetFunctionBreakPoint(Arguments args) {
  HandleScope scope;
  ASSERT(args.length() == 3);
  CONVERT_ARG_CHECKED(JSFunction, fun, 0);
  Handle<SharedFunctionInfo> shared(fun->shared());
  CONVERT_NUMBER_CHECKED(int32_t, source_position, Int32, args[1]);
  RUNTIME_ASSERT(source_position >= 0);
  Handle<Object> break_point_object_arg = args.at<Object>(2);

  // Set break point.
  Debug::SetBreakPoint(shared, break_point_object_arg, &source_position);

  return Smi::FromInt(source_position);
}


Object* Runtime::FindSharedFunctionInfoInScript(Handle<Script> script,
                                                int position) {
  // Iterate the heap looking for SharedFunctionInfo generated from the
  // script. The inner most SharedFunctionInfo containing the source position
  // for the requested break point is found.
  // NOTE: This might reqire several heap iterations. If the SharedFunctionInfo
  // which is found is not compiled it is compiled and the heap is iterated
  // again as the compilation might create inner functions from the newly
  // compiled function and the actual requested break point might be in one of
  // these functions.
  bool done = false;
  // The current candidate for the source position:
  int target_start_position = RelocInfo::kNoPosition;
  Handle<SharedFunctionInfo> target;
  while (!done) {
    HeapIterator iterator;
    for (HeapObject* obj = iterator.next();
         obj != NULL; obj = iterator.next()) {
      if (obj->IsSharedFunctionInfo()) {
        Handle<SharedFunctionInfo> shared(SharedFunctionInfo::cast(obj));
        if (shared->script() == *script) {
          // If the SharedFunctionInfo found has the requested script data and
          // contains the source position it is a candidate.
          int start_position = shared->function_token_position();
          if (start_position == RelocInfo::kNoPosition) {
            start_position = shared->start_position();
          }
          if (start_position <= position &&
              position <= shared->end_position()) {
            // If there is no candidate or this function is within the current
            // candidate this is the new candidate.
            if (target.is_null()) {
              target_start_position = start_position;
              target = shared;
            } else {
              if (target_start_position == start_position &&
                  shared->end_position() == target->end_position()) {
                  // If a top-level function contain only one function
                  // declartion the source for the top-level and the function is
                  // the same. In that case prefer the non top-level function.
                if (!shared->is_toplevel()) {
                  target_start_position = start_position;
                  target = shared;
                }
              } else if (target_start_position <= start_position &&
                         shared->end_position() <= target->end_position()) {
                // This containment check includes equality as a function inside
                // a top-level function can share either start or end position
                // with the top-level function.
                target_start_position = start_position;
                target = shared;
              }
            }
          }
        }
      }
    }

    if (target.is_null()) {
      return Heap::undefined_value();
    }

    // If the candidate found is compiled we are done. NOTE: when lazy
    // compilation of inner functions is introduced some additional checking
    // needs to be done here to compile inner functions.
    done = target->is_compiled();
    if (!done) {
      // If the candidate is not compiled compile it to reveal any inner
      // functions which might contain the requested source position.
      CompileLazyShared(target, KEEP_EXCEPTION);
    }
  }

  return *target;
}


// Changes the state of a break point in a script and returns source position
// where break point was set. NOTE: Regarding performance see the NOTE for
// GetScriptFromScriptData.
// args[0]: script to set break point in
// args[1]: number: break source position (within the script source)
// args[2]: number: break point object
static Object* Runtime_SetScriptBreakPoint(Arguments args) {
  HandleScope scope;
  ASSERT(args.length() == 3);
  CONVERT_ARG_CHECKED(JSValue, wrapper, 0);
  CONVERT_NUMBER_CHECKED(int32_t, source_position, Int32, args[1]);
  RUNTIME_ASSERT(source_position >= 0);
  Handle<Object> break_point_object_arg = args.at<Object>(2);

  // Get the script from the script wrapper.
  RUNTIME_ASSERT(wrapper->value()->IsScript());
  Handle<Script> script(Script::cast(wrapper->value()));

  Object* result = Runtime::FindSharedFunctionInfoInScript(
      script, source_position);
  if (!result->IsUndefined()) {
    Handle<SharedFunctionInfo> shared(SharedFunctionInfo::cast(result));
    // Find position within function. The script position might be before the
    // source position of the first function.
    int position;
    if (shared->start_position() > source_position) {
      position = 0;
    } else {
      position = source_position - shared->start_position();
    }
    Debug::SetBreakPoint(shared, break_point_object_arg, &position);
    position += shared->start_position();
    return Smi::FromInt(position);
  }
  return  Heap::undefined_value();
}


// Clear a break point
// args[0]: number: break point object
static Object* Runtime_ClearBreakPoint(Arguments args) {
  HandleScope scope;
  ASSERT(args.length() == 1);
  Handle<Object> break_point_object_arg = args.at<Object>(0);

  // Clear break point.
  Debug::ClearBreakPoint(break_point_object_arg);

  return Heap::undefined_value();
}


// Change the state of break on exceptions
// args[0]: boolean indicating uncaught exceptions
// args[1]: boolean indicating on/off
static Object* Runtime_ChangeBreakOnException(Arguments args) {
  HandleScope scope;
  ASSERT(args.length() == 2);
  ASSERT(args[0]->IsNumber());
  ASSERT(args[1]->IsBoolean());

  // Update break point state
  ExceptionBreakType type =
      static_cast<ExceptionBreakType>(NumberToUint32(args[0]));
  bool enable = args[1]->ToBoolean()->IsTrue();
  Debug::ChangeBreakOnException(type, enable);
  return Heap::undefined_value();
}


// Prepare for stepping
// args[0]: break id for checking execution state
// args[1]: step action from the enumeration StepAction
// args[2]: number of times to perform the step, for step out it is the number
//          of frames to step down.
static Object* Runtime_PrepareStep(Arguments args) {
  HandleScope scope;
  ASSERT(args.length() == 3);
  // Check arguments.
  Object* check = Runtime_CheckExecutionState(args);
  if (check->IsFailure()) return check;
  if (!args[1]->IsNumber() || !args[2]->IsNumber()) {
    return Top::Throw(Heap::illegal_argument_symbol());
  }

  // Get the step action and check validity.
  StepAction step_action = static_cast<StepAction>(NumberToInt32(args[1]));
  if (step_action != StepIn &&
      step_action != StepNext &&
      step_action != StepOut &&
      step_action != StepInMin &&
      step_action != StepMin) {
    return Top::Throw(Heap::illegal_argument_symbol());
  }

  // Get the number of steps.
  int step_count = NumberToInt32(args[2]);
  if (step_count < 1) {
    return Top::Throw(Heap::illegal_argument_symbol());
  }

  // Clear all current stepping setup.
  Debug::ClearStepping();

  // Prepare step.
  Debug::PrepareStep(static_cast<StepAction>(step_action), step_count);
  return Heap::undefined_value();
}


// Clear all stepping set by PrepareStep.
static Object* Runtime_ClearStepping(Arguments args) {
  HandleScope scope;
  ASSERT(args.length() == 0);
  Debug::ClearStepping();
  return Heap::undefined_value();
}


// Creates a copy of the with context chain. The copy of the context chain is
// is linked to the function context supplied.
static Handle<Context> CopyWithContextChain(Handle<Context> context_chain,
                                            Handle<Context> function_context) {
  // At the bottom of the chain. Return the function context to link to.
  if (context_chain->is_function_context()) {
    return function_context;
  }

  // Recursively copy the with contexts.
  Handle<Context> previous(context_chain->previous());
  Handle<JSObject> extension(JSObject::cast(context_chain->extension()));
  return Factory::NewWithContext(
      CopyWithContextChain(function_context, previous),
      extension,
      context_chain->IsCatchContext());
}


// Helper function to find or create the arguments object for
// Runtime_DebugEvaluate.
static Handle<Object> GetArgumentsObject(JavaScriptFrame* frame,
                                         Handle<JSFunction> function,
                                         Handle<SerializedScopeInfo> scope_info,
                                         const ScopeInfo<>* sinfo,
                                         Handle<Context> function_context) {
  // Try to find the value of 'arguments' to pass as parameter. If it is not
  // found (that is the debugged function does not reference 'arguments' and
  // does not support eval) then create an 'arguments' object.
  int index;
  if (sinfo->number_of_stack_slots() > 0) {
    index = scope_info->StackSlotIndex(Heap::arguments_symbol());
    if (index != -1) {
      return Handle<Object>(frame->GetExpression(index));
    }
  }

  if (sinfo->number_of_context_slots() > Context::MIN_CONTEXT_SLOTS) {
    index = scope_info->ContextSlotIndex(Heap::arguments_symbol(), NULL);
    if (index != -1) {
      return Handle<Object>(function_context->get(index));
    }
  }

  const int length = frame->GetProvidedParametersCount();
  Handle<JSObject> arguments = Factory::NewArgumentsObject(function, length);
  Handle<FixedArray> array = Factory::NewFixedArray(length);

  AssertNoAllocation no_gc;
  WriteBarrierMode mode = array->GetWriteBarrierMode(no_gc);
  for (int i = 0; i < length; i++) {
    array->set(i, frame->GetParameter(i), mode);
  }
  arguments->set_elements(*array);
  return arguments;
}


// Evaluate a piece of JavaScript in the context of a stack frame for
// debugging. This is accomplished by creating a new context which in its
// extension part has all the parameters and locals of the function on the
// stack frame. A function which calls eval with the code to evaluate is then
// compiled in this context and called in this context. As this context
// replaces the context of the function on the stack frame a new (empty)
// function is created as well to be used as the closure for the context.
// This function and the context acts as replacements for the function on the
// stack frame presenting the same view of the values of parameters and
// local variables as if the piece of JavaScript was evaluated at the point
// where the function on the stack frame is currently stopped.
static Object* Runtime_DebugEvaluate(Arguments args) {
  HandleScope scope;

  // Check the execution state and decode arguments frame and source to be
  // evaluated.
  ASSERT(args.length() == 4);
  Object* check_result = Runtime_CheckExecutionState(args);
  if (check_result->IsFailure()) return check_result;
  CONVERT_CHECKED(Smi, wrapped_id, args[1]);
  CONVERT_ARG_CHECKED(String, source, 2);
  CONVERT_BOOLEAN_CHECKED(disable_break, args[3]);

  // Handle the processing of break.
  DisableBreak disable_break_save(disable_break);

  // Get the frame where the debugging is performed.
  StackFrame::Id id = UnwrapFrameId(wrapped_id);
  JavaScriptFrameIterator it(id);
  JavaScriptFrame* frame = it.frame();
  Handle<JSFunction> function(JSFunction::cast(frame->function()));
  Handle<SerializedScopeInfo> scope_info(function->shared()->scope_info());
  ScopeInfo<> sinfo(*scope_info);

  // Traverse the saved contexts chain to find the active context for the
  // selected frame.
  SaveContext* save = Top::save_context();
  while (save != NULL && !save->below(frame)) {
    save = save->prev();
  }
  ASSERT(save != NULL);
  SaveContext savex;
  Top::set_context(*(save->context()));

  // Create the (empty) function replacing the function on the stack frame for
  // the purpose of evaluating in the context created below. It is important
  // that this function does not describe any parameters and local variables
  // in the context. If it does then this will cause problems with the lookup
  // in Context::Lookup, where context slots for parameters and local variables
  // are looked at before the extension object.
  Handle<JSFunction> go_between =
      Factory::NewFunction(Factory::empty_string(), Factory::undefined_value());
  go_between->set_context(function->context());
#ifdef DEBUG
  ScopeInfo<> go_between_sinfo(go_between->shared()->scope_info());
  ASSERT(go_between_sinfo.number_of_parameters() == 0);
  ASSERT(go_between_sinfo.number_of_context_slots() == 0);
#endif

  // Materialize the content of the local scope into a JSObject.
  Handle<JSObject> local_scope = MaterializeLocalScope(frame);

  // Allocate a new context for the debug evaluation and set the extension
  // object build.
  Handle<Context> context =
      Factory::NewFunctionContext(Context::MIN_CONTEXT_SLOTS, go_between);
  context->set_extension(*local_scope);
  // Copy any with contexts present and chain them in front of this context.
  Handle<Context> frame_context(Context::cast(frame->context()));
  Handle<Context> function_context(frame_context->fcontext());
  context = CopyWithContextChain(frame_context, context);

  // Wrap the evaluation statement in a new function compiled in the newly
  // created context. The function has one parameter which has to be called
  // 'arguments'. This it to have access to what would have been 'arguments' in
  // the function being debugged.
  // function(arguments,__source__) {return eval(__source__);}
  static const char* source_str =
      "(function(arguments,__source__){return eval(__source__);})";
  static const int source_str_length = StrLength(source_str);
  Handle<String> function_source =
      Factory::NewStringFromAscii(Vector<const char>(source_str,
                                                     source_str_length));
  Handle<SharedFunctionInfo> shared =
      Compiler::CompileEval(function_source,
                            context,
                            context->IsGlobalContext(),
                            Compiler::DONT_VALIDATE_JSON);
  if (shared.is_null()) return Failure::Exception();
  Handle<JSFunction> compiled_function =
      Factory::NewFunctionFromSharedFunctionInfo(shared, context);

  // Invoke the result of the compilation to get the evaluation function.
  bool has_pending_exception;
  Handle<Object> receiver(frame->receiver());
  Handle<Object> evaluation_function =
      Execution::Call(compiled_function, receiver, 0, NULL,
                      &has_pending_exception);
  if (has_pending_exception) return Failure::Exception();

  Handle<Object> arguments = GetArgumentsObject(frame, function, scope_info,
                                                &sinfo, function_context);

  // Invoke the evaluation function and return the result.
  const int argc = 2;
  Object** argv[argc] = { arguments.location(),
                          Handle<Object>::cast(source).location() };
  Handle<Object> result =
      Execution::Call(Handle<JSFunction>::cast(evaluation_function), receiver,
                      argc, argv, &has_pending_exception);
  if (has_pending_exception) return Failure::Exception();

  // Skip the global proxy as it has no properties and always delegates to the
  // real global object.
  if (result->IsJSGlobalProxy()) {
    result = Handle<JSObject>(JSObject::cast(result->GetPrototype()));
  }

  return *result;
}


static Object* Runtime_DebugEvaluateGlobal(Arguments args) {
  HandleScope scope;

  // Check the execution state and decode arguments frame and source to be
  // evaluated.
  ASSERT(args.length() == 3);
  Object* check_result = Runtime_CheckExecutionState(args);
  if (check_result->IsFailure()) return check_result;
  CONVERT_ARG_CHECKED(String, source, 1);
  CONVERT_BOOLEAN_CHECKED(disable_break, args[2]);

  // Handle the processing of break.
  DisableBreak disable_break_save(disable_break);

  // Enter the top context from before the debugger was invoked.
  SaveContext save;
  SaveContext* top = &save;
  while (top != NULL && *top->context() == *Debug::debug_context()) {
    top = top->prev();
  }
  if (top != NULL) {
    Top::set_context(*top->context());
  }

  // Get the global context now set to the top context from before the
  // debugger was invoked.
  Handle<Context> context = Top::global_context();

  // Compile the source to be evaluated.
  Handle<SharedFunctionInfo> shared =
      Compiler::CompileEval(source,
                            context,
                            true,
                            Compiler::DONT_VALIDATE_JSON);
  if (shared.is_null()) return Failure::Exception();
  Handle<JSFunction> compiled_function =
      Handle<JSFunction>(Factory::NewFunctionFromSharedFunctionInfo(shared,
                                                                    context));

  // Invoke the result of the compilation to get the evaluation function.
  bool has_pending_exception;
  Handle<Object> receiver = Top::global();
  Handle<Object> result =
    Execution::Call(compiled_function, receiver, 0, NULL,
                    &has_pending_exception);
  if (has_pending_exception) return Failure::Exception();
  return *result;
}


static Object* Runtime_DebugGetLoadedScripts(Arguments args) {
  HandleScope scope;
  ASSERT(args.length() == 0);

  // Fill the script objects.
  Handle<FixedArray> instances = Debug::GetLoadedScripts();

  // Convert the script objects to proper JS objects.
  for (int i = 0; i < instances->length(); i++) {
    Handle<Script> script = Handle<Script>(Script::cast(instances->get(i)));
    // Get the script wrapper in a local handle before calling GetScriptWrapper,
    // because using
    //   instances->set(i, *GetScriptWrapper(script))
    // is unsafe as GetScriptWrapper might call GC and the C++ compiler might
    // already have deferenced the instances handle.
    Handle<JSValue> wrapper = GetScriptWrapper(script);
    instances->set(i, *wrapper);
  }

  // Return result as a JS array.
  Handle<JSObject> result = Factory::NewJSObject(Top::array_function());
  Handle<JSArray>::cast(result)->SetContent(*instances);
  return *result;
}


// Helper function used by Runtime_DebugReferencedBy below.
static int DebugReferencedBy(JSObject* target,
                             Object* instance_filter, int max_references,
                             FixedArray* instances, int instances_size,
                             JSFunction* arguments_function) {
  NoHandleAllocation ha;
  AssertNoAllocation no_alloc;

  // Iterate the heap.
  int count = 0;
  JSObject* last = NULL;
  HeapIterator iterator;
  HeapObject* heap_obj = NULL;
  while (((heap_obj = iterator.next()) != NULL) &&
         (max_references == 0 || count < max_references)) {
    // Only look at all JSObjects.
    if (heap_obj->IsJSObject()) {
      // Skip context extension objects and argument arrays as these are
      // checked in the context of functions using them.
      JSObject* obj = JSObject::cast(heap_obj);
      if (obj->IsJSContextExtensionObject() ||
          obj->map()->constructor() == arguments_function) {
        continue;
      }

      // Check if the JS object has a reference to the object looked for.
      if (obj->ReferencesObject(target)) {
        // Check instance filter if supplied. This is normally used to avoid
        // references from mirror objects (see Runtime_IsInPrototypeChain).
        if (!instance_filter->IsUndefined()) {
          Object* V = obj;
          while (true) {
            Object* prototype = V->GetPrototype();
            if (prototype->IsNull()) {
              break;
            }
            if (instance_filter == prototype) {
              obj = NULL;  // Don't add this object.
              break;
            }
            V = prototype;
          }
        }

        if (obj != NULL) {
          // Valid reference found add to instance array if supplied an update
          // count.
          if (instances != NULL && count < instances_size) {
            instances->set(count, obj);
          }
          last = obj;
          count++;
        }
      }
    }
  }

  // Check for circular reference only. This can happen when the object is only
  // referenced from mirrors and has a circular reference in which case the
  // object is not really alive and would have been garbage collected if not
  // referenced from the mirror.
  if (count == 1 && last == target) {
    count = 0;
  }

  // Return the number of referencing objects found.
  return count;
}


// Scan the heap for objects with direct references to an object
// args[0]: the object to find references to
// args[1]: constructor function for instances to exclude (Mirror)
// args[2]: the the maximum number of objects to return
static Object* Runtime_DebugReferencedBy(Arguments args) {
  ASSERT(args.length() == 3);

  // First perform a full GC in order to avoid references from dead objects.
  Heap::CollectAllGarbage(false);

  // Check parameters.
  CONVERT_CHECKED(JSObject, target, args[0]);
  Object* instance_filter = args[1];
  RUNTIME_ASSERT(instance_filter->IsUndefined() ||
                 instance_filter->IsJSObject());
  CONVERT_NUMBER_CHECKED(int32_t, max_references, Int32, args[2]);
  RUNTIME_ASSERT(max_references >= 0);

  // Get the constructor function for context extension and arguments array.
  JSObject* arguments_boilerplate =
      Top::context()->global_context()->arguments_boilerplate();
  JSFunction* arguments_function =
      JSFunction::cast(arguments_boilerplate->map()->constructor());

  // Get the number of referencing objects.
  int count;
  count = DebugReferencedBy(target, instance_filter, max_references,
                            NULL, 0, arguments_function);

  // Allocate an array to hold the result.
  Object* object = Heap::AllocateFixedArray(count);
  if (object->IsFailure()) return object;
  FixedArray* instances = FixedArray::cast(object);

  // Fill the referencing objects.
  count = DebugReferencedBy(target, instance_filter, max_references,
                            instances, count, arguments_function);

  // Return result as JS array.
  Object* result =
      Heap::AllocateJSObject(
          Top::context()->global_context()->array_function());
  if (!result->IsFailure()) JSArray::cast(result)->SetContent(instances);
  return result;
}


// Helper function used by Runtime_DebugConstructedBy below.
static int DebugConstructedBy(JSFunction* constructor, int max_references,
                              FixedArray* instances, int instances_size) {
  AssertNoAllocation no_alloc;

  // Iterate the heap.
  int count = 0;
  HeapIterator iterator;
  HeapObject* heap_obj = NULL;
  while (((heap_obj = iterator.next()) != NULL) &&
         (max_references == 0 || count < max_references)) {
    // Only look at all JSObjects.
    if (heap_obj->IsJSObject()) {
      JSObject* obj = JSObject::cast(heap_obj);
      if (obj->map()->constructor() == constructor) {
        // Valid reference found add to instance array if supplied an update
        // count.
        if (instances != NULL && count < instances_size) {
          instances->set(count, obj);
        }
        count++;
      }
    }
  }

  // Return the number of referencing objects found.
  return count;
}


// Scan the heap for objects constructed by a specific function.
// args[0]: the constructor to find instances of
// args[1]: the the maximum number of objects to return
static Object* Runtime_DebugConstructedBy(Arguments args) {
  ASSERT(args.length() == 2);

  // First perform a full GC in order to avoid dead objects.
  Heap::CollectAllGarbage(false);

  // Check parameters.
  CONVERT_CHECKED(JSFunction, constructor, args[0]);
  CONVERT_NUMBER_CHECKED(int32_t, max_references, Int32, args[1]);
  RUNTIME_ASSERT(max_references >= 0);

  // Get the number of referencing objects.
  int count;
  count = DebugConstructedBy(constructor, max_references, NULL, 0);

  // Allocate an array to hold the result.
  Object* object = Heap::AllocateFixedArray(count);
  if (object->IsFailure()) return object;
  FixedArray* instances = FixedArray::cast(object);

  // Fill the referencing objects.
  count = DebugConstructedBy(constructor, max_references, instances, count);

  // Return result as JS array.
  Object* result =
      Heap::AllocateJSObject(
          Top::context()->global_context()->array_function());
  if (!result->IsFailure()) JSArray::cast(result)->SetContent(instances);
  return result;
}


// Find the effective prototype object as returned by __proto__.
// args[0]: the object to find the prototype for.
static Object* Runtime_DebugGetPrototype(Arguments args) {
  ASSERT(args.length() == 1);

  CONVERT_CHECKED(JSObject, obj, args[0]);

  // Use the __proto__ accessor.
  return Accessors::ObjectPrototype.getter(obj, NULL);
}


static Object* Runtime_SystemBreak(Arguments args) {
  ASSERT(args.length() == 0);
  CPU::DebugBreak();
  return Heap::undefined_value();
}


static Object* Runtime_DebugDisassembleFunction(Arguments args) {
#ifdef DEBUG
  HandleScope scope;
  ASSERT(args.length() == 1);
  // Get the function and make sure it is compiled.
  CONVERT_ARG_CHECKED(JSFunction, func, 0);
  Handle<SharedFunctionInfo> shared(func->shared());
  if (!EnsureCompiled(shared, KEEP_EXCEPTION)) {
    return Failure::Exception();
  }
  func->code()->PrintLn();
#endif  // DEBUG
  return Heap::undefined_value();
}


static Object* Runtime_DebugDisassembleConstructor(Arguments args) {
#ifdef DEBUG
  HandleScope scope;
  ASSERT(args.length() == 1);
  // Get the function and make sure it is compiled.
  CONVERT_ARG_CHECKED(JSFunction, func, 0);
  Handle<SharedFunctionInfo> shared(func->shared());
  if (!EnsureCompiled(shared, KEEP_EXCEPTION)) {
    return Failure::Exception();
  }
  shared->construct_stub()->PrintLn();
#endif  // DEBUG
  return Heap::undefined_value();
}


static Object* Runtime_FunctionGetInferredName(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 1);

  CONVERT_CHECKED(JSFunction, f, args[0]);
  return f->shared()->inferred_name();
}


static int FindSharedFunctionInfosForScript(Script* script,
                                     FixedArray* buffer) {
  AssertNoAllocation no_allocations;

  int counter = 0;
  int buffer_size = buffer->length();
  HeapIterator iterator;
  for (HeapObject* obj = iterator.next(); obj != NULL; obj = iterator.next()) {
    ASSERT(obj != NULL);
    if (!obj->IsSharedFunctionInfo()) {
      continue;
    }
    SharedFunctionInfo* shared = SharedFunctionInfo::cast(obj);
    if (shared->script() != script) {
      continue;
    }
    if (counter < buffer_size) {
      buffer->set(counter, shared);
    }
    counter++;
  }
  return counter;
}

// For a script finds all SharedFunctionInfo's in the heap that points
// to this script. Returns JSArray of SharedFunctionInfo wrapped
// in OpaqueReferences.
static Object* Runtime_LiveEditFindSharedFunctionInfosForScript(
    Arguments args) {
  ASSERT(args.length() == 1);
  HandleScope scope;
  CONVERT_CHECKED(JSValue, script_value, args[0]);

  Handle<Script> script = Handle<Script>(Script::cast(script_value->value()));

  const int kBufferSize = 32;

  Handle<FixedArray> array;
  array = Factory::NewFixedArray(kBufferSize);
  int number = FindSharedFunctionInfosForScript(*script, *array);
  if (number > kBufferSize) {
    array = Factory::NewFixedArray(number);
    FindSharedFunctionInfosForScript(*script, *array);
  }

  Handle<JSArray> result = Factory::NewJSArrayWithElements(array);
  result->set_length(Smi::FromInt(number));

  LiveEdit::WrapSharedFunctionInfos(result);

  return *result;
}

// For a script calculates compilation information about all its functions.
// The script source is explicitly specified by the second argument.
// The source of the actual script is not used, however it is important that
// all generated code keeps references to this particular instance of script.
// Returns a JSArray of compilation infos. The array is ordered so that
// each function with all its descendant is always stored in a continues range
// with the function itself going first. The root function is a script function.
static Object* Runtime_LiveEditGatherCompileInfo(Arguments args) {
  ASSERT(args.length() == 2);
  HandleScope scope;
  CONVERT_CHECKED(JSValue, script, args[0]);
  CONVERT_ARG_CHECKED(String, source, 1);
  Handle<Script> script_handle = Handle<Script>(Script::cast(script->value()));

  JSArray* result =  LiveEdit::GatherCompileInfo(script_handle, source);

  if (Top::has_pending_exception()) {
    return Failure::Exception();
  }

  return result;
}

// Changes the source of the script to a new_source.
// If old_script_name is provided (i.e. is a String), also creates a copy of
// the script with its original source and sends notification to debugger.
static Object* Runtime_LiveEditReplaceScript(Arguments args) {
  ASSERT(args.length() == 3);
  HandleScope scope;
  CONVERT_CHECKED(JSValue, original_script_value, args[0]);
  CONVERT_ARG_CHECKED(String, new_source, 1);
  Handle<Object> old_script_name(args[2]);

  CONVERT_CHECKED(Script, original_script_pointer,
                  original_script_value->value());
  Handle<Script> original_script(original_script_pointer);

  Object* old_script = LiveEdit::ChangeScriptSource(original_script,
                                                    new_source,
                                                    old_script_name);

  if (old_script->IsScript()) {
    Handle<Script> script_handle(Script::cast(old_script));
    return *(GetScriptWrapper(script_handle));
  } else {
    return Heap::null_value();
  }
}

// Replaces code of SharedFunctionInfo with a new one.
static Object* Runtime_LiveEditReplaceFunctionCode(Arguments args) {
  ASSERT(args.length() == 2);
  HandleScope scope;
  CONVERT_ARG_CHECKED(JSArray, new_compile_info, 0);
  CONVERT_ARG_CHECKED(JSArray, shared_info, 1);

  return LiveEdit::ReplaceFunctionCode(new_compile_info, shared_info);
}

// Connects SharedFunctionInfo to another script.
static Object* Runtime_LiveEditFunctionSetScript(Arguments args) {
  ASSERT(args.length() == 2);
  HandleScope scope;
  Handle<Object> function_object(args[0]);
  Handle<Object> script_object(args[1]);

  if (function_object->IsJSValue()) {
    Handle<JSValue> function_wrapper = Handle<JSValue>::cast(function_object);
    if (script_object->IsJSValue()) {
      CONVERT_CHECKED(Script, script, JSValue::cast(*script_object)->value());
      script_object = Handle<Object>(script);
    }

    LiveEdit::SetFunctionScript(function_wrapper, script_object);
  } else {
    // Just ignore this. We may not have a SharedFunctionInfo for some functions
    // and we check it in this function.
  }

  return Heap::undefined_value();
}


// In a code of a parent function replaces original function as embedded object
// with a substitution one.
static Object* Runtime_LiveEditReplaceRefToNestedFunction(Arguments args) {
  ASSERT(args.length() == 3);
  HandleScope scope;

  CONVERT_ARG_CHECKED(JSValue, parent_wrapper, 0);
  CONVERT_ARG_CHECKED(JSValue, orig_wrapper, 1);
  CONVERT_ARG_CHECKED(JSValue, subst_wrapper, 2);

  LiveEdit::ReplaceRefToNestedFunction(parent_wrapper, orig_wrapper,
                                       subst_wrapper);

  return Heap::undefined_value();
}


// Updates positions of a shared function info (first parameter) according
// to script source change. Text change is described in second parameter as
// array of groups of 3 numbers:
// (change_begin, change_end, change_end_new_position).
// Each group describes a change in text; groups are sorted by change_begin.
static Object* Runtime_LiveEditPatchFunctionPositions(Arguments args) {
  ASSERT(args.length() == 2);
  HandleScope scope;
  CONVERT_ARG_CHECKED(JSArray, shared_array, 0);
  CONVERT_ARG_CHECKED(JSArray, position_change_array, 1);

  return LiveEdit::PatchFunctionPositions(shared_array, position_change_array);
}


// For array of SharedFunctionInfo's (each wrapped in JSValue)
// checks that none of them have activations on stacks (of any thread).
// Returns array of the same length with corresponding results of
// LiveEdit::FunctionPatchabilityStatus type.
static Object* Runtime_LiveEditCheckAndDropActivations(Arguments args) {
  ASSERT(args.length() == 2);
  HandleScope scope;
  CONVERT_ARG_CHECKED(JSArray, shared_array, 0);
  CONVERT_BOOLEAN_CHECKED(do_drop, args[1]);

  return *LiveEdit::CheckAndDropActivations(shared_array, do_drop);
}

// Compares 2 strings line-by-line and returns diff in form of JSArray of
// triplets (pos1, pos1_end, pos2_end) describing list of diff chunks.
static Object* Runtime_LiveEditCompareStringsLinewise(Arguments args) {
  ASSERT(args.length() == 2);
  HandleScope scope;
  CONVERT_ARG_CHECKED(String, s1, 0);
  CONVERT_ARG_CHECKED(String, s2, 1);

  return *LiveEdit::CompareStringsLinewise(s1, s2);
}



// A testing entry. Returns statement position which is the closest to
// source_position.
static Object* Runtime_GetFunctionCodePositionFromSource(Arguments args) {
  ASSERT(args.length() == 2);
  HandleScope scope;
  CONVERT_ARG_CHECKED(JSFunction, function, 0);
  CONVERT_NUMBER_CHECKED(int32_t, source_position, Int32, args[1]);

  Handle<Code> code(function->code());

  RelocIterator it(*code, 1 << RelocInfo::STATEMENT_POSITION);
  int closest_pc = 0;
  int distance = kMaxInt;
  while (!it.done()) {
    int statement_position = static_cast<int>(it.rinfo()->data());
    // Check if this break point is closer that what was previously found.
    if (source_position <= statement_position &&
        statement_position - source_position < distance) {
      closest_pc =
          static_cast<int>(it.rinfo()->pc() - code->instruction_start());
      distance = statement_position - source_position;
      // Check whether we can't get any closer.
      if (distance == 0) break;
    }
    it.next();
  }

  return Smi::FromInt(closest_pc);
}


// Calls specified function with or without entering the debugger.
// This is used in unit tests to run code as if debugger is entered or simply
// to have a stack with C++ frame in the middle.
static Object* Runtime_ExecuteInDebugContext(Arguments args) {
  ASSERT(args.length() == 2);
  HandleScope scope;
  CONVERT_ARG_CHECKED(JSFunction, function, 0);
  CONVERT_BOOLEAN_CHECKED(without_debugger, args[1]);

  Handle<Object> result;
  bool pending_exception;
  {
    if (without_debugger) {
      result = Execution::Call(function, Top::global(), 0, NULL,
                               &pending_exception);
    } else {
      EnterDebugger enter_debugger;
      result = Execution::Call(function, Top::global(), 0, NULL,
                               &pending_exception);
    }
  }
  if (!pending_exception) {
    return *result;
  } else {
    return Failure::Exception();
  }
}


#endif  // ENABLE_DEBUGGER_SUPPORT

#ifdef ENABLE_LOGGING_AND_PROFILING

static Object* Runtime_ProfilerResume(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);

  CONVERT_CHECKED(Smi, smi_modules, args[0]);
  CONVERT_CHECKED(Smi, smi_tag, args[1]);
  v8::V8::ResumeProfilerEx(smi_modules->value(), smi_tag->value());
  return Heap::undefined_value();
}


static Object* Runtime_ProfilerPause(Arguments args) {
  NoHandleAllocation ha;
  ASSERT(args.length() == 2);

  CONVERT_CHECKED(Smi, smi_modules, args[0]);
  CONVERT_CHECKED(Smi, smi_tag, args[1]);
  v8::V8::PauseProfilerEx(smi_modules->value(), smi_tag->value());
  return Heap::undefined_value();
}

#endif  // ENABLE_LOGGING_AND_PROFILING

// Finds the script object from the script data. NOTE: This operation uses
// heap traversal to find the function generated for the source position
// for the requested break point. For lazily compiled functions several heap
// traversals might be required rendering this operation as a rather slow
// operation. However for setting break points which is normally done through
// some kind of user interaction the performance is not crucial.
static Handle<Object> Runtime_GetScriptFromScriptName(
    Handle<String> script_name) {
  // Scan the heap for Script objects to find the script with the requested
  // script data.
  Handle<Script> script;
  HeapIterator iterator;
  HeapObject* obj = NULL;
  while (script.is_null() && ((obj = iterator.next()) != NULL)) {
    // If a script is found check if it has the script data requested.
    if (obj->IsScript()) {
      if (Script::cast(obj)->name()->IsString()) {
        if (String::cast(Script::cast(obj)->name())->Equals(*script_name)) {
          script = Handle<Script>(Script::cast(obj));
        }
      }
    }
  }

  // If no script with the requested script data is found return undefined.
  if (script.is_null()) return Factory::undefined_value();

  // Return the script found.
  return GetScriptWrapper(script);
}


// Get the script object from script data. NOTE: Regarding performance
// see the NOTE for GetScriptFromScriptData.
// args[0]: script data for the script to find the source for
static Object* Runtime_GetScript(Arguments args) {
  HandleScope scope;

  ASSERT(args.length() == 1);

  CONVERT_CHECKED(String, script_name, args[0]);

  // Find the requested script.
  Handle<Object> result =
      Runtime_GetScriptFromScriptName(Handle<String>(script_name));
  return *result;
}


// Determines whether the given stack frame should be displayed in
// a stack trace.  The caller is the error constructor that asked
// for the stack trace to be collected.  The first time a construct
// call to this function is encountered it is skipped.  The seen_caller
// in/out parameter is used to remember if the caller has been seen
// yet.
static bool ShowFrameInStackTrace(StackFrame* raw_frame, Object* caller,
    bool* seen_caller) {
  // Only display JS frames.
  if (!raw_frame->is_java_script())
    return false;
  JavaScriptFrame* frame = JavaScriptFrame::cast(raw_frame);
  Object* raw_fun = frame->function();
  // Not sure when this can happen but skip it just in case.
  if (!raw_fun->IsJSFunction())
    return false;
  if ((raw_fun == caller) && !(*seen_caller)) {
    *seen_caller = true;
    return false;
  }
  // Skip all frames until we've seen the caller.  Also, skip the most
  // obvious builtin calls.  Some builtin calls (such as Number.ADD
  // which is invoked using 'call') are very difficult to recognize
  // so we're leaving them in for now.
  return *seen_caller && !frame->receiver()->IsJSBuiltinsObject();
}


// Collect the raw data for a stack trace.  Returns an array of three
// element segments each containing a receiver, function and native
// code offset.
static Object* Runtime_CollectStackTrace(Arguments args) {
  ASSERT_EQ(args.length(), 2);
  Handle<Object> caller = args.at<Object>(0);
  CONVERT_NUMBER_CHECKED(int32_t, limit, Int32, args[1]);

  HandleScope scope;

  limit = Max(limit, 0);  // Ensure that limit is not negative.
  int initial_size = Min(limit, 10);
  Handle<JSArray> result = Factory::NewJSArray(initial_size * 3);

  StackFrameIterator iter;
  // If the caller parameter is a function we skip frames until we're
  // under it before starting to collect.
  bool seen_caller = !caller->IsJSFunction();
  int cursor = 0;
  int frames_seen = 0;
  while (!iter.done() && frames_seen < limit) {
    StackFrame* raw_frame = iter.frame();
    if (ShowFrameInStackTrace(raw_frame, *caller, &seen_caller)) {
      frames_seen++;
      JavaScriptFrame* frame = JavaScriptFrame::cast(raw_frame);
      Object* recv = frame->receiver();
      Object* fun = frame->function();
      Address pc = frame->pc();
      Address start = frame->code()->address();
      Smi* offset = Smi::FromInt(static_cast<int>(pc - start));
      FixedArray* elements = FixedArray::cast(result->elements());
      if (cursor + 2 < elements->length()) {
        elements->set(cursor++, recv);
        elements->set(cursor++, fun);
        elements->set(cursor++, offset);
      } else {
        HandleScope scope;
        Handle<Object> recv_handle(recv);
        Handle<Object> fun_handle(fun);
        SetElement(result, cursor++, recv_handle);
        SetElement(result, cursor++, fun_handle);
        SetElement(result, cursor++, Handle<Smi>(offset));
      }
    }
    iter.Advance();
  }

  result->set_length(Smi::FromInt(cursor));
  return *result;
}


// Returns V8 version as a string.
static Object* Runtime_GetV8Version(Arguments args) {
  ASSERT_EQ(args.length(), 0);

  NoHandleAllocation ha;

  const char* version_string = v8::V8::GetVersion();

  return Heap::AllocateStringFromAscii(CStrVector(version_string), NOT_TENURED);
}


static Object* Runtime_Abort(Arguments args) {
  ASSERT(args.length() == 2);
  OS::PrintError("abort: %s\n", reinterpret_cast<char*>(args[0]) +
                                    Smi::cast(args[1])->value());
  Top::PrintStack();
  OS::Abort();
  UNREACHABLE();
  return NULL;
}


static Object* Runtime_DeleteHandleScopeExtensions(Arguments args) {
  ASSERT(args.length() == 0);
  HandleScope::DeleteExtensions();
  return Heap::undefined_value();
}


static Object* CacheMiss(FixedArray* cache_obj, int index, Object* key_obj) {
  ASSERT(index % 2 == 0);  // index of the key
  ASSERT(index >= JSFunctionResultCache::kEntriesIndex);
  ASSERT(index < cache_obj->length());

  HandleScope scope;

  Handle<FixedArray> cache(cache_obj);
  Handle<Object> key(key_obj);
  Handle<JSFunction> factory(JSFunction::cast(
        cache->get(JSFunctionResultCache::kFactoryIndex)));
  // TODO(antonm): consider passing a receiver when constructing a cache.
  Handle<Object> receiver(Top::global_context()->global());

  Handle<Object> value;
  {
    // This handle is nor shared, nor used later, so it's safe.
    Object** argv[] = { key.location() };
    bool pending_exception = false;
    value = Execution::Call(factory,
                            receiver,
                            1,
                            argv,
                            &pending_exception);
    if (pending_exception) return Failure::Exception();
  }

  cache->set(index, *key);
  cache->set(index + 1, *value);
  cache->set(JSFunctionResultCache::kFingerIndex, Smi::FromInt(index));

  return *value;
}


static Object* Runtime_GetFromCache(Arguments args) {
  // This is only called from codegen, so checks might be more lax.
  CONVERT_CHECKED(FixedArray, cache, args[0]);
  Object* key = args[1];

  const int finger_index =
      Smi::cast(cache->get(JSFunctionResultCache::kFingerIndex))->value();

  Object* o = cache->get(finger_index);
  if (o == key) {
    // The fastest case: hit the same place again.
    return cache->get(finger_index + 1);
  }

  for (int i = finger_index - 2;
       i >= JSFunctionResultCache::kEntriesIndex;
       i -= 2) {
    o = cache->get(i);
    if (o == key) {
      cache->set(JSFunctionResultCache::kFingerIndex, Smi::FromInt(i));
      return cache->get(i + 1);
    }
  }

  const int size =
      Smi::cast(cache->get(JSFunctionResultCache::kCacheSizeIndex))->value();
  ASSERT(size <= cache->length());

  for (int i = size - 2; i > finger_index; i -= 2) {
    o = cache->get(i);
    if (o == key) {
      cache->set(JSFunctionResultCache::kFingerIndex, Smi::FromInt(i));
      return cache->get(i + 1);
    }
  }

  // Cache miss.  If we have spare room, put new data into it, otherwise
  // evict post finger entry which must be least recently used.
  if (size < cache->length()) {
    cache->set(JSFunctionResultCache::kCacheSizeIndex, Smi::FromInt(size + 2));
    return CacheMiss(cache, size, key);
  } else {
    int target_index = finger_index + JSFunctionResultCache::kEntrySize;
    if (target_index == cache->length()) {
      target_index = JSFunctionResultCache::kEntriesIndex;
    }
    return CacheMiss(cache, target_index, key);
  }
}

#ifdef DEBUG
// ListNatives is ONLY used by the fuzz-natives.js in debug mode
// Exclude the code in release mode.
static Object* Runtime_ListNatives(Arguments args) {
  ASSERT(args.length() == 0);
  HandleScope scope;
  Handle<JSArray> result = Factory::NewJSArray(0);
  int index = 0;
  bool inline_runtime_functions = false;
#define ADD_ENTRY(Name, argc, ressize)                                       \
  {                                                                          \
    HandleScope inner;                                                       \
    Handle<String> name;                                                     \
    /* Inline runtime functions have an underscore in front of the name. */  \
    if (inline_runtime_functions) {                                          \
      name = Factory::NewStringFromAscii(                                    \
          Vector<const char>("_" #Name, StrLength("_" #Name)));              \
    } else {                                                                 \
      name = Factory::NewStringFromAscii(                                    \
          Vector<const char>(#Name, StrLength(#Name)));                      \
    }                                                                        \
    Handle<JSArray> pair = Factory::NewJSArray(0);                           \
    SetElement(pair, 0, name);                                               \
    SetElement(pair, 1, Handle<Smi>(Smi::FromInt(argc)));                    \
    SetElement(result, index++, pair);                                       \
  }
  inline_runtime_functions = false;
  RUNTIME_FUNCTION_LIST(ADD_ENTRY)
  inline_runtime_functions = true;
  INLINE_RUNTIME_FUNCTION_LIST(ADD_ENTRY)
#undef ADD_ENTRY
  return *result;
}
#endif


static Object* Runtime_Log(Arguments args) {
  ASSERT(args.length() == 2);
  CONVERT_CHECKED(String, format, args[0]);
  CONVERT_CHECKED(JSArray, elms, args[1]);
  Vector<const char> chars = format->ToAsciiVector();
  Logger::LogRuntime(chars, elms);
  return Heap::undefined_value();
}


static Object* Runtime_IS_VAR(Arguments args) {
  UNREACHABLE();  // implemented as macro in the parser
  return NULL;
}


// ----------------------------------------------------------------------------
// Implementation of Runtime

#define F(name, nargs, ressize)                                           \
  { #name, FUNCTION_ADDR(Runtime_##name), nargs, \
    static_cast<int>(Runtime::k##name), ressize },

static Runtime::Function Runtime_functions[] = {
  RUNTIME_FUNCTION_LIST(F)
  { NULL, NULL, 0, -1, 0 }
};

#undef F


Runtime::Function* Runtime::FunctionForId(FunctionId fid) {
  ASSERT(0 <= fid && fid < kNofFunctions);
  return &Runtime_functions[fid];
}


Runtime::Function* Runtime::FunctionForName(const char* name) {
  for (Function* f = Runtime_functions; f->name != NULL; f++) {
    if (strcmp(f->name, name) == 0) {
      return f;
    }
  }
  return NULL;
}


void Runtime::PerformGC(Object* result) {
  Failure* failure = Failure::cast(result);
  if (failure->IsRetryAfterGC()) {
    // Try to do a garbage collection; ignore it if it fails. The C
    // entry stub will throw an out-of-memory exception in that case.
    Heap::CollectGarbage(failure->requested(), failure->allocation_space());
  } else {
    // Handle last resort GC and make sure to allow future allocations
    // to grow the heap without causing GCs (if possible).
    Counters::gc_last_resort_from_js.Increment();
    Heap::CollectAllGarbage(false);
  }
}


} }  // namespace v8::internal
