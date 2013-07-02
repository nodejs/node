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

#include <stdlib.h>
#include <limits>

#include "v8.h"

#include "accessors.h"
#include "api.h"
#include "arguments.h"
#include "bootstrapper.h"
#include "codegen.h"
#include "compilation-cache.h"
#include "compiler.h"
#include "cpu.h"
#include "cpu-profiler.h"
#include "dateparser-inl.h"
#include "debug.h"
#include "deoptimizer.h"
#include "date.h"
#include "execution.h"
#include "full-codegen.h"
#include "global-handles.h"
#include "isolate-inl.h"
#include "jsregexp.h"
#include "jsregexp-inl.h"
#include "json-parser.h"
#include "json-stringifier.h"
#include "liveedit.h"
#include "misc-intrinsics.h"
#include "parser.h"
#include "platform.h"
#include "runtime-profiler.h"
#include "runtime.h"
#include "scopeinfo.h"
#include "smart-pointers.h"
#include "string-search.h"
#include "stub-cache.h"
#include "uri.h"
#include "v8conversions.h"
#include "v8threads.h"
#include "vm-state-inl.h"

#ifndef _STLP_VENDOR_CSTD
// STLPort doesn't import fpclassify and isless into the std namespace.
using std::fpclassify;
using std::isless;
#endif

namespace v8 {
namespace internal {


#define RUNTIME_ASSERT(value) \
  if (!(value)) return isolate->ThrowIllegalOperation();

// Cast the given object to a value of the specified type and store
// it in a variable with the given name.  If the object is not of the
// expected type call IllegalOperation and return.
#define CONVERT_ARG_CHECKED(Type, name, index)                       \
  RUNTIME_ASSERT(args[index]->Is##Type());                           \
  Type* name = Type::cast(args[index]);

#define CONVERT_ARG_HANDLE_CHECKED(Type, name, index)                \
  RUNTIME_ASSERT(args[index]->Is##Type());                           \
  Handle<Type> name = args.at<Type>(index);

// Cast the given object to a boolean and store it in a variable with
// the given name.  If the object is not a boolean call IllegalOperation
// and return.
#define CONVERT_BOOLEAN_ARG_CHECKED(name, index)                     \
  RUNTIME_ASSERT(args[index]->IsBoolean());                          \
  bool name = args[index]->IsTrue();

// Cast the given argument to a Smi and store its value in an int variable
// with the given name.  If the argument is not a Smi call IllegalOperation
// and return.
#define CONVERT_SMI_ARG_CHECKED(name, index)                         \
  RUNTIME_ASSERT(args[index]->IsSmi());                              \
  int name = args.smi_at(index);

// Cast the given argument to a double and store it in a variable with
// the given name.  If the argument is not a number (as opposed to
// the number not-a-number) call IllegalOperation and return.
#define CONVERT_DOUBLE_ARG_CHECKED(name, index)                      \
  RUNTIME_ASSERT(args[index]->IsNumber());                           \
  double name = args.number_at(index);

// Call the specified converter on the object *comand store the result in
// a variable of the specified type with the given name.  If the
// object is not a Number call IllegalOperation and return.
#define CONVERT_NUMBER_CHECKED(type, name, Type, obj)                \
  RUNTIME_ASSERT(obj->IsNumber());                                   \
  type name = NumberTo##Type(obj);


// Cast the given argument to PropertyDetails and store its value in a
// variable with the given name.  If the argument is not a Smi call
// IllegalOperation and return.
#define CONVERT_PROPERTY_DETAILS_CHECKED(name, index)                \
  RUNTIME_ASSERT(args[index]->IsSmi());                              \
  PropertyDetails name = PropertyDetails(Smi::cast(args[index]));


// Assert that the given argument has a valid value for a StrictModeFlag
// and store it in a StrictModeFlag variable with the given name.
#define CONVERT_STRICT_MODE_ARG_CHECKED(name, index)                 \
  RUNTIME_ASSERT(args[index]->IsSmi());                              \
  RUNTIME_ASSERT(args.smi_at(index) == kStrictMode ||                \
                 args.smi_at(index) == kNonStrictMode);              \
  StrictModeFlag name =                                              \
      static_cast<StrictModeFlag>(args.smi_at(index));


// Assert that the given argument has a valid value for a LanguageMode
// and store it in a LanguageMode variable with the given name.
#define CONVERT_LANGUAGE_MODE_ARG(name, index)                       \
  ASSERT(args[index]->IsSmi());                                      \
  ASSERT(args.smi_at(index) == CLASSIC_MODE ||                       \
         args.smi_at(index) == STRICT_MODE ||                        \
         args.smi_at(index) == EXTENDED_MODE);                       \
  LanguageMode name =                                                \
      static_cast<LanguageMode>(args.smi_at(index));


static Handle<Map> ComputeObjectLiteralMap(
    Handle<Context> context,
    Handle<FixedArray> constant_properties,
    bool* is_result_from_cache) {
  Isolate* isolate = context->GetIsolate();
  int properties_length = constant_properties->length();
  int number_of_properties = properties_length / 2;
  // Check that there are only internal strings and array indices among keys.
  int number_of_string_keys = 0;
  for (int p = 0; p != properties_length; p += 2) {
    Object* key = constant_properties->get(p);
    uint32_t element_index = 0;
    if (key->IsInternalizedString()) {
      number_of_string_keys++;
    } else if (key->ToArrayIndex(&element_index)) {
      // An index key does not require space in the property backing store.
      number_of_properties--;
    } else {
      // Bail out as a non-internalized-string non-index key makes caching
      // impossible.
      // ASSERT to make sure that the if condition after the loop is false.
      ASSERT(number_of_string_keys != number_of_properties);
      break;
    }
  }
  // If we only have internalized strings and array indices among keys then we
  // can use the map cache in the native context.
  const int kMaxKeys = 10;
  if ((number_of_string_keys == number_of_properties) &&
      (number_of_string_keys < kMaxKeys)) {
    // Create the fixed array with the key.
    Handle<FixedArray> keys =
        isolate->factory()->NewFixedArray(number_of_string_keys);
    if (number_of_string_keys > 0) {
      int index = 0;
      for (int p = 0; p < properties_length; p += 2) {
        Object* key = constant_properties->get(p);
        if (key->IsInternalizedString()) {
          keys->set(index++, key);
        }
      }
      ASSERT(index == number_of_string_keys);
    }
    *is_result_from_cache = true;
    return isolate->factory()->ObjectLiteralMapFromCache(context, keys);
  }
  *is_result_from_cache = false;
  return isolate->factory()->CopyMap(
      Handle<Map>(context->object_function()->initial_map()),
      number_of_properties);
}


static Handle<Object> CreateLiteralBoilerplate(
    Isolate* isolate,
    Handle<FixedArray> literals,
    Handle<FixedArray> constant_properties);


static Handle<Object> CreateObjectLiteralBoilerplate(
    Isolate* isolate,
    Handle<FixedArray> literals,
    Handle<FixedArray> constant_properties,
    bool should_have_fast_elements,
    bool has_function_literal) {
  // Get the native context from the literals array.  This is the
  // context in which the function was created and we use the object
  // function from this context to create the object literal.  We do
  // not use the object function from the current native context
  // because this might be the object function from another context
  // which we should not have access to.
  Handle<Context> context =
      Handle<Context>(JSFunction::NativeContextFromLiterals(*literals));

  // In case we have function literals, we want the object to be in
  // slow properties mode for now. We don't go in the map cache because
  // maps with constant functions can't be shared if the functions are
  // not the same (which is the common case).
  bool is_result_from_cache = false;
  Handle<Map> map = has_function_literal
      ? Handle<Map>(context->object_function()->initial_map())
      : ComputeObjectLiteralMap(context,
                                constant_properties,
                                &is_result_from_cache);

  Handle<JSObject> boilerplate =
      isolate->factory()->NewJSObjectFromMap(
          map, isolate->heap()->GetPretenureMode());

  // Normalize the elements of the boilerplate to save space if needed.
  if (!should_have_fast_elements) JSObject::NormalizeElements(boilerplate);

  // Add the constant properties to the boilerplate.
  int length = constant_properties->length();
  bool should_transform =
      !is_result_from_cache && boilerplate->HasFastProperties();
  if (should_transform || has_function_literal) {
    // Normalize the properties of object to avoid n^2 behavior
    // when extending the object multiple properties. Indicate the number of
    // properties to be added.
    JSObject::NormalizeProperties(
        boilerplate, KEEP_INOBJECT_PROPERTIES, length / 2);
  }

  // TODO(verwaest): Support tracking representations in the boilerplate.
  for (int index = 0; index < length; index +=2) {
    Handle<Object> key(constant_properties->get(index+0), isolate);
    Handle<Object> value(constant_properties->get(index+1), isolate);
    if (value->IsFixedArray()) {
      // The value contains the constant_properties of a
      // simple object or array literal.
      Handle<FixedArray> array = Handle<FixedArray>::cast(value);
      value = CreateLiteralBoilerplate(isolate, literals, array);
      if (value.is_null()) return value;
    }
    Handle<Object> result;
    uint32_t element_index = 0;
    if (key->IsInternalizedString()) {
      if (Handle<String>::cast(key)->AsArrayIndex(&element_index)) {
        // Array index as string (uint32).
        result = JSObject::SetOwnElement(
            boilerplate, element_index, value, kNonStrictMode);
      } else {
        Handle<String> name(String::cast(*key));
        ASSERT(!name->AsArrayIndex(&element_index));
        result = JSObject::SetLocalPropertyIgnoreAttributes(
            boilerplate, name, value, NONE);
      }
    } else if (key->ToArrayIndex(&element_index)) {
      // Array index (uint32).
      result = JSObject::SetOwnElement(
          boilerplate, element_index, value, kNonStrictMode);
    } else {
      // Non-uint32 number.
      ASSERT(key->IsNumber());
      double num = key->Number();
      char arr[100];
      Vector<char> buffer(arr, ARRAY_SIZE(arr));
      const char* str = DoubleToCString(num, buffer);
      Handle<String> name =
          isolate->factory()->NewStringFromAscii(CStrVector(str));
      result = JSObject::SetLocalPropertyIgnoreAttributes(
          boilerplate, name, value, NONE);
    }
    // If setting the property on the boilerplate throws an
    // exception, the exception is converted to an empty handle in
    // the handle based operations.  In that case, we need to
    // convert back to an exception.
    if (result.is_null()) return result;
  }

  // Transform to fast properties if necessary. For object literals with
  // containing function literals we defer this operation until after all
  // computed properties have been assigned so that we can generate
  // constant function properties.
  if (should_transform && !has_function_literal) {
    JSObject::TransformToFastProperties(
        boilerplate, boilerplate->map()->unused_property_fields());
  }

  return boilerplate;
}


MaybeObject* TransitionElements(Handle<Object> object,
                                ElementsKind to_kind,
                                Isolate* isolate) {
  HandleScope scope(isolate);
  if (!object->IsJSObject()) return isolate->ThrowIllegalOperation();
  ElementsKind from_kind =
      Handle<JSObject>::cast(object)->map()->elements_kind();
  if (Map::IsValidElementsTransition(from_kind, to_kind)) {
    Handle<Object> result = JSObject::TransitionElementsKind(
        Handle<JSObject>::cast(object), to_kind);
    if (result.is_null()) return isolate->ThrowIllegalOperation();
    return *result;
  }
  return isolate->ThrowIllegalOperation();
}


static const int kSmiLiteralMinimumLength = 1024;


Handle<Object> Runtime::CreateArrayLiteralBoilerplate(
    Isolate* isolate,
    Handle<FixedArray> literals,
    Handle<FixedArray> elements) {
  // Create the JSArray.
  Handle<JSFunction> constructor(
      JSFunction::NativeContextFromLiterals(*literals)->array_function());

  Handle<JSArray> object = Handle<JSArray>::cast(
      isolate->factory()->NewJSObject(
          constructor, isolate->heap()->GetPretenureMode()));

  ElementsKind constant_elements_kind =
      static_cast<ElementsKind>(Smi::cast(elements->get(0))->value());
  Handle<FixedArrayBase> constant_elements_values(
      FixedArrayBase::cast(elements->get(1)));

  ASSERT(IsFastElementsKind(constant_elements_kind));
  Context* native_context = isolate->context()->native_context();
  Object* maybe_maps_array = native_context->js_array_maps();
  ASSERT(!maybe_maps_array->IsUndefined());
  Object* maybe_map = FixedArray::cast(maybe_maps_array)->get(
      constant_elements_kind);
  ASSERT(maybe_map->IsMap());
  object->set_map(Map::cast(maybe_map));

  Handle<FixedArrayBase> copied_elements_values;
  if (IsFastDoubleElementsKind(constant_elements_kind)) {
    ASSERT(FLAG_smi_only_arrays);
    copied_elements_values = isolate->factory()->CopyFixedDoubleArray(
        Handle<FixedDoubleArray>::cast(constant_elements_values));
  } else {
    ASSERT(IsFastSmiOrObjectElementsKind(constant_elements_kind));
    const bool is_cow =
        (constant_elements_values->map() ==
         isolate->heap()->fixed_cow_array_map());
    if (is_cow) {
      copied_elements_values = constant_elements_values;
#if DEBUG
      Handle<FixedArray> fixed_array_values =
          Handle<FixedArray>::cast(copied_elements_values);
      for (int i = 0; i < fixed_array_values->length(); i++) {
        ASSERT(!fixed_array_values->get(i)->IsFixedArray());
      }
#endif
    } else {
      Handle<FixedArray> fixed_array_values =
          Handle<FixedArray>::cast(constant_elements_values);
      Handle<FixedArray> fixed_array_values_copy =
          isolate->factory()->CopyFixedArray(fixed_array_values);
      copied_elements_values = fixed_array_values_copy;
      for (int i = 0; i < fixed_array_values->length(); i++) {
        Object* current = fixed_array_values->get(i);
        if (current->IsFixedArray()) {
          // The value contains the constant_properties of a
          // simple object or array literal.
          Handle<FixedArray> fa(FixedArray::cast(fixed_array_values->get(i)));
          Handle<Object> result =
              CreateLiteralBoilerplate(isolate, literals, fa);
          if (result.is_null()) return result;
          fixed_array_values_copy->set(i, *result);
        }
      }
    }
  }
  object->set_elements(*copied_elements_values);
  object->set_length(Smi::FromInt(copied_elements_values->length()));

  //  Ensure that the boilerplate object has FAST_*_ELEMENTS, unless the flag is
  //  on or the object is larger than the threshold.
  if (!FLAG_smi_only_arrays &&
      constant_elements_values->length() < kSmiLiteralMinimumLength) {
    ElementsKind elements_kind = object->GetElementsKind();
    if (!IsFastObjectElementsKind(elements_kind)) {
      if (IsFastHoleyElementsKind(elements_kind)) {
        CHECK(!TransitionElements(object, FAST_HOLEY_ELEMENTS,
                                  isolate)->IsFailure());
      } else {
        CHECK(!TransitionElements(object, FAST_ELEMENTS, isolate)->IsFailure());
      }
    }
  }

  object->ValidateElements();
  return object;
}


static Handle<Object> CreateLiteralBoilerplate(
    Isolate* isolate,
    Handle<FixedArray> literals,
    Handle<FixedArray> array) {
  Handle<FixedArray> elements = CompileTimeValue::GetElements(array);
  const bool kHasNoFunctionLiteral = false;
  switch (CompileTimeValue::GetLiteralType(array)) {
    case CompileTimeValue::OBJECT_LITERAL_FAST_ELEMENTS:
      return CreateObjectLiteralBoilerplate(isolate,
                                            literals,
                                            elements,
                                            true,
                                            kHasNoFunctionLiteral);
    case CompileTimeValue::OBJECT_LITERAL_SLOW_ELEMENTS:
      return CreateObjectLiteralBoilerplate(isolate,
                                            literals,
                                            elements,
                                            false,
                                            kHasNoFunctionLiteral);
    case CompileTimeValue::ARRAY_LITERAL:
      return Runtime::CreateArrayLiteralBoilerplate(
          isolate, literals, elements);
    default:
      UNREACHABLE();
      return Handle<Object>::null();
  }
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_CreateObjectLiteral) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 4);
  CONVERT_ARG_HANDLE_CHECKED(FixedArray, literals, 0);
  CONVERT_SMI_ARG_CHECKED(literals_index, 1);
  CONVERT_ARG_HANDLE_CHECKED(FixedArray, constant_properties, 2);
  CONVERT_SMI_ARG_CHECKED(flags, 3);
  bool should_have_fast_elements = (flags & ObjectLiteral::kFastElements) != 0;
  bool has_function_literal = (flags & ObjectLiteral::kHasFunction) != 0;

  // Check if boilerplate exists. If not, create it first.
  Handle<Object> boilerplate(literals->get(literals_index), isolate);
  if (*boilerplate == isolate->heap()->undefined_value()) {
    boilerplate = CreateObjectLiteralBoilerplate(isolate,
                                                 literals,
                                                 constant_properties,
                                                 should_have_fast_elements,
                                                 has_function_literal);
    if (boilerplate.is_null()) return Failure::Exception();
    // Update the functions literal and return the boilerplate.
    literals->set(literals_index, *boilerplate);
  }
  return JSObject::cast(*boilerplate)->DeepCopy(isolate);
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_CreateObjectLiteralShallow) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 4);
  CONVERT_ARG_HANDLE_CHECKED(FixedArray, literals, 0);
  CONVERT_SMI_ARG_CHECKED(literals_index, 1);
  CONVERT_ARG_HANDLE_CHECKED(FixedArray, constant_properties, 2);
  CONVERT_SMI_ARG_CHECKED(flags, 3);
  bool should_have_fast_elements = (flags & ObjectLiteral::kFastElements) != 0;
  bool has_function_literal = (flags & ObjectLiteral::kHasFunction) != 0;

  // Check if boilerplate exists. If not, create it first.
  Handle<Object> boilerplate(literals->get(literals_index), isolate);
  if (*boilerplate == isolate->heap()->undefined_value()) {
    boilerplate = CreateObjectLiteralBoilerplate(isolate,
                                                 literals,
                                                 constant_properties,
                                                 should_have_fast_elements,
                                                 has_function_literal);
    if (boilerplate.is_null()) return Failure::Exception();
    // Update the functions literal and return the boilerplate.
    literals->set(literals_index, *boilerplate);
  }
  return isolate->heap()->CopyJSObject(JSObject::cast(*boilerplate));
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_CreateArrayLiteral) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 3);
  CONVERT_ARG_HANDLE_CHECKED(FixedArray, literals, 0);
  CONVERT_SMI_ARG_CHECKED(literals_index, 1);
  CONVERT_ARG_HANDLE_CHECKED(FixedArray, elements, 2);

  // Check if boilerplate exists. If not, create it first.
  Handle<Object> boilerplate(literals->get(literals_index), isolate);
  if (*boilerplate == isolate->heap()->undefined_value()) {
    ASSERT(*elements != isolate->heap()->empty_fixed_array());
    boilerplate =
        Runtime::CreateArrayLiteralBoilerplate(isolate, literals, elements);
    if (boilerplate.is_null()) return Failure::Exception();
    // Update the functions literal and return the boilerplate.
    literals->set(literals_index, *boilerplate);
  }
  return JSObject::cast(*boilerplate)->DeepCopy(isolate);
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_CreateArrayLiteralShallow) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 3);
  CONVERT_ARG_HANDLE_CHECKED(FixedArray, literals, 0);
  CONVERT_SMI_ARG_CHECKED(literals_index, 1);
  CONVERT_ARG_HANDLE_CHECKED(FixedArray, elements, 2);

  // Check if boilerplate exists. If not, create it first.
  Handle<Object> boilerplate(literals->get(literals_index), isolate);
  if (*boilerplate == isolate->heap()->undefined_value()) {
    ASSERT(*elements != isolate->heap()->empty_fixed_array());
    boilerplate =
        Runtime::CreateArrayLiteralBoilerplate(isolate, literals, elements);
    if (boilerplate.is_null()) return Failure::Exception();
    // Update the functions literal and return the boilerplate.
    literals->set(literals_index, *boilerplate);
  }
  if (JSObject::cast(*boilerplate)->elements()->map() ==
      isolate->heap()->fixed_cow_array_map()) {
    isolate->counters()->cow_arrays_created_runtime()->Increment();
  }

  JSObject* boilerplate_object = JSObject::cast(*boilerplate);
  AllocationSiteMode mode = AllocationSiteInfo::GetMode(
      boilerplate_object->GetElementsKind());
  if (mode == TRACK_ALLOCATION_SITE) {
    return isolate->heap()->CopyJSObjectWithAllocationSite(boilerplate_object);
  }

  return isolate->heap()->CopyJSObject(boilerplate_object);
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_CreateSymbol) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  Handle<Object> name(args[0], isolate);
  RUNTIME_ASSERT(name->IsString() || name->IsUndefined());
  Symbol* symbol;
  MaybeObject* maybe = isolate->heap()->AllocateSymbol();
  if (!maybe->To(&symbol)) return maybe;
  if (name->IsString()) symbol->set_name(*name);
  return symbol;
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_SymbolName) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_CHECKED(Symbol, symbol, 0);
  return symbol->name();
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_CreateJSProxy) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_CHECKED(JSReceiver, handler, 0);
  Object* prototype = args[1];
  Object* used_prototype =
      prototype->IsJSReceiver() ? prototype : isolate->heap()->null_value();
  return isolate->heap()->AllocateJSProxy(handler, used_prototype);
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_CreateJSFunctionProxy) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 4);
  CONVERT_ARG_CHECKED(JSReceiver, handler, 0);
  Object* call_trap = args[1];
  RUNTIME_ASSERT(call_trap->IsJSFunction() || call_trap->IsJSFunctionProxy());
  CONVERT_ARG_CHECKED(JSFunction, construct_trap, 2);
  Object* prototype = args[3];
  Object* used_prototype =
      prototype->IsJSReceiver() ? prototype : isolate->heap()->null_value();
  return isolate->heap()->AllocateJSFunctionProxy(
      handler, call_trap, construct_trap, used_prototype);
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_IsJSProxy) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  Object* obj = args[0];
  return isolate->heap()->ToBoolean(obj->IsJSProxy());
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_IsJSFunctionProxy) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  Object* obj = args[0];
  return isolate->heap()->ToBoolean(obj->IsJSFunctionProxy());
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_GetHandler) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_CHECKED(JSProxy, proxy, 0);
  return proxy->handler();
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_GetCallTrap) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_CHECKED(JSFunctionProxy, proxy, 0);
  return proxy->call_trap();
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_GetConstructTrap) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_CHECKED(JSFunctionProxy, proxy, 0);
  return proxy->construct_trap();
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_Fix) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_CHECKED(JSProxy, proxy, 0);
  proxy->Fix();
  return isolate->heap()->undefined_value();
}


void Runtime::FreeArrayBuffer(Isolate* isolate,
                              JSArrayBuffer* phantom_array_buffer) {
  if (phantom_array_buffer->is_external()) return;

  size_t allocated_length = NumberToSize(
      isolate, phantom_array_buffer->byte_length());

  isolate->heap()->AdjustAmountOfExternalAllocatedMemory(
      -static_cast<intptr_t>(allocated_length));
  CHECK(V8::ArrayBufferAllocator() != NULL);
  V8::ArrayBufferAllocator()->Free(phantom_array_buffer->backing_store());
}


void Runtime::SetupArrayBuffer(Isolate* isolate,
                               Handle<JSArrayBuffer> array_buffer,
                               bool is_external,
                               void* data,
                               size_t allocated_length) {
  ASSERT(array_buffer->GetInternalFieldCount() ==
      v8::ArrayBuffer::kInternalFieldCount);
  for (int i = 0; i < v8::ArrayBuffer::kInternalFieldCount; i++) {
    array_buffer->SetInternalField(i, Smi::FromInt(0));
  }
  array_buffer->set_backing_store(data);
  array_buffer->set_flag(Smi::FromInt(0));
  array_buffer->set_is_external(is_external);

  Handle<Object> byte_length =
      isolate->factory()->NewNumberFromSize(allocated_length);
  CHECK(byte_length->IsSmi() || byte_length->IsHeapNumber());
  array_buffer->set_byte_length(*byte_length);

  array_buffer->set_weak_next(isolate->heap()->array_buffers_list());
  isolate->heap()->set_array_buffers_list(*array_buffer);
  array_buffer->set_weak_first_view(isolate->heap()->undefined_value());
}


bool Runtime::SetupArrayBufferAllocatingData(
    Isolate* isolate,
    Handle<JSArrayBuffer> array_buffer,
    size_t allocated_length) {
  void* data;
  CHECK(V8::ArrayBufferAllocator() != NULL);
  if (allocated_length != 0) {
    data = V8::ArrayBufferAllocator()->Allocate(allocated_length);
    if (data == NULL) return false;
    memset(data, 0, allocated_length);
  } else {
    data = NULL;
  }

  SetupArrayBuffer(isolate, array_buffer, false, data, allocated_length);

  isolate->heap()->AdjustAmountOfExternalAllocatedMemory(allocated_length);

  return true;
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_ArrayBufferInitialize) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSArrayBuffer, holder, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, byteLength, 1);
  size_t allocated_length;
  if (byteLength->IsSmi()) {
    allocated_length = Smi::cast(*byteLength)->value();
  } else {
    ASSERT(byteLength->IsHeapNumber());
    double value = HeapNumber::cast(*byteLength)->value();

    ASSERT(value >= 0);

    if (value > std::numeric_limits<size_t>::max()) {
      return isolate->Throw(
          *isolate->factory()->NewRangeError("invalid_array_buffer_length",
            HandleVector<Object>(NULL, 0)));
    }

    allocated_length = static_cast<size_t>(value);
  }

  if (!Runtime::SetupArrayBufferAllocatingData(isolate,
                                               holder, allocated_length)) {
      return isolate->Throw(*isolate->factory()->
          NewRangeError("invalid_array_buffer_length",
            HandleVector<Object>(NULL, 0)));
  }

  return *holder;
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_ArrayBufferGetByteLength) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_CHECKED(JSArrayBuffer, holder, 0);
  return holder->byte_length();
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_ArrayBufferSliceImpl) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 3);
  CONVERT_ARG_HANDLE_CHECKED(JSArrayBuffer, source, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSArrayBuffer, target, 1);
  CONVERT_DOUBLE_ARG_CHECKED(first, 2);
  size_t start = static_cast<size_t>(first);
  size_t target_length = NumberToSize(isolate, target->byte_length());

  if (target_length == 0) return isolate->heap()->undefined_value();

  ASSERT(NumberToSize(isolate, source->byte_length()) - target_length >= start);
  uint8_t* source_data = reinterpret_cast<uint8_t*>(source->backing_store());
  uint8_t* target_data = reinterpret_cast<uint8_t*>(target->backing_store());
  CopyBytes(target_data, source_data + start, target_length);
  return isolate->heap()->undefined_value();
}


enum TypedArrayId {
  // arrayIds below should be synchromized with typedarray.js natives.
  ARRAY_ID_UINT8 = 1,
  ARRAY_ID_INT8 = 2,
  ARRAY_ID_UINT16 = 3,
  ARRAY_ID_INT16 = 4,
  ARRAY_ID_UINT32 = 5,
  ARRAY_ID_INT32 = 6,
  ARRAY_ID_FLOAT32 = 7,
  ARRAY_ID_FLOAT64 = 8,
  ARRAY_ID_UINT8C = 9
};


RUNTIME_FUNCTION(MaybeObject*, Runtime_TypedArrayInitialize) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 5);
  CONVERT_ARG_HANDLE_CHECKED(JSTypedArray, holder, 0);
  CONVERT_SMI_ARG_CHECKED(arrayId, 1);
  CONVERT_ARG_HANDLE_CHECKED(JSArrayBuffer, buffer, 2);
  CONVERT_ARG_HANDLE_CHECKED(Object, byte_offset_object, 3);
  CONVERT_ARG_HANDLE_CHECKED(Object, byte_length_object, 4);

  ExternalArrayType arrayType;
  size_t elementSize;
  switch (arrayId) {
    case ARRAY_ID_UINT8:
      arrayType = kExternalUnsignedByteArray;
      elementSize = 1;
      break;
    case ARRAY_ID_INT8:
      arrayType = kExternalByteArray;
      elementSize = 1;
      break;
    case ARRAY_ID_UINT16:
      arrayType = kExternalUnsignedShortArray;
      elementSize = 2;
      break;
    case ARRAY_ID_INT16:
      arrayType = kExternalShortArray;
      elementSize = 2;
      break;
    case ARRAY_ID_UINT32:
      arrayType = kExternalUnsignedIntArray;
      elementSize = 4;
      break;
    case ARRAY_ID_INT32:
      arrayType = kExternalIntArray;
      elementSize = 4;
      break;
    case ARRAY_ID_FLOAT32:
      arrayType = kExternalFloatArray;
      elementSize = 4;
      break;
    case ARRAY_ID_FLOAT64:
      arrayType = kExternalDoubleArray;
      elementSize = 8;
      break;
    case ARRAY_ID_UINT8C:
      arrayType = kExternalPixelArray;
      elementSize = 1;
      break;
    default:
      UNREACHABLE();
      return NULL;
  }

  holder->set_buffer(*buffer);
  holder->set_byte_offset(*byte_offset_object);
  holder->set_byte_length(*byte_length_object);

  size_t byte_offset = NumberToSize(isolate, *byte_offset_object);
  size_t byte_length = NumberToSize(isolate, *byte_length_object);
  ASSERT(byte_length % elementSize == 0);
  size_t length = byte_length / elementSize;

  Handle<Object> length_obj = isolate->factory()->NewNumberFromSize(length);
  holder->set_length(*length_obj);
  holder->set_weak_next(buffer->weak_first_view());
  buffer->set_weak_first_view(*holder);

  Handle<ExternalArray> elements =
      isolate->factory()->NewExternalArray(
          static_cast<int>(length), arrayType,
          static_cast<uint8_t*>(buffer->backing_store()) + byte_offset);
  holder->set_elements(*elements);
  return isolate->heap()->undefined_value();
}


#define TYPED_ARRAY_GETTER(getter, accessor) \
  RUNTIME_FUNCTION(MaybeObject*, Runtime_TypedArrayGet##getter) {             \
    HandleScope scope(isolate);                                               \
    ASSERT(args.length() == 1);                                               \
    CONVERT_ARG_HANDLE_CHECKED(Object, holder, 0);                            \
    if (!holder->IsJSTypedArray())                                            \
      return isolate->Throw(*isolate->factory()->NewTypeError(                \
          "not_typed_array", HandleVector<Object>(NULL, 0)));                 \
    Handle<JSTypedArray> typed_array(JSTypedArray::cast(*holder));            \
    return typed_array->accessor();                                           \
  }

TYPED_ARRAY_GETTER(Buffer, buffer)
TYPED_ARRAY_GETTER(ByteLength, byte_length)
TYPED_ARRAY_GETTER(ByteOffset, byte_offset)
TYPED_ARRAY_GETTER(Length, length)

#undef TYPED_ARRAY_GETTER

RUNTIME_FUNCTION(MaybeObject*, Runtime_TypedArraySetFastCases) {
  HandleScope scope(isolate);
  CONVERT_ARG_HANDLE_CHECKED(Object, target_obj, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, source_obj, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, offset_obj, 2);

  if (!target_obj->IsJSTypedArray())
    return isolate->Throw(*isolate->factory()->NewTypeError(
        "not_typed_array", HandleVector<Object>(NULL, 0)));

  if (!source_obj->IsJSTypedArray())
    return isolate->heap()->false_value();

  Handle<JSTypedArray> target(JSTypedArray::cast(*target_obj));
  Handle<JSTypedArray> source(JSTypedArray::cast(*source_obj));
  size_t offset = NumberToSize(isolate, *offset_obj);
  size_t target_length = NumberToSize(isolate, target->length());
  size_t source_length = NumberToSize(isolate, source->length());
  size_t target_byte_length = NumberToSize(isolate, target->byte_length());
  size_t source_byte_length = NumberToSize(isolate, source->byte_length());
  if (offset > target_length ||
      offset + source_length > target_length ||
      offset + source_length < offset)  // overflow
    return isolate->Throw(*isolate->factory()->NewRangeError(
          "typed_array_set_source_too_large", HandleVector<Object>(NULL, 0)));

  Handle<JSArrayBuffer> target_buffer(JSArrayBuffer::cast(target->buffer()));
  Handle<JSArrayBuffer> source_buffer(JSArrayBuffer::cast(source->buffer()));
  size_t target_offset = NumberToSize(isolate, target->byte_offset());
  size_t source_offset = NumberToSize(isolate, source->byte_offset());
  uint8_t* target_base =
      static_cast<uint8_t*>(target_buffer->backing_store()) + target_offset;
  uint8_t* source_base =
      static_cast<uint8_t*>(source_buffer->backing_store()) + source_offset;

  // Typed arrays of the same type: use memmove.
  if (target->type() == source->type()) {
    memmove(target_base + offset * target->element_size(),
        source_base, source_byte_length);
    return isolate->heap()->true_value();
  }

  // Typed arrays of different types over the same backing store
  if ((source_base <= target_base &&
        source_base + source_byte_length > target_base) ||
      (target_base <= source_base &&
        target_base + target_byte_length > source_base)) {
    size_t target_element_size = target->element_size();
    size_t source_element_size = source->element_size();

    size_t source_length = NumberToSize(isolate, source->length());

    // Copy left part
    size_t left_index;
    {
      // First un-mutated byte after the next write
      uint8_t* target_ptr = target_base + (offset + 1) * target_element_size;
      // Next read at source_ptr. We do not care for memory changing before
      // source_ptr - we have already copied it.
      uint8_t* source_ptr = source_base;
      for (left_index = 0;
           left_index < source_length && target_ptr <= source_ptr;
           left_index++) {
        Handle<Object> v = Object::GetElement(
            source, static_cast<uint32_t>(left_index));
        JSObject::SetElement(
            target, static_cast<uint32_t>(offset + left_index), v,
            NONE, kNonStrictMode);
        target_ptr += target_element_size;
        source_ptr += source_element_size;
      }
    }
    // Copy right part
    size_t right_index;
    {
      // First unmutated byte before the next write
      uint8_t* target_ptr =
        target_base + (offset + source_length - 1) * target_element_size;
      // Next read before source_ptr. We do not care for memory changing after
      // source_ptr - we have already copied it.
      uint8_t* source_ptr =
        source_base + source_length * source_element_size;
      for (right_index = source_length - 1;
           right_index >= left_index && target_ptr >= source_ptr;
           right_index--) {
        Handle<Object> v = Object::GetElement(
            source, static_cast<uint32_t>(right_index));
        JSObject::SetElement(
            target, static_cast<uint32_t>(offset + right_index), v,
            NONE, kNonStrictMode);
        target_ptr -= target_element_size;
        source_ptr -= source_element_size;
      }
    }
    // There can be at most 8 entries left in the middle that need buffering
    // (because the largest element_size is 8 times the smallest).
    ASSERT((right_index + 1) - left_index <= 8);
    Handle<Object> temp[8];
    size_t idx;
    for (idx = left_index; idx <= right_index; idx++) {
      temp[idx - left_index] = Object::GetElement(
          source, static_cast<uint32_t>(idx));
    }
    for (idx = left_index; idx <= right_index; idx++) {
      JSObject::SetElement(
          target, static_cast<uint32_t>(offset + idx), temp[idx-left_index],
          NONE, kNonStrictMode);
    }
  } else {  // Non-overlapping typed arrays
    for (size_t idx = 0; idx < source_length; idx++) {
      Handle<Object> value = Object::GetElement(
          source, static_cast<uint32_t>(idx));
      JSObject::SetElement(
          target, static_cast<uint32_t>(offset + idx), value,
          NONE, kNonStrictMode);
    }
  }

  return isolate->heap()->true_value();
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_DataViewInitialize) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 4);
  CONVERT_ARG_HANDLE_CHECKED(JSDataView, holder, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSArrayBuffer, buffer, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, byte_offset, 2);
  CONVERT_ARG_HANDLE_CHECKED(Object, byte_length, 3);

  holder->set_buffer(*buffer);
  ASSERT(byte_offset->IsNumber());
  ASSERT(
      NumberToSize(isolate, buffer->byte_length()) >=
        NumberToSize(isolate, *byte_offset)
        + NumberToSize(isolate, *byte_length));
  holder->set_byte_offset(*byte_offset);
  ASSERT(byte_length->IsNumber());
  holder->set_byte_length(*byte_length);

  holder->set_weak_next(buffer->weak_first_view());
  buffer->set_weak_first_view(*holder);

  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_DataViewGetBuffer) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSDataView, data_view, 0);
  return data_view->buffer();
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_DataViewGetByteOffset) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSDataView, data_view, 0);
  return data_view->byte_offset();
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_DataViewGetByteLength) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSDataView, data_view, 0);
  return data_view->byte_length();
}


inline static bool NeedToFlipBytes(bool is_little_endian) {
#ifdef V8_TARGET_LITTLE_ENDIAN
  return !is_little_endian;
#else
  return is_little_endian;
#endif
}


template<int n>
inline void CopyBytes(uint8_t* target, uint8_t* source) {
  for (int i = 0; i < n; i++) {
    *(target++) = *(source++);
  }
}


template<int n>
inline void FlipBytes(uint8_t* target, uint8_t* source) {
  source = source + (n-1);
  for (int i = 0; i < n; i++) {
    *(target++) = *(source--);
  }
}


template<typename T>
inline static bool DataViewGetValue(
    Isolate* isolate,
    Handle<JSDataView> data_view,
    Handle<Object> byte_offset_obj,
    bool is_little_endian,
    T* result) {
  size_t byte_offset = NumberToSize(isolate, *byte_offset_obj);
  Handle<JSArrayBuffer> buffer(JSArrayBuffer::cast(data_view->buffer()));

  size_t data_view_byte_offset =
      NumberToSize(isolate, data_view->byte_offset());
  size_t data_view_byte_length =
      NumberToSize(isolate, data_view->byte_length());
  if (byte_offset + sizeof(T) > data_view_byte_length ||
      byte_offset + sizeof(T) < byte_offset)  {  // overflow
    return false;
  }

  union Value {
    T data;
    uint8_t bytes[sizeof(T)];
  };

  Value value;
  size_t buffer_offset = data_view_byte_offset + byte_offset;
  ASSERT(
      NumberToSize(isolate, buffer->byte_length())
      >= buffer_offset + sizeof(T));
  uint8_t* source =
        static_cast<uint8_t*>(buffer->backing_store()) + buffer_offset;
  if (NeedToFlipBytes(is_little_endian)) {
    FlipBytes<sizeof(T)>(value.bytes, source);
  } else {
    CopyBytes<sizeof(T)>(value.bytes, source);
  }
  *result = value.data;
  return true;
}


template<typename T>
static bool DataViewSetValue(
    Isolate* isolate,
    Handle<JSDataView> data_view,
    Handle<Object> byte_offset_obj,
    bool is_little_endian,
    T data) {
  size_t byte_offset = NumberToSize(isolate, *byte_offset_obj);
  Handle<JSArrayBuffer> buffer(JSArrayBuffer::cast(data_view->buffer()));

  size_t data_view_byte_offset =
      NumberToSize(isolate, data_view->byte_offset());
  size_t data_view_byte_length =
      NumberToSize(isolate, data_view->byte_length());
  if (byte_offset + sizeof(T) > data_view_byte_length ||
      byte_offset + sizeof(T) < byte_offset)  {  // overflow
    return false;
  }

  union Value {
    T data;
    uint8_t bytes[sizeof(T)];
  };

  Value value;
  value.data = data;
  size_t buffer_offset = data_view_byte_offset + byte_offset;
  ASSERT(
      NumberToSize(isolate, buffer->byte_length())
      >= buffer_offset + sizeof(T));
  uint8_t* target =
        static_cast<uint8_t*>(buffer->backing_store()) + buffer_offset;
  if (NeedToFlipBytes(is_little_endian)) {
    FlipBytes<sizeof(T)>(target, value.bytes);
  } else {
    CopyBytes<sizeof(T)>(target, value.bytes);
  }
  return true;
}


#define DATA_VIEW_GETTER(TypeName, Type, Converter)                           \
  RUNTIME_FUNCTION(MaybeObject*, Runtime_DataViewGet##TypeName) {             \
    HandleScope scope(isolate);                                               \
    ASSERT(args.length() == 3);                                               \
    CONVERT_ARG_HANDLE_CHECKED(JSDataView, holder, 0);                        \
    CONVERT_ARG_HANDLE_CHECKED(Object, offset, 1);                            \
    CONVERT_BOOLEAN_ARG_CHECKED(is_little_endian, 2);                         \
    Type result;                                                              \
    if (DataViewGetValue(                                                     \
          isolate, holder, offset, is_little_endian, &result)) {              \
      return isolate->heap()->Converter(result);                              \
    } else {                                                                  \
      return isolate->Throw(*isolate->factory()->NewRangeError(               \
          "invalid_data_view_accessor_offset",                                \
          HandleVector<Object>(NULL, 0)));                                    \
    }                                                                         \
  }

DATA_VIEW_GETTER(Uint8, uint8_t, NumberFromUint32)
DATA_VIEW_GETTER(Int8, int8_t, NumberFromInt32)
DATA_VIEW_GETTER(Uint16, uint16_t, NumberFromUint32)
DATA_VIEW_GETTER(Int16, int16_t, NumberFromInt32)
DATA_VIEW_GETTER(Uint32, uint32_t, NumberFromUint32)
DATA_VIEW_GETTER(Int32, int32_t, NumberFromInt32)
DATA_VIEW_GETTER(Float32, float, NumberFromDouble)
DATA_VIEW_GETTER(Float64, double, NumberFromDouble)

#undef DATA_VIEW_GETTER

#define DATA_VIEW_SETTER(TypeName, Type)                                      \
  RUNTIME_FUNCTION(MaybeObject*, Runtime_DataViewSet##TypeName) {             \
    HandleScope scope(isolate);                                               \
    ASSERT(args.length() == 4);                                               \
    CONVERT_ARG_HANDLE_CHECKED(JSDataView, holder, 0);                        \
    CONVERT_ARG_HANDLE_CHECKED(Object, offset, 1);                            \
    CONVERT_ARG_HANDLE_CHECKED(Object, value, 2);                             \
    CONVERT_BOOLEAN_ARG_CHECKED(is_little_endian, 3);                         \
    Type v = static_cast<Type>(value->Number());                              \
    if (DataViewSetValue(                                                     \
          isolate, holder, offset, is_little_endian, v)) {                    \
      return isolate->heap()->undefined_value();                              \
    } else {                                                                  \
      return isolate->Throw(*isolate->factory()->NewRangeError(               \
          "invalid_data_view_accessor_offset",                                \
          HandleVector<Object>(NULL, 0)));                                    \
    }                                                                         \
  }

DATA_VIEW_SETTER(Uint8, uint8_t)
DATA_VIEW_SETTER(Int8, int8_t)
DATA_VIEW_SETTER(Uint16, uint16_t)
DATA_VIEW_SETTER(Int16, int16_t)
DATA_VIEW_SETTER(Uint32, uint32_t)
DATA_VIEW_SETTER(Int32, int32_t)
DATA_VIEW_SETTER(Float32, float)
DATA_VIEW_SETTER(Float64, double)

#undef DATA_VIEW_SETTER


RUNTIME_FUNCTION(MaybeObject*, Runtime_SetInitialize) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSSet, holder, 0);
  Handle<ObjectHashSet> table = isolate->factory()->NewObjectHashSet(0);
  holder->set_table(*table);
  return *holder;
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_SetAdd) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSSet, holder, 0);
  Handle<Object> key(args[1], isolate);
  Handle<ObjectHashSet> table(ObjectHashSet::cast(holder->table()));
  table = ObjectHashSetAdd(table, key);
  holder->set_table(*table);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_SetHas) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSSet, holder, 0);
  Handle<Object> key(args[1], isolate);
  Handle<ObjectHashSet> table(ObjectHashSet::cast(holder->table()));
  return isolate->heap()->ToBoolean(table->Contains(*key));
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_SetDelete) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSSet, holder, 0);
  Handle<Object> key(args[1], isolate);
  Handle<ObjectHashSet> table(ObjectHashSet::cast(holder->table()));
  table = ObjectHashSetRemove(table, key);
  holder->set_table(*table);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_SetGetSize) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSSet, holder, 0);
  Handle<ObjectHashSet> table(ObjectHashSet::cast(holder->table()));
  return Smi::FromInt(table->NumberOfElements());
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_MapInitialize) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSMap, holder, 0);
  Handle<ObjectHashTable> table = isolate->factory()->NewObjectHashTable(0);
  holder->set_table(*table);
  return *holder;
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_MapGet) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSMap, holder, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 1);
  Handle<ObjectHashTable> table(ObjectHashTable::cast(holder->table()));
  Handle<Object> lookup(table->Lookup(*key), isolate);
  return lookup->IsTheHole() ? isolate->heap()->undefined_value() : *lookup;
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_MapHas) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSMap, holder, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 1);
  Handle<ObjectHashTable> table(ObjectHashTable::cast(holder->table()));
  Handle<Object> lookup(table->Lookup(*key), isolate);
  return isolate->heap()->ToBoolean(!lookup->IsTheHole());
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_MapDelete) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSMap, holder, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 1);
  Handle<ObjectHashTable> table(ObjectHashTable::cast(holder->table()));
  Handle<Object> lookup(table->Lookup(*key), isolate);
  Handle<ObjectHashTable> new_table =
      PutIntoObjectHashTable(table, key, isolate->factory()->the_hole_value());
  holder->set_table(*new_table);
  return isolate->heap()->ToBoolean(!lookup->IsTheHole());
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_MapSet) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 3);
  CONVERT_ARG_HANDLE_CHECKED(JSMap, holder, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 2);
  Handle<ObjectHashTable> table(ObjectHashTable::cast(holder->table()));
  Handle<ObjectHashTable> new_table = PutIntoObjectHashTable(table, key, value);
  holder->set_table(*new_table);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_MapGetSize) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSMap, holder, 0);
  Handle<ObjectHashTable> table(ObjectHashTable::cast(holder->table()));
  return Smi::FromInt(table->NumberOfElements());
}


static JSWeakMap* WeakMapInitialize(Isolate* isolate,
                                    Handle<JSWeakMap> weakmap) {
  ASSERT(weakmap->map()->inobject_properties() == 0);
  Handle<ObjectHashTable> table = isolate->factory()->NewObjectHashTable(0);
  weakmap->set_table(*table);
  weakmap->set_next(Smi::FromInt(0));
  return *weakmap;
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_WeakMapInitialize) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSWeakMap, weakmap, 0);
  return WeakMapInitialize(isolate, weakmap);
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_WeakMapGet) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSWeakMap, weakmap, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 1);
  Handle<ObjectHashTable> table(ObjectHashTable::cast(weakmap->table()));
  Handle<Object> lookup(table->Lookup(*key), isolate);
  return lookup->IsTheHole() ? isolate->heap()->undefined_value() : *lookup;
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_WeakMapHas) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSWeakMap, weakmap, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 1);
  Handle<ObjectHashTable> table(ObjectHashTable::cast(weakmap->table()));
  Handle<Object> lookup(table->Lookup(*key), isolate);
  return isolate->heap()->ToBoolean(!lookup->IsTheHole());
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_WeakMapDelete) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSWeakMap, weakmap, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 1);
  Handle<ObjectHashTable> table(ObjectHashTable::cast(weakmap->table()));
  Handle<Object> lookup(table->Lookup(*key), isolate);
  Handle<ObjectHashTable> new_table =
      PutIntoObjectHashTable(table, key, isolate->factory()->the_hole_value());
  weakmap->set_table(*new_table);
  return isolate->heap()->ToBoolean(!lookup->IsTheHole());
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_WeakMapSet) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 3);
  CONVERT_ARG_HANDLE_CHECKED(JSWeakMap, weakmap, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 1);
  Handle<Object> value(args[2], isolate);
  Handle<ObjectHashTable> table(ObjectHashTable::cast(weakmap->table()));
  Handle<ObjectHashTable> new_table = PutIntoObjectHashTable(table, key, value);
  weakmap->set_table(*new_table);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_ClassOf) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  Object* obj = args[0];
  if (!obj->IsJSObject()) return isolate->heap()->null_value();
  return JSObject::cast(obj)->class_name();
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_GetPrototype) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_CHECKED(Object, obj, 0);
  // We don't expect access checks to be needed on JSProxy objects.
  ASSERT(!obj->IsAccessCheckNeeded() || obj->IsJSObject());
  do {
    if (obj->IsAccessCheckNeeded() &&
        !isolate->MayNamedAccess(JSObject::cast(obj),
                                 isolate->heap()->proto_string(),
                                 v8::ACCESS_GET)) {
      isolate->ReportFailedAccessCheck(JSObject::cast(obj), v8::ACCESS_GET);
      return isolate->heap()->undefined_value();
    }
    obj = obj->GetPrototype(isolate);
  } while (obj->IsJSObject() &&
           JSObject::cast(obj)->map()->is_hidden_prototype());
  return obj;
}


static inline Object* GetPrototypeSkipHiddenPrototypes(Isolate* isolate,
                                                       Object* receiver) {
  Object* current = receiver->GetPrototype(isolate);
  while (current->IsJSObject() &&
         JSObject::cast(current)->map()->is_hidden_prototype()) {
    current = current->GetPrototype(isolate);
  }
  return current;
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_SetPrototype) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, obj, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, prototype, 1);
  if (FLAG_harmony_observation && obj->map()->is_observed()) {
    Handle<Object> old_value(
        GetPrototypeSkipHiddenPrototypes(isolate, *obj), isolate);

    Handle<Object> result = JSObject::SetPrototype(obj, prototype, true);
    if (result.is_null()) return Failure::Exception();

    Handle<Object> new_value(
        GetPrototypeSkipHiddenPrototypes(isolate, *obj), isolate);
    if (!new_value->SameValue(*old_value)) {
      JSObject::EnqueueChangeRecord(obj, "prototype",
                                    isolate->factory()->proto_string(),
                                    old_value);
    }
    return *result;
  }
  Handle<Object> result = JSObject::SetPrototype(obj, prototype, true);
  if (result.is_null()) return Failure::Exception();
  return *result;
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_IsInPrototypeChain) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 2);
  // See ECMA-262, section 15.3.5.3, page 88 (steps 5 - 8).
  Object* O = args[0];
  Object* V = args[1];
  while (true) {
    Object* prototype = V->GetPrototype(isolate);
    if (prototype->IsNull()) return isolate->heap()->false_value();
    if (O == prototype) return isolate->heap()->true_value();
    V = prototype;
  }
}


static bool CheckAccessException(Object* callback,
                                 v8::AccessType access_type) {
  if (callback->IsAccessorInfo()) {
    AccessorInfo* info = AccessorInfo::cast(callback);
    return
        (access_type == v8::ACCESS_HAS &&
           (info->all_can_read() || info->all_can_write())) ||
        (access_type == v8::ACCESS_GET && info->all_can_read()) ||
        (access_type == v8::ACCESS_SET && info->all_can_write());
  }
  return false;
}


template<class Key>
static bool CheckGenericAccess(
    JSObject* receiver,
    JSObject* holder,
    Key key,
    v8::AccessType access_type,
    bool (Isolate::*mayAccess)(JSObject*, Key, v8::AccessType)) {
  Isolate* isolate = receiver->GetIsolate();
  for (JSObject* current = receiver;
       true;
       current = JSObject::cast(current->GetPrototype())) {
    if (current->IsAccessCheckNeeded() &&
        !(isolate->*mayAccess)(current, key, access_type)) {
      return false;
    }
    if (current == holder) break;
  }
  return true;
}


enum AccessCheckResult {
  ACCESS_FORBIDDEN,
  ACCESS_ALLOWED,
  ACCESS_ABSENT
};


static AccessCheckResult CheckElementAccess(
    JSObject* obj,
    uint32_t index,
    v8::AccessType access_type) {
  // TODO(1095): we should traverse hidden prototype hierachy as well.
  if (CheckGenericAccess(
          obj, obj, index, access_type, &Isolate::MayIndexedAccess)) {
    return ACCESS_ALLOWED;
  }

  obj->GetIsolate()->ReportFailedAccessCheck(obj, access_type);
  return ACCESS_FORBIDDEN;
}


static AccessCheckResult CheckPropertyAccess(
    JSObject* obj,
    Name* name,
    v8::AccessType access_type) {
  uint32_t index;
  if (name->AsArrayIndex(&index)) {
    return CheckElementAccess(obj, index, access_type);
  }

  LookupResult lookup(obj->GetIsolate());
  obj->LocalLookup(name, &lookup, true);

  if (!lookup.IsProperty()) return ACCESS_ABSENT;
  if (CheckGenericAccess<Object*>(
          obj, lookup.holder(), name, access_type, &Isolate::MayNamedAccess)) {
    return ACCESS_ALLOWED;
  }

  // Access check callback denied the access, but some properties
  // can have a special permissions which override callbacks descision
  // (currently see v8::AccessControl).
  // API callbacks can have per callback access exceptions.
  switch (lookup.type()) {
    case CALLBACKS:
      if (CheckAccessException(lookup.GetCallbackObject(), access_type)) {
        return ACCESS_ALLOWED;
      }
      break;
    case INTERCEPTOR:
      // If the object has an interceptor, try real named properties.
      // Overwrite the result to fetch the correct property later.
      lookup.holder()->LookupRealNamedProperty(name, &lookup);
      if (lookup.IsProperty() && lookup.IsPropertyCallbacks()) {
        if (CheckAccessException(lookup.GetCallbackObject(), access_type)) {
          return ACCESS_ALLOWED;
        }
      }
      break;
    default:
      break;
  }

  obj->GetIsolate()->ReportFailedAccessCheck(obj, access_type);
  return ACCESS_FORBIDDEN;
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


static MaybeObject* GetOwnProperty(Isolate* isolate,
                                   Handle<JSObject> obj,
                                   Handle<Name> name) {
  Heap* heap = isolate->heap();
  // Due to some WebKit tests, we want to make sure that we do not log
  // more than one access failure here.
  switch (CheckPropertyAccess(*obj, *name, v8::ACCESS_HAS)) {
    case ACCESS_FORBIDDEN: return heap->false_value();
    case ACCESS_ALLOWED: break;
    case ACCESS_ABSENT: return heap->undefined_value();
  }

  PropertyAttributes attrs = obj->GetLocalPropertyAttribute(*name);
  if (attrs == ABSENT) return heap->undefined_value();
  AccessorPair* raw_accessors = obj->GetLocalPropertyAccessorPair(*name);
  Handle<AccessorPair> accessors(raw_accessors, isolate);

  Handle<FixedArray> elms = isolate->factory()->NewFixedArray(DESCRIPTOR_SIZE);
  elms->set(ENUMERABLE_INDEX, heap->ToBoolean((attrs & DONT_ENUM) == 0));
  elms->set(CONFIGURABLE_INDEX, heap->ToBoolean((attrs & DONT_DELETE) == 0));
  elms->set(IS_ACCESSOR_INDEX, heap->ToBoolean(raw_accessors != NULL));

  if (raw_accessors == NULL) {
    elms->set(WRITABLE_INDEX, heap->ToBoolean((attrs & READ_ONLY) == 0));
    // GetProperty does access check.
    Handle<Object> value = GetProperty(isolate, obj, name);
    if (value.is_null()) return Failure::Exception();
    elms->set(VALUE_INDEX, *value);
  } else {
    // Access checks are performed for both accessors separately.
    // When they fail, the respective field is not set in the descriptor.
    Object* getter = accessors->GetComponent(ACCESSOR_GETTER);
    Object* setter = accessors->GetComponent(ACCESSOR_SETTER);
    if (!getter->IsMap() && CheckPropertyAccess(*obj, *name, v8::ACCESS_GET)) {
      elms->set(GETTER_INDEX, getter);
    }
    if (!setter->IsMap() && CheckPropertyAccess(*obj, *name, v8::ACCESS_SET)) {
      elms->set(SETTER_INDEX, setter);
    }
  }

  return *isolate->factory()->NewJSArrayWithElements(elms);
}


// Returns an array with the property description:
//  if args[1] is not a property on args[0]
//          returns undefined
//  if args[1] is a data property on args[0]
//         [false, value, Writeable, Enumerable, Configurable]
//  if args[1] is an accessor on args[0]
//         [true, GetFunction, SetFunction, Enumerable, Configurable]
RUNTIME_FUNCTION(MaybeObject*, Runtime_GetOwnProperty) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, obj, 0);
  CONVERT_ARG_HANDLE_CHECKED(Name, name, 1);
  return GetOwnProperty(isolate, obj, name);
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_PreventExtensions) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_CHECKED(JSObject, obj, 0);
  return obj->PreventExtensions();
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_IsExtensible) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_CHECKED(JSObject, obj, 0);
  if (obj->IsJSGlobalProxy()) {
    Object* proto = obj->GetPrototype();
    if (proto->IsNull()) return isolate->heap()->false_value();
    ASSERT(proto->IsJSGlobalObject());
    obj = JSObject::cast(proto);
  }
  return isolate->heap()->ToBoolean(obj->map()->is_extensible());
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_RegExpCompile) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 3);
  CONVERT_ARG_HANDLE_CHECKED(JSRegExp, re, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, pattern, 1);
  CONVERT_ARG_HANDLE_CHECKED(String, flags, 2);
  Handle<Object> result =
      RegExpImpl::Compile(re, pattern, flags);
  if (result.is_null()) return Failure::Exception();
  return *result;
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_CreateApiFunction) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(FunctionTemplateInfo, data, 0);
  return *isolate->factory()->CreateApiFunction(data);
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_IsTemplate) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  Object* arg = args[0];
  bool result = arg->IsObjectTemplateInfo() || arg->IsFunctionTemplateInfo();
  return isolate->heap()->ToBoolean(result);
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_GetTemplateField) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_CHECKED(HeapObject, templ, 0);
  CONVERT_SMI_ARG_CHECKED(index, 1)
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


RUNTIME_FUNCTION(MaybeObject*, Runtime_DisableAccessChecks) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_CHECKED(HeapObject, object, 0);
  Map* old_map = object->map();
  bool needs_access_checks = old_map->is_access_check_needed();
  if (needs_access_checks) {
    // Copy map so it won't interfere constructor's initial map.
    Map* new_map;
    MaybeObject* maybe_new_map = old_map->Copy();
    if (!maybe_new_map->To(&new_map)) return maybe_new_map;

    new_map->set_is_access_check_needed(false);
    object->set_map(new_map);
  }
  return isolate->heap()->ToBoolean(needs_access_checks);
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_EnableAccessChecks) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_CHECKED(HeapObject, object, 0);
  Map* old_map = object->map();
  if (!old_map->is_access_check_needed()) {
    // Copy map so it won't interfere constructor's initial map.
    Map* new_map;
    MaybeObject* maybe_new_map = old_map->Copy();
    if (!maybe_new_map->To(&new_map)) return maybe_new_map;

    new_map->set_is_access_check_needed(true);
    object->set_map(new_map);
  }
  return isolate->heap()->undefined_value();
}


static Failure* ThrowRedeclarationError(Isolate* isolate,
                                        const char* type,
                                        Handle<String> name) {
  HandleScope scope(isolate);
  Handle<Object> type_handle =
      isolate->factory()->NewStringFromAscii(CStrVector(type));
  Handle<Object> args[2] = { type_handle, name };
  Handle<Object> error =
      isolate->factory()->NewTypeError("redeclaration", HandleVector(args, 2));
  return isolate->Throw(*error);
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_DeclareGlobals) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 3);
  Handle<GlobalObject> global = Handle<GlobalObject>(
      isolate->context()->global_object());

  Handle<Context> context = args.at<Context>(0);
  CONVERT_ARG_HANDLE_CHECKED(FixedArray, pairs, 1);
  CONVERT_SMI_ARG_CHECKED(flags, 2);

  // Traverse the name/value pairs and set the properties.
  int length = pairs->length();
  for (int i = 0; i < length; i += 2) {
    HandleScope scope(isolate);
    Handle<String> name(String::cast(pairs->get(i)));
    Handle<Object> value(pairs->get(i + 1), isolate);

    // We have to declare a global const property. To capture we only
    // assign to it when evaluating the assignment for "const x =
    // <expr>" the initial value is the hole.
    bool is_var = value->IsUndefined();
    bool is_const = value->IsTheHole();
    bool is_function = value->IsSharedFunctionInfo();
    ASSERT(is_var + is_const + is_function == 1);

    if (is_var || is_const) {
      // Lookup the property in the global object, and don't set the
      // value of the variable if the property is already there.
      // Do the lookup locally only, see ES5 erratum.
      LookupResult lookup(isolate);
      if (FLAG_es52_globals) {
        global->LocalLookup(*name, &lookup, true);
      } else {
        global->Lookup(*name, &lookup);
      }
      if (lookup.IsFound()) {
        // We found an existing property. Unless it was an interceptor
        // that claims the property is absent, skip this declaration.
        if (!lookup.IsInterceptor()) continue;
        PropertyAttributes attributes = global->GetPropertyAttribute(*name);
        if (attributes != ABSENT) continue;
        // Fall-through and introduce the absent property by using
        // SetProperty.
      }
    } else if (is_function) {
      // Copy the function and update its context. Use it as value.
      Handle<SharedFunctionInfo> shared =
          Handle<SharedFunctionInfo>::cast(value);
      Handle<JSFunction> function =
          isolate->factory()->NewFunctionFromSharedFunctionInfo(
              shared, context, TENURED);
      value = function;
    }

    LookupResult lookup(isolate);
    global->LocalLookup(*name, &lookup, true);

    // Compute the property attributes. According to ECMA-262,
    // the property must be non-configurable except in eval.
    int attr = NONE;
    bool is_eval = DeclareGlobalsEvalFlag::decode(flags);
    if (!is_eval) {
      attr |= DONT_DELETE;
    }
    bool is_native = DeclareGlobalsNativeFlag::decode(flags);
    if (is_const || (is_native && is_function)) {
      attr |= READ_ONLY;
    }

    LanguageMode language_mode = DeclareGlobalsLanguageMode::decode(flags);

    if (!lookup.IsFound() || is_function) {
      // If the local property exists, check that we can reconfigure it
      // as required for function declarations.
      if (lookup.IsFound() && lookup.IsDontDelete()) {
        if (lookup.IsReadOnly() || lookup.IsDontEnum() ||
            lookup.IsPropertyCallbacks()) {
          return ThrowRedeclarationError(isolate, "function", name);
        }
        // If the existing property is not configurable, keep its attributes.
        attr = lookup.GetAttributes();
      }
      // Define or redefine own property.
      RETURN_IF_EMPTY_HANDLE(isolate,
          JSObject::SetLocalPropertyIgnoreAttributes(
              global, name, value, static_cast<PropertyAttributes>(attr)));
    } else {
      // Do a [[Put]] on the existing (own) property.
      RETURN_IF_EMPTY_HANDLE(isolate,
          JSObject::SetProperty(
              global, name, value, static_cast<PropertyAttributes>(attr),
              language_mode == CLASSIC_MODE ? kNonStrictMode : kStrictMode));
    }
  }

  ASSERT(!isolate->has_pending_exception());
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_DeclareContextSlot) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 4);

  // Declarations are always made in a function or native context.  In the
  // case of eval code, the context passed is the context of the caller,
  // which may be some nested context and not the declaration context.
  RUNTIME_ASSERT(args[0]->IsContext());
  Handle<Context> context(Context::cast(args[0])->declaration_context());

  Handle<String> name(String::cast(args[1]));
  PropertyAttributes mode = static_cast<PropertyAttributes>(args.smi_at(2));
  RUNTIME_ASSERT(mode == READ_ONLY || mode == NONE);
  Handle<Object> initial_value(args[3], isolate);

  int index;
  PropertyAttributes attributes;
  ContextLookupFlags flags = DONT_FOLLOW_CHAINS;
  BindingFlags binding_flags;
  Handle<Object> holder =
      context->Lookup(name, flags, &index, &attributes, &binding_flags);

  if (attributes != ABSENT) {
    // The name was declared before; check for conflicting re-declarations.
    // Note: this is actually inconsistent with what happens for globals (where
    // we silently ignore such declarations).
    if (((attributes & READ_ONLY) != 0) || (mode == READ_ONLY)) {
      // Functions are not read-only.
      ASSERT(mode != READ_ONLY || initial_value->IsTheHole());
      const char* type = ((attributes & READ_ONLY) != 0) ? "const" : "var";
      return ThrowRedeclarationError(isolate, type, name);
    }

    // Initialize it if necessary.
    if (*initial_value != NULL) {
      if (index >= 0) {
        ASSERT(holder.is_identical_to(context));
        if (((attributes & READ_ONLY) == 0) ||
            context->get(index)->IsTheHole()) {
          context->set(index, *initial_value);
        }
      } else {
        // Slow case: The property is in the context extension object of a
        // function context or the global object of a native context.
        Handle<JSObject> object = Handle<JSObject>::cast(holder);
        RETURN_IF_EMPTY_HANDLE(
            isolate,
            JSReceiver::SetProperty(object, name, initial_value, mode,
                                    kNonStrictMode));
      }
    }

  } else {
    // The property is not in the function context. It needs to be
    // "declared" in the function context's extension context or as a
    // property of the the global object.
    Handle<JSObject> object;
    if (context->has_extension()) {
      object = Handle<JSObject>(JSObject::cast(context->extension()));
    } else {
      // Context extension objects are allocated lazily.
      ASSERT(context->IsFunctionContext());
      object = isolate->factory()->NewJSObject(
          isolate->context_extension_function());
      context->set_extension(*object);
    }
    ASSERT(*object != NULL);

    // Declare the property by setting it to the initial value if provided,
    // or undefined, and use the correct mode (e.g. READ_ONLY attribute for
    // constant declarations).
    ASSERT(!object->HasLocalProperty(*name));
    Handle<Object> value(isolate->heap()->undefined_value(), isolate);
    if (*initial_value != NULL) value = initial_value;
    // Declaring a const context slot is a conflicting declaration if
    // there is a callback with that name in a prototype. It is
    // allowed to introduce const variables in
    // JSContextExtensionObjects. They are treated specially in
    // SetProperty and no setters are invoked for those since they are
    // not real JSObjects.
    if (initial_value->IsTheHole() &&
        !object->IsJSContextExtensionObject()) {
      LookupResult lookup(isolate);
      object->Lookup(*name, &lookup);
      if (lookup.IsPropertyCallbacks()) {
        return ThrowRedeclarationError(isolate, "const", name);
      }
    }
    if (object->IsJSGlobalObject()) {
      // Define own property on the global object.
      RETURN_IF_EMPTY_HANDLE(isolate,
         JSObject::SetLocalPropertyIgnoreAttributes(object, name, value, mode));
    } else {
      RETURN_IF_EMPTY_HANDLE(isolate,
         JSReceiver::SetProperty(object, name, value, mode, kNonStrictMode));
    }
  }

  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_InitializeVarGlobal) {
  SealHandleScope shs(isolate);
  // args[0] == name
  // args[1] == language_mode
  // args[2] == value (optional)

  // Determine if we need to assign to the variable if it already
  // exists (based on the number of arguments).
  RUNTIME_ASSERT(args.length() == 2 || args.length() == 3);
  bool assign = args.length() == 3;

  CONVERT_ARG_HANDLE_CHECKED(String, name, 0);
  GlobalObject* global = isolate->context()->global_object();
  RUNTIME_ASSERT(args[1]->IsSmi());
  CONVERT_LANGUAGE_MODE_ARG(language_mode, 1);
  StrictModeFlag strict_mode_flag = (language_mode == CLASSIC_MODE)
      ? kNonStrictMode : kStrictMode;

  // According to ECMA-262, section 12.2, page 62, the property must
  // not be deletable.
  PropertyAttributes attributes = DONT_DELETE;

  // Lookup the property locally in the global object. If it isn't
  // there, there is a property with this name in the prototype chain.
  // We follow Safari and Firefox behavior and only set the property
  // locally if there is an explicit initialization value that we have
  // to assign to the property.
  // Note that objects can have hidden prototypes, so we need to traverse
  // the whole chain of hidden prototypes to do a 'local' lookup.
  Object* object = global;
  LookupResult lookup(isolate);
  JSObject::cast(object)->LocalLookup(*name, &lookup, true);
  if (lookup.IsInterceptor()) {
    HandleScope handle_scope(isolate);
    PropertyAttributes intercepted =
        lookup.holder()->GetPropertyAttribute(*name);
    if (intercepted != ABSENT && (intercepted & READ_ONLY) == 0) {
      // Found an interceptor that's not read only.
      if (assign) {
        return lookup.holder()->SetProperty(
            &lookup, *name, args[2], attributes, strict_mode_flag);
      } else {
        return isolate->heap()->undefined_value();
      }
    }
  }

  // Reload global in case the loop above performed a GC.
  global = isolate->context()->global_object();
  if (assign) {
    return global->SetProperty(*name, args[2], attributes, strict_mode_flag);
  }
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_InitializeConstGlobal) {
  SealHandleScope shs(isolate);
  // All constants are declared with an initial value. The name
  // of the constant is the first argument and the initial value
  // is the second.
  RUNTIME_ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(String, name, 0);
  Handle<Object> value = args.at<Object>(1);

  // Get the current global object from top.
  GlobalObject* global = isolate->context()->global_object();

  // According to ECMA-262, section 12.2, page 62, the property must
  // not be deletable. Since it's a const, it must be READ_ONLY too.
  PropertyAttributes attributes =
      static_cast<PropertyAttributes>(DONT_DELETE | READ_ONLY);

  // Lookup the property locally in the global object. If it isn't
  // there, we add the property and take special precautions to always
  // add it as a local property even in case of callbacks in the
  // prototype chain (this rules out using SetProperty).
  // We use SetLocalPropertyIgnoreAttributes instead
  LookupResult lookup(isolate);
  global->LocalLookup(*name, &lookup);
  if (!lookup.IsFound()) {
    return global->SetLocalPropertyIgnoreAttributes(*name,
                                                    *value,
                                                    attributes);
  }

  if (!lookup.IsReadOnly()) {
    // Restore global object from context (in case of GC) and continue
    // with setting the value.
    HandleScope handle_scope(isolate);
    Handle<GlobalObject> global(isolate->context()->global_object());

    // BUG 1213575: Handle the case where we have to set a read-only
    // property through an interceptor and only do it if it's
    // uninitialized, e.g. the hole. Nirk...
    // Passing non-strict mode because the property is writable.
    RETURN_IF_EMPTY_HANDLE(
        isolate,
        JSReceiver::SetProperty(global, name, value, attributes,
                                kNonStrictMode));
    return *value;
  }

  // Set the value, but only if we're assigning the initial value to a
  // constant. For now, we determine this by checking if the
  // current value is the hole.
  // Strict mode handling not needed (const is disallowed in strict mode).
  if (lookup.IsField()) {
    FixedArray* properties = global->properties();
    int index = lookup.GetFieldIndex().field_index();
    if (properties->get(index)->IsTheHole() || !lookup.IsReadOnly()) {
      properties->set(index, *value);
    }
  } else if (lookup.IsNormal()) {
    if (global->GetNormalizedProperty(&lookup)->IsTheHole() ||
        !lookup.IsReadOnly()) {
      global->SetNormalizedProperty(&lookup, *value);
    }
  } else {
    // Ignore re-initialization of constants that have already been
    // assigned a function value.
    ASSERT(lookup.IsReadOnly() && lookup.IsConstantFunction());
  }

  // Use the set value as the result of the operation.
  return *value;
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_InitializeConstContextSlot) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 3);

  Handle<Object> value(args[0], isolate);
  ASSERT(!value->IsTheHole());

  // Initializations are always done in a function or native context.
  RUNTIME_ASSERT(args[1]->IsContext());
  Handle<Context> context(Context::cast(args[1])->declaration_context());

  Handle<String> name(String::cast(args[2]));

  int index;
  PropertyAttributes attributes;
  ContextLookupFlags flags = FOLLOW_CHAINS;
  BindingFlags binding_flags;
  Handle<Object> holder =
      context->Lookup(name, flags, &index, &attributes, &binding_flags);

  if (index >= 0) {
    ASSERT(holder->IsContext());
    // Property was found in a context.  Perform the assignment if we
    // found some non-constant or an uninitialized constant.
    Handle<Context> context = Handle<Context>::cast(holder);
    if ((attributes & READ_ONLY) == 0 || context->get(index)->IsTheHole()) {
      context->set(index, *value);
    }
    return *value;
  }

  // The property could not be found, we introduce it as a property of the
  // global object.
  if (attributes == ABSENT) {
    Handle<JSObject> global = Handle<JSObject>(
        isolate->context()->global_object());
    // Strict mode not needed (const disallowed in strict mode).
    RETURN_IF_EMPTY_HANDLE(
        isolate,
        JSReceiver::SetProperty(global, name, value, NONE, kNonStrictMode));
    return *value;
  }

  // The property was present in some function's context extension object,
  // as a property on the subject of a with, or as a property of the global
  // object.
  //
  // In most situations, eval-introduced consts should still be present in
  // the context extension object.  However, because declaration and
  // initialization are separate, the property might have been deleted
  // before we reach the initialization point.
  //
  // Example:
  //
  //    function f() { eval("delete x; const x;"); }
  //
  // In that case, the initialization behaves like a normal assignment.
  Handle<JSObject> object = Handle<JSObject>::cast(holder);

  if (*object == context->extension()) {
    // This is the property that was introduced by the const declaration.
    // Set it if it hasn't been set before.  NOTE: We cannot use
    // GetProperty() to get the current value as it 'unholes' the value.
    LookupResult lookup(isolate);
    object->LocalLookupRealNamedProperty(*name, &lookup);
    ASSERT(lookup.IsFound());  // the property was declared
    ASSERT(lookup.IsReadOnly());  // and it was declared as read-only

    if (lookup.IsField()) {
      FixedArray* properties = object->properties();
      int index = lookup.GetFieldIndex().field_index();
      if (properties->get(index)->IsTheHole()) {
        properties->set(index, *value);
      }
    } else if (lookup.IsNormal()) {
      if (object->GetNormalizedProperty(&lookup)->IsTheHole()) {
        object->SetNormalizedProperty(&lookup, *value);
      }
    } else {
      // We should not reach here. Any real, named property should be
      // either a field or a dictionary slot.
      UNREACHABLE();
    }
  } else {
    // The property was found on some other object.  Set it if it is not a
    // read-only property.
    if ((attributes & READ_ONLY) == 0) {
      // Strict mode not needed (const disallowed in strict mode).
      RETURN_IF_EMPTY_HANDLE(
          isolate,
          JSReceiver::SetProperty(object, name, value, attributes,
                                  kNonStrictMode));
    }
  }

  return *value;
}


RUNTIME_FUNCTION(MaybeObject*,
                 Runtime_OptimizeObjectForAddingMultipleProperties) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 0);
  CONVERT_SMI_ARG_CHECKED(properties, 1);
  if (object->HasFastProperties()) {
    JSObject::NormalizeProperties(object, KEEP_INOBJECT_PROPERTIES, properties);
  }
  return *object;
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_RegExpExec) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 4);
  CONVERT_ARG_HANDLE_CHECKED(JSRegExp, regexp, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, subject, 1);
  // Due to the way the JS calls are constructed this must be less than the
  // length of a string, i.e. it is always a Smi.  We check anyway for security.
  CONVERT_SMI_ARG_CHECKED(index, 2);
  CONVERT_ARG_HANDLE_CHECKED(JSArray, last_match_info, 3);
  RUNTIME_ASSERT(index >= 0);
  RUNTIME_ASSERT(index <= subject->length());
  isolate->counters()->regexp_entry_runtime()->Increment();
  Handle<Object> result = RegExpImpl::Exec(regexp,
                                           subject,
                                           index,
                                           last_match_info);
  if (result.is_null()) return Failure::Exception();
  return *result;
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_RegExpConstructResult) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 3);
  CONVERT_SMI_ARG_CHECKED(elements_count, 0);
  if (elements_count < 0 ||
      elements_count > FixedArray::kMaxLength ||
      !Smi::IsValid(elements_count)) {
    return isolate->ThrowIllegalOperation();
  }
  Object* new_object;
  { MaybeObject* maybe_new_object =
        isolate->heap()->AllocateFixedArrayWithHoles(elements_count);
    if (!maybe_new_object->ToObject(&new_object)) return maybe_new_object;
  }
  FixedArray* elements = FixedArray::cast(new_object);
  { MaybeObject* maybe_new_object = isolate->heap()->AllocateRaw(
      JSRegExpResult::kSize, NEW_SPACE, OLD_POINTER_SPACE);
    if (!maybe_new_object->ToObject(&new_object)) return maybe_new_object;
  }
  {
    DisallowHeapAllocation no_gc;
    HandleScope scope(isolate);
    reinterpret_cast<HeapObject*>(new_object)->
        set_map(isolate->native_context()->regexp_result_map());
  }
  JSArray* array = JSArray::cast(new_object);
  array->set_properties(isolate->heap()->empty_fixed_array());
  array->set_elements(elements);
  array->set_length(Smi::FromInt(elements_count));
  // Write in-object properties after the length of the array.
  array->InObjectPropertyAtPut(JSRegExpResult::kIndexIndex, args[1]);
  array->InObjectPropertyAtPut(JSRegExpResult::kInputIndex, args[2]);
  return array;
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_RegExpInitializeObject) {
  SealHandleScope shs(isolate);
  DisallowHeapAllocation no_allocation;
  ASSERT(args.length() == 5);
  CONVERT_ARG_CHECKED(JSRegExp, regexp, 0);
  CONVERT_ARG_CHECKED(String, source, 1);
  // If source is the empty string we set it to "(?:)" instead as
  // suggested by ECMA-262, 5th, section 15.10.4.1.
  if (source->length() == 0) source = isolate->heap()->query_colon_string();

  Object* global = args[2];
  if (!global->IsTrue()) global = isolate->heap()->false_value();

  Object* ignoreCase = args[3];
  if (!ignoreCase->IsTrue()) ignoreCase = isolate->heap()->false_value();

  Object* multiline = args[4];
  if (!multiline->IsTrue()) multiline = isolate->heap()->false_value();

  Map* map = regexp->map();
  Object* constructor = map->constructor();
  if (constructor->IsJSFunction() &&
      JSFunction::cast(constructor)->initial_map() == map) {
    // If we still have the original map, set in-object properties directly.
    regexp->InObjectPropertyAtPut(JSRegExp::kSourceFieldIndex, source);
    // Both true and false are immovable immortal objects so no need for write
    // barrier.
    regexp->InObjectPropertyAtPut(
        JSRegExp::kGlobalFieldIndex, global, SKIP_WRITE_BARRIER);
    regexp->InObjectPropertyAtPut(
        JSRegExp::kIgnoreCaseFieldIndex, ignoreCase, SKIP_WRITE_BARRIER);
    regexp->InObjectPropertyAtPut(
        JSRegExp::kMultilineFieldIndex, multiline, SKIP_WRITE_BARRIER);
    regexp->InObjectPropertyAtPut(
        JSRegExp::kLastIndexFieldIndex, Smi::FromInt(0), SKIP_WRITE_BARRIER);
    return regexp;
  }

  // Map has changed, so use generic, but slower, method.
  PropertyAttributes final =
      static_cast<PropertyAttributes>(READ_ONLY | DONT_ENUM | DONT_DELETE);
  PropertyAttributes writable =
      static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE);
  Heap* heap = isolate->heap();
  MaybeObject* result;
  result = regexp->SetLocalPropertyIgnoreAttributes(heap->source_string(),
                                                    source,
                                                    final);
  // TODO(jkummerow): Turn these back into ASSERTs when we can be certain
  // that it never fires in Release mode in the wild.
  CHECK(!result->IsFailure());
  result = regexp->SetLocalPropertyIgnoreAttributes(heap->global_string(),
                                                    global,
                                                    final);
  CHECK(!result->IsFailure());
  result =
      regexp->SetLocalPropertyIgnoreAttributes(heap->ignore_case_string(),
                                               ignoreCase,
                                               final);
  CHECK(!result->IsFailure());
  result = regexp->SetLocalPropertyIgnoreAttributes(heap->multiline_string(),
                                                    multiline,
                                                    final);
  CHECK(!result->IsFailure());
  result =
      regexp->SetLocalPropertyIgnoreAttributes(heap->last_index_string(),
                                               Smi::FromInt(0),
                                               writable);
  CHECK(!result->IsFailure());
  USE(result);
  return regexp;
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_FinishArrayPrototypeSetup) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSArray, prototype, 0);
  // This is necessary to enable fast checks for absence of elements
  // on Array.prototype and below.
  prototype->set_elements(isolate->heap()->empty_fixed_array());
  return Smi::FromInt(0);
}


static Handle<JSFunction> InstallBuiltin(Isolate* isolate,
                                         Handle<JSObject> holder,
                                         const char* name,
                                         Builtins::Name builtin_name) {
  Handle<String> key = isolate->factory()->InternalizeUtf8String(name);
  Handle<Code> code(isolate->builtins()->builtin(builtin_name));
  Handle<JSFunction> optimized =
      isolate->factory()->NewFunction(key,
                                      JS_OBJECT_TYPE,
                                      JSObject::kHeaderSize,
                                      code,
                                      false);
  optimized->shared()->DontAdaptArguments();
  JSReceiver::SetProperty(holder, key, optimized, NONE, kStrictMode);
  return optimized;
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_SpecialArrayFunctions) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, holder, 0);

  InstallBuiltin(isolate, holder, "pop", Builtins::kArrayPop);
  InstallBuiltin(isolate, holder, "push", Builtins::kArrayPush);
  InstallBuiltin(isolate, holder, "shift", Builtins::kArrayShift);
  InstallBuiltin(isolate, holder, "unshift", Builtins::kArrayUnshift);
  InstallBuiltin(isolate, holder, "slice", Builtins::kArraySlice);
  InstallBuiltin(isolate, holder, "splice", Builtins::kArraySplice);
  InstallBuiltin(isolate, holder, "concat", Builtins::kArrayConcat);

  return *holder;
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_IsClassicModeFunction) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_CHECKED(JSReceiver, callable, 0);
  if (!callable->IsJSFunction()) {
    HandleScope scope(isolate);
    bool threw = false;
    Handle<Object> delegate =
        Execution::TryGetFunctionDelegate(Handle<JSReceiver>(callable), &threw);
    if (threw) return Failure::Exception();
    callable = JSFunction::cast(*delegate);
  }
  JSFunction* function = JSFunction::cast(callable);
  SharedFunctionInfo* shared = function->shared();
  return isolate->heap()->ToBoolean(shared->is_classic_mode());
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_GetDefaultReceiver) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_CHECKED(JSReceiver, callable, 0);

  if (!callable->IsJSFunction()) {
    HandleScope scope(isolate);
    bool threw = false;
    Handle<Object> delegate =
        Execution::TryGetFunctionDelegate(Handle<JSReceiver>(callable), &threw);
    if (threw) return Failure::Exception();
    callable = JSFunction::cast(*delegate);
  }
  JSFunction* function = JSFunction::cast(callable);

  SharedFunctionInfo* shared = function->shared();
  if (shared->native() || !shared->is_classic_mode()) {
    return isolate->heap()->undefined_value();
  }
  // Returns undefined for strict or native functions, or
  // the associated global receiver for "normal" functions.

  Context* native_context =
      function->context()->global_object()->native_context();
  return native_context->global_object()->global_receiver();
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_MaterializeRegExpLiteral) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 4);
  CONVERT_ARG_HANDLE_CHECKED(FixedArray, literals, 0);
  int index = args.smi_at(1);
  Handle<String> pattern = args.at<String>(2);
  Handle<String> flags = args.at<String>(3);

  // Get the RegExp function from the context in the literals array.
  // This is the RegExp function from the context in which the
  // function was created.  We do not use the RegExp function from the
  // current native context because this might be the RegExp function
  // from another context which we should not have access to.
  Handle<JSFunction> constructor =
      Handle<JSFunction>(
          JSFunction::NativeContextFromLiterals(*literals)->regexp_function());
  // Compute the regular expression literal.
  bool has_pending_exception;
  Handle<Object> regexp =
      RegExpImpl::CreateRegExpLiteral(constructor, pattern, flags,
                                      &has_pending_exception);
  if (has_pending_exception) {
    ASSERT(isolate->has_pending_exception());
    return Failure::Exception();
  }
  literals->set(index, *regexp);
  return *regexp;
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_FunctionGetName) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);

  CONVERT_ARG_CHECKED(JSFunction, f, 0);
  return f->shared()->name();
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_FunctionSetName) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 2);

  CONVERT_ARG_CHECKED(JSFunction, f, 0);
  CONVERT_ARG_CHECKED(String, name, 1);
  f->shared()->set_name(name);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_FunctionNameShouldPrintAsAnonymous) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_CHECKED(JSFunction, f, 0);
  return isolate->heap()->ToBoolean(
      f->shared()->name_should_print_as_anonymous());
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_FunctionMarkNameShouldPrintAsAnonymous) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_CHECKED(JSFunction, f, 0);
  f->shared()->set_name_should_print_as_anonymous(true);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_FunctionIsGenerator) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_CHECKED(JSFunction, f, 0);
  return isolate->heap()->ToBoolean(f->shared()->is_generator());
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_FunctionRemovePrototype) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);

  CONVERT_ARG_CHECKED(JSFunction, f, 0);
  f->RemovePrototype();

  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_FunctionGetScript) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);

  CONVERT_ARG_CHECKED(JSFunction, fun, 0);
  Handle<Object> script = Handle<Object>(fun->shared()->script(), isolate);
  if (!script->IsScript()) return isolate->heap()->undefined_value();

  return *GetScriptWrapper(Handle<Script>::cast(script));
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_FunctionGetSourceCode) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);

  CONVERT_ARG_HANDLE_CHECKED(JSFunction, f, 0);
  Handle<SharedFunctionInfo> shared(f->shared());
  return *shared->GetSourceCode();
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_FunctionGetScriptSourcePosition) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);

  CONVERT_ARG_CHECKED(JSFunction, fun, 0);
  int pos = fun->shared()->start_position();
  return Smi::FromInt(pos);
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_FunctionGetPositionForOffset) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 2);

  CONVERT_ARG_CHECKED(Code, code, 0);
  CONVERT_NUMBER_CHECKED(int, offset, Int32, args[1]);

  RUNTIME_ASSERT(0 <= offset && offset < code->Size());

  Address pc = code->address() + offset;
  return Smi::FromInt(code->SourcePosition(pc));
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_FunctionSetInstanceClassName) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 2);

  CONVERT_ARG_CHECKED(JSFunction, fun, 0);
  CONVERT_ARG_CHECKED(String, name, 1);
  fun->SetInstanceClassName(name);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_FunctionSetLength) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 2);

  CONVERT_ARG_CHECKED(JSFunction, fun, 0);
  CONVERT_SMI_ARG_CHECKED(length, 1);
  fun->shared()->set_length(length);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_FunctionSetPrototype) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 2);

  CONVERT_ARG_CHECKED(JSFunction, fun, 0);
  ASSERT(fun->should_have_prototype());
  Object* obj;
  { MaybeObject* maybe_obj =
        Accessors::FunctionSetPrototype(fun, args[1], NULL);
    if (!maybe_obj->ToObject(&obj)) return maybe_obj;
  }
  return args[0];  // return TOS
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_FunctionSetReadOnlyPrototype) {
  SealHandleScope shs(isolate);
  RUNTIME_ASSERT(args.length() == 1);
  CONVERT_ARG_CHECKED(JSFunction, function, 0);

  String* name = isolate->heap()->prototype_string();

  if (function->HasFastProperties()) {
    // Construct a new field descriptor with updated attributes.
    DescriptorArray* instance_desc = function->map()->instance_descriptors();

    int index = instance_desc->SearchWithCache(name, function->map());
    ASSERT(index != DescriptorArray::kNotFound);
    PropertyDetails details = instance_desc->GetDetails(index);

    CallbacksDescriptor new_desc(name,
        instance_desc->GetValue(index),
        static_cast<PropertyAttributes>(details.attributes() | READ_ONLY));

    // Create a new map featuring the new field descriptors array.
    Map* new_map;
    MaybeObject* maybe_map =
        function->map()->CopyReplaceDescriptor(
            instance_desc, &new_desc, index, OMIT_TRANSITION);
    if (!maybe_map->To(&new_map)) return maybe_map;

    function->set_map(new_map);
  } else {  // Dictionary properties.
    // Directly manipulate the property details.
    int entry = function->property_dictionary()->FindEntry(name);
    ASSERT(entry != NameDictionary::kNotFound);
    PropertyDetails details = function->property_dictionary()->DetailsAt(entry);
    PropertyDetails new_details(
        static_cast<PropertyAttributes>(details.attributes() | READ_ONLY),
        details.type(),
        details.dictionary_index());
    function->property_dictionary()->DetailsAtPut(entry, new_details);
  }
  return function;
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_FunctionIsAPIFunction) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);

  CONVERT_ARG_CHECKED(JSFunction, f, 0);
  return isolate->heap()->ToBoolean(f->shared()->IsApiFunction());
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_FunctionIsBuiltin) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);

  CONVERT_ARG_CHECKED(JSFunction, f, 0);
  return isolate->heap()->ToBoolean(f->IsBuiltin());
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_SetCode) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);

  CONVERT_ARG_HANDLE_CHECKED(JSFunction, target, 0);
  Handle<Object> code = args.at<Object>(1);

  if (code->IsNull()) return *target;
  RUNTIME_ASSERT(code->IsJSFunction());
  Handle<JSFunction> source = Handle<JSFunction>::cast(code);
  Handle<SharedFunctionInfo> target_shared(target->shared());
  Handle<SharedFunctionInfo> source_shared(source->shared());

  if (!JSFunction::EnsureCompiled(source, KEEP_EXCEPTION)) {
    return Failure::Exception();
  }

  // Mark both, the source and the target, as un-flushable because the
  // shared unoptimized code makes them impossible to enqueue in a list.
  ASSERT(target_shared->code()->gc_metadata() == NULL);
  ASSERT(source_shared->code()->gc_metadata() == NULL);
  target_shared->set_dont_flush(true);
  source_shared->set_dont_flush(true);

  // Set the code, scope info, formal parameter count, and the length
  // of the target shared function info.  Set the source code of the
  // target function to undefined.  SetCode is only used for built-in
  // constructors like String, Array, and Object, and some web code
  // doesn't like seeing source code for constructors.
  target_shared->ReplaceCode(source_shared->code());
  target_shared->set_scope_info(source_shared->scope_info());
  target_shared->set_length(source_shared->length());
  target_shared->set_formal_parameter_count(
      source_shared->formal_parameter_count());
  target_shared->set_script(isolate->heap()->undefined_value());

  // Since we don't store the source we should never optimize this.
  target_shared->code()->set_optimizable(false);

  // Set the code of the target function.
  target->ReplaceCode(source_shared->code());
  ASSERT(target->next_function_link()->IsUndefined());

  // Make sure we get a fresh copy of the literal vector to avoid cross
  // context contamination.
  Handle<Context> context(source->context());
  int number_of_literals = source->NumberOfLiterals();
  Handle<FixedArray> literals =
      isolate->factory()->NewFixedArray(number_of_literals, TENURED);
  if (number_of_literals > 0) {
    literals->set(JSFunction::kLiteralNativeContextIndex,
                  context->native_context());
  }
  target->set_context(*context);
  target->set_literals(*literals);

  if (isolate->logger()->is_logging_code_events() ||
      isolate->cpu_profiler()->is_profiling()) {
    isolate->logger()->LogExistingFunction(
        source_shared, Handle<Code>(source_shared->code()));
  }

  return *target;
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_SetExpectedNumberOfProperties) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);
  CONVERT_SMI_ARG_CHECKED(num, 1);
  RUNTIME_ASSERT(num >= 0);
  SetExpectedNofProperties(function, num);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_CreateJSGeneratorObject) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 0);

  JavaScriptFrameIterator it(isolate);
  JavaScriptFrame* frame = it.frame();
  JSFunction* function = JSFunction::cast(frame->function());
  RUNTIME_ASSERT(function->shared()->is_generator());

  JSGeneratorObject* generator;
  if (frame->IsConstructor()) {
    generator = JSGeneratorObject::cast(frame->receiver());
  } else {
    MaybeObject* maybe_generator =
        isolate->heap()->AllocateJSGeneratorObject(function);
    if (!maybe_generator->To(&generator)) return maybe_generator;
  }
  generator->set_function(function);
  generator->set_context(Context::cast(frame->context()));
  generator->set_receiver(frame->receiver());
  generator->set_continuation(0);
  generator->set_operand_stack(isolate->heap()->empty_fixed_array());
  generator->set_stack_handler_index(-1);

  return generator;
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_SuspendJSGeneratorObject) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_CHECKED(JSGeneratorObject, generator_object, 0);

  JavaScriptFrameIterator stack_iterator(isolate);
  JavaScriptFrame* frame = stack_iterator.frame();
  RUNTIME_ASSERT(JSFunction::cast(frame->function())->shared()->is_generator());
  ASSERT_EQ(JSFunction::cast(frame->function()), generator_object->function());

  // The caller should have saved the context and continuation already.
  ASSERT_EQ(generator_object->context(), Context::cast(frame->context()));
  ASSERT_LT(0, generator_object->continuation());

  // We expect there to be at least two values on the operand stack: the return
  // value of the yield expression, and the argument to this runtime call.
  // Neither of those should be saved.
  int operands_count = frame->ComputeOperandsCount();
  ASSERT_GE(operands_count, 2);
  operands_count -= 2;

  if (operands_count == 0) {
    // Although it's semantically harmless to call this function with an
    // operands_count of zero, it is also unnecessary.
    ASSERT_EQ(generator_object->operand_stack(),
              isolate->heap()->empty_fixed_array());
    ASSERT_EQ(generator_object->stack_handler_index(), -1);
    // If there are no operands on the stack, there shouldn't be a handler
    // active either.
    ASSERT(!frame->HasHandler());
  } else {
    int stack_handler_index = -1;
    MaybeObject* alloc = isolate->heap()->AllocateFixedArray(operands_count);
    FixedArray* operand_stack;
    if (!alloc->To(&operand_stack)) return alloc;
    frame->SaveOperandStack(operand_stack, &stack_handler_index);
    generator_object->set_operand_stack(operand_stack);
    generator_object->set_stack_handler_index(stack_handler_index);
  }

  return isolate->heap()->undefined_value();
}


// Note that this function is the slow path for resuming generators.  It is only
// called if the suspended activation had operands on the stack, stack handlers
// needing rewinding, or if the resume should throw an exception.  The fast path
// is handled directly in FullCodeGenerator::EmitGeneratorResume(), which is
// inlined into GeneratorNext and GeneratorThrow.  EmitGeneratorResumeResume is
// called in any case, as it needs to reconstruct the stack frame and make space
// for arguments and operands.
RUNTIME_FUNCTION(MaybeObject*, Runtime_ResumeJSGeneratorObject) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 3);
  CONVERT_ARG_CHECKED(JSGeneratorObject, generator_object, 0);
  CONVERT_ARG_CHECKED(Object, value, 1);
  CONVERT_SMI_ARG_CHECKED(resume_mode_int, 2);
  JavaScriptFrameIterator stack_iterator(isolate);
  JavaScriptFrame* frame = stack_iterator.frame();

  ASSERT_EQ(frame->function(), generator_object->function());

  STATIC_ASSERT(JSGeneratorObject::kGeneratorExecuting <= 0);
  STATIC_ASSERT(JSGeneratorObject::kGeneratorClosed <= 0);

  Address pc = generator_object->function()->code()->instruction_start();
  int offset = generator_object->continuation();
  ASSERT(offset > 0);
  frame->set_pc(pc + offset);
  generator_object->set_continuation(JSGeneratorObject::kGeneratorExecuting);

  FixedArray* operand_stack = generator_object->operand_stack();
  int operands_count = operand_stack->length();
  if (operands_count != 0) {
    frame->RestoreOperandStack(operand_stack,
                               generator_object->stack_handler_index());
    generator_object->set_operand_stack(isolate->heap()->empty_fixed_array());
    generator_object->set_stack_handler_index(-1);
  }

  JSGeneratorObject::ResumeMode resume_mode =
      static_cast<JSGeneratorObject::ResumeMode>(resume_mode_int);
  switch (resume_mode) {
    case JSGeneratorObject::NEXT:
      return value;
    case JSGeneratorObject::THROW:
      return isolate->Throw(value);
  }

  UNREACHABLE();
  return isolate->ThrowIllegalOperation();
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_ThrowGeneratorStateError) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSGeneratorObject, generator, 0);
  int continuation = generator->continuation();
  const char* message = continuation == JSGeneratorObject::kGeneratorClosed ?
      "generator_finished" : "generator_running";
  Vector< Handle<Object> > argv = HandleVector<Object>(NULL, 0);
  Handle<Object> error = isolate->factory()->NewError(message, argv);
  return isolate->Throw(*error);
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_ObjectFreeze) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_CHECKED(JSObject, object, 0);
  return object->Freeze(isolate);
}


MUST_USE_RESULT static MaybeObject* CharFromCode(Isolate* isolate,
                                                 Object* char_code) {
  if (char_code->IsNumber()) {
    return isolate->heap()->LookupSingleCharacterStringFromCode(
        NumberToUint32(char_code) & 0xffff);
  }
  return isolate->heap()->empty_string();
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_StringCharCodeAt) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 2);

  CONVERT_ARG_CHECKED(String, subject, 0);
  CONVERT_NUMBER_CHECKED(uint32_t, i, Uint32, args[1]);

  // Flatten the string.  If someone wants to get a char at an index
  // in a cons string, it is likely that more indices will be
  // accessed.
  Object* flat;
  { MaybeObject* maybe_flat = subject->TryFlatten();
    if (!maybe_flat->ToObject(&flat)) return maybe_flat;
  }
  subject = String::cast(flat);

  if (i >= static_cast<uint32_t>(subject->length())) {
    return isolate->heap()->nan_value();
  }

  return Smi::FromInt(subject->Get(i));
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_CharFromCode) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  return CharFromCode(isolate, args[0]);
}


class FixedArrayBuilder {
 public:
  explicit FixedArrayBuilder(Isolate* isolate, int initial_capacity)
      : array_(isolate->factory()->NewFixedArrayWithHoles(initial_capacity)),
        length_(0),
        has_non_smi_elements_(false) {
    // Require a non-zero initial size. Ensures that doubling the size to
    // extend the array will work.
    ASSERT(initial_capacity > 0);
  }

  explicit FixedArrayBuilder(Handle<FixedArray> backing_store)
      : array_(backing_store),
        length_(0),
        has_non_smi_elements_(false) {
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
          array_->GetIsolate()->factory()->NewFixedArrayWithHoles(new_length);
      array_->CopyTo(0, *extended_array, 0, length_);
      array_ = extended_array;
    }
  }

  void Add(Object* value) {
    ASSERT(!value->IsSmi());
    ASSERT(length_ < capacity());
    array_->set(length_, value);
    length_++;
    has_non_smi_elements_ = true;
  }

  void Add(Smi* value) {
    ASSERT(value->IsSmi());
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

  Handle<JSArray> ToJSArray(Handle<JSArray> target_array) {
    Factory* factory = target_array->GetIsolate()->factory();
    factory->SetContent(target_array, array_);
    target_array->set_length(Smi::FromInt(length_));
    return target_array;
  }


 private:
  Handle<FixedArray> array_;
  int length_;
  bool has_non_smi_elements_;
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
  ReplacementStringBuilder(Heap* heap,
                           Handle<String> subject,
                           int estimated_part_count)
      : heap_(heap),
        array_builder_(heap->isolate(), estimated_part_count),
        subject_(subject),
        character_count_(0),
        is_ascii_(subject->IsOneByteRepresentation()) {
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
    if (!string->IsOneByteRepresentation()) {
      is_ascii_ = false;
    }
    IncrementCharacterCount(length);
  }


  Handle<String> ToString() {
    if (array_builder_.length() == 0) {
      return heap_->isolate()->factory()->empty_string();
    }

    Handle<String> joined_string;
    if (is_ascii_) {
      Handle<SeqOneByteString> seq = NewRawOneByteString(character_count_);
      DisallowHeapAllocation no_gc;
      uint8_t* char_buffer = seq->GetChars();
      StringBuilderConcatHelper(*subject_,
                                char_buffer,
                                *array_builder_.array(),
                                array_builder_.length());
      joined_string = Handle<String>::cast(seq);
    } else {
      // Non-ASCII.
      Handle<SeqTwoByteString> seq = NewRawTwoByteString(character_count_);
      DisallowHeapAllocation no_gc;
      uc16* char_buffer = seq->GetChars();
      StringBuilderConcatHelper(*subject_,
                                char_buffer,
                                *array_builder_.array(),
                                array_builder_.length());
      joined_string = Handle<String>::cast(seq);
    }
    return joined_string;
  }


  void IncrementCharacterCount(int by) {
    if (character_count_ > String::kMaxLength - by) {
      V8::FatalProcessOutOfMemory("String.replace result too large.");
    }
    character_count_ += by;
  }

 private:
  Handle<SeqOneByteString> NewRawOneByteString(int length) {
    return heap_->isolate()->factory()->NewRawOneByteString(length);
  }


  Handle<SeqTwoByteString> NewRawTwoByteString(int length) {
    return heap_->isolate()->factory()->NewRawTwoByteString(length);
  }


  void AddElement(Object* element) {
    ASSERT(element->IsSmi() || element->IsString());
    ASSERT(array_builder_.capacity() > array_builder_.length());
    array_builder_.Add(element);
  }

  Heap* heap_;
  FixedArrayBuilder array_builder_;
  Handle<String> subject_;
  int character_count_;
  bool is_ascii_;
};


class CompiledReplacement {
 public:
  explicit CompiledReplacement(Zone* zone)
      : parts_(1, zone), replacement_substrings_(0, zone), zone_(zone) {}

  // Return whether the replacement is simple.
  bool Compile(Handle<String> replacement,
               int capture_count,
               int subject_length);

  // Use Apply only if Compile returned false.
  void Apply(ReplacementStringBuilder* builder,
             int match_from,
             int match_to,
             int32_t* match);

  // Number of distinct parts of the replacement pattern.
  int parts() {
    return parts_.length();
  }

  Zone* zone() const { return zone_; }

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
  bool ParseReplacementPattern(ZoneList<ReplacementPart>* parts,
                               Vector<Char> characters,
                               int capture_count,
                               int subject_length,
                               Zone* zone) {
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
            parts->Add(ReplacementPart::ReplacementSubString(last, next_index),
                       zone);
            last = next_index + 1;  // Continue after the second "$".
          } else {
            // Let the next substring start with the second "$".
            last = next_index;
          }
          i = next_index;
          break;
        case '`':
          if (i > last) {
            parts->Add(ReplacementPart::ReplacementSubString(last, i), zone);
          }
          parts->Add(ReplacementPart::SubjectPrefix(), zone);
          i = next_index;
          last = i + 1;
          break;
        case '\'':
          if (i > last) {
            parts->Add(ReplacementPart::ReplacementSubString(last, i), zone);
          }
          parts->Add(ReplacementPart::SubjectSuffix(subject_length), zone);
          i = next_index;
          last = i + 1;
          break;
        case '&':
          if (i > last) {
            parts->Add(ReplacementPart::ReplacementSubString(last, i), zone);
          }
          parts->Add(ReplacementPart::SubjectMatch(), zone);
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
              parts->Add(ReplacementPart::ReplacementSubString(last, i), zone);
            }
            ASSERT(capture_ref <= capture_count);
            parts->Add(ReplacementPart::SubjectCapture(capture_ref), zone);
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
        // Replacement is simple.  Do not use Apply to do the replacement.
        return true;
      } else {
        parts->Add(ReplacementPart::ReplacementSubString(last, length), zone);
      }
    }
    return false;
  }

  ZoneList<ReplacementPart> parts_;
  ZoneList<Handle<String> > replacement_substrings_;
  Zone* zone_;
};


bool CompiledReplacement::Compile(Handle<String> replacement,
                                  int capture_count,
                                  int subject_length) {
  {
    DisallowHeapAllocation no_gc;
    String::FlatContent content = replacement->GetFlatContent();
    ASSERT(content.IsFlat());
    bool simple = false;
    if (content.IsAscii()) {
      simple = ParseReplacementPattern(&parts_,
                                       content.ToOneByteVector(),
                                       capture_count,
                                       subject_length,
                                       zone());
    } else {
      ASSERT(content.IsTwoByte());
      simple = ParseReplacementPattern(&parts_,
                                       content.ToUC16Vector(),
                                       capture_count,
                                       subject_length,
                                       zone());
    }
    if (simple) return true;
  }

  Isolate* isolate = replacement->GetIsolate();
  // Find substrings of replacement string and create them as String objects.
  int substring_index = 0;
  for (int i = 0, n = parts_.length(); i < n; i++) {
    int tag = parts_[i].tag;
    if (tag <= 0) {  // A replacement string slice.
      int from = -tag;
      int to = parts_[i].data;
      replacement_substrings_.Add(
          isolate->factory()->NewSubString(replacement, from, to), zone());
      parts_[i].tag = REPLACEMENT_SUBSTRING;
      parts_[i].data = substring_index;
      substring_index++;
    } else if (tag == REPLACEMENT_STRING) {
      replacement_substrings_.Add(replacement, zone());
      parts_[i].data = substring_index;
      substring_index++;
    }
  }
  return false;
}


void CompiledReplacement::Apply(ReplacementStringBuilder* builder,
                                int match_from,
                                int match_to,
                                int32_t* match) {
  ASSERT_LT(0, parts_.length());
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
        int from = match[capture * 2];
        int to = match[capture * 2 + 1];
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


void FindAsciiStringIndices(Vector<const uint8_t> subject,
                            char pattern,
                            ZoneList<int>* indices,
                            unsigned int limit,
                            Zone* zone) {
  ASSERT(limit > 0);
  // Collect indices of pattern in subject using memchr.
  // Stop after finding at most limit values.
  const uint8_t* subject_start = subject.start();
  const uint8_t* subject_end = subject_start + subject.length();
  const uint8_t* pos = subject_start;
  while (limit > 0) {
    pos = reinterpret_cast<const uint8_t*>(
        memchr(pos, pattern, subject_end - pos));
    if (pos == NULL) return;
    indices->Add(static_cast<int>(pos - subject_start), zone);
    pos++;
    limit--;
  }
}


void FindTwoByteStringIndices(const Vector<const uc16> subject,
                              uc16 pattern,
                              ZoneList<int>* indices,
                              unsigned int limit,
                              Zone* zone) {
  ASSERT(limit > 0);
  const uc16* subject_start = subject.start();
  const uc16* subject_end = subject_start + subject.length();
  for (const uc16* pos = subject_start; pos < subject_end && limit > 0; pos++) {
    if (*pos == pattern) {
      indices->Add(static_cast<int>(pos - subject_start), zone);
      limit--;
    }
  }
}


template <typename SubjectChar, typename PatternChar>
void FindStringIndices(Isolate* isolate,
                       Vector<const SubjectChar> subject,
                       Vector<const PatternChar> pattern,
                       ZoneList<int>* indices,
                       unsigned int limit,
                       Zone* zone) {
  ASSERT(limit > 0);
  // Collect indices of pattern in subject.
  // Stop after finding at most limit values.
  int pattern_length = pattern.length();
  int index = 0;
  StringSearch<PatternChar, SubjectChar> search(isolate, pattern);
  while (limit > 0) {
    index = search.Search(subject, index);
    if (index < 0) return;
    indices->Add(index, zone);
    index += pattern_length;
    limit--;
  }
}


void FindStringIndicesDispatch(Isolate* isolate,
                               String* subject,
                               String* pattern,
                               ZoneList<int>* indices,
                               unsigned int limit,
                               Zone* zone) {
  {
    DisallowHeapAllocation no_gc;
    String::FlatContent subject_content = subject->GetFlatContent();
    String::FlatContent pattern_content = pattern->GetFlatContent();
    ASSERT(subject_content.IsFlat());
    ASSERT(pattern_content.IsFlat());
    if (subject_content.IsAscii()) {
      Vector<const uint8_t> subject_vector = subject_content.ToOneByteVector();
      if (pattern_content.IsAscii()) {
        Vector<const uint8_t> pattern_vector =
            pattern_content.ToOneByteVector();
        if (pattern_vector.length() == 1) {
          FindAsciiStringIndices(subject_vector,
                                 pattern_vector[0],
                                 indices,
                                 limit,
                                 zone);
        } else {
          FindStringIndices(isolate,
                            subject_vector,
                            pattern_vector,
                            indices,
                            limit,
                            zone);
        }
      } else {
        FindStringIndices(isolate,
                          subject_vector,
                          pattern_content.ToUC16Vector(),
                          indices,
                          limit,
                          zone);
      }
    } else {
      Vector<const uc16> subject_vector = subject_content.ToUC16Vector();
      if (pattern_content.IsAscii()) {
        Vector<const uint8_t> pattern_vector =
            pattern_content.ToOneByteVector();
        if (pattern_vector.length() == 1) {
          FindTwoByteStringIndices(subject_vector,
                                   pattern_vector[0],
                                   indices,
                                   limit,
                                   zone);
        } else {
          FindStringIndices(isolate,
                            subject_vector,
                            pattern_vector,
                            indices,
                            limit,
                            zone);
        }
      } else {
        Vector<const uc16> pattern_vector = pattern_content.ToUC16Vector();
        if (pattern_vector.length() == 1) {
          FindTwoByteStringIndices(subject_vector,
                                   pattern_vector[0],
                                   indices,
                                   limit,
                                   zone);
        } else {
          FindStringIndices(isolate,
                            subject_vector,
                            pattern_vector,
                            indices,
                            limit,
                            zone);
        }
      }
    }
  }
}


template<typename ResultSeqString>
MUST_USE_RESULT static MaybeObject* StringReplaceGlobalAtomRegExpWithString(
    Isolate* isolate,
    Handle<String> subject,
    Handle<JSRegExp> pattern_regexp,
    Handle<String> replacement,
    Handle<JSArray> last_match_info) {
  ASSERT(subject->IsFlat());
  ASSERT(replacement->IsFlat());

  ZoneScope zone_scope(isolate->runtime_zone());
  ZoneList<int> indices(8, zone_scope.zone());
  ASSERT_EQ(JSRegExp::ATOM, pattern_regexp->TypeTag());
  String* pattern =
      String::cast(pattern_regexp->DataAt(JSRegExp::kAtomPatternIndex));
  int subject_len = subject->length();
  int pattern_len = pattern->length();
  int replacement_len = replacement->length();

  FindStringIndicesDispatch(
      isolate, *subject, pattern, &indices, 0xffffffff, zone_scope.zone());

  int matches = indices.length();
  if (matches == 0) return *subject;

  // Detect integer overflow.
  int64_t result_len_64 =
      (static_cast<int64_t>(replacement_len) -
       static_cast<int64_t>(pattern_len)) *
      static_cast<int64_t>(matches) +
      static_cast<int64_t>(subject_len);
  if (result_len_64 > INT_MAX) return Failure::OutOfMemoryException(0x11);
  int result_len = static_cast<int>(result_len_64);

  int subject_pos = 0;
  int result_pos = 0;

  Handle<ResultSeqString> result;
  if (ResultSeqString::kHasAsciiEncoding) {
    result = Handle<ResultSeqString>::cast(
        isolate->factory()->NewRawOneByteString(result_len));
  } else {
    result = Handle<ResultSeqString>::cast(
        isolate->factory()->NewRawTwoByteString(result_len));
  }

  for (int i = 0; i < matches; i++) {
    // Copy non-matched subject content.
    if (subject_pos < indices.at(i)) {
      String::WriteToFlat(*subject,
                          result->GetChars() + result_pos,
                          subject_pos,
                          indices.at(i));
      result_pos += indices.at(i) - subject_pos;
    }

    // Replace match.
    if (replacement_len > 0) {
      String::WriteToFlat(*replacement,
                          result->GetChars() + result_pos,
                          0,
                          replacement_len);
      result_pos += replacement_len;
    }

    subject_pos = indices.at(i) + pattern_len;
  }
  // Add remaining subject content at the end.
  if (subject_pos < subject_len) {
    String::WriteToFlat(*subject,
                        result->GetChars() + result_pos,
                        subject_pos,
                        subject_len);
  }

  int32_t match_indices[] = { indices.at(matches - 1),
                              indices.at(matches - 1) + pattern_len };
  RegExpImpl::SetLastMatchInfo(last_match_info, subject, 0, match_indices);

  return *result;
}


MUST_USE_RESULT static MaybeObject* StringReplaceGlobalRegExpWithString(
    Isolate* isolate,
    Handle<String> subject,
    Handle<JSRegExp> regexp,
    Handle<String> replacement,
    Handle<JSArray> last_match_info) {
  ASSERT(subject->IsFlat());
  ASSERT(replacement->IsFlat());

  int capture_count = regexp->CaptureCount();
  int subject_length = subject->length();

  // CompiledReplacement uses zone allocation.
  ZoneScope zone_scope(isolate->runtime_zone());
  CompiledReplacement compiled_replacement(zone_scope.zone());
  bool simple_replace = compiled_replacement.Compile(replacement,
                                                     capture_count,
                                                     subject_length);

  // Shortcut for simple non-regexp global replacements
  if (regexp->TypeTag() == JSRegExp::ATOM && simple_replace) {
    if (subject->HasOnlyOneByteChars() &&
        replacement->HasOnlyOneByteChars()) {
      return StringReplaceGlobalAtomRegExpWithString<SeqOneByteString>(
          isolate, subject, regexp, replacement, last_match_info);
    } else {
      return StringReplaceGlobalAtomRegExpWithString<SeqTwoByteString>(
          isolate, subject, regexp, replacement, last_match_info);
    }
  }

  RegExpImpl::GlobalCache global_cache(regexp, subject, true, isolate);
  if (global_cache.HasException()) return Failure::Exception();

  int32_t* current_match = global_cache.FetchNext();
  if (current_match == NULL) {
    if (global_cache.HasException()) return Failure::Exception();
    return *subject;
  }

  // Guessing the number of parts that the final result string is built
  // from. Global regexps can match any number of times, so we guess
  // conservatively.
  int expected_parts = (compiled_replacement.parts() + 1) * 4 + 1;
  ReplacementStringBuilder builder(isolate->heap(),
                                   subject,
                                   expected_parts);

  // Number of parts added by compiled replacement plus preceeding
  // string and possibly suffix after last match.  It is possible for
  // all components to use two elements when encoded as two smis.
  const int parts_added_per_loop = 2 * (compiled_replacement.parts() + 2);

  int prev = 0;

  do {
    builder.EnsureCapacity(parts_added_per_loop);

    int start = current_match[0];
    int end = current_match[1];

    if (prev < start) {
      builder.AddSubjectSlice(prev, start);
    }

    if (simple_replace) {
      builder.AddString(replacement);
    } else {
      compiled_replacement.Apply(&builder,
                                 start,
                                 end,
                                 current_match);
    }
    prev = end;

    current_match = global_cache.FetchNext();
  } while (current_match != NULL);

  if (global_cache.HasException()) return Failure::Exception();

  if (prev < subject_length) {
    builder.EnsureCapacity(2);
    builder.AddSubjectSlice(prev, subject_length);
  }

  RegExpImpl::SetLastMatchInfo(last_match_info,
                               subject,
                               capture_count,
                               global_cache.LastSuccessfulMatch());

  return *(builder.ToString());
}


template <typename ResultSeqString>
MUST_USE_RESULT static MaybeObject* StringReplaceGlobalRegExpWithEmptyString(
    Isolate* isolate,
    Handle<String> subject,
    Handle<JSRegExp> regexp,
    Handle<JSArray> last_match_info) {
  ASSERT(subject->IsFlat());

  // Shortcut for simple non-regexp global replacements
  if (regexp->TypeTag() == JSRegExp::ATOM) {
    Handle<String> empty_string = isolate->factory()->empty_string();
    if (subject->IsOneByteRepresentation()) {
      return StringReplaceGlobalAtomRegExpWithString<SeqOneByteString>(
          isolate, subject, regexp, empty_string, last_match_info);
    } else {
      return StringReplaceGlobalAtomRegExpWithString<SeqTwoByteString>(
          isolate, subject, regexp, empty_string, last_match_info);
    }
  }

  RegExpImpl::GlobalCache global_cache(regexp, subject, true, isolate);
  if (global_cache.HasException()) return Failure::Exception();

  int32_t* current_match = global_cache.FetchNext();
  if (current_match == NULL) {
    if (global_cache.HasException()) return Failure::Exception();
    return *subject;
  }

  int start = current_match[0];
  int end = current_match[1];
  int capture_count = regexp->CaptureCount();
  int subject_length = subject->length();

  int new_length = subject_length - (end - start);
  if (new_length == 0) return isolate->heap()->empty_string();

  Handle<ResultSeqString> answer;
  if (ResultSeqString::kHasAsciiEncoding) {
    answer = Handle<ResultSeqString>::cast(
        isolate->factory()->NewRawOneByteString(new_length));
  } else {
    answer = Handle<ResultSeqString>::cast(
        isolate->factory()->NewRawTwoByteString(new_length));
  }

  int prev = 0;
  int position = 0;

  do {
    start = current_match[0];
    end = current_match[1];
    if (prev < start) {
      // Add substring subject[prev;start] to answer string.
      String::WriteToFlat(*subject, answer->GetChars() + position, prev, start);
      position += start - prev;
    }
    prev = end;

    current_match = global_cache.FetchNext();
  } while (current_match != NULL);

  if (global_cache.HasException()) return Failure::Exception();

  RegExpImpl::SetLastMatchInfo(last_match_info,
                               subject,
                               capture_count,
                               global_cache.LastSuccessfulMatch());

  if (prev < subject_length) {
    // Add substring subject[prev;length] to answer string.
    String::WriteToFlat(
        *subject, answer->GetChars() + position, prev, subject_length);
    position += subject_length - prev;
  }

  if (position == 0) return isolate->heap()->empty_string();

  // Shorten string and fill
  int string_size = ResultSeqString::SizeFor(position);
  int allocated_string_size = ResultSeqString::SizeFor(new_length);
  int delta = allocated_string_size - string_size;

  answer->set_length(position);
  if (delta == 0) return *answer;

  Address end_of_string = answer->address() + string_size;
  isolate->heap()->CreateFillerObjectAt(end_of_string, delta);
  if (Marking::IsBlack(Marking::MarkBitFrom(*answer))) {
    MemoryChunk::IncrementLiveBytesFromMutator(answer->address(), -delta);
  }

  return *answer;
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_StringReplaceGlobalRegExpWithString) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 4);

  CONVERT_ARG_HANDLE_CHECKED(String, subject, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, replacement, 2);
  CONVERT_ARG_HANDLE_CHECKED(JSRegExp, regexp, 1);
  CONVERT_ARG_HANDLE_CHECKED(JSArray, last_match_info, 3);

  ASSERT(regexp->GetFlags().is_global());

  if (!subject->IsFlat()) subject = FlattenGetString(subject);

  if (replacement->length() == 0) {
    if (subject->HasOnlyOneByteChars()) {
      return StringReplaceGlobalRegExpWithEmptyString<SeqOneByteString>(
          isolate, subject, regexp, last_match_info);
    } else {
      return StringReplaceGlobalRegExpWithEmptyString<SeqTwoByteString>(
          isolate, subject, regexp, last_match_info);
    }
  }

  if (!replacement->IsFlat()) replacement = FlattenGetString(replacement);

  return StringReplaceGlobalRegExpWithString(
      isolate, subject, regexp, replacement, last_match_info);
}


Handle<String> StringReplaceOneCharWithString(Isolate* isolate,
                                              Handle<String> subject,
                                              Handle<String> search,
                                              Handle<String> replace,
                                              bool* found,
                                              int recursion_limit) {
  if (recursion_limit == 0) return Handle<String>::null();
  if (subject->IsConsString()) {
    ConsString* cons = ConsString::cast(*subject);
    Handle<String> first = Handle<String>(cons->first());
    Handle<String> second = Handle<String>(cons->second());
    Handle<String> new_first =
        StringReplaceOneCharWithString(isolate,
                                       first,
                                       search,
                                       replace,
                                       found,
                                       recursion_limit - 1);
    if (*found) return isolate->factory()->NewConsString(new_first, second);
    if (new_first.is_null()) return new_first;

    Handle<String> new_second =
        StringReplaceOneCharWithString(isolate,
                                       second,
                                       search,
                                       replace,
                                       found,
                                       recursion_limit - 1);
    if (*found) return isolate->factory()->NewConsString(first, new_second);
    if (new_second.is_null()) return new_second;

    return subject;
  } else {
    int index = Runtime::StringMatch(isolate, subject, search, 0);
    if (index == -1) return subject;
    *found = true;
    Handle<String> first = isolate->factory()->NewSubString(subject, 0, index);
    Handle<String> cons1 = isolate->factory()->NewConsString(first, replace);
    Handle<String> second =
        isolate->factory()->NewSubString(subject, index + 1, subject->length());
    return isolate->factory()->NewConsString(cons1, second);
  }
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_StringReplaceOneCharWithString) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 3);
  CONVERT_ARG_HANDLE_CHECKED(String, subject, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, search, 1);
  CONVERT_ARG_HANDLE_CHECKED(String, replace, 2);

  // If the cons string tree is too deep, we simply abort the recursion and
  // retry with a flattened subject string.
  const int kRecursionLimit = 0x1000;
  bool found = false;
  Handle<String> result = StringReplaceOneCharWithString(isolate,
                                                         subject,
                                                         search,
                                                         replace,
                                                         &found,
                                                         kRecursionLimit);
  if (!result.is_null()) return *result;
  return *StringReplaceOneCharWithString(isolate,
                                         FlattenGetString(subject),
                                         search,
                                         replace,
                                         &found,
                                         kRecursionLimit);
}


// Perform string match of pattern on subject, starting at start index.
// Caller must ensure that 0 <= start_index <= sub->length(),
// and should check that pat->length() + start_index <= sub->length().
int Runtime::StringMatch(Isolate* isolate,
                         Handle<String> sub,
                         Handle<String> pat,
                         int start_index) {
  ASSERT(0 <= start_index);
  ASSERT(start_index <= sub->length());

  int pattern_length = pat->length();
  if (pattern_length == 0) return start_index;

  int subject_length = sub->length();
  if (start_index + pattern_length > subject_length) return -1;

  if (!sub->IsFlat()) FlattenString(sub);
  if (!pat->IsFlat()) FlattenString(pat);

  DisallowHeapAllocation no_gc;  // ensure vectors stay valid
  // Extract flattened substrings of cons strings before determining asciiness.
  String::FlatContent seq_sub = sub->GetFlatContent();
  String::FlatContent seq_pat = pat->GetFlatContent();

  // dispatch on type of strings
  if (seq_pat.IsAscii()) {
    Vector<const uint8_t> pat_vector = seq_pat.ToOneByteVector();
    if (seq_sub.IsAscii()) {
      return SearchString(isolate,
                          seq_sub.ToOneByteVector(),
                          pat_vector,
                          start_index);
    }
    return SearchString(isolate,
                        seq_sub.ToUC16Vector(),
                        pat_vector,
                        start_index);
  }
  Vector<const uc16> pat_vector = seq_pat.ToUC16Vector();
  if (seq_sub.IsAscii()) {
    return SearchString(isolate,
                        seq_sub.ToOneByteVector(),
                        pat_vector,
                        start_index);
  }
  return SearchString(isolate,
                      seq_sub.ToUC16Vector(),
                      pat_vector,
                      start_index);
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_StringIndexOf) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 3);

  CONVERT_ARG_HANDLE_CHECKED(String, sub, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, pat, 1);

  Object* index = args[2];
  uint32_t start_index;
  if (!index->ToArrayIndex(&start_index)) return Smi::FromInt(-1);

  RUNTIME_ASSERT(start_index <= static_cast<uint32_t>(sub->length()));
  int position =
      Runtime::StringMatch(isolate, sub, pat, start_index);
  return Smi::FromInt(position);
}


template <typename schar, typename pchar>
static int StringMatchBackwards(Vector<const schar> subject,
                                Vector<const pchar> pattern,
                                int idx) {
  int pattern_length = pattern.length();
  ASSERT(pattern_length >= 1);
  ASSERT(idx + pattern_length <= subject.length());

  if (sizeof(schar) == 1 && sizeof(pchar) > 1) {
    for (int i = 0; i < pattern_length; i++) {
      uc16 c = pattern[i];
      if (c > String::kMaxOneByteCharCode) {
        return -1;
      }
    }
  }

  pchar pattern_first_char = pattern[0];
  for (int i = idx; i >= 0; i--) {
    if (subject[i] != pattern_first_char) continue;
    int j = 1;
    while (j < pattern_length) {
      if (pattern[j] != subject[i+j]) {
        break;
      }
      j++;
    }
    if (j == pattern_length) {
      return i;
    }
  }
  return -1;
}

RUNTIME_FUNCTION(MaybeObject*, Runtime_StringLastIndexOf) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 3);

  CONVERT_ARG_HANDLE_CHECKED(String, sub, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, pat, 1);

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

  if (!sub->IsFlat()) FlattenString(sub);
  if (!pat->IsFlat()) FlattenString(pat);

  int position = -1;
  DisallowHeapAllocation no_gc;  // ensure vectors stay valid

  String::FlatContent sub_content = sub->GetFlatContent();
  String::FlatContent pat_content = pat->GetFlatContent();

  if (pat_content.IsAscii()) {
    Vector<const uint8_t> pat_vector = pat_content.ToOneByteVector();
    if (sub_content.IsAscii()) {
      position = StringMatchBackwards(sub_content.ToOneByteVector(),
                                      pat_vector,
                                      start_index);
    } else {
      position = StringMatchBackwards(sub_content.ToUC16Vector(),
                                      pat_vector,
                                      start_index);
    }
  } else {
    Vector<const uc16> pat_vector = pat_content.ToUC16Vector();
    if (sub_content.IsAscii()) {
      position = StringMatchBackwards(sub_content.ToOneByteVector(),
                                      pat_vector,
                                      start_index);
    } else {
      position = StringMatchBackwards(sub_content.ToUC16Vector(),
                                      pat_vector,
                                      start_index);
    }
  }

  return Smi::FromInt(position);
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_StringLocaleCompare) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 2);

  CONVERT_ARG_CHECKED(String, str1, 0);
  CONVERT_ARG_CHECKED(String, str2, 1);

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

  ConsStringIteratorOp* op1 =
      isolate->runtime_state()->string_locale_compare_it1();
  ConsStringIteratorOp* op2 =
      isolate->runtime_state()->string_locale_compare_it2();
  // TODO(dcarney) Can do array compares here more efficiently.
  StringCharacterStream stream1(str1, op1);
  StringCharacterStream stream2(str2, op2);

  for (int i = 0; i < end; i++) {
    uint16_t char1 = stream1.GetNext();
    uint16_t char2 = stream2.GetNext();
    if (char1 != char2) return Smi::FromInt(char1 - char2);
  }

  return Smi::FromInt(str1_length - str2_length);
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_SubString) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 3);

  CONVERT_ARG_CHECKED(String, value, 0);
  int start, end;
  // We have a fast integer-only case here to avoid a conversion to double in
  // the common case where from and to are Smis.
  if (args[1]->IsSmi() && args[2]->IsSmi()) {
    CONVERT_SMI_ARG_CHECKED(from_number, 1);
    CONVERT_SMI_ARG_CHECKED(to_number, 2);
    start = from_number;
    end = to_number;
  } else {
    CONVERT_DOUBLE_ARG_CHECKED(from_number, 1);
    CONVERT_DOUBLE_ARG_CHECKED(to_number, 2);
    start = FastD2IChecked(from_number);
    end = FastD2IChecked(to_number);
  }
  RUNTIME_ASSERT(end >= start);
  RUNTIME_ASSERT(start >= 0);
  RUNTIME_ASSERT(end <= value->length());
  isolate->counters()->sub_string_runtime()->Increment();
  if (end - start == 1) {
     return isolate->heap()->LookupSingleCharacterStringFromCode(
         value->Get(start));
  }
  return value->SubString(start, end);
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_StringMatch) {
  HandleScope handles(isolate);
  ASSERT_EQ(3, args.length());

  CONVERT_ARG_HANDLE_CHECKED(String, subject, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSRegExp, regexp, 1);
  CONVERT_ARG_HANDLE_CHECKED(JSArray, regexp_info, 2);

  RegExpImpl::GlobalCache global_cache(regexp, subject, true, isolate);
  if (global_cache.HasException()) return Failure::Exception();

  int capture_count = regexp->CaptureCount();

  ZoneScope zone_scope(isolate->runtime_zone());
  ZoneList<int> offsets(8, zone_scope.zone());

  while (true) {
    int32_t* match = global_cache.FetchNext();
    if (match == NULL) break;
    offsets.Add(match[0], zone_scope.zone());  // start
    offsets.Add(match[1], zone_scope.zone());  // end
  }

  if (global_cache.HasException()) return Failure::Exception();

  if (offsets.length() == 0) {
    // Not a single match.
    return isolate->heap()->null_value();
  }

  RegExpImpl::SetLastMatchInfo(regexp_info,
                               subject,
                               capture_count,
                               global_cache.LastSuccessfulMatch());

  int matches = offsets.length() / 2;
  Handle<FixedArray> elements = isolate->factory()->NewFixedArray(matches);
  Handle<String> substring =
      isolate->factory()->NewSubString(subject, offsets.at(0), offsets.at(1));
  elements->set(0, *substring);
  for (int i = 1; i < matches; i++) {
    HandleScope temp_scope(isolate);
    int from = offsets.at(i * 2);
    int to = offsets.at(i * 2 + 1);
    Handle<String> substring =
        isolate->factory()->NewProperSubString(subject, from, to);
    elements->set(i, *substring);
  }
  Handle<JSArray> result = isolate->factory()->NewJSArrayWithElements(elements);
  result->set_length(Smi::FromInt(matches));
  return *result;
}


// Only called from Runtime_RegExpExecMultiple so it doesn't need to maintain
// separate last match info.  See comment on that function.
template<bool has_capture>
static MaybeObject* SearchRegExpMultiple(
    Isolate* isolate,
    Handle<String> subject,
    Handle<JSRegExp> regexp,
    Handle<JSArray> last_match_array,
    Handle<JSArray> result_array) {
  ASSERT(subject->IsFlat());
  ASSERT_NE(has_capture, regexp->CaptureCount() == 0);

  int capture_count = regexp->CaptureCount();
  int subject_length = subject->length();

  static const int kMinLengthToCache = 0x1000;

  if (subject_length > kMinLengthToCache) {
    Handle<Object> cached_answer(RegExpResultsCache::Lookup(
        isolate->heap(),
        *subject,
        regexp->data(),
        RegExpResultsCache::REGEXP_MULTIPLE_INDICES), isolate);
    if (*cached_answer != Smi::FromInt(0)) {
      Handle<FixedArray> cached_fixed_array =
          Handle<FixedArray>(FixedArray::cast(*cached_answer));
      // The cache FixedArray is a COW-array and can therefore be reused.
      isolate->factory()->SetContent(result_array, cached_fixed_array);
      // The actual length of the result array is stored in the last element of
      // the backing store (the backing FixedArray may have a larger capacity).
      Object* cached_fixed_array_last_element =
          cached_fixed_array->get(cached_fixed_array->length() - 1);
      Smi* js_array_length = Smi::cast(cached_fixed_array_last_element);
      result_array->set_length(js_array_length);
      RegExpImpl::SetLastMatchInfo(
          last_match_array, subject, capture_count, NULL);
      return *result_array;
    }
  }

  RegExpImpl::GlobalCache global_cache(regexp, subject, true, isolate);
  if (global_cache.HasException()) return Failure::Exception();

  Handle<FixedArray> result_elements;
  if (result_array->HasFastObjectElements()) {
    result_elements =
        Handle<FixedArray>(FixedArray::cast(result_array->elements()));
  }
  if (result_elements.is_null() || result_elements->length() < 16) {
    result_elements = isolate->factory()->NewFixedArrayWithHoles(16);
  }

  FixedArrayBuilder builder(result_elements);

  // Position to search from.
  int match_start = -1;
  int match_end = 0;
  bool first = true;

  // Two smis before and after the match, for very long strings.
  static const int kMaxBuilderEntriesPerRegExpMatch = 5;

  while (true) {
    int32_t* current_match = global_cache.FetchNext();
    if (current_match == NULL) break;
    match_start = current_match[0];
    builder.EnsureCapacity(kMaxBuilderEntriesPerRegExpMatch);
    if (match_end < match_start) {
      ReplacementStringBuilder::AddSubjectSlice(&builder,
                                                match_end,
                                                match_start);
    }
    match_end = current_match[1];
    {
      // Avoid accumulating new handles inside loop.
      HandleScope temp_scope(isolate);
      Handle<String> match;
      if (!first) {
        match = isolate->factory()->NewProperSubString(subject,
                                                       match_start,
                                                       match_end);
      } else {
        match = isolate->factory()->NewSubString(subject,
                                                 match_start,
                                                 match_end);
        first = false;
      }

      if (has_capture) {
        // Arguments array to replace function is match, captures, index and
        // subject, i.e., 3 + capture count in total.
        Handle<FixedArray> elements =
            isolate->factory()->NewFixedArray(3 + capture_count);

        elements->set(0, *match);
        for (int i = 1; i <= capture_count; i++) {
          int start = current_match[i * 2];
          if (start >= 0) {
            int end = current_match[i * 2 + 1];
            ASSERT(start <= end);
            Handle<String> substring =
                isolate->factory()->NewSubString(subject, start, end);
            elements->set(i, *substring);
          } else {
            ASSERT(current_match[i * 2 + 1] < 0);
            elements->set(i, isolate->heap()->undefined_value());
          }
        }
        elements->set(capture_count + 1, Smi::FromInt(match_start));
        elements->set(capture_count + 2, *subject);
        builder.Add(*isolate->factory()->NewJSArrayWithElements(elements));
      } else {
        builder.Add(*match);
      }
    }
  }

  if (global_cache.HasException()) return Failure::Exception();

  if (match_start >= 0) {
    // Finished matching, with at least one match.
    if (match_end < subject_length) {
      ReplacementStringBuilder::AddSubjectSlice(&builder,
                                                match_end,
                                                subject_length);
    }

    RegExpImpl::SetLastMatchInfo(
        last_match_array, subject, capture_count, NULL);

    if (subject_length > kMinLengthToCache) {
      // Store the length of the result array into the last element of the
      // backing FixedArray.
      builder.EnsureCapacity(1);
      Handle<FixedArray> fixed_array = builder.array();
      fixed_array->set(fixed_array->length() - 1,
                       Smi::FromInt(builder.length()));
      // Cache the result and turn the FixedArray into a COW array.
      RegExpResultsCache::Enter(isolate->heap(),
                                *subject,
                                regexp->data(),
                                *fixed_array,
                                RegExpResultsCache::REGEXP_MULTIPLE_INDICES);
    }
    return *builder.ToJSArray(result_array);
  } else {
    return isolate->heap()->null_value();  // No matches at all.
  }
}


// This is only called for StringReplaceGlobalRegExpWithFunction.  This sets
// lastMatchInfoOverride to maintain the last match info, so we don't need to
// set any other last match array info.
RUNTIME_FUNCTION(MaybeObject*, Runtime_RegExpExecMultiple) {
  HandleScope handles(isolate);
  ASSERT(args.length() == 4);

  CONVERT_ARG_HANDLE_CHECKED(String, subject, 1);
  if (!subject->IsFlat()) FlattenString(subject);
  CONVERT_ARG_HANDLE_CHECKED(JSRegExp, regexp, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSArray, last_match_info, 2);
  CONVERT_ARG_HANDLE_CHECKED(JSArray, result_array, 3);

  ASSERT(regexp->GetFlags().is_global());

  if (regexp->CaptureCount() == 0) {
    return SearchRegExpMultiple<false>(
        isolate, subject, regexp, last_match_info, result_array);
  } else {
    return SearchRegExpMultiple<true>(
        isolate, subject, regexp, last_match_info, result_array);
  }
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_NumberToRadixString) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 2);
  CONVERT_SMI_ARG_CHECKED(radix, 1);
  RUNTIME_ASSERT(2 <= radix && radix <= 36);

  // Fast case where the result is a one character string.
  if (args[0]->IsSmi()) {
    int value = args.smi_at(0);
    if (value >= 0 && value < radix) {
      // Character array used for conversion.
      static const char kCharTable[] = "0123456789abcdefghijklmnopqrstuvwxyz";
      return isolate->heap()->
          LookupSingleCharacterStringFromCode(kCharTable[value]);
    }
  }

  // Slow case.
  CONVERT_DOUBLE_ARG_CHECKED(value, 0);
  if (std::isnan(value)) {
    return *isolate->factory()->nan_string();
  }
  if (std::isinf(value)) {
    if (value < 0) {
      return *isolate->factory()->minus_infinity_string();
    }
    return *isolate->factory()->infinity_string();
  }
  char* str = DoubleToRadixCString(value, radix);
  MaybeObject* result =
      isolate->heap()->AllocateStringFromOneByte(CStrVector(str));
  DeleteArray(str);
  return result;
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_NumberToFixed) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 2);

  CONVERT_DOUBLE_ARG_CHECKED(value, 0);
  CONVERT_DOUBLE_ARG_CHECKED(f_number, 1);
  int f = FastD2IChecked(f_number);
  RUNTIME_ASSERT(f >= 0);
  char* str = DoubleToFixedCString(value, f);
  MaybeObject* res =
      isolate->heap()->AllocateStringFromOneByte(CStrVector(str));
  DeleteArray(str);
  return res;
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_NumberToExponential) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 2);

  CONVERT_DOUBLE_ARG_CHECKED(value, 0);
  CONVERT_DOUBLE_ARG_CHECKED(f_number, 1);
  int f = FastD2IChecked(f_number);
  RUNTIME_ASSERT(f >= -1 && f <= 20);
  char* str = DoubleToExponentialCString(value, f);
  MaybeObject* res =
      isolate->heap()->AllocateStringFromOneByte(CStrVector(str));
  DeleteArray(str);
  return res;
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_NumberToPrecision) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 2);

  CONVERT_DOUBLE_ARG_CHECKED(value, 0);
  CONVERT_DOUBLE_ARG_CHECKED(f_number, 1);
  int f = FastD2IChecked(f_number);
  RUNTIME_ASSERT(f >= 1 && f <= 21);
  char* str = DoubleToPrecisionCString(value, f);
  MaybeObject* res =
      isolate->heap()->AllocateStringFromOneByte(CStrVector(str));
  DeleteArray(str);
  return res;
}


// Returns a single character string where first character equals
// string->Get(index).
static Handle<Object> GetCharAt(Handle<String> string, uint32_t index) {
  if (index < static_cast<uint32_t>(string->length())) {
    string->TryFlatten();
    return LookupSingleCharacterStringFromCode(
        string->GetIsolate(),
        string->Get(index));
  }
  return Execution::CharAt(string, index);
}


MaybeObject* Runtime::GetElementOrCharAtOrFail(Isolate* isolate,
                                               Handle<Object> object,
                                               uint32_t index) {
  CALL_HEAP_FUNCTION_PASS_EXCEPTION(isolate,
      GetElementOrCharAt(isolate, object, index));
}


MaybeObject* Runtime::GetElementOrCharAt(Isolate* isolate,
                                         Handle<Object> object,
                                         uint32_t index) {
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
    return object->GetPrototype(isolate)->GetElement(index);
  }

  return object->GetElement(index);
}


MaybeObject* Runtime::HasObjectProperty(Isolate* isolate,
                                        Handle<JSReceiver> object,
                                        Handle<Object> key) {
  HandleScope scope(isolate);

  // Check if the given key is an array index.
  uint32_t index;
  if (key->ToArrayIndex(&index)) {
    return isolate->heap()->ToBoolean(object->HasElement(index));
  }

  // Convert the key to a name - possibly by calling back into JavaScript.
  Handle<Name> name;
  if (key->IsName()) {
    name = Handle<Name>::cast(key);
  } else {
    bool has_pending_exception = false;
    Handle<Object> converted =
        Execution::ToString(key, &has_pending_exception);
    if (has_pending_exception) return Failure::Exception();
    name = Handle<Name>::cast(converted);
  }

  return isolate->heap()->ToBoolean(object->HasProperty(*name));
}

MaybeObject* Runtime::GetObjectPropertyOrFail(
    Isolate* isolate,
    Handle<Object> object,
    Handle<Object> key) {
  CALL_HEAP_FUNCTION_PASS_EXCEPTION(isolate,
      GetObjectProperty(isolate, object, key));
}

MaybeObject* Runtime::GetObjectProperty(Isolate* isolate,
                                        Handle<Object> object,
                                        Handle<Object> key) {
  HandleScope scope(isolate);

  if (object->IsUndefined() || object->IsNull()) {
    Handle<Object> args[2] = { key, object };
    Handle<Object> error =
        isolate->factory()->NewTypeError("non_object_property_load",
                                         HandleVector(args, 2));
    return isolate->Throw(*error);
  }

  // Check if the given key is an array index.
  uint32_t index;
  if (key->ToArrayIndex(&index)) {
    return GetElementOrCharAt(isolate, object, index);
  }

  // Convert the key to a name - possibly by calling back into JavaScript.
  Handle<Name> name;
  if (key->IsName()) {
    name = Handle<Name>::cast(key);
  } else {
    bool has_pending_exception = false;
    Handle<Object> converted =
        Execution::ToString(key, &has_pending_exception);
    if (has_pending_exception) return Failure::Exception();
    name = Handle<Name>::cast(converted);
  }

  // Check if the name is trivially convertible to an index and get
  // the element if so.
  if (name->AsArrayIndex(&index)) {
    return GetElementOrCharAt(isolate, object, index);
  } else {
    return object->GetProperty(*name);
  }
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_GetProperty) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 2);

  Handle<Object> object = args.at<Object>(0);
  Handle<Object> key = args.at<Object>(1);

  return Runtime::GetObjectProperty(isolate, object, key);
}


// KeyedGetProperty is called from KeyedLoadIC::GenerateGeneric.
RUNTIME_FUNCTION(MaybeObject*, Runtime_KeyedGetProperty) {
  SealHandleScope shs(isolate);
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
  if (args[0]->IsJSObject()) {
    if (!args[0]->IsJSGlobalProxy() &&
        !args[0]->IsAccessCheckNeeded() &&
        args[1]->IsName()) {
      JSObject* receiver = JSObject::cast(args[0]);
      Name* key = Name::cast(args[1]);
      if (receiver->HasFastProperties()) {
        // Attempt to use lookup cache.
        Map* receiver_map = receiver->map();
        KeyedLookupCache* keyed_lookup_cache = isolate->keyed_lookup_cache();
        int offset = keyed_lookup_cache->Lookup(receiver_map, key);
        if (offset != -1) {
          // Doubles are not cached, so raw read the value.
          Object* value = receiver->RawFastPropertyAt(offset);
          return value->IsTheHole()
              ? isolate->heap()->undefined_value()
              : value;
        }
        // Lookup cache miss.  Perform lookup and update the cache if
        // appropriate.
        LookupResult result(isolate);
        receiver->LocalLookup(key, &result);
        if (result.IsField()) {
          int offset = result.GetFieldIndex().field_index();
          // Do not track double fields in the keyed lookup cache. Reading
          // double values requires boxing.
          if (!FLAG_track_double_fields ||
              !result.representation().IsDouble()) {
            keyed_lookup_cache->Update(receiver_map, key, offset);
          }
          return receiver->FastPropertyAt(result.representation(), offset);
        }
      } else {
        // Attempt dictionary lookup.
        NameDictionary* dictionary = receiver->property_dictionary();
        int entry = dictionary->FindEntry(key);
        if ((entry != NameDictionary::kNotFound) &&
            (dictionary->DetailsAt(entry).type() == NORMAL)) {
          Object* value = dictionary->ValueAt(entry);
          if (!receiver->IsGlobalObject()) return value;
          value = PropertyCell::cast(value)->value();
          if (!value->IsTheHole()) return value;
          // If value is the hole do the general lookup.
        }
      }
    } else if (FLAG_smi_only_arrays && args.at<Object>(1)->IsSmi()) {
      // JSObject without a name key. If the key is a Smi, check for a
      // definite out-of-bounds access to elements, which is a strong indicator
      // that subsequent accesses will also call the runtime. Proactively
      // transition elements to FAST_*_ELEMENTS to avoid excessive boxing of
      // doubles for those future calls in the case that the elements would
      // become FAST_DOUBLE_ELEMENTS.
      Handle<JSObject> js_object(args.at<JSObject>(0));
      ElementsKind elements_kind = js_object->GetElementsKind();
      if (IsFastDoubleElementsKind(elements_kind)) {
        FixedArrayBase* elements = js_object->elements();
        if (args.at<Smi>(1)->value() >= elements->length()) {
          if (IsFastHoleyElementsKind(elements_kind)) {
            elements_kind = FAST_HOLEY_ELEMENTS;
          } else {
            elements_kind = FAST_ELEMENTS;
          }
          MaybeObject* maybe_object = TransitionElements(js_object,
                                                         elements_kind,
                                                         isolate);
          if (maybe_object->IsFailure()) return maybe_object;
        }
      } else {
        ASSERT(IsFastSmiOrObjectElementsKind(elements_kind) ||
               !IsFastElementsKind(elements_kind));
      }
    }
  } else if (args[0]->IsString() && args[1]->IsSmi()) {
    // Fast case for string indexing using [] with a smi index.
    HandleScope scope(isolate);
    Handle<String> str = args.at<String>(0);
    int index = args.smi_at(1);
    if (index >= 0 && index < str->length()) {
      Handle<Object> result = GetCharAt(str, index);
      return *result;
    }
  }

  // Fall back to GetObjectProperty.
  return Runtime::GetObjectProperty(isolate,
                                    args.at<Object>(0),
                                    args.at<Object>(1));
}


static bool IsValidAccessor(Handle<Object> obj) {
  return obj->IsUndefined() || obj->IsSpecFunction() || obj->IsNull();
}


// Implements part of 8.12.9 DefineOwnProperty.
// There are 3 cases that lead here:
// Step 4b - define a new accessor property.
// Steps 9c & 12 - replace an existing data property with an accessor property.
// Step 12 - update an existing accessor property with an accessor or generic
//           descriptor.
RUNTIME_FUNCTION(MaybeObject*, Runtime_DefineOrRedefineAccessorProperty) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 5);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, obj, 0);
  RUNTIME_ASSERT(!obj->IsNull());
  CONVERT_ARG_HANDLE_CHECKED(Name, name, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, getter, 2);
  RUNTIME_ASSERT(IsValidAccessor(getter));
  CONVERT_ARG_HANDLE_CHECKED(Object, setter, 3);
  RUNTIME_ASSERT(IsValidAccessor(setter));
  CONVERT_SMI_ARG_CHECKED(unchecked, 4);
  RUNTIME_ASSERT((unchecked & ~(READ_ONLY | DONT_ENUM | DONT_DELETE)) == 0);
  PropertyAttributes attr = static_cast<PropertyAttributes>(unchecked);

  bool fast = obj->HasFastProperties();
  JSObject::DefineAccessor(obj, name, getter, setter, attr);
  if (fast) JSObject::TransformToFastProperties(obj, 0);
  return isolate->heap()->undefined_value();
}

// Implements part of 8.12.9 DefineOwnProperty.
// There are 3 cases that lead here:
// Step 4a - define a new data property.
// Steps 9b & 12 - replace an existing accessor property with a data property.
// Step 12 - update an existing data property with a data or generic
//           descriptor.
RUNTIME_FUNCTION(MaybeObject*, Runtime_DefineOrRedefineDataProperty) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 4);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, js_object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Name, name, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, obj_value, 2);
  CONVERT_SMI_ARG_CHECKED(unchecked, 3);
  RUNTIME_ASSERT((unchecked & ~(READ_ONLY | DONT_ENUM | DONT_DELETE)) == 0);
  PropertyAttributes attr = static_cast<PropertyAttributes>(unchecked);

  LookupResult result(isolate);
  js_object->LocalLookupRealNamedProperty(*name, &result);

  // Special case for callback properties.
  if (result.IsPropertyCallbacks()) {
    Object* callback = result.GetCallbackObject();
    // To be compatible with Safari we do not change the value on API objects
    // in Object.defineProperty(). Firefox disagrees here, and actually changes
    // the value.
    if (callback->IsAccessorInfo()) {
      return isolate->heap()->undefined_value();
    }
    // Avoid redefining foreign callback as data property, just use the stored
    // setter to update the value instead.
    // TODO(mstarzinger): So far this only works if property attributes don't
    // change, this should be fixed once we cleanup the underlying code.
    if (callback->IsForeign() && result.GetAttributes() == attr) {
      return js_object->SetPropertyWithCallback(callback,
                                                *name,
                                                *obj_value,
                                                result.holder(),
                                                kStrictMode);
    }
  }

  // Take special care when attributes are different and there is already
  // a property. For simplicity we normalize the property which enables us
  // to not worry about changing the instance_descriptor and creating a new
  // map. The current version of SetObjectProperty does not handle attributes
  // correctly in the case where a property is a field and is reset with
  // new attributes.
  if (result.IsFound() &&
      (attr != result.GetAttributes() || result.IsPropertyCallbacks())) {
    // New attributes - normalize to avoid writing to instance descriptor
    if (js_object->IsJSGlobalProxy()) {
      // Since the result is a property, the prototype will exist so
      // we don't have to check for null.
      js_object = Handle<JSObject>(JSObject::cast(js_object->GetPrototype()));
    }
    JSObject::NormalizeProperties(js_object, CLEAR_INOBJECT_PROPERTIES, 0);
    // Use IgnoreAttributes version since a readonly property may be
    // overridden and SetProperty does not allow this.
    return js_object->SetLocalPropertyIgnoreAttributes(*name,
                                                       *obj_value,
                                                       attr);
  }

  return Runtime::ForceSetObjectProperty(isolate,
                                         js_object,
                                         name,
                                         obj_value,
                                         attr);
}


// Return property without being observable by accessors or interceptors.
RUNTIME_FUNCTION(MaybeObject*, Runtime_GetDataProperty) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Name, key, 1);
  LookupResult lookup(isolate);
  object->LookupRealNamedProperty(*key, &lookup);
  if (!lookup.IsFound()) return isolate->heap()->undefined_value();
  switch (lookup.type()) {
    case NORMAL:
      return lookup.holder()->GetNormalizedProperty(&lookup);
    case FIELD:
      return lookup.holder()->FastPropertyAt(
          lookup.representation(),
          lookup.GetFieldIndex().field_index());
    case CONSTANT_FUNCTION:
      return lookup.GetConstantFunction();
    case CALLBACKS:
    case HANDLER:
    case INTERCEPTOR:
    case TRANSITION:
      return isolate->heap()->undefined_value();
    case NONEXISTENT:
      UNREACHABLE();
  }
  return isolate->heap()->undefined_value();
}


MaybeObject* Runtime::SetObjectPropertyOrFail(
    Isolate* isolate,
    Handle<Object> object,
    Handle<Object> key,
    Handle<Object> value,
    PropertyAttributes attr,
    StrictModeFlag strict_mode) {
  CALL_HEAP_FUNCTION_PASS_EXCEPTION(isolate,
      SetObjectProperty(isolate, object, key, value, attr, strict_mode));
}


MaybeObject* Runtime::SetObjectProperty(Isolate* isolate,
                                        Handle<Object> object,
                                        Handle<Object> key,
                                        Handle<Object> value,
                                        PropertyAttributes attr,
                                        StrictModeFlag strict_mode) {
  SetPropertyMode set_mode = attr == NONE ? SET_PROPERTY : DEFINE_PROPERTY;
  HandleScope scope(isolate);

  if (object->IsUndefined() || object->IsNull()) {
    Handle<Object> args[2] = { key, object };
    Handle<Object> error =
        isolate->factory()->NewTypeError("non_object_property_store",
                                         HandleVector(args, 2));
    return isolate->Throw(*error);
  }

  if (object->IsJSProxy()) {
    bool has_pending_exception = false;
    Handle<Object> name = key->IsSymbol()
        ? key : Execution::ToString(key, &has_pending_exception);
    if (has_pending_exception) return Failure::Exception();
    return JSProxy::cast(*object)->SetProperty(
        Name::cast(*name), *value, attr, strict_mode);
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

    js_object->ValidateElements();
    if (js_object->HasExternalArrayElements()) {
      if (!value->IsNumber() && !value->IsUndefined()) {
        bool has_exception;
        Handle<Object> number = Execution::ToNumber(value, &has_exception);
        if (has_exception) return Failure::Exception();
        value = number;
      }
    }
    MaybeObject* result = js_object->SetElement(
        index, *value, attr, strict_mode, true, set_mode);
    js_object->ValidateElements();
    if (result->IsFailure()) return result;
    return *value;
  }

  if (key->IsName()) {
    MaybeObject* result;
    Handle<Name> name = Handle<Name>::cast(key);
    if (name->AsArrayIndex(&index)) {
      if (js_object->HasExternalArrayElements()) {
        if (!value->IsNumber() && !value->IsUndefined()) {
          bool has_exception;
          Handle<Object> number = Execution::ToNumber(value, &has_exception);
          if (has_exception) return Failure::Exception();
          value = number;
        }
      }
      result = js_object->SetElement(
          index, *value, attr, strict_mode, true, set_mode);
    } else {
      if (name->IsString()) Handle<String>::cast(name)->TryFlatten();
      result = js_object->SetProperty(*name, *value, attr, strict_mode);
    }
    if (result->IsFailure()) return result;
    return *value;
  }

  // Call-back into JavaScript to convert the key to a string.
  bool has_pending_exception = false;
  Handle<Object> converted = Execution::ToString(key, &has_pending_exception);
  if (has_pending_exception) return Failure::Exception();
  Handle<String> name = Handle<String>::cast(converted);

  if (name->AsArrayIndex(&index)) {
    return js_object->SetElement(
        index, *value, attr, strict_mode, true, set_mode);
  } else {
    return js_object->SetProperty(*name, *value, attr, strict_mode);
  }
}


MaybeObject* Runtime::ForceSetObjectProperty(Isolate* isolate,
                                             Handle<JSObject> js_object,
                                             Handle<Object> key,
                                             Handle<Object> value,
                                             PropertyAttributes attr) {
  HandleScope scope(isolate);

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

    return js_object->SetElement(
        index, *value, attr, kNonStrictMode, false, DEFINE_PROPERTY);
  }

  if (key->IsName()) {
    Handle<Name> name = Handle<Name>::cast(key);
    if (name->AsArrayIndex(&index)) {
      return js_object->SetElement(
          index, *value, attr, kNonStrictMode, false, DEFINE_PROPERTY);
    } else {
      if (name->IsString()) Handle<String>::cast(name)->TryFlatten();
      return js_object->SetLocalPropertyIgnoreAttributes(*name, *value, attr);
    }
  }

  // Call-back into JavaScript to convert the key to a string.
  bool has_pending_exception = false;
  Handle<Object> converted = Execution::ToString(key, &has_pending_exception);
  if (has_pending_exception) return Failure::Exception();
  Handle<String> name = Handle<String>::cast(converted);

  if (name->AsArrayIndex(&index)) {
    return js_object->SetElement(
        index, *value, attr, kNonStrictMode, false, DEFINE_PROPERTY);
  } else {
    return js_object->SetLocalPropertyIgnoreAttributes(*name, *value, attr);
  }
}


MaybeObject* Runtime::DeleteObjectProperty(Isolate* isolate,
                                           Handle<JSReceiver> receiver,
                                           Handle<Object> key,
                                           JSReceiver::DeleteMode mode) {
  HandleScope scope(isolate);

  // Check if the given key is an array index.
  uint32_t index;
  if (key->ToArrayIndex(&index)) {
    // In Firefox/SpiderMonkey, Safari and Opera you can access the
    // characters of a string using [] notation.  In the case of a
    // String object we just need to redirect the deletion to the
    // underlying string if the index is in range.  Since the
    // underlying string does nothing with the deletion, we can ignore
    // such deletions.
    if (receiver->IsStringObjectWithCharacterAt(index)) {
      return isolate->heap()->true_value();
    }

    return receiver->DeleteElement(index, mode);
  }

  Handle<Name> name;
  if (key->IsName()) {
    name = Handle<Name>::cast(key);
  } else {
    // Call-back into JavaScript to convert the key to a string.
    bool has_pending_exception = false;
    Handle<Object> converted = Execution::ToString(key, &has_pending_exception);
    if (has_pending_exception) return Failure::Exception();
    name = Handle<String>::cast(converted);
  }

  if (name->IsString()) Handle<String>::cast(name)->TryFlatten();
  return receiver->DeleteProperty(*name, mode);
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_SetProperty) {
  SealHandleScope shs(isolate);
  RUNTIME_ASSERT(args.length() == 4 || args.length() == 5);

  Handle<Object> object = args.at<Object>(0);
  Handle<Object> key = args.at<Object>(1);
  Handle<Object> value = args.at<Object>(2);
  CONVERT_SMI_ARG_CHECKED(unchecked_attributes, 3);
  RUNTIME_ASSERT(
      (unchecked_attributes & ~(READ_ONLY | DONT_ENUM | DONT_DELETE)) == 0);
  // Compute attributes.
  PropertyAttributes attributes =
      static_cast<PropertyAttributes>(unchecked_attributes);

  StrictModeFlag strict_mode = kNonStrictMode;
  if (args.length() == 5) {
    CONVERT_STRICT_MODE_ARG_CHECKED(strict_mode_flag, 4);
    strict_mode = strict_mode_flag;
  }

  return Runtime::SetObjectProperty(isolate,
                                    object,
                                    key,
                                    value,
                                    attributes,
                                    strict_mode);
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_TransitionElementsKind) {
  HandleScope scope(isolate);
  RUNTIME_ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSArray, array, 0);
  CONVERT_ARG_HANDLE_CHECKED(Map, map, 1);
  JSObject::TransitionElementsKind(array, map->elements_kind());
  return *array;
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_TransitionElementsSmiToDouble) {
  SealHandleScope shs(isolate);
  RUNTIME_ASSERT(args.length() == 1);
  Handle<Object> object = args.at<Object>(0);
  if (object->IsJSObject()) {
    Handle<JSObject> js_object(Handle<JSObject>::cast(object));
    ASSERT(!js_object->map()->is_observed());
    ElementsKind new_kind = js_object->HasFastHoleyElements()
        ? FAST_HOLEY_DOUBLE_ELEMENTS
        : FAST_DOUBLE_ELEMENTS;
    return TransitionElements(object, new_kind, isolate);
  } else {
    return *object;
  }
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_TransitionElementsDoubleToObject) {
  SealHandleScope shs(isolate);
  RUNTIME_ASSERT(args.length() == 1);
  Handle<Object> object = args.at<Object>(0);
  if (object->IsJSObject()) {
    Handle<JSObject> js_object(Handle<JSObject>::cast(object));
    ASSERT(!js_object->map()->is_observed());
    ElementsKind new_kind = js_object->HasFastHoleyElements()
        ? FAST_HOLEY_ELEMENTS
        : FAST_ELEMENTS;
    return TransitionElements(object, new_kind, isolate);
  } else {
    return *object;
  }
}


// Set the native flag on the function.
// This is used to decide if we should transform null and undefined
// into the global object when doing call and apply.
RUNTIME_FUNCTION(MaybeObject*, Runtime_SetNativeFlag) {
  SealHandleScope shs(isolate);
  RUNTIME_ASSERT(args.length() == 1);

  Handle<Object> object = args.at<Object>(0);

  if (object->IsJSFunction()) {
    JSFunction* func = JSFunction::cast(*object);
    func->shared()->set_native(true);
  }
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_StoreArrayLiteralElement) {
  HandleScope scope(isolate);
  RUNTIME_ASSERT(args.length() == 5);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 0);
  CONVERT_SMI_ARG_CHECKED(store_index, 1);
  Handle<Object> value = args.at<Object>(2);
  CONVERT_ARG_HANDLE_CHECKED(FixedArray, literals, 3);
  CONVERT_SMI_ARG_CHECKED(literal_index, 4);

  Object* raw_boilerplate_object = literals->get(literal_index);
  Handle<JSArray> boilerplate_object(JSArray::cast(raw_boilerplate_object));
  ElementsKind elements_kind = object->GetElementsKind();
  ASSERT(IsFastElementsKind(elements_kind));
  // Smis should never trigger transitions.
  ASSERT(!value->IsSmi());

  if (value->IsNumber()) {
    ASSERT(IsFastSmiElementsKind(elements_kind));
    ElementsKind transitioned_kind = IsFastHoleyElementsKind(elements_kind)
        ? FAST_HOLEY_DOUBLE_ELEMENTS
        : FAST_DOUBLE_ELEMENTS;
    if (IsMoreGeneralElementsKindTransition(
            boilerplate_object->GetElementsKind(),
            transitioned_kind)) {
      JSObject::TransitionElementsKind(boilerplate_object, transitioned_kind);
    }
    JSObject::TransitionElementsKind(object, transitioned_kind);
    ASSERT(IsFastDoubleElementsKind(object->GetElementsKind()));
    FixedDoubleArray* double_array = FixedDoubleArray::cast(object->elements());
    HeapNumber* number = HeapNumber::cast(*value);
    double_array->set(store_index, number->Number());
  } else {
    ASSERT(IsFastSmiElementsKind(elements_kind) ||
           IsFastDoubleElementsKind(elements_kind));
    ElementsKind transitioned_kind = IsFastHoleyElementsKind(elements_kind)
        ? FAST_HOLEY_ELEMENTS
        : FAST_ELEMENTS;
    JSObject::TransitionElementsKind(object, transitioned_kind);
    if (IsMoreGeneralElementsKindTransition(
            boilerplate_object->GetElementsKind(),
            transitioned_kind)) {
      JSObject::TransitionElementsKind(boilerplate_object, transitioned_kind);
    }
    FixedArray* object_array = FixedArray::cast(object->elements());
    object_array->set(store_index, *value);
  }
  return *object;
}


// Check whether debugger and is about to step into the callback that is passed
// to a built-in function such as Array.forEach.
RUNTIME_FUNCTION(MaybeObject*, Runtime_DebugCallbackSupportsStepping) {
  SealHandleScope shs(isolate);
#ifdef ENABLE_DEBUGGER_SUPPORT
  if (!isolate->IsDebuggerActive() || !isolate->debug()->StepInActive()) {
    return isolate->heap()->false_value();
  }
  CONVERT_ARG_CHECKED(Object, callback, 0);
  // We do not step into the callback if it's a builtin or not even a function.
  if (!callback->IsJSFunction() || JSFunction::cast(callback)->IsBuiltin()) {
    return isolate->heap()->false_value();
  }
  return isolate->heap()->true_value();
#else
  return isolate->heap()->false_value();
#endif  // ENABLE_DEBUGGER_SUPPORT
}


// Set one shot breakpoints for the callback function that is passed to a
// built-in function such as Array.forEach to enable stepping into the callback.
RUNTIME_FUNCTION(MaybeObject*, Runtime_DebugPrepareStepInIfStepping) {
  SealHandleScope shs(isolate);
#ifdef ENABLE_DEBUGGER_SUPPORT
  Debug* debug = isolate->debug();
  if (!debug->IsStepping()) return isolate->heap()->undefined_value();
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, callback, 0);
  HandleScope scope(isolate);
  // When leaving the callback, step out has been activated, but not performed
  // if we do not leave the builtin.  To be able to step into the callback
  // again, we need to clear the step out at this point.
  debug->ClearStepOut();
  debug->FloodWithOneShot(callback);
#endif  // ENABLE_DEBUGGER_SUPPORT
  return isolate->heap()->undefined_value();
}


// Set a local property, even if it is READ_ONLY.  If the property does not
// exist, it will be added with attributes NONE.
RUNTIME_FUNCTION(MaybeObject*, Runtime_IgnoreAttributesAndSetProperty) {
  SealHandleScope shs(isolate);
  RUNTIME_ASSERT(args.length() == 3 || args.length() == 4);
  CONVERT_ARG_CHECKED(JSObject, object, 0);
  CONVERT_ARG_CHECKED(Name, name, 1);
  // Compute attributes.
  PropertyAttributes attributes = NONE;
  if (args.length() == 4) {
    CONVERT_SMI_ARG_CHECKED(unchecked_value, 3);
    // Only attribute bits should be set.
    RUNTIME_ASSERT(
        (unchecked_value & ~(READ_ONLY | DONT_ENUM | DONT_DELETE)) == 0);
    attributes = static_cast<PropertyAttributes>(unchecked_value);
  }

  return object->
      SetLocalPropertyIgnoreAttributes(name, args[2], attributes);
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_DeleteProperty) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 3);

  CONVERT_ARG_CHECKED(JSReceiver, object, 0);
  CONVERT_ARG_CHECKED(Name, key, 1);
  CONVERT_STRICT_MODE_ARG_CHECKED(strict_mode, 2);
  return object->DeleteProperty(key, (strict_mode == kStrictMode)
                                      ? JSReceiver::STRICT_DELETION
                                      : JSReceiver::NORMAL_DELETION);
}


static Object* HasLocalPropertyImplementation(Isolate* isolate,
                                              Handle<JSObject> object,
                                              Handle<Name> key) {
  if (object->HasLocalProperty(*key)) return isolate->heap()->true_value();
  // Handle hidden prototypes.  If there's a hidden prototype above this thing
  // then we have to check it for properties, because they are supposed to
  // look like they are on this object.
  Handle<Object> proto(object->GetPrototype(), isolate);
  if (proto->IsJSObject() &&
      Handle<JSObject>::cast(proto)->map()->is_hidden_prototype()) {
    return HasLocalPropertyImplementation(isolate,
                                          Handle<JSObject>::cast(proto),
                                          key);
  }
  return isolate->heap()->false_value();
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_HasLocalProperty) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_CHECKED(Name, key, 1);

  uint32_t index;
  const bool key_is_array_index = key->AsArrayIndex(&index);

  Object* obj = args[0];
  // Only JS objects can have properties.
  if (obj->IsJSObject()) {
    JSObject* object = JSObject::cast(obj);
    // Fast case: either the key is a real named property or it is not
    // an array index and there are no interceptors or hidden
    // prototypes.
    if (object->HasRealNamedProperty(isolate, key))
      return isolate->heap()->true_value();
    Map* map = object->map();
    if (!key_is_array_index &&
        !map->has_named_interceptor() &&
        !HeapObject::cast(map->prototype())->map()->is_hidden_prototype()) {
      return isolate->heap()->false_value();
    }
    // Slow case.
    HandleScope scope(isolate);
    return HasLocalPropertyImplementation(isolate,
                                          Handle<JSObject>(object),
                                          Handle<Name>(key));
  } else if (obj->IsString() && key_is_array_index) {
    // Well, there is one exception:  Handle [] on strings.
    String* string = String::cast(obj);
    if (index < static_cast<uint32_t>(string->length())) {
      return isolate->heap()->true_value();
    }
  }
  return isolate->heap()->false_value();
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_HasProperty) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_CHECKED(JSReceiver, receiver, 0);
  CONVERT_ARG_CHECKED(Name, key, 1);

  bool result = receiver->HasProperty(key);
  if (isolate->has_pending_exception()) return Failure::Exception();
  return isolate->heap()->ToBoolean(result);
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_HasElement) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_CHECKED(JSReceiver, receiver, 0);
  CONVERT_SMI_ARG_CHECKED(index, 1);

  bool result = receiver->HasElement(index);
  if (isolate->has_pending_exception()) return Failure::Exception();
  return isolate->heap()->ToBoolean(result);
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_IsPropertyEnumerable) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 2);

  CONVERT_ARG_CHECKED(JSObject, object, 0);
  CONVERT_ARG_CHECKED(Name, key, 1);

  PropertyAttributes att = object->GetLocalPropertyAttribute(key);
  return isolate->heap()->ToBoolean(att != ABSENT && (att & DONT_ENUM) == 0);
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_GetPropertyNames) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, object, 0);
  bool threw = false;
  Handle<JSArray> result = GetKeysFor(object, &threw);
  if (threw) return Failure::Exception();
  return *result;
}


// Returns either a FixedArray as Runtime_GetPropertyNames,
// or, if the given object has an enum cache that contains
// all enumerable properties of the object and its prototypes
// have none, the map of the object. This is used to speed up
// the check for deletions during a for-in.
RUNTIME_FUNCTION(MaybeObject*, Runtime_GetPropertyNamesFast) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);

  CONVERT_ARG_CHECKED(JSReceiver, raw_object, 0);

  if (raw_object->IsSimpleEnum()) return raw_object->map();

  HandleScope scope(isolate);
  Handle<JSReceiver> object(raw_object);
  bool threw = false;
  Handle<FixedArray> content =
      GetKeysInFixedArrayFor(object, INCLUDE_PROTOS, &threw);
  if (threw) return Failure::Exception();

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
RUNTIME_FUNCTION(MaybeObject*, Runtime_GetLocalPropertyNames) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  if (!args[0]->IsJSObject()) {
    return isolate->heap()->undefined_value();
  }
  CONVERT_ARG_HANDLE_CHECKED(JSObject, obj, 0);
  CONVERT_BOOLEAN_ARG_CHECKED(include_symbols, 1);
  PropertyAttributes filter = include_symbols ? NONE : SYMBOLIC;

  // Skip the global proxy as it has no properties and always delegates to the
  // real global object.
  if (obj->IsJSGlobalProxy()) {
    // Only collect names if access is permitted.
    if (obj->IsAccessCheckNeeded() &&
        !isolate->MayNamedAccess(*obj,
                                 isolate->heap()->undefined_value(),
                                 v8::ACCESS_KEYS)) {
      isolate->ReportFailedAccessCheck(*obj, v8::ACCESS_KEYS);
      return *isolate->factory()->NewJSArray(0);
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
        !isolate->MayNamedAccess(*jsproto,
                                 isolate->heap()->undefined_value(),
                                 v8::ACCESS_KEYS)) {
      isolate->ReportFailedAccessCheck(*jsproto, v8::ACCESS_KEYS);
      return *isolate->factory()->NewJSArray(0);
    }
    int n;
    n = jsproto->NumberOfLocalProperties(filter);
    local_property_count[i] = n;
    total_property_count += n;
    if (i < length - 1) {
      jsproto = Handle<JSObject>(JSObject::cast(jsproto->GetPrototype()));
    }
  }

  // Allocate an array with storage for all the property names.
  Handle<FixedArray> names =
      isolate->factory()->NewFixedArray(total_property_count);

  // Get the property names.
  jsproto = obj;
  int proto_with_hidden_properties = 0;
  int next_copy_index = 0;
  for (int i = 0; i < length; i++) {
    jsproto->GetLocalPropertyNames(*names, next_copy_index, filter);
    next_copy_index += local_property_count[i];
    if (jsproto->HasHiddenProperties()) {
      proto_with_hidden_properties++;
    }
    if (i < length - 1) {
      jsproto = Handle<JSObject>(JSObject::cast(jsproto->GetPrototype()));
    }
  }

  // Filter out name of hidden properties object.
  if (proto_with_hidden_properties > 0) {
    Handle<FixedArray> old_names = names;
    names = isolate->factory()->NewFixedArray(
        names->length() - proto_with_hidden_properties);
    int dest_pos = 0;
    for (int i = 0; i < total_property_count; i++) {
      Object* name = old_names->get(i);
      if (name == isolate->heap()->hidden_string()) {
        continue;
      }
      names->set(dest_pos++, name);
    }
  }

  return *isolate->factory()->NewJSArrayWithElements(names);
}


// Return the names of the local indexed properties.
// args[0]: object
RUNTIME_FUNCTION(MaybeObject*, Runtime_GetLocalElementNames) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  if (!args[0]->IsJSObject()) {
    return isolate->heap()->undefined_value();
  }
  CONVERT_ARG_HANDLE_CHECKED(JSObject, obj, 0);

  int n = obj->NumberOfLocalElements(static_cast<PropertyAttributes>(NONE));
  Handle<FixedArray> names = isolate->factory()->NewFixedArray(n);
  obj->GetLocalElementKeys(*names, static_cast<PropertyAttributes>(NONE));
  return *isolate->factory()->NewJSArrayWithElements(names);
}


// Return information on whether an object has a named or indexed interceptor.
// args[0]: object
RUNTIME_FUNCTION(MaybeObject*, Runtime_GetInterceptorInfo) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  if (!args[0]->IsJSObject()) {
    return Smi::FromInt(0);
  }
  CONVERT_ARG_HANDLE_CHECKED(JSObject, obj, 0);

  int result = 0;
  if (obj->HasNamedInterceptor()) result |= 2;
  if (obj->HasIndexedInterceptor()) result |= 1;

  return Smi::FromInt(result);
}


// Return property names from named interceptor.
// args[0]: object
RUNTIME_FUNCTION(MaybeObject*, Runtime_GetNamedInterceptorPropertyNames) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, obj, 0);

  if (obj->HasNamedInterceptor()) {
    v8::Handle<v8::Array> result = GetKeysForNamedInterceptor(obj, obj);
    if (!result.IsEmpty()) return *v8::Utils::OpenHandle(*result);
  }
  return isolate->heap()->undefined_value();
}


// Return element names from indexed interceptor.
// args[0]: object
RUNTIME_FUNCTION(MaybeObject*, Runtime_GetIndexedInterceptorElementNames) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, obj, 0);

  if (obj->HasIndexedInterceptor()) {
    v8::Handle<v8::Array> result = GetKeysForIndexedInterceptor(obj, obj);
    if (!result.IsEmpty()) return *v8::Utils::OpenHandle(*result);
  }
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_LocalKeys) {
  HandleScope scope(isolate);
  ASSERT_EQ(args.length(), 1);
  CONVERT_ARG_CHECKED(JSObject, raw_object, 0);
  Handle<JSObject> object(raw_object);

  if (object->IsJSGlobalProxy()) {
    // Do access checks before going to the global object.
    if (object->IsAccessCheckNeeded() &&
        !isolate->MayNamedAccess(*object, isolate->heap()->undefined_value(),
                             v8::ACCESS_KEYS)) {
      isolate->ReportFailedAccessCheck(*object, v8::ACCESS_KEYS);
      return *isolate->factory()->NewJSArray(0);
    }

    Handle<Object> proto(object->GetPrototype(), isolate);
    // If proxy is detached we simply return an empty array.
    if (proto->IsNull()) return *isolate->factory()->NewJSArray(0);
    object = Handle<JSObject>::cast(proto);
  }

  bool threw = false;
  Handle<FixedArray> contents =
      GetKeysInFixedArrayFor(object, LOCAL_ONLY, &threw);
  if (threw) return Failure::Exception();

  // Some fast paths through GetKeysInFixedArrayFor reuse a cached
  // property array and since the result is mutable we have to create
  // a fresh clone on each invocation.
  int length = contents->length();
  Handle<FixedArray> copy = isolate->factory()->NewFixedArray(length);
  for (int i = 0; i < length; i++) {
    Object* entry = contents->get(i);
    if (entry->IsString()) {
      copy->set(i, entry);
    } else {
      ASSERT(entry->IsNumber());
      HandleScope scope(isolate);
      Handle<Object> entry_handle(entry, isolate);
      Handle<Object> entry_str =
          isolate->factory()->NumberToString(entry_handle);
      copy->set(i, *entry_str);
    }
  }
  return *isolate->factory()->NewJSArrayWithElements(copy);
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_GetArgumentsProperty) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);

  // Compute the frame holding the arguments.
  JavaScriptFrameIterator it(isolate);
  it.AdvanceToArgumentsFrame();
  JavaScriptFrame* frame = it.frame();

  // Get the actual number of provided arguments.
  const uint32_t n = frame->ComputeParametersCount();

  // Try to convert the key to an index. If successful and within
  // index return the the argument from the frame.
  uint32_t index;
  if (args[0]->ToArrayIndex(&index) && index < n) {
    return frame->GetParameter(index);
  }

  if (args[0]->IsSymbol()) {
    // Lookup in the initial Object.prototype object.
    return isolate->initial_object_prototype()->GetProperty(
        Symbol::cast(args[0]));
  }

  // Convert the key to a string.
  HandleScope scope(isolate);
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
      return isolate->initial_object_prototype()->GetElement(index);
    }
  }

  // Handle special arguments properties.
  if (key->Equals(isolate->heap()->length_string())) return Smi::FromInt(n);
  if (key->Equals(isolate->heap()->callee_string())) {
    Object* function = frame->function();
    if (function->IsJSFunction() &&
        !JSFunction::cast(function)->shared()->is_classic_mode()) {
      return isolate->Throw(*isolate->factory()->NewTypeError(
          "strict_arguments_callee", HandleVector<Object>(NULL, 0)));
    }
    return function;
  }

  // Lookup in the initial Object.prototype object.
  return isolate->initial_object_prototype()->GetProperty(*key);
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_ToFastProperties) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  Object* object = args[0];
  return (object->IsJSObject() && !object->IsGlobalObject())
      ? JSObject::cast(object)->TransformToFastProperties(0)
      : object;
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_ToBool) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);

  return isolate->heap()->ToBoolean(args[0]->BooleanValue());
}


// Returns the type string of a value; see ECMA-262, 11.4.3 (p 47).
// Possible optimizations: put the type string into the oddballs.
RUNTIME_FUNCTION(MaybeObject*, Runtime_Typeof) {
  SealHandleScope shs(isolate);

  Object* obj = args[0];
  if (obj->IsNumber()) return isolate->heap()->number_string();
  HeapObject* heap_obj = HeapObject::cast(obj);

  // typeof an undetectable object is 'undefined'
  if (heap_obj->map()->is_undetectable()) {
    return isolate->heap()->undefined_string();
  }

  InstanceType instance_type = heap_obj->map()->instance_type();
  if (instance_type < FIRST_NONSTRING_TYPE) {
    return isolate->heap()->string_string();
  }

  switch (instance_type) {
    case ODDBALL_TYPE:
      if (heap_obj->IsTrue() || heap_obj->IsFalse()) {
        return isolate->heap()->boolean_string();
      }
      if (heap_obj->IsNull()) {
        return FLAG_harmony_typeof
            ? isolate->heap()->null_string()
            : isolate->heap()->object_string();
      }
      ASSERT(heap_obj->IsUndefined());
      return isolate->heap()->undefined_string();
    case SYMBOL_TYPE:
      return isolate->heap()->symbol_string();
    case JS_FUNCTION_TYPE:
    case JS_FUNCTION_PROXY_TYPE:
      return isolate->heap()->function_string();
    default:
      // For any kind of object not handled above, the spec rule for
      // host objects gives that it is okay to return "object"
      return isolate->heap()->object_string();
  }
}


static bool AreDigits(const uint8_t*s, int from, int to) {
  for (int i = from; i < to; i++) {
    if (s[i] < '0' || s[i] > '9') return false;
  }

  return true;
}


static int ParseDecimalInteger(const uint8_t*s, int from, int to) {
  ASSERT(to - from < 10);  // Overflow is not possible.
  ASSERT(from < to);
  int d = s[from] - '0';

  for (int i = from + 1; i < to; i++) {
    d = 10 * d + (s[i] - '0');
  }

  return d;
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_StringToNumber) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_CHECKED(String, subject, 0);
  subject->TryFlatten();

  // Fast case: short integer or some sorts of junk values.
  int len = subject->length();
  if (subject->IsSeqOneByteString()) {
    if (len == 0) return Smi::FromInt(0);

    uint8_t const* data = SeqOneByteString::cast(subject)->GetChars();
    bool minus = (data[0] == '-');
    int start_pos = (minus ? 1 : 0);

    if (start_pos == len) {
      return isolate->heap()->nan_value();
    } else if (data[start_pos] > '9') {
      // Fast check for a junk value. A valid string may start from a
      // whitespace, a sign ('+' or '-'), the decimal point, a decimal digit or
      // the 'I' character ('Infinity'). All of that have codes not greater than
      // '9' except 'I' and &nbsp;.
      if (data[start_pos] != 'I' && data[start_pos] != 0xa0) {
        return isolate->heap()->nan_value();
      }
    } else if (len - start_pos < 10 && AreDigits(data, start_pos, len)) {
      // The maximal/minimal smi has 10 digits. If the string has less digits we
      // know it will fit into the smi-data type.
      int d = ParseDecimalInteger(data, start_pos, len);
      if (minus) {
        if (d == 0) return isolate->heap()->minus_zero_value();
        d = -d;
      } else if (!subject->HasHashCode() &&
                 len <= String::kMaxArrayIndexSize &&
                 (len == 1 || data[0] != '0')) {
        // String hash is not calculated yet but all the data are present.
        // Update the hash field to speed up sequential convertions.
        uint32_t hash = StringHasher::MakeArrayIndexHash(d, len);
#ifdef DEBUG
        subject->Hash();  // Force hash calculation.
        ASSERT_EQ(static_cast<int>(subject->hash_field()),
                  static_cast<int>(hash));
#endif
        subject->set_hash_field(hash);
      }
      return Smi::FromInt(d);
    }
  }

  // Slower case.
  return isolate->heap()->NumberFromDouble(
      StringToDouble(isolate->unicode_cache(), subject, ALLOW_HEX));
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_NewString) {
  SealHandleScope shs(isolate);
  CONVERT_SMI_ARG_CHECKED(length, 0);
  CONVERT_BOOLEAN_ARG_CHECKED(is_one_byte, 1);
  if (length == 0) return isolate->heap()->empty_string();
  if (is_one_byte) {
    return isolate->heap()->AllocateRawOneByteString(length);
  } else {
    return isolate->heap()->AllocateRawTwoByteString(length);
  }
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_TruncateString) {
  HandleScope scope(isolate);
  CONVERT_ARG_HANDLE_CHECKED(SeqString, string, 0);
  CONVERT_SMI_ARG_CHECKED(new_length, 1);
  return *SeqString::Truncate(string, new_length);
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_URIEscape) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(String, source, 0);
  Handle<String> string = FlattenGetString(source);
  ASSERT(string->IsFlat());
  Handle<String> result = string->IsOneByteRepresentationUnderneath()
      ? URIEscape::Escape<uint8_t>(isolate, source)
      : URIEscape::Escape<uc16>(isolate, source);
  if (result.is_null()) return Failure::OutOfMemoryException(0x12);
  return *result;
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_URIUnescape) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(String, source, 0);
  Handle<String> string = FlattenGetString(source);
  ASSERT(string->IsFlat());
  return string->IsOneByteRepresentationUnderneath()
      ? *URIUnescape::Unescape<uint8_t>(isolate, source)
      : *URIUnescape::Unescape<uc16>(isolate, source);
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_QuoteJSONString) {
  HandleScope scope(isolate);
  CONVERT_ARG_HANDLE_CHECKED(String, string, 0);
  ASSERT(args.length() == 1);
  return BasicJsonStringifier::StringifyString(isolate, string);
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_BasicJSONStringify) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  BasicJsonStringifier stringifier(isolate);
  return stringifier.Stringify(Handle<Object>(args[0], isolate));
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_StringParseInt) {
  SealHandleScope shs(isolate);

  CONVERT_ARG_CHECKED(String, s, 0);
  CONVERT_SMI_ARG_CHECKED(radix, 1);

  s->TryFlatten();

  RUNTIME_ASSERT(radix == 0 || (2 <= radix && radix <= 36));
  double value = StringToInt(isolate->unicode_cache(), s, radix);
  return isolate->heap()->NumberFromDouble(value);
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_StringParseFloat) {
  SealHandleScope shs(isolate);
  CONVERT_ARG_CHECKED(String, str, 0);

  // ECMA-262 section 15.1.2.3, empty string is NaN
  double value = StringToDouble(isolate->unicode_cache(),
                                str, ALLOW_TRAILING_JUNK, OS::nan_value());

  // Create a number object from the value.
  return isolate->heap()->NumberFromDouble(value);
}


template <class Converter>
MUST_USE_RESULT static MaybeObject* ConvertCaseHelper(
    Isolate* isolate,
    String* s,
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
  // NOTE: This assumes that the upper/lower case of an ASCII
  // character is also ASCII.  This is currently the case, but it
  // might break in the future if we implement more context and locale
  // dependent upper/lower conversions.
  Object* o;
  { MaybeObject* maybe_o = s->IsOneByteRepresentation()
        ? isolate->heap()->AllocateRawOneByteString(length)
        : isolate->heap()->AllocateRawTwoByteString(length);
    if (!maybe_o->ToObject(&o)) return maybe_o;
  }
  String* result = String::cast(o);
  bool has_changed_character = false;

  // Convert all characters to upper case, assuming that they will fit
  // in the buffer
  Access<ConsStringIteratorOp> op(
      isolate->runtime_state()->string_iterator());
  StringCharacterStream stream(s, op.value());
  unibrow::uchar chars[Converter::kMaxWidth];
  // We can assume that the string is not empty
  uc32 current = stream.GetNext();
  for (int i = 0; i < length;) {
    bool has_next = stream.HasMore();
    uc32 next = has_next ? stream.GetNext() : 0;
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
      while (stream.HasMore()) {
        current = stream.GetNext();
        // NOTE: we use 0 as the next character here because, while
        // the next character may affect what a character converts to,
        // it does not in any case affect the length of what it convert
        // to.
        int char_length = mapping->get(current, 0, chars);
        if (char_length == 0) char_length = 1;
        current_length += char_length;
        if (current_length > Smi::kMaxValue) {
          isolate->context()->mark_out_of_memory();
          return Failure::OutOfMemoryException(0x13);
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

static const uintptr_t kOneInEveryByte = kUintptrAllBitsSet / 0xFF;
static const uintptr_t kAsciiMask = kOneInEveryByte << 7;

// Given a word and two range boundaries returns a word with high bit
// set in every byte iff the corresponding input byte was strictly in
// the range (m, n). All the other bits in the result are cleared.
// This function is only useful when it can be inlined and the
// boundaries are statically known.
// Requires: all bytes in the input word and the boundaries must be
// ASCII (less than 0x7F).
static inline uintptr_t AsciiRangeMask(uintptr_t w, char m, char n) {
  // Use strict inequalities since in edge cases the function could be
  // further simplified.
  ASSERT(0 < m && m < n);
  // Has high bit set in every w byte less than n.
  uintptr_t tmp1 = kOneInEveryByte * (0x7F + n) - w;
  // Has high bit set in every w byte greater than m.
  uintptr_t tmp2 = w + kOneInEveryByte * (0x7F - m);
  return (tmp1 & tmp2 & (kOneInEveryByte * 0x80));
}


enum AsciiCaseConversion {
  ASCII_TO_LOWER,
  ASCII_TO_UPPER
};


template <AsciiCaseConversion dir>
struct FastAsciiConverter {
  static bool Convert(char* dst, char* src, int length, bool* changed_out) {
#ifdef DEBUG
    char* saved_dst = dst;
    char* saved_src = src;
#endif
    // We rely on the distance between upper and lower case letters
    // being a known power of 2.
    ASSERT('a' - 'A' == (1 << 5));
    // Boundaries for the range of input characters than require conversion.
    const char lo = (dir == ASCII_TO_LOWER) ? 'A' - 1 : 'a' - 1;
    const char hi = (dir == ASCII_TO_LOWER) ? 'Z' + 1 : 'z' + 1;
    bool changed = false;
    uintptr_t or_acc = 0;
    char* const limit = src + length;
#ifdef V8_HOST_CAN_READ_UNALIGNED
    // Process the prefix of the input that requires no conversion one
    // (machine) word at a time.
    while (src <= limit - sizeof(uintptr_t)) {
      uintptr_t w = *reinterpret_cast<uintptr_t*>(src);
      or_acc |= w;
      if (AsciiRangeMask(w, lo, hi) != 0) {
        changed = true;
        break;
      }
      *reinterpret_cast<uintptr_t*>(dst) = w;
      src += sizeof(uintptr_t);
      dst += sizeof(uintptr_t);
    }
    // Process the remainder of the input performing conversion when
    // required one word at a time.
    while (src <= limit - sizeof(uintptr_t)) {
      uintptr_t w = *reinterpret_cast<uintptr_t*>(src);
      or_acc |= w;
      uintptr_t m = AsciiRangeMask(w, lo, hi);
      // The mask has high (7th) bit set in every byte that needs
      // conversion and we know that the distance between cases is
      // 1 << 5.
      *reinterpret_cast<uintptr_t*>(dst) = w ^ (m >> 2);
      src += sizeof(uintptr_t);
      dst += sizeof(uintptr_t);
    }
#endif
    // Process the last few bytes of the input (or the whole input if
    // unaligned access is not supported).
    while (src < limit) {
      char c = *src;
      or_acc |= c;
      if (lo < c && c < hi) {
        c ^= (1 << 5);
        changed = true;
      }
      *dst = c;
      ++src;
      ++dst;
    }
    if ((or_acc & kAsciiMask) != 0) {
      return false;
    }
#ifdef DEBUG
    CheckConvert(saved_dst, saved_src, length, changed);
#endif
    *changed_out = changed;
    return true;
  }

#ifdef DEBUG
  static void CheckConvert(char* dst, char* src, int length, bool changed) {
    bool expected_changed = false;
    for (int i = 0; i < length; i++) {
      if (dst[i] == src[i]) continue;
      expected_changed = true;
      if (dir == ASCII_TO_LOWER) {
        ASSERT('A' <= src[i] && src[i] <= 'Z');
        ASSERT(dst[i] == src[i] + ('a' - 'A'));
      } else {
        ASSERT(dir == ASCII_TO_UPPER);
        ASSERT('a' <= src[i] && src[i] <= 'z');
        ASSERT(dst[i] == src[i] - ('a' - 'A'));
      }
    }
    ASSERT(expected_changed == changed);
  }
#endif
};


struct ToLowerTraits {
  typedef unibrow::ToLowercase UnibrowConverter;

  typedef FastAsciiConverter<ASCII_TO_LOWER> AsciiConverter;
};


struct ToUpperTraits {
  typedef unibrow::ToUppercase UnibrowConverter;

  typedef FastAsciiConverter<ASCII_TO_UPPER> AsciiConverter;
};

}  // namespace


template <typename ConvertTraits>
MUST_USE_RESULT static MaybeObject* ConvertCase(
    Arguments args,
    Isolate* isolate,
    unibrow::Mapping<typename ConvertTraits::UnibrowConverter, 128>* mapping) {
  SealHandleScope shs(isolate);
  CONVERT_ARG_CHECKED(String, s, 0);
  s = s->TryFlattenGetString();

  const int length = s->length();
  // Assume that the string is not empty; we need this assumption later
  if (length == 0) return s;

  // Simpler handling of ASCII strings.
  //
  // NOTE: This assumes that the upper/lower case of an ASCII
  // character is also ASCII.  This is currently the case, but it
  // might break in the future if we implement more context and locale
  // dependent upper/lower conversions.
  if (s->IsSeqOneByteString()) {
    Object* o;
    { MaybeObject* maybe_o = isolate->heap()->AllocateRawOneByteString(length);
      if (!maybe_o->ToObject(&o)) return maybe_o;
    }
    SeqOneByteString* result = SeqOneByteString::cast(o);
    bool has_changed_character;
    bool is_ascii = ConvertTraits::AsciiConverter::Convert(
        reinterpret_cast<char*>(result->GetChars()),
        reinterpret_cast<char*>(SeqOneByteString::cast(s)->GetChars()),
        length,
        &has_changed_character);
    // If not ASCII, we discard the result and take the 2 byte path.
    if (is_ascii) {
      return has_changed_character ? result : s;
    }
  }

  Object* answer;
  { MaybeObject* maybe_answer =
        ConvertCaseHelper(isolate, s, length, length, mapping);
    if (!maybe_answer->ToObject(&answer)) return maybe_answer;
  }
  if (answer->IsSmi()) {
    // Retry with correct length.
    { MaybeObject* maybe_answer =
          ConvertCaseHelper(isolate,
                            s, Smi::cast(answer)->value(), length, mapping);
      if (!maybe_answer->ToObject(&answer)) return maybe_answer;
    }
  }
  return answer;
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_StringToLowerCase) {
  return ConvertCase<ToLowerTraits>(
      args, isolate, isolate->runtime_state()->to_lower_mapping());
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_StringToUpperCase) {
  return ConvertCase<ToUpperTraits>(
      args, isolate, isolate->runtime_state()->to_upper_mapping());
}


static inline bool IsTrimWhiteSpace(unibrow::uchar c) {
  return unibrow::WhiteSpace::Is(c) || c == 0x200b || c == 0xfeff;
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_StringTrim) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 3);

  CONVERT_ARG_CHECKED(String, s, 0);
  CONVERT_BOOLEAN_ARG_CHECKED(trimLeft, 1);
  CONVERT_BOOLEAN_ARG_CHECKED(trimRight, 2);

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


RUNTIME_FUNCTION(MaybeObject*, Runtime_StringSplit) {
  HandleScope handle_scope(isolate);
  ASSERT(args.length() == 3);
  CONVERT_ARG_HANDLE_CHECKED(String, subject, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, pattern, 1);
  CONVERT_NUMBER_CHECKED(uint32_t, limit, Uint32, args[2]);

  int subject_length = subject->length();
  int pattern_length = pattern->length();
  RUNTIME_ASSERT(pattern_length > 0);

  if (limit == 0xffffffffu) {
    Handle<Object> cached_answer(
        RegExpResultsCache::Lookup(isolate->heap(),
                                   *subject,
                                   *pattern,
                                   RegExpResultsCache::STRING_SPLIT_SUBSTRINGS),
        isolate);
    if (*cached_answer != Smi::FromInt(0)) {
      // The cache FixedArray is a COW-array and can therefore be reused.
      Handle<JSArray> result =
          isolate->factory()->NewJSArrayWithElements(
              Handle<FixedArray>::cast(cached_answer));
      return *result;
    }
  }

  // The limit can be very large (0xffffffffu), but since the pattern
  // isn't empty, we can never create more parts than ~half the length
  // of the subject.

  if (!subject->IsFlat()) FlattenString(subject);

  static const int kMaxInitialListCapacity = 16;

  ZoneScope zone_scope(isolate->runtime_zone());

  // Find (up to limit) indices of separator and end-of-string in subject
  int initial_capacity = Min<uint32_t>(kMaxInitialListCapacity, limit);
  ZoneList<int> indices(initial_capacity, zone_scope.zone());
  if (!pattern->IsFlat()) FlattenString(pattern);

  FindStringIndicesDispatch(isolate, *subject, *pattern,
                            &indices, limit, zone_scope.zone());

  if (static_cast<uint32_t>(indices.length()) < limit) {
    indices.Add(subject_length, zone_scope.zone());
  }

  // The list indices now contains the end of each part to create.

  // Create JSArray of substrings separated by separator.
  int part_count = indices.length();

  Handle<JSArray> result = isolate->factory()->NewJSArray(part_count);
  MaybeObject* maybe_result = result->EnsureCanContainHeapObjectElements();
  if (maybe_result->IsFailure()) return maybe_result;
  result->set_length(Smi::FromInt(part_count));

  ASSERT(result->HasFastObjectElements());

  if (part_count == 1 && indices.at(0) == subject_length) {
    FixedArray::cast(result->elements())->set(0, *subject);
    return *result;
  }

  Handle<FixedArray> elements(FixedArray::cast(result->elements()));
  int part_start = 0;
  for (int i = 0; i < part_count; i++) {
    HandleScope local_loop_handle(isolate);
    int part_end = indices.at(i);
    Handle<String> substring =
        isolate->factory()->NewProperSubString(subject, part_start, part_end);
    elements->set(i, *substring);
    part_start = part_end + pattern_length;
  }

  if (limit == 0xffffffffu) {
    if (result->HasFastObjectElements()) {
      RegExpResultsCache::Enter(isolate->heap(),
                                *subject,
                                *pattern,
                                *elements,
                                RegExpResultsCache::STRING_SPLIT_SUBSTRINGS);
    }
  }

  return *result;
}


// Copies ASCII characters to the given fixed array looking up
// one-char strings in the cache. Gives up on the first char that is
// not in the cache and fills the remainder with smi zeros. Returns
// the length of the successfully copied prefix.
static int CopyCachedAsciiCharsToArray(Heap* heap,
                                       const uint8_t* chars,
                                       FixedArray* elements,
                                       int length) {
  DisallowHeapAllocation no_gc;
  FixedArray* ascii_cache = heap->single_character_string_cache();
  Object* undefined = heap->undefined_value();
  int i;
  WriteBarrierMode mode = elements->GetWriteBarrierMode(no_gc);
  for (i = 0; i < length; ++i) {
    Object* value = ascii_cache->get(chars[i]);
    if (value == undefined) break;
    elements->set(i, value, mode);
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
RUNTIME_FUNCTION(MaybeObject*, Runtime_StringToArray) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(String, s, 0);
  CONVERT_NUMBER_CHECKED(uint32_t, limit, Uint32, args[1]);

  s = FlattenGetString(s);
  const int length = static_cast<int>(Min<uint32_t>(s->length(), limit));

  Handle<FixedArray> elements;
  int position = 0;
  if (s->IsFlat() && s->IsOneByteRepresentation()) {
    // Try using cached chars where possible.
    Object* obj;
    { MaybeObject* maybe_obj =
          isolate->heap()->AllocateUninitializedFixedArray(length);
      if (!maybe_obj->ToObject(&obj)) return maybe_obj;
    }
    elements = Handle<FixedArray>(FixedArray::cast(obj), isolate);
    DisallowHeapAllocation no_gc;
    String::FlatContent content = s->GetFlatContent();
    if (content.IsAscii()) {
      Vector<const uint8_t> chars = content.ToOneByteVector();
      // Note, this will initialize all elements (not only the prefix)
      // to prevent GC from seeing partially initialized array.
      position = CopyCachedAsciiCharsToArray(isolate->heap(),
                                             chars.start(),
                                             *elements,
                                             length);
    } else {
      MemsetPointer(elements->data_start(),
                    isolate->heap()->undefined_value(),
                    length);
    }
  } else {
    elements = isolate->factory()->NewFixedArray(length);
  }
  for (int i = position; i < length; ++i) {
    Handle<Object> str =
        LookupSingleCharacterStringFromCode(isolate, s->Get(i));
    elements->set(i, *str);
  }

#ifdef DEBUG
  for (int i = 0; i < length; ++i) {
    ASSERT(String::cast(elements->get(i))->length() == 1);
  }
#endif

  return *isolate->factory()->NewJSArrayWithElements(elements);
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_NewStringWrapper) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_CHECKED(String, value, 0);
  return value->ToObject();
}


bool Runtime::IsUpperCaseChar(RuntimeState* runtime_state, uint16_t ch) {
  unibrow::uchar chars[unibrow::ToUppercase::kMaxWidth];
  int char_length = runtime_state->to_upper_mapping()->get(ch, 0, chars);
  return char_length == 0;
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_NumberToString) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);

  Object* number = args[0];
  RUNTIME_ASSERT(number->IsNumber());

  return isolate->heap()->NumberToString(number);
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_NumberToStringSkipCache) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);

  Object* number = args[0];
  RUNTIME_ASSERT(number->IsNumber());

  return isolate->heap()->NumberToString(
      number, false, isolate->heap()->GetPretenureMode());
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_NumberToInteger) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);

  CONVERT_DOUBLE_ARG_CHECKED(number, 0);

  // We do not include 0 so that we don't have to treat +0 / -0 cases.
  if (number > 0 && number <= Smi::kMaxValue) {
    return Smi::FromInt(static_cast<int>(number));
  }
  return isolate->heap()->NumberFromDouble(DoubleToInteger(number));
}


// ES6 draft 9.1.11
RUNTIME_FUNCTION(MaybeObject*, Runtime_NumberToPositiveInteger) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);

  CONVERT_DOUBLE_ARG_CHECKED(number, 0);

  // We do not include 0 so that we don't have to treat +0 / -0 cases.
  if (number > 0 && number <= Smi::kMaxValue) {
    return Smi::FromInt(static_cast<int>(number));
  }
  if (number <= 0) {
    return Smi::FromInt(0);
  }
  return isolate->heap()->NumberFromDouble(DoubleToInteger(number));
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_NumberToIntegerMapMinusZero) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);

  CONVERT_DOUBLE_ARG_CHECKED(number, 0);

  // We do not include 0 so that we don't have to treat +0 / -0 cases.
  if (number > 0 && number <= Smi::kMaxValue) {
    return Smi::FromInt(static_cast<int>(number));
  }

  double double_value = DoubleToInteger(number);
  // Map both -0 and +0 to +0.
  if (double_value == 0) double_value = 0;

  return isolate->heap()->NumberFromDouble(double_value);
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_NumberToJSUint32) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);

  CONVERT_NUMBER_CHECKED(int32_t, number, Uint32, args[0]);
  return isolate->heap()->NumberFromUint32(number);
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_NumberToJSInt32) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);

  CONVERT_DOUBLE_ARG_CHECKED(number, 0);

  // We do not include 0 so that we don't have to treat +0 / -0 cases.
  if (number > 0 && number <= Smi::kMaxValue) {
    return Smi::FromInt(static_cast<int>(number));
  }
  return isolate->heap()->NumberFromInt32(DoubleToInt32(number));
}


// Converts a Number to a Smi, if possible. Returns NaN if the number is not
// a small integer.
RUNTIME_FUNCTION(MaybeObject*, Runtime_NumberToSmi) {
  SealHandleScope shs(isolate);
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
  return isolate->heap()->nan_value();
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_AllocateHeapNumber) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 0);
  return isolate->heap()->AllocateHeapNumber(0);
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_NumberAdd) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 2);

  CONVERT_DOUBLE_ARG_CHECKED(x, 0);
  CONVERT_DOUBLE_ARG_CHECKED(y, 1);
  return isolate->heap()->NumberFromDouble(x + y);
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_NumberSub) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 2);

  CONVERT_DOUBLE_ARG_CHECKED(x, 0);
  CONVERT_DOUBLE_ARG_CHECKED(y, 1);
  return isolate->heap()->NumberFromDouble(x - y);
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_NumberMul) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 2);

  CONVERT_DOUBLE_ARG_CHECKED(x, 0);
  CONVERT_DOUBLE_ARG_CHECKED(y, 1);
  return isolate->heap()->NumberFromDouble(x * y);
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_NumberUnaryMinus) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);

  CONVERT_DOUBLE_ARG_CHECKED(x, 0);
  return isolate->heap()->NumberFromDouble(-x);
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_NumberAlloc) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 0);

  return isolate->heap()->NumberFromDouble(9876543210.0);
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_NumberDiv) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 2);

  CONVERT_DOUBLE_ARG_CHECKED(x, 0);
  CONVERT_DOUBLE_ARG_CHECKED(y, 1);
  return isolate->heap()->NumberFromDouble(x / y);
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_NumberMod) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 2);

  CONVERT_DOUBLE_ARG_CHECKED(x, 0);
  CONVERT_DOUBLE_ARG_CHECKED(y, 1);

  x = modulo(x, y);
  // NumberFromDouble may return a Smi instead of a Number object
  return isolate->heap()->NumberFromDouble(x);
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_NumberImul) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 2);

  CONVERT_NUMBER_CHECKED(int32_t, x, Int32, args[0]);
  CONVERT_NUMBER_CHECKED(int32_t, y, Int32, args[1]);
  return isolate->heap()->NumberFromInt32(x * y);
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_StringAdd) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_CHECKED(String, str1, 0);
  CONVERT_ARG_CHECKED(String, str2, 1);
  isolate->counters()->string_add_runtime()->Increment();
  return isolate->heap()->AllocateConsString(str1, str2);
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


RUNTIME_FUNCTION(MaybeObject*, Runtime_StringBuilderConcat) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 3);
  CONVERT_ARG_CHECKED(JSArray, array, 0);
  if (!args[1]->IsSmi()) {
    isolate->context()->mark_out_of_memory();
    return Failure::OutOfMemoryException(0x14);
  }
  int array_length = args.smi_at(1);
  CONVERT_ARG_CHECKED(String, special, 2);

  // This assumption is used by the slice encoding in one or two smis.
  ASSERT(Smi::kMaxValue >= String::kMaxLength);

  MaybeObject* maybe_result = array->EnsureCanContainHeapObjectElements();
  if (maybe_result->IsFailure()) return maybe_result;

  int special_length = special->length();
  if (!array->HasFastObjectElements()) {
    return isolate->Throw(isolate->heap()->illegal_argument_string());
  }
  FixedArray* fixed_array = FixedArray::cast(array->elements());
  if (fixed_array->length() < array_length) {
    array_length = fixed_array->length();
  }

  if (array_length == 0) {
    return isolate->heap()->empty_string();
  } else if (array_length == 1) {
    Object* first = fixed_array->get(0);
    if (first->IsString()) return first;
  }

  bool one_byte = special->HasOnlyOneByteChars();
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
          return isolate->Throw(isolate->heap()->illegal_argument_string());
        }
        Object* next_smi = fixed_array->get(i);
        if (!next_smi->IsSmi()) {
          return isolate->Throw(isolate->heap()->illegal_argument_string());
        }
        pos = Smi::cast(next_smi)->value();
        if (pos < 0) {
          return isolate->Throw(isolate->heap()->illegal_argument_string());
        }
      }
      ASSERT(pos >= 0);
      ASSERT(len >= 0);
      if (pos > special_length || len > special_length - pos) {
        return isolate->Throw(isolate->heap()->illegal_argument_string());
      }
      increment = len;
    } else if (elt->IsString()) {
      String* element = String::cast(elt);
      int element_length = element->length();
      increment = element_length;
      if (one_byte && !element->HasOnlyOneByteChars()) {
        one_byte = false;
      }
    } else {
      ASSERT(!elt->IsTheHole());
      return isolate->Throw(isolate->heap()->illegal_argument_string());
    }
    if (increment > String::kMaxLength - position) {
      isolate->context()->mark_out_of_memory();
      return Failure::OutOfMemoryException(0x15);
    }
    position += increment;
  }

  int length = position;
  Object* object;

  if (one_byte) {
    { MaybeObject* maybe_object =
          isolate->heap()->AllocateRawOneByteString(length);
      if (!maybe_object->ToObject(&object)) return maybe_object;
    }
    SeqOneByteString* answer = SeqOneByteString::cast(object);
    StringBuilderConcatHelper(special,
                              answer->GetChars(),
                              fixed_array,
                              array_length);
    return answer;
  } else {
    { MaybeObject* maybe_object =
          isolate->heap()->AllocateRawTwoByteString(length);
      if (!maybe_object->ToObject(&object)) return maybe_object;
    }
    SeqTwoByteString* answer = SeqTwoByteString::cast(object);
    StringBuilderConcatHelper(special,
                              answer->GetChars(),
                              fixed_array,
                              array_length);
    return answer;
  }
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_StringBuilderJoin) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 3);
  CONVERT_ARG_CHECKED(JSArray, array, 0);
  if (!args[1]->IsSmi()) {
    isolate->context()->mark_out_of_memory();
    return Failure::OutOfMemoryException(0x16);
  }
  int array_length = args.smi_at(1);
  CONVERT_ARG_CHECKED(String, separator, 2);

  if (!array->HasFastObjectElements()) {
    return isolate->Throw(isolate->heap()->illegal_argument_string());
  }
  FixedArray* fixed_array = FixedArray::cast(array->elements());
  if (fixed_array->length() < array_length) {
    array_length = fixed_array->length();
  }

  if (array_length == 0) {
    return isolate->heap()->empty_string();
  } else if (array_length == 1) {
    Object* first = fixed_array->get(0);
    if (first->IsString()) return first;
  }

  int separator_length = separator->length();
  int max_nof_separators =
      (String::kMaxLength + separator_length - 1) / separator_length;
  if (max_nof_separators < (array_length - 1)) {
      isolate->context()->mark_out_of_memory();
      return Failure::OutOfMemoryException(0x17);
  }
  int length = (array_length - 1) * separator_length;
  for (int i = 0; i < array_length; i++) {
    Object* element_obj = fixed_array->get(i);
    if (!element_obj->IsString()) {
      // TODO(1161): handle this case.
      return isolate->Throw(isolate->heap()->illegal_argument_string());
    }
    String* element = String::cast(element_obj);
    int increment = element->length();
    if (increment > String::kMaxLength - length) {
      isolate->context()->mark_out_of_memory();
      return Failure::OutOfMemoryException(0x18);
    }
    length += increment;
  }

  Object* object;
  { MaybeObject* maybe_object =
        isolate->heap()->AllocateRawTwoByteString(length);
    if (!maybe_object->ToObject(&object)) return maybe_object;
  }
  SeqTwoByteString* answer = SeqTwoByteString::cast(object);

  uc16* sink = answer->GetChars();
#ifdef DEBUG
  uc16* end = sink + length;
#endif

  String* first = String::cast(fixed_array->get(0));
  int first_length = first->length();
  String::WriteToFlat(first, sink, 0, first_length);
  sink += first_length;

  for (int i = 1; i < array_length; i++) {
    ASSERT(sink + separator_length <= end);
    String::WriteToFlat(separator, sink, 0, separator_length);
    sink += separator_length;

    String* element = String::cast(fixed_array->get(i));
    int element_length = element->length();
    ASSERT(sink + element_length <= end);
    String::WriteToFlat(element, sink, 0, element_length);
    sink += element_length;
  }
  ASSERT(sink == end);

  // Use %_FastAsciiArrayJoin instead.
  ASSERT(!answer->IsOneByteRepresentation());
  return answer;
}

template <typename Char>
static void JoinSparseArrayWithSeparator(FixedArray* elements,
                                         int elements_length,
                                         uint32_t array_length,
                                         String* separator,
                                         Vector<Char> buffer) {
  int previous_separator_position = 0;
  int separator_length = separator->length();
  int cursor = 0;
  for (int i = 0; i < elements_length; i += 2) {
    int position = NumberToInt32(elements->get(i));
    String* string = String::cast(elements->get(i + 1));
    int string_length = string->length();
    if (string->length() > 0) {
      while (previous_separator_position < position) {
        String::WriteToFlat<Char>(separator, &buffer[cursor],
                                  0, separator_length);
        cursor += separator_length;
        previous_separator_position++;
      }
      String::WriteToFlat<Char>(string, &buffer[cursor],
                                0, string_length);
      cursor += string->length();
    }
  }
  if (separator_length > 0) {
    // Array length must be representable as a signed 32-bit number,
    // otherwise the total string length would have been too large.
    ASSERT(array_length <= 0x7fffffff);  // Is int32_t.
    int last_array_index = static_cast<int>(array_length - 1);
    while (previous_separator_position < last_array_index) {
      String::WriteToFlat<Char>(separator, &buffer[cursor],
                                0, separator_length);
      cursor += separator_length;
      previous_separator_position++;
    }
  }
  ASSERT(cursor <= buffer.length());
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_SparseJoinWithSeparator) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 3);
  CONVERT_ARG_CHECKED(JSArray, elements_array, 0);
  RUNTIME_ASSERT(elements_array->HasFastSmiOrObjectElements());
  CONVERT_NUMBER_CHECKED(uint32_t, array_length, Uint32, args[1]);
  CONVERT_ARG_CHECKED(String, separator, 2);
  // elements_array is fast-mode JSarray of alternating positions
  // (increasing order) and strings.
  // array_length is length of original array (used to add separators);
  // separator is string to put between elements. Assumed to be non-empty.

  // Find total length of join result.
  int string_length = 0;
  bool is_ascii = separator->IsOneByteRepresentation();
  int max_string_length;
  if (is_ascii) {
    max_string_length = SeqOneByteString::kMaxLength;
  } else {
    max_string_length = SeqTwoByteString::kMaxLength;
  }
  bool overflow = false;
  CONVERT_NUMBER_CHECKED(int, elements_length,
                         Int32, elements_array->length());
  RUNTIME_ASSERT((elements_length & 1) == 0);  // Even length.
  FixedArray* elements = FixedArray::cast(elements_array->elements());
  for (int i = 0; i < elements_length; i += 2) {
    RUNTIME_ASSERT(elements->get(i)->IsNumber());
    RUNTIME_ASSERT(elements->get(i + 1)->IsString());
    String* string = String::cast(elements->get(i + 1));
    int length = string->length();
    if (is_ascii && !string->IsOneByteRepresentation()) {
      is_ascii = false;
      max_string_length = SeqTwoByteString::kMaxLength;
    }
    if (length > max_string_length ||
        max_string_length - length < string_length) {
      overflow = true;
      break;
    }
    string_length += length;
  }
  int separator_length = separator->length();
  if (!overflow && separator_length > 0) {
    if (array_length <= 0x7fffffffu) {
      int separator_count = static_cast<int>(array_length) - 1;
      int remaining_length = max_string_length - string_length;
      if ((remaining_length / separator_length) >= separator_count) {
        string_length += separator_length * (array_length - 1);
      } else {
        // Not room for the separators within the maximal string length.
        overflow = true;
      }
    } else {
      // Nonempty separator and at least 2^31-1 separators necessary
      // means that the string is too large to create.
      STATIC_ASSERT(String::kMaxLength < 0x7fffffff);
      overflow = true;
    }
  }
  if (overflow) {
    // Throw OutOfMemory exception for creating too large a string.
    V8::FatalProcessOutOfMemory("Array join result too large.");
  }

  if (is_ascii) {
    MaybeObject* result_allocation =
        isolate->heap()->AllocateRawOneByteString(string_length);
    if (result_allocation->IsFailure()) return result_allocation;
    SeqOneByteString* result_string =
        SeqOneByteString::cast(result_allocation->ToObjectUnchecked());
    JoinSparseArrayWithSeparator<uint8_t>(elements,
                                          elements_length,
                                          array_length,
                                          separator,
                                          Vector<uint8_t>(
                                              result_string->GetChars(),
                                              string_length));
    return result_string;
  } else {
    MaybeObject* result_allocation =
        isolate->heap()->AllocateRawTwoByteString(string_length);
    if (result_allocation->IsFailure()) return result_allocation;
    SeqTwoByteString* result_string =
        SeqTwoByteString::cast(result_allocation->ToObjectUnchecked());
    JoinSparseArrayWithSeparator<uc16>(elements,
                                       elements_length,
                                       array_length,
                                       separator,
                                       Vector<uc16>(result_string->GetChars(),
                                                    string_length));
    return result_string;
  }
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_NumberOr) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 2);

  CONVERT_NUMBER_CHECKED(int32_t, x, Int32, args[0]);
  CONVERT_NUMBER_CHECKED(int32_t, y, Int32, args[1]);
  return isolate->heap()->NumberFromInt32(x | y);
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_NumberAnd) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 2);

  CONVERT_NUMBER_CHECKED(int32_t, x, Int32, args[0]);
  CONVERT_NUMBER_CHECKED(int32_t, y, Int32, args[1]);
  return isolate->heap()->NumberFromInt32(x & y);
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_NumberXor) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 2);

  CONVERT_NUMBER_CHECKED(int32_t, x, Int32, args[0]);
  CONVERT_NUMBER_CHECKED(int32_t, y, Int32, args[1]);
  return isolate->heap()->NumberFromInt32(x ^ y);
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_NumberNot) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);

  CONVERT_NUMBER_CHECKED(int32_t, x, Int32, args[0]);
  return isolate->heap()->NumberFromInt32(~x);
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_NumberShl) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 2);

  CONVERT_NUMBER_CHECKED(int32_t, x, Int32, args[0]);
  CONVERT_NUMBER_CHECKED(int32_t, y, Int32, args[1]);
  return isolate->heap()->NumberFromInt32(x << (y & 0x1f));
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_NumberShr) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 2);

  CONVERT_NUMBER_CHECKED(uint32_t, x, Uint32, args[0]);
  CONVERT_NUMBER_CHECKED(int32_t, y, Int32, args[1]);
  return isolate->heap()->NumberFromUint32(x >> (y & 0x1f));
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_NumberSar) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 2);

  CONVERT_NUMBER_CHECKED(int32_t, x, Int32, args[0]);
  CONVERT_NUMBER_CHECKED(int32_t, y, Int32, args[1]);
  return isolate->heap()->NumberFromInt32(ArithmeticShiftRight(x, y & 0x1f));
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_NumberEquals) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 2);

  CONVERT_DOUBLE_ARG_CHECKED(x, 0);
  CONVERT_DOUBLE_ARG_CHECKED(y, 1);
  if (std::isnan(x)) return Smi::FromInt(NOT_EQUAL);
  if (std::isnan(y)) return Smi::FromInt(NOT_EQUAL);
  if (x == y) return Smi::FromInt(EQUAL);
  Object* result;
  if ((fpclassify(x) == FP_ZERO) && (fpclassify(y) == FP_ZERO)) {
    result = Smi::FromInt(EQUAL);
  } else {
    result = Smi::FromInt(NOT_EQUAL);
  }
  return result;
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_StringEquals) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 2);

  CONVERT_ARG_CHECKED(String, x, 0);
  CONVERT_ARG_CHECKED(String, y, 1);

  bool not_equal = !x->Equals(y);
  // This is slightly convoluted because the value that signifies
  // equality is 0 and inequality is 1 so we have to negate the result
  // from String::Equals.
  ASSERT(not_equal == 0 || not_equal == 1);
  STATIC_CHECK(EQUAL == 0);
  STATIC_CHECK(NOT_EQUAL == 1);
  return Smi::FromInt(not_equal);
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_NumberCompare) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 3);

  CONVERT_DOUBLE_ARG_CHECKED(x, 0);
  CONVERT_DOUBLE_ARG_CHECKED(y, 1);
  if (std::isnan(x) || std::isnan(y)) return args[2];
  if (x == y) return Smi::FromInt(EQUAL);
  if (isless(x, y)) return Smi::FromInt(LESS);
  return Smi::FromInt(GREATER);
}


// Compare two Smis as if they were converted to strings and then
// compared lexicographically.
RUNTIME_FUNCTION(MaybeObject*, Runtime_SmiLexicographicCompare) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 2);
  CONVERT_SMI_ARG_CHECKED(x_value, 0);
  CONVERT_SMI_ARG_CHECKED(y_value, 1);

  // If the integers are equal so are the string representations.
  if (x_value == y_value) return Smi::FromInt(EQUAL);

  // If one of the integers is zero the normal integer order is the
  // same as the lexicographic order of the string representations.
  if (x_value == 0 || y_value == 0)
    return Smi::FromInt(x_value < y_value ? LESS : GREATER);

  // If only one of the integers is negative the negative number is
  // smallest because the char code of '-' is less than the char code
  // of any digit.  Otherwise, we make both values positive.

  // Use unsigned values otherwise the logic is incorrect for -MIN_INT on
  // architectures using 32-bit Smis.
  uint32_t x_scaled = x_value;
  uint32_t y_scaled = y_value;
  if (x_value < 0 || y_value < 0) {
    if (y_value >= 0) return Smi::FromInt(LESS);
    if (x_value >= 0) return Smi::FromInt(GREATER);
    x_scaled = -x_value;
    y_scaled = -y_value;
  }

  static const uint32_t kPowersOf10[] = {
    1, 10, 100, 1000, 10*1000, 100*1000,
    1000*1000, 10*1000*1000, 100*1000*1000,
    1000*1000*1000
  };

  // If the integers have the same number of decimal digits they can be
  // compared directly as the numeric order is the same as the
  // lexicographic order.  If one integer has fewer digits, it is scaled
  // by some power of 10 to have the same number of digits as the longer
  // integer.  If the scaled integers are equal it means the shorter
  // integer comes first in the lexicographic order.

  // From http://graphics.stanford.edu/~seander/bithacks.html#IntegerLog10
  int x_log2 = IntegerLog2(x_scaled);
  int x_log10 = ((x_log2 + 1) * 1233) >> 12;
  x_log10 -= x_scaled < kPowersOf10[x_log10];

  int y_log2 = IntegerLog2(y_scaled);
  int y_log10 = ((y_log2 + 1) * 1233) >> 12;
  y_log10 -= y_scaled < kPowersOf10[y_log10];

  int tie = EQUAL;

  if (x_log10 < y_log10) {
    // X has fewer digits.  We would like to simply scale up X but that
    // might overflow, e.g when comparing 9 with 1_000_000_000, 9 would
    // be scaled up to 9_000_000_000. So we scale up by the next
    // smallest power and scale down Y to drop one digit. It is OK to
    // drop one digit from the longer integer since the final digit is
    // past the length of the shorter integer.
    x_scaled *= kPowersOf10[y_log10 - x_log10 - 1];
    y_scaled /= 10;
    tie = LESS;
  } else if (y_log10 < x_log10) {
    y_scaled *= kPowersOf10[x_log10 - y_log10 - 1];
    x_scaled /= 10;
    tie = GREATER;
  }

  if (x_scaled < y_scaled) return Smi::FromInt(LESS);
  if (x_scaled > y_scaled) return Smi::FromInt(GREATER);
  return Smi::FromInt(tie);
}


static Object* StringCharacterStreamCompare(RuntimeState* state,
                                        String* x,
                                        String* y) {
  StringCharacterStream stream_x(x, state->string_iterator_compare_x());
  StringCharacterStream stream_y(y, state->string_iterator_compare_y());
  while (stream_x.HasMore() && stream_y.HasMore()) {
    int d = stream_x.GetNext() - stream_y.GetNext();
    if (d < 0) return Smi::FromInt(LESS);
    else if (d > 0) return Smi::FromInt(GREATER);
  }

  // x is (non-trivial) prefix of y:
  if (stream_y.HasMore()) return Smi::FromInt(LESS);
  // y is prefix of x:
  return Smi::FromInt(stream_x.HasMore() ? GREATER : EQUAL);
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
  DisallowHeapAllocation no_gc;
  String::FlatContent x_content = x->GetFlatContent();
  String::FlatContent y_content = y->GetFlatContent();
  if (x_content.IsAscii()) {
    Vector<const uint8_t> x_chars = x_content.ToOneByteVector();
    if (y_content.IsAscii()) {
      Vector<const uint8_t> y_chars = y_content.ToOneByteVector();
      r = CompareChars(x_chars.start(), y_chars.start(), prefix_length);
    } else {
      Vector<const uc16> y_chars = y_content.ToUC16Vector();
      r = CompareChars(x_chars.start(), y_chars.start(), prefix_length);
    }
  } else {
    Vector<const uc16> x_chars = x_content.ToUC16Vector();
    if (y_content.IsAscii()) {
      Vector<const uint8_t> y_chars = y_content.ToOneByteVector();
      r = CompareChars(x_chars.start(), y_chars.start(), prefix_length);
    } else {
      Vector<const uc16> y_chars = y_content.ToUC16Vector();
      r = CompareChars(x_chars.start(), y_chars.start(), prefix_length);
    }
  }
  Object* result;
  if (r == 0) {
    result = equal_prefix_result;
  } else {
    result = (r < 0) ? Smi::FromInt(LESS) : Smi::FromInt(GREATER);
  }
  ASSERT(result ==
      StringCharacterStreamCompare(Isolate::Current()->runtime_state(), x, y));
  return result;
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_StringCompare) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 2);

  CONVERT_ARG_CHECKED(String, x, 0);
  CONVERT_ARG_CHECKED(String, y, 1);

  isolate->counters()->string_compare_runtime()->Increment();

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

  Object* obj;
  { MaybeObject* maybe_obj = isolate->heap()->PrepareForCompare(x);
    if (!maybe_obj->ToObject(&obj)) return maybe_obj;
  }
  { MaybeObject* maybe_obj = isolate->heap()->PrepareForCompare(y);
    if (!maybe_obj->ToObject(&obj)) return maybe_obj;
  }

  return (x->IsFlat() && y->IsFlat()) ? FlatStringCompare(x, y)
      : StringCharacterStreamCompare(isolate->runtime_state(), x, y);
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_Math_acos) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  isolate->counters()->math_acos()->Increment();

  CONVERT_DOUBLE_ARG_CHECKED(x, 0);
  return isolate->transcendental_cache()->Get(TranscendentalCache::ACOS, x);
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_Math_asin) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  isolate->counters()->math_asin()->Increment();

  CONVERT_DOUBLE_ARG_CHECKED(x, 0);
  return isolate->transcendental_cache()->Get(TranscendentalCache::ASIN, x);
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_Math_atan) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  isolate->counters()->math_atan()->Increment();

  CONVERT_DOUBLE_ARG_CHECKED(x, 0);
  return isolate->transcendental_cache()->Get(TranscendentalCache::ATAN, x);
}


static const double kPiDividedBy4 = 0.78539816339744830962;


RUNTIME_FUNCTION(MaybeObject*, Runtime_Math_atan2) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 2);
  isolate->counters()->math_atan2()->Increment();

  CONVERT_DOUBLE_ARG_CHECKED(x, 0);
  CONVERT_DOUBLE_ARG_CHECKED(y, 1);
  double result;
  if (std::isinf(x) && std::isinf(y)) {
    // Make sure that the result in case of two infinite arguments
    // is a multiple of Pi / 4. The sign of the result is determined
    // by the first argument (x) and the sign of the second argument
    // determines the multiplier: one or three.
    int multiplier = (x < 0) ? -1 : 1;
    if (y < 0) multiplier *= 3;
    result = multiplier * kPiDividedBy4;
  } else {
    result = atan2(x, y);
  }
  return isolate->heap()->AllocateHeapNumber(result);
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_Math_ceil) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  isolate->counters()->math_ceil()->Increment();

  CONVERT_DOUBLE_ARG_CHECKED(x, 0);
  return isolate->heap()->NumberFromDouble(ceiling(x));
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_Math_cos) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  isolate->counters()->math_cos()->Increment();

  CONVERT_DOUBLE_ARG_CHECKED(x, 0);
  return isolate->transcendental_cache()->Get(TranscendentalCache::COS, x);
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_Math_exp) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  isolate->counters()->math_exp()->Increment();

  CONVERT_DOUBLE_ARG_CHECKED(x, 0);
  lazily_initialize_fast_exp();
  return isolate->heap()->NumberFromDouble(fast_exp(x));
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_Math_floor) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  isolate->counters()->math_floor()->Increment();

  CONVERT_DOUBLE_ARG_CHECKED(x, 0);
  return isolate->heap()->NumberFromDouble(floor(x));
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_Math_log) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  isolate->counters()->math_log()->Increment();

  CONVERT_DOUBLE_ARG_CHECKED(x, 0);
  return isolate->transcendental_cache()->Get(TranscendentalCache::LOG, x);
}

// Slow version of Math.pow.  We check for fast paths for special cases.
// Used if SSE2/VFP3 is not available.
RUNTIME_FUNCTION(MaybeObject*, Runtime_Math_pow) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 2);
  isolate->counters()->math_pow()->Increment();

  CONVERT_DOUBLE_ARG_CHECKED(x, 0);

  // If the second argument is a smi, it is much faster to call the
  // custom powi() function than the generic pow().
  if (args[1]->IsSmi()) {
    int y = args.smi_at(1);
    return isolate->heap()->NumberFromDouble(power_double_int(x, y));
  }

  CONVERT_DOUBLE_ARG_CHECKED(y, 1);
  double result = power_helper(x, y);
  if (std::isnan(result)) return isolate->heap()->nan_value();
  return isolate->heap()->AllocateHeapNumber(result);
}

// Fast version of Math.pow if we know that y is not an integer and y is not
// -0.5 or 0.5.  Used as slow case from full codegen.
RUNTIME_FUNCTION(MaybeObject*, Runtime_Math_pow_cfunction) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 2);
  isolate->counters()->math_pow()->Increment();

  CONVERT_DOUBLE_ARG_CHECKED(x, 0);
  CONVERT_DOUBLE_ARG_CHECKED(y, 1);
  if (y == 0) {
    return Smi::FromInt(1);
  } else {
    double result = power_double_double(x, y);
    if (std::isnan(result)) return isolate->heap()->nan_value();
    return isolate->heap()->AllocateHeapNumber(result);
  }
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_RoundNumber) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  isolate->counters()->math_round()->Increment();

  if (!args[0]->IsHeapNumber()) {
    // Must be smi. Return the argument unchanged for all the other types
    // to make fuzz-natives test happy.
    return args[0];
  }

  HeapNumber* number = reinterpret_cast<HeapNumber*>(args[0]);

  double value = number->value();
  int exponent = number->get_exponent();
  int sign = number->get_sign();

  if (exponent < -1) {
    // Number in range ]-0.5..0.5[. These always round to +/-zero.
    if (sign) return isolate->heap()->minus_zero_value();
    return Smi::FromInt(0);
  }

  // We compare with kSmiValueSize - 2 because (2^30 - 0.1) has exponent 29 and
  // should be rounded to 2^30, which is not smi (for 31-bit smis, similar
  // argument holds for 32-bit smis).
  if (!sign && exponent < kSmiValueSize - 2) {
    return Smi::FromInt(static_cast<int>(value + 0.5));
  }

  // If the magnitude is big enough, there's no place for fraction part. If we
  // try to add 0.5 to this number, 1.0 will be added instead.
  if (exponent >= 52) {
    return number;
  }

  if (sign && value >= -0.5) return isolate->heap()->minus_zero_value();

  // Do not call NumberFromDouble() to avoid extra checks.
  return isolate->heap()->AllocateHeapNumber(floor(value + 0.5));
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_Math_sin) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  isolate->counters()->math_sin()->Increment();

  CONVERT_DOUBLE_ARG_CHECKED(x, 0);
  return isolate->transcendental_cache()->Get(TranscendentalCache::SIN, x);
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_Math_sqrt) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  isolate->counters()->math_sqrt()->Increment();

  CONVERT_DOUBLE_ARG_CHECKED(x, 0);
  return isolate->heap()->AllocateHeapNumber(fast_sqrt(x));
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_Math_tan) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  isolate->counters()->math_tan()->Increment();

  CONVERT_DOUBLE_ARG_CHECKED(x, 0);
  return isolate->transcendental_cache()->Get(TranscendentalCache::TAN, x);
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_DateMakeDay) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 2);

  CONVERT_SMI_ARG_CHECKED(year, 0);
  CONVERT_SMI_ARG_CHECKED(month, 1);

  return Smi::FromInt(isolate->date_cache()->DaysFromYearMonth(year, month));
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_DateSetValue) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 3);

  CONVERT_ARG_HANDLE_CHECKED(JSDate, date, 0);
  CONVERT_DOUBLE_ARG_CHECKED(time, 1);
  CONVERT_SMI_ARG_CHECKED(is_utc, 2);

  DateCache* date_cache = isolate->date_cache();

  Object* value = NULL;
  bool is_value_nan = false;
  if (std::isnan(time)) {
    value = isolate->heap()->nan_value();
    is_value_nan = true;
  } else if (!is_utc &&
             (time < -DateCache::kMaxTimeBeforeUTCInMs ||
              time > DateCache::kMaxTimeBeforeUTCInMs)) {
    value = isolate->heap()->nan_value();
    is_value_nan = true;
  } else {
    time = is_utc ? time : date_cache->ToUTC(static_cast<int64_t>(time));
    if (time < -DateCache::kMaxTimeInMs ||
        time > DateCache::kMaxTimeInMs) {
      value = isolate->heap()->nan_value();
      is_value_nan = true;
    } else  {
      MaybeObject* maybe_result =
          isolate->heap()->AllocateHeapNumber(DoubleToInteger(time));
      if (!maybe_result->ToObject(&value)) return maybe_result;
    }
  }
  date->SetValue(value, is_value_nan);
  return value;
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_NewArgumentsFast) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 3);

  Handle<JSFunction> callee = args.at<JSFunction>(0);
  Object** parameters = reinterpret_cast<Object**>(args[1]);
  const int argument_count = Smi::cast(args[2])->value();

  Handle<JSObject> result =
      isolate->factory()->NewArgumentsObject(callee, argument_count);
  // Allocate the elements if needed.
  int parameter_count = callee->shared()->formal_parameter_count();
  if (argument_count > 0) {
    if (parameter_count > 0) {
      int mapped_count = Min(argument_count, parameter_count);
      Handle<FixedArray> parameter_map =
          isolate->factory()->NewFixedArray(mapped_count + 2, NOT_TENURED);
      parameter_map->set_map(
          isolate->heap()->non_strict_arguments_elements_map());

      Handle<Map> old_map(result->map());
      Handle<Map> new_map = isolate->factory()->CopyMap(old_map);
      new_map->set_elements_kind(NON_STRICT_ARGUMENTS_ELEMENTS);

      result->set_map(*new_map);
      result->set_elements(*parameter_map);

      // Store the context and the arguments array at the beginning of the
      // parameter map.
      Handle<Context> context(isolate->context());
      Handle<FixedArray> arguments =
          isolate->factory()->NewFixedArray(argument_count, NOT_TENURED);
      parameter_map->set(0, *context);
      parameter_map->set(1, *arguments);

      // Loop over the actual parameters backwards.
      int index = argument_count - 1;
      while (index >= mapped_count) {
        // These go directly in the arguments array and have no
        // corresponding slot in the parameter map.
        arguments->set(index, *(parameters - index - 1));
        --index;
      }

      Handle<ScopeInfo> scope_info(callee->shared()->scope_info());
      while (index >= 0) {
        // Detect duplicate names to the right in the parameter list.
        Handle<String> name(scope_info->ParameterName(index));
        int context_local_count = scope_info->ContextLocalCount();
        bool duplicate = false;
        for (int j = index + 1; j < parameter_count; ++j) {
          if (scope_info->ParameterName(j) == *name) {
            duplicate = true;
            break;
          }
        }

        if (duplicate) {
          // This goes directly in the arguments array with a hole in the
          // parameter map.
          arguments->set(index, *(parameters - index - 1));
          parameter_map->set_the_hole(index + 2);
        } else {
          // The context index goes in the parameter map with a hole in the
          // arguments array.
          int context_index = -1;
          for (int j = 0; j < context_local_count; ++j) {
            if (scope_info->ContextLocalName(j) == *name) {
              context_index = j;
              break;
            }
          }
          ASSERT(context_index >= 0);
          arguments->set_the_hole(index);
          parameter_map->set(index + 2, Smi::FromInt(
              Context::MIN_CONTEXT_SLOTS + context_index));
        }

        --index;
      }
    } else {
      // If there is no aliasing, the arguments object elements are not
      // special in any way.
      Handle<FixedArray> elements =
          isolate->factory()->NewFixedArray(argument_count, NOT_TENURED);
      result->set_elements(*elements);
      for (int i = 0; i < argument_count; ++i) {
        elements->set(i, *(parameters - i - 1));
      }
    }
  }
  return *result;
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_NewStrictArgumentsFast) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 3);

  JSFunction* callee = JSFunction::cast(args[0]);
  Object** parameters = reinterpret_cast<Object**>(args[1]);
  const int length = args.smi_at(2);

  Object* result;
  { MaybeObject* maybe_result =
        isolate->heap()->AllocateArgumentsObject(callee, length);
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }
  // Allocate the elements if needed.
  if (length > 0) {
    // Allocate the fixed array.
    Object* obj;
    { MaybeObject* maybe_obj = isolate->heap()->AllocateRawFixedArray(length);
      if (!maybe_obj->ToObject(&obj)) return maybe_obj;
    }

    DisallowHeapAllocation no_gc;
    FixedArray* array = reinterpret_cast<FixedArray*>(obj);
    array->set_map_no_write_barrier(isolate->heap()->fixed_array_map());
    array->set_length(length);

    WriteBarrierMode mode = array->GetWriteBarrierMode(no_gc);
    for (int i = 0; i < length; i++) {
      array->set(i, *--parameters, mode);
    }
    JSObject::cast(result)->set_elements(FixedArray::cast(obj));
  }
  return result;
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_NewClosure) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 3);
  CONVERT_ARG_HANDLE_CHECKED(Context, context, 0);
  CONVERT_ARG_HANDLE_CHECKED(SharedFunctionInfo, shared, 1);
  CONVERT_BOOLEAN_ARG_CHECKED(pretenure, 2);

  // The caller ensures that we pretenure closures that are assigned
  // directly to properties.
  PretenureFlag pretenure_flag = pretenure ? TENURED : NOT_TENURED;
  Handle<JSFunction> result =
      isolate->factory()->NewFunctionFromSharedFunctionInfo(shared,
                                                            context,
                                                            pretenure_flag);
  return *result;
}


// Find the arguments of the JavaScript function invocation that called
// into C++ code. Collect these in a newly allocated array of handles (possibly
// prefixed by a number of empty handles).
static SmartArrayPointer<Handle<Object> > GetCallerArguments(
    Isolate* isolate,
    int prefix_argc,
    int* total_argc) {
  // Find frame containing arguments passed to the caller.
  JavaScriptFrameIterator it(isolate);
  JavaScriptFrame* frame = it.frame();
  List<JSFunction*> functions(2);
  frame->GetFunctions(&functions);
  if (functions.length() > 1) {
    int inlined_jsframe_index = functions.length() - 1;
    JSFunction* inlined_function = functions[inlined_jsframe_index];
    Vector<SlotRef> args_slots =
        SlotRef::ComputeSlotMappingForArguments(
            frame,
            inlined_jsframe_index,
            inlined_function->shared()->formal_parameter_count());

    int args_count = args_slots.length();

    *total_argc = prefix_argc + args_count;
    SmartArrayPointer<Handle<Object> > param_data(
        NewArray<Handle<Object> >(*total_argc));
    for (int i = 0; i < args_count; i++) {
      Handle<Object> val = args_slots[i].GetValue(isolate);
      param_data[prefix_argc + i] = val;
    }

    args_slots.Dispose();

    return param_data;
  } else {
    it.AdvanceToArgumentsFrame();
    frame = it.frame();
    int args_count = frame->ComputeParametersCount();

    *total_argc = prefix_argc + args_count;
    SmartArrayPointer<Handle<Object> > param_data(
        NewArray<Handle<Object> >(*total_argc));
    for (int i = 0; i < args_count; i++) {
      Handle<Object> val = Handle<Object>(frame->GetParameter(i), isolate);
      param_data[prefix_argc + i] = val;
    }
    return param_data;
  }
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_FunctionBindArguments) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 4);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, bound_function, 0);
  RUNTIME_ASSERT(args[3]->IsNumber());
  Handle<Object> bindee = args.at<Object>(1);

  // TODO(lrn): Create bound function in C++ code from premade shared info.
  bound_function->shared()->set_bound(true);
  // Get all arguments of calling function (Function.prototype.bind).
  int argc = 0;
  SmartArrayPointer<Handle<Object> > arguments =
      GetCallerArguments(isolate, 0, &argc);
  // Don't count the this-arg.
  if (argc > 0) {
    ASSERT(*arguments[0] == args[2]);
    argc--;
  } else {
    ASSERT(args[2]->IsUndefined());
  }
  // Initialize array of bindings (function, this, and any existing arguments
  // if the function was already bound).
  Handle<FixedArray> new_bindings;
  int i;
  if (bindee->IsJSFunction() && JSFunction::cast(*bindee)->shared()->bound()) {
    Handle<FixedArray> old_bindings(
        JSFunction::cast(*bindee)->function_bindings());
    new_bindings =
        isolate->factory()->NewFixedArray(old_bindings->length() + argc);
    bindee = Handle<Object>(old_bindings->get(JSFunction::kBoundFunctionIndex),
                            isolate);
    i = 0;
    for (int n = old_bindings->length(); i < n; i++) {
      new_bindings->set(i, old_bindings->get(i));
    }
  } else {
    int array_size = JSFunction::kBoundArgumentsStartIndex + argc;
    new_bindings = isolate->factory()->NewFixedArray(array_size);
    new_bindings->set(JSFunction::kBoundFunctionIndex, *bindee);
    new_bindings->set(JSFunction::kBoundThisIndex, args[2]);
    i = 2;
  }
  // Copy arguments, skipping the first which is "this_arg".
  for (int j = 0; j < argc; j++, i++) {
    new_bindings->set(i, *arguments[j + 1]);
  }
  new_bindings->set_map_no_write_barrier(
      isolate->heap()->fixed_cow_array_map());
  bound_function->set_function_bindings(*new_bindings);

  // Update length.
  Handle<String> length_string = isolate->factory()->length_string();
  Handle<Object> new_length(args.at<Object>(3));
  PropertyAttributes attr =
      static_cast<PropertyAttributes>(DONT_DELETE | DONT_ENUM | READ_ONLY);
  ForceSetProperty(bound_function, length_string, new_length, attr);
  return *bound_function;
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_BoundFunctionGetBindings) {
  HandleScope handles(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, callable, 0);
  if (callable->IsJSFunction()) {
    Handle<JSFunction> function = Handle<JSFunction>::cast(callable);
    if (function->shared()->bound()) {
      Handle<FixedArray> bindings(function->function_bindings());
      ASSERT(bindings->map() == isolate->heap()->fixed_cow_array_map());
      return *isolate->factory()->NewJSArrayWithElements(bindings);
    }
  }
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_NewObjectFromBound) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  // First argument is a function to use as a constructor.
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);
  RUNTIME_ASSERT(function->shared()->bound());

  // The argument is a bound function. Extract its bound arguments
  // and callable.
  Handle<FixedArray> bound_args =
      Handle<FixedArray>(FixedArray::cast(function->function_bindings()));
  int bound_argc = bound_args->length() - JSFunction::kBoundArgumentsStartIndex;
  Handle<Object> bound_function(
      JSReceiver::cast(bound_args->get(JSFunction::kBoundFunctionIndex)),
      isolate);
  ASSERT(!bound_function->IsJSFunction() ||
         !Handle<JSFunction>::cast(bound_function)->shared()->bound());

  int total_argc = 0;
  SmartArrayPointer<Handle<Object> > param_data =
      GetCallerArguments(isolate, bound_argc, &total_argc);
  for (int i = 0; i < bound_argc; i++) {
    param_data[i] = Handle<Object>(bound_args->get(
        JSFunction::kBoundArgumentsStartIndex + i), isolate);
  }

  if (!bound_function->IsJSFunction()) {
    bool exception_thrown;
    bound_function = Execution::TryGetConstructorDelegate(bound_function,
                                                          &exception_thrown);
    if (exception_thrown) return Failure::Exception();
  }
  ASSERT(bound_function->IsJSFunction());

  bool exception = false;
  Handle<Object> result =
      Execution::New(Handle<JSFunction>::cast(bound_function),
                     total_argc, *param_data, &exception);
  if (exception) {
    return Failure::Exception();
  }
  ASSERT(!result.is_null());
  return *result;
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_NewObject) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);

  Handle<Object> constructor = args.at<Object>(0);

  // If the constructor isn't a proper function we throw a type error.
  if (!constructor->IsJSFunction()) {
    Vector< Handle<Object> > arguments = HandleVector(&constructor, 1);
    Handle<Object> type_error =
        isolate->factory()->NewTypeError("not_constructor", arguments);
    return isolate->Throw(*type_error);
  }

  Handle<JSFunction> function = Handle<JSFunction>::cast(constructor);

  // If function should not have prototype, construction is not allowed. In this
  // case generated code bailouts here, since function has no initial_map.
  if (!function->should_have_prototype() && !function->shared()->bound()) {
    Vector< Handle<Object> > arguments = HandleVector(&constructor, 1);
    Handle<Object> type_error =
        isolate->factory()->NewTypeError("not_constructor", arguments);
    return isolate->Throw(*type_error);
  }

#ifdef ENABLE_DEBUGGER_SUPPORT
  Debug* debug = isolate->debug();
  // Handle stepping into constructors if step into is active.
  if (debug->StepInActive()) {
    debug->HandleStepIn(function, Handle<Object>::null(), 0, true);
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
      return isolate->context()->global_object();
    }
  }

  // The function should be compiled for the optimization hints to be
  // available.
  JSFunction::EnsureCompiled(function, CLEAR_EXCEPTION);

  Handle<SharedFunctionInfo> shared(function->shared(), isolate);
  if (!function->has_initial_map() &&
      shared->IsInobjectSlackTrackingInProgress()) {
    // The tracking is already in progress for another function. We can only
    // track one initial_map at a time, so we force the completion before the
    // function is called as a constructor for the first time.
    shared->CompleteInobjectSlackTracking();
  }

  Handle<JSObject> result = isolate->factory()->NewJSObject(function);
  RETURN_IF_EMPTY_HANDLE(isolate, result);

  isolate->counters()->constructed_objects()->Increment();
  isolate->counters()->constructed_objects_runtime()->Increment();

  return *result;
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_FinalizeInstanceSize) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);

  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);
  function->shared()->CompleteInobjectSlackTracking();

  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_LazyCompile) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);

  Handle<JSFunction> function = args.at<JSFunction>(0);
#ifdef DEBUG
  if (FLAG_trace_lazy && !function->shared()->is_compiled()) {
    PrintF("[lazy: ");
    function->PrintName();
    PrintF("]\n");
  }
#endif

  // Compile the target function.
  ASSERT(!function->is_compiled());
  if (!JSFunction::CompileLazy(function, KEEP_EXCEPTION)) {
    return Failure::Exception();
  }

  // All done. Return the compiled code.
  ASSERT(function->is_compiled());
  return function->code();
}


bool AllowOptimization(Isolate* isolate, Handle<JSFunction> function) {
  // If the function is not compiled ignore the lazy
  // recompilation. This can happen if the debugger is activated and
  // the function is returned to the not compiled state.
  if (!function->shared()->is_compiled()) return false;

  // If the function is not optimizable or debugger is active continue using the
  // code from the full compiler.
  if (!FLAG_crankshaft ||
      function->shared()->optimization_disabled() ||
      isolate->DebuggerHasBreakPoints()) {
    if (FLAG_trace_opt) {
      PrintF("[failed to optimize ");
      function->PrintName();
      PrintF(": is code optimizable: %s, is debugger enabled: %s]\n",
          function->shared()->optimization_disabled() ? "F" : "T",
          isolate->DebuggerHasBreakPoints() ? "T" : "F");
    }
    return false;
  }
  return true;
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_LazyRecompile) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  Handle<JSFunction> function = args.at<JSFunction>(0);

  if (!AllowOptimization(isolate, function)) {
    function->ReplaceCode(function->shared()->code());
    return function->code();
  }
  function->shared()->code()->set_profiler_ticks(0);
  if (JSFunction::CompileOptimized(function,
                                   BailoutId::None(),
                                   CLEAR_EXCEPTION)) {
    return function->code();
  }
  if (FLAG_trace_opt) {
    PrintF("[failed to optimize ");
    function->PrintName();
    PrintF(": optimized compilation failed]\n");
  }
  function->ReplaceCode(function->shared()->code());
  return function->code();
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_ParallelRecompile) {
  HandleScope handle_scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);
  if (!AllowOptimization(isolate, function)) {
    function->ReplaceCode(function->shared()->code());
    return isolate->heap()->undefined_value();
  }
  function->shared()->code()->set_profiler_ticks(0);
  ASSERT(FLAG_parallel_recompilation);
  Compiler::RecompileParallel(function);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_InstallRecompiledCode) {
  HandleScope handle_scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);
  ASSERT(V8::UseCrankshaft() && FLAG_parallel_recompilation);
  isolate->optimizing_compiler_thread()->InstallOptimizedFunctions();
  return function->code();
}


class ActivationsFinder : public ThreadVisitor {
 public:
  explicit ActivationsFinder(JSFunction* function)
      : function_(function), has_activations_(false) {}

  void VisitThread(Isolate* isolate, ThreadLocalTop* top) {
    if (has_activations_) return;

    for (JavaScriptFrameIterator it(isolate, top); !it.done(); it.Advance()) {
      JavaScriptFrame* frame = it.frame();
      if (frame->is_optimized() && frame->function() == function_) {
        has_activations_ = true;
        return;
      }
    }
  }

  bool has_activations() { return has_activations_; }

 private:
  JSFunction* function_;
  bool has_activations_;
};


RUNTIME_FUNCTION(MaybeObject*, Runtime_NotifyStubFailure) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 0);
  Deoptimizer* deoptimizer = Deoptimizer::Grab(isolate);
  ASSERT(AllowHeapAllocation::IsAllowed());
  delete deoptimizer;
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_NotifyDeoptimized) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  RUNTIME_ASSERT(args[0]->IsSmi());
  Deoptimizer::BailoutType type =
      static_cast<Deoptimizer::BailoutType>(args.smi_at(0));
  Deoptimizer* deoptimizer = Deoptimizer::Grab(isolate);
  ASSERT(AllowHeapAllocation::IsAllowed());

  ASSERT(deoptimizer->compiled_code_kind() == Code::OPTIMIZED_FUNCTION);

  // Make sure to materialize objects before causing any allocation.
  JavaScriptFrameIterator it(isolate);
  deoptimizer->MaterializeHeapObjects(&it);
  delete deoptimizer;

  JavaScriptFrame* frame = it.frame();
  RUNTIME_ASSERT(frame->function()->IsJSFunction());
  Handle<JSFunction> function(JSFunction::cast(frame->function()), isolate);
  Handle<Code> optimized_code(function->code());
  RUNTIME_ASSERT((type != Deoptimizer::EAGER &&
                  type != Deoptimizer::SOFT) || function->IsOptimized());

  // Avoid doing too much work when running with --always-opt and keep
  // the optimized code around.
  if (FLAG_always_opt || type == Deoptimizer::LAZY) {
    return isolate->heap()->undefined_value();
  }

  // Find other optimized activations of the function or functions that
  // share the same optimized code.
  bool has_other_activations = false;
  while (!it.done()) {
    JavaScriptFrame* frame = it.frame();
    JSFunction* other_function = JSFunction::cast(frame->function());
    if (frame->is_optimized() && other_function->code() == function->code()) {
      has_other_activations = true;
      break;
    }
    it.Advance();
  }

  if (!has_other_activations) {
    ActivationsFinder activations_finder(*function);
    isolate->thread_manager()->IterateArchivedThreads(&activations_finder);
    has_other_activations = activations_finder.has_activations();
  }

  if (!has_other_activations) {
    if (FLAG_trace_deopt) {
      PrintF("[removing optimized code for: ");
      function->PrintName();
      PrintF("]\n");
    }
    function->ReplaceCode(function->shared()->code());
  } else {
    Deoptimizer::DeoptimizeFunction(*function);
  }
  // Evict optimized code for this function from the cache so that it doesn't
  // get used for new closures.
  function->shared()->EvictFromOptimizedCodeMap(*optimized_code,
                                                "notify deoptimized");

  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_NotifyOSR) {
  SealHandleScope shs(isolate);
  Deoptimizer* deoptimizer = Deoptimizer::Grab(isolate);
  delete deoptimizer;
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_DeoptimizeFunction) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);
  if (!function->IsOptimized()) return isolate->heap()->undefined_value();

  Deoptimizer::DeoptimizeFunction(*function);

  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_ClearFunctionTypeFeedback) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);
  Code* unoptimized = function->shared()->code();
  if (unoptimized->kind() == Code::FUNCTION) {
    unoptimized->ClearInlineCaches();
    unoptimized->ClearTypeFeedbackCells(isolate->heap());
  }
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_RunningInSimulator) {
  SealHandleScope shs(isolate);
#if defined(USE_SIMULATOR)
  return isolate->heap()->true_value();
#else
  return isolate->heap()->false_value();
#endif
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_IsParallelRecompilationSupported) {
  HandleScope scope(isolate);
  return FLAG_parallel_recompilation
      ? isolate->heap()->true_value() : isolate->heap()->false_value();
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_OptimizeFunctionOnNextCall) {
  HandleScope scope(isolate);
  RUNTIME_ASSERT(args.length() == 1 || args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);

  if (!function->IsOptimizable()) return isolate->heap()->undefined_value();
  function->MarkForLazyRecompilation();

  Code* unoptimized = function->shared()->code();
  if (args.length() == 2 &&
      unoptimized->kind() == Code::FUNCTION) {
    CONVERT_ARG_HANDLE_CHECKED(String, type, 1);
    if (type->IsOneByteEqualTo(STATIC_ASCII_VECTOR("osr"))) {
      for (int i = 0; i <= Code::kMaxLoopNestingMarker; i++) {
        unoptimized->set_allow_osr_at_loop_nesting_level(i);
        isolate->runtime_profiler()->AttemptOnStackReplacement(*function);
      }
    } else if (type->IsOneByteEqualTo(STATIC_ASCII_VECTOR("parallel"))) {
      function->MarkForParallelRecompilation();
    }
  }

  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_CompleteOptimization) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);
  if (FLAG_parallel_recompilation && V8::UseCrankshaft()) {
    // While function is in optimization pipeline, it is marked accordingly.
    // Note that if the debugger is activated during parallel recompilation,
    // the function will be marked with the lazy-recompile builtin, which is
    // not related to parallel recompilation.
    while (function->IsMarkedForParallelRecompilation() ||
           function->IsInRecompileQueue() ||
           function->IsMarkedForInstallingRecompiledCode()) {
      isolate->optimizing_compiler_thread()->InstallOptimizedFunctions();
      OS::Sleep(50);
    }
  }
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_GetOptimizationStatus) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  // The least significant bit (after untagging) indicates whether the
  // function is currently optimized, regardless of reason.
  if (!V8::UseCrankshaft()) {
    return Smi::FromInt(4);  // 4 == "never".
  }
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);
  if (FLAG_parallel_recompilation) {
    if (function->IsMarkedForLazyRecompilation()) {
      return Smi::FromInt(5);
    }
  }
  if (FLAG_always_opt) {
    // We may have always opt, but that is more best-effort than a real
    // promise, so we still say "no" if it is not optimized.
    return function->IsOptimized() ? Smi::FromInt(3)   // 3 == "always".
                                   : Smi::FromInt(2);  // 2 == "no".
  }
  return function->IsOptimized() ? Smi::FromInt(1)   // 1 == "yes".
                                 : Smi::FromInt(2);  // 2 == "no".
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_GetOptimizationCount) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);
  return Smi::FromInt(function->shared()->opt_count());
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_CompileForOnStackReplacement) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);

  // We're not prepared to handle a function with arguments object.
  ASSERT(!function->shared()->uses_arguments());

  // We have hit a back edge in an unoptimized frame for a function that was
  // selected for on-stack replacement.  Find the unoptimized code object.
  Handle<Code> unoptimized(function->shared()->code(), isolate);
  // Keep track of whether we've succeeded in optimizing.
  bool succeeded = unoptimized->optimizable();
  if (succeeded) {
    // If we are trying to do OSR when there are already optimized
    // activations of the function, it means (a) the function is directly or
    // indirectly recursive and (b) an optimized invocation has been
    // deoptimized so that we are currently in an unoptimized activation.
    // Check for optimized activations of this function.
    JavaScriptFrameIterator it(isolate);
    while (succeeded && !it.done()) {
      JavaScriptFrame* frame = it.frame();
      succeeded = !frame->is_optimized() || frame->function() != *function;
      it.Advance();
    }
  }

  BailoutId ast_id = BailoutId::None();
  if (succeeded) {
    // The top JS function is this one, the PC is somewhere in the
    // unoptimized code.
    JavaScriptFrameIterator it(isolate);
    JavaScriptFrame* frame = it.frame();
    ASSERT(frame->function() == *function);
    ASSERT(frame->LookupCode() == *unoptimized);
    ASSERT(unoptimized->contains(frame->pc()));

    // Use linear search of the unoptimized code's back edge table to find
    // the AST id matching the PC.
    Address start = unoptimized->instruction_start();
    unsigned target_pc_offset = static_cast<unsigned>(frame->pc() - start);
    Address table_cursor = start + unoptimized->back_edge_table_offset();
    uint32_t table_length = Memory::uint32_at(table_cursor);
    table_cursor += kIntSize;
    uint8_t loop_depth = 0;
    for (unsigned i = 0; i < table_length; ++i) {
      // Table entries are (AST id, pc offset) pairs.
      uint32_t pc_offset = Memory::uint32_at(table_cursor + kIntSize);
      if (pc_offset == target_pc_offset) {
        ast_id = BailoutId(static_cast<int>(Memory::uint32_at(table_cursor)));
        loop_depth = Memory::uint8_at(table_cursor + 2 * kIntSize);
        break;
      }
      table_cursor += FullCodeGenerator::kBackEdgeEntrySize;
    }
    ASSERT(!ast_id.IsNone());
    if (FLAG_trace_osr) {
      PrintF("[replacing on-stack at AST id %d, loop depth %d in ",
              ast_id.ToInt(), loop_depth);
      function->PrintName();
      PrintF("]\n");
    }

    // Try to compile the optimized code.  A true return value from
    // CompileOptimized means that compilation succeeded, not necessarily
    // that optimization succeeded.
    if (JSFunction::CompileOptimized(function, ast_id, CLEAR_EXCEPTION) &&
        function->IsOptimized()) {
      DeoptimizationInputData* data = DeoptimizationInputData::cast(
          function->code()->deoptimization_data());
      if (data->OsrPcOffset()->value() >= 0) {
        if (FLAG_trace_osr) {
          PrintF("[on-stack replacement offset %d in optimized code]\n",
               data->OsrPcOffset()->value());
        }
        ASSERT(BailoutId(data->OsrAstId()->value()) == ast_id);
      } else {
        // We may never generate the desired OSR entry if we emit an
        // early deoptimize.
        succeeded = false;
      }
    } else {
      succeeded = false;
    }
  }

  // Revert to the original interrupt calls in the original unoptimized code.
  if (FLAG_trace_osr) {
    PrintF("[restoring original interrupt calls in ");
    function->PrintName();
    PrintF("]\n");
  }
  InterruptStub interrupt_stub;
  Handle<Code> interrupt_code = interrupt_stub.GetCode(isolate);
  Handle<Code> replacement_code = isolate->builtins()->OnStackReplacement();
  Deoptimizer::RevertInterruptCode(*unoptimized,
                                   *interrupt_code,
                                   *replacement_code);

  // If the optimization attempt succeeded, return the AST id tagged as a
  // smi. This tells the builtin that we need to translate the unoptimized
  // frame to an optimized one.
  if (succeeded) {
    ASSERT(function->code()->kind() == Code::OPTIMIZED_FUNCTION);
    return Smi::FromInt(ast_id.ToInt());
  } else {
    if (function->IsMarkedForLazyRecompilation()) {
      function->ReplaceCode(function->shared()->code());
    }
    return Smi::FromInt(-1);
  }
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_CheckIsBootstrapping) {
  SealHandleScope shs(isolate);
  RUNTIME_ASSERT(isolate->bootstrapper()->IsActive());
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_GetRootNaN) {
  SealHandleScope shs(isolate);
  RUNTIME_ASSERT(isolate->bootstrapper()->IsActive());
  return isolate->heap()->nan_value();
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_Call) {
  HandleScope scope(isolate);
  ASSERT(args.length() >= 2);
  int argc = args.length() - 2;
  CONVERT_ARG_CHECKED(JSReceiver, fun, argc + 1);
  Object* receiver = args[0];

  // If there are too many arguments, allocate argv via malloc.
  const int argv_small_size = 10;
  Handle<Object> argv_small_buffer[argv_small_size];
  SmartArrayPointer<Handle<Object> > argv_large_buffer;
  Handle<Object>* argv = argv_small_buffer;
  if (argc > argv_small_size) {
    argv = new Handle<Object>[argc];
    if (argv == NULL) return isolate->StackOverflow();
    argv_large_buffer = SmartArrayPointer<Handle<Object> >(argv);
  }

  for (int i = 0; i < argc; ++i) {
     MaybeObject* maybe = args[1 + i];
     Object* object;
     if (!maybe->To<Object>(&object)) return maybe;
     argv[i] = Handle<Object>(object, isolate);
  }

  bool threw;
  Handle<JSReceiver> hfun(fun);
  Handle<Object> hreceiver(receiver, isolate);
  Handle<Object> result =
      Execution::Call(hfun, hreceiver, argc, argv, &threw, true);

  if (threw) return Failure::Exception();
  return *result;
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_Apply) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 5);
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, fun, 0);
  Handle<Object> receiver = args.at<Object>(1);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, arguments, 2);
  CONVERT_SMI_ARG_CHECKED(offset, 3);
  CONVERT_SMI_ARG_CHECKED(argc, 4);
  ASSERT(offset >= 0);
  ASSERT(argc >= 0);

  // If there are too many arguments, allocate argv via malloc.
  const int argv_small_size = 10;
  Handle<Object> argv_small_buffer[argv_small_size];
  SmartArrayPointer<Handle<Object> > argv_large_buffer;
  Handle<Object>* argv = argv_small_buffer;
  if (argc > argv_small_size) {
    argv = new Handle<Object>[argc];
    if (argv == NULL) return isolate->StackOverflow();
    argv_large_buffer = SmartArrayPointer<Handle<Object> >(argv);
  }

  for (int i = 0; i < argc; ++i) {
    argv[i] = Object::GetElement(arguments, offset + i);
  }

  bool threw;
  Handle<Object> result =
      Execution::Call(fun, receiver, argc, argv, &threw, true);

  if (threw) return Failure::Exception();
  return *result;
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_GetFunctionDelegate) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  RUNTIME_ASSERT(!args[0]->IsJSFunction());
  return *Execution::GetFunctionDelegate(args.at<Object>(0));
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_GetConstructorDelegate) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  RUNTIME_ASSERT(!args[0]->IsJSFunction());
  return *Execution::GetConstructorDelegate(args.at<Object>(0));
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_NewGlobalContext) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 2);

  CONVERT_ARG_CHECKED(JSFunction, function, 0);
  CONVERT_ARG_CHECKED(ScopeInfo, scope_info, 1);
  Context* result;
  MaybeObject* maybe_result =
      isolate->heap()->AllocateGlobalContext(function, scope_info);
  if (!maybe_result->To(&result)) return maybe_result;

  ASSERT(function->context() == isolate->context());
  ASSERT(function->context()->global_object() == result->global_object());
  isolate->set_context(result);
  result->global_object()->set_global_context(result);

  return result;  // non-failure
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_NewFunctionContext) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);

  CONVERT_ARG_CHECKED(JSFunction, function, 0);
  int length = function->shared()->scope_info()->ContextLength();
  Context* result;
  MaybeObject* maybe_result =
      isolate->heap()->AllocateFunctionContext(length, function);
  if (!maybe_result->To(&result)) return maybe_result;

  isolate->set_context(result);

  return result;  // non-failure
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_PushWithContext) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 2);
  JSObject* extension_object;
  if (args[0]->IsJSObject()) {
    extension_object = JSObject::cast(args[0]);
  } else {
    // Convert the object to a proper JavaScript object.
    MaybeObject* maybe_js_object = args[0]->ToObject();
    if (!maybe_js_object->To(&extension_object)) {
      if (Failure::cast(maybe_js_object)->IsInternalError()) {
        HandleScope scope(isolate);
        Handle<Object> handle = args.at<Object>(0);
        Handle<Object> result =
            isolate->factory()->NewTypeError("with_expression",
                                             HandleVector(&handle, 1));
        return isolate->Throw(*result);
      } else {
        return maybe_js_object;
      }
    }
  }

  JSFunction* function;
  if (args[1]->IsSmi()) {
    // A smi sentinel indicates a context nested inside global code rather
    // than some function.  There is a canonical empty function that can be
    // gotten from the native context.
    function = isolate->context()->native_context()->closure();
  } else {
    function = JSFunction::cast(args[1]);
  }

  Context* context;
  MaybeObject* maybe_context =
      isolate->heap()->AllocateWithContext(function,
                                           isolate->context(),
                                           extension_object);
  if (!maybe_context->To(&context)) return maybe_context;
  isolate->set_context(context);
  return context;
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_PushCatchContext) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 3);
  String* name = String::cast(args[0]);
  Object* thrown_object = args[1];
  JSFunction* function;
  if (args[2]->IsSmi()) {
    // A smi sentinel indicates a context nested inside global code rather
    // than some function.  There is a canonical empty function that can be
    // gotten from the native context.
    function = isolate->context()->native_context()->closure();
  } else {
    function = JSFunction::cast(args[2]);
  }
  Context* context;
  MaybeObject* maybe_context =
      isolate->heap()->AllocateCatchContext(function,
                                            isolate->context(),
                                            name,
                                            thrown_object);
  if (!maybe_context->To(&context)) return maybe_context;
  isolate->set_context(context);
  return context;
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_PushBlockContext) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 2);
  ScopeInfo* scope_info = ScopeInfo::cast(args[0]);
  JSFunction* function;
  if (args[1]->IsSmi()) {
    // A smi sentinel indicates a context nested inside global code rather
    // than some function.  There is a canonical empty function that can be
    // gotten from the native context.
    function = isolate->context()->native_context()->closure();
  } else {
    function = JSFunction::cast(args[1]);
  }
  Context* context;
  MaybeObject* maybe_context =
      isolate->heap()->AllocateBlockContext(function,
                                            isolate->context(),
                                            scope_info);
  if (!maybe_context->To(&context)) return maybe_context;
  isolate->set_context(context);
  return context;
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_IsJSModule) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  Object* obj = args[0];
  return isolate->heap()->ToBoolean(obj->IsJSModule());
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_PushModuleContext) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 2);
  CONVERT_SMI_ARG_CHECKED(index, 0);

  if (!args[1]->IsScopeInfo()) {
    // Module already initialized. Find hosting context and retrieve context.
    Context* host = Context::cast(isolate->context())->global_context();
    Context* context = Context::cast(host->get(index));
    ASSERT(context->previous() == isolate->context());
    isolate->set_context(context);
    return context;
  }

  CONVERT_ARG_HANDLE_CHECKED(ScopeInfo, scope_info, 1);

  // Allocate module context.
  HandleScope scope(isolate);
  Factory* factory = isolate->factory();
  Handle<Context> context = factory->NewModuleContext(scope_info);
  Handle<JSModule> module = factory->NewJSModule(context, scope_info);
  context->set_module(*module);
  Context* previous = isolate->context();
  context->set_previous(previous);
  context->set_closure(previous->closure());
  context->set_global_object(previous->global_object());
  isolate->set_context(*context);

  // Find hosting scope and initialize internal variable holding module there.
  previous->global_context()->set(index, *context);

  return *context;
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_DeclareModules) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(FixedArray, descriptions, 0);
  Context* host_context = isolate->context();

  for (int i = 0; i < descriptions->length(); ++i) {
    Handle<ModuleInfo> description(ModuleInfo::cast(descriptions->get(i)));
    int host_index = description->host_index();
    Handle<Context> context(Context::cast(host_context->get(host_index)));
    Handle<JSModule> module(context->module());

    for (int j = 0; j < description->length(); ++j) {
      Handle<String> name(description->name(j));
      VariableMode mode = description->mode(j);
      int index = description->index(j);
      switch (mode) {
        case VAR:
        case LET:
        case CONST:
        case CONST_HARMONY: {
          PropertyAttributes attr =
              IsImmutableVariableMode(mode) ? FROZEN : SEALED;
          Handle<AccessorInfo> info =
              Accessors::MakeModuleExport(name, index, attr);
          Handle<Object> result = SetAccessor(module, info);
          ASSERT(!(result.is_null() || result->IsUndefined()));
          USE(result);
          break;
        }
        case MODULE: {
          Object* referenced_context = Context::cast(host_context)->get(index);
          Handle<JSModule> value(Context::cast(referenced_context)->module());
          JSReceiver::SetProperty(module, name, value, FROZEN, kStrictMode);
          break;
        }
        case INTERNAL:
        case TEMPORARY:
        case DYNAMIC:
        case DYNAMIC_GLOBAL:
        case DYNAMIC_LOCAL:
          UNREACHABLE();
      }
    }

    JSObject::PreventExtensions(module);
  }

  ASSERT(!isolate->has_pending_exception());
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_DeleteContextSlot) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);

  CONVERT_ARG_HANDLE_CHECKED(Context, context, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, name, 1);

  int index;
  PropertyAttributes attributes;
  ContextLookupFlags flags = FOLLOW_CHAINS;
  BindingFlags binding_flags;
  Handle<Object> holder = context->Lookup(name,
                                          flags,
                                          &index,
                                          &attributes,
                                          &binding_flags);

  // If the slot was not found the result is true.
  if (holder.is_null()) {
    return isolate->heap()->true_value();
  }

  // If the slot was found in a context, it should be DONT_DELETE.
  if (holder->IsContext()) {
    return isolate->heap()->false_value();
  }

  // The slot was found in a JSObject, either a context extension object,
  // the global object, or the subject of a with.  Try to delete it
  // (respecting DONT_DELETE).
  Handle<JSObject> object = Handle<JSObject>::cast(holder);
  return object->DeleteProperty(*name, JSReceiver::NORMAL_DELETION);
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
  MaybeObject* x;
  MaybeObject* y;
};

static inline ObjectPair MakePair(MaybeObject* x, MaybeObject* y) {
  ObjectPair result = {x, y};
  // Pointers x and y returned in rax and rdx, in AMD-x64-abi.
  // In Win64 they are assigned to a hidden first argument.
  return result;
}
#else
typedef uint64_t ObjectPair;
static inline ObjectPair MakePair(MaybeObject* x, MaybeObject* y) {
  return reinterpret_cast<uint32_t>(x) |
      (reinterpret_cast<ObjectPair>(y) << 32);
}
#endif


static inline MaybeObject* Unhole(Heap* heap,
                                  MaybeObject* x,
                                  PropertyAttributes attributes) {
  ASSERT(!x->IsTheHole() || (attributes & READ_ONLY) != 0);
  USE(attributes);
  return x->IsTheHole() ? heap->undefined_value() : x;
}


static Object* ComputeReceiverForNonGlobal(Isolate* isolate,
                                           JSObject* holder) {
  ASSERT(!holder->IsGlobalObject());
  Context* top = isolate->context();
  // Get the context extension function.
  JSFunction* context_extension_function =
      top->native_context()->context_extension_function();
  // If the holder isn't a context extension object, we just return it
  // as the receiver. This allows arguments objects to be used as
  // receivers, but only if they are put in the context scope chain
  // explicitly via a with-statement.
  Object* constructor = holder->map()->constructor();
  if (constructor != context_extension_function) return holder;
  // Fall back to using the global object as the implicit receiver if
  // the property turns out to be a local variable allocated in a
  // context extension object - introduced via eval. Implicit global
  // receivers are indicated with the hole value.
  return isolate->heap()->the_hole_value();
}


static ObjectPair LoadContextSlotHelper(Arguments args,
                                        Isolate* isolate,
                                        bool throw_error) {
  HandleScope scope(isolate);
  ASSERT_EQ(2, args.length());

  if (!args[0]->IsContext() || !args[1]->IsString()) {
    return MakePair(isolate->ThrowIllegalOperation(), NULL);
  }
  Handle<Context> context = args.at<Context>(0);
  Handle<String> name = args.at<String>(1);

  int index;
  PropertyAttributes attributes;
  ContextLookupFlags flags = FOLLOW_CHAINS;
  BindingFlags binding_flags;
  Handle<Object> holder = context->Lookup(name,
                                          flags,
                                          &index,
                                          &attributes,
                                          &binding_flags);

  // If the index is non-negative, the slot has been found in a context.
  if (index >= 0) {
    ASSERT(holder->IsContext());
    // If the "property" we were looking for is a local variable, the
    // receiver is the global object; see ECMA-262, 3rd., 10.1.6 and 10.2.3.
    //
    // Use the hole as the receiver to signal that the receiver is implicit
    // and that the global receiver should be used (as distinguished from an
    // explicit receiver that happens to be a global object).
    Handle<Object> receiver = isolate->factory()->the_hole_value();
    Object* value = Context::cast(*holder)->get(index);
    // Check for uninitialized bindings.
    switch (binding_flags) {
      case MUTABLE_CHECK_INITIALIZED:
      case IMMUTABLE_CHECK_INITIALIZED_HARMONY:
        if (value->IsTheHole()) {
          Handle<Object> reference_error =
              isolate->factory()->NewReferenceError("not_defined",
                                                    HandleVector(&name, 1));
          return MakePair(isolate->Throw(*reference_error), NULL);
        }
        // FALLTHROUGH
      case MUTABLE_IS_INITIALIZED:
      case IMMUTABLE_IS_INITIALIZED:
      case IMMUTABLE_IS_INITIALIZED_HARMONY:
        ASSERT(!value->IsTheHole());
        return MakePair(value, *receiver);
      case IMMUTABLE_CHECK_INITIALIZED:
        return MakePair(Unhole(isolate->heap(), value, attributes), *receiver);
      case MISSING_BINDING:
        UNREACHABLE();
        return MakePair(NULL, NULL);
    }
  }

  // Otherwise, if the slot was found the holder is a context extension
  // object, subject of a with, or a global object.  We read the named
  // property from it.
  if (!holder.is_null()) {
    Handle<JSObject> object = Handle<JSObject>::cast(holder);
    ASSERT(object->HasProperty(*name));
    // GetProperty below can cause GC.
    Handle<Object> receiver_handle(
        object->IsGlobalObject()
            ? GlobalObject::cast(*object)->global_receiver()
            : ComputeReceiverForNonGlobal(isolate, *object),
        isolate);

    // No need to unhole the value here.  This is taken care of by the
    // GetProperty function.
    MaybeObject* value = object->GetProperty(*name);
    return MakePair(value, *receiver_handle);
  }

  if (throw_error) {
    // The property doesn't exist - throw exception.
    Handle<Object> reference_error =
        isolate->factory()->NewReferenceError("not_defined",
                                              HandleVector(&name, 1));
    return MakePair(isolate->Throw(*reference_error), NULL);
  } else {
    // The property doesn't exist - return undefined.
    return MakePair(isolate->heap()->undefined_value(),
                    isolate->heap()->undefined_value());
  }
}


RUNTIME_FUNCTION(ObjectPair, Runtime_LoadContextSlot) {
  return LoadContextSlotHelper(args, isolate, true);
}


RUNTIME_FUNCTION(ObjectPair, Runtime_LoadContextSlotNoReferenceError) {
  return LoadContextSlotHelper(args, isolate, false);
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_StoreContextSlot) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 4);

  Handle<Object> value(args[0], isolate);
  CONVERT_ARG_HANDLE_CHECKED(Context, context, 1);
  CONVERT_ARG_HANDLE_CHECKED(String, name, 2);
  CONVERT_LANGUAGE_MODE_ARG(language_mode, 3);
  StrictModeFlag strict_mode = (language_mode == CLASSIC_MODE)
      ? kNonStrictMode : kStrictMode;

  int index;
  PropertyAttributes attributes;
  ContextLookupFlags flags = FOLLOW_CHAINS;
  BindingFlags binding_flags;
  Handle<Object> holder = context->Lookup(name,
                                          flags,
                                          &index,
                                          &attributes,
                                          &binding_flags);

  if (index >= 0) {
    // The property was found in a context slot.
    Handle<Context> context = Handle<Context>::cast(holder);
    if (binding_flags == MUTABLE_CHECK_INITIALIZED &&
        context->get(index)->IsTheHole()) {
      Handle<Object> error =
          isolate->factory()->NewReferenceError("not_defined",
                                                HandleVector(&name, 1));
      return isolate->Throw(*error);
    }
    // Ignore if read_only variable.
    if ((attributes & READ_ONLY) == 0) {
      // Context is a fixed array and set cannot fail.
      context->set(index, *value);
    } else if (strict_mode == kStrictMode) {
      // Setting read only property in strict mode.
      Handle<Object> error =
          isolate->factory()->NewTypeError("strict_cannot_assign",
                                           HandleVector(&name, 1));
      return isolate->Throw(*error);
    }
    return *value;
  }

  // Slow case: The property is not in a context slot.  It is either in a
  // context extension object, a property of the subject of a with, or a
  // property of the global object.
  Handle<JSObject> object;

  if (!holder.is_null()) {
    // The property exists on the holder.
    object = Handle<JSObject>::cast(holder);
  } else {
    // The property was not found.
    ASSERT(attributes == ABSENT);

    if (strict_mode == kStrictMode) {
      // Throw in strict mode (assignment to undefined variable).
      Handle<Object> error =
          isolate->factory()->NewReferenceError(
              "not_defined", HandleVector(&name, 1));
      return isolate->Throw(*error);
    }
    // In non-strict mode, the property is added to the global object.
    attributes = NONE;
    object = Handle<JSObject>(isolate->context()->global_object());
  }

  // Set the property if it's not read only or doesn't yet exist.
  if ((attributes & READ_ONLY) == 0 ||
      (object->GetLocalPropertyAttribute(*name) == ABSENT)) {
    RETURN_IF_EMPTY_HANDLE(
        isolate,
        JSReceiver::SetProperty(object, name, value, NONE, strict_mode));
  } else if (strict_mode == kStrictMode && (attributes & READ_ONLY) != 0) {
    // Setting read only property in strict mode.
    Handle<Object> error =
      isolate->factory()->NewTypeError(
          "strict_cannot_assign", HandleVector(&name, 1));
    return isolate->Throw(*error);
  }
  return *value;
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_Throw) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);

  return isolate->Throw(args[0]);
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_ReThrow) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);

  return isolate->ReThrow(args[0]);
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_PromoteScheduledException) {
  SealHandleScope shs(isolate);
  ASSERT_EQ(0, args.length());
  return isolate->PromoteScheduledException();
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_ThrowReferenceError) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);

  Handle<Object> name(args[0], isolate);
  Handle<Object> reference_error =
    isolate->factory()->NewReferenceError("not_defined",
                                          HandleVector(&name, 1));
  return isolate->Throw(*reference_error);
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_ThrowNotDateError) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 0);
  return isolate->Throw(*isolate->factory()->NewTypeError(
      "not_date_object", HandleVector<Object>(NULL, 0)));
}



RUNTIME_FUNCTION(MaybeObject*, Runtime_StackGuard) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 0);

  // First check if this is a real stack overflow.
  if (isolate->stack_guard()->IsStackOverflow()) {
    SealHandleScope shs(isolate);
    return isolate->StackOverflow();
  }

  return Execution::HandleStackGuardInterrupt(isolate);
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_Interrupt) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 0);
  return Execution::HandleStackGuardInterrupt(isolate);
}


static int StackSize(Isolate* isolate) {
  int n = 0;
  for (JavaScriptFrameIterator it(isolate); !it.done(); it.Advance()) n++;
  return n;
}


static void PrintTransition(Isolate* isolate, Object* result) {
  // indentation
  { const int nmax = 80;
    int n = StackSize(isolate);
    if (n <= nmax)
      PrintF("%4d:%*s", n, n, "");
    else
      PrintF("%4d:%*s", n, nmax, "...");
  }

  if (result == NULL) {
    JavaScriptFrame::PrintTop(isolate, stdout, true, false);
    PrintF(" {\n");
  } else {
    // function result
    PrintF("} -> ");
    result->ShortPrint();
    PrintF("\n");
  }
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_TraceEnter) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 0);
  PrintTransition(isolate, NULL);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_TraceExit) {
  SealHandleScope shs(isolate);
  PrintTransition(isolate, args[0]);
  return args[0];  // return TOS
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_DebugPrint) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);

#ifdef DEBUG
  if (args[0]->IsString()) {
    // If we have a string, assume it's a code "marker"
    // and print some interesting cpu debugging info.
    JavaScriptFrameIterator it(isolate);
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


RUNTIME_FUNCTION(MaybeObject*, Runtime_DebugTrace) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 0);
  isolate->PrintStack(stdout);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_DateCurrentTime) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 0);

  // According to ECMA-262, section 15.9.1, page 117, the precision of
  // the number in a Date object representing a particular instant in
  // time is milliseconds. Therefore, we floor the result of getting
  // the OS time.
  double millis = floor(OS::TimeCurrentMillis());
  return isolate->heap()->NumberFromDouble(millis);
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_DateParseString) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);

  CONVERT_ARG_HANDLE_CHECKED(String, str, 0);
  FlattenString(str);

  CONVERT_ARG_HANDLE_CHECKED(JSArray, output, 1);

  MaybeObject* maybe_result_array =
      output->EnsureCanContainHeapObjectElements();
  if (maybe_result_array->IsFailure()) return maybe_result_array;
  RUNTIME_ASSERT(output->HasFastObjectElements());

  DisallowHeapAllocation no_gc;

  FixedArray* output_array = FixedArray::cast(output->elements());
  RUNTIME_ASSERT(output_array->length() >= DateParser::OUTPUT_SIZE);
  bool result;
  String::FlatContent str_content = str->GetFlatContent();
  if (str_content.IsAscii()) {
    result = DateParser::Parse(str_content.ToOneByteVector(),
                               output_array,
                               isolate->unicode_cache());
  } else {
    ASSERT(str_content.IsTwoByte());
    result = DateParser::Parse(str_content.ToUC16Vector(),
                               output_array,
                               isolate->unicode_cache());
  }

  if (result) {
    return *output;
  } else {
    return isolate->heap()->null_value();
  }
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_DateLocalTimezone) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);

  CONVERT_DOUBLE_ARG_CHECKED(x, 0);
  int64_t time = isolate->date_cache()->EquivalentTime(static_cast<int64_t>(x));
  const char* zone = OS::LocalTimezone(static_cast<double>(time));
  return isolate->heap()->AllocateStringFromUtf8(CStrVector(zone));
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_DateToUTC) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);

  CONVERT_DOUBLE_ARG_CHECKED(x, 0);
  int64_t time = isolate->date_cache()->ToUTC(static_cast<int64_t>(x));

  return isolate->heap()->NumberFromDouble(static_cast<double>(time));
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_GlobalReceiver) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  Object* global = args[0];
  if (!global->IsJSGlobalObject()) return isolate->heap()->null_value();
  return JSGlobalObject::cast(global)->global_receiver();
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_ParseJson) {
  HandleScope scope(isolate);
  ASSERT_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(String, source, 0);

  source = Handle<String>(source->TryFlattenGetString());
  // Optimized fast case where we only have ASCII characters.
  Handle<Object> result;
  if (source->IsSeqOneByteString()) {
    result = JsonParser<true>::Parse(source);
  } else {
    result = JsonParser<false>::Parse(source);
  }
  if (result.is_null()) {
    // Syntax error or stack overflow in scanner.
    ASSERT(isolate->has_pending_exception());
    return Failure::Exception();
  }
  return *result;
}


bool CodeGenerationFromStringsAllowed(Isolate* isolate,
                                      Handle<Context> context) {
  ASSERT(context->allow_code_gen_from_strings()->IsFalse());
  // Check with callback if set.
  AllowCodeGenerationFromStringsCallback callback =
      isolate->allow_code_gen_callback();
  if (callback == NULL) {
    // No callback set and code generation disallowed.
    return false;
  } else {
    // Callback set. Let it decide if code generation is allowed.
    VMState<EXTERNAL> state(isolate);
    return callback(v8::Utils::ToLocal(context));
  }
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_CompileString) {
  HandleScope scope(isolate);
  ASSERT_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(String, source, 0);
  CONVERT_BOOLEAN_ARG_CHECKED(function_literal_only, 1);

  // Extract native context.
  Handle<Context> context(isolate->context()->native_context());

  // Check if native context allows code generation from
  // strings. Throw an exception if it doesn't.
  if (context->allow_code_gen_from_strings()->IsFalse() &&
      !CodeGenerationFromStringsAllowed(isolate, context)) {
    Handle<Object> error_message =
        context->ErrorMessageForCodeGenerationFromStrings();
    return isolate->Throw(*isolate->factory()->NewEvalError(
        "code_gen_from_strings", HandleVector<Object>(&error_message, 1)));
  }

  // Compile source string in the native context.
  ParseRestriction restriction = function_literal_only
      ? ONLY_SINGLE_FUNCTION_LITERAL : NO_PARSE_RESTRICTION;
  Handle<SharedFunctionInfo> shared = Compiler::CompileEval(
      source, context, true, CLASSIC_MODE, restriction, RelocInfo::kNoPosition);
  if (shared.is_null()) return Failure::Exception();
  Handle<JSFunction> fun =
      isolate->factory()->NewFunctionFromSharedFunctionInfo(shared,
                                                            context,
                                                            NOT_TENURED);
  return *fun;
}


static ObjectPair CompileGlobalEval(Isolate* isolate,
                                    Handle<String> source,
                                    Handle<Object> receiver,
                                    LanguageMode language_mode,
                                    int scope_position) {
  Handle<Context> context = Handle<Context>(isolate->context());
  Handle<Context> native_context = Handle<Context>(context->native_context());

  // Check if native context allows code generation from
  // strings. Throw an exception if it doesn't.
  if (native_context->allow_code_gen_from_strings()->IsFalse() &&
      !CodeGenerationFromStringsAllowed(isolate, native_context)) {
    Handle<Object> error_message =
        native_context->ErrorMessageForCodeGenerationFromStrings();
    isolate->Throw(*isolate->factory()->NewEvalError(
        "code_gen_from_strings", HandleVector<Object>(&error_message, 1)));
    return MakePair(Failure::Exception(), NULL);
  }

  // Deal with a normal eval call with a string argument. Compile it
  // and return the compiled function bound in the local context.
  Handle<SharedFunctionInfo> shared = Compiler::CompileEval(
      source,
      context,
      context->IsNativeContext(),
      language_mode,
      NO_PARSE_RESTRICTION,
      scope_position);
  if (shared.is_null()) return MakePair(Failure::Exception(), NULL);
  Handle<JSFunction> compiled =
      isolate->factory()->NewFunctionFromSharedFunctionInfo(
          shared, context, NOT_TENURED);
  return MakePair(*compiled, *receiver);
}


RUNTIME_FUNCTION(ObjectPair, Runtime_ResolvePossiblyDirectEval) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 5);

  Handle<Object> callee = args.at<Object>(0);

  // If "eval" didn't refer to the original GlobalEval, it's not a
  // direct call to eval.
  // (And even if it is, but the first argument isn't a string, just let
  // execution default to an indirect call to eval, which will also return
  // the first argument without doing anything).
  if (*callee != isolate->native_context()->global_eval_fun() ||
      !args[1]->IsString()) {
    return MakePair(*callee, isolate->heap()->the_hole_value());
  }

  CONVERT_LANGUAGE_MODE_ARG(language_mode, 3);
  ASSERT(args[4]->IsSmi());
  return CompileGlobalEval(isolate,
                           args.at<String>(1),
                           args.at<Object>(2),
                           language_mode,
                           args.smi_at(4));
}


static MaybeObject* Allocate(Isolate* isolate,
                             int size,
                             AllocationSpace space) {
  // Allocate a block of memory in the given space (filled with a filler).
  // Use as fallback for allocation in generated code when the space
  // is full.
  SealHandleScope shs(isolate);
  RUNTIME_ASSERT(IsAligned(size, kPointerSize));
  RUNTIME_ASSERT(size > 0);
  Heap* heap = isolate->heap();
  RUNTIME_ASSERT(size <= heap->MaxRegularSpaceAllocationSize());
  Object* allocation;
  { MaybeObject* maybe_allocation;
    if (space == NEW_SPACE) {
      maybe_allocation = heap->new_space()->AllocateRaw(size);
    } else {
      ASSERT(space == OLD_POINTER_SPACE || space == OLD_DATA_SPACE);
      maybe_allocation = heap->paged_space(space)->AllocateRaw(size);
    }
    if (maybe_allocation->ToObject(&allocation)) {
      heap->CreateFillerObjectAt(HeapObject::cast(allocation)->address(), size);
    }
    return maybe_allocation;
  }
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_AllocateInNewSpace) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(Smi, size_smi, 0);
  return Allocate(isolate, size_smi->value(), NEW_SPACE);
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_AllocateInOldPointerSpace) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(Smi, size_smi, 0);
  return Allocate(isolate, size_smi->value(), OLD_POINTER_SPACE);
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_AllocateInOldDataSpace) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(Smi, size_smi, 0);
  return Allocate(isolate, size_smi->value(), OLD_DATA_SPACE);
}


// Push an object unto an array of objects if it is not already in the
// array.  Returns true if the element was pushed on the stack and
// false otherwise.
RUNTIME_FUNCTION(MaybeObject*, Runtime_PushIfAbsent) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_CHECKED(JSArray, array, 0);
  CONVERT_ARG_CHECKED(JSReceiver, element, 1);
  RUNTIME_ASSERT(array->HasFastSmiOrObjectElements());
  int length = Smi::cast(array->length())->value();
  FixedArray* elements = FixedArray::cast(array->elements());
  for (int i = 0; i < length; i++) {
    if (elements->get(i) == element) return isolate->heap()->false_value();
  }
  Object* obj;
  // Strict not needed. Used for cycle detection in Array join implementation.
  { MaybeObject* maybe_obj =
        array->SetFastElement(length, element, kNonStrictMode, true);
    if (!maybe_obj->ToObject(&obj)) return maybe_obj;
  }
  return isolate->heap()->true_value();
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
  ArrayConcatVisitor(Isolate* isolate,
                     Handle<FixedArray> storage,
                     bool fast_elements) :
      isolate_(isolate),
      storage_(Handle<FixedArray>::cast(
          isolate->global_handles()->Create(*storage))),
      index_offset_(0u),
      fast_elements_(fast_elements),
      exceeds_array_limit_(false) { }

  ~ArrayConcatVisitor() {
    clear_storage();
  }

  void visit(uint32_t i, Handle<Object> elm) {
    if (i > JSObject::kMaxElementCount - index_offset_) {
      exceeds_array_limit_ = true;
      return;
    }
    uint32_t index = index_offset_ + i;

    if (fast_elements_) {
      if (index < static_cast<uint32_t>(storage_->length())) {
        storage_->set(index, *elm);
        return;
      }
      // Our initial estimate of length was foiled, possibly by
      // getters on the arrays increasing the length of later arrays
      // during iteration.
      // This shouldn't happen in anything but pathological cases.
      SetDictionaryMode(index);
      // Fall-through to dictionary mode.
    }
    ASSERT(!fast_elements_);
    Handle<SeededNumberDictionary> dict(
        SeededNumberDictionary::cast(*storage_));
    Handle<SeededNumberDictionary> result =
        isolate_->factory()->DictionaryAtNumberPut(dict, index, elm);
    if (!result.is_identical_to(dict)) {
      // Dictionary needed to grow.
      clear_storage();
      set_storage(*result);
    }
  }

  void increase_index_offset(uint32_t delta) {
    if (JSObject::kMaxElementCount - index_offset_ < delta) {
      index_offset_ = JSObject::kMaxElementCount;
    } else {
      index_offset_ += delta;
    }
  }

  bool exceeds_array_limit() {
    return exceeds_array_limit_;
  }

  Handle<JSArray> ToArray() {
    Handle<JSArray> array = isolate_->factory()->NewJSArray(0);
    Handle<Object> length =
        isolate_->factory()->NewNumber(static_cast<double>(index_offset_));
    Handle<Map> map;
    if (fast_elements_) {
      map = isolate_->factory()->GetElementsTransitionMap(array,
                                                          FAST_HOLEY_ELEMENTS);
    } else {
      map = isolate_->factory()->GetElementsTransitionMap(array,
                                                          DICTIONARY_ELEMENTS);
    }
    array->set_map(*map);
    array->set_length(*length);
    array->set_elements(*storage_);
    return array;
  }

 private:
  // Convert storage to dictionary mode.
  void SetDictionaryMode(uint32_t index) {
    ASSERT(fast_elements_);
    Handle<FixedArray> current_storage(*storage_);
    Handle<SeededNumberDictionary> slow_storage(
        isolate_->factory()->NewSeededNumberDictionary(
            current_storage->length()));
    uint32_t current_length = static_cast<uint32_t>(current_storage->length());
    for (uint32_t i = 0; i < current_length; i++) {
      HandleScope loop_scope(isolate_);
      Handle<Object> element(current_storage->get(i), isolate_);
      if (!element->IsTheHole()) {
        Handle<SeededNumberDictionary> new_storage =
          isolate_->factory()->DictionaryAtNumberPut(slow_storage, i, element);
        if (!new_storage.is_identical_to(slow_storage)) {
          slow_storage = loop_scope.CloseAndEscape(new_storage);
        }
      }
    }
    clear_storage();
    set_storage(*slow_storage);
    fast_elements_ = false;
  }

  inline void clear_storage() {
    isolate_->global_handles()->Destroy(
        Handle<Object>::cast(storage_).location());
  }

  inline void set_storage(FixedArray* storage) {
    storage_ = Handle<FixedArray>::cast(
        isolate_->global_handles()->Create(storage));
  }

  Isolate* isolate_;
  Handle<FixedArray> storage_;  // Always a global handle.
  // Index after last seen index. Always less than or equal to
  // JSObject::kMaxElementCount.
  uint32_t index_offset_;
  bool fast_elements_ : 1;
  bool exceeds_array_limit_ : 1;
};


static uint32_t EstimateElementCount(Handle<JSArray> array) {
  uint32_t length = static_cast<uint32_t>(array->length()->Number());
  int element_count = 0;
  switch (array->GetElementsKind()) {
    case FAST_SMI_ELEMENTS:
    case FAST_HOLEY_SMI_ELEMENTS:
    case FAST_ELEMENTS:
    case FAST_HOLEY_ELEMENTS: {
      // Fast elements can't have lengths that are not representable by
      // a 32-bit signed integer.
      ASSERT(static_cast<int32_t>(FixedArray::kMaxLength) >= 0);
      int fast_length = static_cast<int>(length);
      Handle<FixedArray> elements(FixedArray::cast(array->elements()));
      for (int i = 0; i < fast_length; i++) {
        if (!elements->get(i)->IsTheHole()) element_count++;
      }
      break;
    }
    case FAST_DOUBLE_ELEMENTS:
    case FAST_HOLEY_DOUBLE_ELEMENTS: {
      // Fast elements can't have lengths that are not representable by
      // a 32-bit signed integer.
      ASSERT(static_cast<int32_t>(FixedDoubleArray::kMaxLength) >= 0);
      int fast_length = static_cast<int>(length);
      if (array->elements()->IsFixedArray()) {
        ASSERT(FixedArray::cast(array->elements())->length() == 0);
        break;
      }
      Handle<FixedDoubleArray> elements(
          FixedDoubleArray::cast(array->elements()));
      for (int i = 0; i < fast_length; i++) {
        if (!elements->is_the_hole(i)) element_count++;
      }
      break;
    }
    case DICTIONARY_ELEMENTS: {
      Handle<SeededNumberDictionary> dictionary(
          SeededNumberDictionary::cast(array->elements()));
      int capacity = dictionary->Capacity();
      for (int i = 0; i < capacity; i++) {
        Handle<Object> key(dictionary->KeyAt(i), array->GetIsolate());
        if (dictionary->IsKey(*key)) {
          element_count++;
        }
      }
      break;
    }
    case NON_STRICT_ARGUMENTS_ELEMENTS:
    case EXTERNAL_BYTE_ELEMENTS:
    case EXTERNAL_UNSIGNED_BYTE_ELEMENTS:
    case EXTERNAL_SHORT_ELEMENTS:
    case EXTERNAL_UNSIGNED_SHORT_ELEMENTS:
    case EXTERNAL_INT_ELEMENTS:
    case EXTERNAL_UNSIGNED_INT_ELEMENTS:
    case EXTERNAL_FLOAT_ELEMENTS:
    case EXTERNAL_DOUBLE_ELEMENTS:
    case EXTERNAL_PIXEL_ELEMENTS:
      // External arrays are always dense.
      return length;
  }
  // As an estimate, we assume that the prototype doesn't contain any
  // inherited elements.
  return element_count;
}



template<class ExternalArrayClass, class ElementType>
static void IterateExternalArrayElements(Isolate* isolate,
                                         Handle<JSObject> receiver,
                                         bool elements_are_ints,
                                         bool elements_are_guaranteed_smis,
                                         ArrayConcatVisitor* visitor) {
  Handle<ExternalArrayClass> array(
      ExternalArrayClass::cast(receiver->elements()));
  uint32_t len = static_cast<uint32_t>(array->length());

  ASSERT(visitor != NULL);
  if (elements_are_ints) {
    if (elements_are_guaranteed_smis) {
      for (uint32_t j = 0; j < len; j++) {
        HandleScope loop_scope(isolate);
        Handle<Smi> e(Smi::FromInt(static_cast<int>(array->get_scalar(j))),
                      isolate);
        visitor->visit(j, e);
      }
    } else {
      for (uint32_t j = 0; j < len; j++) {
        HandleScope loop_scope(isolate);
        int64_t val = static_cast<int64_t>(array->get_scalar(j));
        if (Smi::IsValid(static_cast<intptr_t>(val))) {
          Handle<Smi> e(Smi::FromInt(static_cast<int>(val)), isolate);
          visitor->visit(j, e);
        } else {
          Handle<Object> e =
              isolate->factory()->NewNumber(static_cast<ElementType>(val));
          visitor->visit(j, e);
        }
      }
    }
  } else {
    for (uint32_t j = 0; j < len; j++) {
      HandleScope loop_scope(isolate);
      Handle<Object> e = isolate->factory()->NewNumber(array->get_scalar(j));
      visitor->visit(j, e);
    }
  }
}


// Used for sorting indices in a List<uint32_t>.
static int compareUInt32(const uint32_t* ap, const uint32_t* bp) {
  uint32_t a = *ap;
  uint32_t b = *bp;
  return (a == b) ? 0 : (a < b) ? -1 : 1;
}


static void CollectElementIndices(Handle<JSObject> object,
                                  uint32_t range,
                                  List<uint32_t>* indices) {
  Isolate* isolate = object->GetIsolate();
  ElementsKind kind = object->GetElementsKind();
  switch (kind) {
    case FAST_SMI_ELEMENTS:
    case FAST_ELEMENTS:
    case FAST_HOLEY_SMI_ELEMENTS:
    case FAST_HOLEY_ELEMENTS: {
      Handle<FixedArray> elements(FixedArray::cast(object->elements()));
      uint32_t length = static_cast<uint32_t>(elements->length());
      if (range < length) length = range;
      for (uint32_t i = 0; i < length; i++) {
        if (!elements->get(i)->IsTheHole()) {
          indices->Add(i);
        }
      }
      break;
    }
    case FAST_HOLEY_DOUBLE_ELEMENTS:
    case FAST_DOUBLE_ELEMENTS: {
      // TODO(1810): Decide if it's worthwhile to implement this.
      UNREACHABLE();
      break;
    }
    case DICTIONARY_ELEMENTS: {
      Handle<SeededNumberDictionary> dict(
          SeededNumberDictionary::cast(object->elements()));
      uint32_t capacity = dict->Capacity();
      for (uint32_t j = 0; j < capacity; j++) {
        HandleScope loop_scope(isolate);
        Handle<Object> k(dict->KeyAt(j), isolate);
        if (dict->IsKey(*k)) {
          ASSERT(k->IsNumber());
          uint32_t index = static_cast<uint32_t>(k->Number());
          if (index < range) {
            indices->Add(index);
          }
        }
      }
      break;
    }
    default: {
      int dense_elements_length;
      switch (kind) {
        case EXTERNAL_PIXEL_ELEMENTS: {
          dense_elements_length =
              ExternalPixelArray::cast(object->elements())->length();
          break;
        }
        case EXTERNAL_BYTE_ELEMENTS: {
          dense_elements_length =
              ExternalByteArray::cast(object->elements())->length();
          break;
        }
        case EXTERNAL_UNSIGNED_BYTE_ELEMENTS: {
          dense_elements_length =
              ExternalUnsignedByteArray::cast(object->elements())->length();
          break;
        }
        case EXTERNAL_SHORT_ELEMENTS: {
          dense_elements_length =
              ExternalShortArray::cast(object->elements())->length();
          break;
        }
        case EXTERNAL_UNSIGNED_SHORT_ELEMENTS: {
          dense_elements_length =
              ExternalUnsignedShortArray::cast(object->elements())->length();
          break;
        }
        case EXTERNAL_INT_ELEMENTS: {
          dense_elements_length =
              ExternalIntArray::cast(object->elements())->length();
          break;
        }
        case EXTERNAL_UNSIGNED_INT_ELEMENTS: {
          dense_elements_length =
              ExternalUnsignedIntArray::cast(object->elements())->length();
          break;
        }
        case EXTERNAL_FLOAT_ELEMENTS: {
          dense_elements_length =
              ExternalFloatArray::cast(object->elements())->length();
          break;
        }
        case EXTERNAL_DOUBLE_ELEMENTS: {
          dense_elements_length =
              ExternalDoubleArray::cast(object->elements())->length();
          break;
        }
        default:
          UNREACHABLE();
          dense_elements_length = 0;
          break;
      }
      uint32_t length = static_cast<uint32_t>(dense_elements_length);
      if (range <= length) {
        length = range;
        // We will add all indices, so we might as well clear it first
        // and avoid duplicates.
        indices->Clear();
      }
      for (uint32_t i = 0; i < length; i++) {
        indices->Add(i);
      }
      if (length == range) return;  // All indices accounted for already.
      break;
    }
  }

  Handle<Object> prototype(object->GetPrototype(), isolate);
  if (prototype->IsJSObject()) {
    // The prototype will usually have no inherited element indices,
    // but we have to check.
    CollectElementIndices(Handle<JSObject>::cast(prototype), range, indices);
  }
}


/**
 * A helper function that visits elements of a JSArray in numerical
 * order.
 *
 * The visitor argument called for each existing element in the array
 * with the element index and the element's value.
 * Afterwards it increments the base-index of the visitor by the array
 * length.
 * Returns false if any access threw an exception, otherwise true.
 */
static bool IterateElements(Isolate* isolate,
                            Handle<JSArray> receiver,
                            ArrayConcatVisitor* visitor) {
  uint32_t length = static_cast<uint32_t>(receiver->length()->Number());
  switch (receiver->GetElementsKind()) {
    case FAST_SMI_ELEMENTS:
    case FAST_ELEMENTS:
    case FAST_HOLEY_SMI_ELEMENTS:
    case FAST_HOLEY_ELEMENTS: {
      // Run through the elements FixedArray and use HasElement and GetElement
      // to check the prototype for missing elements.
      Handle<FixedArray> elements(FixedArray::cast(receiver->elements()));
      int fast_length = static_cast<int>(length);
      ASSERT(fast_length <= elements->length());
      for (int j = 0; j < fast_length; j++) {
        HandleScope loop_scope(isolate);
        Handle<Object> element_value(elements->get(j), isolate);
        if (!element_value->IsTheHole()) {
          visitor->visit(j, element_value);
        } else if (receiver->HasElement(j)) {
          // Call GetElement on receiver, not its prototype, or getters won't
          // have the correct receiver.
          element_value = Object::GetElement(receiver, j);
          RETURN_IF_EMPTY_HANDLE_VALUE(isolate, element_value, false);
          visitor->visit(j, element_value);
        }
      }
      break;
    }
    case FAST_HOLEY_DOUBLE_ELEMENTS:
    case FAST_DOUBLE_ELEMENTS: {
      // Run through the elements FixedArray and use HasElement and GetElement
      // to check the prototype for missing elements.
      Handle<FixedDoubleArray> elements(
          FixedDoubleArray::cast(receiver->elements()));
      int fast_length = static_cast<int>(length);
      ASSERT(fast_length <= elements->length());
      for (int j = 0; j < fast_length; j++) {
        HandleScope loop_scope(isolate);
        if (!elements->is_the_hole(j)) {
          double double_value = elements->get_scalar(j);
          Handle<Object> element_value =
              isolate->factory()->NewNumber(double_value);
          visitor->visit(j, element_value);
        } else if (receiver->HasElement(j)) {
          // Call GetElement on receiver, not its prototype, or getters won't
          // have the correct receiver.
          Handle<Object> element_value = Object::GetElement(receiver, j);
          RETURN_IF_EMPTY_HANDLE_VALUE(isolate, element_value, false);
          visitor->visit(j, element_value);
        }
      }
      break;
    }
    case DICTIONARY_ELEMENTS: {
      Handle<SeededNumberDictionary> dict(receiver->element_dictionary());
      List<uint32_t> indices(dict->Capacity() / 2);
      // Collect all indices in the object and the prototypes less
      // than length. This might introduce duplicates in the indices list.
      CollectElementIndices(receiver, length, &indices);
      indices.Sort(&compareUInt32);
      int j = 0;
      int n = indices.length();
      while (j < n) {
        HandleScope loop_scope(isolate);
        uint32_t index = indices[j];
        Handle<Object> element = Object::GetElement(receiver, index);
        RETURN_IF_EMPTY_HANDLE_VALUE(isolate, element, false);
        visitor->visit(index, element);
        // Skip to next different index (i.e., omit duplicates).
        do {
          j++;
        } while (j < n && indices[j] == index);
      }
      break;
    }
    case EXTERNAL_PIXEL_ELEMENTS: {
      Handle<ExternalPixelArray> pixels(ExternalPixelArray::cast(
          receiver->elements()));
      for (uint32_t j = 0; j < length; j++) {
        Handle<Smi> e(Smi::FromInt(pixels->get_scalar(j)), isolate);
        visitor->visit(j, e);
      }
      break;
    }
    case EXTERNAL_BYTE_ELEMENTS: {
      IterateExternalArrayElements<ExternalByteArray, int8_t>(
          isolate, receiver, true, true, visitor);
      break;
    }
    case EXTERNAL_UNSIGNED_BYTE_ELEMENTS: {
      IterateExternalArrayElements<ExternalUnsignedByteArray, uint8_t>(
          isolate, receiver, true, true, visitor);
      break;
    }
    case EXTERNAL_SHORT_ELEMENTS: {
      IterateExternalArrayElements<ExternalShortArray, int16_t>(
          isolate, receiver, true, true, visitor);
      break;
    }
    case EXTERNAL_UNSIGNED_SHORT_ELEMENTS: {
      IterateExternalArrayElements<ExternalUnsignedShortArray, uint16_t>(
          isolate, receiver, true, true, visitor);
      break;
    }
    case EXTERNAL_INT_ELEMENTS: {
      IterateExternalArrayElements<ExternalIntArray, int32_t>(
          isolate, receiver, true, false, visitor);
      break;
    }
    case EXTERNAL_UNSIGNED_INT_ELEMENTS: {
      IterateExternalArrayElements<ExternalUnsignedIntArray, uint32_t>(
          isolate, receiver, true, false, visitor);
      break;
    }
    case EXTERNAL_FLOAT_ELEMENTS: {
      IterateExternalArrayElements<ExternalFloatArray, float>(
          isolate, receiver, false, false, visitor);
      break;
    }
    case EXTERNAL_DOUBLE_ELEMENTS: {
      IterateExternalArrayElements<ExternalDoubleArray, double>(
          isolate, receiver, false, false, visitor);
      break;
    }
    default:
      UNREACHABLE();
      break;
  }
  visitor->increase_index_offset(length);
  return true;
}


/**
 * Array::concat implementation.
 * See ECMAScript 262, 15.4.4.4.
 * TODO(581): Fix non-compliance for very large concatenations and update to
 * following the ECMAScript 5 specification.
 */
RUNTIME_FUNCTION(MaybeObject*, Runtime_ArrayConcat) {
  HandleScope handle_scope(isolate);
  ASSERT(args.length() == 1);

  CONVERT_ARG_HANDLE_CHECKED(JSArray, arguments, 0);
  int argument_count = static_cast<int>(arguments->length()->Number());
  RUNTIME_ASSERT(arguments->HasFastObjectElements());
  Handle<FixedArray> elements(FixedArray::cast(arguments->elements()));

  // Pass 1: estimate the length and number of elements of the result.
  // The actual length can be larger if any of the arguments have getters
  // that mutate other arguments (but will otherwise be precise).
  // The number of elements is precise if there are no inherited elements.

  ElementsKind kind = FAST_SMI_ELEMENTS;

  uint32_t estimate_result_length = 0;
  uint32_t estimate_nof_elements = 0;
  for (int i = 0; i < argument_count; i++) {
    HandleScope loop_scope(isolate);
    Handle<Object> obj(elements->get(i), isolate);
    uint32_t length_estimate;
    uint32_t element_estimate;
    if (obj->IsJSArray()) {
      Handle<JSArray> array(Handle<JSArray>::cast(obj));
      length_estimate = static_cast<uint32_t>(array->length()->Number());
      if (length_estimate != 0) {
        ElementsKind array_kind =
            GetPackedElementsKind(array->map()->elements_kind());
        if (IsMoreGeneralElementsKindTransition(kind, array_kind)) {
          kind = array_kind;
        }
      }
      element_estimate = EstimateElementCount(array);
    } else {
      if (obj->IsHeapObject()) {
        if (obj->IsNumber()) {
          if (IsMoreGeneralElementsKindTransition(kind, FAST_DOUBLE_ELEMENTS)) {
            kind = FAST_DOUBLE_ELEMENTS;
          }
        } else if (IsMoreGeneralElementsKindTransition(kind, FAST_ELEMENTS)) {
          kind = FAST_ELEMENTS;
        }
      }
      length_estimate = 1;
      element_estimate = 1;
    }
    // Avoid overflows by capping at kMaxElementCount.
    if (JSObject::kMaxElementCount - estimate_result_length <
        length_estimate) {
      estimate_result_length = JSObject::kMaxElementCount;
    } else {
      estimate_result_length += length_estimate;
    }
    if (JSObject::kMaxElementCount - estimate_nof_elements <
        element_estimate) {
      estimate_nof_elements = JSObject::kMaxElementCount;
    } else {
      estimate_nof_elements += element_estimate;
    }
  }

  // If estimated number of elements is more than half of length, a
  // fixed array (fast case) is more time and space-efficient than a
  // dictionary.
  bool fast_case = (estimate_nof_elements * 2) >= estimate_result_length;

  Handle<FixedArray> storage;
  if (fast_case) {
    if (kind == FAST_DOUBLE_ELEMENTS) {
      Handle<FixedDoubleArray> double_storage =
          isolate->factory()->NewFixedDoubleArray(estimate_result_length);
      int j = 0;
      bool failure = false;
      for (int i = 0; i < argument_count; i++) {
        Handle<Object> obj(elements->get(i), isolate);
        if (obj->IsSmi()) {
          double_storage->set(j, Smi::cast(*obj)->value());
          j++;
        } else if (obj->IsNumber()) {
          double_storage->set(j, obj->Number());
          j++;
        } else {
          JSArray* array = JSArray::cast(*obj);
          uint32_t length = static_cast<uint32_t>(array->length()->Number());
          switch (array->map()->elements_kind()) {
            case FAST_HOLEY_DOUBLE_ELEMENTS:
            case FAST_DOUBLE_ELEMENTS: {
              // Empty fixed array indicates that there are no elements.
              if (array->elements()->IsFixedArray()) break;
              FixedDoubleArray* elements =
                  FixedDoubleArray::cast(array->elements());
              for (uint32_t i = 0; i < length; i++) {
                if (elements->is_the_hole(i)) {
                  failure = true;
                  break;
                }
                double double_value = elements->get_scalar(i);
                double_storage->set(j, double_value);
                j++;
              }
              break;
            }
            case FAST_HOLEY_SMI_ELEMENTS:
            case FAST_SMI_ELEMENTS: {
              FixedArray* elements(
                  FixedArray::cast(array->elements()));
              for (uint32_t i = 0; i < length; i++) {
                Object* element = elements->get(i);
                if (element->IsTheHole()) {
                  failure = true;
                  break;
                }
                int32_t int_value = Smi::cast(element)->value();
                double_storage->set(j, int_value);
                j++;
              }
              break;
            }
            case FAST_HOLEY_ELEMENTS:
              ASSERT_EQ(0, length);
              break;
            default:
              UNREACHABLE();
          }
        }
        if (failure) break;
      }
      Handle<JSArray> array = isolate->factory()->NewJSArray(0);
      Smi* length = Smi::FromInt(j);
      Handle<Map> map;
      map = isolate->factory()->GetElementsTransitionMap(array, kind);
      array->set_map(*map);
      array->set_length(length);
      array->set_elements(*double_storage);
      return *array;
    }
    // The backing storage array must have non-existing elements to preserve
    // holes across concat operations.
    storage = isolate->factory()->NewFixedArrayWithHoles(
        estimate_result_length);
  } else {
    // TODO(126): move 25% pre-allocation logic into Dictionary::Allocate
    uint32_t at_least_space_for = estimate_nof_elements +
                                  (estimate_nof_elements >> 2);
    storage = Handle<FixedArray>::cast(
        isolate->factory()->NewSeededNumberDictionary(at_least_space_for));
  }

  ArrayConcatVisitor visitor(isolate, storage, fast_case);

  for (int i = 0; i < argument_count; i++) {
    Handle<Object> obj(elements->get(i), isolate);
    if (obj->IsJSArray()) {
      Handle<JSArray> array = Handle<JSArray>::cast(obj);
      if (!IterateElements(isolate, array, &visitor)) {
        return Failure::Exception();
      }
    } else {
      visitor.visit(0, obj);
      visitor.increase_index_offset(1);
    }
  }

  if (visitor.exceeds_array_limit()) {
    return isolate->Throw(
        *isolate->factory()->NewRangeError("invalid_array_length",
                                           HandleVector<Object>(NULL, 0)));
  }
  return *visitor.ToArray();
}


// This will not allocate (flatten the string), but it may run
// very slowly for very deeply nested ConsStrings.  For debugging use only.
RUNTIME_FUNCTION(MaybeObject*, Runtime_GlobalPrint) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);

  CONVERT_ARG_CHECKED(String, string, 0);
  ConsStringIteratorOp op;
  StringCharacterStream stream(string, &op);
  while (stream.HasMore()) {
    uint16_t character = stream.GetNext();
    PrintF("%c", character);
  }
  return string;
}

// Moves all own elements of an object, that are below a limit, to positions
// starting at zero. All undefined values are placed after non-undefined values,
// and are followed by non-existing element. Does not change the length
// property.
// Returns the number of non-undefined elements collected.
RUNTIME_FUNCTION(MaybeObject*, Runtime_RemoveArrayHoles) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_CHECKED(JSObject, object, 0);
  CONVERT_NUMBER_CHECKED(uint32_t, limit, Uint32, args[1]);
  return object->PrepareElementsForSort(limit);
}


// Move contents of argument 0 (an array) to argument 1 (an array)
RUNTIME_FUNCTION(MaybeObject*, Runtime_MoveArrayContents) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_CHECKED(JSArray, from, 0);
  CONVERT_ARG_CHECKED(JSArray, to, 1);
  from->ValidateElements();
  to->ValidateElements();
  FixedArrayBase* new_elements = from->elements();
  ElementsKind from_kind = from->GetElementsKind();
  MaybeObject* maybe_new_map;
  maybe_new_map = to->GetElementsTransitionMap(isolate, from_kind);
  Object* new_map;
  if (!maybe_new_map->ToObject(&new_map)) return maybe_new_map;
  to->set_map_and_elements(Map::cast(new_map), new_elements);
  to->set_length(from->length());
  Object* obj;
  { MaybeObject* maybe_obj = from->ResetElements();
    if (!maybe_obj->ToObject(&obj)) return maybe_obj;
  }
  from->set_length(Smi::FromInt(0));
  to->ValidateElements();
  return to;
}


// How many elements does this object/array have?
RUNTIME_FUNCTION(MaybeObject*, Runtime_EstimateNumberOfElements) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_CHECKED(JSObject, object, 0);
  HeapObject* elements = object->elements();
  if (elements->IsDictionary()) {
    int result = SeededNumberDictionary::cast(elements)->NumberOfElements();
    return Smi::FromInt(result);
  } else if (object->IsJSArray()) {
    return JSArray::cast(object)->length();
  } else {
    return Smi::FromInt(FixedArray::cast(elements)->length());
  }
}


// Returns an array that tells you where in the [0, length) interval an array
// might have elements.  Can either return an array of keys (positive integers
// or undefined) or a number representing the positive length of an interval
// starting at index 0.
// Intervals can span over some keys that are not in the object.
RUNTIME_FUNCTION(MaybeObject*, Runtime_GetArrayKeys) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, array, 0);
  CONVERT_NUMBER_CHECKED(uint32_t, length, Uint32, args[1]);
  if (array->elements()->IsDictionary()) {
    Handle<FixedArray> keys = isolate->factory()->empty_fixed_array();
    for (Handle<Object> p = array;
         !p->IsNull();
         p = Handle<Object>(p->GetPrototype(isolate), isolate)) {
      if (p->IsJSProxy() || JSObject::cast(*p)->HasIndexedInterceptor()) {
        // Bail out if we find a proxy or interceptor, likely not worth
        // collecting keys in that case.
        return *isolate->factory()->NewNumberFromUint(length);
      }
      Handle<JSObject> current = Handle<JSObject>::cast(p);
      Handle<FixedArray> current_keys =
          isolate->factory()->NewFixedArray(
              current->NumberOfLocalElements(NONE));
      current->GetLocalElementKeys(*current_keys, NONE);
      keys = UnionOfKeys(keys, current_keys);
    }
    // Erase any keys >= length.
    // TODO(adamk): Remove this step when the contract of %GetArrayKeys
    // is changed to let this happen on the JS side.
    for (int i = 0; i < keys->length(); i++) {
      if (NumberToUint32(keys->get(i)) >= length) keys->set_undefined(i);
    }
    return *isolate->factory()->NewJSArrayWithElements(keys);
  } else {
    ASSERT(array->HasFastSmiOrObjectElements() ||
           array->HasFastDoubleElements());
    uint32_t actual_length = static_cast<uint32_t>(array->elements()->length());
    return *isolate->factory()->NewNumberFromUint(Min(actual_length, length));
  }
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_LookupAccessor) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 3);
  CONVERT_ARG_CHECKED(JSReceiver, receiver, 0);
  CONVERT_ARG_CHECKED(Name, name, 1);
  CONVERT_SMI_ARG_CHECKED(flag, 2);
  AccessorComponent component = flag == 0 ? ACCESSOR_GETTER : ACCESSOR_SETTER;
  if (!receiver->IsJSObject()) return isolate->heap()->undefined_value();
  return JSObject::cast(receiver)->LookupAccessor(name, component);
}


#ifdef ENABLE_DEBUGGER_SUPPORT
RUNTIME_FUNCTION(MaybeObject*, Runtime_DebugBreak) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 0);
  return Execution::DebugBreakHelper();
}


// Helper functions for wrapping and unwrapping stack frame ids.
static Smi* WrapFrameId(StackFrame::Id id) {
  ASSERT(IsAligned(OffsetFrom(id), static_cast<intptr_t>(4)));
  return Smi::FromInt(id >> 2);
}


static StackFrame::Id UnwrapFrameId(int wrapped) {
  return static_cast<StackFrame::Id>(wrapped << 2);
}


// Adds a JavaScript function as a debug event listener.
// args[0]: debug event listener function to set or null or undefined for
//          clearing the event listener function
// args[1]: object supplied during callback
RUNTIME_FUNCTION(MaybeObject*, Runtime_SetDebugEventListener) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 2);
  RUNTIME_ASSERT(args[0]->IsJSFunction() ||
                 args[0]->IsUndefined() ||
                 args[0]->IsNull());
  Handle<Object> callback = args.at<Object>(0);
  Handle<Object> data = args.at<Object>(1);
  isolate->debugger()->SetEventListener(callback, data);

  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_Break) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 0);
  isolate->stack_guard()->DebugBreak();
  return isolate->heap()->undefined_value();
}


static MaybeObject* DebugLookupResultValue(Heap* heap,
                                           Object* receiver,
                                           Name* name,
                                           LookupResult* result,
                                           bool* caught_exception) {
  Object* value;
  switch (result->type()) {
    case NORMAL:
      value = result->holder()->GetNormalizedProperty(result);
      if (value->IsTheHole()) {
        return heap->undefined_value();
      }
      return value;
    case FIELD: {
      Object* value;
      MaybeObject* maybe_value =
          JSObject::cast(result->holder())->FastPropertyAt(
              result->representation(),
              result->GetFieldIndex().field_index());
      if (!maybe_value->To(&value)) return maybe_value;
      if (value->IsTheHole()) {
        return heap->undefined_value();
      }
      return value;
    }
    case CONSTANT_FUNCTION:
      return result->GetConstantFunction();
    case CALLBACKS: {
      Object* structure = result->GetCallbackObject();
      if (structure->IsForeign() || structure->IsAccessorInfo()) {
        MaybeObject* maybe_value = result->holder()->GetPropertyWithCallback(
            receiver, structure, name);
        if (!maybe_value->ToObject(&value)) {
          if (maybe_value->IsRetryAfterGC()) return maybe_value;
          ASSERT(maybe_value->IsException());
          maybe_value = heap->isolate()->pending_exception();
          heap->isolate()->clear_pending_exception();
          if (caught_exception != NULL) {
            *caught_exception = true;
          }
          return maybe_value;
        }
        return value;
      } else {
        return heap->undefined_value();
      }
    }
    case INTERCEPTOR:
    case TRANSITION:
      return heap->undefined_value();
    case HANDLER:
    case NONEXISTENT:
      UNREACHABLE();
      return heap->undefined_value();
  }
  UNREACHABLE();  // keep the compiler happy
  return heap->undefined_value();
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
RUNTIME_FUNCTION(MaybeObject*, Runtime_DebugGetPropertyDetails) {
  HandleScope scope(isolate);

  ASSERT(args.length() == 2);

  CONVERT_ARG_HANDLE_CHECKED(JSObject, obj, 0);
  CONVERT_ARG_HANDLE_CHECKED(Name, name, 1);

  // Make sure to set the current context to the context before the debugger was
  // entered (if the debugger is entered). The reason for switching context here
  // is that for some property lookups (accessors and interceptors) callbacks
  // into the embedding application can occour, and the embedding application
  // could have the assumption that its own native context is the current
  // context and not some internal debugger context.
  SaveContext save(isolate);
  if (isolate->debug()->InDebugger()) {
    isolate->set_context(*isolate->debug()->debugger_entry()->GetContext());
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
    Handle<FixedArray> details = isolate->factory()->NewFixedArray(2);
    Object* element_or_char;
    { MaybeObject* maybe_element_or_char =
          Runtime::GetElementOrCharAt(isolate, obj, index);
      if (!maybe_element_or_char->ToObject(&element_or_char)) {
        return maybe_element_or_char;
      }
    }
    details->set(0, element_or_char);
    details->set(
        1, PropertyDetails(NONE, NORMAL, Representation::None()).AsSmi());
    return *isolate->factory()->NewJSArrayWithElements(details);
  }

  // Find the number of objects making up this.
  int length = LocalPrototypeChainLength(*obj);

  // Try local lookup on each of the objects.
  Handle<JSObject> jsproto = obj;
  for (int i = 0; i < length; i++) {
    LookupResult result(isolate);
    jsproto->LocalLookup(*name, &result);
    if (result.IsFound()) {
      // LookupResult is not GC safe as it holds raw object pointers.
      // GC can happen later in this code so put the required fields into
      // local variables using handles when required for later use.
      Handle<Object> result_callback_obj;
      if (result.IsPropertyCallbacks()) {
        result_callback_obj = Handle<Object>(result.GetCallbackObject(),
                                             isolate);
      }
      Smi* property_details = result.GetPropertyDetails().AsSmi();
      // DebugLookupResultValue can cause GC so details from LookupResult needs
      // to be copied to handles before this.
      bool caught_exception = false;
      Object* raw_value;
      { MaybeObject* maybe_raw_value =
            DebugLookupResultValue(isolate->heap(), *obj, *name,
                                   &result, &caught_exception);
        if (!maybe_raw_value->ToObject(&raw_value)) return maybe_raw_value;
      }
      Handle<Object> value(raw_value, isolate);

      // If the callback object is a fixed array then it contains JavaScript
      // getter and/or setter.
      bool hasJavaScriptAccessors = result.IsPropertyCallbacks() &&
                                    result_callback_obj->IsAccessorPair();
      Handle<FixedArray> details =
          isolate->factory()->NewFixedArray(hasJavaScriptAccessors ? 5 : 2);
      details->set(0, *value);
      details->set(1, property_details);
      if (hasJavaScriptAccessors) {
        AccessorPair* accessors = AccessorPair::cast(*result_callback_obj);
        details->set(2, isolate->heap()->ToBoolean(caught_exception));
        details->set(3, accessors->GetComponent(ACCESSOR_GETTER));
        details->set(4, accessors->GetComponent(ACCESSOR_SETTER));
      }

      return *isolate->factory()->NewJSArrayWithElements(details);
    }
    if (i < length - 1) {
      jsproto = Handle<JSObject>(JSObject::cast(jsproto->GetPrototype()));
    }
  }

  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_DebugGetProperty) {
  HandleScope scope(isolate);

  ASSERT(args.length() == 2);

  CONVERT_ARG_HANDLE_CHECKED(JSObject, obj, 0);
  CONVERT_ARG_HANDLE_CHECKED(Name, name, 1);

  LookupResult result(isolate);
  obj->Lookup(*name, &result);
  if (result.IsFound()) {
    return DebugLookupResultValue(isolate->heap(), *obj, *name, &result, NULL);
  }
  return isolate->heap()->undefined_value();
}


// Return the property type calculated from the property details.
// args[0]: smi with property details.
RUNTIME_FUNCTION(MaybeObject*, Runtime_DebugPropertyTypeFromDetails) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  CONVERT_PROPERTY_DETAILS_CHECKED(details, 0);
  return Smi::FromInt(static_cast<int>(details.type()));
}


// Return the property attribute calculated from the property details.
// args[0]: smi with property details.
RUNTIME_FUNCTION(MaybeObject*, Runtime_DebugPropertyAttributesFromDetails) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  CONVERT_PROPERTY_DETAILS_CHECKED(details, 0);
  return Smi::FromInt(static_cast<int>(details.attributes()));
}


// Return the property insertion index calculated from the property details.
// args[0]: smi with property details.
RUNTIME_FUNCTION(MaybeObject*, Runtime_DebugPropertyIndexFromDetails) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  CONVERT_PROPERTY_DETAILS_CHECKED(details, 0);
  // TODO(verwaest): Depends on the type of details.
  return Smi::FromInt(details.dictionary_index());
}


// Return property value from named interceptor.
// args[0]: object
// args[1]: property name
RUNTIME_FUNCTION(MaybeObject*, Runtime_DebugNamedInterceptorPropertyValue) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, obj, 0);
  RUNTIME_ASSERT(obj->HasNamedInterceptor());
  CONVERT_ARG_HANDLE_CHECKED(Name, name, 1);

  PropertyAttributes attributes;
  return obj->GetPropertyWithInterceptor(*obj, *name, &attributes);
}


// Return element value from indexed interceptor.
// args[0]: object
// args[1]: index
RUNTIME_FUNCTION(MaybeObject*, Runtime_DebugIndexedInterceptorElementValue) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, obj, 0);
  RUNTIME_ASSERT(obj->HasIndexedInterceptor());
  CONVERT_NUMBER_CHECKED(uint32_t, index, Uint32, args[1]);

  return obj->GetElementWithInterceptor(*obj, index);
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_CheckExecutionState) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() >= 1);
  CONVERT_NUMBER_CHECKED(int, break_id, Int32, args[0]);
  // Check that the break id is valid.
  if (isolate->debug()->break_id() == 0 ||
      break_id != isolate->debug()->break_id()) {
    return isolate->Throw(
        isolate->heap()->illegal_execution_state_string());
  }

  return isolate->heap()->true_value();
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_GetFrameCount) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);

  // Check arguments.
  Object* result;
  { MaybeObject* maybe_result = Runtime_CheckExecutionState(
      RUNTIME_ARGUMENTS(isolate, args));
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }

  // Count all frames which are relevant to debugging stack trace.
  int n = 0;
  StackFrame::Id id = isolate->debug()->break_frame_id();
  if (id == StackFrame::NO_ID) {
    // If there is no JavaScript stack frame count is 0.
    return Smi::FromInt(0);
  }

  for (JavaScriptFrameIterator it(isolate, id); !it.done(); it.Advance()) {
    n += it.frame()->GetInlineCount();
  }
  return Smi::FromInt(n);
}


class FrameInspector {
 public:
  FrameInspector(JavaScriptFrame* frame,
                 int inlined_jsframe_index,
                 Isolate* isolate)
      : frame_(frame), deoptimized_frame_(NULL), isolate_(isolate) {
    // Calculate the deoptimized frame.
    if (frame->is_optimized()) {
      deoptimized_frame_ = Deoptimizer::DebuggerInspectableFrame(
          frame, inlined_jsframe_index, isolate);
    }
    has_adapted_arguments_ = frame_->has_adapted_arguments();
    is_bottommost_ = inlined_jsframe_index == 0;
    is_optimized_ = frame_->is_optimized();
  }

  ~FrameInspector() {
    // Get rid of the calculated deoptimized frame if any.
    if (deoptimized_frame_ != NULL) {
      Deoptimizer::DeleteDebuggerInspectableFrame(deoptimized_frame_,
                                                  isolate_);
    }
  }

  int GetParametersCount() {
    return is_optimized_
        ? deoptimized_frame_->parameters_count()
        : frame_->ComputeParametersCount();
  }
  int expression_count() { return deoptimized_frame_->expression_count(); }
  Object* GetFunction() {
    return is_optimized_
        ? deoptimized_frame_->GetFunction()
        : frame_->function();
  }
  Object* GetParameter(int index) {
    return is_optimized_
        ? deoptimized_frame_->GetParameter(index)
        : frame_->GetParameter(index);
  }
  Object* GetExpression(int index) {
    return is_optimized_
        ? deoptimized_frame_->GetExpression(index)
        : frame_->GetExpression(index);
  }
  int GetSourcePosition() {
    return is_optimized_
        ? deoptimized_frame_->GetSourcePosition()
        : frame_->LookupCode()->SourcePosition(frame_->pc());
  }
  bool IsConstructor() {
    return is_optimized_ && !is_bottommost_
        ? deoptimized_frame_->HasConstructStub()
        : frame_->IsConstructor();
  }

  // To inspect all the provided arguments the frame might need to be
  // replaced with the arguments frame.
  void SetArgumentsFrame(JavaScriptFrame* frame) {
    ASSERT(has_adapted_arguments_);
    frame_ = frame;
    is_optimized_ = frame_->is_optimized();
    ASSERT(!is_optimized_);
  }

 private:
  JavaScriptFrame* frame_;
  DeoptimizedFrameInfo* deoptimized_frame_;
  Isolate* isolate_;
  bool is_optimized_;
  bool is_bottommost_;
  bool has_adapted_arguments_;

  DISALLOW_COPY_AND_ASSIGN(FrameInspector);
};


static const int kFrameDetailsFrameIdIndex = 0;
static const int kFrameDetailsReceiverIndex = 1;
static const int kFrameDetailsFunctionIndex = 2;
static const int kFrameDetailsArgumentCountIndex = 3;
static const int kFrameDetailsLocalCountIndex = 4;
static const int kFrameDetailsSourcePositionIndex = 5;
static const int kFrameDetailsConstructCallIndex = 6;
static const int kFrameDetailsAtReturnIndex = 7;
static const int kFrameDetailsFlagsIndex = 8;
static const int kFrameDetailsFirstDynamicIndex = 9;


static SaveContext* FindSavedContextForFrame(Isolate* isolate,
                                             JavaScriptFrame* frame) {
  SaveContext* save = isolate->save_context();
  while (save != NULL && !save->IsBelowFrame(frame)) {
    save = save->prev();
  }
  ASSERT(save != NULL);
  return save;
}


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
// 8: Flags
// Arguments name, value
// Locals name, value
// Return value if any
RUNTIME_FUNCTION(MaybeObject*, Runtime_GetFrameDetails) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);

  // Check arguments.
  Object* check;
  { MaybeObject* maybe_check = Runtime_CheckExecutionState(
      RUNTIME_ARGUMENTS(isolate, args));
    if (!maybe_check->ToObject(&check)) return maybe_check;
  }
  CONVERT_NUMBER_CHECKED(int, index, Int32, args[1]);
  Heap* heap = isolate->heap();

  // Find the relevant frame with the requested index.
  StackFrame::Id id = isolate->debug()->break_frame_id();
  if (id == StackFrame::NO_ID) {
    // If there are no JavaScript stack frames return undefined.
    return heap->undefined_value();
  }

  int count = 0;
  JavaScriptFrameIterator it(isolate, id);
  for (; !it.done(); it.Advance()) {
    if (index < count + it.frame()->GetInlineCount()) break;
    count += it.frame()->GetInlineCount();
  }
  if (it.done()) return heap->undefined_value();

  bool is_optimized = it.frame()->is_optimized();

  int inlined_jsframe_index = 0;  // Inlined frame index in optimized frame.
  if (is_optimized) {
    inlined_jsframe_index =
        it.frame()->GetInlineCount() - (index - count) - 1;
  }
  FrameInspector frame_inspector(it.frame(), inlined_jsframe_index, isolate);

  // Traverse the saved contexts chain to find the active context for the
  // selected frame.
  SaveContext* save = FindSavedContextForFrame(isolate, it.frame());

  // Get the frame id.
  Handle<Object> frame_id(WrapFrameId(it.frame()->id()), isolate);

  // Find source position in unoptimized code.
  int position = frame_inspector.GetSourcePosition();

  // Check for constructor frame.
  bool constructor = frame_inspector.IsConstructor();

  // Get scope info and read from it for local variable information.
  Handle<JSFunction> function(JSFunction::cast(frame_inspector.GetFunction()));
  Handle<SharedFunctionInfo> shared(function->shared());
  Handle<ScopeInfo> scope_info(shared->scope_info());
  ASSERT(*scope_info != ScopeInfo::Empty(isolate));

  // Get the locals names and values into a temporary array.
  //
  // TODO(1240907): Hide compiler-introduced stack variables
  // (e.g. .result)?  For users of the debugger, they will probably be
  // confusing.
  Handle<FixedArray> locals =
      isolate->factory()->NewFixedArray(scope_info->LocalCount() * 2);

  // Fill in the values of the locals.
  int i = 0;
  for (; i < scope_info->StackLocalCount(); ++i) {
    // Use the value from the stack.
    locals->set(i * 2, scope_info->LocalName(i));
    locals->set(i * 2 + 1, frame_inspector.GetExpression(i));
  }
  if (i < scope_info->LocalCount()) {
    // Get the context containing declarations.
    Handle<Context> context(
        Context::cast(it.frame()->context())->declaration_context());
    for (; i < scope_info->LocalCount(); ++i) {
      Handle<String> name(scope_info->LocalName(i));
      VariableMode mode;
      InitializationFlag init_flag;
      locals->set(i * 2, *name);
      locals->set(i * 2 + 1, context->get(
          scope_info->ContextSlotIndex(*name, &mode, &init_flag)));
    }
  }

  // Check whether this frame is positioned at return. If not top
  // frame or if the frame is optimized it cannot be at a return.
  bool at_return = false;
  if (!is_optimized && index == 0) {
    at_return = isolate->debug()->IsBreakAtReturn(it.frame());
  }

  // If positioned just before return find the value to be returned and add it
  // to the frame information.
  Handle<Object> return_value = isolate->factory()->undefined_value();
  if (at_return) {
    StackFrameIterator it2(isolate);
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
                  Handle<Object>(Memory::Object_at(internal_frame_sp),
                                 isolate);
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
  if ((inlined_jsframe_index == 0) && it.frame()->has_adapted_arguments()) {
    it.AdvanceToArgumentsFrame();
    frame_inspector.SetArgumentsFrame(it.frame());
  }

  // Find the number of arguments to fill. At least fill the number of
  // parameters for the function and fill more if more parameters are provided.
  int argument_count = scope_info->ParameterCount();
  if (argument_count < frame_inspector.GetParametersCount()) {
    argument_count = frame_inspector.GetParametersCount();
  }

  // Calculate the size of the result.
  int details_size = kFrameDetailsFirstDynamicIndex +
                     2 * (argument_count + scope_info->LocalCount()) +
                     (at_return ? 1 : 0);
  Handle<FixedArray> details = isolate->factory()->NewFixedArray(details_size);

  // Add the frame id.
  details->set(kFrameDetailsFrameIdIndex, *frame_id);

  // Add the function (same as in function frame).
  details->set(kFrameDetailsFunctionIndex, frame_inspector.GetFunction());

  // Add the arguments count.
  details->set(kFrameDetailsArgumentCountIndex, Smi::FromInt(argument_count));

  // Add the locals count
  details->set(kFrameDetailsLocalCountIndex,
               Smi::FromInt(scope_info->LocalCount()));

  // Add the source position.
  if (position != RelocInfo::kNoPosition) {
    details->set(kFrameDetailsSourcePositionIndex, Smi::FromInt(position));
  } else {
    details->set(kFrameDetailsSourcePositionIndex, heap->undefined_value());
  }

  // Add the constructor information.
  details->set(kFrameDetailsConstructCallIndex, heap->ToBoolean(constructor));

  // Add the at return information.
  details->set(kFrameDetailsAtReturnIndex, heap->ToBoolean(at_return));

  // Add flags to indicate information on whether this frame is
  //   bit 0: invoked in the debugger context.
  //   bit 1: optimized frame.
  //   bit 2: inlined in optimized frame
  int flags = 0;
  if (*save->context() == *isolate->debug()->debug_context()) {
    flags |= 1 << 0;
  }
  if (is_optimized) {
    flags |= 1 << 1;
    flags |= inlined_jsframe_index << 2;
  }
  details->set(kFrameDetailsFlagsIndex, Smi::FromInt(flags));

  // Fill the dynamic part.
  int details_index = kFrameDetailsFirstDynamicIndex;

  // Add arguments name and value.
  for (int i = 0; i < argument_count; i++) {
    // Name of the argument.
    if (i < scope_info->ParameterCount()) {
      details->set(details_index++, scope_info->ParameterName(i));
    } else {
      details->set(details_index++, heap->undefined_value());
    }

    // Parameter value.
    if (i < frame_inspector.GetParametersCount()) {
      // Get the value from the stack.
      details->set(details_index++, frame_inspector.GetParameter(i));
    } else {
      details->set(details_index++, heap->undefined_value());
    }
  }

  // Add locals name and value from the temporary copy from the function frame.
  for (int i = 0; i < scope_info->LocalCount() * 2; i++) {
    details->set(details_index++, locals->get(i));
  }

  // Add the value being returned.
  if (at_return) {
    details->set(details_index++, *return_value);
  }

  // Add the receiver (same as in function frame).
  // THIS MUST BE DONE LAST SINCE WE MIGHT ADVANCE
  // THE FRAME ITERATOR TO WRAP THE RECEIVER.
  Handle<Object> receiver(it.frame()->receiver(), isolate);
  if (!receiver->IsJSObject() &&
      shared->is_classic_mode() &&
      !shared->native()) {
    // If the receiver is not a JSObject and the function is not a
    // builtin or strict-mode we have hit an optimization where a
    // value object is not converted into a wrapped JS objects. To
    // hide this optimization from the debugger, we wrap the receiver
    // by creating correct wrapper object based on the calling frame's
    // native context.
    it.Advance();
    Handle<Context> calling_frames_native_context(
        Context::cast(Context::cast(it.frame()->context())->native_context()));
    receiver =
        isolate->factory()->ToObject(receiver, calling_frames_native_context);
  }
  details->set(kFrameDetailsReceiverIndex, *receiver);

  ASSERT_EQ(details_size, details_index);
  return *isolate->factory()->NewJSArrayWithElements(details);
}


// Create a plain JSObject which materializes the local scope for the specified
// frame.
static Handle<JSObject> MaterializeLocalScopeWithFrameInspector(
    Isolate* isolate,
    JavaScriptFrame* frame,
    FrameInspector* frame_inspector) {
  Handle<JSFunction> function(JSFunction::cast(frame_inspector->GetFunction()));
  Handle<SharedFunctionInfo> shared(function->shared());
  Handle<ScopeInfo> scope_info(shared->scope_info());

  // Allocate and initialize a JSObject with all the arguments, stack locals
  // heap locals and extension properties of the debugged function.
  Handle<JSObject> local_scope =
      isolate->factory()->NewJSObject(isolate->object_function());

  // First fill all parameters.
  for (int i = 0; i < scope_info->ParameterCount(); ++i) {
    Handle<Object> value(i < frame_inspector->GetParametersCount()
                             ? frame_inspector->GetParameter(i)
                             : isolate->heap()->undefined_value(),
                         isolate);

    RETURN_IF_EMPTY_HANDLE_VALUE(
        isolate,
        SetProperty(isolate,
                    local_scope,
                    Handle<String>(scope_info->ParameterName(i)),
                    value,
                    NONE,
                    kNonStrictMode),
        Handle<JSObject>());
  }

  // Second fill all stack locals.
  for (int i = 0; i < scope_info->StackLocalCount(); ++i) {
    RETURN_IF_EMPTY_HANDLE_VALUE(
        isolate,
        SetProperty(isolate,
                    local_scope,
                    Handle<String>(scope_info->StackLocalName(i)),
                    Handle<Object>(frame_inspector->GetExpression(i), isolate),
                    NONE,
                    kNonStrictMode),
        Handle<JSObject>());
  }

  if (scope_info->HasContext()) {
    // Third fill all context locals.
    Handle<Context> frame_context(Context::cast(frame->context()));
    Handle<Context> function_context(frame_context->declaration_context());
    if (!scope_info->CopyContextLocalsToScopeObject(
            isolate, function_context, local_scope)) {
      return Handle<JSObject>();
    }

    // Finally copy any properties from the function context extension.
    // These will be variables introduced by eval.
    if (function_context->closure() == *function) {
      if (function_context->has_extension() &&
          !function_context->IsNativeContext()) {
        Handle<JSObject> ext(JSObject::cast(function_context->extension()));
        bool threw = false;
        Handle<FixedArray> keys =
            GetKeysInFixedArrayFor(ext, INCLUDE_PROTOS, &threw);
        if (threw) return Handle<JSObject>();

        for (int i = 0; i < keys->length(); i++) {
          // Names of variables introduced by eval are strings.
          ASSERT(keys->get(i)->IsString());
          Handle<String> key(String::cast(keys->get(i)));
          RETURN_IF_EMPTY_HANDLE_VALUE(
              isolate,
              SetProperty(isolate,
                          local_scope,
                          key,
                          GetProperty(isolate, ext, key),
                          NONE,
                          kNonStrictMode),
              Handle<JSObject>());
        }
      }
    }
  }

  return local_scope;
}


static Handle<JSObject> MaterializeLocalScope(
    Isolate* isolate,
    JavaScriptFrame* frame,
    int inlined_jsframe_index) {
  FrameInspector frame_inspector(frame, inlined_jsframe_index, isolate);
  return MaterializeLocalScopeWithFrameInspector(isolate,
                                                 frame,
                                                 &frame_inspector);
}


// Set the context local variable value.
static bool SetContextLocalValue(Isolate* isolate,
                                 Handle<ScopeInfo> scope_info,
                                 Handle<Context> context,
                                 Handle<String> variable_name,
                                 Handle<Object> new_value) {
  for (int i = 0; i < scope_info->ContextLocalCount(); i++) {
    Handle<String> next_name(scope_info->ContextLocalName(i));
    if (variable_name->Equals(*next_name)) {
      VariableMode mode;
      InitializationFlag init_flag;
      int context_index =
          scope_info->ContextSlotIndex(*next_name, &mode, &init_flag);
      context->set(context_index, *new_value);
      return true;
    }
  }

  return false;
}


static bool SetLocalVariableValue(Isolate* isolate,
                                  JavaScriptFrame* frame,
                                  int inlined_jsframe_index,
                                  Handle<String> variable_name,
                                  Handle<Object> new_value) {
  if (inlined_jsframe_index != 0 || frame->is_optimized()) {
    // Optimized frames are not supported.
    return false;
  }

  Handle<JSFunction> function(JSFunction::cast(frame->function()));
  Handle<SharedFunctionInfo> shared(function->shared());
  Handle<ScopeInfo> scope_info(shared->scope_info());

  bool default_result = false;

  // Parameters.
  for (int i = 0; i < scope_info->ParameterCount(); ++i) {
    if (scope_info->ParameterName(i)->Equals(*variable_name)) {
      frame->SetParameterValue(i, *new_value);
      // Argument might be shadowed in heap context, don't stop here.
      default_result = true;
    }
  }

  // Stack locals.
  for (int i = 0; i < scope_info->StackLocalCount(); ++i) {
    if (scope_info->StackLocalName(i)->Equals(*variable_name)) {
      frame->SetExpression(i, *new_value);
      return true;
    }
  }

  if (scope_info->HasContext()) {
    // Context locals.
    Handle<Context> frame_context(Context::cast(frame->context()));
    Handle<Context> function_context(frame_context->declaration_context());
    if (SetContextLocalValue(
        isolate, scope_info, function_context, variable_name, new_value)) {
      return true;
    }

    // Function context extension. These are variables introduced by eval.
    if (function_context->closure() == *function) {
      if (function_context->has_extension() &&
          !function_context->IsNativeContext()) {
        Handle<JSObject> ext(JSObject::cast(function_context->extension()));

        if (ext->HasProperty(*variable_name)) {
          // We don't expect this to do anything except replacing
          // property value.
          SetProperty(isolate,
                      ext,
                      variable_name,
                      new_value,
                      NONE,
                      kNonStrictMode);
          return true;
        }
      }
    }
  }

  return default_result;
}


// Create a plain JSObject which materializes the closure content for the
// context.
static Handle<JSObject> MaterializeClosure(Isolate* isolate,
                                           Handle<Context> context) {
  ASSERT(context->IsFunctionContext());

  Handle<SharedFunctionInfo> shared(context->closure()->shared());
  Handle<ScopeInfo> scope_info(shared->scope_info());

  // Allocate and initialize a JSObject with all the content of this function
  // closure.
  Handle<JSObject> closure_scope =
      isolate->factory()->NewJSObject(isolate->object_function());

  // Fill all context locals to the context extension.
  if (!scope_info->CopyContextLocalsToScopeObject(
          isolate, context, closure_scope)) {
    return Handle<JSObject>();
  }

  // Finally copy any properties from the function context extension. This will
  // be variables introduced by eval.
  if (context->has_extension()) {
    Handle<JSObject> ext(JSObject::cast(context->extension()));
    bool threw = false;
    Handle<FixedArray> keys =
        GetKeysInFixedArrayFor(ext, INCLUDE_PROTOS, &threw);
    if (threw) return Handle<JSObject>();

    for (int i = 0; i < keys->length(); i++) {
      // Names of variables introduced by eval are strings.
      ASSERT(keys->get(i)->IsString());
      Handle<String> key(String::cast(keys->get(i)));
       RETURN_IF_EMPTY_HANDLE_VALUE(
          isolate,
          SetProperty(isolate,
                      closure_scope,
                      key,
                      GetProperty(isolate, ext, key),
                      NONE,
                      kNonStrictMode),
          Handle<JSObject>());
    }
  }

  return closure_scope;
}


// This method copies structure of MaterializeClosure method above.
static bool SetClosureVariableValue(Isolate* isolate,
                                    Handle<Context> context,
                                    Handle<String> variable_name,
                                    Handle<Object> new_value) {
  ASSERT(context->IsFunctionContext());

  Handle<SharedFunctionInfo> shared(context->closure()->shared());
  Handle<ScopeInfo> scope_info(shared->scope_info());

  // Context locals to the context extension.
  if (SetContextLocalValue(
          isolate, scope_info, context, variable_name, new_value)) {
    return true;
  }

  // Properties from the function context extension. This will
  // be variables introduced by eval.
  if (context->has_extension()) {
    Handle<JSObject> ext(JSObject::cast(context->extension()));
    if (ext->HasProperty(*variable_name)) {
      // We don't expect this to do anything except replacing property value.
      SetProperty(isolate,
                  ext,
                  variable_name,
                  new_value,
                  NONE,
                  kNonStrictMode);
      return true;
    }
  }

  return false;
}


// Create a plain JSObject which materializes the scope for the specified
// catch context.
static Handle<JSObject> MaterializeCatchScope(Isolate* isolate,
                                              Handle<Context> context) {
  ASSERT(context->IsCatchContext());
  Handle<String> name(String::cast(context->extension()));
  Handle<Object> thrown_object(context->get(Context::THROWN_OBJECT_INDEX),
                               isolate);
  Handle<JSObject> catch_scope =
      isolate->factory()->NewJSObject(isolate->object_function());
  RETURN_IF_EMPTY_HANDLE_VALUE(
      isolate,
      SetProperty(isolate,
                  catch_scope,
                  name,
                  thrown_object,
                  NONE,
                  kNonStrictMode),
      Handle<JSObject>());
  return catch_scope;
}


static bool SetCatchVariableValue(Isolate* isolate,
                                  Handle<Context> context,
                                  Handle<String> variable_name,
                                  Handle<Object> new_value) {
  ASSERT(context->IsCatchContext());
  Handle<String> name(String::cast(context->extension()));
  if (!name->Equals(*variable_name)) {
    return false;
  }
  context->set(Context::THROWN_OBJECT_INDEX, *new_value);
  return true;
}


// Create a plain JSObject which materializes the block scope for the specified
// block context.
static Handle<JSObject> MaterializeBlockScope(
    Isolate* isolate,
    Handle<Context> context) {
  ASSERT(context->IsBlockContext());
  Handle<ScopeInfo> scope_info(ScopeInfo::cast(context->extension()));

  // Allocate and initialize a JSObject with all the arguments, stack locals
  // heap locals and extension properties of the debugged function.
  Handle<JSObject> block_scope =
      isolate->factory()->NewJSObject(isolate->object_function());

  // Fill all context locals.
  if (!scope_info->CopyContextLocalsToScopeObject(
          isolate, context, block_scope)) {
    return Handle<JSObject>();
  }

  return block_scope;
}


// Create a plain JSObject which materializes the module scope for the specified
// module context.
static Handle<JSObject> MaterializeModuleScope(
    Isolate* isolate,
    Handle<Context> context) {
  ASSERT(context->IsModuleContext());
  Handle<ScopeInfo> scope_info(ScopeInfo::cast(context->extension()));

  // Allocate and initialize a JSObject with all the members of the debugged
  // module.
  Handle<JSObject> module_scope =
      isolate->factory()->NewJSObject(isolate->object_function());

  // Fill all context locals.
  if (!scope_info->CopyContextLocalsToScopeObject(
          isolate, context, module_scope)) {
    return Handle<JSObject>();
  }

  return module_scope;
}


// Iterate over the actual scopes visible from a stack frame or from a closure.
// The iteration proceeds from the innermost visible nested scope outwards.
// All scopes are backed by an actual context except the local scope,
// which is inserted "artificially" in the context chain.
class ScopeIterator {
 public:
  enum ScopeType {
    ScopeTypeGlobal = 0,
    ScopeTypeLocal,
    ScopeTypeWith,
    ScopeTypeClosure,
    ScopeTypeCatch,
    ScopeTypeBlock,
    ScopeTypeModule
  };

  ScopeIterator(Isolate* isolate,
                JavaScriptFrame* frame,
                int inlined_jsframe_index)
    : isolate_(isolate),
      frame_(frame),
      inlined_jsframe_index_(inlined_jsframe_index),
      function_(JSFunction::cast(frame->function())),
      context_(Context::cast(frame->context())),
      nested_scope_chain_(4),
      failed_(false) {

    // Catch the case when the debugger stops in an internal function.
    Handle<SharedFunctionInfo> shared_info(function_->shared());
    Handle<ScopeInfo> scope_info(shared_info->scope_info());
    if (shared_info->script() == isolate->heap()->undefined_value()) {
      while (context_->closure() == *function_) {
        context_ = Handle<Context>(context_->previous(), isolate_);
      }
      return;
    }

    // Get the debug info (create it if it does not exist).
    if (!isolate->debug()->EnsureDebugInfo(shared_info, function_)) {
      // Return if ensuring debug info failed.
      return;
    }
    Handle<DebugInfo> debug_info = Debug::GetDebugInfo(shared_info);

    // Find the break point where execution has stopped.
    BreakLocationIterator break_location_iterator(debug_info,
                                                  ALL_BREAK_LOCATIONS);
    // pc points to the instruction after the current one, possibly a break
    // location as well. So the "- 1" to exclude it from the search.
    break_location_iterator.FindBreakLocationFromAddress(frame->pc() - 1);
    if (break_location_iterator.IsExit()) {
      // We are within the return sequence. At the momemt it is not possible to
      // get a source position which is consistent with the current scope chain.
      // Thus all nested with, catch and block contexts are skipped and we only
      // provide the function scope.
      if (scope_info->HasContext()) {
        context_ = Handle<Context>(context_->declaration_context(), isolate_);
      } else {
        while (context_->closure() == *function_) {
          context_ = Handle<Context>(context_->previous(), isolate_);
        }
      }
      if (scope_info->scope_type() != EVAL_SCOPE) {
        nested_scope_chain_.Add(scope_info);
      }
    } else {
      // Reparse the code and analyze the scopes.
      Handle<Script> script(Script::cast(shared_info->script()));
      Scope* scope = NULL;

      // Check whether we are in global, eval or function code.
      Handle<ScopeInfo> scope_info(shared_info->scope_info());
      if (scope_info->scope_type() != FUNCTION_SCOPE) {
        // Global or eval code.
        CompilationInfoWithZone info(script);
        if (scope_info->scope_type() == GLOBAL_SCOPE) {
          info.MarkAsGlobal();
        } else {
          ASSERT(scope_info->scope_type() == EVAL_SCOPE);
          info.MarkAsEval();
          info.SetContext(Handle<Context>(function_->context()));
        }
        if (Parser::Parse(&info) && Scope::Analyze(&info)) {
          scope = info.function()->scope();
        }
        RetrieveScopeChain(scope, shared_info);
      } else {
        // Function code
        CompilationInfoWithZone info(shared_info);
        if (Parser::Parse(&info) && Scope::Analyze(&info)) {
          scope = info.function()->scope();
        }
        RetrieveScopeChain(scope, shared_info);
      }
    }
  }

  ScopeIterator(Isolate* isolate,
                Handle<JSFunction> function)
    : isolate_(isolate),
      frame_(NULL),
      inlined_jsframe_index_(0),
      function_(function),
      context_(function->context()),
      failed_(false) {
    if (function->IsBuiltin()) {
      context_ = Handle<Context>();
    }
  }

  // More scopes?
  bool Done() {
    ASSERT(!failed_);
    return context_.is_null();
  }

  bool Failed() { return failed_; }

  // Move to the next scope.
  void Next() {
    ASSERT(!failed_);
    ScopeType scope_type = Type();
    if (scope_type == ScopeTypeGlobal) {
      // The global scope is always the last in the chain.
      ASSERT(context_->IsNativeContext());
      context_ = Handle<Context>();
      return;
    }
    if (nested_scope_chain_.is_empty()) {
      context_ = Handle<Context>(context_->previous(), isolate_);
    } else {
      if (nested_scope_chain_.last()->HasContext()) {
        ASSERT(context_->previous() != NULL);
        context_ = Handle<Context>(context_->previous(), isolate_);
      }
      nested_scope_chain_.RemoveLast();
    }
  }

  // Return the type of the current scope.
  ScopeType Type() {
    ASSERT(!failed_);
    if (!nested_scope_chain_.is_empty()) {
      Handle<ScopeInfo> scope_info = nested_scope_chain_.last();
      switch (scope_info->scope_type()) {
        case FUNCTION_SCOPE:
          ASSERT(context_->IsFunctionContext() ||
                 !scope_info->HasContext());
          return ScopeTypeLocal;
        case MODULE_SCOPE:
          ASSERT(context_->IsModuleContext());
          return ScopeTypeModule;
        case GLOBAL_SCOPE:
          ASSERT(context_->IsNativeContext());
          return ScopeTypeGlobal;
        case WITH_SCOPE:
          ASSERT(context_->IsWithContext());
          return ScopeTypeWith;
        case CATCH_SCOPE:
          ASSERT(context_->IsCatchContext());
          return ScopeTypeCatch;
        case BLOCK_SCOPE:
          ASSERT(!scope_info->HasContext() ||
                 context_->IsBlockContext());
          return ScopeTypeBlock;
        case EVAL_SCOPE:
          UNREACHABLE();
      }
    }
    if (context_->IsNativeContext()) {
      ASSERT(context_->global_object()->IsGlobalObject());
      return ScopeTypeGlobal;
    }
    if (context_->IsFunctionContext()) {
      return ScopeTypeClosure;
    }
    if (context_->IsCatchContext()) {
      return ScopeTypeCatch;
    }
    if (context_->IsBlockContext()) {
      return ScopeTypeBlock;
    }
    if (context_->IsModuleContext()) {
      return ScopeTypeModule;
    }
    ASSERT(context_->IsWithContext());
    return ScopeTypeWith;
  }

  // Return the JavaScript object with the content of the current scope.
  Handle<JSObject> ScopeObject() {
    ASSERT(!failed_);
    switch (Type()) {
      case ScopeIterator::ScopeTypeGlobal:
        return Handle<JSObject>(CurrentContext()->global_object());
      case ScopeIterator::ScopeTypeLocal:
        // Materialize the content of the local scope into a JSObject.
        ASSERT(nested_scope_chain_.length() == 1);
        return MaterializeLocalScope(isolate_, frame_, inlined_jsframe_index_);
      case ScopeIterator::ScopeTypeWith:
        // Return the with object.
        return Handle<JSObject>(JSObject::cast(CurrentContext()->extension()));
      case ScopeIterator::ScopeTypeCatch:
        return MaterializeCatchScope(isolate_, CurrentContext());
      case ScopeIterator::ScopeTypeClosure:
        // Materialize the content of the closure scope into a JSObject.
        return MaterializeClosure(isolate_, CurrentContext());
      case ScopeIterator::ScopeTypeBlock:
        return MaterializeBlockScope(isolate_, CurrentContext());
      case ScopeIterator::ScopeTypeModule:
        return MaterializeModuleScope(isolate_, CurrentContext());
    }
    UNREACHABLE();
    return Handle<JSObject>();
  }

  bool SetVariableValue(Handle<String> variable_name,
                        Handle<Object> new_value) {
    ASSERT(!failed_);
    switch (Type()) {
      case ScopeIterator::ScopeTypeGlobal:
        break;
      case ScopeIterator::ScopeTypeLocal:
        return SetLocalVariableValue(isolate_, frame_, inlined_jsframe_index_,
            variable_name, new_value);
      case ScopeIterator::ScopeTypeWith:
        break;
      case ScopeIterator::ScopeTypeCatch:
        return SetCatchVariableValue(isolate_, CurrentContext(),
            variable_name, new_value);
      case ScopeIterator::ScopeTypeClosure:
        return SetClosureVariableValue(isolate_, CurrentContext(),
            variable_name, new_value);
      case ScopeIterator::ScopeTypeBlock:
        // TODO(2399): should we implement it?
        break;
      case ScopeIterator::ScopeTypeModule:
        // TODO(2399): should we implement it?
        break;
    }
    return false;
  }

  Handle<ScopeInfo> CurrentScopeInfo() {
    ASSERT(!failed_);
    if (!nested_scope_chain_.is_empty()) {
      return nested_scope_chain_.last();
    } else if (context_->IsBlockContext()) {
      return Handle<ScopeInfo>(ScopeInfo::cast(context_->extension()));
    } else if (context_->IsFunctionContext()) {
      return Handle<ScopeInfo>(context_->closure()->shared()->scope_info());
    }
    return Handle<ScopeInfo>::null();
  }

  // Return the context for this scope. For the local context there might not
  // be an actual context.
  Handle<Context> CurrentContext() {
    ASSERT(!failed_);
    if (Type() == ScopeTypeGlobal ||
        nested_scope_chain_.is_empty()) {
      return context_;
    } else if (nested_scope_chain_.last()->HasContext()) {
      return context_;
    } else {
      return Handle<Context>();
    }
  }

#ifdef DEBUG
  // Debug print of the content of the current scope.
  void DebugPrint() {
    ASSERT(!failed_);
    switch (Type()) {
      case ScopeIterator::ScopeTypeGlobal:
        PrintF("Global:\n");
        CurrentContext()->Print();
        break;

      case ScopeIterator::ScopeTypeLocal: {
        PrintF("Local:\n");
        function_->shared()->scope_info()->Print();
        if (!CurrentContext().is_null()) {
          CurrentContext()->Print();
          if (CurrentContext()->has_extension()) {
            Handle<Object> extension(CurrentContext()->extension(), isolate_);
            if (extension->IsJSContextExtensionObject()) {
              extension->Print();
            }
          }
        }
        break;
      }

      case ScopeIterator::ScopeTypeWith:
        PrintF("With:\n");
        CurrentContext()->extension()->Print();
        break;

      case ScopeIterator::ScopeTypeCatch:
        PrintF("Catch:\n");
        CurrentContext()->extension()->Print();
        CurrentContext()->get(Context::THROWN_OBJECT_INDEX)->Print();
        break;

      case ScopeIterator::ScopeTypeClosure:
        PrintF("Closure:\n");
        CurrentContext()->Print();
        if (CurrentContext()->has_extension()) {
          Handle<Object> extension(CurrentContext()->extension(), isolate_);
          if (extension->IsJSContextExtensionObject()) {
            extension->Print();
          }
        }
        break;

      default:
        UNREACHABLE();
    }
    PrintF("\n");
  }
#endif

 private:
  Isolate* isolate_;
  JavaScriptFrame* frame_;
  int inlined_jsframe_index_;
  Handle<JSFunction> function_;
  Handle<Context> context_;
  List<Handle<ScopeInfo> > nested_scope_chain_;
  bool failed_;

  void RetrieveScopeChain(Scope* scope,
                          Handle<SharedFunctionInfo> shared_info) {
    if (scope != NULL) {
      int source_position = shared_info->code()->SourcePosition(frame_->pc());
      scope->GetNestedScopeChain(&nested_scope_chain_, source_position);
    } else {
      // A failed reparse indicates that the preparser has diverged from the
      // parser or that the preparse data given to the initial parse has been
      // faulty. We fail in debug mode but in release mode we only provide the
      // information we get from the context chain but nothing about
      // completely stack allocated scopes or stack allocated locals.
      // Or it could be due to stack overflow.
      ASSERT(isolate_->has_pending_exception());
      failed_ = true;
    }
  }

  DISALLOW_IMPLICIT_CONSTRUCTORS(ScopeIterator);
};


RUNTIME_FUNCTION(MaybeObject*, Runtime_GetScopeCount) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);

  // Check arguments.
  Object* check;
  { MaybeObject* maybe_check = Runtime_CheckExecutionState(
      RUNTIME_ARGUMENTS(isolate, args));
    if (!maybe_check->ToObject(&check)) return maybe_check;
  }
  CONVERT_SMI_ARG_CHECKED(wrapped_id, 1);

  // Get the frame where the debugging is performed.
  StackFrame::Id id = UnwrapFrameId(wrapped_id);
  JavaScriptFrameIterator it(isolate, id);
  JavaScriptFrame* frame = it.frame();

  // Count the visible scopes.
  int n = 0;
  for (ScopeIterator it(isolate, frame, 0);
       !it.Done();
       it.Next()) {
    n++;
  }

  return Smi::FromInt(n);
}


// Returns the list of step-in positions (text offset) in a function of the
// stack frame in a range from the current debug break position to the end
// of the corresponding statement.
RUNTIME_FUNCTION(MaybeObject*, Runtime_GetStepInPositions) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);

  // Check arguments.
  Object* check;
  { MaybeObject* maybe_check = Runtime_CheckExecutionState(
      RUNTIME_ARGUMENTS(isolate, args));
    if (!maybe_check->ToObject(&check)) return maybe_check;
  }
  CONVERT_SMI_ARG_CHECKED(wrapped_id, 1);

  // Get the frame where the debugging is performed.
  StackFrame::Id id = UnwrapFrameId(wrapped_id);
  JavaScriptFrameIterator frame_it(isolate, id);
  JavaScriptFrame* frame = frame_it.frame();

  Handle<SharedFunctionInfo> shared =
      Handle<SharedFunctionInfo>(JSFunction::cast(frame->function())->shared());
  Handle<DebugInfo> debug_info = Debug::GetDebugInfo(shared);

  int len = 0;
  Handle<JSArray> array(isolate->factory()->NewJSArray(10));
  // Find the break point where execution has stopped.
  BreakLocationIterator break_location_iterator(debug_info,
                                                ALL_BREAK_LOCATIONS);

  break_location_iterator.FindBreakLocationFromAddress(frame->pc());
  int current_statement_pos = break_location_iterator.statement_position();

  while (!break_location_iterator.Done()) {
    if (break_location_iterator.IsStepInLocation(isolate)) {
      Smi* position_value = Smi::FromInt(break_location_iterator.position());
      JSObject::SetElement(array, len,
          Handle<Object>(position_value, isolate),
          NONE, kNonStrictMode);
      len++;
    }
    // Advance iterator.
    break_location_iterator.Next();
    if (current_statement_pos !=
        break_location_iterator.statement_position()) {
      break;
    }
  }
  return *array;
}


static const int kScopeDetailsTypeIndex = 0;
static const int kScopeDetailsObjectIndex = 1;
static const int kScopeDetailsSize = 2;


static MaybeObject* MaterializeScopeDetails(Isolate* isolate,
    ScopeIterator* it) {
  // Calculate the size of the result.
  int details_size = kScopeDetailsSize;
  Handle<FixedArray> details = isolate->factory()->NewFixedArray(details_size);

  // Fill in scope details.
  details->set(kScopeDetailsTypeIndex, Smi::FromInt(it->Type()));
  Handle<JSObject> scope_object = it->ScopeObject();
  RETURN_IF_EMPTY_HANDLE(isolate, scope_object);
  details->set(kScopeDetailsObjectIndex, *scope_object);

  return *isolate->factory()->NewJSArrayWithElements(details);
}

// Return an array with scope details
// args[0]: number: break id
// args[1]: number: frame index
// args[2]: number: inlined frame index
// args[3]: number: scope index
//
// The array returned contains the following information:
// 0: Scope type
// 1: Scope object
RUNTIME_FUNCTION(MaybeObject*, Runtime_GetScopeDetails) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 4);

  // Check arguments.
  Object* check;
  { MaybeObject* maybe_check = Runtime_CheckExecutionState(
      RUNTIME_ARGUMENTS(isolate, args));
    if (!maybe_check->ToObject(&check)) return maybe_check;
  }
  CONVERT_SMI_ARG_CHECKED(wrapped_id, 1);
  CONVERT_NUMBER_CHECKED(int, inlined_jsframe_index, Int32, args[2]);
  CONVERT_NUMBER_CHECKED(int, index, Int32, args[3]);

  // Get the frame where the debugging is performed.
  StackFrame::Id id = UnwrapFrameId(wrapped_id);
  JavaScriptFrameIterator frame_it(isolate, id);
  JavaScriptFrame* frame = frame_it.frame();

  // Find the requested scope.
  int n = 0;
  ScopeIterator it(isolate, frame, inlined_jsframe_index);
  for (; !it.Done() && n < index; it.Next()) {
    n++;
  }
  if (it.Done()) {
    return isolate->heap()->undefined_value();
  }
  return MaterializeScopeDetails(isolate, &it);
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_GetFunctionScopeCount) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);

  // Check arguments.
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, fun, 0);

  // Count the visible scopes.
  int n = 0;
  for (ScopeIterator it(isolate, fun); !it.Done(); it.Next()) {
    n++;
  }

  return Smi::FromInt(n);
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_GetFunctionScopeDetails) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);

  // Check arguments.
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, fun, 0);
  CONVERT_NUMBER_CHECKED(int, index, Int32, args[1]);

  // Find the requested scope.
  int n = 0;
  ScopeIterator it(isolate, fun);
  for (; !it.Done() && n < index; it.Next()) {
    n++;
  }
  if (it.Done()) {
    return isolate->heap()->undefined_value();
  }

  return MaterializeScopeDetails(isolate, &it);
}


static bool SetScopeVariableValue(ScopeIterator* it, int index,
                                  Handle<String> variable_name,
                                  Handle<Object> new_value) {
  for (int n = 0; !it->Done() && n < index; it->Next()) {
    n++;
  }
  if (it->Done()) {
    return false;
  }
  return it->SetVariableValue(variable_name, new_value);
}


// Change variable value in closure or local scope
// args[0]: number or JsFunction: break id or function
// args[1]: number: frame index (when arg[0] is break id)
// args[2]: number: inlined frame index (when arg[0] is break id)
// args[3]: number: scope index
// args[4]: string: variable name
// args[5]: object: new value
//
// Return true if success and false otherwise
RUNTIME_FUNCTION(MaybeObject*, Runtime_SetScopeVariableValue) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 6);

  // Check arguments.
  CONVERT_NUMBER_CHECKED(int, index, Int32, args[3]);
  CONVERT_ARG_HANDLE_CHECKED(String, variable_name, 4);
  Handle<Object> new_value = args.at<Object>(5);

  bool res;
  if (args[0]->IsNumber()) {
    Object* check;
    { MaybeObject* maybe_check = Runtime_CheckExecutionState(
        RUNTIME_ARGUMENTS(isolate, args));
      if (!maybe_check->ToObject(&check)) return maybe_check;
    }
    CONVERT_SMI_ARG_CHECKED(wrapped_id, 1);
    CONVERT_NUMBER_CHECKED(int, inlined_jsframe_index, Int32, args[2]);

    // Get the frame where the debugging is performed.
    StackFrame::Id id = UnwrapFrameId(wrapped_id);
    JavaScriptFrameIterator frame_it(isolate, id);
    JavaScriptFrame* frame = frame_it.frame();

    ScopeIterator it(isolate, frame, inlined_jsframe_index);
    res = SetScopeVariableValue(&it, index, variable_name, new_value);
  } else {
    CONVERT_ARG_HANDLE_CHECKED(JSFunction, fun, 0);
    ScopeIterator it(isolate, fun);
    res = SetScopeVariableValue(&it, index, variable_name, new_value);
  }

  return isolate->heap()->ToBoolean(res);
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_DebugPrintScopes) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 0);

#ifdef DEBUG
  // Print the scopes for the top frame.
  StackFrameLocator locator(isolate);
  JavaScriptFrame* frame = locator.FindJavaScriptFrame(0);
  for (ScopeIterator it(isolate, frame, 0);
       !it.Done();
       it.Next()) {
    it.DebugPrint();
  }
#endif
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_GetThreadCount) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);

  // Check arguments.
  Object* result;
  { MaybeObject* maybe_result = Runtime_CheckExecutionState(
      RUNTIME_ARGUMENTS(isolate, args));
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }

  // Count all archived V8 threads.
  int n = 0;
  for (ThreadState* thread =
          isolate->thread_manager()->FirstThreadStateInUse();
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
RUNTIME_FUNCTION(MaybeObject*, Runtime_GetThreadDetails) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);

  // Check arguments.
  Object* check;
  { MaybeObject* maybe_check = Runtime_CheckExecutionState(
      RUNTIME_ARGUMENTS(isolate, args));
    if (!maybe_check->ToObject(&check)) return maybe_check;
  }
  CONVERT_NUMBER_CHECKED(int, index, Int32, args[1]);

  // Allocate array for result.
  Handle<FixedArray> details =
      isolate->factory()->NewFixedArray(kThreadDetailsSize);

  // Thread index 0 is current thread.
  if (index == 0) {
    // Fill the details.
    details->set(kThreadDetailsCurrentThreadIndex,
                 isolate->heap()->true_value());
    details->set(kThreadDetailsThreadIdIndex,
                 Smi::FromInt(ThreadId::Current().ToInteger()));
  } else {
    // Find the thread with the requested index.
    int n = 1;
    ThreadState* thread =
        isolate->thread_manager()->FirstThreadStateInUse();
    while (index != n && thread != NULL) {
      thread = thread->Next();
      n++;
    }
    if (thread == NULL) {
      return isolate->heap()->undefined_value();
    }

    // Fill the details.
    details->set(kThreadDetailsCurrentThreadIndex,
                 isolate->heap()->false_value());
    details->set(kThreadDetailsThreadIdIndex,
                 Smi::FromInt(thread->id().ToInteger()));
  }

  // Convert to JS array and return.
  return *isolate->factory()->NewJSArrayWithElements(details);
}


// Sets the disable break state
// args[0]: disable break state
RUNTIME_FUNCTION(MaybeObject*, Runtime_SetDisableBreak) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_BOOLEAN_ARG_CHECKED(disable_break, 0);
  isolate->debug()->set_disable_break(disable_break);
  return  isolate->heap()->undefined_value();
}


static bool IsPositionAlignmentCodeCorrect(int alignment) {
  return alignment == STATEMENT_ALIGNED || alignment == BREAK_POSITION_ALIGNED;
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_GetBreakLocations) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);

  CONVERT_ARG_HANDLE_CHECKED(JSFunction, fun, 0);
  CONVERT_NUMBER_CHECKED(int32_t, statement_aligned_code, Int32, args[1]);

  if (!IsPositionAlignmentCodeCorrect(statement_aligned_code)) {
    return isolate->ThrowIllegalOperation();
  }
  BreakPositionAlignment alignment =
      static_cast<BreakPositionAlignment>(statement_aligned_code);

  Handle<SharedFunctionInfo> shared(fun->shared());
  // Find the number of break points
  Handle<Object> break_locations =
      Debug::GetSourceBreakLocations(shared, alignment);
  if (break_locations->IsUndefined()) return isolate->heap()->undefined_value();
  // Return array as JS array
  return *isolate->factory()->NewJSArrayWithElements(
      Handle<FixedArray>::cast(break_locations));
}


// Set a break point in a function.
// args[0]: function
// args[1]: number: break source position (within the function source)
// args[2]: number: break point object
RUNTIME_FUNCTION(MaybeObject*, Runtime_SetFunctionBreakPoint) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 3);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);
  CONVERT_NUMBER_CHECKED(int32_t, source_position, Int32, args[1]);
  RUNTIME_ASSERT(source_position >= 0);
  Handle<Object> break_point_object_arg = args.at<Object>(2);

  // Set break point.
  isolate->debug()->SetBreakPoint(function, break_point_object_arg,
                                  &source_position);

  return Smi::FromInt(source_position);
}


// Changes the state of a break point in a script and returns source position
// where break point was set. NOTE: Regarding performance see the NOTE for
// GetScriptFromScriptData.
// args[0]: script to set break point in
// args[1]: number: break source position (within the script source)
// args[2]: number, breakpoint position alignment
// args[3]: number: break point object
RUNTIME_FUNCTION(MaybeObject*, Runtime_SetScriptBreakPoint) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 4);
  CONVERT_ARG_HANDLE_CHECKED(JSValue, wrapper, 0);
  CONVERT_NUMBER_CHECKED(int32_t, source_position, Int32, args[1]);
  RUNTIME_ASSERT(source_position >= 0);
  CONVERT_NUMBER_CHECKED(int32_t, statement_aligned_code, Int32, args[2]);
  Handle<Object> break_point_object_arg = args.at<Object>(3);

  if (!IsPositionAlignmentCodeCorrect(statement_aligned_code)) {
    return isolate->ThrowIllegalOperation();
  }
  BreakPositionAlignment alignment =
      static_cast<BreakPositionAlignment>(statement_aligned_code);

  // Get the script from the script wrapper.
  RUNTIME_ASSERT(wrapper->value()->IsScript());
  Handle<Script> script(Script::cast(wrapper->value()));

  // Set break point.
  if (!isolate->debug()->SetBreakPointForScript(script, break_point_object_arg,
                                                &source_position,
                                                alignment)) {
    return  isolate->heap()->undefined_value();
  }

  return Smi::FromInt(source_position);
}


// Clear a break point
// args[0]: number: break point object
RUNTIME_FUNCTION(MaybeObject*, Runtime_ClearBreakPoint) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  Handle<Object> break_point_object_arg = args.at<Object>(0);

  // Clear break point.
  isolate->debug()->ClearBreakPoint(break_point_object_arg);

  return isolate->heap()->undefined_value();
}


// Change the state of break on exceptions.
// args[0]: Enum value indicating whether to affect caught/uncaught exceptions.
// args[1]: Boolean indicating on/off.
RUNTIME_FUNCTION(MaybeObject*, Runtime_ChangeBreakOnException) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  RUNTIME_ASSERT(args[0]->IsNumber());
  CONVERT_BOOLEAN_ARG_CHECKED(enable, 1);

  // If the number doesn't match an enum value, the ChangeBreakOnException
  // function will default to affecting caught exceptions.
  ExceptionBreakType type =
      static_cast<ExceptionBreakType>(NumberToUint32(args[0]));
  // Update break point state.
  isolate->debug()->ChangeBreakOnException(type, enable);
  return isolate->heap()->undefined_value();
}


// Returns the state of break on exceptions
// args[0]: boolean indicating uncaught exceptions
RUNTIME_FUNCTION(MaybeObject*, Runtime_IsBreakOnException) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  RUNTIME_ASSERT(args[0]->IsNumber());

  ExceptionBreakType type =
      static_cast<ExceptionBreakType>(NumberToUint32(args[0]));
  bool result = isolate->debug()->IsBreakOnException(type);
  return Smi::FromInt(result);
}


// Prepare for stepping
// args[0]: break id for checking execution state
// args[1]: step action from the enumeration StepAction
// args[2]: number of times to perform the step, for step out it is the number
//          of frames to step down.
RUNTIME_FUNCTION(MaybeObject*, Runtime_PrepareStep) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 3);
  // Check arguments.
  Object* check;
  { MaybeObject* maybe_check = Runtime_CheckExecutionState(
      RUNTIME_ARGUMENTS(isolate, args));
    if (!maybe_check->ToObject(&check)) return maybe_check;
  }
  if (!args[1]->IsNumber() || !args[2]->IsNumber()) {
    return isolate->Throw(isolate->heap()->illegal_argument_string());
  }

  // Get the step action and check validity.
  StepAction step_action = static_cast<StepAction>(NumberToInt32(args[1]));
  if (step_action != StepIn &&
      step_action != StepNext &&
      step_action != StepOut &&
      step_action != StepInMin &&
      step_action != StepMin) {
    return isolate->Throw(isolate->heap()->illegal_argument_string());
  }

  // Get the number of steps.
  int step_count = NumberToInt32(args[2]);
  if (step_count < 1) {
    return isolate->Throw(isolate->heap()->illegal_argument_string());
  }

  // Clear all current stepping setup.
  isolate->debug()->ClearStepping();

  // Prepare step.
  isolate->debug()->PrepareStep(static_cast<StepAction>(step_action),
                                step_count);
  return isolate->heap()->undefined_value();
}


// Clear all stepping set by PrepareStep.
RUNTIME_FUNCTION(MaybeObject*, Runtime_ClearStepping) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 0);
  isolate->debug()->ClearStepping();
  return isolate->heap()->undefined_value();
}


static bool IsBlockOrCatchOrWithScope(ScopeIterator::ScopeType type) {
  return type == ScopeIterator::ScopeTypeBlock ||
         type == ScopeIterator::ScopeTypeCatch ||
         type == ScopeIterator::ScopeTypeWith;
}


// Creates a copy of the with context chain. The copy of the context chain is
// is linked to the function context supplied.
static Handle<Context> CopyNestedScopeContextChain(Isolate* isolate,
                                                   Handle<JSFunction> function,
                                                   Handle<Context> base,
                                                   JavaScriptFrame* frame,
                                                   int inlined_jsframe_index) {
  HandleScope scope(isolate);
  List<Handle<ScopeInfo> > scope_chain;
  List<Handle<Context> > context_chain;

  ScopeIterator it(isolate, frame, inlined_jsframe_index);
  if (it.Failed()) return Handle<Context>::null();

  for ( ; IsBlockOrCatchOrWithScope(it.Type()); it.Next()) {
    ASSERT(!it.Done());
    scope_chain.Add(it.CurrentScopeInfo());
    context_chain.Add(it.CurrentContext());
  }

  // At the end of the chain. Return the base context to link to.
  Handle<Context> context = base;

  // Iteratively copy and or materialize the nested contexts.
  while (!scope_chain.is_empty()) {
    Handle<ScopeInfo> scope_info = scope_chain.RemoveLast();
    Handle<Context> current = context_chain.RemoveLast();
    ASSERT(!(scope_info->HasContext() & current.is_null()));

    if (scope_info->scope_type() == CATCH_SCOPE) {
      ASSERT(current->IsCatchContext());
      Handle<String> name(String::cast(current->extension()));
      Handle<Object> thrown_object(current->get(Context::THROWN_OBJECT_INDEX),
                                   isolate);
      context =
          isolate->factory()->NewCatchContext(function,
                                              context,
                                              name,
                                              thrown_object);
    } else if (scope_info->scope_type() == BLOCK_SCOPE) {
      // Materialize the contents of the block scope into a JSObject.
      ASSERT(current->IsBlockContext());
      Handle<JSObject> block_scope_object =
          MaterializeBlockScope(isolate, current);
      CHECK(!block_scope_object.is_null());
      // Allocate a new function context for the debug evaluation and set the
      // extension object.
      Handle<Context> new_context =
          isolate->factory()->NewFunctionContext(Context::MIN_CONTEXT_SLOTS,
                                                 function);
      new_context->set_extension(*block_scope_object);
      new_context->set_previous(*context);
      context = new_context;
    } else {
      ASSERT(scope_info->scope_type() == WITH_SCOPE);
      ASSERT(current->IsWithContext());
      Handle<JSObject> extension(JSObject::cast(current->extension()));
      context =
          isolate->factory()->NewWithContext(function, context, extension);
    }
  }

  return scope.CloseAndEscape(context);
}


// Helper function to find or create the arguments object for
// Runtime_DebugEvaluate.
static Handle<Object> GetArgumentsObject(Isolate* isolate,
                                         JavaScriptFrame* frame,
                                         FrameInspector* frame_inspector,
                                         Handle<ScopeInfo> scope_info,
                                         Handle<Context> function_context) {
  // Try to find the value of 'arguments' to pass as parameter. If it is not
  // found (that is the debugged function does not reference 'arguments' and
  // does not support eval) then create an 'arguments' object.
  int index;
  if (scope_info->StackLocalCount() > 0) {
    index = scope_info->StackSlotIndex(isolate->heap()->arguments_string());
    if (index != -1) {
      return Handle<Object>(frame->GetExpression(index), isolate);
    }
  }

  if (scope_info->HasHeapAllocatedLocals()) {
    VariableMode mode;
    InitializationFlag init_flag;
    index = scope_info->ContextSlotIndex(
        isolate->heap()->arguments_string(), &mode, &init_flag);
    if (index != -1) {
      return Handle<Object>(function_context->get(index), isolate);
    }
  }

  // FunctionGetArguments can't return a non-Object.
  return Handle<JSObject>(JSObject::cast(
      Accessors::FunctionGetArguments(frame_inspector->GetFunction(),
                                      NULL)->ToObjectUnchecked()), isolate);
}


// Compile and evaluate source for the given context.
static MaybeObject* DebugEvaluate(Isolate* isolate,
                                  Handle<Context> context,
                                  Handle<Object> context_extension,
                                  Handle<Object> receiver,
                                  Handle<String> source) {
  if (context_extension->IsJSObject()) {
    Handle<JSObject> extension = Handle<JSObject>::cast(context_extension);
    Handle<JSFunction> closure(context->closure(), isolate);
    context = isolate->factory()->NewWithContext(closure, context, extension);
  }

  Handle<SharedFunctionInfo> shared = Compiler::CompileEval(
      source,
      context,
      context->IsNativeContext(),
      CLASSIC_MODE,
      NO_PARSE_RESTRICTION,
      RelocInfo::kNoPosition);
  if (shared.is_null()) return Failure::Exception();

  Handle<JSFunction> eval_fun =
      isolate->factory()->NewFunctionFromSharedFunctionInfo(
          shared, context, NOT_TENURED);
  bool pending_exception;
  Handle<Object> result = Execution::Call(
      eval_fun, receiver, 0, NULL, &pending_exception);

  if (pending_exception) return Failure::Exception();

  // Skip the global proxy as it has no properties and always delegates to the
  // real global object.
  if (result->IsJSGlobalProxy()) {
    result = Handle<JSObject>(JSObject::cast(result->GetPrototype(isolate)));
  }

  // Clear the oneshot breakpoints so that the debugger does not step further.
  isolate->debug()->ClearStepping();
  return *result;
}


// Evaluate a piece of JavaScript in the context of a stack frame for
// debugging.  This is done by creating a new context which in its extension
// part has all the parameters and locals of the function on the stack frame
// as well as a materialized arguments object.  As this context replaces
// the context of the function on the stack frame a new (empty) function
// is created as well to be used as the closure for the context.
// This closure as replacements for the one on the stack frame presenting
// the same view of the values of parameters and local variables as if the
// piece of JavaScript was evaluated at the point where the function on the
// stack frame is currently stopped when we compile and run the (direct) eval.
// Returns array of
// #0: evaluate result
// #1: local variables materizalized again as object after evaluation, contain
//     original variable values as they remained on stack
// #2: local variables materizalized as object before evaluation (and possibly
//     modified by expression having been executed)
// Since user expression only reaches (and modifies) copies of local variables,
// those copies are returned to the caller to allow tracking the changes and
// manually updating the actual variables.
RUNTIME_FUNCTION(MaybeObject*, Runtime_DebugEvaluate) {
  HandleScope scope(isolate);

  // Check the execution state and decode arguments frame and source to be
  // evaluated.
  ASSERT(args.length() == 6);
  Object* check_result;
  { MaybeObject* maybe_result = Runtime_CheckExecutionState(
      RUNTIME_ARGUMENTS(isolate, args));
    if (!maybe_result->ToObject(&check_result)) return maybe_result;
  }
  CONVERT_SMI_ARG_CHECKED(wrapped_id, 1);
  CONVERT_NUMBER_CHECKED(int, inlined_jsframe_index, Int32, args[2]);
  CONVERT_ARG_HANDLE_CHECKED(String, source, 3);
  CONVERT_BOOLEAN_ARG_CHECKED(disable_break, 4);
  Handle<Object> context_extension(args[5], isolate);

  // Handle the processing of break.
  DisableBreak disable_break_save(disable_break);

  // Get the frame where the debugging is performed.
  StackFrame::Id id = UnwrapFrameId(wrapped_id);
  JavaScriptFrameIterator it(isolate, id);
  JavaScriptFrame* frame = it.frame();
  FrameInspector frame_inspector(frame, inlined_jsframe_index, isolate);
  Handle<JSFunction> function(JSFunction::cast(frame_inspector.GetFunction()));

  // Traverse the saved contexts chain to find the active context for the
  // selected frame.
  SaveContext* save = FindSavedContextForFrame(isolate, frame);

  SaveContext savex(isolate);
  isolate->set_context(*(save->context()));

  // Create the (empty) function replacing the function on the stack frame for
  // the purpose of evaluating in the context created below. It is important
  // that this function does not describe any parameters and local variables
  // in the context. If it does then this will cause problems with the lookup
  // in Context::Lookup, where context slots for parameters and local variables
  // are looked at before the extension object.
  Handle<JSFunction> go_between =
      isolate->factory()->NewFunction(isolate->factory()->empty_string(),
                                      isolate->factory()->undefined_value());
  go_between->set_context(function->context());
#ifdef DEBUG
  Handle<ScopeInfo> go_between_scope_info(go_between->shared()->scope_info());
  ASSERT(go_between_scope_info->ParameterCount() == 0);
  ASSERT(go_between_scope_info->ContextLocalCount() == 0);
#endif

  // Materialize the content of the local scope including the arguments object.
  Handle<JSObject> local_scope = MaterializeLocalScopeWithFrameInspector(
      isolate, frame, &frame_inspector);
  RETURN_IF_EMPTY_HANDLE(isolate, local_scope);

  // Do not materialize the arguments object for eval or top-level code.
  if (function->shared()->is_function()) {
    Handle<Context> frame_context(Context::cast(frame->context()));
    Handle<Context> function_context;
    Handle<ScopeInfo> scope_info(function->shared()->scope_info());
    if (scope_info->HasContext()) {
      function_context = Handle<Context>(frame_context->declaration_context());
    }
    Handle<Object> arguments = GetArgumentsObject(isolate,
                                                  frame,
                                                  &frame_inspector,
                                                  scope_info,
                                                  function_context);
    SetProperty(isolate,
                local_scope,
                isolate->factory()->arguments_string(),
                arguments,
                ::NONE,
                kNonStrictMode);
  }

  // Allocate a new context for the debug evaluation and set the extension
  // object build.
  Handle<Context> context = isolate->factory()->NewFunctionContext(
      Context::MIN_CONTEXT_SLOTS, go_between);

  // Use the materialized local scope in a with context.
  context =
      isolate->factory()->NewWithContext(go_between, context, local_scope);

  // Copy any with contexts present and chain them in front of this context.
  context = CopyNestedScopeContextChain(isolate,
                                        go_between,
                                        context,
                                        frame,
                                        inlined_jsframe_index);
  if (context.is_null()) {
    ASSERT(isolate->has_pending_exception());
    MaybeObject* exception = isolate->pending_exception();
    isolate->clear_pending_exception();
    return exception;
  }

  Handle<Object> receiver(frame->receiver(), isolate);
  Object* evaluate_result_object;
  { MaybeObject* maybe_result =
    DebugEvaluate(isolate, context, context_extension, receiver, source);
    if (!maybe_result->ToObject(&evaluate_result_object)) return maybe_result;
  }
  Handle<Object> evaluate_result(evaluate_result_object, isolate);

  Handle<JSObject> local_scope_control_copy =
      MaterializeLocalScopeWithFrameInspector(isolate, frame,
                                              &frame_inspector);

  Handle<FixedArray> resultArray = isolate->factory()->NewFixedArray(3);
  resultArray->set(0, *evaluate_result);
  resultArray->set(1, *local_scope_control_copy);
  resultArray->set(2, *local_scope);

  return *(isolate->factory()->NewJSArrayWithElements(resultArray));
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_DebugEvaluateGlobal) {
  HandleScope scope(isolate);

  // Check the execution state and decode arguments frame and source to be
  // evaluated.
  ASSERT(args.length() == 4);
  Object* check_result;
  { MaybeObject* maybe_result = Runtime_CheckExecutionState(
      RUNTIME_ARGUMENTS(isolate, args));
    if (!maybe_result->ToObject(&check_result)) return maybe_result;
  }
  CONVERT_ARG_HANDLE_CHECKED(String, source, 1);
  CONVERT_BOOLEAN_ARG_CHECKED(disable_break, 2);
  Handle<Object> context_extension(args[3], isolate);

  // Handle the processing of break.
  DisableBreak disable_break_save(disable_break);

  // Enter the top context from before the debugger was invoked.
  SaveContext save(isolate);
  SaveContext* top = &save;
  while (top != NULL && *top->context() == *isolate->debug()->debug_context()) {
    top = top->prev();
  }
  if (top != NULL) {
    isolate->set_context(*top->context());
  }

  // Get the native context now set to the top context from before the
  // debugger was invoked.
  Handle<Context> context = isolate->native_context();
  Handle<Object> receiver = isolate->global_object();
  return DebugEvaluate(isolate, context, context_extension, receiver, source);
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_DebugGetLoadedScripts) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 0);

  // Fill the script objects.
  Handle<FixedArray> instances = isolate->debug()->GetLoadedScripts();

  // Convert the script objects to proper JS objects.
  for (int i = 0; i < instances->length(); i++) {
    Handle<Script> script = Handle<Script>(Script::cast(instances->get(i)));
    // Get the script wrapper in a local handle before calling GetScriptWrapper,
    // because using
    //   instances->set(i, *GetScriptWrapper(script))
    // is unsafe as GetScriptWrapper might call GC and the C++ compiler might
    // already have dereferenced the instances handle.
    Handle<JSValue> wrapper = GetScriptWrapper(script);
    instances->set(i, *wrapper);
  }

  // Return result as a JS array.
  Handle<JSObject> result =
      isolate->factory()->NewJSObject(isolate->array_function());
  isolate->factory()->SetContent(Handle<JSArray>::cast(result), instances);
  return *result;
}


// Helper function used by Runtime_DebugReferencedBy below.
static int DebugReferencedBy(HeapIterator* iterator,
                             JSObject* target,
                             Object* instance_filter, int max_references,
                             FixedArray* instances, int instances_size,
                             JSFunction* arguments_function) {
  Isolate* isolate = target->GetIsolate();
  SealHandleScope shs(isolate);
  DisallowHeapAllocation no_allocation;

  // Iterate the heap.
  int count = 0;
  JSObject* last = NULL;
  HeapObject* heap_obj = NULL;
  while (((heap_obj = iterator->next()) != NULL) &&
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
            Object* prototype = V->GetPrototype(isolate);
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
RUNTIME_FUNCTION(MaybeObject*, Runtime_DebugReferencedBy) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 3);

  // First perform a full GC in order to avoid references from dead objects.
  isolate->heap()->CollectAllGarbage(Heap::kMakeHeapIterableMask,
                                     "%DebugReferencedBy");
  // The heap iterator reserves the right to do a GC to make the heap iterable.
  // Due to the GC above we know it won't need to do that, but it seems cleaner
  // to get the heap iterator constructed before we start having unprotected
  // Object* locals that are not protected by handles.

  // Check parameters.
  CONVERT_ARG_CHECKED(JSObject, target, 0);
  Object* instance_filter = args[1];
  RUNTIME_ASSERT(instance_filter->IsUndefined() ||
                 instance_filter->IsJSObject());
  CONVERT_NUMBER_CHECKED(int32_t, max_references, Int32, args[2]);
  RUNTIME_ASSERT(max_references >= 0);


  // Get the constructor function for context extension and arguments array.
  JSObject* arguments_boilerplate =
      isolate->context()->native_context()->arguments_boilerplate();
  JSFunction* arguments_function =
      JSFunction::cast(arguments_boilerplate->map()->constructor());

  // Get the number of referencing objects.
  int count;
  Heap* heap = isolate->heap();
  HeapIterator heap_iterator(heap);
  count = DebugReferencedBy(&heap_iterator,
                            target, instance_filter, max_references,
                            NULL, 0, arguments_function);

  // Allocate an array to hold the result.
  Object* object;
  { MaybeObject* maybe_object = heap->AllocateFixedArray(count);
    if (!maybe_object->ToObject(&object)) return maybe_object;
  }
  FixedArray* instances = FixedArray::cast(object);

  // Fill the referencing objects.
  // AllocateFixedArray above does not make the heap non-iterable.
  ASSERT(heap->IsHeapIterable());
  HeapIterator heap_iterator2(heap);
  count = DebugReferencedBy(&heap_iterator2,
                            target, instance_filter, max_references,
                            instances, count, arguments_function);

  // Return result as JS array.
  Object* result;
  MaybeObject* maybe_result = heap->AllocateJSObject(
      isolate->context()->native_context()->array_function());
  if (!maybe_result->ToObject(&result)) return maybe_result;
  return JSArray::cast(result)->SetContent(instances);
}


// Helper function used by Runtime_DebugConstructedBy below.
static int DebugConstructedBy(HeapIterator* iterator,
                              JSFunction* constructor,
                              int max_references,
                              FixedArray* instances,
                              int instances_size) {
  DisallowHeapAllocation no_allocation;

  // Iterate the heap.
  int count = 0;
  HeapObject* heap_obj = NULL;
  while (((heap_obj = iterator->next()) != NULL) &&
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
RUNTIME_FUNCTION(MaybeObject*, Runtime_DebugConstructedBy) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 2);

  // First perform a full GC in order to avoid dead objects.
  Heap* heap = isolate->heap();
  heap->CollectAllGarbage(Heap::kMakeHeapIterableMask, "%DebugConstructedBy");

  // Check parameters.
  CONVERT_ARG_CHECKED(JSFunction, constructor, 0);
  CONVERT_NUMBER_CHECKED(int32_t, max_references, Int32, args[1]);
  RUNTIME_ASSERT(max_references >= 0);

  // Get the number of referencing objects.
  int count;
  HeapIterator heap_iterator(heap);
  count = DebugConstructedBy(&heap_iterator,
                             constructor,
                             max_references,
                             NULL,
                             0);

  // Allocate an array to hold the result.
  Object* object;
  { MaybeObject* maybe_object = heap->AllocateFixedArray(count);
    if (!maybe_object->ToObject(&object)) return maybe_object;
  }
  FixedArray* instances = FixedArray::cast(object);

  ASSERT(HEAP->IsHeapIterable());
  // Fill the referencing objects.
  HeapIterator heap_iterator2(heap);
  count = DebugConstructedBy(&heap_iterator2,
                             constructor,
                             max_references,
                             instances,
                             count);

  // Return result as JS array.
  Object* result;
  { MaybeObject* maybe_result = isolate->heap()->AllocateJSObject(
      isolate->context()->native_context()->array_function());
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }
  return JSArray::cast(result)->SetContent(instances);
}


// Find the effective prototype object as returned by __proto__.
// args[0]: the object to find the prototype for.
RUNTIME_FUNCTION(MaybeObject*, Runtime_DebugGetPrototype) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_CHECKED(JSObject, obj, 0);
  return GetPrototypeSkipHiddenPrototypes(isolate, obj);
}


// Patches script source (should be called upon BeforeCompile event).
RUNTIME_FUNCTION(MaybeObject*, Runtime_DebugSetScriptSource) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);

  CONVERT_ARG_HANDLE_CHECKED(JSValue, script_wrapper, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, source, 1);

  RUNTIME_ASSERT(script_wrapper->value()->IsScript());
  Handle<Script> script(Script::cast(script_wrapper->value()));

  int compilation_state = Smi::cast(script->compilation_state())->value();
  RUNTIME_ASSERT(compilation_state == Script::COMPILATION_STATE_INITIAL);
  script->set_source(*source);

  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_SystemBreak) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 0);
  CPU::DebugBreak();
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_DebugDisassembleFunction) {
  HandleScope scope(isolate);
#ifdef DEBUG
  ASSERT(args.length() == 1);
  // Get the function and make sure it is compiled.
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, func, 0);
  if (!JSFunction::EnsureCompiled(func, KEEP_EXCEPTION)) {
    return Failure::Exception();
  }
  func->code()->PrintLn();
#endif  // DEBUG
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_DebugDisassembleConstructor) {
  HandleScope scope(isolate);
#ifdef DEBUG
  ASSERT(args.length() == 1);
  // Get the function and make sure it is compiled.
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, func, 0);
  if (!JSFunction::EnsureCompiled(func, KEEP_EXCEPTION)) {
    return Failure::Exception();
  }
  func->shared()->construct_stub()->PrintLn();
#endif  // DEBUG
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_FunctionGetInferredName) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);

  CONVERT_ARG_CHECKED(JSFunction, f, 0);
  return f->shared()->inferred_name();
}


static int FindSharedFunctionInfosForScript(HeapIterator* iterator,
                                            Script* script,
                                            FixedArray* buffer) {
  DisallowHeapAllocation no_allocation;
  int counter = 0;
  int buffer_size = buffer->length();
  for (HeapObject* obj = iterator->next();
       obj != NULL;
       obj = iterator->next()) {
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
RUNTIME_FUNCTION(MaybeObject*,
                 Runtime_LiveEditFindSharedFunctionInfosForScript) {
  HandleScope scope(isolate);
  CHECK(isolate->debugger()->live_edit_enabled());
  ASSERT(args.length() == 1);
  CONVERT_ARG_CHECKED(JSValue, script_value, 0);

  RUNTIME_ASSERT(script_value->value()->IsScript());
  Handle<Script> script = Handle<Script>(Script::cast(script_value->value()));

  const int kBufferSize = 32;

  Handle<FixedArray> array;
  array = isolate->factory()->NewFixedArray(kBufferSize);
  int number;
  Heap* heap = isolate->heap();
  {
    heap->EnsureHeapIsIterable();
    DisallowHeapAllocation no_allocation;
    HeapIterator heap_iterator(heap);
    Script* scr = *script;
    FixedArray* arr = *array;
    number = FindSharedFunctionInfosForScript(&heap_iterator, scr, arr);
  }
  if (number > kBufferSize) {
    array = isolate->factory()->NewFixedArray(number);
    heap->EnsureHeapIsIterable();
    DisallowHeapAllocation no_allocation;
    HeapIterator heap_iterator(heap);
    Script* scr = *script;
    FixedArray* arr = *array;
    FindSharedFunctionInfosForScript(&heap_iterator, scr, arr);
  }

  Handle<JSArray> result = isolate->factory()->NewJSArrayWithElements(array);
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
RUNTIME_FUNCTION(MaybeObject*, Runtime_LiveEditGatherCompileInfo) {
  HandleScope scope(isolate);
  CHECK(isolate->debugger()->live_edit_enabled());
  ASSERT(args.length() == 2);
  CONVERT_ARG_CHECKED(JSValue, script, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, source, 1);

  RUNTIME_ASSERT(script->value()->IsScript());
  Handle<Script> script_handle = Handle<Script>(Script::cast(script->value()));

  JSArray* result =  LiveEdit::GatherCompileInfo(script_handle, source);

  if (isolate->has_pending_exception()) {
    return Failure::Exception();
  }

  return result;
}

// Changes the source of the script to a new_source.
// If old_script_name is provided (i.e. is a String), also creates a copy of
// the script with its original source and sends notification to debugger.
RUNTIME_FUNCTION(MaybeObject*, Runtime_LiveEditReplaceScript) {
  HandleScope scope(isolate);
  CHECK(isolate->debugger()->live_edit_enabled());
  ASSERT(args.length() == 3);
  CONVERT_ARG_CHECKED(JSValue, original_script_value, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, new_source, 1);
  Handle<Object> old_script_name(args[2], isolate);

  RUNTIME_ASSERT(original_script_value->value()->IsScript());
  Handle<Script> original_script(Script::cast(original_script_value->value()));

  Object* old_script = LiveEdit::ChangeScriptSource(original_script,
                                                    new_source,
                                                    old_script_name);

  if (old_script->IsScript()) {
    Handle<Script> script_handle(Script::cast(old_script));
    return *(GetScriptWrapper(script_handle));
  } else {
    return isolate->heap()->null_value();
  }
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_LiveEditFunctionSourceUpdated) {
  HandleScope scope(isolate);
  CHECK(isolate->debugger()->live_edit_enabled());
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSArray, shared_info, 0);
  return LiveEdit::FunctionSourceUpdated(shared_info);
}


// Replaces code of SharedFunctionInfo with a new one.
RUNTIME_FUNCTION(MaybeObject*, Runtime_LiveEditReplaceFunctionCode) {
  HandleScope scope(isolate);
  CHECK(isolate->debugger()->live_edit_enabled());
  ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSArray, new_compile_info, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSArray, shared_info, 1);

  return LiveEdit::ReplaceFunctionCode(new_compile_info, shared_info);
}

// Connects SharedFunctionInfo to another script.
RUNTIME_FUNCTION(MaybeObject*, Runtime_LiveEditFunctionSetScript) {
  HandleScope scope(isolate);
  CHECK(isolate->debugger()->live_edit_enabled());
  ASSERT(args.length() == 2);
  Handle<Object> function_object(args[0], isolate);
  Handle<Object> script_object(args[1], isolate);

  if (function_object->IsJSValue()) {
    Handle<JSValue> function_wrapper = Handle<JSValue>::cast(function_object);
    if (script_object->IsJSValue()) {
      RUNTIME_ASSERT(JSValue::cast(*script_object)->value()->IsScript());
      Script* script = Script::cast(JSValue::cast(*script_object)->value());
      script_object = Handle<Object>(script, isolate);
    }

    LiveEdit::SetFunctionScript(function_wrapper, script_object);
  } else {
    // Just ignore this. We may not have a SharedFunctionInfo for some functions
    // and we check it in this function.
  }

  return isolate->heap()->undefined_value();
}


// In a code of a parent function replaces original function as embedded object
// with a substitution one.
RUNTIME_FUNCTION(MaybeObject*, Runtime_LiveEditReplaceRefToNestedFunction) {
  HandleScope scope(isolate);
  CHECK(isolate->debugger()->live_edit_enabled());
  ASSERT(args.length() == 3);

  CONVERT_ARG_HANDLE_CHECKED(JSValue, parent_wrapper, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSValue, orig_wrapper, 1);
  CONVERT_ARG_HANDLE_CHECKED(JSValue, subst_wrapper, 2);

  LiveEdit::ReplaceRefToNestedFunction(parent_wrapper, orig_wrapper,
                                       subst_wrapper);

  return isolate->heap()->undefined_value();
}


// Updates positions of a shared function info (first parameter) according
// to script source change. Text change is described in second parameter as
// array of groups of 3 numbers:
// (change_begin, change_end, change_end_new_position).
// Each group describes a change in text; groups are sorted by change_begin.
RUNTIME_FUNCTION(MaybeObject*, Runtime_LiveEditPatchFunctionPositions) {
  HandleScope scope(isolate);
  CHECK(isolate->debugger()->live_edit_enabled());
  ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSArray, shared_array, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSArray, position_change_array, 1);

  return LiveEdit::PatchFunctionPositions(shared_array, position_change_array);
}


// For array of SharedFunctionInfo's (each wrapped in JSValue)
// checks that none of them have activations on stacks (of any thread).
// Returns array of the same length with corresponding results of
// LiveEdit::FunctionPatchabilityStatus type.
RUNTIME_FUNCTION(MaybeObject*, Runtime_LiveEditCheckAndDropActivations) {
  HandleScope scope(isolate);
  CHECK(isolate->debugger()->live_edit_enabled());
  ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSArray, shared_array, 0);
  CONVERT_BOOLEAN_ARG_CHECKED(do_drop, 1);

  return *LiveEdit::CheckAndDropActivations(shared_array, do_drop);
}

// Compares 2 strings line-by-line, then token-wise and returns diff in form
// of JSArray of triplets (pos1, pos1_end, pos2_end) describing list
// of diff chunks.
RUNTIME_FUNCTION(MaybeObject*, Runtime_LiveEditCompareStrings) {
  HandleScope scope(isolate);
  CHECK(isolate->debugger()->live_edit_enabled());
  ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(String, s1, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, s2, 1);

  return *LiveEdit::CompareStrings(s1, s2);
}


// Restarts a call frame and completely drops all frames above.
// Returns true if successful. Otherwise returns undefined or an error message.
RUNTIME_FUNCTION(MaybeObject*, Runtime_LiveEditRestartFrame) {
  HandleScope scope(isolate);
  CHECK(isolate->debugger()->live_edit_enabled());
  ASSERT(args.length() == 2);

  // Check arguments.
  Object* check;
  { MaybeObject* maybe_check = Runtime_CheckExecutionState(
      RUNTIME_ARGUMENTS(isolate, args));
    if (!maybe_check->ToObject(&check)) return maybe_check;
  }
  CONVERT_NUMBER_CHECKED(int, index, Int32, args[1]);
  Heap* heap = isolate->heap();

  // Find the relevant frame with the requested index.
  StackFrame::Id id = isolate->debug()->break_frame_id();
  if (id == StackFrame::NO_ID) {
    // If there are no JavaScript stack frames return undefined.
    return heap->undefined_value();
  }

  int count = 0;
  JavaScriptFrameIterator it(isolate, id);
  for (; !it.done(); it.Advance()) {
    if (index < count + it.frame()->GetInlineCount()) break;
    count += it.frame()->GetInlineCount();
  }
  if (it.done()) return heap->undefined_value();

  const char* error_message = LiveEdit::RestartFrame(it.frame());
  if (error_message) {
    return *(isolate->factory()->InternalizeUtf8String(error_message));
  }
  return heap->true_value();
}


// A testing entry. Returns statement position which is the closest to
// source_position.
RUNTIME_FUNCTION(MaybeObject*, Runtime_GetFunctionCodePositionFromSource) {
  HandleScope scope(isolate);
  CHECK(isolate->debugger()->live_edit_enabled());
  ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);
  CONVERT_NUMBER_CHECKED(int32_t, source_position, Int32, args[1]);

  Handle<Code> code(function->code(), isolate);

  if (code->kind() != Code::FUNCTION &&
      code->kind() != Code::OPTIMIZED_FUNCTION) {
    return isolate->heap()->undefined_value();
  }

  RelocIterator it(*code, RelocInfo::ModeMask(RelocInfo::STATEMENT_POSITION));
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
RUNTIME_FUNCTION(MaybeObject*, Runtime_ExecuteInDebugContext) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);
  CONVERT_BOOLEAN_ARG_CHECKED(without_debugger, 1);

  Handle<Object> result;
  bool pending_exception;
  {
    if (without_debugger) {
      result = Execution::Call(function, isolate->global_object(), 0, NULL,
                               &pending_exception);
    } else {
      EnterDebugger enter_debugger;
      result = Execution::Call(function, isolate->global_object(), 0, NULL,
                               &pending_exception);
    }
  }
  if (!pending_exception) {
    return *result;
  } else {
    return Failure::Exception();
  }
}


// Sets a v8 flag.
RUNTIME_FUNCTION(MaybeObject*, Runtime_SetFlags) {
  SealHandleScope shs(isolate);
  CONVERT_ARG_CHECKED(String, arg, 0);
  SmartArrayPointer<char> flags =
      arg->ToCString(DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL);
  FlagList::SetFlagsFromString(*flags, StrLength(*flags));
  return isolate->heap()->undefined_value();
}


// Performs a GC.
// Presently, it only does a full GC.
RUNTIME_FUNCTION(MaybeObject*, Runtime_CollectGarbage) {
  SealHandleScope shs(isolate);
  isolate->heap()->CollectAllGarbage(Heap::kNoGCFlags, "%CollectGarbage");
  return isolate->heap()->undefined_value();
}


// Gets the current heap usage.
RUNTIME_FUNCTION(MaybeObject*, Runtime_GetHeapUsage) {
  SealHandleScope shs(isolate);
  int usage = static_cast<int>(isolate->heap()->SizeOfObjects());
  if (!Smi::IsValid(usage)) {
    return *isolate->factory()->NewNumberFromInt(usage);
  }
  return Smi::FromInt(usage);
}

#endif  // ENABLE_DEBUGGER_SUPPORT


RUNTIME_FUNCTION(MaybeObject*, Runtime_ProfilerResume) {
  SealHandleScope shs(isolate);
  v8::V8::ResumeProfiler();
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_ProfilerPause) {
  SealHandleScope shs(isolate);
  v8::V8::PauseProfiler();
  return isolate->heap()->undefined_value();
}


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
  Factory* factory = script_name->GetIsolate()->factory();
  Heap* heap = script_name->GetHeap();
  heap->EnsureHeapIsIterable();
  DisallowHeapAllocation no_allocation_during_heap_iteration;
  HeapIterator iterator(heap);
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
  if (script.is_null()) return factory->undefined_value();

  // Return the script found.
  return GetScriptWrapper(script);
}


// Get the script object from script data. NOTE: Regarding performance
// see the NOTE for GetScriptFromScriptData.
// args[0]: script data for the script to find the source for
RUNTIME_FUNCTION(MaybeObject*, Runtime_GetScript) {
  HandleScope scope(isolate);

  ASSERT(args.length() == 1);

  CONVERT_ARG_CHECKED(String, script_name, 0);

  // Find the requested script.
  Handle<Object> result =
      Runtime_GetScriptFromScriptName(Handle<String>(script_name));
  return *result;
}


// Collect the raw data for a stack trace.  Returns an array of 4
// element segments each containing a receiver, function, code and
// native code offset.
RUNTIME_FUNCTION(MaybeObject*, Runtime_CollectStackTrace) {
  HandleScope scope(isolate);
  ASSERT_EQ(args.length(), 3);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, error_object, 0);
  Handle<Object> caller = args.at<Object>(1);
  CONVERT_NUMBER_CHECKED(int32_t, limit, Int32, args[2]);

  // Optionally capture a more detailed stack trace for the message.
  isolate->CaptureAndSetDetailedStackTrace(error_object);
  // Capture a simple stack trace for the stack property.
  return *isolate->CaptureSimpleStackTrace(error_object, caller, limit);
}


// Mark a function to recognize when called after GC to format the stack trace.
RUNTIME_FUNCTION(MaybeObject*, Runtime_MarkOneShotGetter) {
  HandleScope scope(isolate);
  ASSERT_EQ(args.length(), 1);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, fun, 0);
  Handle<String> key = isolate->factory()->hidden_stack_trace_string();
  JSObject::SetHiddenProperty(fun, key, key);
  return *fun;
}


// Retrieve the stack trace.  This could be the raw stack trace collected
// on stack overflow or the already formatted stack trace string.
RUNTIME_FUNCTION(MaybeObject*, Runtime_GetOverflowedStackTrace) {
  HandleScope scope(isolate);
  ASSERT_EQ(args.length(), 1);
  CONVERT_ARG_CHECKED(JSObject, error_object, 0);
  String* key = isolate->heap()->hidden_stack_trace_string();
  Object* result = error_object->GetHiddenProperty(key);
  if (result->IsTheHole()) result = isolate->heap()->undefined_value();
  RUNTIME_ASSERT(result->IsJSArray() ||
                 result->IsString() ||
                 result->IsUndefined());
  return result;
}


// Set or clear the stack trace attached to an stack overflow error object.
RUNTIME_FUNCTION(MaybeObject*, Runtime_SetOverflowedStackTrace) {
  HandleScope scope(isolate);
  ASSERT_EQ(args.length(), 2);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, error_object, 0);
  CONVERT_ARG_HANDLE_CHECKED(HeapObject, value, 1);
  Handle<String> key = isolate->factory()->hidden_stack_trace_string();
  if (value->IsUndefined()) {
    error_object->DeleteHiddenProperty(*key);
  } else {
    RUNTIME_ASSERT(value->IsString());
    JSObject::SetHiddenProperty(error_object, key, value);
  }
  return *error_object;
}


// Returns V8 version as a string.
RUNTIME_FUNCTION(MaybeObject*, Runtime_GetV8Version) {
  SealHandleScope shs(isolate);
  ASSERT_EQ(args.length(), 0);

  const char* version_string = v8::V8::GetVersion();

  return isolate->heap()->AllocateStringFromOneByte(CStrVector(version_string),
                                                  NOT_TENURED);
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_Abort) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 2);
  OS::PrintError("abort: %s\n",
                 reinterpret_cast<char*>(args[0]) + args.smi_at(1));
  isolate->PrintStack(stderr);
  OS::Abort();
  UNREACHABLE();
  return NULL;
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_FlattenString) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(String, str, 0);
  FlattenString(str);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_GetFromCache) {
  SealHandleScope shs(isolate);
  // This is only called from codegen, so checks might be more lax.
  CONVERT_ARG_CHECKED(JSFunctionResultCache, cache, 0);
  Object* key = args[1];

  int finger_index = cache->finger_index();
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
      cache->set_finger_index(i);
      return cache->get(i + 1);
    }
  }

  int size = cache->size();
  ASSERT(size <= cache->length());

  for (int i = size - 2; i > finger_index; i -= 2) {
    o = cache->get(i);
    if (o == key) {
      cache->set_finger_index(i);
      return cache->get(i + 1);
    }
  }

  // There is no value in the cache.  Invoke the function and cache result.
  HandleScope scope(isolate);

  Handle<JSFunctionResultCache> cache_handle(cache);
  Handle<Object> key_handle(key, isolate);
  Handle<Object> value;
  {
    Handle<JSFunction> factory(JSFunction::cast(
          cache_handle->get(JSFunctionResultCache::kFactoryIndex)));
    // TODO(antonm): consider passing a receiver when constructing a cache.
    Handle<Object> receiver(isolate->native_context()->global_object(),
                            isolate);
    // This handle is nor shared, nor used later, so it's safe.
    Handle<Object> argv[] = { key_handle };
    bool pending_exception;
    value = Execution::Call(factory,
                            receiver,
                            ARRAY_SIZE(argv),
                            argv,
                            &pending_exception);
    if (pending_exception) return Failure::Exception();
  }

#ifdef VERIFY_HEAP
  if (FLAG_verify_heap) {
    cache_handle->JSFunctionResultCacheVerify();
  }
#endif

  // Function invocation may have cleared the cache.  Reread all the data.
  finger_index = cache_handle->finger_index();
  size = cache_handle->size();

  // If we have spare room, put new data into it, otherwise evict post finger
  // entry which is likely to be the least recently used.
  int index = -1;
  if (size < cache_handle->length()) {
    cache_handle->set_size(size + JSFunctionResultCache::kEntrySize);
    index = size;
  } else {
    index = finger_index + JSFunctionResultCache::kEntrySize;
    if (index == cache_handle->length()) {
      index = JSFunctionResultCache::kEntriesIndex;
    }
  }

  ASSERT(index % 2 == 0);
  ASSERT(index >= JSFunctionResultCache::kEntriesIndex);
  ASSERT(index < cache_handle->length());

  cache_handle->set(index, *key_handle);
  cache_handle->set(index + 1, *value);
  cache_handle->set_finger_index(index);

#ifdef VERIFY_HEAP
  if (FLAG_verify_heap) {
    cache_handle->JSFunctionResultCacheVerify();
  }
#endif

  return *value;
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_MessageGetStartPosition) {
  SealHandleScope shs(isolate);
  CONVERT_ARG_CHECKED(JSMessageObject, message, 0);
  return Smi::FromInt(message->start_position());
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_MessageGetScript) {
  SealHandleScope shs(isolate);
  CONVERT_ARG_CHECKED(JSMessageObject, message, 0);
  return message->script();
}


#ifdef DEBUG
// ListNatives is ONLY used by the fuzz-natives.js in debug mode
// Exclude the code in release mode.
RUNTIME_FUNCTION(MaybeObject*, Runtime_ListNatives) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 0);
#define COUNT_ENTRY(Name, argc, ressize) + 1
  int entry_count = 0
      RUNTIME_FUNCTION_LIST(COUNT_ENTRY)
      INLINE_FUNCTION_LIST(COUNT_ENTRY)
      INLINE_RUNTIME_FUNCTION_LIST(COUNT_ENTRY);
#undef COUNT_ENTRY
  Factory* factory = isolate->factory();
  Handle<FixedArray> elements = factory->NewFixedArray(entry_count);
  int index = 0;
  bool inline_runtime_functions = false;
#define ADD_ENTRY(Name, argc, ressize)                                       \
  {                                                                          \
    HandleScope inner(isolate);                                              \
    Handle<String> name;                                                     \
    /* Inline runtime functions have an underscore in front of the name. */  \
    if (inline_runtime_functions) {                                          \
      name = factory->NewStringFromAscii(                                    \
          Vector<const char>("_" #Name, StrLength("_" #Name)));              \
    } else {                                                                 \
      name = factory->NewStringFromAscii(                                    \
          Vector<const char>(#Name, StrLength(#Name)));                      \
    }                                                                        \
    Handle<FixedArray> pair_elements = factory->NewFixedArray(2);            \
    pair_elements->set(0, *name);                                            \
    pair_elements->set(1, Smi::FromInt(argc));                               \
    Handle<JSArray> pair = factory->NewJSArrayWithElements(pair_elements);   \
    elements->set(index++, *pair);                                           \
  }
  inline_runtime_functions = false;
  RUNTIME_FUNCTION_LIST(ADD_ENTRY)
  inline_runtime_functions = true;
  INLINE_FUNCTION_LIST(ADD_ENTRY)
  INLINE_RUNTIME_FUNCTION_LIST(ADD_ENTRY)
#undef ADD_ENTRY
  ASSERT_EQ(index, entry_count);
  Handle<JSArray> result = factory->NewJSArrayWithElements(elements);
  return *result;
}
#endif


RUNTIME_FUNCTION(MaybeObject*, Runtime_Log) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_CHECKED(String, format, 0);
  CONVERT_ARG_CHECKED(JSArray, elms, 1);
  DisallowHeapAllocation no_gc;
  String::FlatContent format_content = format->GetFlatContent();
  RUNTIME_ASSERT(format_content.IsAscii());
  Vector<const uint8_t> chars = format_content.ToOneByteVector();
  isolate->logger()->LogRuntime(Vector<const char>::cast(chars), elms);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_IS_VAR) {
  UNREACHABLE();  // implemented as macro in the parser
  return NULL;
}


#define ELEMENTS_KIND_CHECK_RUNTIME_FUNCTION(Name)        \
  RUNTIME_FUNCTION(MaybeObject*, Runtime_Has##Name) {     \
    CONVERT_ARG_CHECKED(JSObject, obj, 0);              \
    return isolate->heap()->ToBoolean(obj->Has##Name());  \
  }

ELEMENTS_KIND_CHECK_RUNTIME_FUNCTION(FastSmiElements)
ELEMENTS_KIND_CHECK_RUNTIME_FUNCTION(FastObjectElements)
ELEMENTS_KIND_CHECK_RUNTIME_FUNCTION(FastSmiOrObjectElements)
ELEMENTS_KIND_CHECK_RUNTIME_FUNCTION(FastDoubleElements)
ELEMENTS_KIND_CHECK_RUNTIME_FUNCTION(FastHoleyElements)
ELEMENTS_KIND_CHECK_RUNTIME_FUNCTION(DictionaryElements)
ELEMENTS_KIND_CHECK_RUNTIME_FUNCTION(NonStrictArgumentsElements)
ELEMENTS_KIND_CHECK_RUNTIME_FUNCTION(ExternalPixelElements)
ELEMENTS_KIND_CHECK_RUNTIME_FUNCTION(ExternalArrayElements)
ELEMENTS_KIND_CHECK_RUNTIME_FUNCTION(ExternalByteElements)
ELEMENTS_KIND_CHECK_RUNTIME_FUNCTION(ExternalUnsignedByteElements)
ELEMENTS_KIND_CHECK_RUNTIME_FUNCTION(ExternalShortElements)
ELEMENTS_KIND_CHECK_RUNTIME_FUNCTION(ExternalUnsignedShortElements)
ELEMENTS_KIND_CHECK_RUNTIME_FUNCTION(ExternalIntElements)
ELEMENTS_KIND_CHECK_RUNTIME_FUNCTION(ExternalUnsignedIntElements)
ELEMENTS_KIND_CHECK_RUNTIME_FUNCTION(ExternalFloatElements)
ELEMENTS_KIND_CHECK_RUNTIME_FUNCTION(ExternalDoubleElements)
// Properties test sitting with elements tests - not fooling anyone.
ELEMENTS_KIND_CHECK_RUNTIME_FUNCTION(FastProperties)

#undef ELEMENTS_KIND_CHECK_RUNTIME_FUNCTION


RUNTIME_FUNCTION(MaybeObject*, Runtime_HaveSameMap) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_CHECKED(JSObject, obj1, 0);
  CONVERT_ARG_CHECKED(JSObject, obj2, 1);
  return isolate->heap()->ToBoolean(obj1->map() == obj2->map());
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_IsObserved) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);

  if (!args[0]->IsJSReceiver()) return isolate->heap()->false_value();
  JSReceiver* obj = JSReceiver::cast(args[0]);
  if (obj->IsJSGlobalProxy()) {
    Object* proto = obj->GetPrototype();
    if (proto->IsNull()) return isolate->heap()->false_value();
    ASSERT(proto->IsJSGlobalObject());
    obj = JSReceiver::cast(proto);
  }
  return isolate->heap()->ToBoolean(obj->map()->is_observed());
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_SetIsObserved) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_CHECKED(JSReceiver, obj, 0);
  CONVERT_BOOLEAN_ARG_CHECKED(is_observed, 1);
  if (obj->IsJSGlobalProxy()) {
    Object* proto = obj->GetPrototype();
    if (proto->IsNull()) return isolate->heap()->undefined_value();
    ASSERT(proto->IsJSGlobalObject());
    obj = JSReceiver::cast(proto);
  }
  ASSERT(!(obj->map()->is_observed() && obj->IsJSObject() &&
           JSObject::cast(obj)->HasFastElements()));
  if (obj->map()->is_observed() != is_observed) {
    if (is_observed && obj->IsJSObject() &&
        !JSObject::cast(obj)->HasExternalArrayElements()) {
      // Go to dictionary mode, so that we don't skip map checks.
      MaybeObject* maybe = JSObject::cast(obj)->NormalizeElements();
      if (maybe->IsFailure()) return maybe;
      ASSERT(!JSObject::cast(obj)->HasFastElements());
    }
    MaybeObject* maybe = obj->map()->Copy();
    Map* map;
    if (!maybe->To(&map)) return maybe;
    map->set_is_observed(is_observed);
    obj->set_map(map);
  }
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_SetObserverDeliveryPending) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 0);
  isolate->set_observer_delivery_pending(true);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_GetObservationState) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 0);
  return isolate->heap()->observation_state();
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_ObservationWeakMapCreate) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 0);
  // TODO(adamk): Currently this runtime function is only called three times per
  // isolate. If it's called more often, the map should be moved into the
  // strong root list.
  Handle<Map> map =
      isolate->factory()->NewMap(JS_WEAK_MAP_TYPE, JSWeakMap::kSize);
  Handle<JSWeakMap> weakmap =
      Handle<JSWeakMap>::cast(isolate->factory()->NewJSObjectFromMap(map));
  return WeakMapInitialize(isolate, weakmap);
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_UnwrapGlobalProxy) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  Object* object = args[0];
  if (object->IsJSGlobalProxy()) {
    object = object->GetPrototype(isolate);
    if (object->IsNull()) return isolate->heap()->undefined_value();
  }
  return object;
}


static MaybeObject* ArrayConstructorCommon(Isolate* isolate,
                                           Handle<JSFunction> constructor,
                                           Handle<Object> type_info,
                                           Arguments* caller_args) {
  bool holey = false;
  bool can_use_type_feedback = true;
  if (caller_args->length() == 1) {
    Object* argument_one = (*caller_args)[0];
    if (argument_one->IsSmi()) {
      int value = Smi::cast(argument_one)->value();
      if (value < 0 || value >= JSObject::kInitialMaxFastElementArray) {
        // the array is a dictionary in this case.
        can_use_type_feedback = false;
      } else if (value != 0) {
        holey = true;
      }
    } else {
      // Non-smi length argument produces a dictionary
      can_use_type_feedback = false;
    }
  }

  JSArray* array;
  MaybeObject* maybe_array;
  if (!type_info.is_null() &&
      *type_info != isolate->heap()->undefined_value() &&
      Cell::cast(*type_info)->value()->IsSmi() &&
      can_use_type_feedback) {
    Cell* cell = Cell::cast(*type_info);
    Smi* smi = Smi::cast(cell->value());
    ElementsKind to_kind = static_cast<ElementsKind>(smi->value());
    if (holey && !IsFastHoleyElementsKind(to_kind)) {
      to_kind = GetHoleyElementsKind(to_kind);
      // Update the allocation site info to reflect the advice alteration.
      cell->set_value(Smi::FromInt(to_kind));
    }

    maybe_array = isolate->heap()->AllocateJSObjectWithAllocationSite(
        *constructor, type_info);
    if (!maybe_array->To(&array)) return maybe_array;
  } else {
    maybe_array = isolate->heap()->AllocateJSObject(*constructor);
    if (!maybe_array->To(&array)) return maybe_array;
    // We might need to transition to holey
    ElementsKind kind = constructor->initial_map()->elements_kind();
    if (holey && !IsFastHoleyElementsKind(kind)) {
      kind = GetHoleyElementsKind(kind);
      maybe_array = array->TransitionElementsKind(kind);
      if (maybe_array->IsFailure()) return maybe_array;
    }
  }

  maybe_array = isolate->heap()->AllocateJSArrayStorage(array, 0, 0,
      DONT_INITIALIZE_ARRAY_ELEMENTS);
  if (maybe_array->IsFailure()) return maybe_array;
  maybe_array = ArrayConstructInitializeElements(array, caller_args);
  if (maybe_array->IsFailure()) return maybe_array;
  return array;
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_ArrayConstructor) {
  HandleScope scope(isolate);
  // If we get 2 arguments then they are the stub parameters (constructor, type
  // info).  If we get 3, then the first one is a pointer to the arguments
  // passed by the caller.
  Arguments empty_args(0, NULL);
  bool no_caller_args = args.length() == 2;
  ASSERT(no_caller_args || args.length() == 3);
  int parameters_start = no_caller_args ? 0 : 1;
  Arguments* caller_args = no_caller_args
      ? &empty_args
      : reinterpret_cast<Arguments*>(args[0]);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, constructor, parameters_start);
  CONVERT_ARG_HANDLE_CHECKED(Object, type_info, parameters_start + 1);

  return ArrayConstructorCommon(isolate,
                                constructor,
                                type_info,
                                caller_args);
}


RUNTIME_FUNCTION(MaybeObject*, Runtime_InternalArrayConstructor) {
  HandleScope scope(isolate);
  Arguments empty_args(0, NULL);
  bool no_caller_args = args.length() == 1;
  ASSERT(no_caller_args || args.length() == 2);
  int parameters_start = no_caller_args ? 0 : 1;
  Arguments* caller_args = no_caller_args
      ? &empty_args
      : reinterpret_cast<Arguments*>(args[0]);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, constructor, parameters_start);

  return ArrayConstructorCommon(isolate,
                                constructor,
                                Handle<Object>::null(),
                                caller_args);
}


// ----------------------------------------------------------------------------
// Implementation of Runtime

#define F(name, number_of_args, result_size)                             \
  { Runtime::k##name, Runtime::RUNTIME, #name,   \
    FUNCTION_ADDR(Runtime_##name), number_of_args, result_size },


#define I(name, number_of_args, result_size)                             \
  { Runtime::kInline##name, Runtime::INLINE,     \
    "_" #name, NULL, number_of_args, result_size },

static const Runtime::Function kIntrinsicFunctions[] = {
  RUNTIME_FUNCTION_LIST(F)
  INLINE_FUNCTION_LIST(I)
  INLINE_RUNTIME_FUNCTION_LIST(I)
};


MaybeObject* Runtime::InitializeIntrinsicFunctionNames(Heap* heap,
                                                       Object* dictionary) {
  ASSERT(Isolate::Current()->heap() == heap);
  ASSERT(dictionary != NULL);
  ASSERT(NameDictionary::cast(dictionary)->NumberOfElements() == 0);
  for (int i = 0; i < kNumFunctions; ++i) {
    Object* name_string;
    { MaybeObject* maybe_name_string =
          heap->InternalizeUtf8String(kIntrinsicFunctions[i].name);
      if (!maybe_name_string->ToObject(&name_string)) return maybe_name_string;
    }
    NameDictionary* name_dictionary = NameDictionary::cast(dictionary);
    { MaybeObject* maybe_dictionary = name_dictionary->Add(
          String::cast(name_string),
          Smi::FromInt(i),
          PropertyDetails(NONE, NORMAL, Representation::None()));
      if (!maybe_dictionary->ToObject(&dictionary)) {
        // Non-recoverable failure.  Calling code must restart heap
        // initialization.
        return maybe_dictionary;
      }
    }
  }
  return dictionary;
}


const Runtime::Function* Runtime::FunctionForName(Handle<String> name) {
  Heap* heap = name->GetHeap();
  int entry = heap->intrinsic_function_names()->FindEntry(*name);
  if (entry != kNotFound) {
    Object* smi_index = heap->intrinsic_function_names()->ValueAt(entry);
    int function_index = Smi::cast(smi_index)->value();
    return &(kIntrinsicFunctions[function_index]);
  }
  return NULL;
}


const Runtime::Function* Runtime::FunctionForId(Runtime::FunctionId id) {
  return &(kIntrinsicFunctions[static_cast<int>(id)]);
}


void Runtime::PerformGC(Object* result) {
  Isolate* isolate = Isolate::Current();
  Failure* failure = Failure::cast(result);
  if (failure->IsRetryAfterGC()) {
    if (isolate->heap()->new_space()->AddFreshPage()) {
      return;
    }

    // Try to do a garbage collection; ignore it if it fails. The C
    // entry stub will throw an out-of-memory exception in that case.
    isolate->heap()->CollectGarbage(failure->allocation_space(),
                                    "Runtime::PerformGC");
  } else {
    // Handle last resort GC and make sure to allow future allocations
    // to grow the heap without causing GCs (if possible).
    isolate->counters()->gc_last_resort_from_js()->Increment();
    isolate->heap()->CollectAllGarbage(Heap::kNoGCFlags,
                                       "Runtime::PerformGC");
  }
}


} }  // namespace v8::internal
