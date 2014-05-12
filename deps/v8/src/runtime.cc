// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>
#include <limits>

#include "v8.h"

#include "accessors.h"
#include "allocation-site-scopes.h"
#include "api.h"
#include "arguments.h"
#include "bootstrapper.h"
#include "codegen.h"
#include "compilation-cache.h"
#include "compiler.h"
#include "conversions.h"
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
#include "v8threads.h"
#include "vm-state-inl.h"

#ifdef V8_I18N_SUPPORT
#include "i18n.h"
#include "unicode/brkiter.h"
#include "unicode/calendar.h"
#include "unicode/coll.h"
#include "unicode/curramt.h"
#include "unicode/datefmt.h"
#include "unicode/dcfmtsym.h"
#include "unicode/decimfmt.h"
#include "unicode/dtfmtsym.h"
#include "unicode/dtptngen.h"
#include "unicode/locid.h"
#include "unicode/numfmt.h"
#include "unicode/numsys.h"
#include "unicode/rbbi.h"
#include "unicode/smpdtfmt.h"
#include "unicode/timezone.h"
#include "unicode/uchar.h"
#include "unicode/ucol.h"
#include "unicode/ucurr.h"
#include "unicode/uloc.h"
#include "unicode/unum.h"
#include "unicode/uversion.h"
#endif

#ifndef _STLP_VENDOR_CSTD
// STLPort doesn't import fpclassify and isless into the std namespace.
using std::fpclassify;
using std::isless;
#endif

namespace v8 {
namespace internal {


#define RUNTIME_ASSERT(value) \
  if (!(value)) return isolate->ThrowIllegalOperation();

#define RUNTIME_ASSERT_HANDLIFIED(value, T)                          \
  if (!(value)) {                                                    \
    isolate->ThrowIllegalOperation();                                \
    return MaybeHandle<T>();                                         \
  }

// Cast the given object to a value of the specified type and store
// it in a variable with the given name.  If the object is not of the
// expected type call IllegalOperation and return.
#define CONVERT_ARG_CHECKED(Type, name, index)                       \
  RUNTIME_ASSERT(args[index]->Is##Type());                           \
  Type* name = Type::cast(args[index]);

#define CONVERT_ARG_HANDLE_CHECKED(Type, name, index)                \
  RUNTIME_ASSERT(args[index]->Is##Type());                           \
  Handle<Type> name = args.at<Type>(index);

#define CONVERT_NUMBER_ARG_HANDLE_CHECKED(name, index)               \
  RUNTIME_ASSERT(args[index]->IsNumber());                           \
  Handle<Object> name = args.at<Object>(index);

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


// Assert that the given argument has a valid value for a StrictMode
// and store it in a StrictMode variable with the given name.
#define CONVERT_STRICT_MODE_ARG_CHECKED(name, index)                 \
  RUNTIME_ASSERT(args[index]->IsSmi());                              \
  RUNTIME_ASSERT(args.smi_at(index) == STRICT ||                     \
                 args.smi_at(index) == SLOPPY);                      \
  StrictMode name = static_cast<StrictMode>(args.smi_at(index));


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
  return Map::Create(handle(context->object_function()), number_of_properties);
}


MUST_USE_RESULT static MaybeHandle<Object> CreateLiteralBoilerplate(
    Isolate* isolate,
    Handle<FixedArray> literals,
    Handle<FixedArray> constant_properties);


MUST_USE_RESULT static MaybeHandle<Object> CreateObjectLiteralBoilerplate(
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

  PretenureFlag pretenure_flag =
      isolate->heap()->InNewSpace(*literals) ? NOT_TENURED : TENURED;

  Handle<JSObject> boilerplate =
      isolate->factory()->NewJSObjectFromMap(map, pretenure_flag);

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
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, value,
          CreateLiteralBoilerplate(isolate, literals, array),
          Object);
    }
    MaybeHandle<Object> maybe_result;
    uint32_t element_index = 0;
    StoreMode mode = value->IsJSObject() ? FORCE_FIELD : ALLOW_AS_CONSTANT;
    if (key->IsInternalizedString()) {
      if (Handle<String>::cast(key)->AsArrayIndex(&element_index)) {
        // Array index as string (uint32).
        maybe_result = JSObject::SetOwnElement(
            boilerplate, element_index, value, SLOPPY);
      } else {
        Handle<String> name(String::cast(*key));
        ASSERT(!name->AsArrayIndex(&element_index));
        maybe_result = JSObject::SetLocalPropertyIgnoreAttributes(
            boilerplate, name, value, NONE,
            Object::OPTIMAL_REPRESENTATION, mode);
      }
    } else if (key->ToArrayIndex(&element_index)) {
      // Array index (uint32).
      maybe_result = JSObject::SetOwnElement(
          boilerplate, element_index, value, SLOPPY);
    } else {
      // Non-uint32 number.
      ASSERT(key->IsNumber());
      double num = key->Number();
      char arr[100];
      Vector<char> buffer(arr, ARRAY_SIZE(arr));
      const char* str = DoubleToCString(num, buffer);
      Handle<String> name = isolate->factory()->NewStringFromAsciiChecked(str);
      maybe_result = JSObject::SetLocalPropertyIgnoreAttributes(
          boilerplate, name, value, NONE,
          Object::OPTIMAL_REPRESENTATION, mode);
    }
    // If setting the property on the boilerplate throws an
    // exception, the exception is converted to an empty handle in
    // the handle based operations.  In that case, we need to
    // convert back to an exception.
    RETURN_ON_EXCEPTION(isolate, maybe_result, Object);
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


MUST_USE_RESULT static MaybeHandle<Object> TransitionElements(
    Handle<Object> object,
    ElementsKind to_kind,
    Isolate* isolate) {
  HandleScope scope(isolate);
  if (!object->IsJSObject()) {
    isolate->ThrowIllegalOperation();
    return MaybeHandle<Object>();
  }
  ElementsKind from_kind =
      Handle<JSObject>::cast(object)->map()->elements_kind();
  if (Map::IsValidElementsTransition(from_kind, to_kind)) {
    JSObject::TransitionElementsKind(Handle<JSObject>::cast(object), to_kind);
    return object;
  }
  isolate->ThrowIllegalOperation();
  return MaybeHandle<Object>();
}


static const int kSmiLiteralMinimumLength = 1024;


MaybeHandle<Object> Runtime::CreateArrayLiteralBoilerplate(
    Isolate* isolate,
    Handle<FixedArray> literals,
    Handle<FixedArray> elements) {
  // Create the JSArray.
  Handle<JSFunction> constructor(
      JSFunction::NativeContextFromLiterals(*literals)->array_function());

  PretenureFlag pretenure_flag =
      isolate->heap()->InNewSpace(*literals) ? NOT_TENURED : TENURED;

  Handle<JSArray> object = Handle<JSArray>::cast(
      isolate->factory()->NewJSObject(constructor, pretenure_flag));

  ElementsKind constant_elements_kind =
      static_cast<ElementsKind>(Smi::cast(elements->get(0))->value());
  Handle<FixedArrayBase> constant_elements_values(
      FixedArrayBase::cast(elements->get(1)));

  { DisallowHeapAllocation no_gc;
    ASSERT(IsFastElementsKind(constant_elements_kind));
    Context* native_context = isolate->context()->native_context();
    Object* maps_array = native_context->js_array_maps();
    ASSERT(!maps_array->IsUndefined());
    Object* map = FixedArray::cast(maps_array)->get(constant_elements_kind);
    object->set_map(Map::cast(map));
  }

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
        if (fixed_array_values->get(i)->IsFixedArray()) {
          // The value contains the constant_properties of a
          // simple object or array literal.
          Handle<FixedArray> fa(FixedArray::cast(fixed_array_values->get(i)));
          Handle<Object> result;
          ASSIGN_RETURN_ON_EXCEPTION(
              isolate, result,
              CreateLiteralBoilerplate(isolate, literals, fa),
              Object);
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
        TransitionElements(object, FAST_HOLEY_ELEMENTS, isolate).Check();
      } else {
        TransitionElements(object, FAST_ELEMENTS, isolate).Check();
      }
    }
  }

  JSObject::ValidateElements(object);
  return object;
}


MUST_USE_RESULT static MaybeHandle<Object> CreateLiteralBoilerplate(
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
      return MaybeHandle<Object>();
  }
}


RUNTIME_FUNCTION(RuntimeHidden_CreateObjectLiteral) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 4);
  CONVERT_ARG_HANDLE_CHECKED(FixedArray, literals, 0);
  CONVERT_SMI_ARG_CHECKED(literals_index, 1);
  CONVERT_ARG_HANDLE_CHECKED(FixedArray, constant_properties, 2);
  CONVERT_SMI_ARG_CHECKED(flags, 3);
  bool should_have_fast_elements = (flags & ObjectLiteral::kFastElements) != 0;
  bool has_function_literal = (flags & ObjectLiteral::kHasFunction) != 0;

  RUNTIME_ASSERT(literals_index >= 0 && literals_index < literals->length());

  // Check if boilerplate exists. If not, create it first.
  Handle<Object> literal_site(literals->get(literals_index), isolate);
  Handle<AllocationSite> site;
  Handle<JSObject> boilerplate;
  if (*literal_site == isolate->heap()->undefined_value()) {
    Handle<Object> raw_boilerplate;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, raw_boilerplate,
        CreateObjectLiteralBoilerplate(
            isolate,
            literals,
            constant_properties,
            should_have_fast_elements,
            has_function_literal));
    boilerplate = Handle<JSObject>::cast(raw_boilerplate);

    AllocationSiteCreationContext creation_context(isolate);
    site = creation_context.EnterNewScope();
    RETURN_FAILURE_ON_EXCEPTION(
        isolate,
        JSObject::DeepWalk(boilerplate, &creation_context));
    creation_context.ExitScope(site, boilerplate);

    // Update the functions literal and return the boilerplate.
    literals->set(literals_index, *site);
  } else {
    site = Handle<AllocationSite>::cast(literal_site);
    boilerplate = Handle<JSObject>(JSObject::cast(site->transition_info()),
                                   isolate);
  }

  AllocationSiteUsageContext usage_context(isolate, site, true);
  usage_context.EnterNewScope();
  MaybeHandle<Object> maybe_copy = JSObject::DeepCopy(
      boilerplate, &usage_context);
  usage_context.ExitScope(site, boilerplate);
  Handle<Object> copy;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, copy, maybe_copy);
  return *copy;
}


MUST_USE_RESULT static MaybeHandle<AllocationSite> GetLiteralAllocationSite(
    Isolate* isolate,
    Handle<FixedArray> literals,
    int literals_index,
    Handle<FixedArray> elements) {
  // Check if boilerplate exists. If not, create it first.
  Handle<Object> literal_site(literals->get(literals_index), isolate);
  Handle<AllocationSite> site;
  if (*literal_site == isolate->heap()->undefined_value()) {
    ASSERT(*elements != isolate->heap()->empty_fixed_array());
    Handle<Object> boilerplate;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, boilerplate,
        Runtime::CreateArrayLiteralBoilerplate(isolate, literals, elements),
        AllocationSite);

    AllocationSiteCreationContext creation_context(isolate);
    site = creation_context.EnterNewScope();
    if (JSObject::DeepWalk(Handle<JSObject>::cast(boilerplate),
                           &creation_context).is_null()) {
      return Handle<AllocationSite>::null();
    }
    creation_context.ExitScope(site, Handle<JSObject>::cast(boilerplate));

    literals->set(literals_index, *site);
  } else {
    site = Handle<AllocationSite>::cast(literal_site);
  }

  return site;
}


static MaybeHandle<JSObject> CreateArrayLiteralImpl(Isolate* isolate,
                                           Handle<FixedArray> literals,
                                           int literals_index,
                                           Handle<FixedArray> elements,
                                           int flags) {
  RUNTIME_ASSERT_HANDLIFIED(literals_index >= 0 &&
                            literals_index < literals->length(), JSObject);
  Handle<AllocationSite> site;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, site,
      GetLiteralAllocationSite(isolate, literals, literals_index, elements),
      JSObject);

  bool enable_mementos = (flags & ArrayLiteral::kDisableMementos) == 0;
  Handle<JSObject> boilerplate(JSObject::cast(site->transition_info()));
  AllocationSiteUsageContext usage_context(isolate, site, enable_mementos);
  usage_context.EnterNewScope();
  JSObject::DeepCopyHints hints = (flags & ArrayLiteral::kShallowElements) == 0
      ? JSObject::kNoHints
      : JSObject::kObjectIsShallowArray;
  MaybeHandle<JSObject> copy = JSObject::DeepCopy(boilerplate, &usage_context,
                                                  hints);
  usage_context.ExitScope(site, boilerplate);
  return copy;
}


RUNTIME_FUNCTION(RuntimeHidden_CreateArrayLiteral) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 4);
  CONVERT_ARG_HANDLE_CHECKED(FixedArray, literals, 0);
  CONVERT_SMI_ARG_CHECKED(literals_index, 1);
  CONVERT_ARG_HANDLE_CHECKED(FixedArray, elements, 2);
  CONVERT_SMI_ARG_CHECKED(flags, 3);

  Handle<JSObject> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, result,
      CreateArrayLiteralImpl(isolate, literals, literals_index, elements,
                             flags));
  return *result;
}


RUNTIME_FUNCTION(RuntimeHidden_CreateArrayLiteralStubBailout) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 3);
  CONVERT_ARG_HANDLE_CHECKED(FixedArray, literals, 0);
  CONVERT_SMI_ARG_CHECKED(literals_index, 1);
  CONVERT_ARG_HANDLE_CHECKED(FixedArray, elements, 2);

  Handle<JSObject> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, result,
     CreateArrayLiteralImpl(isolate, literals, literals_index, elements,
                            ArrayLiteral::kShallowElements));
  return *result;
}


RUNTIME_FUNCTION(Runtime_CreateSymbol) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, name, 0);
  RUNTIME_ASSERT(name->IsString() || name->IsUndefined());
  Handle<Symbol> symbol = isolate->factory()->NewSymbol();
  if (name->IsString()) symbol->set_name(*name);
  return *symbol;
}


RUNTIME_FUNCTION(Runtime_CreatePrivateSymbol) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, name, 0);
  RUNTIME_ASSERT(name->IsString() || name->IsUndefined());
  Handle<Symbol> symbol = isolate->factory()->NewPrivateSymbol();
  if (name->IsString()) symbol->set_name(*name);
  return *symbol;
}


RUNTIME_FUNCTION(Runtime_CreateGlobalPrivateSymbol) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(String, name, 0);
  Handle<JSObject> registry = isolate->GetSymbolRegistry();
  Handle<String> part = isolate->factory()->private_intern_string();
  Handle<Object> privates;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, privates, Object::GetPropertyOrElement(registry, part));
  Handle<Object> symbol;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, symbol, Object::GetPropertyOrElement(privates, name));
  if (!symbol->IsSymbol()) {
    ASSERT(symbol->IsUndefined());
    symbol = isolate->factory()->NewPrivateSymbol();
    Handle<Symbol>::cast(symbol)->set_name(*name);
    JSObject::SetProperty(Handle<JSObject>::cast(privates),
                          name, symbol, NONE, STRICT).Assert();
  }
  return *symbol;
}


RUNTIME_FUNCTION(Runtime_NewSymbolWrapper) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(Symbol, symbol, 0);
  return *Object::ToObject(isolate, symbol).ToHandleChecked();
}


RUNTIME_FUNCTION(Runtime_SymbolDescription) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_CHECKED(Symbol, symbol, 0);
  return symbol->name();
}


RUNTIME_FUNCTION(Runtime_SymbolRegistry) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 0);
  return *isolate->GetSymbolRegistry();
}


RUNTIME_FUNCTION(Runtime_SymbolIsPrivate) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_CHECKED(Symbol, symbol, 0);
  return isolate->heap()->ToBoolean(symbol->is_private());
}


RUNTIME_FUNCTION(Runtime_CreateJSProxy) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, handler, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, prototype, 1);
  if (!prototype->IsJSReceiver()) prototype = isolate->factory()->null_value();
  return *isolate->factory()->NewJSProxy(handler, prototype);
}


RUNTIME_FUNCTION(Runtime_CreateJSFunctionProxy) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 4);
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, handler, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, call_trap, 1);
  RUNTIME_ASSERT(call_trap->IsJSFunction() || call_trap->IsJSFunctionProxy());
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, construct_trap, 2);
  CONVERT_ARG_HANDLE_CHECKED(Object, prototype, 3);
  if (!prototype->IsJSReceiver()) prototype = isolate->factory()->null_value();
  return *isolate->factory()->NewJSFunctionProxy(
      handler, call_trap, construct_trap, prototype);
}


RUNTIME_FUNCTION(Runtime_IsJSProxy) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, obj, 0);
  return isolate->heap()->ToBoolean(obj->IsJSProxy());
}


RUNTIME_FUNCTION(Runtime_IsJSFunctionProxy) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, obj, 0);
  return isolate->heap()->ToBoolean(obj->IsJSFunctionProxy());
}


RUNTIME_FUNCTION(Runtime_GetHandler) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_CHECKED(JSProxy, proxy, 0);
  return proxy->handler();
}


RUNTIME_FUNCTION(Runtime_GetCallTrap) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_CHECKED(JSFunctionProxy, proxy, 0);
  return proxy->call_trap();
}


RUNTIME_FUNCTION(Runtime_GetConstructTrap) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_CHECKED(JSFunctionProxy, proxy, 0);
  return proxy->construct_trap();
}


RUNTIME_FUNCTION(Runtime_Fix) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSProxy, proxy, 0);
  JSProxy::Fix(proxy);
  return isolate->heap()->undefined_value();
}


void Runtime::FreeArrayBuffer(Isolate* isolate,
                              JSArrayBuffer* phantom_array_buffer) {
  if (phantom_array_buffer->should_be_freed()) {
    ASSERT(phantom_array_buffer->is_external());
    free(phantom_array_buffer->backing_store());
  }
  if (phantom_array_buffer->is_external()) return;

  size_t allocated_length = NumberToSize(
      isolate, phantom_array_buffer->byte_length());

  isolate->heap()->AdjustAmountOfExternalAllocatedMemory(
      -static_cast<int64_t>(allocated_length));
  CHECK(V8::ArrayBufferAllocator() != NULL);
  V8::ArrayBufferAllocator()->Free(
      phantom_array_buffer->backing_store(),
      allocated_length);
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
    size_t allocated_length,
    bool initialize) {
  void* data;
  CHECK(V8::ArrayBufferAllocator() != NULL);
  if (allocated_length != 0) {
    if (initialize) {
      data = V8::ArrayBufferAllocator()->Allocate(allocated_length);
    } else {
      data =
          V8::ArrayBufferAllocator()->AllocateUninitialized(allocated_length);
    }
    if (data == NULL) return false;
  } else {
    data = NULL;
  }

  SetupArrayBuffer(isolate, array_buffer, false, data, allocated_length);

  isolate->heap()->AdjustAmountOfExternalAllocatedMemory(allocated_length);

  return true;
}


void Runtime::NeuterArrayBuffer(Handle<JSArrayBuffer> array_buffer) {
  Isolate* isolate = array_buffer->GetIsolate();
  for (Handle<Object> view_obj(array_buffer->weak_first_view(), isolate);
       !view_obj->IsUndefined();) {
    Handle<JSArrayBufferView> view(JSArrayBufferView::cast(*view_obj));
    if (view->IsJSTypedArray()) {
      JSTypedArray::cast(*view)->Neuter();
    } else if (view->IsJSDataView()) {
      JSDataView::cast(*view)->Neuter();
    } else {
      UNREACHABLE();
    }
    view_obj = handle(view->weak_next(), isolate);
  }
  array_buffer->Neuter();
}


RUNTIME_FUNCTION(Runtime_ArrayBufferInitialize) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSArrayBuffer, holder, 0);
  CONVERT_NUMBER_ARG_HANDLE_CHECKED(byteLength, 1);
  if (!holder->byte_length()->IsUndefined()) {
    // ArrayBuffer is already initialized; probably a fuzz test.
    return *holder;
  }
  size_t allocated_length = 0;
  if (!TryNumberToSize(isolate, *byteLength, &allocated_length)) {
    return isolate->Throw(
        *isolate->factory()->NewRangeError("invalid_array_buffer_length",
                                           HandleVector<Object>(NULL, 0)));
  }
  if (!Runtime::SetupArrayBufferAllocatingData(isolate,
                                               holder, allocated_length)) {
    return isolate->Throw(
        *isolate->factory()->NewRangeError("invalid_array_buffer_length",
                                           HandleVector<Object>(NULL, 0)));
  }
  return *holder;
}


RUNTIME_FUNCTION(Runtime_ArrayBufferGetByteLength) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_CHECKED(JSArrayBuffer, holder, 0);
  return holder->byte_length();
}


RUNTIME_FUNCTION(Runtime_ArrayBufferSliceImpl) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 3);
  CONVERT_ARG_HANDLE_CHECKED(JSArrayBuffer, source, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSArrayBuffer, target, 1);
  CONVERT_NUMBER_ARG_HANDLE_CHECKED(first, 2);
  size_t start = 0;
  RUNTIME_ASSERT(TryNumberToSize(isolate, *first, &start));
  size_t target_length = NumberToSize(isolate, target->byte_length());

  if (target_length == 0) return isolate->heap()->undefined_value();

  size_t source_byte_length = NumberToSize(isolate, source->byte_length());
  RUNTIME_ASSERT(start <= source_byte_length);
  RUNTIME_ASSERT(source_byte_length - start >= target_length);
  uint8_t* source_data = reinterpret_cast<uint8_t*>(source->backing_store());
  uint8_t* target_data = reinterpret_cast<uint8_t*>(target->backing_store());
  CopyBytes(target_data, source_data + start, target_length);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_ArrayBufferIsView) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_CHECKED(Object, object, 0);
  return isolate->heap()->ToBoolean(object->IsJSArrayBufferView());
}


RUNTIME_FUNCTION(Runtime_ArrayBufferNeuter) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSArrayBuffer, array_buffer, 0);
  if (array_buffer->backing_store() == NULL) {
    CHECK(Smi::FromInt(0) == array_buffer->byte_length());
    return isolate->heap()->undefined_value();
  }
  ASSERT(!array_buffer->is_external());
  void* backing_store = array_buffer->backing_store();
  size_t byte_length = NumberToSize(isolate, array_buffer->byte_length());
  array_buffer->set_is_external(true);
  Runtime::NeuterArrayBuffer(array_buffer);
  V8::ArrayBufferAllocator()->Free(backing_store, byte_length);
  return isolate->heap()->undefined_value();
}


void Runtime::ArrayIdToTypeAndSize(
    int arrayId,
    ExternalArrayType* array_type,
    ElementsKind* external_elements_kind,
    ElementsKind* fixed_elements_kind,
    size_t* element_size) {
  switch (arrayId) {
#define ARRAY_ID_CASE(Type, type, TYPE, ctype, size)                           \
    case ARRAY_ID_##TYPE:                                                      \
      *array_type = kExternal##Type##Array;                                    \
      *external_elements_kind = EXTERNAL_##TYPE##_ELEMENTS;                    \
      *fixed_elements_kind = TYPE##_ELEMENTS;                                  \
      *element_size = size;                                                    \
      break;

    TYPED_ARRAYS(ARRAY_ID_CASE)
#undef ARRAY_ID_CASE

    default:
      UNREACHABLE();
  }
}


RUNTIME_FUNCTION(Runtime_TypedArrayInitialize) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 5);
  CONVERT_ARG_HANDLE_CHECKED(JSTypedArray, holder, 0);
  CONVERT_SMI_ARG_CHECKED(arrayId, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, maybe_buffer, 2);
  CONVERT_NUMBER_ARG_HANDLE_CHECKED(byte_offset_object, 3);
  CONVERT_NUMBER_ARG_HANDLE_CHECKED(byte_length_object, 4);

  RUNTIME_ASSERT(arrayId >= Runtime::ARRAY_ID_FIRST &&
                 arrayId <= Runtime::ARRAY_ID_LAST);
  RUNTIME_ASSERT(maybe_buffer->IsNull() || maybe_buffer->IsJSArrayBuffer());

  ASSERT(holder->GetInternalFieldCount() ==
      v8::ArrayBufferView::kInternalFieldCount);
  for (int i = 0; i < v8::ArrayBufferView::kInternalFieldCount; i++) {
    holder->SetInternalField(i, Smi::FromInt(0));
  }

  ExternalArrayType array_type = kExternalInt8Array;  // Bogus initialization.
  size_t element_size = 1;  // Bogus initialization.
  ElementsKind external_elements_kind =
      EXTERNAL_INT8_ELEMENTS;  // Bogus initialization.
  ElementsKind fixed_elements_kind = INT8_ELEMENTS;  // Bogus initialization.
  Runtime::ArrayIdToTypeAndSize(arrayId,
      &array_type,
      &external_elements_kind,
      &fixed_elements_kind,
      &element_size);

  size_t byte_offset = 0;
  size_t byte_length = 0;
  RUNTIME_ASSERT(TryNumberToSize(isolate, *byte_offset_object, &byte_offset));
  RUNTIME_ASSERT(TryNumberToSize(isolate, *byte_length_object, &byte_length));

  holder->set_byte_offset(*byte_offset_object);
  holder->set_byte_length(*byte_length_object);

  CHECK_EQ(0, static_cast<int>(byte_length % element_size));
  size_t length = byte_length / element_size;

  if (length > static_cast<unsigned>(Smi::kMaxValue)) {
    return isolate->Throw(
        *isolate->factory()->NewRangeError("invalid_typed_array_length",
                                           HandleVector<Object>(NULL, 0)));
  }

  Handle<Object> length_obj = isolate->factory()->NewNumberFromSize(length);
  holder->set_length(*length_obj);
  if (!maybe_buffer->IsNull()) {
    Handle<JSArrayBuffer> buffer(JSArrayBuffer::cast(*maybe_buffer));

    size_t array_buffer_byte_length =
        NumberToSize(isolate, buffer->byte_length());
    RUNTIME_ASSERT(byte_offset <= array_buffer_byte_length);
    RUNTIME_ASSERT(array_buffer_byte_length - byte_offset >= byte_length);

    holder->set_buffer(*buffer);
    holder->set_weak_next(buffer->weak_first_view());
    buffer->set_weak_first_view(*holder);

    Handle<ExternalArray> elements =
        isolate->factory()->NewExternalArray(
            static_cast<int>(length), array_type,
            static_cast<uint8_t*>(buffer->backing_store()) + byte_offset);
    Handle<Map> map =
        JSObject::GetElementsTransitionMap(holder, external_elements_kind);
    JSObject::SetMapAndElements(holder, map, elements);
    ASSERT(IsExternalArrayElementsKind(holder->map()->elements_kind()));
  } else {
    holder->set_buffer(Smi::FromInt(0));
    holder->set_weak_next(isolate->heap()->undefined_value());
    Handle<FixedTypedArrayBase> elements =
        isolate->factory()->NewFixedTypedArray(
            static_cast<int>(length), array_type);
    holder->set_elements(*elements);
  }
  return isolate->heap()->undefined_value();
}


// Initializes a typed array from an array-like object.
// If an array-like object happens to be a typed array of the same type,
// initializes backing store using memove.
//
// Returns true if backing store was initialized or false otherwise.
RUNTIME_FUNCTION(Runtime_TypedArrayInitializeFromArrayLike) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 4);
  CONVERT_ARG_HANDLE_CHECKED(JSTypedArray, holder, 0);
  CONVERT_SMI_ARG_CHECKED(arrayId, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, source, 2);
  CONVERT_NUMBER_ARG_HANDLE_CHECKED(length_obj, 3);

  RUNTIME_ASSERT(arrayId >= Runtime::ARRAY_ID_FIRST &&
                 arrayId <= Runtime::ARRAY_ID_LAST);

  ASSERT(holder->GetInternalFieldCount() ==
      v8::ArrayBufferView::kInternalFieldCount);
  for (int i = 0; i < v8::ArrayBufferView::kInternalFieldCount; i++) {
    holder->SetInternalField(i, Smi::FromInt(0));
  }

  ExternalArrayType array_type = kExternalInt8Array;  // Bogus initialization.
  size_t element_size = 1;  // Bogus initialization.
  ElementsKind external_elements_kind =
      EXTERNAL_INT8_ELEMENTS;  // Bogus intialization.
  ElementsKind fixed_elements_kind = INT8_ELEMENTS;  // Bogus initialization.
  Runtime::ArrayIdToTypeAndSize(arrayId,
      &array_type,
      &external_elements_kind,
      &fixed_elements_kind,
      &element_size);

  Handle<JSArrayBuffer> buffer = isolate->factory()->NewJSArrayBuffer();
  if (source->IsJSTypedArray() &&
      JSTypedArray::cast(*source)->type() == array_type) {
    length_obj = Handle<Object>(JSTypedArray::cast(*source)->length(), isolate);
  }
  size_t length = 0;
  RUNTIME_ASSERT(TryNumberToSize(isolate, *length_obj, &length));

  if ((length > static_cast<unsigned>(Smi::kMaxValue)) ||
      (length > (kMaxInt / element_size))) {
    return isolate->Throw(*isolate->factory()->
          NewRangeError("invalid_typed_array_length",
            HandleVector<Object>(NULL, 0)));
  }
  size_t byte_length = length * element_size;

  // NOTE: not initializing backing store.
  // We assume that the caller of this function will initialize holder
  // with the loop
  //      for(i = 0; i < length; i++) { holder[i] = source[i]; }
  // We assume that the caller of this function is always a typed array
  // constructor.
  // If source is a typed array, this loop will always run to completion,
  // so we are sure that the backing store will be initialized.
  // Otherwise, the indexing operation might throw, so the loop will not
  // run to completion and the typed array might remain partly initialized.
  // However we further assume that the caller of this function is a typed array
  // constructor, and the exception will propagate out of the constructor,
  // therefore uninitialized memory will not be accessible by a user program.
  //
  // TODO(dslomov): revise this once we support subclassing.

  if (!Runtime::SetupArrayBufferAllocatingData(
        isolate, buffer, byte_length, false)) {
    return isolate->Throw(*isolate->factory()->
          NewRangeError("invalid_array_buffer_length",
            HandleVector<Object>(NULL, 0)));
  }

  holder->set_buffer(*buffer);
  holder->set_byte_offset(Smi::FromInt(0));
  Handle<Object> byte_length_obj(
      isolate->factory()->NewNumberFromSize(byte_length));
  holder->set_byte_length(*byte_length_obj);
  holder->set_length(*length_obj);
  holder->set_weak_next(buffer->weak_first_view());
  buffer->set_weak_first_view(*holder);

  Handle<ExternalArray> elements =
      isolate->factory()->NewExternalArray(
          static_cast<int>(length), array_type,
          static_cast<uint8_t*>(buffer->backing_store()));
  Handle<Map> map = JSObject::GetElementsTransitionMap(
      holder, external_elements_kind);
  JSObject::SetMapAndElements(holder, map, elements);

  if (source->IsJSTypedArray()) {
    Handle<JSTypedArray> typed_array(JSTypedArray::cast(*source));

    if (typed_array->type() == holder->type()) {
      uint8_t* backing_store =
        static_cast<uint8_t*>(
          typed_array->GetBuffer()->backing_store());
      size_t source_byte_offset =
          NumberToSize(isolate, typed_array->byte_offset());
      memcpy(
          buffer->backing_store(),
          backing_store + source_byte_offset,
          byte_length);
      return isolate->heap()->true_value();
    }
  }

  return isolate->heap()->false_value();
}


#define BUFFER_VIEW_GETTER(Type, getter, accessor) \
  RUNTIME_FUNCTION(Runtime_##Type##Get##getter) {                    \
    HandleScope scope(isolate);                                               \
    ASSERT(args.length() == 1);                                               \
    CONVERT_ARG_HANDLE_CHECKED(JS##Type, holder, 0);                          \
    return holder->accessor();                                                \
  }

BUFFER_VIEW_GETTER(ArrayBufferView, ByteLength, byte_length)
BUFFER_VIEW_GETTER(ArrayBufferView, ByteOffset, byte_offset)
BUFFER_VIEW_GETTER(TypedArray, Length, length)
BUFFER_VIEW_GETTER(DataView, Buffer, buffer)

#undef BUFFER_VIEW_GETTER

RUNTIME_FUNCTION(Runtime_TypedArrayGetBuffer) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSTypedArray, holder, 0);
  return *holder->GetBuffer();
}


// Return codes for Runtime_TypedArraySetFastCases.
// Should be synchronized with typedarray.js natives.
enum TypedArraySetResultCodes {
  // Set from typed array of the same type.
  // This is processed by TypedArraySetFastCases
  TYPED_ARRAY_SET_TYPED_ARRAY_SAME_TYPE = 0,
  // Set from typed array of the different type, overlapping in memory.
  TYPED_ARRAY_SET_TYPED_ARRAY_OVERLAPPING = 1,
  // Set from typed array of the different type, non-overlapping.
  TYPED_ARRAY_SET_TYPED_ARRAY_NONOVERLAPPING = 2,
  // Set from non-typed array.
  TYPED_ARRAY_SET_NON_TYPED_ARRAY = 3
};


RUNTIME_FUNCTION(Runtime_TypedArraySetFastCases) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 3);
  if (!args[0]->IsJSTypedArray())
    return isolate->Throw(*isolate->factory()->NewTypeError(
        "not_typed_array", HandleVector<Object>(NULL, 0)));

  if (!args[1]->IsJSTypedArray())
    return Smi::FromInt(TYPED_ARRAY_SET_NON_TYPED_ARRAY);

  CONVERT_ARG_HANDLE_CHECKED(JSTypedArray, target_obj, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSTypedArray, source_obj, 1);
  CONVERT_NUMBER_ARG_HANDLE_CHECKED(offset_obj, 2);

  Handle<JSTypedArray> target(JSTypedArray::cast(*target_obj));
  Handle<JSTypedArray> source(JSTypedArray::cast(*source_obj));
  size_t offset = 0;
  RUNTIME_ASSERT(TryNumberToSize(isolate, *offset_obj, &offset));
  size_t target_length = NumberToSize(isolate, target->length());
  size_t source_length = NumberToSize(isolate, source->length());
  size_t target_byte_length = NumberToSize(isolate, target->byte_length());
  size_t source_byte_length = NumberToSize(isolate, source->byte_length());
  if (offset > target_length ||
      offset + source_length > target_length ||
      offset + source_length < offset)  // overflow
    return isolate->Throw(*isolate->factory()->NewRangeError(
          "typed_array_set_source_too_large", HandleVector<Object>(NULL, 0)));

  size_t target_offset = NumberToSize(isolate, target->byte_offset());
  size_t source_offset = NumberToSize(isolate, source->byte_offset());
  uint8_t* target_base =
      static_cast<uint8_t*>(
        target->GetBuffer()->backing_store()) + target_offset;
  uint8_t* source_base =
      static_cast<uint8_t*>(
        source->GetBuffer()->backing_store()) + source_offset;

  // Typed arrays of the same type: use memmove.
  if (target->type() == source->type()) {
    memmove(target_base + offset * target->element_size(),
        source_base, source_byte_length);
    return Smi::FromInt(TYPED_ARRAY_SET_TYPED_ARRAY_SAME_TYPE);
  }

  // Typed arrays of different types over the same backing store
  if ((source_base <= target_base &&
        source_base + source_byte_length > target_base) ||
      (target_base <= source_base &&
        target_base + target_byte_length > source_base)) {
    // We do not support overlapping ArrayBuffers
    ASSERT(
      target->GetBuffer()->backing_store() ==
      source->GetBuffer()->backing_store());
    return Smi::FromInt(TYPED_ARRAY_SET_TYPED_ARRAY_OVERLAPPING);
  } else {  // Non-overlapping typed arrays
    return Smi::FromInt(TYPED_ARRAY_SET_TYPED_ARRAY_NONOVERLAPPING);
  }
}


RUNTIME_FUNCTION(Runtime_TypedArrayMaxSizeInHeap) {
  ASSERT(args.length() == 0);
  ASSERT_OBJECT_SIZE(
      FLAG_typed_array_max_size_in_heap + FixedTypedArrayBase::kDataOffset);
  return Smi::FromInt(FLAG_typed_array_max_size_in_heap);
}


RUNTIME_FUNCTION(Runtime_DataViewInitialize) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 4);
  CONVERT_ARG_HANDLE_CHECKED(JSDataView, holder, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSArrayBuffer, buffer, 1);
  CONVERT_NUMBER_ARG_HANDLE_CHECKED(byte_offset, 2);
  CONVERT_NUMBER_ARG_HANDLE_CHECKED(byte_length, 3);

  ASSERT(holder->GetInternalFieldCount() ==
      v8::ArrayBufferView::kInternalFieldCount);
  for (int i = 0; i < v8::ArrayBufferView::kInternalFieldCount; i++) {
    holder->SetInternalField(i, Smi::FromInt(0));
  }
  size_t buffer_length = 0;
  size_t offset = 0;
  size_t length = 0;
  RUNTIME_ASSERT(
      TryNumberToSize(isolate, buffer->byte_length(), &buffer_length));
  RUNTIME_ASSERT(TryNumberToSize(isolate, *byte_offset, &offset));
  RUNTIME_ASSERT(TryNumberToSize(isolate, *byte_length, &length));

  // TODO(jkummerow): When we have a "safe numerics" helper class, use it here.
  // Entire range [offset, offset + length] must be in bounds.
  RUNTIME_ASSERT(offset <= buffer_length);
  RUNTIME_ASSERT(offset + length <= buffer_length);
  // No overflow.
  RUNTIME_ASSERT(offset + length >= offset);

  holder->set_buffer(*buffer);
  holder->set_byte_offset(*byte_offset);
  holder->set_byte_length(*byte_length);

  holder->set_weak_next(buffer->weak_first_view());
  buffer->set_weak_first_view(*holder);

  return isolate->heap()->undefined_value();
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
  size_t byte_offset = 0;
  if (!TryNumberToSize(isolate, *byte_offset_obj, &byte_offset)) {
    return false;
  }
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
  size_t byte_offset = 0;
  if (!TryNumberToSize(isolate, *byte_offset_obj, &byte_offset)) {
    return false;
  }
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
  RUNTIME_FUNCTION(Runtime_DataViewGet##TypeName) {                  \
    HandleScope scope(isolate);                                               \
    ASSERT(args.length() == 3);                                               \
    CONVERT_ARG_HANDLE_CHECKED(JSDataView, holder, 0);                        \
    CONVERT_ARG_HANDLE_CHECKED(Object, offset, 1);                            \
    CONVERT_BOOLEAN_ARG_CHECKED(is_little_endian, 2);                         \
    Type result;                                                              \
    if (DataViewGetValue(                                                     \
          isolate, holder, offset, is_little_endian, &result)) {              \
      return *isolate->factory()->Converter(result);                          \
    } else {                                                                  \
      return isolate->Throw(*isolate->factory()->NewRangeError(               \
          "invalid_data_view_accessor_offset",                                \
          HandleVector<Object>(NULL, 0)));                                    \
    }                                                                         \
  }

DATA_VIEW_GETTER(Uint8, uint8_t, NewNumberFromUint)
DATA_VIEW_GETTER(Int8, int8_t, NewNumberFromInt)
DATA_VIEW_GETTER(Uint16, uint16_t, NewNumberFromUint)
DATA_VIEW_GETTER(Int16, int16_t, NewNumberFromInt)
DATA_VIEW_GETTER(Uint32, uint32_t, NewNumberFromUint)
DATA_VIEW_GETTER(Int32, int32_t, NewNumberFromInt)
DATA_VIEW_GETTER(Float32, float, NewNumber)
DATA_VIEW_GETTER(Float64, double, NewNumber)

#undef DATA_VIEW_GETTER


template <typename T>
static T DataViewConvertValue(double value);


template <>
int8_t DataViewConvertValue<int8_t>(double value) {
  return static_cast<int8_t>(DoubleToInt32(value));
}


template <>
int16_t DataViewConvertValue<int16_t>(double value) {
  return static_cast<int16_t>(DoubleToInt32(value));
}


template <>
int32_t DataViewConvertValue<int32_t>(double value) {
  return DoubleToInt32(value);
}


template <>
uint8_t DataViewConvertValue<uint8_t>(double value) {
  return static_cast<uint8_t>(DoubleToUint32(value));
}


template <>
uint16_t DataViewConvertValue<uint16_t>(double value) {
  return static_cast<uint16_t>(DoubleToUint32(value));
}


template <>
uint32_t DataViewConvertValue<uint32_t>(double value) {
  return DoubleToUint32(value);
}


template <>
float DataViewConvertValue<float>(double value) {
  return static_cast<float>(value);
}


template <>
double DataViewConvertValue<double>(double value) {
  return value;
}


#define DATA_VIEW_SETTER(TypeName, Type)                                      \
  RUNTIME_FUNCTION(Runtime_DataViewSet##TypeName) {                  \
    HandleScope scope(isolate);                                               \
    ASSERT(args.length() == 4);                                               \
    CONVERT_ARG_HANDLE_CHECKED(JSDataView, holder, 0);                        \
    CONVERT_ARG_HANDLE_CHECKED(Object, offset, 1);                            \
    CONVERT_ARG_HANDLE_CHECKED(Object, value, 2);                             \
    CONVERT_BOOLEAN_ARG_CHECKED(is_little_endian, 3);                         \
    Type v = DataViewConvertValue<Type>(value->Number());                     \
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


RUNTIME_FUNCTION(Runtime_SetInitialize) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSSet, holder, 0);
  Handle<OrderedHashSet> table = isolate->factory()->NewOrderedHashSet();
  holder->set_table(*table);
  return *holder;
}


RUNTIME_FUNCTION(Runtime_SetAdd) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSSet, holder, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 1);
  Handle<OrderedHashSet> table(OrderedHashSet::cast(holder->table()));
  table = OrderedHashSet::Add(table, key);
  holder->set_table(*table);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_SetHas) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSSet, holder, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 1);
  Handle<OrderedHashSet> table(OrderedHashSet::cast(holder->table()));
  return isolate->heap()->ToBoolean(table->Contains(key));
}


RUNTIME_FUNCTION(Runtime_SetDelete) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSSet, holder, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 1);
  Handle<OrderedHashSet> table(OrderedHashSet::cast(holder->table()));
  table = OrderedHashSet::Remove(table, key);
  holder->set_table(*table);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_SetClear) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSSet, holder, 0);
  Handle<OrderedHashSet> table(OrderedHashSet::cast(holder->table()));
  table = OrderedHashSet::Clear(table);
  holder->set_table(*table);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_SetGetSize) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSSet, holder, 0);
  Handle<OrderedHashSet> table(OrderedHashSet::cast(holder->table()));
  return Smi::FromInt(table->NumberOfElements());
}


RUNTIME_FUNCTION(Runtime_SetCreateIterator) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSSet, holder, 0);
  CONVERT_SMI_ARG_CHECKED(kind, 1)
  RUNTIME_ASSERT(kind == JSSetIterator::kKindValues ||
                 kind == JSSetIterator::kKindEntries);
  Handle<OrderedHashSet> table(OrderedHashSet::cast(holder->table()));
  return *JSSetIterator::Create(table, kind);
}


RUNTIME_FUNCTION(Runtime_SetIteratorNext) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSSetIterator, holder, 0);
  return *JSSetIterator::Next(holder);
}


RUNTIME_FUNCTION(Runtime_SetIteratorClose) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSSetIterator, holder, 0);
  holder->Close();
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_MapInitialize) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSMap, holder, 0);
  Handle<OrderedHashMap> table = isolate->factory()->NewOrderedHashMap();
  holder->set_table(*table);
  return *holder;
}


RUNTIME_FUNCTION(Runtime_MapGet) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSMap, holder, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 1);
  Handle<OrderedHashMap> table(OrderedHashMap::cast(holder->table()));
  Handle<Object> lookup(table->Lookup(key), isolate);
  return lookup->IsTheHole() ? isolate->heap()->undefined_value() : *lookup;
}


RUNTIME_FUNCTION(Runtime_MapHas) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSMap, holder, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 1);
  Handle<OrderedHashMap> table(OrderedHashMap::cast(holder->table()));
  Handle<Object> lookup(table->Lookup(key), isolate);
  return isolate->heap()->ToBoolean(!lookup->IsTheHole());
}


RUNTIME_FUNCTION(Runtime_MapDelete) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSMap, holder, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 1);
  Handle<OrderedHashMap> table(OrderedHashMap::cast(holder->table()));
  Handle<Object> lookup(table->Lookup(key), isolate);
  Handle<OrderedHashMap> new_table =
      OrderedHashMap::Put(table, key, isolate->factory()->the_hole_value());
  holder->set_table(*new_table);
  return isolate->heap()->ToBoolean(!lookup->IsTheHole());
}


RUNTIME_FUNCTION(Runtime_MapClear) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSMap, holder, 0);
  Handle<OrderedHashMap> table(OrderedHashMap::cast(holder->table()));
  table = OrderedHashMap::Clear(table);
  holder->set_table(*table);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_MapSet) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 3);
  CONVERT_ARG_HANDLE_CHECKED(JSMap, holder, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 2);
  Handle<OrderedHashMap> table(OrderedHashMap::cast(holder->table()));
  Handle<OrderedHashMap> new_table = OrderedHashMap::Put(table, key, value);
  holder->set_table(*new_table);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_MapGetSize) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSMap, holder, 0);
  Handle<OrderedHashMap> table(OrderedHashMap::cast(holder->table()));
  return Smi::FromInt(table->NumberOfElements());
}


RUNTIME_FUNCTION(Runtime_MapCreateIterator) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSMap, holder, 0);
  CONVERT_SMI_ARG_CHECKED(kind, 1)
  RUNTIME_ASSERT(kind == JSMapIterator::kKindKeys
      || kind == JSMapIterator::kKindValues
      || kind == JSMapIterator::kKindEntries);
  Handle<OrderedHashMap> table(OrderedHashMap::cast(holder->table()));
  return *JSMapIterator::Create(table, kind);
}


RUNTIME_FUNCTION(Runtime_MapIteratorNext) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSMapIterator, holder, 0);
  return *JSMapIterator::Next(holder);
}


RUNTIME_FUNCTION(Runtime_MapIteratorClose) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSMapIterator, holder, 0);
  holder->Close();
  return isolate->heap()->undefined_value();
}


static Handle<JSWeakCollection> WeakCollectionInitialize(
    Isolate* isolate,
    Handle<JSWeakCollection> weak_collection) {
  ASSERT(weak_collection->map()->inobject_properties() == 0);
  Handle<ObjectHashTable> table = ObjectHashTable::New(isolate, 0);
  weak_collection->set_table(*table);
  weak_collection->set_next(Smi::FromInt(0));
  return weak_collection;
}


RUNTIME_FUNCTION(Runtime_WeakCollectionInitialize) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSWeakCollection, weak_collection, 0);
  return *WeakCollectionInitialize(isolate, weak_collection);
}


RUNTIME_FUNCTION(Runtime_WeakCollectionGet) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSWeakCollection, weak_collection, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 1);
  Handle<ObjectHashTable> table(
      ObjectHashTable::cast(weak_collection->table()));
  Handle<Object> lookup(table->Lookup(key), isolate);
  return lookup->IsTheHole() ? isolate->heap()->undefined_value() : *lookup;
}


RUNTIME_FUNCTION(Runtime_WeakCollectionHas) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSWeakCollection, weak_collection, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 1);
  Handle<ObjectHashTable> table(
      ObjectHashTable::cast(weak_collection->table()));
  Handle<Object> lookup(table->Lookup(key), isolate);
  return isolate->heap()->ToBoolean(!lookup->IsTheHole());
}


RUNTIME_FUNCTION(Runtime_WeakCollectionDelete) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSWeakCollection, weak_collection, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 1);
  Handle<ObjectHashTable> table(ObjectHashTable::cast(
      weak_collection->table()));
  Handle<Object> lookup(table->Lookup(key), isolate);
  Handle<ObjectHashTable> new_table =
      ObjectHashTable::Put(table, key, isolate->factory()->the_hole_value());
  weak_collection->set_table(*new_table);
  return isolate->heap()->ToBoolean(!lookup->IsTheHole());
}


RUNTIME_FUNCTION(Runtime_WeakCollectionSet) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 3);
  CONVERT_ARG_HANDLE_CHECKED(JSWeakCollection, weak_collection, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 2);
  Handle<ObjectHashTable> table(
      ObjectHashTable::cast(weak_collection->table()));
  Handle<ObjectHashTable> new_table = ObjectHashTable::Put(table, key, value);
  weak_collection->set_table(*new_table);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_ClassOf) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_CHECKED(Object, obj, 0);
  if (!obj->IsJSObject()) return isolate->heap()->null_value();
  return JSObject::cast(obj)->class_name();
}


RUNTIME_FUNCTION(Runtime_GetPrototype) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, obj, 0);
  // We don't expect access checks to be needed on JSProxy objects.
  ASSERT(!obj->IsAccessCheckNeeded() || obj->IsJSObject());
  do {
    if (obj->IsAccessCheckNeeded() &&
        !isolate->MayNamedAccess(Handle<JSObject>::cast(obj),
                                 isolate->factory()->proto_string(),
                                 v8::ACCESS_GET)) {
      isolate->ReportFailedAccessCheck(Handle<JSObject>::cast(obj),
                                       v8::ACCESS_GET);
      RETURN_FAILURE_IF_SCHEDULED_EXCEPTION(isolate);
      return isolate->heap()->undefined_value();
    }
    obj = Object::GetPrototype(isolate, obj);
  } while (obj->IsJSObject() &&
           JSObject::cast(*obj)->map()->is_hidden_prototype());
  return *obj;
}


static inline Handle<Object> GetPrototypeSkipHiddenPrototypes(
    Isolate* isolate, Handle<Object> receiver) {
  Handle<Object> current = Object::GetPrototype(isolate, receiver);
  while (current->IsJSObject() &&
         JSObject::cast(*current)->map()->is_hidden_prototype()) {
    current = Object::GetPrototype(isolate, current);
  }
  return current;
}


RUNTIME_FUNCTION(Runtime_SetPrototype) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, obj, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, prototype, 1);
  if (obj->IsAccessCheckNeeded() &&
      !isolate->MayNamedAccess(
          obj, isolate->factory()->proto_string(), v8::ACCESS_SET)) {
    isolate->ReportFailedAccessCheck(obj, v8::ACCESS_SET);
    RETURN_FAILURE_IF_SCHEDULED_EXCEPTION(isolate);
    return isolate->heap()->undefined_value();
  }
  if (obj->map()->is_observed()) {
    Handle<Object> old_value = GetPrototypeSkipHiddenPrototypes(isolate, obj);
    Handle<Object> result;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, result,
        JSObject::SetPrototype(obj, prototype, true));

    Handle<Object> new_value = GetPrototypeSkipHiddenPrototypes(isolate, obj);
    if (!new_value->SameValue(*old_value)) {
      JSObject::EnqueueChangeRecord(obj, "setPrototype",
                                    isolate->factory()->proto_string(),
                                    old_value);
    }
    return *result;
  }
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      JSObject::SetPrototype(obj, prototype, true));
  return *result;
}


RUNTIME_FUNCTION(Runtime_IsInPrototypeChain) {
  HandleScope shs(isolate);
  ASSERT(args.length() == 2);
  // See ECMA-262, section 15.3.5.3, page 88 (steps 5 - 8).
  CONVERT_ARG_HANDLE_CHECKED(Object, O, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, V, 1);
  while (true) {
    Handle<Object> prototype = Object::GetPrototype(isolate, V);
    if (prototype->IsNull()) return isolate->heap()->false_value();
    if (*O == *prototype) return isolate->heap()->true_value();
    V = prototype;
  }
}


static bool CheckAccessException(Object* callback,
                                 v8::AccessType access_type) {
  DisallowHeapAllocation no_gc;
  ASSERT(!callback->IsForeign());
  if (callback->IsAccessorInfo()) {
    AccessorInfo* info = AccessorInfo::cast(callback);
    return
        (access_type == v8::ACCESS_HAS &&
           (info->all_can_read() || info->all_can_write())) ||
        (access_type == v8::ACCESS_GET && info->all_can_read()) ||
        (access_type == v8::ACCESS_SET && info->all_can_write());
  }
  if (callback->IsAccessorPair()) {
    AccessorPair* info = AccessorPair::cast(callback);
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
    Handle<JSObject> receiver,
    Handle<JSObject> holder,
    Key key,
    v8::AccessType access_type,
    bool (Isolate::*mayAccess)(Handle<JSObject>, Key, v8::AccessType)) {
  Isolate* isolate = receiver->GetIsolate();
  for (Handle<JSObject> current = receiver;
       true;
       current = handle(JSObject::cast(current->GetPrototype()), isolate)) {
    if (current->IsAccessCheckNeeded() &&
        !(isolate->*mayAccess)(current, key, access_type)) {
      return false;
    }
    if (current.is_identical_to(holder)) break;
  }
  return true;
}


enum AccessCheckResult {
  ACCESS_FORBIDDEN,
  ACCESS_ALLOWED,
  ACCESS_ABSENT
};


static AccessCheckResult CheckPropertyAccess(Handle<JSObject> obj,
                                             Handle<Name> name,
                                             v8::AccessType access_type) {
  uint32_t index;
  if (name->AsArrayIndex(&index)) {
    // TODO(1095): we should traverse hidden prototype hierachy as well.
    if (CheckGenericAccess(
            obj, obj, index, access_type, &Isolate::MayIndexedAccess)) {
      return ACCESS_ALLOWED;
    }

    obj->GetIsolate()->ReportFailedAccessCheck(obj, access_type);
    return ACCESS_FORBIDDEN;
  }

  Isolate* isolate = obj->GetIsolate();
  LookupResult lookup(isolate);
  obj->LocalLookup(name, &lookup, true);

  if (!lookup.IsProperty()) return ACCESS_ABSENT;
  Handle<JSObject> holder(lookup.holder(), isolate);
  if (CheckGenericAccess<Handle<Object> >(
          obj, holder, name, access_type, &Isolate::MayNamedAccess)) {
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
      holder->LookupRealNamedProperty(name, &lookup);
      if (lookup.IsProperty() && lookup.IsPropertyCallbacks()) {
        if (CheckAccessException(lookup.GetCallbackObject(), access_type)) {
          return ACCESS_ALLOWED;
        }
      }
      break;
    default:
      break;
  }

  isolate->ReportFailedAccessCheck(obj, access_type);
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


MUST_USE_RESULT static MaybeHandle<Object> GetOwnProperty(Isolate* isolate,
                                                          Handle<JSObject> obj,
                                                          Handle<Name> name) {
  Heap* heap = isolate->heap();
  Factory* factory = isolate->factory();
  // Due to some WebKit tests, we want to make sure that we do not log
  // more than one access failure here.
  AccessCheckResult access_check_result =
      CheckPropertyAccess(obj, name, v8::ACCESS_HAS);
  RETURN_EXCEPTION_IF_SCHEDULED_EXCEPTION(isolate, Object);
  switch (access_check_result) {
    case ACCESS_FORBIDDEN: return factory->false_value();
    case ACCESS_ALLOWED: break;
    case ACCESS_ABSENT: return factory->undefined_value();
  }

  PropertyAttributes attrs = JSReceiver::GetLocalPropertyAttribute(obj, name);
  if (attrs == ABSENT) {
    RETURN_EXCEPTION_IF_SCHEDULED_EXCEPTION(isolate, Object);
    return factory->undefined_value();
  }
  ASSERT(!isolate->has_scheduled_exception());
  Handle<AccessorPair> accessors;
  bool has_accessors =
      JSObject::GetLocalPropertyAccessorPair(obj, name).ToHandle(&accessors);
  Handle<FixedArray> elms = isolate->factory()->NewFixedArray(DESCRIPTOR_SIZE);
  elms->set(ENUMERABLE_INDEX, heap->ToBoolean((attrs & DONT_ENUM) == 0));
  elms->set(CONFIGURABLE_INDEX, heap->ToBoolean((attrs & DONT_DELETE) == 0));
  elms->set(IS_ACCESSOR_INDEX, heap->ToBoolean(has_accessors));

  if (!has_accessors) {
    elms->set(WRITABLE_INDEX, heap->ToBoolean((attrs & READ_ONLY) == 0));
    // Runtime::GetObjectProperty does access check.
    Handle<Object> value;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, value, Runtime::GetObjectProperty(isolate, obj, name),
        Object);
    elms->set(VALUE_INDEX, *value);
  } else {
    // Access checks are performed for both accessors separately.
    // When they fail, the respective field is not set in the descriptor.
    Handle<Object> getter(accessors->GetComponent(ACCESSOR_GETTER), isolate);
    Handle<Object> setter(accessors->GetComponent(ACCESSOR_SETTER), isolate);

    if (!getter->IsMap() && CheckPropertyAccess(obj, name, v8::ACCESS_GET)) {
      ASSERT(!isolate->has_scheduled_exception());
      elms->set(GETTER_INDEX, *getter);
    } else {
      RETURN_EXCEPTION_IF_SCHEDULED_EXCEPTION(isolate, Object);
    }

    if (!setter->IsMap() && CheckPropertyAccess(obj, name, v8::ACCESS_SET)) {
      ASSERT(!isolate->has_scheduled_exception());
      elms->set(SETTER_INDEX, *setter);
    } else {
      RETURN_EXCEPTION_IF_SCHEDULED_EXCEPTION(isolate, Object);
    }
  }

  return isolate->factory()->NewJSArrayWithElements(elms);
}


// Returns an array with the property description:
//  if args[1] is not a property on args[0]
//          returns undefined
//  if args[1] is a data property on args[0]
//         [false, value, Writeable, Enumerable, Configurable]
//  if args[1] is an accessor on args[0]
//         [true, GetFunction, SetFunction, Enumerable, Configurable]
RUNTIME_FUNCTION(Runtime_GetOwnProperty) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, obj, 0);
  CONVERT_ARG_HANDLE_CHECKED(Name, name, 1);
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result, GetOwnProperty(isolate, obj, name));
  return *result;
}


RUNTIME_FUNCTION(Runtime_PreventExtensions) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, obj, 0);
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result, JSObject::PreventExtensions(obj));
  return *result;
}


RUNTIME_FUNCTION(Runtime_IsExtensible) {
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


RUNTIME_FUNCTION(Runtime_RegExpCompile) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 3);
  CONVERT_ARG_HANDLE_CHECKED(JSRegExp, re, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, pattern, 1);
  CONVERT_ARG_HANDLE_CHECKED(String, flags, 2);
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result, RegExpImpl::Compile(re, pattern, flags));
  return *result;
}


RUNTIME_FUNCTION(Runtime_CreateApiFunction) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(FunctionTemplateInfo, data, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, prototype, 1);
  return *isolate->factory()->CreateApiFunction(data, prototype);
}


RUNTIME_FUNCTION(Runtime_IsTemplate) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, arg, 0);
  bool result = arg->IsObjectTemplateInfo() || arg->IsFunctionTemplateInfo();
  return isolate->heap()->ToBoolean(result);
}


RUNTIME_FUNCTION(Runtime_GetTemplateField) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_CHECKED(HeapObject, templ, 0);
  CONVERT_SMI_ARG_CHECKED(index, 1);
  int offset = index * kPointerSize + HeapObject::kHeaderSize;
  InstanceType type = templ->map()->instance_type();
  RUNTIME_ASSERT(type == FUNCTION_TEMPLATE_INFO_TYPE ||
                 type == OBJECT_TEMPLATE_INFO_TYPE);
  RUNTIME_ASSERT(offset > 0);
  if (type == FUNCTION_TEMPLATE_INFO_TYPE) {
    RUNTIME_ASSERT(offset < FunctionTemplateInfo::kSize);
  } else {
    RUNTIME_ASSERT(offset < ObjectTemplateInfo::kSize);
  }
  return *HeapObject::RawField(templ, offset);
}


RUNTIME_FUNCTION(Runtime_DisableAccessChecks) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(HeapObject, object, 0);
  Handle<Map> old_map(object->map());
  bool needs_access_checks = old_map->is_access_check_needed();
  if (needs_access_checks) {
    // Copy map so it won't interfere constructor's initial map.
    Handle<Map> new_map = Map::Copy(old_map);
    new_map->set_is_access_check_needed(false);
    if (object->IsJSObject()) {
      JSObject::MigrateToMap(Handle<JSObject>::cast(object), new_map);
    } else {
      object->set_map(*new_map);
    }
  }
  return isolate->heap()->ToBoolean(needs_access_checks);
}


RUNTIME_FUNCTION(Runtime_EnableAccessChecks) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(HeapObject, object, 0);
  Handle<Map> old_map(object->map());
  if (!old_map->is_access_check_needed()) {
    // Copy map so it won't interfere constructor's initial map.
    Handle<Map> new_map = Map::Copy(old_map);
    new_map->set_is_access_check_needed(true);
    if (object->IsJSObject()) {
      JSObject::MigrateToMap(Handle<JSObject>::cast(object), new_map);
    } else {
      object->set_map(*new_map);
    }
  }
  return isolate->heap()->undefined_value();
}


// Transform getter or setter into something DefineAccessor can handle.
static Handle<Object> InstantiateAccessorComponent(Isolate* isolate,
                                                   Handle<Object> component) {
  if (component->IsUndefined()) return isolate->factory()->null_value();
  Handle<FunctionTemplateInfo> info =
      Handle<FunctionTemplateInfo>::cast(component);
  return Utils::OpenHandle(*Utils::ToLocal(info)->GetFunction());
}


RUNTIME_FUNCTION(Runtime_SetAccessorProperty) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 6);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Name, name, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, getter, 2);
  CONVERT_ARG_HANDLE_CHECKED(Object, setter, 3);
  CONVERT_SMI_ARG_CHECKED(attribute, 4);
  CONVERT_SMI_ARG_CHECKED(access_control, 5);
  RUNTIME_ASSERT(getter->IsUndefined() || getter->IsFunctionTemplateInfo());
  RUNTIME_ASSERT(setter->IsUndefined() || setter->IsFunctionTemplateInfo());
  RUNTIME_ASSERT(PropertyDetails::AttributesField::is_valid(
      static_cast<PropertyAttributes>(attribute)));
  JSObject::DefineAccessor(object,
                           name,
                           InstantiateAccessorComponent(isolate, getter),
                           InstantiateAccessorComponent(isolate, setter),
                           static_cast<PropertyAttributes>(attribute),
                           static_cast<v8::AccessControl>(access_control));
  return isolate->heap()->undefined_value();
}


static Object* ThrowRedeclarationError(Isolate* isolate, Handle<String> name) {
  HandleScope scope(isolate);
  Handle<Object> args[1] = { name };
  Handle<Object> error = isolate->factory()->NewTypeError(
      "var_redeclaration", HandleVector(args, 1));
  return isolate->Throw(*error);
}


RUNTIME_FUNCTION(RuntimeHidden_DeclareGlobals) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 3);
  Handle<GlobalObject> global = Handle<GlobalObject>(
      isolate->context()->global_object());

  CONVERT_ARG_HANDLE_CHECKED(Context, context, 0);
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
      global->LocalLookup(name, &lookup, true);
      if (lookup.IsFound()) {
        // We found an existing property. Unless it was an interceptor
        // that claims the property is absent, skip this declaration.
        if (!lookup.IsInterceptor()) continue;
        if (JSReceiver::GetPropertyAttribute(global, name) != ABSENT) continue;
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
    global->LocalLookup(name, &lookup, true);

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

    StrictMode strict_mode = DeclareGlobalsStrictMode::decode(flags);

    if (!lookup.IsFound() || is_function) {
      // If the local property exists, check that we can reconfigure it
      // as required for function declarations.
      if (lookup.IsFound() && lookup.IsDontDelete()) {
        if (lookup.IsReadOnly() || lookup.IsDontEnum() ||
            lookup.IsPropertyCallbacks()) {
          return ThrowRedeclarationError(isolate, name);
        }
        // If the existing property is not configurable, keep its attributes.
        attr = lookup.GetAttributes();
      }
      // Define or redefine own property.
      RETURN_FAILURE_ON_EXCEPTION(isolate,
          JSObject::SetLocalPropertyIgnoreAttributes(
              global, name, value, static_cast<PropertyAttributes>(attr)));
    } else {
      // Do a [[Put]] on the existing (own) property.
      RETURN_FAILURE_ON_EXCEPTION(
          isolate,
          JSObject::SetProperty(
              global, name, value, static_cast<PropertyAttributes>(attr),
              strict_mode));
    }
  }

  ASSERT(!isolate->has_pending_exception());
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(RuntimeHidden_DeclareContextSlot) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 4);

  // Declarations are always made in a function or native context.  In the
  // case of eval code, the context passed is the context of the caller,
  // which may be some nested context and not the declaration context.
  CONVERT_ARG_HANDLE_CHECKED(Context, context_arg, 0);
  Handle<Context> context(context_arg->declaration_context());
  CONVERT_ARG_HANDLE_CHECKED(String, name, 1);
  CONVERT_SMI_ARG_CHECKED(mode_arg, 2);
  PropertyAttributes mode = static_cast<PropertyAttributes>(mode_arg);
  RUNTIME_ASSERT(mode == READ_ONLY || mode == NONE);
  CONVERT_ARG_HANDLE_CHECKED(Object, initial_value, 3);

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
      return ThrowRedeclarationError(isolate, name);
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
        RETURN_FAILURE_ON_EXCEPTION(
            isolate,
            JSReceiver::SetProperty(object, name, initial_value, mode, SLOPPY));
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
    ASSERT(!JSReceiver::HasLocalProperty(object, name));
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
      object->Lookup(name, &lookup);
      if (lookup.IsPropertyCallbacks()) {
        return ThrowRedeclarationError(isolate, name);
      }
    }
    if (object->IsJSGlobalObject()) {
      // Define own property on the global object.
      RETURN_FAILURE_ON_EXCEPTION(isolate,
         JSObject::SetLocalPropertyIgnoreAttributes(object, name, value, mode));
    } else {
      RETURN_FAILURE_ON_EXCEPTION(isolate,
         JSReceiver::SetProperty(object, name, value, mode, SLOPPY));
    }
  }

  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_InitializeVarGlobal) {
  HandleScope scope(isolate);
  // args[0] == name
  // args[1] == language_mode
  // args[2] == value (optional)

  // Determine if we need to assign to the variable if it already
  // exists (based on the number of arguments).
  RUNTIME_ASSERT(args.length() == 2 || args.length() == 3);
  bool assign = args.length() == 3;

  CONVERT_ARG_HANDLE_CHECKED(String, name, 0);
  CONVERT_STRICT_MODE_ARG_CHECKED(strict_mode, 1);

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
  LookupResult lookup(isolate);
  isolate->context()->global_object()->LocalLookup(name, &lookup, true);
  if (lookup.IsInterceptor()) {
    Handle<JSObject> holder(lookup.holder());
    PropertyAttributes intercepted =
        JSReceiver::GetPropertyAttribute(holder, name);
    if (intercepted != ABSENT && (intercepted & READ_ONLY) == 0) {
      // Found an interceptor that's not read only.
      if (assign) {
        CONVERT_ARG_HANDLE_CHECKED(Object, value, 2);
        Handle<Object> result;
        ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
            isolate, result,
            JSObject::SetPropertyForResult(
                holder, &lookup, name, value, attributes, strict_mode));
        return *result;
      } else {
        return isolate->heap()->undefined_value();
      }
    }
  }

  if (assign) {
    CONVERT_ARG_HANDLE_CHECKED(Object, value, 2);
    Handle<GlobalObject> global(isolate->context()->global_object());
    Handle<Object> result;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, result,
        JSReceiver::SetProperty(global, name, value, attributes, strict_mode));
    return *result;
  }
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(RuntimeHidden_InitializeConstGlobal) {
  SealHandleScope shs(isolate);
  // All constants are declared with an initial value. The name
  // of the constant is the first argument and the initial value
  // is the second.
  RUNTIME_ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(String, name, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 1);

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
  global->LocalLookup(name, &lookup);
  if (!lookup.IsFound()) {
    HandleScope handle_scope(isolate);
    Handle<GlobalObject> global(isolate->context()->global_object());
    RETURN_FAILURE_ON_EXCEPTION(
        isolate,
        JSObject::SetLocalPropertyIgnoreAttributes(global, name, value,
                                                   attributes));
    return *value;
  }

  if (!lookup.IsReadOnly()) {
    // Restore global object from context (in case of GC) and continue
    // with setting the value.
    HandleScope handle_scope(isolate);
    Handle<GlobalObject> global(isolate->context()->global_object());

    // BUG 1213575: Handle the case where we have to set a read-only
    // property through an interceptor and only do it if it's
    // uninitialized, e.g. the hole. Nirk...
    // Passing sloppy mode because the property is writable.
    RETURN_FAILURE_ON_EXCEPTION(
        isolate,
        JSReceiver::SetProperty(global, name, value, attributes, SLOPPY));
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
      HandleScope scope(isolate);
      JSObject::SetNormalizedProperty(Handle<JSObject>(global), &lookup, value);
    }
  } else {
    // Ignore re-initialization of constants that have already been
    // assigned a constant value.
    ASSERT(lookup.IsReadOnly() && lookup.IsConstant());
  }

  // Use the set value as the result of the operation.
  return *value;
}


RUNTIME_FUNCTION(RuntimeHidden_InitializeConstContextSlot) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 3);

  CONVERT_ARG_HANDLE_CHECKED(Object, value, 0);
  ASSERT(!value->IsTheHole());
  // Initializations are always done in a function or native context.
  CONVERT_ARG_HANDLE_CHECKED(Context, context_arg, 1);
  Handle<Context> context(context_arg->declaration_context());
  CONVERT_ARG_HANDLE_CHECKED(String, name, 2);

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
    RETURN_FAILURE_ON_EXCEPTION(
        isolate,
        JSReceiver::SetProperty(global, name, value, NONE, SLOPPY));
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
    object->LocalLookupRealNamedProperty(name, &lookup);
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
        JSObject::SetNormalizedProperty(object, &lookup, value);
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
      RETURN_FAILURE_ON_EXCEPTION(
          isolate,
          JSReceiver::SetProperty(object, name, value, attributes, SLOPPY));
    }
  }

  return *value;
}


RUNTIME_FUNCTION(Runtime_OptimizeObjectForAddingMultipleProperties) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 0);
  CONVERT_SMI_ARG_CHECKED(properties, 1);
  // Conservative upper limit to prevent fuzz tests from going OOM.
  RUNTIME_ASSERT(properties <= 100000);
  if (object->HasFastProperties() && !object->IsJSGlobalProxy()) {
    JSObject::NormalizeProperties(object, KEEP_INOBJECT_PROPERTIES, properties);
  }
  return *object;
}


RUNTIME_FUNCTION(RuntimeHidden_RegExpExec) {
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
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      RegExpImpl::Exec(regexp, subject, index, last_match_info));
  return *result;
}


RUNTIME_FUNCTION(RuntimeHidden_RegExpConstructResult) {
  HandleScope handle_scope(isolate);
  ASSERT(args.length() == 3);
  CONVERT_SMI_ARG_CHECKED(size, 0);
  RUNTIME_ASSERT(size >= 0 && size <= FixedArray::kMaxLength);
  CONVERT_ARG_HANDLE_CHECKED(Object, index, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, input, 2);
  Handle<FixedArray> elements =  isolate->factory()->NewFixedArray(size);
  Handle<Map> regexp_map(isolate->native_context()->regexp_result_map());
  Handle<JSObject> object =
      isolate->factory()->NewJSObjectFromMap(regexp_map, NOT_TENURED, false);
  Handle<JSArray> array = Handle<JSArray>::cast(object);
  array->set_elements(*elements);
  array->set_length(Smi::FromInt(size));
  // Write in-object properties after the length of the array.
  array->InObjectPropertyAtPut(JSRegExpResult::kIndexIndex, *index);
  array->InObjectPropertyAtPut(JSRegExpResult::kInputIndex, *input);
  return *array;
}


RUNTIME_FUNCTION(Runtime_RegExpInitializeObject) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 5);
  CONVERT_ARG_HANDLE_CHECKED(JSRegExp, regexp, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, source, 1);
  // If source is the empty string we set it to "(?:)" instead as
  // suggested by ECMA-262, 5th, section 15.10.4.1.
  if (source->length() == 0) source = isolate->factory()->query_colon_string();

  CONVERT_ARG_HANDLE_CHECKED(Object, global, 2);
  if (!global->IsTrue()) global = isolate->factory()->false_value();

  CONVERT_ARG_HANDLE_CHECKED(Object, ignoreCase, 3);
  if (!ignoreCase->IsTrue()) ignoreCase = isolate->factory()->false_value();

  CONVERT_ARG_HANDLE_CHECKED(Object, multiline, 4);
  if (!multiline->IsTrue()) multiline = isolate->factory()->false_value();

  Map* map = regexp->map();
  Object* constructor = map->constructor();
  if (constructor->IsJSFunction() &&
      JSFunction::cast(constructor)->initial_map() == map) {
    // If we still have the original map, set in-object properties directly.
    regexp->InObjectPropertyAtPut(JSRegExp::kSourceFieldIndex, *source);
    // Both true and false are immovable immortal objects so no need for write
    // barrier.
    regexp->InObjectPropertyAtPut(
        JSRegExp::kGlobalFieldIndex, *global, SKIP_WRITE_BARRIER);
    regexp->InObjectPropertyAtPut(
        JSRegExp::kIgnoreCaseFieldIndex, *ignoreCase, SKIP_WRITE_BARRIER);
    regexp->InObjectPropertyAtPut(
        JSRegExp::kMultilineFieldIndex, *multiline, SKIP_WRITE_BARRIER);
    regexp->InObjectPropertyAtPut(
        JSRegExp::kLastIndexFieldIndex, Smi::FromInt(0), SKIP_WRITE_BARRIER);
    return *regexp;
  }

  // Map has changed, so use generic, but slower, method.
  PropertyAttributes final =
      static_cast<PropertyAttributes>(READ_ONLY | DONT_ENUM | DONT_DELETE);
  PropertyAttributes writable =
      static_cast<PropertyAttributes>(DONT_ENUM | DONT_DELETE);
  Handle<Object> zero(Smi::FromInt(0), isolate);
  Factory* factory = isolate->factory();
  JSObject::SetLocalPropertyIgnoreAttributes(
      regexp, factory->source_string(), source, final).Check();
  JSObject::SetLocalPropertyIgnoreAttributes(
      regexp, factory->global_string(), global, final).Check();
  JSObject::SetLocalPropertyIgnoreAttributes(
      regexp, factory->ignore_case_string(), ignoreCase, final).Check();
  JSObject::SetLocalPropertyIgnoreAttributes(
      regexp, factory->multiline_string(), multiline, final).Check();
  JSObject::SetLocalPropertyIgnoreAttributes(
      regexp, factory->last_index_string(), zero, writable).Check();
  return *regexp;
}


RUNTIME_FUNCTION(Runtime_FinishArrayPrototypeSetup) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSArray, prototype, 0);
  Object* length = prototype->length();
  RUNTIME_ASSERT(length->IsSmi() && Smi::cast(length)->value() == 0);
  RUNTIME_ASSERT(prototype->HasFastSmiOrObjectElements());
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
      isolate->factory()->NewFunction(MaybeHandle<Object>(),
                                      key,
                                      JS_OBJECT_TYPE,
                                      JSObject::kHeaderSize,
                                      code,
                                      false);
  optimized->shared()->DontAdaptArguments();
  JSReceiver::SetProperty(holder, key, optimized, NONE, STRICT).Assert();
  return optimized;
}


RUNTIME_FUNCTION(Runtime_SpecialArrayFunctions) {
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


RUNTIME_FUNCTION(Runtime_IsSloppyModeFunction) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_CHECKED(JSReceiver, callable, 0);
  if (!callable->IsJSFunction()) {
    HandleScope scope(isolate);
    Handle<Object> delegate;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, delegate,
        Execution::TryGetFunctionDelegate(
            isolate, Handle<JSReceiver>(callable)));
    callable = JSFunction::cast(*delegate);
  }
  JSFunction* function = JSFunction::cast(callable);
  SharedFunctionInfo* shared = function->shared();
  return isolate->heap()->ToBoolean(shared->strict_mode() == SLOPPY);
}


RUNTIME_FUNCTION(Runtime_GetDefaultReceiver) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_CHECKED(JSReceiver, callable, 0);

  if (!callable->IsJSFunction()) {
    HandleScope scope(isolate);
    Handle<Object> delegate;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, delegate,
        Execution::TryGetFunctionDelegate(
            isolate, Handle<JSReceiver>(callable)));
    callable = JSFunction::cast(*delegate);
  }
  JSFunction* function = JSFunction::cast(callable);

  SharedFunctionInfo* shared = function->shared();
  if (shared->native() || shared->strict_mode() == STRICT) {
    return isolate->heap()->undefined_value();
  }
  // Returns undefined for strict or native functions, or
  // the associated global receiver for "normal" functions.

  Context* native_context =
      function->context()->global_object()->native_context();
  return native_context->global_object()->global_receiver();
}


RUNTIME_FUNCTION(RuntimeHidden_MaterializeRegExpLiteral) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 4);
  CONVERT_ARG_HANDLE_CHECKED(FixedArray, literals, 0);
  CONVERT_SMI_ARG_CHECKED(index, 1);
  CONVERT_ARG_HANDLE_CHECKED(String, pattern, 2);
  CONVERT_ARG_HANDLE_CHECKED(String, flags, 3);

  // Get the RegExp function from the context in the literals array.
  // This is the RegExp function from the context in which the
  // function was created.  We do not use the RegExp function from the
  // current native context because this might be the RegExp function
  // from another context which we should not have access to.
  Handle<JSFunction> constructor =
      Handle<JSFunction>(
          JSFunction::NativeContextFromLiterals(*literals)->regexp_function());
  // Compute the regular expression literal.
  Handle<Object> regexp;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, regexp,
      RegExpImpl::CreateRegExpLiteral(constructor, pattern, flags));
  literals->set(index, *regexp);
  return *regexp;
}


RUNTIME_FUNCTION(Runtime_FunctionGetName) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);

  CONVERT_ARG_CHECKED(JSFunction, f, 0);
  return f->shared()->name();
}


RUNTIME_FUNCTION(Runtime_FunctionSetName) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 2);

  CONVERT_ARG_CHECKED(JSFunction, f, 0);
  CONVERT_ARG_CHECKED(String, name, 1);
  f->shared()->set_name(name);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_FunctionNameShouldPrintAsAnonymous) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_CHECKED(JSFunction, f, 0);
  return isolate->heap()->ToBoolean(
      f->shared()->name_should_print_as_anonymous());
}


RUNTIME_FUNCTION(Runtime_FunctionMarkNameShouldPrintAsAnonymous) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_CHECKED(JSFunction, f, 0);
  f->shared()->set_name_should_print_as_anonymous(true);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_FunctionIsGenerator) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_CHECKED(JSFunction, f, 0);
  return isolate->heap()->ToBoolean(f->shared()->is_generator());
}


RUNTIME_FUNCTION(Runtime_FunctionRemovePrototype) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);

  CONVERT_ARG_CHECKED(JSFunction, f, 0);
  RUNTIME_ASSERT(f->RemovePrototype());

  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_FunctionGetScript) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);

  CONVERT_ARG_CHECKED(JSFunction, fun, 0);
  Handle<Object> script = Handle<Object>(fun->shared()->script(), isolate);
  if (!script->IsScript()) return isolate->heap()->undefined_value();

  return *Script::GetWrapper(Handle<Script>::cast(script));
}


RUNTIME_FUNCTION(Runtime_FunctionGetSourceCode) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);

  CONVERT_ARG_HANDLE_CHECKED(JSFunction, f, 0);
  Handle<SharedFunctionInfo> shared(f->shared());
  return *shared->GetSourceCode();
}


RUNTIME_FUNCTION(Runtime_FunctionGetScriptSourcePosition) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);

  CONVERT_ARG_CHECKED(JSFunction, fun, 0);
  int pos = fun->shared()->start_position();
  return Smi::FromInt(pos);
}


RUNTIME_FUNCTION(Runtime_FunctionGetPositionForOffset) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 2);

  CONVERT_ARG_CHECKED(Code, code, 0);
  CONVERT_NUMBER_CHECKED(int, offset, Int32, args[1]);

  RUNTIME_ASSERT(0 <= offset && offset < code->Size());

  Address pc = code->address() + offset;
  return Smi::FromInt(code->SourcePosition(pc));
}


RUNTIME_FUNCTION(Runtime_FunctionSetInstanceClassName) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 2);

  CONVERT_ARG_CHECKED(JSFunction, fun, 0);
  CONVERT_ARG_CHECKED(String, name, 1);
  fun->SetInstanceClassName(name);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_FunctionSetLength) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 2);

  CONVERT_ARG_CHECKED(JSFunction, fun, 0);
  CONVERT_SMI_ARG_CHECKED(length, 1);
  fun->shared()->set_length(length);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_FunctionSetPrototype) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);

  CONVERT_ARG_HANDLE_CHECKED(JSFunction, fun, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 1);
  ASSERT(fun->should_have_prototype());
  Accessors::FunctionSetPrototype(fun, value);
  return args[0];  // return TOS
}


RUNTIME_FUNCTION(Runtime_FunctionSetReadOnlyPrototype) {
  HandleScope shs(isolate);
  RUNTIME_ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);

  Handle<String> name = isolate->factory()->prototype_string();

  if (function->HasFastProperties()) {
    // Construct a new field descriptor with updated attributes.
    Handle<DescriptorArray> instance_desc =
        handle(function->map()->instance_descriptors());

    int index = instance_desc->SearchWithCache(*name, function->map());
    ASSERT(index != DescriptorArray::kNotFound);
    PropertyDetails details = instance_desc->GetDetails(index);

    CallbacksDescriptor new_desc(
        name,
        handle(instance_desc->GetValue(index), isolate),
        static_cast<PropertyAttributes>(details.attributes() | READ_ONLY));

    // Create a new map featuring the new field descriptors array.
    Handle<Map> map = handle(function->map());
    Handle<Map> new_map = Map::CopyReplaceDescriptor(
        map, instance_desc, &new_desc, index, OMIT_TRANSITION);

    JSObject::MigrateToMap(function, new_map);
  } else {  // Dictionary properties.
    // Directly manipulate the property details.
    DisallowHeapAllocation no_gc;
    int entry = function->property_dictionary()->FindEntry(name);
    ASSERT(entry != NameDictionary::kNotFound);
    PropertyDetails details = function->property_dictionary()->DetailsAt(entry);
    PropertyDetails new_details(
        static_cast<PropertyAttributes>(details.attributes() | READ_ONLY),
        details.type(),
        details.dictionary_index());
    function->property_dictionary()->DetailsAtPut(entry, new_details);
  }
  return *function;
}


RUNTIME_FUNCTION(Runtime_FunctionIsAPIFunction) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);

  CONVERT_ARG_CHECKED(JSFunction, f, 0);
  return isolate->heap()->ToBoolean(f->shared()->IsApiFunction());
}


RUNTIME_FUNCTION(Runtime_FunctionIsBuiltin) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);

  CONVERT_ARG_CHECKED(JSFunction, f, 0);
  return isolate->heap()->ToBoolean(f->IsBuiltin());
}


RUNTIME_FUNCTION(Runtime_SetCode) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);

  CONVERT_ARG_HANDLE_CHECKED(JSFunction, target, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, source, 1);

  Handle<SharedFunctionInfo> target_shared(target->shared());
  Handle<SharedFunctionInfo> source_shared(source->shared());
  RUNTIME_ASSERT(!source_shared->bound());

  if (!Compiler::EnsureCompiled(source, KEEP_EXCEPTION)) {
    return isolate->heap()->exception();
  }

  // Mark both, the source and the target, as un-flushable because the
  // shared unoptimized code makes them impossible to enqueue in a list.
  ASSERT(target_shared->code()->gc_metadata() == NULL);
  ASSERT(source_shared->code()->gc_metadata() == NULL);
  target_shared->set_dont_flush(true);
  source_shared->set_dont_flush(true);

  // Set the code, scope info, formal parameter count, and the length
  // of the target shared function info.
  target_shared->ReplaceCode(source_shared->code());
  target_shared->set_scope_info(source_shared->scope_info());
  target_shared->set_length(source_shared->length());
  target_shared->set_feedback_vector(source_shared->feedback_vector());
  target_shared->set_formal_parameter_count(
      source_shared->formal_parameter_count());
  target_shared->set_script(source_shared->script());
  target_shared->set_start_position_and_type(
      source_shared->start_position_and_type());
  target_shared->set_end_position(source_shared->end_position());
  bool was_native = target_shared->native();
  target_shared->set_compiler_hints(source_shared->compiler_hints());
  target_shared->set_native(was_native);
  target_shared->set_profiler_ticks(source_shared->profiler_ticks());

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


RUNTIME_FUNCTION(Runtime_SetExpectedNumberOfProperties) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, func, 0);
  CONVERT_SMI_ARG_CHECKED(num, 1);
  RUNTIME_ASSERT(num >= 0);
  // If objects constructed from this function exist then changing
  // 'estimated_nof_properties' is dangerous since the previous value might
  // have been compiled into the fast construct stub. Moreover, the inobject
  // slack tracking logic might have adjusted the previous value, so even
  // passing the same value is risky.
  if (!func->shared()->live_objects_may_exist()) {
    func->shared()->set_expected_nof_properties(num);
    if (func->has_initial_map()) {
      Handle<Map> new_initial_map = Map::Copy(handle(func->initial_map()));
      new_initial_map->set_unused_property_fields(num);
      func->set_initial_map(*new_initial_map);
    }
  }
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(RuntimeHidden_CreateJSGeneratorObject) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 0);

  JavaScriptFrameIterator it(isolate);
  JavaScriptFrame* frame = it.frame();
  Handle<JSFunction> function(frame->function());
  RUNTIME_ASSERT(function->shared()->is_generator());

  Handle<JSGeneratorObject> generator;
  if (frame->IsConstructor()) {
    generator = handle(JSGeneratorObject::cast(frame->receiver()));
  } else {
    generator = isolate->factory()->NewJSGeneratorObject(function);
  }
  generator->set_function(*function);
  generator->set_context(Context::cast(frame->context()));
  generator->set_receiver(frame->receiver());
  generator->set_continuation(0);
  generator->set_operand_stack(isolate->heap()->empty_fixed_array());
  generator->set_stack_handler_index(-1);

  return *generator;
}


RUNTIME_FUNCTION(RuntimeHidden_SuspendJSGeneratorObject) {
  HandleScope handle_scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSGeneratorObject, generator_object, 0);

  JavaScriptFrameIterator stack_iterator(isolate);
  JavaScriptFrame* frame = stack_iterator.frame();
  RUNTIME_ASSERT(frame->function()->shared()->is_generator());
  ASSERT_EQ(frame->function(), generator_object->function());

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
    Handle<FixedArray> operand_stack =
        isolate->factory()->NewFixedArray(operands_count);
    frame->SaveOperandStack(*operand_stack, &stack_handler_index);
    generator_object->set_operand_stack(*operand_stack);
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
RUNTIME_FUNCTION(RuntimeHidden_ResumeJSGeneratorObject) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 3);
  CONVERT_ARG_CHECKED(JSGeneratorObject, generator_object, 0);
  CONVERT_ARG_CHECKED(Object, value, 1);
  CONVERT_SMI_ARG_CHECKED(resume_mode_int, 2);
  JavaScriptFrameIterator stack_iterator(isolate);
  JavaScriptFrame* frame = stack_iterator.frame();

  ASSERT_EQ(frame->function(), generator_object->function());
  ASSERT(frame->function()->is_compiled());

  STATIC_ASSERT(JSGeneratorObject::kGeneratorExecuting < 0);
  STATIC_ASSERT(JSGeneratorObject::kGeneratorClosed == 0);

  Address pc = generator_object->function()->code()->instruction_start();
  int offset = generator_object->continuation();
  ASSERT(offset > 0);
  frame->set_pc(pc + offset);
  if (FLAG_enable_ool_constant_pool) {
    frame->set_constant_pool(
        generator_object->function()->code()->constant_pool());
  }
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


RUNTIME_FUNCTION(RuntimeHidden_ThrowGeneratorStateError) {
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


RUNTIME_FUNCTION(Runtime_ObjectFreeze) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 0);
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, result, JSObject::Freeze(object));
  return *result;
}


RUNTIME_FUNCTION(RuntimeHidden_StringCharCodeAt) {
  HandleScope handle_scope(isolate);
  ASSERT(args.length() == 2);

  CONVERT_ARG_HANDLE_CHECKED(String, subject, 0);
  CONVERT_NUMBER_CHECKED(uint32_t, i, Uint32, args[1]);

  // Flatten the string.  If someone wants to get a char at an index
  // in a cons string, it is likely that more indices will be
  // accessed.
  subject = String::Flatten(subject);

  if (i >= static_cast<uint32_t>(subject->length())) {
    return isolate->heap()->nan_value();
  }

  return Smi::FromInt(subject->Get(i));
}


RUNTIME_FUNCTION(Runtime_CharFromCode) {
  HandleScope handlescope(isolate);
  ASSERT(args.length() == 1);
  if (args[0]->IsNumber()) {
    CONVERT_NUMBER_CHECKED(uint32_t, code, Uint32, args[0]);
    code &= 0xffff;
    return *isolate->factory()->LookupSingleCharacterStringFromCode(code);
  }
  return isolate->heap()->empty_string();
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
    JSArray::SetContent(target_array, array_);
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


  MaybeHandle<String> ToString() {
    Isolate* isolate = heap_->isolate();
    if (array_builder_.length() == 0) {
      return isolate->factory()->empty_string();
    }

    Handle<String> joined_string;
    if (is_ascii_) {
      Handle<SeqOneByteString> seq;
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, seq,
          isolate->factory()->NewRawOneByteString(character_count_),
          String);

      DisallowHeapAllocation no_gc;
      uint8_t* char_buffer = seq->GetChars();
      StringBuilderConcatHelper(*subject_,
                                char_buffer,
                                *array_builder_.array(),
                                array_builder_.length());
      joined_string = Handle<String>::cast(seq);
    } else {
      // Non-ASCII.
      Handle<SeqTwoByteString> seq;
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, seq,
          isolate->factory()->NewRawTwoByteString(character_count_),
          String);

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
      STATIC_ASSERT(String::kMaxLength < kMaxInt);
      character_count_ = kMaxInt;
    } else {
      character_count_ += by;
    }
  }

 private:
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
MUST_USE_RESULT static Object* StringReplaceGlobalAtomRegExpWithString(
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
  int result_len;
  if (result_len_64 > static_cast<int64_t>(String::kMaxLength)) {
    STATIC_ASSERT(String::kMaxLength < kMaxInt);
    result_len = kMaxInt;  // Provoke exception.
  } else {
    result_len = static_cast<int>(result_len_64);
  }

  int subject_pos = 0;
  int result_pos = 0;

  MaybeHandle<SeqString> maybe_res;
  if (ResultSeqString::kHasAsciiEncoding) {
    maybe_res = isolate->factory()->NewRawOneByteString(result_len);
  } else {
    maybe_res = isolate->factory()->NewRawTwoByteString(result_len);
  }
  Handle<SeqString> untyped_res;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, untyped_res, maybe_res);
  Handle<ResultSeqString> result = Handle<ResultSeqString>::cast(untyped_res);

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


MUST_USE_RESULT static Object* StringReplaceGlobalRegExpWithString(
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
  if (global_cache.HasException()) return isolate->heap()->exception();

  int32_t* current_match = global_cache.FetchNext();
  if (current_match == NULL) {
    if (global_cache.HasException()) return isolate->heap()->exception();
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

  if (global_cache.HasException()) return isolate->heap()->exception();

  if (prev < subject_length) {
    builder.EnsureCapacity(2);
    builder.AddSubjectSlice(prev, subject_length);
  }

  RegExpImpl::SetLastMatchInfo(last_match_info,
                               subject,
                               capture_count,
                               global_cache.LastSuccessfulMatch());

  Handle<String> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, result, builder.ToString());
  return *result;
}


template <typename ResultSeqString>
MUST_USE_RESULT static Object* StringReplaceGlobalRegExpWithEmptyString(
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
  if (global_cache.HasException()) return isolate->heap()->exception();

  int32_t* current_match = global_cache.FetchNext();
  if (current_match == NULL) {
    if (global_cache.HasException()) return isolate->heap()->exception();
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
        isolate->factory()->NewRawOneByteString(new_length).ToHandleChecked());
  } else {
    answer = Handle<ResultSeqString>::cast(
        isolate->factory()->NewRawTwoByteString(new_length).ToHandleChecked());
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

  if (global_cache.HasException()) return isolate->heap()->exception();

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
  Heap* heap = isolate->heap();

  // The trimming is performed on a newly allocated object, which is on a
  // fresly allocated page or on an already swept page. Hence, the sweeper
  // thread can not get confused with the filler creation. No synchronization
  // needed.
  heap->CreateFillerObjectAt(end_of_string, delta);
  heap->AdjustLiveBytes(answer->address(), -delta, Heap::FROM_MUTATOR);
  return *answer;
}


RUNTIME_FUNCTION(Runtime_StringReplaceGlobalRegExpWithString) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 4);

  CONVERT_ARG_HANDLE_CHECKED(String, subject, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, replacement, 2);
  CONVERT_ARG_HANDLE_CHECKED(JSRegExp, regexp, 1);
  CONVERT_ARG_HANDLE_CHECKED(JSArray, last_match_info, 3);

  RUNTIME_ASSERT(regexp->GetFlags().is_global());

  subject = String::Flatten(subject);

  if (replacement->length() == 0) {
    if (subject->HasOnlyOneByteChars()) {
      return StringReplaceGlobalRegExpWithEmptyString<SeqOneByteString>(
          isolate, subject, regexp, last_match_info);
    } else {
      return StringReplaceGlobalRegExpWithEmptyString<SeqTwoByteString>(
          isolate, subject, regexp, last_match_info);
    }
  }

  replacement = String::Flatten(replacement);

  return StringReplaceGlobalRegExpWithString(
      isolate, subject, regexp, replacement, last_match_info);
}


// This may return an empty MaybeHandle if an exception is thrown or
// we abort due to reaching the recursion limit.
MaybeHandle<String> StringReplaceOneCharWithString(Isolate* isolate,
                                                   Handle<String> subject,
                                                   Handle<String> search,
                                                   Handle<String> replace,
                                                   bool* found,
                                                   int recursion_limit) {
  if (recursion_limit == 0) return MaybeHandle<String>();
  recursion_limit--;
  if (subject->IsConsString()) {
    ConsString* cons = ConsString::cast(*subject);
    Handle<String> first = Handle<String>(cons->first());
    Handle<String> second = Handle<String>(cons->second());
    Handle<String> new_first;
    if (!StringReplaceOneCharWithString(
            isolate, first, search, replace, found, recursion_limit)
            .ToHandle(&new_first)) {
      return MaybeHandle<String>();
    }
    if (*found) return isolate->factory()->NewConsString(new_first, second);

    Handle<String> new_second;
    if (!StringReplaceOneCharWithString(
            isolate, second, search, replace, found, recursion_limit)
            .ToHandle(&new_second)) {
      return MaybeHandle<String>();
    }
    if (*found) return isolate->factory()->NewConsString(first, new_second);

    return subject;
  } else {
    int index = Runtime::StringMatch(isolate, subject, search, 0);
    if (index == -1) return subject;
    *found = true;
    Handle<String> first = isolate->factory()->NewSubString(subject, 0, index);
    Handle<String> cons1;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, cons1,
        isolate->factory()->NewConsString(first, replace),
        String);
    Handle<String> second =
        isolate->factory()->NewSubString(subject, index + 1, subject->length());
    return isolate->factory()->NewConsString(cons1, second);
  }
}


RUNTIME_FUNCTION(Runtime_StringReplaceOneCharWithString) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 3);
  CONVERT_ARG_HANDLE_CHECKED(String, subject, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, search, 1);
  CONVERT_ARG_HANDLE_CHECKED(String, replace, 2);

  // If the cons string tree is too deep, we simply abort the recursion and
  // retry with a flattened subject string.
  const int kRecursionLimit = 0x1000;
  bool found = false;
  Handle<String> result;
  if (StringReplaceOneCharWithString(
          isolate, subject, search, replace, &found, kRecursionLimit)
          .ToHandle(&result)) {
    return *result;
  }
  if (isolate->has_pending_exception()) return isolate->heap()->exception();

  subject = String::Flatten(subject);
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      StringReplaceOneCharWithString(
          isolate, subject, search, replace, &found, kRecursionLimit));
  return *result;
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

  sub = String::Flatten(sub);
  pat = String::Flatten(pat);

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


RUNTIME_FUNCTION(Runtime_StringIndexOf) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 3);

  CONVERT_ARG_HANDLE_CHECKED(String, sub, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, pat, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, index, 2);

  uint32_t start_index;
  if (!index->ToArrayIndex(&start_index)) return Smi::FromInt(-1);

  RUNTIME_ASSERT(start_index <= static_cast<uint32_t>(sub->length()));
  int position = Runtime::StringMatch(isolate, sub, pat, start_index);
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


RUNTIME_FUNCTION(Runtime_StringLastIndexOf) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 3);

  CONVERT_ARG_HANDLE_CHECKED(String, sub, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, pat, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, index, 2);

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

  sub = String::Flatten(sub);
  pat = String::Flatten(pat);

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


RUNTIME_FUNCTION(Runtime_StringLocaleCompare) {
  HandleScope handle_scope(isolate);
  ASSERT(args.length() == 2);

  CONVERT_ARG_HANDLE_CHECKED(String, str1, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, str2, 1);

  if (str1.is_identical_to(str2)) return Smi::FromInt(0);  // Equal.
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

  str1 = String::Flatten(str1);
  str2 = String::Flatten(str2);

  DisallowHeapAllocation no_gc;
  String::FlatContent flat1 = str1->GetFlatContent();
  String::FlatContent flat2 = str2->GetFlatContent();

  for (int i = 0; i < end; i++) {
    if (flat1.Get(i) != flat2.Get(i)) {
      return Smi::FromInt(flat1.Get(i) - flat2.Get(i));
    }
  }

  return Smi::FromInt(str1_length - str2_length);
}


RUNTIME_FUNCTION(RuntimeHidden_SubString) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 3);

  CONVERT_ARG_HANDLE_CHECKED(String, string, 0);
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
  RUNTIME_ASSERT(end <= string->length());
  isolate->counters()->sub_string_runtime()->Increment();

  return *isolate->factory()->NewSubString(string, start, end);
}


RUNTIME_FUNCTION(Runtime_StringMatch) {
  HandleScope handles(isolate);
  ASSERT(args.length() == 3);

  CONVERT_ARG_HANDLE_CHECKED(String, subject, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSRegExp, regexp, 1);
  CONVERT_ARG_HANDLE_CHECKED(JSArray, regexp_info, 2);

  RUNTIME_ASSERT(regexp_info->HasFastObjectElements());

  RegExpImpl::GlobalCache global_cache(regexp, subject, true, isolate);
  if (global_cache.HasException()) return isolate->heap()->exception();

  int capture_count = regexp->CaptureCount();

  ZoneScope zone_scope(isolate->runtime_zone());
  ZoneList<int> offsets(8, zone_scope.zone());

  while (true) {
    int32_t* match = global_cache.FetchNext();
    if (match == NULL) break;
    offsets.Add(match[0], zone_scope.zone());  // start
    offsets.Add(match[1], zone_scope.zone());  // end
  }

  if (global_cache.HasException()) return isolate->heap()->exception();

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
static Object* SearchRegExpMultiple(
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
      JSArray::SetContent(result_array, cached_fixed_array);
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
  if (global_cache.HasException()) return isolate->heap()->exception();

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

  if (global_cache.HasException()) return isolate->heap()->exception();

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
      RegExpResultsCache::Enter(isolate,
                                subject,
                                handle(regexp->data(), isolate),
                                fixed_array,
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
RUNTIME_FUNCTION(Runtime_RegExpExecMultiple) {
  HandleScope handles(isolate);
  ASSERT(args.length() == 4);

  CONVERT_ARG_HANDLE_CHECKED(String, subject, 1);
  CONVERT_ARG_HANDLE_CHECKED(JSRegExp, regexp, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSArray, last_match_info, 2);
  CONVERT_ARG_HANDLE_CHECKED(JSArray, result_array, 3);

  subject = String::Flatten(subject);
  ASSERT(regexp->GetFlags().is_global());

  if (regexp->CaptureCount() == 0) {
    return SearchRegExpMultiple<false>(
        isolate, subject, regexp, last_match_info, result_array);
  } else {
    return SearchRegExpMultiple<true>(
        isolate, subject, regexp, last_match_info, result_array);
  }
}


RUNTIME_FUNCTION(Runtime_NumberToRadixString) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  CONVERT_SMI_ARG_CHECKED(radix, 1);
  RUNTIME_ASSERT(2 <= radix && radix <= 36);

  // Fast case where the result is a one character string.
  if (args[0]->IsSmi()) {
    int value = args.smi_at(0);
    if (value >= 0 && value < radix) {
      // Character array used for conversion.
      static const char kCharTable[] = "0123456789abcdefghijklmnopqrstuvwxyz";
      return *isolate->factory()->
          LookupSingleCharacterStringFromCode(kCharTable[value]);
    }
  }

  // Slow case.
  CONVERT_DOUBLE_ARG_CHECKED(value, 0);
  if (std::isnan(value)) {
    return isolate->heap()->nan_string();
  }
  if (std::isinf(value)) {
    if (value < 0) {
      return isolate->heap()->minus_infinity_string();
    }
    return isolate->heap()->infinity_string();
  }
  char* str = DoubleToRadixCString(value, radix);
  Handle<String> result = isolate->factory()->NewStringFromAsciiChecked(str);
  DeleteArray(str);
  return *result;
}


RUNTIME_FUNCTION(Runtime_NumberToFixed) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);

  CONVERT_DOUBLE_ARG_CHECKED(value, 0);
  CONVERT_DOUBLE_ARG_CHECKED(f_number, 1);
  int f = FastD2IChecked(f_number);
  // See DoubleToFixedCString for these constants:
  RUNTIME_ASSERT(f >= 0 && f <= 20);
  char* str = DoubleToFixedCString(value, f);
  Handle<String> result = isolate->factory()->NewStringFromAsciiChecked(str);
  DeleteArray(str);
  return *result;
}


RUNTIME_FUNCTION(Runtime_NumberToExponential) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);

  CONVERT_DOUBLE_ARG_CHECKED(value, 0);
  CONVERT_DOUBLE_ARG_CHECKED(f_number, 1);
  int f = FastD2IChecked(f_number);
  RUNTIME_ASSERT(f >= -1 && f <= 20);
  char* str = DoubleToExponentialCString(value, f);
  Handle<String> result = isolate->factory()->NewStringFromAsciiChecked(str);
  DeleteArray(str);
  return *result;
}


RUNTIME_FUNCTION(Runtime_NumberToPrecision) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);

  CONVERT_DOUBLE_ARG_CHECKED(value, 0);
  CONVERT_DOUBLE_ARG_CHECKED(f_number, 1);
  int f = FastD2IChecked(f_number);
  RUNTIME_ASSERT(f >= 1 && f <= 21);
  char* str = DoubleToPrecisionCString(value, f);
  Handle<String> result = isolate->factory()->NewStringFromAsciiChecked(str);
  DeleteArray(str);
  return *result;
}


RUNTIME_FUNCTION(Runtime_IsValidSmi) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);

  CONVERT_NUMBER_CHECKED(int32_t, number, Int32, args[0]);
  return isolate->heap()->ToBoolean(Smi::IsValid(number));
}


// Returns a single character string where first character equals
// string->Get(index).
static Handle<Object> GetCharAt(Handle<String> string, uint32_t index) {
  if (index < static_cast<uint32_t>(string->length())) {
    Factory* factory = string->GetIsolate()->factory();
    return factory->LookupSingleCharacterStringFromCode(
        String::Flatten(string)->Get(index));
  }
  return Execution::CharAt(string, index);
}


MaybeHandle<Object> Runtime::GetElementOrCharAt(Isolate* isolate,
                                                Handle<Object> object,
                                                uint32_t index) {
  // Handle [] indexing on Strings
  if (object->IsString()) {
    Handle<Object> result = GetCharAt(Handle<String>::cast(object), index);
    if (!result->IsUndefined()) return result;
  }

  // Handle [] indexing on String objects
  if (object->IsStringObjectWithCharacterAt(index)) {
    Handle<JSValue> js_value = Handle<JSValue>::cast(object);
    Handle<Object> result =
        GetCharAt(Handle<String>(String::cast(js_value->value())), index);
    if (!result->IsUndefined()) return result;
  }

  Handle<Object> result;
  if (object->IsString() || object->IsNumber() || object->IsBoolean()) {
    Handle<Object> proto(object->GetPrototype(isolate), isolate);
    return Object::GetElement(isolate, proto, index);
  } else {
    return Object::GetElement(isolate, object, index);
  }
}


MUST_USE_RESULT
static MaybeHandle<Name> ToName(Isolate* isolate, Handle<Object> key) {
  if (key->IsName()) {
    return Handle<Name>::cast(key);
  } else {
    Handle<Object> converted;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, converted, Execution::ToString(isolate, key), Name);
    return Handle<Name>::cast(converted);
  }
}


MaybeHandle<Object> Runtime::HasObjectProperty(Isolate* isolate,
                                               Handle<JSReceiver> object,
                                               Handle<Object> key) {
  // Check if the given key is an array index.
  uint32_t index;
  if (key->ToArrayIndex(&index)) {
    return isolate->factory()->ToBoolean(JSReceiver::HasElement(object, index));
  }

  // Convert the key to a name - possibly by calling back into JavaScript.
  Handle<Name> name;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, name, ToName(isolate, key), Object);

  return isolate->factory()->ToBoolean(JSReceiver::HasProperty(object, name));
}


MaybeHandle<Object> Runtime::GetObjectProperty(Isolate* isolate,
                                               Handle<Object> object,
                                               Handle<Object> key) {
  if (object->IsUndefined() || object->IsNull()) {
    Handle<Object> args[2] = { key, object };
    return isolate->Throw<Object>(
        isolate->factory()->NewTypeError("non_object_property_load",
                                         HandleVector(args, 2)));
  }

  // Check if the given key is an array index.
  uint32_t index;
  if (key->ToArrayIndex(&index)) {
    return GetElementOrCharAt(isolate, object, index);
  }

  // Convert the key to a name - possibly by calling back into JavaScript.
  Handle<Name> name;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, name, ToName(isolate, key), Object);

  // Check if the name is trivially convertible to an index and get
  // the element if so.
  if (name->AsArrayIndex(&index)) {
    return GetElementOrCharAt(isolate, object, index);
  } else {
    return Object::GetProperty(object, name);
  }
}


RUNTIME_FUNCTION(Runtime_GetProperty) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);

  CONVERT_ARG_HANDLE_CHECKED(Object, object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 1);
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      Runtime::GetObjectProperty(isolate, object, key));
  return *result;
}


// KeyedGetProperty is called from KeyedLoadIC::GenerateGeneric.
RUNTIME_FUNCTION(Runtime_KeyedGetProperty) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);

  CONVERT_ARG_HANDLE_CHECKED(Object, receiver_obj, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, key_obj, 1);

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
  if (receiver_obj->IsJSObject()) {
    if (!receiver_obj->IsJSGlobalProxy() &&
        !receiver_obj->IsAccessCheckNeeded() &&
        key_obj->IsName()) {
      DisallowHeapAllocation no_allocation;
      Handle<JSObject> receiver = Handle<JSObject>::cast(receiver_obj);
      Handle<Name> key = Handle<Name>::cast(key_obj);
      if (receiver->HasFastProperties()) {
        // Attempt to use lookup cache.
        Handle<Map> receiver_map(receiver->map(), isolate);
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
          if (!result.representation().IsDouble()) {
            keyed_lookup_cache->Update(receiver_map, key, offset);
          }
          AllowHeapAllocation allow_allocation;
          return *JSObject::FastPropertyAt(
              receiver, result.representation(), offset);
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
    } else if (FLAG_smi_only_arrays && key_obj->IsSmi()) {
      // JSObject without a name key. If the key is a Smi, check for a
      // definite out-of-bounds access to elements, which is a strong indicator
      // that subsequent accesses will also call the runtime. Proactively
      // transition elements to FAST_*_ELEMENTS to avoid excessive boxing of
      // doubles for those future calls in the case that the elements would
      // become FAST_DOUBLE_ELEMENTS.
      Handle<JSObject> js_object = Handle<JSObject>::cast(receiver_obj);
      ElementsKind elements_kind = js_object->GetElementsKind();
      if (IsFastDoubleElementsKind(elements_kind)) {
        Handle<Smi> key = Handle<Smi>::cast(key_obj);
        if (key->value() >= js_object->elements()->length()) {
          if (IsFastHoleyElementsKind(elements_kind)) {
            elements_kind = FAST_HOLEY_ELEMENTS;
          } else {
            elements_kind = FAST_ELEMENTS;
          }
          RETURN_FAILURE_ON_EXCEPTION(
              isolate, TransitionElements(js_object, elements_kind, isolate));
        }
      } else {
        ASSERT(IsFastSmiOrObjectElementsKind(elements_kind) ||
               !IsFastElementsKind(elements_kind));
      }
    }
  } else if (receiver_obj->IsString() && key_obj->IsSmi()) {
    // Fast case for string indexing using [] with a smi index.
    Handle<String> str = Handle<String>::cast(receiver_obj);
    int index = args.smi_at(1);
    if (index >= 0 && index < str->length()) {
      return *GetCharAt(str, index);
    }
  }

  // Fall back to GetObjectProperty.
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      Runtime::GetObjectProperty(isolate, receiver_obj, key_obj));
  return *result;
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
RUNTIME_FUNCTION(Runtime_DefineOrRedefineAccessorProperty) {
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
  // DefineAccessor checks access rights.
  JSObject::DefineAccessor(obj, name, getter, setter, attr);
  RETURN_FAILURE_IF_SCHEDULED_EXCEPTION(isolate);
  if (fast) JSObject::TransformToFastProperties(obj, 0);
  return isolate->heap()->undefined_value();
}


// Implements part of 8.12.9 DefineOwnProperty.
// There are 3 cases that lead here:
// Step 4a - define a new data property.
// Steps 9b & 12 - replace an existing accessor property with a data property.
// Step 12 - update an existing data property with a data or generic
//           descriptor.
RUNTIME_FUNCTION(Runtime_DefineOrRedefineDataProperty) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 4);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, js_object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Name, name, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, obj_value, 2);
  CONVERT_SMI_ARG_CHECKED(unchecked, 3);
  RUNTIME_ASSERT((unchecked & ~(READ_ONLY | DONT_ENUM | DONT_DELETE)) == 0);
  PropertyAttributes attr = static_cast<PropertyAttributes>(unchecked);

  // Check access rights if needed.
  if (js_object->IsAccessCheckNeeded() &&
      !isolate->MayNamedAccess(js_object, name, v8::ACCESS_SET)) {
    return isolate->heap()->undefined_value();
  }

  LookupResult lookup(isolate);
  js_object->LocalLookupRealNamedProperty(name, &lookup);

  // Special case for callback properties.
  if (lookup.IsPropertyCallbacks()) {
    Handle<Object> callback(lookup.GetCallbackObject(), isolate);
    // Avoid redefining callback as data property, just use the stored
    // setter to update the value instead.
    // TODO(mstarzinger): So far this only works if property attributes don't
    // change, this should be fixed once we cleanup the underlying code.
    ASSERT(!callback->IsForeign());
    if (callback->IsAccessorInfo() &&
        lookup.GetAttributes() == attr) {
      Handle<Object> result_object;
      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
          isolate, result_object,
          JSObject::SetPropertyWithCallback(js_object,
                                            callback,
                                            name,
                                            obj_value,
                                            handle(lookup.holder()),
                                            STRICT));
      return *result_object;
    }
  }

  // Take special care when attributes are different and there is already
  // a property. For simplicity we normalize the property which enables us
  // to not worry about changing the instance_descriptor and creating a new
  // map. The current version of SetObjectProperty does not handle attributes
  // correctly in the case where a property is a field and is reset with
  // new attributes.
  if (lookup.IsFound() &&
      (attr != lookup.GetAttributes() || lookup.IsPropertyCallbacks())) {
    // New attributes - normalize to avoid writing to instance descriptor
    if (js_object->IsJSGlobalProxy()) {
      // Since the result is a property, the prototype will exist so
      // we don't have to check for null.
      js_object = Handle<JSObject>(JSObject::cast(js_object->GetPrototype()));
    }
    JSObject::NormalizeProperties(js_object, CLEAR_INOBJECT_PROPERTIES, 0);
    // Use IgnoreAttributes version since a readonly property may be
    // overridden and SetProperty does not allow this.
    Handle<Object> result;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, result,
        JSObject::SetLocalPropertyIgnoreAttributes(
            js_object, name, obj_value, attr));
    return *result;
  }

  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      Runtime::ForceSetObjectProperty(
          js_object, name, obj_value, attr,
          JSReceiver::CERTAINLY_NOT_STORE_FROM_KEYED));
  return *result;
}


// Return property without being observable by accessors or interceptors.
RUNTIME_FUNCTION(Runtime_GetDataProperty) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Name, key, 1);
  return *JSObject::GetDataProperty(object, key);
}


MaybeHandle<Object> Runtime::SetObjectProperty(Isolate* isolate,
                                               Handle<Object> object,
                                               Handle<Object> key,
                                               Handle<Object> value,
                                               PropertyAttributes attr,
                                               StrictMode strict_mode) {
  SetPropertyMode set_mode = attr == NONE ? SET_PROPERTY : DEFINE_PROPERTY;

  if (object->IsUndefined() || object->IsNull()) {
    Handle<Object> args[2] = { key, object };
    Handle<Object> error =
        isolate->factory()->NewTypeError("non_object_property_store",
                                         HandleVector(args, 2));
    return isolate->Throw<Object>(error);
  }

  if (object->IsJSProxy()) {
    Handle<Object> name_object;
    if (key->IsSymbol()) {
      name_object = key;
    } else {
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, name_object, Execution::ToString(isolate, key), Object);
    }
    Handle<Name> name = Handle<Name>::cast(name_object);
    return JSReceiver::SetProperty(Handle<JSProxy>::cast(object), name, value,
                                   attr,
                                   strict_mode);
  }

  // If the object isn't a JavaScript object, we ignore the store.
  if (!object->IsJSObject()) return value;

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
      return value;
    }

    JSObject::ValidateElements(js_object);
    if (js_object->HasExternalArrayElements() ||
        js_object->HasFixedTypedArrayElements()) {
      if (!value->IsNumber() && !value->IsUndefined()) {
        ASSIGN_RETURN_ON_EXCEPTION(
            isolate, value, Execution::ToNumber(isolate, value), Object);
      }
    }

    MaybeHandle<Object> result = JSObject::SetElement(
        js_object, index, value, attr, strict_mode, true, set_mode);
    JSObject::ValidateElements(js_object);

    return result.is_null() ? result : value;
  }

  if (key->IsName()) {
    Handle<Name> name = Handle<Name>::cast(key);
    if (name->AsArrayIndex(&index)) {
      if (js_object->HasExternalArrayElements()) {
        if (!value->IsNumber() && !value->IsUndefined()) {
          ASSIGN_RETURN_ON_EXCEPTION(
              isolate, value, Execution::ToNumber(isolate, value), Object);
        }
      }
      return JSObject::SetElement(js_object, index, value, attr,
                                  strict_mode, true, set_mode);
    } else {
      if (name->IsString()) name = String::Flatten(Handle<String>::cast(name));
      return JSReceiver::SetProperty(js_object, name, value, attr, strict_mode);
    }
  }

  // Call-back into JavaScript to convert the key to a string.
  Handle<Object> converted;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, converted, Execution::ToString(isolate, key), Object);
  Handle<String> name = Handle<String>::cast(converted);

  if (name->AsArrayIndex(&index)) {
    return JSObject::SetElement(js_object, index, value, attr,
                                strict_mode, true, set_mode);
  } else {
    return JSReceiver::SetProperty(js_object, name, value, attr, strict_mode);
  }
}


MaybeHandle<Object> Runtime::ForceSetObjectProperty(
    Handle<JSObject> js_object,
    Handle<Object> key,
    Handle<Object> value,
    PropertyAttributes attr,
    JSReceiver::StoreFromKeyed store_from_keyed) {
  Isolate* isolate = js_object->GetIsolate();
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
      return value;
    }

    return JSObject::SetElement(js_object, index, value, attr,
                                SLOPPY, false, DEFINE_PROPERTY);
  }

  if (key->IsName()) {
    Handle<Name> name = Handle<Name>::cast(key);
    if (name->AsArrayIndex(&index)) {
      return JSObject::SetElement(js_object, index, value, attr,
                                  SLOPPY, false, DEFINE_PROPERTY);
    } else {
      if (name->IsString()) name = String::Flatten(Handle<String>::cast(name));
      return JSObject::SetLocalPropertyIgnoreAttributes(
          js_object, name, value, attr, Object::OPTIMAL_REPRESENTATION,
          ALLOW_AS_CONSTANT, JSReceiver::PERFORM_EXTENSIBILITY_CHECK,
          store_from_keyed);
    }
  }

  // Call-back into JavaScript to convert the key to a string.
  Handle<Object> converted;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, converted, Execution::ToString(isolate, key), Object);
  Handle<String> name = Handle<String>::cast(converted);

  if (name->AsArrayIndex(&index)) {
    return JSObject::SetElement(js_object, index, value, attr,
                                SLOPPY, false, DEFINE_PROPERTY);
  } else {
    return JSObject::SetLocalPropertyIgnoreAttributes(
        js_object, name, value, attr, Object::OPTIMAL_REPRESENTATION,
        ALLOW_AS_CONSTANT, JSReceiver::PERFORM_EXTENSIBILITY_CHECK,
        store_from_keyed);
  }
}


MaybeHandle<Object> Runtime::DeleteObjectProperty(Isolate* isolate,
                                                  Handle<JSReceiver> receiver,
                                                  Handle<Object> key,
                                                  JSReceiver::DeleteMode mode) {
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
      return isolate->factory()->true_value();
    }

    return JSReceiver::DeleteElement(receiver, index, mode);
  }

  Handle<Name> name;
  if (key->IsName()) {
    name = Handle<Name>::cast(key);
  } else {
    // Call-back into JavaScript to convert the key to a string.
    Handle<Object> converted;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, converted, Execution::ToString(isolate, key), Object);
    name = Handle<String>::cast(converted);
  }

  if (name->IsString()) name = String::Flatten(Handle<String>::cast(name));
  return JSReceiver::DeleteProperty(receiver, name, mode);
}


RUNTIME_FUNCTION(Runtime_SetHiddenProperty) {
  HandleScope scope(isolate);
  RUNTIME_ASSERT(args.length() == 3);

  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, key, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 2);
  return *JSObject::SetHiddenProperty(object, key, value);
}


RUNTIME_FUNCTION(Runtime_SetProperty) {
  HandleScope scope(isolate);
  RUNTIME_ASSERT(args.length() == 4 || args.length() == 5);

  CONVERT_ARG_HANDLE_CHECKED(Object, object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, key, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 2);
  CONVERT_SMI_ARG_CHECKED(unchecked_attributes, 3);
  RUNTIME_ASSERT(
      (unchecked_attributes & ~(READ_ONLY | DONT_ENUM | DONT_DELETE)) == 0);
  // Compute attributes.
  PropertyAttributes attributes =
      static_cast<PropertyAttributes>(unchecked_attributes);

  StrictMode strict_mode = SLOPPY;
  if (args.length() == 5) {
    CONVERT_STRICT_MODE_ARG_CHECKED(strict_mode_arg, 4);
    strict_mode = strict_mode_arg;
  }

  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      Runtime::SetObjectProperty(
          isolate, object, key, value, attributes, strict_mode));
  return *result;
}


RUNTIME_FUNCTION(Runtime_TransitionElementsKind) {
  HandleScope scope(isolate);
  RUNTIME_ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSArray, array, 0);
  CONVERT_ARG_HANDLE_CHECKED(Map, map, 1);
  JSObject::TransitionElementsKind(array, map->elements_kind());
  return *array;
}


// Set the native flag on the function.
// This is used to decide if we should transform null and undefined
// into the global object when doing call and apply.
RUNTIME_FUNCTION(Runtime_SetNativeFlag) {
  SealHandleScope shs(isolate);
  RUNTIME_ASSERT(args.length() == 1);

  CONVERT_ARG_CHECKED(Object, object, 0);

  if (object->IsJSFunction()) {
    JSFunction* func = JSFunction::cast(object);
    func->shared()->set_native(true);
  }
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_SetInlineBuiltinFlag) {
  SealHandleScope shs(isolate);
  RUNTIME_ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, object, 0);

  if (object->IsJSFunction()) {
    JSFunction* func = JSFunction::cast(*object);
    func->shared()->set_inline_builtin(true);
  }
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_StoreArrayLiteralElement) {
  HandleScope scope(isolate);
  RUNTIME_ASSERT(args.length() == 5);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 0);
  CONVERT_SMI_ARG_CHECKED(store_index, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 2);
  CONVERT_ARG_HANDLE_CHECKED(FixedArray, literals, 3);
  CONVERT_SMI_ARG_CHECKED(literal_index, 4);

  Object* raw_literal_cell = literals->get(literal_index);
  JSArray* boilerplate = NULL;
  if (raw_literal_cell->IsAllocationSite()) {
    AllocationSite* site = AllocationSite::cast(raw_literal_cell);
    boilerplate = JSArray::cast(site->transition_info());
  } else {
    boilerplate = JSArray::cast(raw_literal_cell);
  }
  Handle<JSArray> boilerplate_object(boilerplate);
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
RUNTIME_FUNCTION(Runtime_DebugCallbackSupportsStepping) {
  ASSERT(args.length() == 1);
  if (!isolate->IsDebuggerActive() || !isolate->debug()->StepInActive()) {
    return isolate->heap()->false_value();
  }
  CONVERT_ARG_CHECKED(Object, callback, 0);
  // We do not step into the callback if it's a builtin or not even a function.
  return isolate->heap()->ToBoolean(
      callback->IsJSFunction() && !JSFunction::cast(callback)->IsBuiltin());
}


// Set one shot breakpoints for the callback function that is passed to a
// built-in function such as Array.forEach to enable stepping into the callback.
RUNTIME_FUNCTION(Runtime_DebugPrepareStepInIfStepping) {
  ASSERT(args.length() == 1);
  Debug* debug = isolate->debug();
  if (!debug->IsStepping()) return isolate->heap()->undefined_value();
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, callback, 0);
  HandleScope scope(isolate);
  // When leaving the callback, step out has been activated, but not performed
  // if we do not leave the builtin.  To be able to step into the callback
  // again, we need to clear the step out at this point.
  debug->ClearStepOut();
  debug->FloodWithOneShot(callback);
  return isolate->heap()->undefined_value();
}


// The argument is a closure that is kept until the epilogue is called.
// On exception, the closure is called, which returns the promise if the
// exception is considered uncaught, or undefined otherwise.
RUNTIME_FUNCTION(Runtime_DebugPromiseHandlePrologue) {
  ASSERT(args.length() == 1);
  HandleScope scope(isolate);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, promise_getter, 0);
  isolate->debug()->PromiseHandlePrologue(promise_getter);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_DebugPromiseHandleEpilogue) {
  ASSERT(args.length() == 0);
  SealHandleScope shs(isolate);
  isolate->debug()->PromiseHandleEpilogue();
  return isolate->heap()->undefined_value();
}


// Set a local property, even if it is READ_ONLY.  If the property does not
// exist, it will be added with attributes NONE.
RUNTIME_FUNCTION(Runtime_IgnoreAttributesAndSetProperty) {
  HandleScope scope(isolate);
  RUNTIME_ASSERT(args.length() == 3 || args.length() == 4);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Name, name, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, value, 2);
  // Compute attributes.
  PropertyAttributes attributes = NONE;
  if (args.length() == 4) {
    CONVERT_SMI_ARG_CHECKED(unchecked_value, 3);
    // Only attribute bits should be set.
    RUNTIME_ASSERT(
        (unchecked_value & ~(READ_ONLY | DONT_ENUM | DONT_DELETE)) == 0);
    attributes = static_cast<PropertyAttributes>(unchecked_value);
  }
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      JSObject::SetLocalPropertyIgnoreAttributes(
          object, name, value, attributes));
  return *result;
}


RUNTIME_FUNCTION(Runtime_DeleteProperty) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 3);
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Name, key, 1);
  CONVERT_STRICT_MODE_ARG_CHECKED(strict_mode, 2);
  JSReceiver::DeleteMode delete_mode = strict_mode == STRICT
      ? JSReceiver::STRICT_DELETION : JSReceiver::NORMAL_DELETION;
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      JSReceiver::DeleteProperty(object, key, delete_mode));
  return *result;
}


static Object* HasLocalPropertyImplementation(Isolate* isolate,
                                              Handle<JSObject> object,
                                              Handle<Name> key) {
  if (JSReceiver::HasLocalProperty(object, key)) {
    return isolate->heap()->true_value();
  }
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
  RETURN_FAILURE_IF_SCHEDULED_EXCEPTION(isolate);
  return isolate->heap()->false_value();
}


RUNTIME_FUNCTION(Runtime_HasLocalProperty) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(Object, object, 0)
  CONVERT_ARG_HANDLE_CHECKED(Name, key, 1);

  uint32_t index;
  const bool key_is_array_index = key->AsArrayIndex(&index);

  // Only JS objects can have properties.
  if (object->IsJSObject()) {
    Handle<JSObject> js_obj = Handle<JSObject>::cast(object);
    // Fast case: either the key is a real named property or it is not
    // an array index and there are no interceptors or hidden
    // prototypes.
    if (JSObject::HasRealNamedProperty(js_obj, key)) {
      ASSERT(!isolate->has_scheduled_exception());
      return isolate->heap()->true_value();
    } else {
      RETURN_FAILURE_IF_SCHEDULED_EXCEPTION(isolate);
    }
    Map* map = js_obj->map();
    if (!key_is_array_index &&
        !map->has_named_interceptor() &&
        !HeapObject::cast(map->prototype())->map()->is_hidden_prototype()) {
      return isolate->heap()->false_value();
    }
    // Slow case.
    return HasLocalPropertyImplementation(isolate,
                                          Handle<JSObject>(js_obj),
                                          Handle<Name>(key));
  } else if (object->IsString() && key_is_array_index) {
    // Well, there is one exception:  Handle [] on strings.
    Handle<String> string = Handle<String>::cast(object);
    if (index < static_cast<uint32_t>(string->length())) {
      return isolate->heap()->true_value();
    }
  }
  return isolate->heap()->false_value();
}


RUNTIME_FUNCTION(Runtime_HasProperty) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, receiver, 0);
  CONVERT_ARG_HANDLE_CHECKED(Name, key, 1);

  bool result = JSReceiver::HasProperty(receiver, key);
  RETURN_FAILURE_IF_SCHEDULED_EXCEPTION(isolate);
  if (isolate->has_pending_exception()) return isolate->heap()->exception();
  return isolate->heap()->ToBoolean(result);
}


RUNTIME_FUNCTION(Runtime_HasElement) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, receiver, 0);
  CONVERT_SMI_ARG_CHECKED(index, 1);

  bool result = JSReceiver::HasElement(receiver, index);
  RETURN_FAILURE_IF_SCHEDULED_EXCEPTION(isolate);
  return isolate->heap()->ToBoolean(result);
}


RUNTIME_FUNCTION(Runtime_IsPropertyEnumerable) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);

  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Name, key, 1);

  PropertyAttributes att = JSReceiver::GetLocalPropertyAttribute(object, key);
  if (att == ABSENT || (att & DONT_ENUM) != 0) {
    RETURN_FAILURE_IF_SCHEDULED_EXCEPTION(isolate);
    return isolate->heap()->false_value();
  }
  ASSERT(!isolate->has_scheduled_exception());
  return isolate->heap()->true_value();
}


RUNTIME_FUNCTION(Runtime_GetPropertyNames) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, object, 0);
  Handle<JSArray> result;

  isolate->counters()->for_in()->Increment();
  Handle<FixedArray> elements;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, elements,
      JSReceiver::GetKeys(object, JSReceiver::INCLUDE_PROTOS));
  return *isolate->factory()->NewJSArrayWithElements(elements);
}


// Returns either a FixedArray as Runtime_GetPropertyNames,
// or, if the given object has an enum cache that contains
// all enumerable properties of the object and its prototypes
// have none, the map of the object. This is used to speed up
// the check for deletions during a for-in.
RUNTIME_FUNCTION(Runtime_GetPropertyNamesFast) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);

  CONVERT_ARG_CHECKED(JSReceiver, raw_object, 0);

  if (raw_object->IsSimpleEnum()) return raw_object->map();

  HandleScope scope(isolate);
  Handle<JSReceiver> object(raw_object);
  Handle<FixedArray> content;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, content,
      JSReceiver::GetKeys(object, JSReceiver::INCLUDE_PROTOS));

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
// args[1]: PropertyAttributes as int
RUNTIME_FUNCTION(Runtime_GetLocalPropertyNames) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  if (!args[0]->IsJSObject()) {
    return isolate->heap()->undefined_value();
  }
  CONVERT_ARG_HANDLE_CHECKED(JSObject, obj, 0);
  CONVERT_SMI_ARG_CHECKED(filter_value, 1);
  PropertyAttributes filter = static_cast<PropertyAttributes>(filter_value);

  // Skip the global proxy as it has no properties and always delegates to the
  // real global object.
  if (obj->IsJSGlobalProxy()) {
    // Only collect names if access is permitted.
    if (obj->IsAccessCheckNeeded() &&
        !isolate->MayNamedAccess(
            obj, isolate->factory()->undefined_value(), v8::ACCESS_KEYS)) {
      isolate->ReportFailedAccessCheck(obj, v8::ACCESS_KEYS);
      RETURN_FAILURE_IF_SCHEDULED_EXCEPTION(isolate);
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
        !isolate->MayNamedAccess(
            jsproto, isolate->factory()->undefined_value(), v8::ACCESS_KEYS)) {
      isolate->ReportFailedAccessCheck(jsproto, v8::ACCESS_KEYS);
      RETURN_FAILURE_IF_SCHEDULED_EXCEPTION(isolate);
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
  int next_copy_index = 0;
  int hidden_strings = 0;
  for (int i = 0; i < length; i++) {
    jsproto->GetLocalPropertyNames(*names, next_copy_index, filter);
    if (i > 0) {
      // Names from hidden prototypes may already have been added
      // for inherited function template instances. Count the duplicates
      // and stub them out; the final copy pass at the end ignores holes.
      for (int j = next_copy_index;
           j < next_copy_index + local_property_count[i];
           j++) {
        Object* name_from_hidden_proto = names->get(j);
        for (int k = 0; k < next_copy_index; k++) {
          if (names->get(k) != isolate->heap()->hidden_string()) {
            Object* name = names->get(k);
            if (name_from_hidden_proto == name) {
              names->set(j, isolate->heap()->hidden_string());
              hidden_strings++;
              break;
            }
          }
        }
      }
    }
    next_copy_index += local_property_count[i];

    // Hidden properties only show up if the filter does not skip strings.
    if ((filter & STRING) == 0 && JSObject::HasHiddenProperties(jsproto)) {
      hidden_strings++;
    }
    if (i < length - 1) {
      jsproto = Handle<JSObject>(JSObject::cast(jsproto->GetPrototype()));
    }
  }

  // Filter out name of hidden properties object and
  // hidden prototype duplicates.
  if (hidden_strings > 0) {
    Handle<FixedArray> old_names = names;
    names = isolate->factory()->NewFixedArray(
        names->length() - hidden_strings);
    int dest_pos = 0;
    for (int i = 0; i < total_property_count; i++) {
      Object* name = old_names->get(i);
      if (name == isolate->heap()->hidden_string()) {
        hidden_strings--;
        continue;
      }
      names->set(dest_pos++, name);
    }
    ASSERT_EQ(0, hidden_strings);
  }

  return *isolate->factory()->NewJSArrayWithElements(names);
}


// Return the names of the local indexed properties.
// args[0]: object
RUNTIME_FUNCTION(Runtime_GetLocalElementNames) {
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
RUNTIME_FUNCTION(Runtime_GetInterceptorInfo) {
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
RUNTIME_FUNCTION(Runtime_GetNamedInterceptorPropertyNames) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, obj, 0);

  if (obj->HasNamedInterceptor()) {
    Handle<JSObject> result;
    if (JSObject::GetKeysForNamedInterceptor(obj, obj).ToHandle(&result)) {
      return *result;
    }
  }
  return isolate->heap()->undefined_value();
}


// Return element names from indexed interceptor.
// args[0]: object
RUNTIME_FUNCTION(Runtime_GetIndexedInterceptorElementNames) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, obj, 0);

  if (obj->HasIndexedInterceptor()) {
    Handle<JSObject> result;
    if (JSObject::GetKeysForIndexedInterceptor(obj, obj).ToHandle(&result)) {
      return *result;
    }
  }
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_LocalKeys) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_CHECKED(JSObject, raw_object, 0);
  Handle<JSObject> object(raw_object);

  if (object->IsJSGlobalProxy()) {
    // Do access checks before going to the global object.
    if (object->IsAccessCheckNeeded() &&
        !isolate->MayNamedAccess(
            object, isolate->factory()->undefined_value(), v8::ACCESS_KEYS)) {
      isolate->ReportFailedAccessCheck(object, v8::ACCESS_KEYS);
      RETURN_FAILURE_IF_SCHEDULED_EXCEPTION(isolate);
      return *isolate->factory()->NewJSArray(0);
    }

    Handle<Object> proto(object->GetPrototype(), isolate);
    // If proxy is detached we simply return an empty array.
    if (proto->IsNull()) return *isolate->factory()->NewJSArray(0);
    object = Handle<JSObject>::cast(proto);
  }

  Handle<FixedArray> contents;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, contents,
      JSReceiver::GetKeys(object, JSReceiver::LOCAL_ONLY));

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


RUNTIME_FUNCTION(Runtime_GetArgumentsProperty) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, raw_key, 0);

  // Compute the frame holding the arguments.
  JavaScriptFrameIterator it(isolate);
  it.AdvanceToArgumentsFrame();
  JavaScriptFrame* frame = it.frame();

  // Get the actual number of provided arguments.
  const uint32_t n = frame->ComputeParametersCount();

  // Try to convert the key to an index. If successful and within
  // index return the the argument from the frame.
  uint32_t index;
  if (raw_key->ToArrayIndex(&index) && index < n) {
    return frame->GetParameter(index);
  }

  HandleScope scope(isolate);
  if (raw_key->IsSymbol()) {
    // Lookup in the initial Object.prototype object.
    Handle<Object> result;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, result,
        Object::GetProperty(isolate->initial_object_prototype(),
                            Handle<Symbol>::cast(raw_key)));
    return *result;
  }

  // Convert the key to a string.
  Handle<Object> converted;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, converted, Execution::ToString(isolate, raw_key));
  Handle<String> key = Handle<String>::cast(converted);

  // Try to convert the string key into an array index.
  if (key->AsArrayIndex(&index)) {
    if (index < n) {
      return frame->GetParameter(index);
    } else {
      Handle<Object> initial_prototype(isolate->initial_object_prototype());
      Handle<Object> result;
      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
          isolate, result,
          Object::GetElement(isolate, initial_prototype, index));
      return *result;
    }
  }

  // Handle special arguments properties.
  if (String::Equals(isolate->factory()->length_string(), key)) {
    return Smi::FromInt(n);
  }
  if (String::Equals(isolate->factory()->callee_string(), key)) {
    JSFunction* function = frame->function();
    if (function->shared()->strict_mode() == STRICT) {
      return isolate->Throw(*isolate->factory()->NewTypeError(
          "strict_arguments_callee", HandleVector<Object>(NULL, 0)));
    }
    return function;
  }

  // Lookup in the initial Object.prototype object.
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      Object::GetProperty(isolate->initial_object_prototype(), key));
  return *result;
}


RUNTIME_FUNCTION(Runtime_ToFastProperties) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, object, 0);
  if (object->IsJSObject() && !object->IsGlobalObject()) {
    JSObject::TransformToFastProperties(Handle<JSObject>::cast(object), 0);
  }
  return *object;
}


RUNTIME_FUNCTION(Runtime_ToBool) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_CHECKED(Object, object, 0);

  return isolate->heap()->ToBoolean(object->BooleanValue());
}


// Returns the type string of a value; see ECMA-262, 11.4.3 (p 47).
// Possible optimizations: put the type string into the oddballs.
RUNTIME_FUNCTION(Runtime_Typeof) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_CHECKED(Object, obj, 0);
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


RUNTIME_FUNCTION(Runtime_StringToNumber) {
  HandleScope handle_scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(String, subject, 0);
  subject = String::Flatten(subject);

  // Fast case: short integer or some sorts of junk values.
  if (subject->IsSeqOneByteString()) {
    int len = subject->length();
    if (len == 0) return Smi::FromInt(0);

    DisallowHeapAllocation no_gc;
    uint8_t const* data = Handle<SeqOneByteString>::cast(subject)->GetChars();
    bool minus = (data[0] == '-');
    int start_pos = (minus ? 1 : 0);

    if (start_pos == len) {
      return isolate->heap()->nan_value();
    } else if (data[start_pos] > '9') {
      // Fast check for a junk value. A valid string may start from a
      // whitespace, a sign ('+' or '-'), the decimal point, a decimal digit
      // or the 'I' character ('Infinity'). All of that have codes not greater
      // than '9' except 'I' and &nbsp;.
      if (data[start_pos] != 'I' && data[start_pos] != 0xa0) {
        return isolate->heap()->nan_value();
      }
    } else if (len - start_pos < 10 && AreDigits(data, start_pos, len)) {
      // The maximal/minimal smi has 10 digits. If the string has less digits
      // we know it will fit into the smi-data type.
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
  int flags = ALLOW_HEX;
  if (FLAG_harmony_numeric_literals) {
    // The current spec draft has not updated "ToNumber Applied to the String
    // Type", https://bugs.ecmascript.org/show_bug.cgi?id=1584
    flags |= ALLOW_OCTAL | ALLOW_BINARY;
  }

  return *isolate->factory()->NewNumber(StringToDouble(
      isolate->unicode_cache(), *subject, flags));
}


RUNTIME_FUNCTION(Runtime_NewString) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  CONVERT_SMI_ARG_CHECKED(length, 0);
  CONVERT_BOOLEAN_ARG_CHECKED(is_one_byte, 1);
  if (length == 0) return isolate->heap()->empty_string();
  Handle<String> result;
  if (is_one_byte) {
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, result, isolate->factory()->NewRawOneByteString(length));
  } else {
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, result, isolate->factory()->NewRawTwoByteString(length));
  }
  return *result;
}


RUNTIME_FUNCTION(Runtime_TruncateString) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(SeqString, string, 0);
  CONVERT_SMI_ARG_CHECKED(new_length, 1);
  RUNTIME_ASSERT(new_length >= 0);
  return *SeqString::Truncate(string, new_length);
}


RUNTIME_FUNCTION(Runtime_URIEscape) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(String, source, 0);
  Handle<String> string = String::Flatten(source);
  ASSERT(string->IsFlat());
  Handle<String> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      string->IsOneByteRepresentationUnderneath()
            ? URIEscape::Escape<uint8_t>(isolate, source)
            : URIEscape::Escape<uc16>(isolate, source));
  return *result;
}


RUNTIME_FUNCTION(Runtime_URIUnescape) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(String, source, 0);
  Handle<String> string = String::Flatten(source);
  ASSERT(string->IsFlat());
  Handle<String> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      string->IsOneByteRepresentationUnderneath()
            ? URIUnescape::Unescape<uint8_t>(isolate, source)
            : URIUnescape::Unescape<uc16>(isolate, source));
  return *result;
}


RUNTIME_FUNCTION(Runtime_QuoteJSONString) {
  HandleScope scope(isolate);
  CONVERT_ARG_HANDLE_CHECKED(String, string, 0);
  ASSERT(args.length() == 1);
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result, BasicJsonStringifier::StringifyString(isolate, string));
  return *result;
}


RUNTIME_FUNCTION(Runtime_BasicJSONStringify) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, object, 0);
  BasicJsonStringifier stringifier(isolate);
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result, stringifier.Stringify(object));
  return *result;
}


RUNTIME_FUNCTION(Runtime_StringParseInt) {
  HandleScope handle_scope(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(String, subject, 0);
  CONVERT_NUMBER_CHECKED(int, radix, Int32, args[1]);
  RUNTIME_ASSERT(radix == 0 || (2 <= radix && radix <= 36));

  subject = String::Flatten(subject);
  double value;

  { DisallowHeapAllocation no_gc;
    String::FlatContent flat = subject->GetFlatContent();

    // ECMA-262 section 15.1.2.3, empty string is NaN
    if (flat.IsAscii()) {
      value = StringToInt(
          isolate->unicode_cache(), flat.ToOneByteVector(), radix);
    } else {
      value = StringToInt(
          isolate->unicode_cache(), flat.ToUC16Vector(), radix);
    }
  }

  return *isolate->factory()->NewNumber(value);
}


RUNTIME_FUNCTION(Runtime_StringParseFloat) {
  HandleScope shs(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(String, subject, 0);

  subject = String::Flatten(subject);
  double value = StringToDouble(
      isolate->unicode_cache(), *subject, ALLOW_TRAILING_JUNK, OS::nan_value());

  return *isolate->factory()->NewNumber(value);
}


static inline bool ToUpperOverflows(uc32 character) {
  // y with umlauts and the micro sign are the only characters that stop
  // fitting into one-byte when converting to uppercase.
  static const uc32 yuml_code = 0xff;
  static const uc32 micro_code = 0xb5;
  return (character == yuml_code || character == micro_code);
}


template <class Converter>
MUST_USE_RESULT static Object* ConvertCaseHelper(
    Isolate* isolate,
    String* string,
    SeqString* result,
    int result_length,
    unibrow::Mapping<Converter, 128>* mapping) {
  DisallowHeapAllocation no_gc;
  // We try this twice, once with the assumption that the result is no longer
  // than the input and, if that assumption breaks, again with the exact
  // length.  This may not be pretty, but it is nicer than what was here before
  // and I hereby claim my vaffel-is.
  //
  // NOTE: This assumes that the upper/lower case of an ASCII
  // character is also ASCII.  This is currently the case, but it
  // might break in the future if we implement more context and locale
  // dependent upper/lower conversions.
  bool has_changed_character = false;

  // Convert all characters to upper case, assuming that they will fit
  // in the buffer
  Access<ConsStringIteratorOp> op(
      isolate->runtime_state()->string_iterator());
  StringCharacterStream stream(string, op.value());
  unibrow::uchar chars[Converter::kMaxWidth];
  // We can assume that the string is not empty
  uc32 current = stream.GetNext();
  bool ignore_overflow = Converter::kIsToLower || result->IsSeqTwoByteString();
  for (int i = 0; i < result_length;) {
    bool has_next = stream.HasMore();
    uc32 next = has_next ? stream.GetNext() : 0;
    int char_length = mapping->get(current, next, chars);
    if (char_length == 0) {
      // The case conversion of this character is the character itself.
      result->Set(i, current);
      i++;
    } else if (char_length == 1 &&
               (ignore_overflow || !ToUpperOverflows(current))) {
      // Common case: converting the letter resulted in one character.
      ASSERT(static_cast<uc32>(chars[0]) != current);
      result->Set(i, chars[0]);
      has_changed_character = true;
      i++;
    } else if (result_length == string->length()) {
      bool overflows = ToUpperOverflows(current);
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
        overflows |= ToUpperOverflows(current);
        // NOTE: we use 0 as the next character here because, while
        // the next character may affect what a character converts to,
        // it does not in any case affect the length of what it convert
        // to.
        int char_length = mapping->get(current, 0, chars);
        if (char_length == 0) char_length = 1;
        current_length += char_length;
        if (current_length > String::kMaxLength) {
          AllowHeapAllocation allocate_error_and_return;
          return isolate->ThrowInvalidStringLength();
        }
      }
      // Try again with the real length.  Return signed if we need
      // to allocate a two-byte string for to uppercase.
      return (overflows && !ignore_overflow) ? Smi::FromInt(-current_length)
                                             : Smi::FromInt(current_length);
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
    return string;
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


#ifdef DEBUG
static bool CheckFastAsciiConvert(char* dst,
                                  const char* src,
                                  int length,
                                  bool changed,
                                  bool is_to_lower) {
  bool expected_changed = false;
  for (int i = 0; i < length; i++) {
    if (dst[i] == src[i]) continue;
    expected_changed = true;
    if (is_to_lower) {
      ASSERT('A' <= src[i] && src[i] <= 'Z');
      ASSERT(dst[i] == src[i] + ('a' - 'A'));
    } else {
      ASSERT('a' <= src[i] && src[i] <= 'z');
      ASSERT(dst[i] == src[i] - ('a' - 'A'));
    }
  }
  return (expected_changed == changed);
}
#endif


template<class Converter>
static bool FastAsciiConvert(char* dst,
                             const char* src,
                             int length,
                             bool* changed_out) {
#ifdef DEBUG
    char* saved_dst = dst;
    const char* saved_src = src;
#endif
  DisallowHeapAllocation no_gc;
  // We rely on the distance between upper and lower case letters
  // being a known power of 2.
  ASSERT('a' - 'A' == (1 << 5));
  // Boundaries for the range of input characters than require conversion.
  static const char lo = Converter::kIsToLower ? 'A' - 1 : 'a' - 1;
  static const char hi = Converter::kIsToLower ? 'Z' + 1 : 'z' + 1;
  bool changed = false;
  uintptr_t or_acc = 0;
  const char* const limit = src + length;
#ifdef V8_HOST_CAN_READ_UNALIGNED
  // Process the prefix of the input that requires no conversion one
  // (machine) word at a time.
  while (src <= limit - sizeof(uintptr_t)) {
    const uintptr_t w = *reinterpret_cast<const uintptr_t*>(src);
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
    const uintptr_t w = *reinterpret_cast<const uintptr_t*>(src);
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

  ASSERT(CheckFastAsciiConvert(
             saved_dst, saved_src, length, changed, Converter::kIsToLower));

  *changed_out = changed;
  return true;
}

}  // namespace


template <class Converter>
MUST_USE_RESULT static Object* ConvertCase(
    Handle<String> s,
    Isolate* isolate,
    unibrow::Mapping<Converter, 128>* mapping) {
  s = String::Flatten(s);
  int length = s->length();
  // Assume that the string is not empty; we need this assumption later
  if (length == 0) return *s;

  // Simpler handling of ASCII strings.
  //
  // NOTE: This assumes that the upper/lower case of an ASCII
  // character is also ASCII.  This is currently the case, but it
  // might break in the future if we implement more context and locale
  // dependent upper/lower conversions.
  if (s->IsOneByteRepresentationUnderneath()) {
    // Same length as input.
    Handle<SeqOneByteString> result =
        isolate->factory()->NewRawOneByteString(length).ToHandleChecked();
    DisallowHeapAllocation no_gc;
    String::FlatContent flat_content = s->GetFlatContent();
    ASSERT(flat_content.IsFlat());
    bool has_changed_character = false;
    bool is_ascii = FastAsciiConvert<Converter>(
        reinterpret_cast<char*>(result->GetChars()),
        reinterpret_cast<const char*>(flat_content.ToOneByteVector().start()),
        length,
        &has_changed_character);
    // If not ASCII, we discard the result and take the 2 byte path.
    if (is_ascii) return has_changed_character ? *result : *s;
  }

  Handle<SeqString> result;  // Same length as input.
  if (s->IsOneByteRepresentation()) {
    result = isolate->factory()->NewRawOneByteString(length).ToHandleChecked();
  } else {
    result = isolate->factory()->NewRawTwoByteString(length).ToHandleChecked();
  }

  Object* answer = ConvertCaseHelper(isolate, *s, *result, length, mapping);
  if (answer->IsException() || answer->IsString()) return answer;

  ASSERT(answer->IsSmi());
  length = Smi::cast(answer)->value();
  if (s->IsOneByteRepresentation() && length > 0) {
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, result, isolate->factory()->NewRawOneByteString(length));
  } else {
    if (length < 0) length = -length;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, result, isolate->factory()->NewRawTwoByteString(length));
  }
  return ConvertCaseHelper(isolate, *s, *result, length, mapping);
}


RUNTIME_FUNCTION(Runtime_StringToLowerCase) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(String, s, 0);
  return ConvertCase(
      s, isolate, isolate->runtime_state()->to_lower_mapping());
}


RUNTIME_FUNCTION(Runtime_StringToUpperCase) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(String, s, 0);
  return ConvertCase(
      s, isolate, isolate->runtime_state()->to_upper_mapping());
}


RUNTIME_FUNCTION(Runtime_StringTrim) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 3);

  CONVERT_ARG_HANDLE_CHECKED(String, string, 0);
  CONVERT_BOOLEAN_ARG_CHECKED(trimLeft, 1);
  CONVERT_BOOLEAN_ARG_CHECKED(trimRight, 2);

  string = String::Flatten(string);
  int length = string->length();

  int left = 0;
  UnicodeCache* unicode_cache = isolate->unicode_cache();
  if (trimLeft) {
    while (left < length &&
           unicode_cache->IsWhiteSpaceOrLineTerminator(string->Get(left))) {
      left++;
    }
  }

  int right = length;
  if (trimRight) {
    while (right > left &&
           unicode_cache->IsWhiteSpaceOrLineTerminator(
               string->Get(right - 1))) {
      right--;
    }
  }

  return *isolate->factory()->NewSubString(string, left, right);
}


RUNTIME_FUNCTION(Runtime_StringSplit) {
  HandleScope handle_scope(isolate);
  ASSERT(args.length() == 3);
  CONVERT_ARG_HANDLE_CHECKED(String, subject, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, pattern, 1);
  CONVERT_NUMBER_CHECKED(uint32_t, limit, Uint32, args[2]);
  RUNTIME_ASSERT(limit > 0);

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

  subject = String::Flatten(subject);
  pattern = String::Flatten(pattern);

  static const int kMaxInitialListCapacity = 16;

  ZoneScope zone_scope(isolate->runtime_zone());

  // Find (up to limit) indices of separator and end-of-string in subject
  int initial_capacity = Min<uint32_t>(kMaxInitialListCapacity, limit);
  ZoneList<int> indices(initial_capacity, zone_scope.zone());

  FindStringIndicesDispatch(isolate, *subject, *pattern,
                            &indices, limit, zone_scope.zone());

  if (static_cast<uint32_t>(indices.length()) < limit) {
    indices.Add(subject_length, zone_scope.zone());
  }

  // The list indices now contains the end of each part to create.

  // Create JSArray of substrings separated by separator.
  int part_count = indices.length();

  Handle<JSArray> result = isolate->factory()->NewJSArray(part_count);
  JSObject::EnsureCanContainHeapObjectElements(result);
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
      RegExpResultsCache::Enter(isolate,
                                subject,
                                pattern,
                                elements,
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
RUNTIME_FUNCTION(Runtime_StringToArray) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(String, s, 0);
  CONVERT_NUMBER_CHECKED(uint32_t, limit, Uint32, args[1]);

  s = String::Flatten(s);
  const int length = static_cast<int>(Min<uint32_t>(s->length(), limit));

  Handle<FixedArray> elements;
  int position = 0;
  if (s->IsFlat() && s->IsOneByteRepresentation()) {
    // Try using cached chars where possible.
    elements = isolate->factory()->NewUninitializedFixedArray(length);

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
        isolate->factory()->LookupSingleCharacterStringFromCode(s->Get(i));
    elements->set(i, *str);
  }

#ifdef DEBUG
  for (int i = 0; i < length; ++i) {
    ASSERT(String::cast(elements->get(i))->length() == 1);
  }
#endif

  return *isolate->factory()->NewJSArrayWithElements(elements);
}


RUNTIME_FUNCTION(Runtime_NewStringWrapper) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(String, value, 0);
  return *Object::ToObject(isolate, value).ToHandleChecked();
}


bool Runtime::IsUpperCaseChar(RuntimeState* runtime_state, uint16_t ch) {
  unibrow::uchar chars[unibrow::ToUppercase::kMaxWidth];
  int char_length = runtime_state->to_upper_mapping()->get(ch, 0, chars);
  return char_length == 0;
}


RUNTIME_FUNCTION(RuntimeHidden_NumberToString) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_NUMBER_ARG_HANDLE_CHECKED(number, 0);

  return *isolate->factory()->NumberToString(number);
}


RUNTIME_FUNCTION(RuntimeHidden_NumberToStringSkipCache) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_NUMBER_ARG_HANDLE_CHECKED(number, 0);

  return *isolate->factory()->NumberToString(number, false);
}


RUNTIME_FUNCTION(Runtime_NumberToInteger) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);

  CONVERT_DOUBLE_ARG_CHECKED(number, 0);
  return *isolate->factory()->NewNumber(DoubleToInteger(number));
}


RUNTIME_FUNCTION(Runtime_NumberToIntegerMapMinusZero) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);

  CONVERT_DOUBLE_ARG_CHECKED(number, 0);
  double double_value = DoubleToInteger(number);
  // Map both -0 and +0 to +0.
  if (double_value == 0) double_value = 0;

  return *isolate->factory()->NewNumber(double_value);
}


RUNTIME_FUNCTION(Runtime_NumberToJSUint32) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);

  CONVERT_NUMBER_CHECKED(int32_t, number, Uint32, args[0]);
  return *isolate->factory()->NewNumberFromUint(number);
}


RUNTIME_FUNCTION(Runtime_NumberToJSInt32) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);

  CONVERT_DOUBLE_ARG_CHECKED(number, 0);
  return *isolate->factory()->NewNumberFromInt(DoubleToInt32(number));
}


// Converts a Number to a Smi, if possible. Returns NaN if the number is not
// a small integer.
RUNTIME_FUNCTION(RuntimeHidden_NumberToSmi) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_CHECKED(Object, obj, 0);
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


RUNTIME_FUNCTION(RuntimeHidden_AllocateHeapNumber) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 0);
  return *isolate->factory()->NewHeapNumber(0);
}


RUNTIME_FUNCTION(Runtime_NumberAdd) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);

  CONVERT_DOUBLE_ARG_CHECKED(x, 0);
  CONVERT_DOUBLE_ARG_CHECKED(y, 1);
  return *isolate->factory()->NewNumber(x + y);
}


RUNTIME_FUNCTION(Runtime_NumberSub) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);

  CONVERT_DOUBLE_ARG_CHECKED(x, 0);
  CONVERT_DOUBLE_ARG_CHECKED(y, 1);
  return *isolate->factory()->NewNumber(x - y);
}


RUNTIME_FUNCTION(Runtime_NumberMul) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);

  CONVERT_DOUBLE_ARG_CHECKED(x, 0);
  CONVERT_DOUBLE_ARG_CHECKED(y, 1);
  return *isolate->factory()->NewNumber(x * y);
}


RUNTIME_FUNCTION(Runtime_NumberUnaryMinus) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);

  CONVERT_DOUBLE_ARG_CHECKED(x, 0);
  return *isolate->factory()->NewNumber(-x);
}


RUNTIME_FUNCTION(Runtime_NumberDiv) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);

  CONVERT_DOUBLE_ARG_CHECKED(x, 0);
  CONVERT_DOUBLE_ARG_CHECKED(y, 1);
  return *isolate->factory()->NewNumber(x / y);
}


RUNTIME_FUNCTION(Runtime_NumberMod) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);

  CONVERT_DOUBLE_ARG_CHECKED(x, 0);
  CONVERT_DOUBLE_ARG_CHECKED(y, 1);
  return *isolate->factory()->NewNumber(modulo(x, y));
}


RUNTIME_FUNCTION(Runtime_NumberImul) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);

  CONVERT_NUMBER_CHECKED(int32_t, x, Int32, args[0]);
  CONVERT_NUMBER_CHECKED(int32_t, y, Int32, args[1]);
  return *isolate->factory()->NewNumberFromInt(x * y);
}


RUNTIME_FUNCTION(RuntimeHidden_StringAdd) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(String, str1, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, str2, 1);
  isolate->counters()->string_add_runtime()->Increment();
  Handle<String> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result, isolate->factory()->NewConsString(str1, str2));
  return *result;
}


template <typename sinkchar>
static inline void StringBuilderConcatHelper(String* special,
                                             sinkchar* sink,
                                             FixedArray* fixed_array,
                                             int array_length) {
  DisallowHeapAllocation no_gc;
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


// Returns the result length of the concatenation.
// On illegal argument, -1 is returned.
static inline int StringBuilderConcatLength(int special_length,
                                            FixedArray* fixed_array,
                                            int array_length,
                                            bool* one_byte) {
  DisallowHeapAllocation no_gc;
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
        if (i >= array_length) return -1;
        Object* next_smi = fixed_array->get(i);
        if (!next_smi->IsSmi()) return -1;
        pos = Smi::cast(next_smi)->value();
        if (pos < 0) return -1;
      }
      ASSERT(pos >= 0);
      ASSERT(len >= 0);
      if (pos > special_length || len > special_length - pos) return -1;
      increment = len;
    } else if (elt->IsString()) {
      String* element = String::cast(elt);
      int element_length = element->length();
      increment = element_length;
      if (*one_byte && !element->HasOnlyOneByteChars()) {
        *one_byte = false;
      }
    } else {
      return -1;
    }
    if (increment > String::kMaxLength - position) {
      return kMaxInt;  // Provoke throw on allocation.
    }
    position += increment;
  }
  return position;
}


RUNTIME_FUNCTION(Runtime_StringBuilderConcat) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 3);
  CONVERT_ARG_HANDLE_CHECKED(JSArray, array, 0);
  if (!args[1]->IsSmi()) return isolate->ThrowInvalidStringLength();
  CONVERT_SMI_ARG_CHECKED(array_length, 1);
  CONVERT_ARG_HANDLE_CHECKED(String, special, 2);

  size_t actual_array_length = 0;
  RUNTIME_ASSERT(
      TryNumberToSize(isolate, array->length(), &actual_array_length));
  RUNTIME_ASSERT(array_length >= 0);
  RUNTIME_ASSERT(static_cast<size_t>(array_length) <= actual_array_length);

  // This assumption is used by the slice encoding in one or two smis.
  ASSERT(Smi::kMaxValue >= String::kMaxLength);

  RUNTIME_ASSERT(array->HasFastElements());
  JSObject::EnsureCanContainHeapObjectElements(array);

  int special_length = special->length();
  if (!array->HasFastObjectElements()) {
    return isolate->Throw(isolate->heap()->illegal_argument_string());
  }

  int length;
  bool one_byte = special->HasOnlyOneByteChars();

  { DisallowHeapAllocation no_gc;
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
    length = StringBuilderConcatLength(
        special_length, fixed_array, array_length, &one_byte);
  }

  if (length == -1) {
    return isolate->Throw(isolate->heap()->illegal_argument_string());
  }

  if (one_byte) {
    Handle<SeqOneByteString> answer;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, answer,
        isolate->factory()->NewRawOneByteString(length));
    StringBuilderConcatHelper(*special,
                              answer->GetChars(),
                              FixedArray::cast(array->elements()),
                              array_length);
    return *answer;
  } else {
    Handle<SeqTwoByteString> answer;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, answer,
        isolate->factory()->NewRawTwoByteString(length));
    StringBuilderConcatHelper(*special,
                              answer->GetChars(),
                              FixedArray::cast(array->elements()),
                              array_length);
    return *answer;
  }
}


RUNTIME_FUNCTION(Runtime_StringBuilderJoin) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 3);
  CONVERT_ARG_HANDLE_CHECKED(JSArray, array, 0);
  if (!args[1]->IsSmi()) return isolate->ThrowInvalidStringLength();
  CONVERT_SMI_ARG_CHECKED(array_length, 1);
  CONVERT_ARG_HANDLE_CHECKED(String, separator, 2);
  RUNTIME_ASSERT(array->HasFastObjectElements());

  Handle<FixedArray> fixed_array(FixedArray::cast(array->elements()));
  if (fixed_array->length() < array_length) {
    array_length = fixed_array->length();
  }

  if (array_length == 0) {
    return isolate->heap()->empty_string();
  } else if (array_length == 1) {
    Object* first = fixed_array->get(0);
    RUNTIME_ASSERT(first->IsString());
    return first;
  }

  int separator_length = separator->length();
  RUNTIME_ASSERT(separator_length > 0);
  int max_nof_separators =
      (String::kMaxLength + separator_length - 1) / separator_length;
  if (max_nof_separators < (array_length - 1)) {
    return isolate->ThrowInvalidStringLength();
  }
  int length = (array_length - 1) * separator_length;
  for (int i = 0; i < array_length; i++) {
    Object* element_obj = fixed_array->get(i);
    RUNTIME_ASSERT(element_obj->IsString());
    String* element = String::cast(element_obj);
    int increment = element->length();
    if (increment > String::kMaxLength - length) {
      STATIC_ASSERT(String::kMaxLength < kMaxInt);
      length = kMaxInt;  // Provoke exception;
      break;
    }
    length += increment;
  }

  Handle<SeqTwoByteString> answer;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, answer,
      isolate->factory()->NewRawTwoByteString(length));

  DisallowHeapAllocation no_gc;

  uc16* sink = answer->GetChars();
#ifdef DEBUG
  uc16* end = sink + length;
#endif

  String* first = String::cast(fixed_array->get(0));
  String* seperator_raw = *separator;
  int first_length = first->length();
  String::WriteToFlat(first, sink, 0, first_length);
  sink += first_length;

  for (int i = 1; i < array_length; i++) {
    ASSERT(sink + separator_length <= end);
    String::WriteToFlat(seperator_raw, sink, 0, separator_length);
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
  return *answer;
}

template <typename Char>
static void JoinSparseArrayWithSeparator(FixedArray* elements,
                                         int elements_length,
                                         uint32_t array_length,
                                         String* separator,
                                         Vector<Char> buffer) {
  DisallowHeapAllocation no_gc;
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


RUNTIME_FUNCTION(Runtime_SparseJoinWithSeparator) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 3);
  CONVERT_ARG_HANDLE_CHECKED(JSArray, elements_array, 0);
  CONVERT_NUMBER_CHECKED(uint32_t, array_length, Uint32, args[1]);
  CONVERT_ARG_HANDLE_CHECKED(String, separator, 2);
  // elements_array is fast-mode JSarray of alternating positions
  // (increasing order) and strings.
  RUNTIME_ASSERT(elements_array->HasFastSmiOrObjectElements());
  // array_length is length of original array (used to add separators);
  // separator is string to put between elements. Assumed to be non-empty.
  RUNTIME_ASSERT(array_length > 0);

  // Find total length of join result.
  int string_length = 0;
  bool is_ascii = separator->IsOneByteRepresentation();
  bool overflow = false;
  CONVERT_NUMBER_CHECKED(int, elements_length, Int32, elements_array->length());
  RUNTIME_ASSERT(elements_length <= elements_array->elements()->length());
  RUNTIME_ASSERT((elements_length & 1) == 0);  // Even length.
  FixedArray* elements = FixedArray::cast(elements_array->elements());
  for (int i = 0; i < elements_length; i += 2) {
    RUNTIME_ASSERT(elements->get(i)->IsNumber());
    RUNTIME_ASSERT(elements->get(i + 1)->IsString());
  }

  { DisallowHeapAllocation no_gc;
    for (int i = 0; i < elements_length; i += 2) {
      String* string = String::cast(elements->get(i + 1));
      int length = string->length();
      if (is_ascii && !string->IsOneByteRepresentation()) {
        is_ascii = false;
      }
      if (length > String::kMaxLength ||
          String::kMaxLength - length < string_length) {
        overflow = true;
        break;
      }
      string_length += length;
    }
  }

  int separator_length = separator->length();
  if (!overflow && separator_length > 0) {
    if (array_length <= 0x7fffffffu) {
      int separator_count = static_cast<int>(array_length) - 1;
      int remaining_length = String::kMaxLength - string_length;
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
    // Throw an exception if the resulting string is too large. See
    // https://code.google.com/p/chromium/issues/detail?id=336820
    // for details.
    return isolate->ThrowInvalidStringLength();
  }

  if (is_ascii) {
    Handle<SeqOneByteString> result = isolate->factory()->NewRawOneByteString(
        string_length).ToHandleChecked();
    JoinSparseArrayWithSeparator<uint8_t>(
        FixedArray::cast(elements_array->elements()),
        elements_length,
        array_length,
        *separator,
        Vector<uint8_t>(result->GetChars(), string_length));
    return *result;
  } else {
    Handle<SeqTwoByteString> result = isolate->factory()->NewRawTwoByteString(
        string_length).ToHandleChecked();
    JoinSparseArrayWithSeparator<uc16>(
        FixedArray::cast(elements_array->elements()),
        elements_length,
        array_length,
        *separator,
        Vector<uc16>(result->GetChars(), string_length));
    return *result;
  }
}


RUNTIME_FUNCTION(Runtime_NumberOr) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);

  CONVERT_NUMBER_CHECKED(int32_t, x, Int32, args[0]);
  CONVERT_NUMBER_CHECKED(int32_t, y, Int32, args[1]);
  return *isolate->factory()->NewNumberFromInt(x | y);
}


RUNTIME_FUNCTION(Runtime_NumberAnd) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);

  CONVERT_NUMBER_CHECKED(int32_t, x, Int32, args[0]);
  CONVERT_NUMBER_CHECKED(int32_t, y, Int32, args[1]);
  return *isolate->factory()->NewNumberFromInt(x & y);
}


RUNTIME_FUNCTION(Runtime_NumberXor) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);

  CONVERT_NUMBER_CHECKED(int32_t, x, Int32, args[0]);
  CONVERT_NUMBER_CHECKED(int32_t, y, Int32, args[1]);
  return *isolate->factory()->NewNumberFromInt(x ^ y);
}


RUNTIME_FUNCTION(Runtime_NumberShl) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);

  CONVERT_NUMBER_CHECKED(int32_t, x, Int32, args[0]);
  CONVERT_NUMBER_CHECKED(int32_t, y, Int32, args[1]);
  return *isolate->factory()->NewNumberFromInt(x << (y & 0x1f));
}


RUNTIME_FUNCTION(Runtime_NumberShr) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);

  CONVERT_NUMBER_CHECKED(uint32_t, x, Uint32, args[0]);
  CONVERT_NUMBER_CHECKED(int32_t, y, Int32, args[1]);
  return *isolate->factory()->NewNumberFromUint(x >> (y & 0x1f));
}


RUNTIME_FUNCTION(Runtime_NumberSar) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);

  CONVERT_NUMBER_CHECKED(int32_t, x, Int32, args[0]);
  CONVERT_NUMBER_CHECKED(int32_t, y, Int32, args[1]);
  return *isolate->factory()->NewNumberFromInt(
      ArithmeticShiftRight(x, y & 0x1f));
}


RUNTIME_FUNCTION(Runtime_NumberEquals) {
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


RUNTIME_FUNCTION(Runtime_StringEquals) {
  HandleScope handle_scope(isolate);
  ASSERT(args.length() == 2);

  CONVERT_ARG_HANDLE_CHECKED(String, x, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, y, 1);

  bool not_equal = !String::Equals(x, y);
  // This is slightly convoluted because the value that signifies
  // equality is 0 and inequality is 1 so we have to negate the result
  // from String::Equals.
  ASSERT(not_equal == 0 || not_equal == 1);
  STATIC_CHECK(EQUAL == 0);
  STATIC_CHECK(NOT_EQUAL == 1);
  return Smi::FromInt(not_equal);
}


RUNTIME_FUNCTION(Runtime_NumberCompare) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 3);

  CONVERT_DOUBLE_ARG_CHECKED(x, 0);
  CONVERT_DOUBLE_ARG_CHECKED(y, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, uncomparable_result, 2)
  if (std::isnan(x) || std::isnan(y)) return *uncomparable_result;
  if (x == y) return Smi::FromInt(EQUAL);
  if (isless(x, y)) return Smi::FromInt(LESS);
  return Smi::FromInt(GREATER);
}


// Compare two Smis as if they were converted to strings and then
// compared lexicographically.
RUNTIME_FUNCTION(Runtime_SmiLexicographicCompare) {
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


RUNTIME_FUNCTION(RuntimeHidden_StringCompare) {
  HandleScope handle_scope(isolate);
  ASSERT(args.length() == 2);

  CONVERT_ARG_HANDLE_CHECKED(String, x, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, y, 1);

  isolate->counters()->string_compare_runtime()->Increment();

  // A few fast case tests before we flatten.
  if (x.is_identical_to(y)) return Smi::FromInt(EQUAL);
  if (y->length() == 0) {
    if (x->length() == 0) return Smi::FromInt(EQUAL);
    return Smi::FromInt(GREATER);
  } else if (x->length() == 0) {
    return Smi::FromInt(LESS);
  }

  int d = x->Get(0) - y->Get(0);
  if (d < 0) return Smi::FromInt(LESS);
  else if (d > 0) return Smi::FromInt(GREATER);

  // Slow case.
  x = String::Flatten(x);
  y = String::Flatten(y);

  DisallowHeapAllocation no_gc;
  Object* equal_prefix_result = Smi::FromInt(EQUAL);
  int prefix_length = x->length();
  if (y->length() < prefix_length) {
    prefix_length = y->length();
    equal_prefix_result = Smi::FromInt(GREATER);
  } else if (y->length() > prefix_length) {
    equal_prefix_result = Smi::FromInt(LESS);
  }
  int r;
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
  return result;
}


#define RUNTIME_UNARY_MATH(Name, name)                                         \
RUNTIME_FUNCTION(Runtime_Math##Name) {                           \
  HandleScope scope(isolate);                                                  \
  ASSERT(args.length() == 1);                                                  \
  isolate->counters()->math_##name()->Increment();                             \
  CONVERT_DOUBLE_ARG_CHECKED(x, 0);                                            \
  return *isolate->factory()->NewHeapNumber(std::name(x));                     \
}

RUNTIME_UNARY_MATH(Acos, acos)
RUNTIME_UNARY_MATH(Asin, asin)
RUNTIME_UNARY_MATH(Atan, atan)
RUNTIME_UNARY_MATH(Log, log)
#undef RUNTIME_UNARY_MATH


RUNTIME_FUNCTION(Runtime_DoubleHi) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_DOUBLE_ARG_CHECKED(x, 0);
  uint64_t integer = double_to_uint64(x);
  integer = (integer >> 32) & 0xFFFFFFFFu;
  return *isolate->factory()->NewNumber(static_cast<int32_t>(integer));
}


RUNTIME_FUNCTION(Runtime_DoubleLo) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_DOUBLE_ARG_CHECKED(x, 0);
  return *isolate->factory()->NewNumber(
      static_cast<int32_t>(double_to_uint64(x) & 0xFFFFFFFFu));
}


RUNTIME_FUNCTION(Runtime_ConstructDouble) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  CONVERT_NUMBER_CHECKED(uint32_t, hi, Uint32, args[0]);
  CONVERT_NUMBER_CHECKED(uint32_t, lo, Uint32, args[1]);
  uint64_t result = (static_cast<uint64_t>(hi) << 32) | lo;
  return *isolate->factory()->NewNumber(uint64_to_double(result));
}


static const double kPiDividedBy4 = 0.78539816339744830962;


RUNTIME_FUNCTION(Runtime_MathAtan2) {
  HandleScope scope(isolate);
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
    result = std::atan2(x, y);
  }
  return *isolate->factory()->NewNumber(result);
}


RUNTIME_FUNCTION(Runtime_MathExp) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  isolate->counters()->math_exp()->Increment();

  CONVERT_DOUBLE_ARG_CHECKED(x, 0);
  lazily_initialize_fast_exp();
  return *isolate->factory()->NewNumber(fast_exp(x));
}


RUNTIME_FUNCTION(Runtime_MathFloor) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  isolate->counters()->math_floor()->Increment();

  CONVERT_DOUBLE_ARG_CHECKED(x, 0);
  return *isolate->factory()->NewNumber(std::floor(x));
}


// Slow version of Math.pow.  We check for fast paths for special cases.
// Used if SSE2/VFP3 is not available.
RUNTIME_FUNCTION(RuntimeHidden_MathPowSlow) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  isolate->counters()->math_pow()->Increment();

  CONVERT_DOUBLE_ARG_CHECKED(x, 0);

  // If the second argument is a smi, it is much faster to call the
  // custom powi() function than the generic pow().
  if (args[1]->IsSmi()) {
    int y = args.smi_at(1);
    return *isolate->factory()->NewNumber(power_double_int(x, y));
  }

  CONVERT_DOUBLE_ARG_CHECKED(y, 1);
  double result = power_helper(x, y);
  if (std::isnan(result)) return isolate->heap()->nan_value();
  return *isolate->factory()->NewNumber(result);
}


// Fast version of Math.pow if we know that y is not an integer and y is not
// -0.5 or 0.5.  Used as slow case from full codegen.
RUNTIME_FUNCTION(RuntimeHidden_MathPow) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  isolate->counters()->math_pow()->Increment();

  CONVERT_DOUBLE_ARG_CHECKED(x, 0);
  CONVERT_DOUBLE_ARG_CHECKED(y, 1);
  if (y == 0) {
    return Smi::FromInt(1);
  } else {
    double result = power_double_double(x, y);
    if (std::isnan(result)) return isolate->heap()->nan_value();
    return *isolate->factory()->NewNumber(result);
  }
}


RUNTIME_FUNCTION(Runtime_RoundNumber) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_NUMBER_ARG_HANDLE_CHECKED(input, 0);
  isolate->counters()->math_round()->Increment();

  if (!input->IsHeapNumber()) {
    ASSERT(input->IsSmi());
    return *input;
  }

  Handle<HeapNumber> number = Handle<HeapNumber>::cast(input);

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
    return *number;
  }

  if (sign && value >= -0.5) return isolate->heap()->minus_zero_value();

  // Do not call NumberFromDouble() to avoid extra checks.
  return *isolate->factory()->NewNumber(std::floor(value + 0.5));
}


RUNTIME_FUNCTION(Runtime_MathSqrt) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  isolate->counters()->math_sqrt()->Increment();

  CONVERT_DOUBLE_ARG_CHECKED(x, 0);
  return *isolate->factory()->NewNumber(fast_sqrt(x));
}


RUNTIME_FUNCTION(Runtime_MathFround) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);

  CONVERT_DOUBLE_ARG_CHECKED(x, 0);
  float xf = static_cast<float>(x);
  return *isolate->factory()->NewNumber(xf);
}


RUNTIME_FUNCTION(Runtime_DateMakeDay) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 2);

  CONVERT_SMI_ARG_CHECKED(year, 0);
  CONVERT_SMI_ARG_CHECKED(month, 1);

  int days = isolate->date_cache()->DaysFromYearMonth(year, month);
  RUNTIME_ASSERT(Smi::IsValid(days));
  return Smi::FromInt(days);
}


RUNTIME_FUNCTION(Runtime_DateSetValue) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 3);

  CONVERT_ARG_HANDLE_CHECKED(JSDate, date, 0);
  CONVERT_DOUBLE_ARG_CHECKED(time, 1);
  CONVERT_SMI_ARG_CHECKED(is_utc, 2);

  DateCache* date_cache = isolate->date_cache();

  Handle<Object> value;;
  bool is_value_nan = false;
  if (std::isnan(time)) {
    value = isolate->factory()->nan_value();
    is_value_nan = true;
  } else if (!is_utc &&
             (time < -DateCache::kMaxTimeBeforeUTCInMs ||
              time > DateCache::kMaxTimeBeforeUTCInMs)) {
    value = isolate->factory()->nan_value();
    is_value_nan = true;
  } else {
    time = is_utc ? time : date_cache->ToUTC(static_cast<int64_t>(time));
    if (time < -DateCache::kMaxTimeInMs ||
        time > DateCache::kMaxTimeInMs) {
      value = isolate->factory()->nan_value();
      is_value_nan = true;
    } else  {
      value = isolate->factory()->NewNumber(DoubleToInteger(time));
    }
  }
  date->SetValue(*value, is_value_nan);
  return *value;
}


RUNTIME_FUNCTION(RuntimeHidden_NewArgumentsFast) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 3);

  CONVERT_ARG_HANDLE_CHECKED(JSFunction, callee, 0);
  Object** parameters = reinterpret_cast<Object**>(args[1]);
  CONVERT_SMI_ARG_CHECKED(argument_count, 2);

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
          isolate->heap()->sloppy_arguments_elements_map());

      Handle<Map> map = Map::Copy(handle(result->map()));
      map->set_elements_kind(SLOPPY_ARGUMENTS_ELEMENTS);

      result->set_map(*map);
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


RUNTIME_FUNCTION(RuntimeHidden_NewStrictArgumentsFast) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 3);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, callee, 0)
  Object** parameters = reinterpret_cast<Object**>(args[1]);
  CONVERT_SMI_ARG_CHECKED(length, 2);

  Handle<JSObject> result =
        isolate->factory()->NewArgumentsObject(callee, length);

  if (length > 0) {
    Handle<FixedArray> array =
        isolate->factory()->NewUninitializedFixedArray(length);
    DisallowHeapAllocation no_gc;
    WriteBarrierMode mode = array->GetWriteBarrierMode(no_gc);
    for (int i = 0; i < length; i++) {
      array->set(i, *--parameters, mode);
    }
    result->set_elements(*array);
  }
  return *result;
}


RUNTIME_FUNCTION(RuntimeHidden_NewClosureFromStubFailure) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(SharedFunctionInfo, shared, 0);
  Handle<Context> context(isolate->context());
  PretenureFlag pretenure_flag = NOT_TENURED;
  return *isolate->factory()->NewFunctionFromSharedFunctionInfo(
      shared,  context, pretenure_flag);
}


RUNTIME_FUNCTION(RuntimeHidden_NewClosure) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 3);
  CONVERT_ARG_HANDLE_CHECKED(Context, context, 0);
  CONVERT_ARG_HANDLE_CHECKED(SharedFunctionInfo, shared, 1);
  CONVERT_BOOLEAN_ARG_CHECKED(pretenure, 2);

  // The caller ensures that we pretenure closures that are assigned
  // directly to properties.
  PretenureFlag pretenure_flag = pretenure ? TENURED : NOT_TENURED;
  return *isolate->factory()->NewFunctionFromSharedFunctionInfo(
      shared, context, pretenure_flag);
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
    SlotRefValueBuilder slot_refs(
        frame,
        inlined_jsframe_index,
        inlined_function->shared()->formal_parameter_count());

    int args_count = slot_refs.args_length();

    *total_argc = prefix_argc + args_count;
    SmartArrayPointer<Handle<Object> > param_data(
        NewArray<Handle<Object> >(*total_argc));
    slot_refs.Prepare(isolate);
    for (int i = 0; i < args_count; i++) {
      Handle<Object> val = slot_refs.GetNext(isolate, 0);
      param_data[prefix_argc + i] = val;
    }
    slot_refs.Finish(isolate);

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


RUNTIME_FUNCTION(Runtime_FunctionBindArguments) {
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
    RUNTIME_ASSERT(*arguments[0] == args[2]);
    argc--;
  } else {
    RUNTIME_ASSERT(args[2]->IsUndefined());
  }
  // Initialize array of bindings (function, this, and any existing arguments
  // if the function was already bound).
  Handle<FixedArray> new_bindings;
  int i;
  if (bindee->IsJSFunction() && JSFunction::cast(*bindee)->shared()->bound()) {
    Handle<FixedArray> old_bindings(
        JSFunction::cast(*bindee)->function_bindings());
    RUNTIME_ASSERT(old_bindings->length() > JSFunction::kBoundFunctionIndex);
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
  Runtime::ForceSetObjectProperty(
      bound_function, length_string, new_length, attr).Assert();
  return *bound_function;
}


RUNTIME_FUNCTION(Runtime_BoundFunctionGetBindings) {
  HandleScope handles(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, callable, 0);
  if (callable->IsJSFunction()) {
    Handle<JSFunction> function = Handle<JSFunction>::cast(callable);
    if (function->shared()->bound()) {
      Handle<FixedArray> bindings(function->function_bindings());
      RUNTIME_ASSERT(bindings->map() == isolate->heap()->fixed_cow_array_map());
      return *isolate->factory()->NewJSArrayWithElements(bindings);
    }
  }
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_NewObjectFromBound) {
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
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, bound_function,
        Execution::TryGetConstructorDelegate(isolate, bound_function));
  }
  ASSERT(bound_function->IsJSFunction());

  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      Execution::New(Handle<JSFunction>::cast(bound_function),
                     total_argc, param_data.get()));
  return *result;
}


static Object* Runtime_NewObjectHelper(Isolate* isolate,
                                            Handle<Object> constructor,
                                            Handle<AllocationSite> site) {
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

  Debug* debug = isolate->debug();
  // Handle stepping into constructors if step into is active.
  if (debug->StepInActive()) {
    debug->HandleStepIn(function, Handle<Object>::null(), 0, true);
  }

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
  Compiler::EnsureCompiled(function, CLEAR_EXCEPTION);

  Handle<SharedFunctionInfo> shared(function->shared(), isolate);
  if (!function->has_initial_map() &&
      shared->IsInobjectSlackTrackingInProgress()) {
    // The tracking is already in progress for another function. We can only
    // track one initial_map at a time, so we force the completion before the
    // function is called as a constructor for the first time.
    shared->CompleteInobjectSlackTracking();
  }

  Handle<JSObject> result;
  if (site.is_null()) {
    result = isolate->factory()->NewJSObject(function);
  } else {
    result = isolate->factory()->NewJSObjectWithMemento(function, site);
  }

  isolate->counters()->constructed_objects()->Increment();
  isolate->counters()->constructed_objects_runtime()->Increment();

  return *result;
}


RUNTIME_FUNCTION(RuntimeHidden_NewObject) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, constructor, 0);
  return Runtime_NewObjectHelper(isolate,
                                 constructor,
                                 Handle<AllocationSite>::null());
}


RUNTIME_FUNCTION(RuntimeHidden_NewObjectWithAllocationSite) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(Object, constructor, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, feedback, 0);
  Handle<AllocationSite> site;
  if (feedback->IsAllocationSite()) {
    // The feedback can be an AllocationSite or undefined.
    site = Handle<AllocationSite>::cast(feedback);
  }
  return Runtime_NewObjectHelper(isolate, constructor, site);
}


RUNTIME_FUNCTION(RuntimeHidden_FinalizeInstanceSize) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);

  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);
  function->shared()->CompleteInobjectSlackTracking();

  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(RuntimeHidden_CompileUnoptimized) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);
#ifdef DEBUG
  if (FLAG_trace_lazy && !function->shared()->is_compiled()) {
    PrintF("[unoptimized: ");
    function->PrintName();
    PrintF("]\n");
  }
#endif

  // Compile the target function.
  ASSERT(function->shared()->allows_lazy_compilation());

  Handle<Code> code;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, code,
                                     Compiler::GetUnoptimizedCode(function));
  function->ReplaceCode(*code);

  // All done. Return the compiled code.
  ASSERT(function->is_compiled());
  ASSERT(function->code()->kind() == Code::FUNCTION ||
         (FLAG_always_opt &&
          function->code()->kind() == Code::OPTIMIZED_FUNCTION));
  return *code;
}


RUNTIME_FUNCTION(RuntimeHidden_CompileOptimized) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);
  CONVERT_BOOLEAN_ARG_CHECKED(concurrent, 1);

  Handle<Code> unoptimized(function->shared()->code());
  if (!function->shared()->is_compiled()) {
    // If the function is not compiled, do not optimize.
    // This can happen if the debugger is activated and
    // the function is returned to the not compiled state.
    // TODO(yangguo): reconsider this.
    function->ReplaceCode(function->shared()->code());
  } else if (!isolate->use_crankshaft() ||
             function->shared()->optimization_disabled() ||
             isolate->DebuggerHasBreakPoints()) {
    // If the function is not optimizable or debugger is active continue
    // using the code from the full compiler.
    if (FLAG_trace_opt) {
      PrintF("[failed to optimize ");
      function->PrintName();
      PrintF(": is code optimizable: %s, is debugger enabled: %s]\n",
          function->shared()->optimization_disabled() ? "F" : "T",
          isolate->DebuggerHasBreakPoints() ? "T" : "F");
    }
    function->ReplaceCode(*unoptimized);
  } else {
    Compiler::ConcurrencyMode mode = concurrent ? Compiler::CONCURRENT
                                                : Compiler::NOT_CONCURRENT;
    Handle<Code> code;
    if (Compiler::GetOptimizedCode(
            function, unoptimized, mode).ToHandle(&code)) {
      function->ReplaceCode(*code);
    } else {
      function->ReplaceCode(*unoptimized);
    }
  }

  ASSERT(function->code()->kind() == Code::FUNCTION ||
         function->code()->kind() == Code::OPTIMIZED_FUNCTION ||
         function->IsInOptimizationQueue());
  return function->code();
}


class ActivationsFinder : public ThreadVisitor {
 public:
  Code* code_;
  bool has_code_activations_;

  explicit ActivationsFinder(Code* code)
    : code_(code),
      has_code_activations_(false) { }

  void VisitThread(Isolate* isolate, ThreadLocalTop* top) {
    JavaScriptFrameIterator it(isolate, top);
    VisitFrames(&it);
  }

  void VisitFrames(JavaScriptFrameIterator* it) {
    for (; !it->done(); it->Advance()) {
      JavaScriptFrame* frame = it->frame();
      if (code_->contains(frame->pc())) has_code_activations_ = true;
    }
  }
};


RUNTIME_FUNCTION(RuntimeHidden_NotifyStubFailure) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 0);
  Deoptimizer* deoptimizer = Deoptimizer::Grab(isolate);
  ASSERT(AllowHeapAllocation::IsAllowed());
  delete deoptimizer;
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(RuntimeHidden_NotifyDeoptimized) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_SMI_ARG_CHECKED(type_arg, 0);
  Deoptimizer::BailoutType type =
      static_cast<Deoptimizer::BailoutType>(type_arg);
  Deoptimizer* deoptimizer = Deoptimizer::Grab(isolate);
  ASSERT(AllowHeapAllocation::IsAllowed());

  Handle<JSFunction> function = deoptimizer->function();
  Handle<Code> optimized_code = deoptimizer->compiled_code();

  ASSERT(optimized_code->kind() == Code::OPTIMIZED_FUNCTION);
  ASSERT(type == deoptimizer->bailout_type());

  // Make sure to materialize objects before causing any allocation.
  JavaScriptFrameIterator it(isolate);
  deoptimizer->MaterializeHeapObjects(&it);
  delete deoptimizer;

  JavaScriptFrame* frame = it.frame();
  RUNTIME_ASSERT(frame->function()->IsJSFunction());
  ASSERT(frame->function() == *function);

  // Avoid doing too much work when running with --always-opt and keep
  // the optimized code around.
  if (FLAG_always_opt || type == Deoptimizer::LAZY) {
    return isolate->heap()->undefined_value();
  }

  // Search for other activations of the same function and code.
  ActivationsFinder activations_finder(*optimized_code);
  activations_finder.VisitFrames(&it);
  isolate->thread_manager()->IterateArchivedThreads(&activations_finder);

  if (!activations_finder.has_code_activations_) {
    if (function->code() == *optimized_code) {
      if (FLAG_trace_deopt) {
        PrintF("[removing optimized code for: ");
        function->PrintName();
        PrintF("]\n");
      }
      function->ReplaceCode(function->shared()->code());
      // Evict optimized code for this function from the cache so that it
      // doesn't get used for new closures.
      function->shared()->EvictFromOptimizedCodeMap(*optimized_code,
                                                    "notify deoptimized");
    }
  } else {
    // TODO(titzer): we should probably do DeoptimizeCodeList(code)
    // unconditionally if the code is not already marked for deoptimization.
    // If there is an index by shared function info, all the better.
    Deoptimizer::DeoptimizeFunction(*function);
  }

  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_DeoptimizeFunction) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);
  if (!function->IsOptimized()) return isolate->heap()->undefined_value();

  Deoptimizer::DeoptimizeFunction(*function);

  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_ClearFunctionTypeFeedback) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);
  function->shared()->ClearTypeFeedbackInfo();
  Code* unoptimized = function->shared()->code();
  if (unoptimized->kind() == Code::FUNCTION) {
    unoptimized->ClearInlineCaches();
  }
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_RunningInSimulator) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 0);
#if defined(USE_SIMULATOR)
  return isolate->heap()->true_value();
#else
  return isolate->heap()->false_value();
#endif
}


RUNTIME_FUNCTION(Runtime_IsConcurrentRecompilationSupported) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 0);
  return isolate->heap()->ToBoolean(
      isolate->concurrent_recompilation_enabled());
}


RUNTIME_FUNCTION(Runtime_OptimizeFunctionOnNextCall) {
  HandleScope scope(isolate);
  RUNTIME_ASSERT(args.length() == 1 || args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);

  if (!function->IsOptimizable() &&
      !function->IsMarkedForConcurrentOptimization() &&
      !function->IsInOptimizationQueue()) {
    return isolate->heap()->undefined_value();
  }

  function->MarkForOptimization();

  Code* unoptimized = function->shared()->code();
  if (args.length() == 2 &&
      unoptimized->kind() == Code::FUNCTION) {
    CONVERT_ARG_HANDLE_CHECKED(String, type, 1);
    if (type->IsOneByteEqualTo(STATIC_ASCII_VECTOR("osr"))) {
      // Start patching from the currently patched loop nesting level.
      int current_level = unoptimized->allow_osr_at_loop_nesting_level();
      ASSERT(BackEdgeTable::Verify(isolate, unoptimized, current_level));
      for (int i = current_level + 1; i <= Code::kMaxLoopNestingMarker; i++) {
        unoptimized->set_allow_osr_at_loop_nesting_level(i);
        isolate->runtime_profiler()->AttemptOnStackReplacement(*function);
      }
    } else if (type->IsOneByteEqualTo(STATIC_ASCII_VECTOR("concurrent")) &&
               isolate->concurrent_recompilation_enabled()) {
      function->MarkForConcurrentOptimization();
    }
  }

  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_NeverOptimizeFunction) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_CHECKED(JSFunction, function, 0);
  function->shared()->set_optimization_disabled(true);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_GetOptimizationStatus) {
  HandleScope scope(isolate);
  RUNTIME_ASSERT(args.length() == 1 || args.length() == 2);
  if (!isolate->use_crankshaft()) {
    return Smi::FromInt(4);  // 4 == "never".
  }
  bool sync_with_compiler_thread = true;
  if (args.length() == 2) {
    CONVERT_ARG_HANDLE_CHECKED(String, sync, 1);
    if (sync->IsOneByteEqualTo(STATIC_ASCII_VECTOR("no sync"))) {
      sync_with_compiler_thread = false;
    }
  }
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);
  if (isolate->concurrent_recompilation_enabled() &&
      sync_with_compiler_thread) {
    while (function->IsInOptimizationQueue()) {
      isolate->optimizing_compiler_thread()->InstallOptimizedFunctions();
      OS::Sleep(50);
    }
  }
  if (FLAG_always_opt) {
    // We may have always opt, but that is more best-effort than a real
    // promise, so we still say "no" if it is not optimized.
    return function->IsOptimized() ? Smi::FromInt(3)   // 3 == "always".
                                   : Smi::FromInt(2);  // 2 == "no".
  }
  if (FLAG_deopt_every_n_times) {
    return Smi::FromInt(6);  // 6 == "maybe deopted".
  }
  return function->IsOptimized() ? Smi::FromInt(1)   // 1 == "yes".
                                 : Smi::FromInt(2);  // 2 == "no".
}


RUNTIME_FUNCTION(Runtime_UnblockConcurrentRecompilation) {
  ASSERT(args.length() == 0);
  RUNTIME_ASSERT(FLAG_block_concurrent_recompilation);
  RUNTIME_ASSERT(isolate->concurrent_recompilation_enabled());
  isolate->optimizing_compiler_thread()->Unblock();
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_GetOptimizationCount) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);
  return Smi::FromInt(function->shared()->opt_count());
}


static bool IsSuitableForOnStackReplacement(Isolate* isolate,
                                            Handle<JSFunction> function,
                                            Handle<Code> current_code) {
  // Keep track of whether we've succeeded in optimizing.
  if (!isolate->use_crankshaft() || !current_code->optimizable()) return false;
  // If we are trying to do OSR when there are already optimized
  // activations of the function, it means (a) the function is directly or
  // indirectly recursive and (b) an optimized invocation has been
  // deoptimized so that we are currently in an unoptimized activation.
  // Check for optimized activations of this function.
  for (JavaScriptFrameIterator it(isolate); !it.done(); it.Advance()) {
    JavaScriptFrame* frame = it.frame();
    if (frame->is_optimized() && frame->function() == *function) return false;
  }

  return true;
}


RUNTIME_FUNCTION(Runtime_CompileForOnStackReplacement) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);
  Handle<Code> caller_code(function->shared()->code());

  // We're not prepared to handle a function with arguments object.
  ASSERT(!function->shared()->uses_arguments());

  // Passing the PC in the javascript frame from the caller directly is
  // not GC safe, so we walk the stack to get it.
  JavaScriptFrameIterator it(isolate);
  JavaScriptFrame* frame = it.frame();
  if (!caller_code->contains(frame->pc())) {
    // Code on the stack may not be the code object referenced by the shared
    // function info.  It may have been replaced to include deoptimization data.
    caller_code = Handle<Code>(frame->LookupCode());
  }

  uint32_t pc_offset = static_cast<uint32_t>(
      frame->pc() - caller_code->instruction_start());

#ifdef DEBUG
  ASSERT_EQ(frame->function(), *function);
  ASSERT_EQ(frame->LookupCode(), *caller_code);
  ASSERT(caller_code->contains(frame->pc()));
#endif  // DEBUG


  BailoutId ast_id = caller_code->TranslatePcOffsetToAstId(pc_offset);
  ASSERT(!ast_id.IsNone());

  Compiler::ConcurrencyMode mode = isolate->concurrent_osr_enabled()
      ? Compiler::CONCURRENT : Compiler::NOT_CONCURRENT;
  Handle<Code> result = Handle<Code>::null();

  OptimizedCompileJob* job = NULL;
  if (mode == Compiler::CONCURRENT) {
    // Gate the OSR entry with a stack check.
    BackEdgeTable::AddStackCheck(caller_code, pc_offset);
    // Poll already queued compilation jobs.
    OptimizingCompilerThread* thread = isolate->optimizing_compiler_thread();
    if (thread->IsQueuedForOSR(function, ast_id)) {
      if (FLAG_trace_osr) {
        PrintF("[OSR - Still waiting for queued: ");
        function->PrintName();
        PrintF(" at AST id %d]\n", ast_id.ToInt());
      }
      return NULL;
    }

    job = thread->FindReadyOSRCandidate(function, ast_id);
  }

  if (job != NULL) {
    if (FLAG_trace_osr) {
      PrintF("[OSR - Found ready: ");
      function->PrintName();
      PrintF(" at AST id %d]\n", ast_id.ToInt());
    }
    result = Compiler::GetConcurrentlyOptimizedCode(job);
  } else if (IsSuitableForOnStackReplacement(isolate, function, caller_code)) {
    if (FLAG_trace_osr) {
      PrintF("[OSR - Compiling: ");
      function->PrintName();
      PrintF(" at AST id %d]\n", ast_id.ToInt());
    }
    MaybeHandle<Code> maybe_result = Compiler::GetOptimizedCode(
        function, caller_code, mode, ast_id);
    if (maybe_result.ToHandle(&result) &&
        result.is_identical_to(isolate->builtins()->InOptimizationQueue())) {
      // Optimization is queued.  Return to check later.
      return NULL;
    }
  }

  // Revert the patched back edge table, regardless of whether OSR succeeds.
  BackEdgeTable::Revert(isolate, *caller_code);

  // Check whether we ended up with usable optimized code.
  if (!result.is_null() && result->kind() == Code::OPTIMIZED_FUNCTION) {
    DeoptimizationInputData* data =
        DeoptimizationInputData::cast(result->deoptimization_data());

    if (data->OsrPcOffset()->value() >= 0) {
      ASSERT(BailoutId(data->OsrAstId()->value()) == ast_id);
      if (FLAG_trace_osr) {
        PrintF("[OSR - Entry at AST id %d, offset %d in optimized code]\n",
               ast_id.ToInt(), data->OsrPcOffset()->value());
      }
      // TODO(titzer): this is a massive hack to make the deopt counts
      // match. Fix heuristics for reenabling optimizations!
      function->shared()->increment_deopt_count();

      // TODO(titzer): Do not install code into the function.
      function->ReplaceCode(*result);
      return *result;
    }
  }

  // Failed.
  if (FLAG_trace_osr) {
    PrintF("[OSR - Failed: ");
    function->PrintName();
    PrintF(" at AST id %d]\n", ast_id.ToInt());
  }

  if (!function->IsOptimized()) {
    function->ReplaceCode(function->shared()->code());
  }
  return NULL;
}


RUNTIME_FUNCTION(Runtime_SetAllocationTimeout) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 2 || args.length() == 3);
#ifdef DEBUG
  CONVERT_SMI_ARG_CHECKED(interval, 0);
  CONVERT_SMI_ARG_CHECKED(timeout, 1);
  isolate->heap()->set_allocation_timeout(timeout);
  FLAG_gc_interval = interval;
  if (args.length() == 3) {
    // Enable/disable inline allocation if requested.
    CONVERT_BOOLEAN_ARG_CHECKED(inline_allocation, 2);
    if (inline_allocation) {
      isolate->heap()->EnableInlineAllocation();
    } else {
      isolate->heap()->DisableInlineAllocation();
    }
  }
#endif
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_CheckIsBootstrapping) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 0);
  RUNTIME_ASSERT(isolate->bootstrapper()->IsActive());
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_GetRootNaN) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 0);
  RUNTIME_ASSERT(isolate->bootstrapper()->IsActive());
  return isolate->heap()->nan_value();
}


RUNTIME_FUNCTION(Runtime_Call) {
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
     argv[i] = Handle<Object>(args[1 + i], isolate);
  }

  Handle<JSReceiver> hfun(fun);
  Handle<Object> hreceiver(receiver, isolate);
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      Execution::Call(isolate, hfun, hreceiver, argc, argv, true));
  return *result;
}


RUNTIME_FUNCTION(Runtime_Apply) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 5);
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, fun, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, receiver, 1);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, arguments, 2);
  CONVERT_SMI_ARG_CHECKED(offset, 3);
  CONVERT_SMI_ARG_CHECKED(argc, 4);
  RUNTIME_ASSERT(offset >= 0);
  // Loose upper bound to allow fuzzing. We'll most likely run out of
  // stack space before hitting this limit.
  static int kMaxArgc = 1000000;
  RUNTIME_ASSERT(argc >= 0 && argc <= kMaxArgc);

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
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, argv[i],
        Object::GetElement(isolate, arguments, offset + i));
  }

  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      Execution::Call(isolate, fun, receiver, argc, argv, true));
  return *result;
}


RUNTIME_FUNCTION(Runtime_GetFunctionDelegate) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, object, 0);
  RUNTIME_ASSERT(!object->IsJSFunction());
  return *Execution::GetFunctionDelegate(isolate, object);
}


RUNTIME_FUNCTION(Runtime_GetConstructorDelegate) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, object, 0);
  RUNTIME_ASSERT(!object->IsJSFunction());
  return *Execution::GetConstructorDelegate(isolate, object);
}


RUNTIME_FUNCTION(RuntimeHidden_NewGlobalContext) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);

  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);
  CONVERT_ARG_HANDLE_CHECKED(ScopeInfo, scope_info, 1);
  Handle<Context> result =
      isolate->factory()->NewGlobalContext(function, scope_info);

  ASSERT(function->context() == isolate->context());
  ASSERT(function->context()->global_object() == result->global_object());
  result->global_object()->set_global_context(*result);
  return *result;
}


RUNTIME_FUNCTION(RuntimeHidden_NewFunctionContext) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);

  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);
  int length = function->shared()->scope_info()->ContextLength();
  return *isolate->factory()->NewFunctionContext(length, function);
}


RUNTIME_FUNCTION(RuntimeHidden_PushWithContext) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  Handle<JSReceiver> extension_object;
  if (args[0]->IsJSReceiver()) {
    extension_object = args.at<JSReceiver>(0);
  } else {
    // Try to convert the object to a proper JavaScript object.
    MaybeHandle<JSReceiver> maybe_object =
        Object::ToObject(isolate, args.at<Object>(0));
    if (!maybe_object.ToHandle(&extension_object)) {
      Handle<Object> handle = args.at<Object>(0);
      Handle<Object> result =
          isolate->factory()->NewTypeError("with_expression",
                                           HandleVector(&handle, 1));
      return isolate->Throw(*result);
    }
  }

  Handle<JSFunction> function;
  if (args[1]->IsSmi()) {
    // A smi sentinel indicates a context nested inside global code rather
    // than some function.  There is a canonical empty function that can be
    // gotten from the native context.
    function = handle(isolate->context()->native_context()->closure());
  } else {
    function = args.at<JSFunction>(1);
  }

  Handle<Context> current(isolate->context());
  Handle<Context> context = isolate->factory()->NewWithContext(
      function, current, extension_object);
  isolate->set_context(*context);
  return *context;
}


RUNTIME_FUNCTION(RuntimeHidden_PushCatchContext) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 3);
  CONVERT_ARG_HANDLE_CHECKED(String, name, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, thrown_object, 1);
  Handle<JSFunction> function;
  if (args[2]->IsSmi()) {
    // A smi sentinel indicates a context nested inside global code rather
    // than some function.  There is a canonical empty function that can be
    // gotten from the native context.
    function = handle(isolate->context()->native_context()->closure());
  } else {
    function = args.at<JSFunction>(2);
  }
  Handle<Context> current(isolate->context());
  Handle<Context> context = isolate->factory()->NewCatchContext(
      function, current, name, thrown_object);
  isolate->set_context(*context);
  return *context;
}


RUNTIME_FUNCTION(RuntimeHidden_PushBlockContext) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(ScopeInfo, scope_info, 0);
  Handle<JSFunction> function;
  if (args[1]->IsSmi()) {
    // A smi sentinel indicates a context nested inside global code rather
    // than some function.  There is a canonical empty function that can be
    // gotten from the native context.
    function = handle(isolate->context()->native_context()->closure());
  } else {
    function = args.at<JSFunction>(1);
  }
  Handle<Context> current(isolate->context());
  Handle<Context> context = isolate->factory()->NewBlockContext(
      function, current, scope_info);
  isolate->set_context(*context);
  return *context;
}


RUNTIME_FUNCTION(Runtime_IsJSModule) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_CHECKED(Object, obj, 0);
  return isolate->heap()->ToBoolean(obj->IsJSModule());
}


RUNTIME_FUNCTION(RuntimeHidden_PushModuleContext) {
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


RUNTIME_FUNCTION(RuntimeHidden_DeclareModules) {
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
        case CONST_LEGACY: {
          PropertyAttributes attr =
              IsImmutableVariableMode(mode) ? FROZEN : SEALED;
          Handle<AccessorInfo> info =
              Accessors::MakeModuleExport(name, index, attr);
          Handle<Object> result =
              JSObject::SetAccessor(module, info).ToHandleChecked();
          ASSERT(!result->IsUndefined());
          USE(result);
          break;
        }
        case MODULE: {
          Object* referenced_context = Context::cast(host_context)->get(index);
          Handle<JSModule> value(Context::cast(referenced_context)->module());
          JSReceiver::SetProperty(module, name, value, FROZEN, STRICT).Assert();
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

    JSObject::PreventExtensions(module).Assert();
  }

  ASSERT(!isolate->has_pending_exception());
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(RuntimeHidden_DeleteContextSlot) {
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
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      JSReceiver::DeleteProperty(object, name));
  return *result;
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
#if defined(V8_TARGET_LITTLE_ENDIAN)
  return reinterpret_cast<uint32_t>(x) |
      (reinterpret_cast<ObjectPair>(y) << 32);
#elif defined(V8_TARGET_BIG_ENDIAN)
    return reinterpret_cast<uint32_t>(y) |
        (reinterpret_cast<ObjectPair>(x) << 32);
#else
#error Unknown endianness
#endif
}
#endif


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
  // context extension object - introduced via eval.
  return isolate->heap()->undefined_value();
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
  if (isolate->has_pending_exception()) {
    return MakePair(isolate->heap()->exception(), NULL);
  }

  // If the index is non-negative, the slot has been found in a context.
  if (index >= 0) {
    ASSERT(holder->IsContext());
    // If the "property" we were looking for is a local variable, the
    // receiver is the global object; see ECMA-262, 3rd., 10.1.6 and 10.2.3.
    Handle<Object> receiver = isolate->factory()->undefined_value();
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
        if (value->IsTheHole()) {
          ASSERT((attributes & READ_ONLY) != 0);
          value = isolate->heap()->undefined_value();
        }
        return MakePair(value, *receiver);
      case MISSING_BINDING:
        UNREACHABLE();
        return MakePair(NULL, NULL);
    }
  }

  // Otherwise, if the slot was found the holder is a context extension
  // object, subject of a with, or a global object.  We read the named
  // property from it.
  if (!holder.is_null()) {
    Handle<JSReceiver> object = Handle<JSReceiver>::cast(holder);
    ASSERT(object->IsJSProxy() || JSReceiver::HasProperty(object, name));
    // GetProperty below can cause GC.
    Handle<Object> receiver_handle(
        object->IsGlobalObject()
            ? Object::cast(isolate->heap()->undefined_value())
            : object->IsJSProxy() ? static_cast<Object*>(*object)
                : ComputeReceiverForNonGlobal(isolate, JSObject::cast(*object)),
        isolate);

    // No need to unhole the value here.  This is taken care of by the
    // GetProperty function.
    Handle<Object> value;
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(
        isolate, value,
        Object::GetProperty(object, name),
        MakePair(isolate->heap()->exception(), NULL));
    return MakePair(*value, *receiver_handle);
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


RUNTIME_FUNCTION_RETURN_PAIR(RuntimeHidden_LoadContextSlot) {
  return LoadContextSlotHelper(args, isolate, true);
}


RUNTIME_FUNCTION_RETURN_PAIR(RuntimeHidden_LoadContextSlotNoReferenceError) {
  return LoadContextSlotHelper(args, isolate, false);
}


RUNTIME_FUNCTION(RuntimeHidden_StoreContextSlot) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 4);

  CONVERT_ARG_HANDLE_CHECKED(Object, value, 0);
  CONVERT_ARG_HANDLE_CHECKED(Context, context, 1);
  CONVERT_ARG_HANDLE_CHECKED(String, name, 2);
  CONVERT_STRICT_MODE_ARG_CHECKED(strict_mode, 3);

  int index;
  PropertyAttributes attributes;
  ContextLookupFlags flags = FOLLOW_CHAINS;
  BindingFlags binding_flags;
  Handle<Object> holder = context->Lookup(name,
                                          flags,
                                          &index,
                                          &attributes,
                                          &binding_flags);
  if (isolate->has_pending_exception()) return isolate->heap()->exception();

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
    } else if (strict_mode == STRICT) {
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
  Handle<JSReceiver> object;

  if (!holder.is_null()) {
    // The property exists on the holder.
    object = Handle<JSReceiver>::cast(holder);
  } else {
    // The property was not found.
    ASSERT(attributes == ABSENT);

    if (strict_mode == STRICT) {
      // Throw in strict mode (assignment to undefined variable).
      Handle<Object> error =
          isolate->factory()->NewReferenceError(
              "not_defined", HandleVector(&name, 1));
      return isolate->Throw(*error);
    }
    // In sloppy mode, the property is added to the global object.
    attributes = NONE;
    object = Handle<JSReceiver>(isolate->context()->global_object());
  }

  // Set the property if it's not read only or doesn't yet exist.
  if ((attributes & READ_ONLY) == 0 ||
      (JSReceiver::GetLocalPropertyAttribute(object, name) == ABSENT)) {
    RETURN_FAILURE_ON_EXCEPTION(
        isolate,
        JSReceiver::SetProperty(object, name, value, NONE, strict_mode));
  } else if (strict_mode == STRICT && (attributes & READ_ONLY) != 0) {
    // Setting read only property in strict mode.
    Handle<Object> error =
      isolate->factory()->NewTypeError(
          "strict_cannot_assign", HandleVector(&name, 1));
    return isolate->Throw(*error);
  }
  return *value;
}


RUNTIME_FUNCTION(RuntimeHidden_Throw) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);

  return isolate->Throw(args[0]);
}


RUNTIME_FUNCTION(RuntimeHidden_ReThrow) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);

  return isolate->ReThrow(args[0]);
}


RUNTIME_FUNCTION(RuntimeHidden_PromoteScheduledException) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 0);
  return isolate->PromoteScheduledException();
}


RUNTIME_FUNCTION(RuntimeHidden_ThrowReferenceError) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, name, 0);
  Handle<Object> reference_error =
    isolate->factory()->NewReferenceError("not_defined",
                                          HandleVector(&name, 1));
  return isolate->Throw(*reference_error);
}


RUNTIME_FUNCTION(RuntimeHidden_ThrowNotDateError) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 0);
  return isolate->Throw(*isolate->factory()->NewTypeError(
      "not_date_object", HandleVector<Object>(NULL, 0)));
}


RUNTIME_FUNCTION(RuntimeHidden_ThrowMessage) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_SMI_ARG_CHECKED(message_id, 0);
  const char* message = GetBailoutReason(
      static_cast<BailoutReason>(message_id));
  Handle<String> message_handle =
      isolate->factory()->NewStringFromAsciiChecked(message);
  return isolate->Throw(*message_handle);
}


RUNTIME_FUNCTION(RuntimeHidden_StackGuard) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 0);

  // First check if this is a real stack overflow.
  if (isolate->stack_guard()->IsStackOverflow()) {
    return isolate->StackOverflow();
  }

  return Execution::HandleStackGuardInterrupt(isolate);
}


RUNTIME_FUNCTION(RuntimeHidden_TryInstallOptimizedCode) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);

  // First check if this is a real stack overflow.
  if (isolate->stack_guard()->IsStackOverflow()) {
    SealHandleScope shs(isolate);
    return isolate->StackOverflow();
  }

  isolate->optimizing_compiler_thread()->InstallOptimizedFunctions();
  return (function->IsOptimized()) ? function->code()
                                   : function->shared()->code();
}


RUNTIME_FUNCTION(RuntimeHidden_Interrupt) {
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


RUNTIME_FUNCTION(Runtime_TraceEnter) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 0);
  PrintTransition(isolate, NULL);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_TraceExit) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_CHECKED(Object, obj, 0);
  PrintTransition(isolate, obj);
  return obj;  // return TOS
}


RUNTIME_FUNCTION(Runtime_DebugPrint) {
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


RUNTIME_FUNCTION(Runtime_DebugTrace) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 0);
  isolate->PrintStack(stdout);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_DateCurrentTime) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 0);

  // According to ECMA-262, section 15.9.1, page 117, the precision of
  // the number in a Date object representing a particular instant in
  // time is milliseconds. Therefore, we floor the result of getting
  // the OS time.
  double millis = std::floor(OS::TimeCurrentMillis());
  return *isolate->factory()->NewNumber(millis);
}


RUNTIME_FUNCTION(Runtime_DateParseString) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(String, str, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSArray, output, 1);

  RUNTIME_ASSERT(output->HasFastElements());
  JSObject::EnsureCanContainHeapObjectElements(output);
  RUNTIME_ASSERT(output->HasFastObjectElements());
  Handle<FixedArray> output_array(FixedArray::cast(output->elements()));
  RUNTIME_ASSERT(output_array->length() >= DateParser::OUTPUT_SIZE);

  str = String::Flatten(str);
  DisallowHeapAllocation no_gc;

  bool result;
  String::FlatContent str_content = str->GetFlatContent();
  if (str_content.IsAscii()) {
    result = DateParser::Parse(str_content.ToOneByteVector(),
                               *output_array,
                               isolate->unicode_cache());
  } else {
    ASSERT(str_content.IsTwoByte());
    result = DateParser::Parse(str_content.ToUC16Vector(),
                               *output_array,
                               isolate->unicode_cache());
  }

  if (result) {
    return *output;
  } else {
    return isolate->heap()->null_value();
  }
}


RUNTIME_FUNCTION(Runtime_DateLocalTimezone) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);

  CONVERT_DOUBLE_ARG_CHECKED(x, 0);
  const char* zone =
      isolate->date_cache()->LocalTimezone(static_cast<int64_t>(x));
  Handle<String> result = isolate->factory()->NewStringFromUtf8(
      CStrVector(zone)).ToHandleChecked();
  return *result;
}


RUNTIME_FUNCTION(Runtime_DateToUTC) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);

  CONVERT_DOUBLE_ARG_CHECKED(x, 0);
  int64_t time = isolate->date_cache()->ToUTC(static_cast<int64_t>(x));

  return *isolate->factory()->NewNumber(static_cast<double>(time));
}


RUNTIME_FUNCTION(Runtime_DateCacheVersion) {
  HandleScope hs(isolate);
  ASSERT(args.length() == 0);
  if (!isolate->eternal_handles()->Exists(EternalHandles::DATE_CACHE_VERSION)) {
    Handle<FixedArray> date_cache_version =
        isolate->factory()->NewFixedArray(1, TENURED);
    date_cache_version->set(0, Smi::FromInt(0));
    isolate->eternal_handles()->CreateSingleton(
        isolate, *date_cache_version, EternalHandles::DATE_CACHE_VERSION);
  }
  Handle<FixedArray> date_cache_version =
      Handle<FixedArray>::cast(isolate->eternal_handles()->GetSingleton(
          EternalHandles::DATE_CACHE_VERSION));
  // Return result as a JS array.
  Handle<JSObject> result =
      isolate->factory()->NewJSObject(isolate->array_function());
  JSArray::SetContent(Handle<JSArray>::cast(result), date_cache_version);
  return *result;
}


RUNTIME_FUNCTION(Runtime_GlobalReceiver) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_CHECKED(Object, global, 0);
  if (!global->IsJSGlobalObject()) return isolate->heap()->null_value();
  return JSGlobalObject::cast(global)->global_receiver();
}


RUNTIME_FUNCTION(Runtime_IsAttachedGlobal) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_CHECKED(Object, global, 0);
  if (!global->IsJSGlobalObject()) return isolate->heap()->false_value();
  return isolate->heap()->ToBoolean(
      !JSGlobalObject::cast(global)->IsDetached());
}


RUNTIME_FUNCTION(Runtime_ParseJson) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(String, source, 0);

  source = String::Flatten(source);
  // Optimized fast case where we only have ASCII characters.
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      source->IsSeqOneByteString() ? JsonParser<true>::Parse(source)
                                   : JsonParser<false>::Parse(source));
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


RUNTIME_FUNCTION(Runtime_CompileString) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
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
  Handle<JSFunction> fun;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, fun,
      Compiler::GetFunctionFromEval(
          source, context, SLOPPY, restriction, RelocInfo::kNoPosition));
  return *fun;
}


static ObjectPair CompileGlobalEval(Isolate* isolate,
                                    Handle<String> source,
                                    Handle<Object> receiver,
                                    StrictMode strict_mode,
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
    return MakePair(isolate->heap()->exception(), NULL);
  }

  // Deal with a normal eval call with a string argument. Compile it
  // and return the compiled function bound in the local context.
  static const ParseRestriction restriction = NO_PARSE_RESTRICTION;
  Handle<JSFunction> compiled;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate, compiled,
      Compiler::GetFunctionFromEval(
          source, context, strict_mode, restriction, scope_position),
      MakePair(isolate->heap()->exception(), NULL));
  return MakePair(*compiled, *receiver);
}


RUNTIME_FUNCTION_RETURN_PAIR(RuntimeHidden_ResolvePossiblyDirectEval) {
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
    return MakePair(*callee, isolate->heap()->undefined_value());
  }

  ASSERT(args[3]->IsSmi());
  ASSERT(args.smi_at(3) == SLOPPY || args.smi_at(3) == STRICT);
  StrictMode strict_mode = static_cast<StrictMode>(args.smi_at(3));
  ASSERT(args[4]->IsSmi());
  return CompileGlobalEval(isolate,
                           args.at<String>(1),
                           args.at<Object>(2),
                           strict_mode,
                           args.smi_at(4));
}


RUNTIME_FUNCTION(RuntimeHidden_AllocateInNewSpace) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_SMI_ARG_CHECKED(size, 0);
  RUNTIME_ASSERT(IsAligned(size, kPointerSize));
  RUNTIME_ASSERT(size > 0);
  RUNTIME_ASSERT(size <= Page::kMaxRegularHeapObjectSize);
  return *isolate->factory()->NewFillerObject(size, false, NEW_SPACE);
}


RUNTIME_FUNCTION(RuntimeHidden_AllocateInTargetSpace) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  CONVERT_SMI_ARG_CHECKED(size, 0);
  CONVERT_SMI_ARG_CHECKED(flags, 1);
  RUNTIME_ASSERT(IsAligned(size, kPointerSize));
  RUNTIME_ASSERT(size > 0);
  RUNTIME_ASSERT(size <= Page::kMaxRegularHeapObjectSize);
  bool double_align = AllocateDoubleAlignFlag::decode(flags);
  AllocationSpace space = AllocateTargetSpace::decode(flags);
  return *isolate->factory()->NewFillerObject(size, double_align, space);
}


// Push an object unto an array of objects if it is not already in the
// array.  Returns true if the element was pushed on the stack and
// false otherwise.
RUNTIME_FUNCTION(Runtime_PushIfAbsent) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSArray, array, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, element, 1);
  RUNTIME_ASSERT(array->HasFastSmiOrObjectElements());
  int length = Smi::cast(array->length())->value();
  FixedArray* elements = FixedArray::cast(array->elements());
  for (int i = 0; i < length; i++) {
    if (elements->get(i) == *element) return isolate->heap()->false_value();
  }

  // Strict not needed. Used for cycle detection in Array join implementation.
  RETURN_FAILURE_ON_EXCEPTION(
      isolate,
      JSObject::SetFastElement(array, length, element, SLOPPY, true));
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
        SeededNumberDictionary::AtNumberPut(dict, index, elm);
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
    Handle<Map> map = JSObject::GetElementsTransitionMap(
        array,
        fast_elements_ ? FAST_HOLEY_ELEMENTS : DICTIONARY_ELEMENTS);
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
        SeededNumberDictionary::New(isolate_, current_storage->length()));
    uint32_t current_length = static_cast<uint32_t>(current_storage->length());
    for (uint32_t i = 0; i < current_length; i++) {
      HandleScope loop_scope(isolate_);
      Handle<Object> element(current_storage->get(i), isolate_);
      if (!element->IsTheHole()) {
        Handle<SeededNumberDictionary> new_storage =
            SeededNumberDictionary::AtNumberPut(slow_storage, i, element);
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
    GlobalHandles::Destroy(Handle<Object>::cast(storage_).location());
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
    case SLOPPY_ARGUMENTS_ELEMENTS:
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype, size)                      \
    case EXTERNAL_##TYPE##_ELEMENTS:                                         \
    case TYPE##_ELEMENTS:                                                    \

    TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE
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
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype, size)                        \
        case EXTERNAL_##TYPE##_ELEMENTS: {                                     \
          dense_elements_length =                                              \
              External##Type##Array::cast(object->elements())->length();       \
          break;                                                               \
        }

        TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE

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
        } else if (JSReceiver::HasElement(receiver, j)) {
          // Call GetElement on receiver, not its prototype, or getters won't
          // have the correct receiver.
          ASSIGN_RETURN_ON_EXCEPTION_VALUE(
              isolate, element_value,
              Object::GetElement(isolate, receiver, j),
              false);
          visitor->visit(j, element_value);
        }
      }
      break;
    }
    case FAST_HOLEY_DOUBLE_ELEMENTS:
    case FAST_DOUBLE_ELEMENTS: {
      // Empty array is FixedArray but not FixedDoubleArray.
      if (length == 0) break;
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
        } else if (JSReceiver::HasElement(receiver, j)) {
          // Call GetElement on receiver, not its prototype, or getters won't
          // have the correct receiver.
          Handle<Object> element_value;
          ASSIGN_RETURN_ON_EXCEPTION_VALUE(
              isolate, element_value,
              Object::GetElement(isolate, receiver, j),
              false);
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
        Handle<Object> element;
        ASSIGN_RETURN_ON_EXCEPTION_VALUE(
            isolate, element,
            Object::GetElement(isolate, receiver, index),
            false);
        visitor->visit(index, element);
        // Skip to next different index (i.e., omit duplicates).
        do {
          j++;
        } while (j < n && indices[j] == index);
      }
      break;
    }
    case EXTERNAL_UINT8_CLAMPED_ELEMENTS: {
      Handle<ExternalUint8ClampedArray> pixels(ExternalUint8ClampedArray::cast(
          receiver->elements()));
      for (uint32_t j = 0; j < length; j++) {
        Handle<Smi> e(Smi::FromInt(pixels->get_scalar(j)), isolate);
        visitor->visit(j, e);
      }
      break;
    }
    case EXTERNAL_INT8_ELEMENTS: {
      IterateExternalArrayElements<ExternalInt8Array, int8_t>(
          isolate, receiver, true, true, visitor);
      break;
    }
    case EXTERNAL_UINT8_ELEMENTS: {
      IterateExternalArrayElements<ExternalUint8Array, uint8_t>(
          isolate, receiver, true, true, visitor);
      break;
    }
    case EXTERNAL_INT16_ELEMENTS: {
      IterateExternalArrayElements<ExternalInt16Array, int16_t>(
          isolate, receiver, true, true, visitor);
      break;
    }
    case EXTERNAL_UINT16_ELEMENTS: {
      IterateExternalArrayElements<ExternalUint16Array, uint16_t>(
          isolate, receiver, true, true, visitor);
      break;
    }
    case EXTERNAL_INT32_ELEMENTS: {
      IterateExternalArrayElements<ExternalInt32Array, int32_t>(
          isolate, receiver, true, false, visitor);
      break;
    }
    case EXTERNAL_UINT32_ELEMENTS: {
      IterateExternalArrayElements<ExternalUint32Array, uint32_t>(
          isolate, receiver, true, false, visitor);
      break;
    }
    case EXTERNAL_FLOAT32_ELEMENTS: {
      IterateExternalArrayElements<ExternalFloat32Array, float>(
          isolate, receiver, false, false, visitor);
      break;
    }
    case EXTERNAL_FLOAT64_ELEMENTS: {
      IterateExternalArrayElements<ExternalFloat64Array, double>(
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
RUNTIME_FUNCTION(Runtime_ArrayConcat) {
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

  if (fast_case && kind == FAST_DOUBLE_ELEMENTS) {
    Handle<FixedArrayBase> storage =
        isolate->factory()->NewFixedDoubleArray(estimate_result_length);
    int j = 0;
    if (estimate_result_length > 0) {
      Handle<FixedDoubleArray> double_storage =
          Handle<FixedDoubleArray>::cast(storage);
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
              // Empty array is FixedArray but not FixedDoubleArray.
              if (length == 0) break;
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
    }
    Handle<JSArray> array = isolate->factory()->NewJSArray(0);
    Smi* length = Smi::FromInt(j);
    Handle<Map> map;
    map = JSObject::GetElementsTransitionMap(array, kind);
    array->set_map(*map);
    array->set_length(length);
    array->set_elements(*storage);
    return *array;
  }

  Handle<FixedArray> storage;
  if (fast_case) {
    // The backing storage array must have non-existing elements to preserve
    // holes across concat operations.
    storage = isolate->factory()->NewFixedArrayWithHoles(
        estimate_result_length);
  } else {
    // TODO(126): move 25% pre-allocation logic into Dictionary::Allocate
    uint32_t at_least_space_for = estimate_nof_elements +
                                  (estimate_nof_elements >> 2);
    storage = Handle<FixedArray>::cast(
        SeededNumberDictionary::New(isolate, at_least_space_for));
  }

  ArrayConcatVisitor visitor(isolate, storage, fast_case);

  for (int i = 0; i < argument_count; i++) {
    Handle<Object> obj(elements->get(i), isolate);
    if (obj->IsJSArray()) {
      Handle<JSArray> array = Handle<JSArray>::cast(obj);
      if (!IterateElements(isolate, array, &visitor)) {
        return isolate->heap()->exception();
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
RUNTIME_FUNCTION(Runtime_GlobalPrint) {
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
// Returns -1 if hole removal is not supported by this method.
RUNTIME_FUNCTION(Runtime_RemoveArrayHoles) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 0);
  CONVERT_NUMBER_CHECKED(uint32_t, limit, Uint32, args[1]);
  return *JSObject::PrepareElementsForSort(object, limit);
}


// Move contents of argument 0 (an array) to argument 1 (an array)
RUNTIME_FUNCTION(Runtime_MoveArrayContents) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSArray, from, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSArray, to, 1);
  JSObject::ValidateElements(from);
  JSObject::ValidateElements(to);

  Handle<FixedArrayBase> new_elements(from->elements());
  ElementsKind from_kind = from->GetElementsKind();
  Handle<Map> new_map = JSObject::GetElementsTransitionMap(to, from_kind);
  JSObject::SetMapAndElements(to, new_map, new_elements);
  to->set_length(from->length());

  JSObject::ResetElements(from);
  from->set_length(Smi::FromInt(0));

  JSObject::ValidateElements(to);
  return *to;
}


// How many elements does this object/array have?
RUNTIME_FUNCTION(Runtime_EstimateNumberOfElements) {
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
RUNTIME_FUNCTION(Runtime_GetArrayKeys) {
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
      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
          isolate, keys, FixedArray::UnionOfKeys(keys, current_keys));
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


RUNTIME_FUNCTION(Runtime_LookupAccessor) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 3);
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, receiver, 0);
  CONVERT_ARG_HANDLE_CHECKED(Name, name, 1);
  CONVERT_SMI_ARG_CHECKED(flag, 2);
  AccessorComponent component = flag == 0 ? ACCESSOR_GETTER : ACCESSOR_SETTER;
  if (!receiver->IsJSObject()) return isolate->heap()->undefined_value();
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      JSObject::GetAccessor(Handle<JSObject>::cast(receiver), name, component));
  return *result;
}


RUNTIME_FUNCTION(Runtime_DebugBreak) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 0);
  return Execution::DebugBreakHelper(isolate);
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
RUNTIME_FUNCTION(Runtime_SetDebugEventListener) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 2);
  RUNTIME_ASSERT(args[0]->IsJSFunction() ||
                 args[0]->IsUndefined() ||
                 args[0]->IsNull());
  CONVERT_ARG_HANDLE_CHECKED(Object, callback, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, data, 1);
  isolate->debugger()->SetEventListener(callback, data);

  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_Break) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 0);
  isolate->stack_guard()->DebugBreak();
  return isolate->heap()->undefined_value();
}


static Handle<Object> DebugLookupResultValue(Isolate* isolate,
                                             Handle<Object> receiver,
                                             Handle<Name> name,
                                             LookupResult* result,
                                             bool* has_caught = NULL) {
  Handle<Object> value = isolate->factory()->undefined_value();
  if  (!result->IsFound()) return value;
  switch (result->type()) {
    case NORMAL:
      value = JSObject::GetNormalizedProperty(
          handle(result->holder(), isolate), result);
      break;
    case FIELD:
      value = JSObject::FastPropertyAt(handle(result->holder(), isolate),
                                       result->representation(),
                                       result->GetFieldIndex().field_index());
      break;
    case CONSTANT:
      return handle(result->GetConstant(), isolate);
    case CALLBACKS: {
      Handle<Object> structure(result->GetCallbackObject(), isolate);
      ASSERT(!structure->IsForeign());
      if (structure->IsAccessorInfo()) {
        MaybeHandle<Object> obj = JSObject::GetPropertyWithCallback(
            handle(result->holder(), isolate), receiver, structure, name);
        if (!obj.ToHandle(&value)) {
          value = handle(isolate->pending_exception(), isolate);
          isolate->clear_pending_exception();
          if (has_caught != NULL) *has_caught = true;
          return value;
        }
      }
      break;
    }
    case INTERCEPTOR:
      break;
    case HANDLER:
    case NONEXISTENT:
      UNREACHABLE();
      break;
  }
  ASSERT(!value->IsTheHole() || result->IsReadOnly());
  return value->IsTheHole()
      ? Handle<Object>::cast(isolate->factory()->undefined_value()) : value;
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
RUNTIME_FUNCTION(Runtime_DebugGetPropertyDetails) {
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
    Handle<Object> element_or_char;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, element_or_char,
        Runtime::GetElementOrCharAt(isolate, obj, index));
    details->set(0, *element_or_char);
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
    jsproto->LocalLookup(name, &result);
    if (result.IsFound()) {
      // LookupResult is not GC safe as it holds raw object pointers.
      // GC can happen later in this code so put the required fields into
      // local variables using handles when required for later use.
      Handle<Object> result_callback_obj;
      if (result.IsPropertyCallbacks()) {
        result_callback_obj = Handle<Object>(result.GetCallbackObject(),
                                             isolate);
      }


      bool has_caught = false;
      Handle<Object> value = DebugLookupResultValue(
          isolate, obj, name, &result, &has_caught);

      // If the callback object is a fixed array then it contains JavaScript
      // getter and/or setter.
      bool has_js_accessors = result.IsPropertyCallbacks() &&
                              result_callback_obj->IsAccessorPair();
      Handle<FixedArray> details =
          isolate->factory()->NewFixedArray(has_js_accessors ? 5 : 2);
      details->set(0, *value);
      details->set(1, result.GetPropertyDetails().AsSmi());
      if (has_js_accessors) {
        AccessorPair* accessors = AccessorPair::cast(*result_callback_obj);
        details->set(2, isolate->heap()->ToBoolean(has_caught));
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


RUNTIME_FUNCTION(Runtime_DebugGetProperty) {
  HandleScope scope(isolate);

  ASSERT(args.length() == 2);

  CONVERT_ARG_HANDLE_CHECKED(JSObject, obj, 0);
  CONVERT_ARG_HANDLE_CHECKED(Name, name, 1);

  LookupResult result(isolate);
  obj->Lookup(name, &result);
  return *DebugLookupResultValue(isolate, obj, name, &result);
}


// Return the property type calculated from the property details.
// args[0]: smi with property details.
RUNTIME_FUNCTION(Runtime_DebugPropertyTypeFromDetails) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  CONVERT_PROPERTY_DETAILS_CHECKED(details, 0);
  return Smi::FromInt(static_cast<int>(details.type()));
}


// Return the property attribute calculated from the property details.
// args[0]: smi with property details.
RUNTIME_FUNCTION(Runtime_DebugPropertyAttributesFromDetails) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  CONVERT_PROPERTY_DETAILS_CHECKED(details, 0);
  return Smi::FromInt(static_cast<int>(details.attributes()));
}


// Return the property insertion index calculated from the property details.
// args[0]: smi with property details.
RUNTIME_FUNCTION(Runtime_DebugPropertyIndexFromDetails) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  CONVERT_PROPERTY_DETAILS_CHECKED(details, 0);
  // TODO(verwaest): Depends on the type of details.
  return Smi::FromInt(details.dictionary_index());
}


// Return property value from named interceptor.
// args[0]: object
// args[1]: property name
RUNTIME_FUNCTION(Runtime_DebugNamedInterceptorPropertyValue) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, obj, 0);
  RUNTIME_ASSERT(obj->HasNamedInterceptor());
  CONVERT_ARG_HANDLE_CHECKED(Name, name, 1);

  PropertyAttributes attributes;
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      JSObject::GetPropertyWithInterceptor(obj, obj, name, &attributes));
  return *result;
}


// Return element value from indexed interceptor.
// args[0]: object
// args[1]: index
RUNTIME_FUNCTION(Runtime_DebugIndexedInterceptorElementValue) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, obj, 0);
  RUNTIME_ASSERT(obj->HasIndexedInterceptor());
  CONVERT_NUMBER_CHECKED(uint32_t, index, Uint32, args[1]);
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result, JSObject::GetElementWithInterceptor(obj, obj, index));
  return *result;
}


static bool CheckExecutionState(Isolate* isolate, int break_id) {
  return (isolate->debug()->break_id() != 0 &&
          break_id == isolate->debug()->break_id());
}


RUNTIME_FUNCTION(Runtime_CheckExecutionState) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  CONVERT_NUMBER_CHECKED(int, break_id, Int32, args[0]);
  RUNTIME_ASSERT(CheckExecutionState(isolate, break_id));
  return isolate->heap()->true_value();
}


RUNTIME_FUNCTION(Runtime_GetFrameCount) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_NUMBER_CHECKED(int, break_id, Int32, args[0]);
  RUNTIME_ASSERT(CheckExecutionState(isolate, break_id));

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
RUNTIME_FUNCTION(Runtime_GetFrameDetails) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  CONVERT_NUMBER_CHECKED(int, break_id, Int32, args[0]);
  RUNTIME_ASSERT(CheckExecutionState(isolate, break_id));

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
  int local_count = scope_info->LocalCount();
  for (int slot = 0; slot < scope_info->LocalCount(); ++slot) {
    // Hide compiler-introduced temporary variables, whether on the stack or on
    // the context.
    if (scope_info->LocalIsSynthetic(slot))
      local_count--;
  }

  Handle<FixedArray> locals =
      isolate->factory()->NewFixedArray(local_count * 2);

  // Fill in the values of the locals.
  int local = 0;
  int i = 0;
  for (; i < scope_info->StackLocalCount(); ++i) {
    // Use the value from the stack.
    if (scope_info->LocalIsSynthetic(i))
      continue;
    locals->set(local * 2, scope_info->LocalName(i));
    locals->set(local * 2 + 1, frame_inspector.GetExpression(i));
    local++;
  }
  if (local < local_count) {
    // Get the context containing declarations.
    Handle<Context> context(
        Context::cast(it.frame()->context())->declaration_context());
    for (; i < scope_info->LocalCount(); ++i) {
      if (scope_info->LocalIsSynthetic(i))
        continue;
      Handle<String> name(scope_info->LocalName(i));
      VariableMode mode;
      InitializationFlag init_flag;
      locals->set(local * 2, *name);
      int context_slot_index =
          ScopeInfo::ContextSlotIndex(scope_info, name, &mode, &init_flag);
      Object* value = context->get(context_slot_index);
      locals->set(local * 2 + 1, value);
      local++;
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
                     2 * (argument_count + local_count) +
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
               Smi::FromInt(local_count));

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
  for (int i = 0; i < local_count * 2; i++) {
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
      shared->strict_mode() == SLOPPY &&
      !function->IsBuiltin()) {
    // If the receiver is not a JSObject and the function is not a
    // builtin or strict-mode we have hit an optimization where a
    // value object is not converted into a wrapped JS objects. To
    // hide this optimization from the debugger, we wrap the receiver
    // by creating correct wrapper object based on the calling frame's
    // native context.
    it.Advance();
    if (receiver->IsUndefined()) {
      Context* context = function->context();
      receiver = handle(context->global_object()->global_receiver());
    } else {
      ASSERT(!receiver->IsNull());
      Context* context = Context::cast(it.frame()->context());
      Handle<Context> native_context(Context::cast(context->native_context()));
      receiver = Object::ToObject(
          isolate, receiver, native_context).ToHandleChecked();
    }
  }
  details->set(kFrameDetailsReceiverIndex, *receiver);

  ASSERT_EQ(details_size, details_index);
  return *isolate->factory()->NewJSArrayWithElements(details);
}


static bool ParameterIsShadowedByContextLocal(Handle<ScopeInfo> info,
                                              Handle<String> parameter_name) {
  VariableMode mode;
  InitializationFlag flag;
  return ScopeInfo::ContextSlotIndex(info, parameter_name, &mode, &flag) != -1;
}


// Create a plain JSObject which materializes the local scope for the specified
// frame.
MUST_USE_RESULT
static MaybeHandle<JSObject> MaterializeStackLocalsWithFrameInspector(
    Isolate* isolate,
    Handle<JSObject> target,
    Handle<JSFunction> function,
    FrameInspector* frame_inspector) {
  Handle<SharedFunctionInfo> shared(function->shared());
  Handle<ScopeInfo> scope_info(shared->scope_info());

  // First fill all parameters.
  for (int i = 0; i < scope_info->ParameterCount(); ++i) {
    // Do not materialize the parameter if it is shadowed by a context local.
    Handle<String> name(scope_info->ParameterName(i));
    if (ParameterIsShadowedByContextLocal(scope_info, name)) continue;

    HandleScope scope(isolate);
    Handle<Object> value(i < frame_inspector->GetParametersCount()
                             ? frame_inspector->GetParameter(i)
                             : isolate->heap()->undefined_value(),
                         isolate);
    ASSERT(!value->IsTheHole());

    RETURN_ON_EXCEPTION(
        isolate,
        Runtime::SetObjectProperty(isolate, target, name, value, NONE, SLOPPY),
        JSObject);
  }

  // Second fill all stack locals.
  for (int i = 0; i < scope_info->StackLocalCount(); ++i) {
    if (scope_info->LocalIsSynthetic(i)) continue;
    Handle<String> name(scope_info->StackLocalName(i));
    Handle<Object> value(frame_inspector->GetExpression(i), isolate);
    if (value->IsTheHole()) continue;

    RETURN_ON_EXCEPTION(
        isolate,
        Runtime::SetObjectProperty(isolate, target, name, value, NONE, SLOPPY),
        JSObject);
  }

  return target;
}


static void UpdateStackLocalsFromMaterializedObject(Isolate* isolate,
                                                    Handle<JSObject> target,
                                                    Handle<JSFunction> function,
                                                    JavaScriptFrame* frame,
                                                    int inlined_jsframe_index) {
  if (inlined_jsframe_index != 0 || frame->is_optimized()) {
    // Optimized frames are not supported.
    // TODO(yangguo): make sure all code deoptimized when debugger is active
    //                and assert that this cannot happen.
    return;
  }

  Handle<SharedFunctionInfo> shared(function->shared());
  Handle<ScopeInfo> scope_info(shared->scope_info());

  // Parameters.
  for (int i = 0; i < scope_info->ParameterCount(); ++i) {
    // Shadowed parameters were not materialized.
    Handle<String> name(scope_info->ParameterName(i));
    if (ParameterIsShadowedByContextLocal(scope_info, name)) continue;

    ASSERT(!frame->GetParameter(i)->IsTheHole());
    HandleScope scope(isolate);
    Handle<Object> value =
        Object::GetPropertyOrElement(target, name).ToHandleChecked();
    frame->SetParameterValue(i, *value);
  }

  // Stack locals.
  for (int i = 0; i < scope_info->StackLocalCount(); ++i) {
    if (scope_info->LocalIsSynthetic(i)) continue;
    if (frame->GetExpression(i)->IsTheHole()) continue;
    HandleScope scope(isolate);
    Handle<Object> value = Object::GetPropertyOrElement(
        target,
        handle(scope_info->StackLocalName(i), isolate)).ToHandleChecked();
    frame->SetExpression(i, *value);
  }
}


MUST_USE_RESULT static MaybeHandle<JSObject> MaterializeLocalContext(
    Isolate* isolate,
    Handle<JSObject> target,
    Handle<JSFunction> function,
    JavaScriptFrame* frame) {
  HandleScope scope(isolate);
  Handle<SharedFunctionInfo> shared(function->shared());
  Handle<ScopeInfo> scope_info(shared->scope_info());

  if (!scope_info->HasContext()) return target;

  // Third fill all context locals.
  Handle<Context> frame_context(Context::cast(frame->context()));
  Handle<Context> function_context(frame_context->declaration_context());
  if (!ScopeInfo::CopyContextLocalsToScopeObject(
          scope_info, function_context, target)) {
    return MaybeHandle<JSObject>();
  }

  // Finally copy any properties from the function context extension.
  // These will be variables introduced by eval.
  if (function_context->closure() == *function) {
    if (function_context->has_extension() &&
        !function_context->IsNativeContext()) {
      Handle<JSObject> ext(JSObject::cast(function_context->extension()));
      Handle<FixedArray> keys;
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, keys,
          JSReceiver::GetKeys(ext, JSReceiver::INCLUDE_PROTOS),
          JSObject);

      for (int i = 0; i < keys->length(); i++) {
        // Names of variables introduced by eval are strings.
        ASSERT(keys->get(i)->IsString());
        Handle<String> key(String::cast(keys->get(i)));
        Handle<Object> value;
        ASSIGN_RETURN_ON_EXCEPTION(
            isolate, value, Object::GetPropertyOrElement(ext, key), JSObject);
        RETURN_ON_EXCEPTION(
            isolate,
            Runtime::SetObjectProperty(
                isolate, target, key, value, NONE, SLOPPY),
            JSObject);
      }
    }
  }

  return target;
}


MUST_USE_RESULT static MaybeHandle<JSObject> MaterializeLocalScope(
    Isolate* isolate,
    JavaScriptFrame* frame,
    int inlined_jsframe_index) {
  FrameInspector frame_inspector(frame, inlined_jsframe_index, isolate);
  Handle<JSFunction> function(JSFunction::cast(frame_inspector.GetFunction()));

  Handle<JSObject> local_scope =
      isolate->factory()->NewJSObject(isolate->object_function());
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, local_scope,
      MaterializeStackLocalsWithFrameInspector(
          isolate, local_scope, function, &frame_inspector),
      JSObject);

  return MaterializeLocalContext(isolate, local_scope, function, frame);
}


// Set the context local variable value.
static bool SetContextLocalValue(Isolate* isolate,
                                 Handle<ScopeInfo> scope_info,
                                 Handle<Context> context,
                                 Handle<String> variable_name,
                                 Handle<Object> new_value) {
  for (int i = 0; i < scope_info->ContextLocalCount(); i++) {
    Handle<String> next_name(scope_info->ContextLocalName(i));
    if (String::Equals(variable_name, next_name)) {
      VariableMode mode;
      InitializationFlag init_flag;
      int context_index =
          ScopeInfo::ContextSlotIndex(scope_info, next_name, &mode, &init_flag);
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

  Handle<JSFunction> function(frame->function());
  Handle<SharedFunctionInfo> shared(function->shared());
  Handle<ScopeInfo> scope_info(shared->scope_info());

  bool default_result = false;

  // Parameters.
  for (int i = 0; i < scope_info->ParameterCount(); ++i) {
    HandleScope scope(isolate);
    if (String::Equals(handle(scope_info->ParameterName(i)), variable_name)) {
      frame->SetParameterValue(i, *new_value);
      // Argument might be shadowed in heap context, don't stop here.
      default_result = true;
    }
  }

  // Stack locals.
  for (int i = 0; i < scope_info->StackLocalCount(); ++i) {
    HandleScope scope(isolate);
    if (String::Equals(handle(scope_info->StackLocalName(i)), variable_name)) {
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

        if (JSReceiver::HasProperty(ext, variable_name)) {
          // We don't expect this to do anything except replacing
          // property value.
          Runtime::SetObjectProperty(isolate, ext, variable_name, new_value,
                                     NONE, SLOPPY).Assert();
          return true;
        }
      }
    }
  }

  return default_result;
}


// Create a plain JSObject which materializes the closure content for the
// context.
MUST_USE_RESULT static MaybeHandle<JSObject> MaterializeClosure(
    Isolate* isolate,
    Handle<Context> context) {
  ASSERT(context->IsFunctionContext());

  Handle<SharedFunctionInfo> shared(context->closure()->shared());
  Handle<ScopeInfo> scope_info(shared->scope_info());

  // Allocate and initialize a JSObject with all the content of this function
  // closure.
  Handle<JSObject> closure_scope =
      isolate->factory()->NewJSObject(isolate->object_function());

  // Fill all context locals to the context extension.
  if (!ScopeInfo::CopyContextLocalsToScopeObject(
          scope_info, context, closure_scope)) {
    return MaybeHandle<JSObject>();
  }

  // Finally copy any properties from the function context extension. This will
  // be variables introduced by eval.
  if (context->has_extension()) {
    Handle<JSObject> ext(JSObject::cast(context->extension()));
    Handle<FixedArray> keys;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, keys,
        JSReceiver::GetKeys(ext, JSReceiver::INCLUDE_PROTOS), JSObject);

    for (int i = 0; i < keys->length(); i++) {
      HandleScope scope(isolate);
      // Names of variables introduced by eval are strings.
      ASSERT(keys->get(i)->IsString());
      Handle<String> key(String::cast(keys->get(i)));
      Handle<Object> value;
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, value, Object::GetPropertyOrElement(ext, key), JSObject);
      RETURN_ON_EXCEPTION(
          isolate,
          Runtime::SetObjectProperty(
              isolate, closure_scope, key, value, NONE, SLOPPY),
          JSObject);
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
    if (JSReceiver::HasProperty(ext, variable_name)) {
      // We don't expect this to do anything except replacing property value.
      Runtime::SetObjectProperty(isolate, ext, variable_name, new_value,
                                 NONE, SLOPPY).Assert();
      return true;
    }
  }

  return false;
}


// Create a plain JSObject which materializes the scope for the specified
// catch context.
MUST_USE_RESULT static MaybeHandle<JSObject> MaterializeCatchScope(
    Isolate* isolate,
    Handle<Context> context) {
  ASSERT(context->IsCatchContext());
  Handle<String> name(String::cast(context->extension()));
  Handle<Object> thrown_object(context->get(Context::THROWN_OBJECT_INDEX),
                               isolate);
  Handle<JSObject> catch_scope =
      isolate->factory()->NewJSObject(isolate->object_function());
  RETURN_ON_EXCEPTION(
      isolate,
      Runtime::SetObjectProperty(isolate, catch_scope, name, thrown_object,
                                 NONE, SLOPPY),
      JSObject);
  return catch_scope;
}


static bool SetCatchVariableValue(Isolate* isolate,
                                  Handle<Context> context,
                                  Handle<String> variable_name,
                                  Handle<Object> new_value) {
  ASSERT(context->IsCatchContext());
  Handle<String> name(String::cast(context->extension()));
  if (!String::Equals(name, variable_name)) {
    return false;
  }
  context->set(Context::THROWN_OBJECT_INDEX, *new_value);
  return true;
}


// Create a plain JSObject which materializes the block scope for the specified
// block context.
MUST_USE_RESULT static MaybeHandle<JSObject> MaterializeBlockScope(
    Isolate* isolate,
    Handle<Context> context) {
  ASSERT(context->IsBlockContext());
  Handle<ScopeInfo> scope_info(ScopeInfo::cast(context->extension()));

  // Allocate and initialize a JSObject with all the arguments, stack locals
  // heap locals and extension properties of the debugged function.
  Handle<JSObject> block_scope =
      isolate->factory()->NewJSObject(isolate->object_function());

  // Fill all context locals.
  if (!ScopeInfo::CopyContextLocalsToScopeObject(
          scope_info, context, block_scope)) {
    return MaybeHandle<JSObject>();
  }

  return block_scope;
}


// Create a plain JSObject which materializes the module scope for the specified
// module context.
MUST_USE_RESULT static MaybeHandle<JSObject> MaterializeModuleScope(
    Isolate* isolate,
    Handle<Context> context) {
  ASSERT(context->IsModuleContext());
  Handle<ScopeInfo> scope_info(ScopeInfo::cast(context->extension()));

  // Allocate and initialize a JSObject with all the members of the debugged
  // module.
  Handle<JSObject> module_scope =
      isolate->factory()->NewJSObject(isolate->object_function());

  // Fill all context locals.
  if (!ScopeInfo::CopyContextLocalsToScopeObject(
          scope_info, context, module_scope)) {
    return MaybeHandle<JSObject>();
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
                int inlined_jsframe_index,
                bool ignore_nested_scopes = false)
    : isolate_(isolate),
      frame_(frame),
      inlined_jsframe_index_(inlined_jsframe_index),
      function_(frame->function()),
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

    // Currently it takes too much time to find nested scopes due to script
    // parsing. Sometimes we want to run the ScopeIterator as fast as possible
    // (for example, while collecting async call stacks on every
    // addEventListener call), even if we drop some nested scopes.
    // Later we may optimize getting the nested scopes (cache the result?)
    // and include nested scopes into the "fast" iteration case as well.
    if (!ignore_nested_scopes) {
      Handle<DebugInfo> debug_info = Debug::GetDebugInfo(shared_info);

      // Find the break point where execution has stopped.
      BreakLocationIterator break_location_iterator(debug_info,
                                                    ALL_BREAK_LOCATIONS);
      // pc points to the instruction after the current one, possibly a break
      // location as well. So the "- 1" to exclude it from the search.
      break_location_iterator.FindBreakLocationFromAddress(frame->pc() - 1);

      // Within the return sequence at the moment it is not possible to
      // get a source position which is consistent with the current scope chain.
      // Thus all nested with, catch and block contexts are skipped and we only
      // provide the function scope.
      ignore_nested_scopes = break_location_iterator.IsExit();
    }

    if (ignore_nested_scopes) {
      if (scope_info->HasContext()) {
        context_ = Handle<Context>(context_->declaration_context(), isolate_);
      } else {
        while (context_->closure() == *function_) {
          context_ = Handle<Context>(context_->previous(), isolate_);
        }
      }
      if (scope_info->scope_type() == FUNCTION_SCOPE) {
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
  MaybeHandle<JSObject> ScopeObject() {
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


RUNTIME_FUNCTION(Runtime_GetScopeCount) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  CONVERT_NUMBER_CHECKED(int, break_id, Int32, args[0]);
  RUNTIME_ASSERT(CheckExecutionState(isolate, break_id));

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
RUNTIME_FUNCTION(Runtime_GetStepInPositions) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  CONVERT_NUMBER_CHECKED(int, break_id, Int32, args[0]);
  RUNTIME_ASSERT(CheckExecutionState(isolate, break_id));

  CONVERT_SMI_ARG_CHECKED(wrapped_id, 1);

  // Get the frame where the debugging is performed.
  StackFrame::Id id = UnwrapFrameId(wrapped_id);
  JavaScriptFrameIterator frame_it(isolate, id);
  RUNTIME_ASSERT(!frame_it.done());

  JavaScriptFrame* frame = frame_it.frame();

  Handle<JSFunction> fun =
      Handle<JSFunction>(frame->function());
  Handle<SharedFunctionInfo> shared =
      Handle<SharedFunctionInfo>(fun->shared());

  if (!isolate->debug()->EnsureDebugInfo(shared, fun)) {
    return isolate->heap()->undefined_value();
  }

  Handle<DebugInfo> debug_info = Debug::GetDebugInfo(shared);

  int len = 0;
  Handle<JSArray> array(isolate->factory()->NewJSArray(10));
  // Find the break point where execution has stopped.
  BreakLocationIterator break_location_iterator(debug_info,
                                                ALL_BREAK_LOCATIONS);

  break_location_iterator.FindBreakLocationFromAddress(frame->pc() - 1);
  int current_statement_pos = break_location_iterator.statement_position();

  while (!break_location_iterator.Done()) {
    bool accept;
    if (break_location_iterator.pc() > frame->pc()) {
      accept = true;
    } else {
      StackFrame::Id break_frame_id = isolate->debug()->break_frame_id();
      // The break point is near our pc. Could be a step-in possibility,
      // that is currently taken by active debugger call.
      if (break_frame_id == StackFrame::NO_ID) {
        // We are not stepping.
        accept = false;
      } else {
        JavaScriptFrameIterator additional_frame_it(isolate, break_frame_id);
        // If our frame is a top frame and we are stepping, we can do step-in
        // at this place.
        accept = additional_frame_it.frame()->id() == id;
      }
    }
    if (accept) {
      if (break_location_iterator.IsStepInLocation(isolate)) {
        Smi* position_value = Smi::FromInt(break_location_iterator.position());
        RETURN_FAILURE_ON_EXCEPTION(
            isolate,
            JSObject::SetElement(array, len,
                                 Handle<Object>(position_value, isolate),
                                 NONE, SLOPPY));
        len++;
      }
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


MUST_USE_RESULT static MaybeHandle<JSObject> MaterializeScopeDetails(
    Isolate* isolate,
    ScopeIterator* it) {
  // Calculate the size of the result.
  int details_size = kScopeDetailsSize;
  Handle<FixedArray> details = isolate->factory()->NewFixedArray(details_size);

  // Fill in scope details.
  details->set(kScopeDetailsTypeIndex, Smi::FromInt(it->Type()));
  Handle<JSObject> scope_object;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, scope_object, it->ScopeObject(), JSObject);
  details->set(kScopeDetailsObjectIndex, *scope_object);

  return isolate->factory()->NewJSArrayWithElements(details);
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
RUNTIME_FUNCTION(Runtime_GetScopeDetails) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 4);
  CONVERT_NUMBER_CHECKED(int, break_id, Int32, args[0]);
  RUNTIME_ASSERT(CheckExecutionState(isolate, break_id));

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
  Handle<JSObject> details;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, details, MaterializeScopeDetails(isolate, &it));
  return *details;
}


// Return an array of scope details
// args[0]: number: break id
// args[1]: number: frame index
// args[2]: number: inlined frame index
// args[3]: boolean: ignore nested scopes
//
// The array returned contains arrays with the following information:
// 0: Scope type
// 1: Scope object
RUNTIME_FUNCTION(Runtime_GetAllScopesDetails) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 3 || args.length() == 4);
  CONVERT_NUMBER_CHECKED(int, break_id, Int32, args[0]);
  RUNTIME_ASSERT(CheckExecutionState(isolate, break_id));

  CONVERT_SMI_ARG_CHECKED(wrapped_id, 1);
  CONVERT_NUMBER_CHECKED(int, inlined_jsframe_index, Int32, args[2]);

  bool ignore_nested_scopes = false;
  if (args.length() == 4) {
    CONVERT_BOOLEAN_ARG_CHECKED(flag, 3);
    ignore_nested_scopes = flag;
  }

  // Get the frame where the debugging is performed.
  StackFrame::Id id = UnwrapFrameId(wrapped_id);
  JavaScriptFrameIterator frame_it(isolate, id);
  JavaScriptFrame* frame = frame_it.frame();

  List<Handle<JSObject> > result(4);
  ScopeIterator it(isolate, frame, inlined_jsframe_index, ignore_nested_scopes);
  for (; !it.Done(); it.Next()) {
    Handle<JSObject> details;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, details, MaterializeScopeDetails(isolate, &it));
    result.Add(details);
  }

  Handle<FixedArray> array = isolate->factory()->NewFixedArray(result.length());
  for (int i = 0; i < result.length(); ++i) {
    array->set(i, *result[i]);
  }
  return *isolate->factory()->NewJSArrayWithElements(array);
}


RUNTIME_FUNCTION(Runtime_GetFunctionScopeCount) {
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


RUNTIME_FUNCTION(Runtime_GetFunctionScopeDetails) {
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

  Handle<JSObject> details;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, details, MaterializeScopeDetails(isolate, &it));
  return *details;
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
RUNTIME_FUNCTION(Runtime_SetScopeVariableValue) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 6);

  // Check arguments.
  CONVERT_NUMBER_CHECKED(int, index, Int32, args[3]);
  CONVERT_ARG_HANDLE_CHECKED(String, variable_name, 4);
  CONVERT_ARG_HANDLE_CHECKED(Object, new_value, 5);

  bool res;
  if (args[0]->IsNumber()) {
    CONVERT_NUMBER_CHECKED(int, break_id, Int32, args[0]);
    RUNTIME_ASSERT(CheckExecutionState(isolate, break_id));

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


RUNTIME_FUNCTION(Runtime_DebugPrintScopes) {
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


RUNTIME_FUNCTION(Runtime_GetThreadCount) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_NUMBER_CHECKED(int, break_id, Int32, args[0]);
  RUNTIME_ASSERT(CheckExecutionState(isolate, break_id));

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
RUNTIME_FUNCTION(Runtime_GetThreadDetails) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  CONVERT_NUMBER_CHECKED(int, break_id, Int32, args[0]);
  RUNTIME_ASSERT(CheckExecutionState(isolate, break_id));

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
RUNTIME_FUNCTION(Runtime_SetDisableBreak) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_BOOLEAN_ARG_CHECKED(disable_break, 0);
  isolate->debug()->set_disable_break(disable_break);
  return  isolate->heap()->undefined_value();
}


static bool IsPositionAlignmentCodeCorrect(int alignment) {
  return alignment == STATEMENT_ALIGNED || alignment == BREAK_POSITION_ALIGNED;
}


RUNTIME_FUNCTION(Runtime_GetBreakLocations) {
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
RUNTIME_FUNCTION(Runtime_SetFunctionBreakPoint) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 3);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);
  CONVERT_NUMBER_CHECKED(int32_t, source_position, Int32, args[1]);
  RUNTIME_ASSERT(source_position >= 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, break_point_object_arg, 2);

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
RUNTIME_FUNCTION(Runtime_SetScriptBreakPoint) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 4);
  CONVERT_ARG_HANDLE_CHECKED(JSValue, wrapper, 0);
  CONVERT_NUMBER_CHECKED(int32_t, source_position, Int32, args[1]);
  RUNTIME_ASSERT(source_position >= 0);
  CONVERT_NUMBER_CHECKED(int32_t, statement_aligned_code, Int32, args[2]);
  CONVERT_ARG_HANDLE_CHECKED(Object, break_point_object_arg, 3);

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
    return isolate->heap()->undefined_value();
  }

  return Smi::FromInt(source_position);
}


// Clear a break point
// args[0]: number: break point object
RUNTIME_FUNCTION(Runtime_ClearBreakPoint) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, break_point_object_arg, 0);

  // Clear break point.
  isolate->debug()->ClearBreakPoint(break_point_object_arg);

  return isolate->heap()->undefined_value();
}


// Change the state of break on exceptions.
// args[0]: Enum value indicating whether to affect caught/uncaught exceptions.
// args[1]: Boolean indicating on/off.
RUNTIME_FUNCTION(Runtime_ChangeBreakOnException) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  CONVERT_NUMBER_CHECKED(uint32_t, type_arg, Uint32, args[0]);
  CONVERT_BOOLEAN_ARG_CHECKED(enable, 1);

  // If the number doesn't match an enum value, the ChangeBreakOnException
  // function will default to affecting caught exceptions.
  ExceptionBreakType type = static_cast<ExceptionBreakType>(type_arg);
  // Update break point state.
  isolate->debug()->ChangeBreakOnException(type, enable);
  return isolate->heap()->undefined_value();
}


// Returns the state of break on exceptions
// args[0]: boolean indicating uncaught exceptions
RUNTIME_FUNCTION(Runtime_IsBreakOnException) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_NUMBER_CHECKED(uint32_t, type_arg, Uint32, args[0]);

  ExceptionBreakType type = static_cast<ExceptionBreakType>(type_arg);
  bool result = isolate->debug()->IsBreakOnException(type);
  return Smi::FromInt(result);
}


// Prepare for stepping
// args[0]: break id for checking execution state
// args[1]: step action from the enumeration StepAction
// args[2]: number of times to perform the step, for step out it is the number
//          of frames to step down.
RUNTIME_FUNCTION(Runtime_PrepareStep) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 4);
  CONVERT_NUMBER_CHECKED(int, break_id, Int32, args[0]);
  RUNTIME_ASSERT(CheckExecutionState(isolate, break_id));

  if (!args[1]->IsNumber() || !args[2]->IsNumber()) {
    return isolate->Throw(isolate->heap()->illegal_argument_string());
  }

  CONVERT_NUMBER_CHECKED(int, wrapped_frame_id, Int32, args[3]);

  StackFrame::Id frame_id;
  if (wrapped_frame_id == 0) {
    frame_id = StackFrame::NO_ID;
  } else {
    frame_id = UnwrapFrameId(wrapped_frame_id);
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

  if (frame_id != StackFrame::NO_ID && step_action != StepNext &&
      step_action != StepMin && step_action != StepOut) {
    return isolate->ThrowIllegalOperation();
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
                                step_count,
                                frame_id);
  return isolate->heap()->undefined_value();
}


// Clear all stepping set by PrepareStep.
RUNTIME_FUNCTION(Runtime_ClearStepping) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 0);
  isolate->debug()->ClearStepping();
  return isolate->heap()->undefined_value();
}


// Helper function to find or create the arguments object for
// Runtime_DebugEvaluate.
MUST_USE_RESULT static MaybeHandle<JSObject> MaterializeArgumentsObject(
    Isolate* isolate,
    Handle<JSObject> target,
    Handle<JSFunction> function) {
  // Do not materialize the arguments object for eval or top-level code.
  // Skip if "arguments" is already taken.
  if (!function->shared()->is_function() ||
      JSReceiver::HasLocalProperty(target,
                                   isolate->factory()->arguments_string())) {
    return target;
  }

  // FunctionGetArguments can't throw an exception.
  Handle<JSObject> arguments = Handle<JSObject>::cast(
      Accessors::FunctionGetArguments(function));
  Handle<String> arguments_str = isolate->factory()->arguments_string();
  RETURN_ON_EXCEPTION(
      isolate,
      Runtime::SetObjectProperty(
          isolate, target, arguments_str, arguments, ::NONE, SLOPPY),
      JSObject);
  return target;
}


// Compile and evaluate source for the given context.
static MaybeHandle<Object> DebugEvaluate(Isolate* isolate,
                                         Handle<Context> context,
                                         Handle<Object> context_extension,
                                         Handle<Object> receiver,
                                         Handle<String> source) {
  if (context_extension->IsJSObject()) {
    Handle<JSObject> extension = Handle<JSObject>::cast(context_extension);
    Handle<JSFunction> closure(context->closure(), isolate);
    context = isolate->factory()->NewWithContext(closure, context, extension);
  }

  Handle<JSFunction> eval_fun;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, eval_fun,
      Compiler::GetFunctionFromEval(source,
                                    context,
                                    SLOPPY,
                                    NO_PARSE_RESTRICTION,
                                    RelocInfo::kNoPosition),
      Object);

  Handle<Object> result;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, result,
      Execution::Call(isolate, eval_fun, receiver, 0, NULL),
      Object);

  // Skip the global proxy as it has no properties and always delegates to the
  // real global object.
  if (result->IsJSGlobalProxy()) {
    result = Handle<JSObject>(JSObject::cast(result->GetPrototype(isolate)));
  }

  // Clear the oneshot breakpoints so that the debugger does not step further.
  isolate->debug()->ClearStepping();
  return result;
}


// Evaluate a piece of JavaScript in the context of a stack frame for
// debugging.  Things that need special attention are:
// - Parameters and stack-allocated locals need to be materialized.  Altered
//   values need to be written back to the stack afterwards.
// - The arguments object needs to materialized.
RUNTIME_FUNCTION(Runtime_DebugEvaluate) {
  HandleScope scope(isolate);

  // Check the execution state and decode arguments frame and source to be
  // evaluated.
  ASSERT(args.length() == 6);
  CONVERT_NUMBER_CHECKED(int, break_id, Int32, args[0]);
  RUNTIME_ASSERT(CheckExecutionState(isolate, break_id));

  CONVERT_SMI_ARG_CHECKED(wrapped_id, 1);
  CONVERT_NUMBER_CHECKED(int, inlined_jsframe_index, Int32, args[2]);
  CONVERT_ARG_HANDLE_CHECKED(String, source, 3);
  CONVERT_BOOLEAN_ARG_CHECKED(disable_break, 4);
  CONVERT_ARG_HANDLE_CHECKED(Object, context_extension, 5);

  // Handle the processing of break.
  DisableBreak disable_break_save(isolate, disable_break);

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

  // Evaluate on the context of the frame.
  Handle<Context> context(Context::cast(frame->context()));
  ASSERT(!context.is_null());

  // Materialize stack locals and the arguments object.
  Handle<JSObject> materialized =
      isolate->factory()->NewJSObject(isolate->object_function());

  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, materialized,
      MaterializeStackLocalsWithFrameInspector(
          isolate, materialized, function, &frame_inspector));

  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, materialized,
      MaterializeArgumentsObject(isolate, materialized, function));

  // Add the materialized object in a with-scope to shadow the stack locals.
  context = isolate->factory()->NewWithContext(function, context, materialized);

  Handle<Object> receiver(frame->receiver(), isolate);
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      DebugEvaluate(isolate, context, context_extension, receiver, source));

  // Write back potential changes to materialized stack locals to the stack.
  UpdateStackLocalsFromMaterializedObject(
      isolate, materialized, function, frame, inlined_jsframe_index);

  return *result;
}


RUNTIME_FUNCTION(Runtime_DebugEvaluateGlobal) {
  HandleScope scope(isolate);

  // Check the execution state and decode arguments frame and source to be
  // evaluated.
  ASSERT(args.length() == 4);
  CONVERT_NUMBER_CHECKED(int, break_id, Int32, args[0]);
  RUNTIME_ASSERT(CheckExecutionState(isolate, break_id));

  CONVERT_ARG_HANDLE_CHECKED(String, source, 1);
  CONVERT_BOOLEAN_ARG_CHECKED(disable_break, 2);
  CONVERT_ARG_HANDLE_CHECKED(Object, context_extension, 3);

  // Handle the processing of break.
  DisableBreak disable_break_save(isolate, disable_break);

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
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      DebugEvaluate(isolate, context, context_extension, receiver, source));
  return *result;
}


RUNTIME_FUNCTION(Runtime_DebugGetLoadedScripts) {
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
    Handle<JSObject> wrapper = Script::GetWrapper(script);
    instances->set(i, *wrapper);
  }

  // Return result as a JS array.
  Handle<JSObject> result =
      isolate->factory()->NewJSObject(isolate->array_function());
  JSArray::SetContent(Handle<JSArray>::cast(result), instances);
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
RUNTIME_FUNCTION(Runtime_DebugReferencedBy) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 3);

  // First perform a full GC in order to avoid references from dead objects.
  Heap* heap = isolate->heap();
  heap->CollectAllGarbage(Heap::kMakeHeapIterableMask, "%DebugReferencedBy");
  // The heap iterator reserves the right to do a GC to make the heap iterable.
  // Due to the GC above we know it won't need to do that, but it seems cleaner
  // to get the heap iterator constructed before we start having unprotected
  // Object* locals that are not protected by handles.

  // Check parameters.
  CONVERT_ARG_HANDLE_CHECKED(JSObject, target, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, instance_filter, 1);
  RUNTIME_ASSERT(instance_filter->IsUndefined() ||
                 instance_filter->IsJSObject());
  CONVERT_NUMBER_CHECKED(int32_t, max_references, Int32, args[2]);
  RUNTIME_ASSERT(max_references >= 0);


  // Get the constructor function for context extension and arguments array.
  Handle<JSObject> arguments_boilerplate(
      isolate->context()->native_context()->sloppy_arguments_boilerplate());
  Handle<JSFunction> arguments_function(
      JSFunction::cast(arguments_boilerplate->map()->constructor()));

  // Get the number of referencing objects.
  int count;
  HeapIterator heap_iterator(heap);
  count = DebugReferencedBy(&heap_iterator,
                            *target, *instance_filter, max_references,
                            NULL, 0, *arguments_function);

  // Allocate an array to hold the result.
  Handle<FixedArray> instances = isolate->factory()->NewFixedArray(count);

  // Fill the referencing objects.
  // AllocateFixedArray above does not make the heap non-iterable.
  ASSERT(heap->IsHeapIterable());
  HeapIterator heap_iterator2(heap);
  count = DebugReferencedBy(&heap_iterator2,
                            *target, *instance_filter, max_references,
                            *instances, count, *arguments_function);

  // Return result as JS array.
  Handle<JSFunction> constructor(
      isolate->context()->native_context()->array_function());

  Handle<JSObject> result = isolate->factory()->NewJSObject(constructor);
  JSArray::SetContent(Handle<JSArray>::cast(result), instances);
  return *result;
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
RUNTIME_FUNCTION(Runtime_DebugConstructedBy) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);

  // First perform a full GC in order to avoid dead objects.
  Heap* heap = isolate->heap();
  heap->CollectAllGarbage(Heap::kMakeHeapIterableMask, "%DebugConstructedBy");

  // Check parameters.
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, constructor, 0);
  CONVERT_NUMBER_CHECKED(int32_t, max_references, Int32, args[1]);
  RUNTIME_ASSERT(max_references >= 0);

  // Get the number of referencing objects.
  int count;
  HeapIterator heap_iterator(heap);
  count = DebugConstructedBy(&heap_iterator,
                             *constructor,
                             max_references,
                             NULL,
                             0);

  // Allocate an array to hold the result.
  Handle<FixedArray> instances = isolate->factory()->NewFixedArray(count);

  ASSERT(heap->IsHeapIterable());
  // Fill the referencing objects.
  HeapIterator heap_iterator2(heap);
  count = DebugConstructedBy(&heap_iterator2,
                             *constructor,
                             max_references,
                             *instances,
                             count);

  // Return result as JS array.
  Handle<JSFunction> array_function(
      isolate->context()->native_context()->array_function());
  Handle<JSObject> result = isolate->factory()->NewJSObject(array_function);
  JSArray::SetContent(Handle<JSArray>::cast(result), instances);
  return *result;
}


// Find the effective prototype object as returned by __proto__.
// args[0]: the object to find the prototype for.
RUNTIME_FUNCTION(Runtime_DebugGetPrototype) {
  HandleScope shs(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, obj, 0);
  return *GetPrototypeSkipHiddenPrototypes(isolate, obj);
}


// Patches script source (should be called upon BeforeCompile event).
RUNTIME_FUNCTION(Runtime_DebugSetScriptSource) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);

  CONVERT_ARG_HANDLE_CHECKED(JSValue, script_wrapper, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, source, 1);

  RUNTIME_ASSERT(script_wrapper->value()->IsScript());
  Handle<Script> script(Script::cast(script_wrapper->value()));

  int compilation_state = script->compilation_state();
  RUNTIME_ASSERT(compilation_state == Script::COMPILATION_STATE_INITIAL);
  script->set_source(*source);

  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_SystemBreak) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 0);
  OS::DebugBreak();
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_DebugDisassembleFunction) {
  HandleScope scope(isolate);
#ifdef DEBUG
  ASSERT(args.length() == 1);
  // Get the function and make sure it is compiled.
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, func, 0);
  if (!Compiler::EnsureCompiled(func, KEEP_EXCEPTION)) {
    return isolate->heap()->exception();
  }
  func->code()->PrintLn();
#endif  // DEBUG
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_DebugDisassembleConstructor) {
  HandleScope scope(isolate);
#ifdef DEBUG
  ASSERT(args.length() == 1);
  // Get the function and make sure it is compiled.
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, func, 0);
  if (!Compiler::EnsureCompiled(func, KEEP_EXCEPTION)) {
    return isolate->heap()->exception();
  }
  func->shared()->construct_stub()->PrintLn();
#endif  // DEBUG
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_FunctionGetInferredName) {
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
RUNTIME_FUNCTION(Runtime_LiveEditFindSharedFunctionInfosForScript) {
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
RUNTIME_FUNCTION(Runtime_LiveEditGatherCompileInfo) {
  HandleScope scope(isolate);
  CHECK(isolate->debugger()->live_edit_enabled());
  ASSERT(args.length() == 2);
  CONVERT_ARG_CHECKED(JSValue, script, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, source, 1);

  RUNTIME_ASSERT(script->value()->IsScript());
  Handle<Script> script_handle = Handle<Script>(Script::cast(script->value()));

  Handle<JSArray> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result, LiveEdit::GatherCompileInfo(script_handle, source));
  return *result;
}


// Changes the source of the script to a new_source.
// If old_script_name is provided (i.e. is a String), also creates a copy of
// the script with its original source and sends notification to debugger.
RUNTIME_FUNCTION(Runtime_LiveEditReplaceScript) {
  HandleScope scope(isolate);
  CHECK(isolate->debugger()->live_edit_enabled());
  ASSERT(args.length() == 3);
  CONVERT_ARG_CHECKED(JSValue, original_script_value, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, new_source, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, old_script_name, 2);

  RUNTIME_ASSERT(original_script_value->value()->IsScript());
  Handle<Script> original_script(Script::cast(original_script_value->value()));

  Handle<Object> old_script = LiveEdit::ChangeScriptSource(
      original_script,  new_source,  old_script_name);

  if (old_script->IsScript()) {
    Handle<Script> script_handle = Handle<Script>::cast(old_script);
    return *Script::GetWrapper(script_handle);
  } else {
    return isolate->heap()->null_value();
  }
}


RUNTIME_FUNCTION(Runtime_LiveEditFunctionSourceUpdated) {
  HandleScope scope(isolate);
  CHECK(isolate->debugger()->live_edit_enabled());
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSArray, shared_info, 0);
  RUNTIME_ASSERT(SharedInfoWrapper::IsInstance(shared_info));

  LiveEdit::FunctionSourceUpdated(shared_info);
  return isolate->heap()->undefined_value();
}


// Replaces code of SharedFunctionInfo with a new one.
RUNTIME_FUNCTION(Runtime_LiveEditReplaceFunctionCode) {
  HandleScope scope(isolate);
  CHECK(isolate->debugger()->live_edit_enabled());
  ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSArray, new_compile_info, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSArray, shared_info, 1);
  RUNTIME_ASSERT(SharedInfoWrapper::IsInstance(shared_info));

  LiveEdit::ReplaceFunctionCode(new_compile_info, shared_info);
  return isolate->heap()->undefined_value();
}


// Connects SharedFunctionInfo to another script.
RUNTIME_FUNCTION(Runtime_LiveEditFunctionSetScript) {
  HandleScope scope(isolate);
  CHECK(isolate->debugger()->live_edit_enabled());
  ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(Object, function_object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, script_object, 1);

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
RUNTIME_FUNCTION(Runtime_LiveEditReplaceRefToNestedFunction) {
  HandleScope scope(isolate);
  CHECK(isolate->debugger()->live_edit_enabled());
  ASSERT(args.length() == 3);

  CONVERT_ARG_HANDLE_CHECKED(JSValue, parent_wrapper, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSValue, orig_wrapper, 1);
  CONVERT_ARG_HANDLE_CHECKED(JSValue, subst_wrapper, 2);
  RUNTIME_ASSERT(parent_wrapper->value()->IsSharedFunctionInfo());
  RUNTIME_ASSERT(orig_wrapper->value()->IsSharedFunctionInfo());
  RUNTIME_ASSERT(subst_wrapper->value()->IsSharedFunctionInfo());

  LiveEdit::ReplaceRefToNestedFunction(
      parent_wrapper, orig_wrapper, subst_wrapper);
  return isolate->heap()->undefined_value();
}


// Updates positions of a shared function info (first parameter) according
// to script source change. Text change is described in second parameter as
// array of groups of 3 numbers:
// (change_begin, change_end, change_end_new_position).
// Each group describes a change in text; groups are sorted by change_begin.
RUNTIME_FUNCTION(Runtime_LiveEditPatchFunctionPositions) {
  HandleScope scope(isolate);
  CHECK(isolate->debugger()->live_edit_enabled());
  ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSArray, shared_array, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSArray, position_change_array, 1);
  RUNTIME_ASSERT(SharedInfoWrapper::IsInstance(shared_array))

  LiveEdit::PatchFunctionPositions(shared_array, position_change_array);
  return isolate->heap()->undefined_value();
}


// For array of SharedFunctionInfo's (each wrapped in JSValue)
// checks that none of them have activations on stacks (of any thread).
// Returns array of the same length with corresponding results of
// LiveEdit::FunctionPatchabilityStatus type.
RUNTIME_FUNCTION(Runtime_LiveEditCheckAndDropActivations) {
  HandleScope scope(isolate);
  CHECK(isolate->debugger()->live_edit_enabled());
  ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSArray, shared_array, 0);
  CONVERT_BOOLEAN_ARG_CHECKED(do_drop, 1);
  RUNTIME_ASSERT(shared_array->length()->IsSmi());
  int array_length = Smi::cast(shared_array->length())->value();
  for (int i = 0; i < array_length; i++) {
    Handle<Object> element =
        Object::GetElement(isolate, shared_array, i).ToHandleChecked();
    RUNTIME_ASSERT(
        element->IsJSValue() &&
        Handle<JSValue>::cast(element)->value()->IsSharedFunctionInfo());
  }

  return *LiveEdit::CheckAndDropActivations(shared_array, do_drop);
}


// Compares 2 strings line-by-line, then token-wise and returns diff in form
// of JSArray of triplets (pos1, pos1_end, pos2_end) describing list
// of diff chunks.
RUNTIME_FUNCTION(Runtime_LiveEditCompareStrings) {
  HandleScope scope(isolate);
  CHECK(isolate->debugger()->live_edit_enabled());
  ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(String, s1, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, s2, 1);

  return *LiveEdit::CompareStrings(s1, s2);
}


// Restarts a call frame and completely drops all frames above.
// Returns true if successful. Otherwise returns undefined or an error message.
RUNTIME_FUNCTION(Runtime_LiveEditRestartFrame) {
  HandleScope scope(isolate);
  CHECK(isolate->debugger()->live_edit_enabled());
  ASSERT(args.length() == 2);
  CONVERT_NUMBER_CHECKED(int, break_id, Int32, args[0]);
  RUNTIME_ASSERT(CheckExecutionState(isolate, break_id));

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
RUNTIME_FUNCTION(Runtime_GetFunctionCodePositionFromSource) {
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
RUNTIME_FUNCTION(Runtime_ExecuteInDebugContext) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, function, 0);
  CONVERT_BOOLEAN_ARG_CHECKED(without_debugger, 1);

  MaybeHandle<Object> maybe_result;
  if (without_debugger) {
    maybe_result = Execution::Call(isolate,
                                   function,
                                   isolate->global_object(),
                                   0,
                                   NULL);
  } else {
    EnterDebugger enter_debugger(isolate);
    maybe_result = Execution::Call(isolate,
                                   function,
                                   isolate->global_object(),
                                   0,
                                   NULL);
  }
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, result, maybe_result);
  return *result;
}


// Sets a v8 flag.
RUNTIME_FUNCTION(Runtime_SetFlags) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_CHECKED(String, arg, 0);
  SmartArrayPointer<char> flags =
      arg->ToCString(DISALLOW_NULLS, ROBUST_STRING_TRAVERSAL);
  FlagList::SetFlagsFromString(flags.get(), StrLength(flags.get()));
  return isolate->heap()->undefined_value();
}


// Performs a GC.
// Presently, it only does a full GC.
RUNTIME_FUNCTION(Runtime_CollectGarbage) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  isolate->heap()->CollectAllGarbage(Heap::kNoGCFlags, "%CollectGarbage");
  return isolate->heap()->undefined_value();
}


// Gets the current heap usage.
RUNTIME_FUNCTION(Runtime_GetHeapUsage) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 0);
  int usage = static_cast<int>(isolate->heap()->SizeOfObjects());
  if (!Smi::IsValid(usage)) {
    return *isolate->factory()->NewNumberFromInt(usage);
  }
  return Smi::FromInt(usage);
}


#ifdef V8_I18N_SUPPORT
RUNTIME_FUNCTION(Runtime_CanonicalizeLanguageTag) {
  HandleScope scope(isolate);
  Factory* factory = isolate->factory();

  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(String, locale_id_str, 0);

  v8::String::Utf8Value locale_id(v8::Utils::ToLocal(locale_id_str));

  // Return value which denotes invalid language tag.
  const char* const kInvalidTag = "invalid-tag";

  UErrorCode error = U_ZERO_ERROR;
  char icu_result[ULOC_FULLNAME_CAPACITY];
  int icu_length = 0;

  uloc_forLanguageTag(*locale_id, icu_result, ULOC_FULLNAME_CAPACITY,
                      &icu_length, &error);
  if (U_FAILURE(error) || icu_length == 0) {
    return *factory->NewStringFromAsciiChecked(kInvalidTag);
  }

  char result[ULOC_FULLNAME_CAPACITY];

  // Force strict BCP47 rules.
  uloc_toLanguageTag(icu_result, result, ULOC_FULLNAME_CAPACITY, TRUE, &error);

  if (U_FAILURE(error)) {
    return *factory->NewStringFromAsciiChecked(kInvalidTag);
  }

  return *factory->NewStringFromAsciiChecked(result);
}


RUNTIME_FUNCTION(Runtime_AvailableLocalesOf) {
  HandleScope scope(isolate);
  Factory* factory = isolate->factory();

  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(String, service, 0);

  const icu::Locale* available_locales = NULL;
  int32_t count = 0;

  if (service->IsUtf8EqualTo(CStrVector("collator"))) {
    available_locales = icu::Collator::getAvailableLocales(count);
  } else if (service->IsUtf8EqualTo(CStrVector("numberformat"))) {
    available_locales = icu::NumberFormat::getAvailableLocales(count);
  } else if (service->IsUtf8EqualTo(CStrVector("dateformat"))) {
    available_locales = icu::DateFormat::getAvailableLocales(count);
  } else if (service->IsUtf8EqualTo(CStrVector("breakiterator"))) {
    available_locales = icu::BreakIterator::getAvailableLocales(count);
  }

  UErrorCode error = U_ZERO_ERROR;
  char result[ULOC_FULLNAME_CAPACITY];
  Handle<JSObject> locales =
      factory->NewJSObject(isolate->object_function());

  for (int32_t i = 0; i < count; ++i) {
    const char* icu_name = available_locales[i].getName();

    error = U_ZERO_ERROR;
    // No need to force strict BCP47 rules.
    uloc_toLanguageTag(icu_name, result, ULOC_FULLNAME_CAPACITY, FALSE, &error);
    if (U_FAILURE(error)) {
      // This shouldn't happen, but lets not break the user.
      continue;
    }

    RETURN_FAILURE_ON_EXCEPTION(isolate,
        JSObject::SetLocalPropertyIgnoreAttributes(
            locales,
            factory->NewStringFromAsciiChecked(result),
            factory->NewNumber(i),
            NONE));
  }

  return *locales;
}


RUNTIME_FUNCTION(Runtime_GetDefaultICULocale) {
  HandleScope scope(isolate);
  Factory* factory = isolate->factory();

  ASSERT(args.length() == 0);

  icu::Locale default_locale;

  // Set the locale
  char result[ULOC_FULLNAME_CAPACITY];
  UErrorCode status = U_ZERO_ERROR;
  uloc_toLanguageTag(
      default_locale.getName(), result, ULOC_FULLNAME_CAPACITY, FALSE, &status);
  if (U_SUCCESS(status)) {
    return *factory->NewStringFromAsciiChecked(result);
  }

  return *factory->NewStringFromStaticAscii("und");
}


RUNTIME_FUNCTION(Runtime_GetLanguageTagVariants) {
  HandleScope scope(isolate);
  Factory* factory = isolate->factory();

  ASSERT(args.length() == 1);

  CONVERT_ARG_HANDLE_CHECKED(JSArray, input, 0);

  uint32_t length = static_cast<uint32_t>(input->length()->Number());
  // Set some limit to prevent fuzz tests from going OOM.
  // Can be bumped when callers' requirements change.
  RUNTIME_ASSERT(length < 100);
  Handle<FixedArray> output = factory->NewFixedArray(length);
  Handle<Name> maximized = factory->NewStringFromStaticAscii("maximized");
  Handle<Name> base = factory->NewStringFromStaticAscii("base");
  for (unsigned int i = 0; i < length; ++i) {
    Handle<Object> locale_id;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, locale_id, Object::GetElement(isolate, input, i));
    if (!locale_id->IsString()) {
      return isolate->Throw(*factory->illegal_argument_string());
    }

    v8::String::Utf8Value utf8_locale_id(
        v8::Utils::ToLocal(Handle<String>::cast(locale_id)));

    UErrorCode error = U_ZERO_ERROR;

    // Convert from BCP47 to ICU format.
    // de-DE-u-co-phonebk -> de_DE@collation=phonebook
    char icu_locale[ULOC_FULLNAME_CAPACITY];
    int icu_locale_length = 0;
    uloc_forLanguageTag(*utf8_locale_id, icu_locale, ULOC_FULLNAME_CAPACITY,
                        &icu_locale_length, &error);
    if (U_FAILURE(error) || icu_locale_length == 0) {
      return isolate->Throw(*factory->illegal_argument_string());
    }

    // Maximize the locale.
    // de_DE@collation=phonebook -> de_Latn_DE@collation=phonebook
    char icu_max_locale[ULOC_FULLNAME_CAPACITY];
    uloc_addLikelySubtags(
        icu_locale, icu_max_locale, ULOC_FULLNAME_CAPACITY, &error);

    // Remove extensions from maximized locale.
    // de_Latn_DE@collation=phonebook -> de_Latn_DE
    char icu_base_max_locale[ULOC_FULLNAME_CAPACITY];
    uloc_getBaseName(
        icu_max_locale, icu_base_max_locale, ULOC_FULLNAME_CAPACITY, &error);

    // Get original name without extensions.
    // de_DE@collation=phonebook -> de_DE
    char icu_base_locale[ULOC_FULLNAME_CAPACITY];
    uloc_getBaseName(
        icu_locale, icu_base_locale, ULOC_FULLNAME_CAPACITY, &error);

    // Convert from ICU locale format to BCP47 format.
    // de_Latn_DE -> de-Latn-DE
    char base_max_locale[ULOC_FULLNAME_CAPACITY];
    uloc_toLanguageTag(icu_base_max_locale, base_max_locale,
                       ULOC_FULLNAME_CAPACITY, FALSE, &error);

    // de_DE -> de-DE
    char base_locale[ULOC_FULLNAME_CAPACITY];
    uloc_toLanguageTag(
        icu_base_locale, base_locale, ULOC_FULLNAME_CAPACITY, FALSE, &error);

    if (U_FAILURE(error)) {
      return isolate->Throw(*factory->illegal_argument_string());
    }

    Handle<JSObject> result = factory->NewJSObject(isolate->object_function());
    RETURN_FAILURE_ON_EXCEPTION(isolate,
        JSObject::SetLocalPropertyIgnoreAttributes(
            result,
            maximized,
            factory->NewStringFromAsciiChecked(base_max_locale),
            NONE));
    RETURN_FAILURE_ON_EXCEPTION(isolate,
        JSObject::SetLocalPropertyIgnoreAttributes(
            result,
            base,
            factory->NewStringFromAsciiChecked(base_locale),
            NONE));
    output->set(i, *result);
  }

  Handle<JSArray> result = factory->NewJSArrayWithElements(output);
  result->set_length(Smi::FromInt(length));
  return *result;
}


RUNTIME_FUNCTION(Runtime_IsInitializedIntlObject) {
  HandleScope scope(isolate);

  ASSERT(args.length() == 1);

  CONVERT_ARG_HANDLE_CHECKED(Object, input, 0);

  if (!input->IsJSObject()) return isolate->heap()->false_value();
  Handle<JSObject> obj = Handle<JSObject>::cast(input);

  Handle<String> marker = isolate->factory()->intl_initialized_marker_string();
  Handle<Object> tag(obj->GetHiddenProperty(marker), isolate);
  return isolate->heap()->ToBoolean(!tag->IsTheHole());
}


RUNTIME_FUNCTION(Runtime_IsInitializedIntlObjectOfType) {
  HandleScope scope(isolate);

  ASSERT(args.length() == 2);

  CONVERT_ARG_HANDLE_CHECKED(Object, input, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, expected_type, 1);

  if (!input->IsJSObject()) return isolate->heap()->false_value();
  Handle<JSObject> obj = Handle<JSObject>::cast(input);

  Handle<String> marker = isolate->factory()->intl_initialized_marker_string();
  Handle<Object> tag(obj->GetHiddenProperty(marker), isolate);
  return isolate->heap()->ToBoolean(
      tag->IsString() && String::cast(*tag)->Equals(*expected_type));
}


RUNTIME_FUNCTION(Runtime_MarkAsInitializedIntlObjectOfType) {
  HandleScope scope(isolate);

  ASSERT(args.length() == 3);

  CONVERT_ARG_HANDLE_CHECKED(JSObject, input, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, type, 1);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, impl, 2);

  Handle<String> marker = isolate->factory()->intl_initialized_marker_string();
  JSObject::SetHiddenProperty(input, marker, type);

  marker = isolate->factory()->intl_impl_object_string();
  JSObject::SetHiddenProperty(input, marker, impl);

  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_GetImplFromInitializedIntlObject) {
  HandleScope scope(isolate);

  ASSERT(args.length() == 1);

  CONVERT_ARG_HANDLE_CHECKED(Object, input, 0);

  if (!input->IsJSObject()) {
    Vector< Handle<Object> > arguments = HandleVector(&input, 1);
    Handle<Object> type_error =
        isolate->factory()->NewTypeError("not_intl_object", arguments);
    return isolate->Throw(*type_error);
  }

  Handle<JSObject> obj = Handle<JSObject>::cast(input);

  Handle<String> marker = isolate->factory()->intl_impl_object_string();
  Handle<Object> impl(obj->GetHiddenProperty(marker), isolate);
  if (impl->IsTheHole()) {
    Vector< Handle<Object> > arguments = HandleVector(&obj, 1);
    Handle<Object> type_error =
        isolate->factory()->NewTypeError("not_intl_object", arguments);
    return isolate->Throw(*type_error);
  }
  return *impl;
}


RUNTIME_FUNCTION(Runtime_CreateDateTimeFormat) {
  HandleScope scope(isolate);

  ASSERT(args.length() == 3);

  CONVERT_ARG_HANDLE_CHECKED(String, locale, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, options, 1);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, resolved, 2);

  Handle<ObjectTemplateInfo> date_format_template =
      I18N::GetTemplate(isolate);

  // Create an empty object wrapper.
  Handle<JSObject> local_object;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, local_object,
      Execution::InstantiateObject(date_format_template));

  // Set date time formatter as internal field of the resulting JS object.
  icu::SimpleDateFormat* date_format = DateFormat::InitializeDateTimeFormat(
      isolate, locale, options, resolved);

  if (!date_format) return isolate->ThrowIllegalOperation();

  local_object->SetInternalField(0, reinterpret_cast<Smi*>(date_format));

  RETURN_FAILURE_ON_EXCEPTION(isolate,
      JSObject::SetLocalPropertyIgnoreAttributes(
          local_object,
          isolate->factory()->NewStringFromStaticAscii("dateFormat"),
          isolate->factory()->NewStringFromStaticAscii("valid"),
          NONE));

  // Make object handle weak so we can delete the data format once GC kicks in.
  Handle<Object> wrapper = isolate->global_handles()->Create(*local_object);
  GlobalHandles::MakeWeak(wrapper.location(),
                          reinterpret_cast<void*>(wrapper.location()),
                          DateFormat::DeleteDateFormat);
  return *local_object;
}


RUNTIME_FUNCTION(Runtime_InternalDateFormat) {
  HandleScope scope(isolate);

  ASSERT(args.length() == 2);

  CONVERT_ARG_HANDLE_CHECKED(JSObject, date_format_holder, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSDate, date, 1);

  Handle<Object> value;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, value, Execution::ToNumber(isolate, date));

  icu::SimpleDateFormat* date_format =
      DateFormat::UnpackDateFormat(isolate, date_format_holder);
  if (!date_format) return isolate->ThrowIllegalOperation();

  icu::UnicodeString result;
  date_format->format(value->Number(), result);

  Handle<String> result_str;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result_str,
      isolate->factory()->NewStringFromTwoByte(
          Vector<const uint16_t>(
              reinterpret_cast<const uint16_t*>(result.getBuffer()),
              result.length())));
  return *result_str;
}


RUNTIME_FUNCTION(Runtime_InternalDateParse) {
  HandleScope scope(isolate);

  ASSERT(args.length() == 2);

  CONVERT_ARG_HANDLE_CHECKED(JSObject, date_format_holder, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, date_string, 1);

  v8::String::Utf8Value utf8_date(v8::Utils::ToLocal(date_string));
  icu::UnicodeString u_date(icu::UnicodeString::fromUTF8(*utf8_date));
  icu::SimpleDateFormat* date_format =
      DateFormat::UnpackDateFormat(isolate, date_format_holder);
  if (!date_format) return isolate->ThrowIllegalOperation();

  UErrorCode status = U_ZERO_ERROR;
  UDate date = date_format->parse(u_date, status);
  if (U_FAILURE(status)) return isolate->heap()->undefined_value();

  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      Execution::NewDate(isolate, static_cast<double>(date)));
  ASSERT(result->IsJSDate());
  return *result;
}


RUNTIME_FUNCTION(Runtime_CreateNumberFormat) {
  HandleScope scope(isolate);

  ASSERT(args.length() == 3);

  CONVERT_ARG_HANDLE_CHECKED(String, locale, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, options, 1);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, resolved, 2);

  Handle<ObjectTemplateInfo> number_format_template =
      I18N::GetTemplate(isolate);

  // Create an empty object wrapper.
  Handle<JSObject> local_object;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, local_object,
      Execution::InstantiateObject(number_format_template));

  // Set number formatter as internal field of the resulting JS object.
  icu::DecimalFormat* number_format = NumberFormat::InitializeNumberFormat(
      isolate, locale, options, resolved);

  if (!number_format) return isolate->ThrowIllegalOperation();

  local_object->SetInternalField(0, reinterpret_cast<Smi*>(number_format));

  RETURN_FAILURE_ON_EXCEPTION(isolate,
      JSObject::SetLocalPropertyIgnoreAttributes(
          local_object,
          isolate->factory()->NewStringFromStaticAscii("numberFormat"),
          isolate->factory()->NewStringFromStaticAscii("valid"),
          NONE));

  Handle<Object> wrapper = isolate->global_handles()->Create(*local_object);
  GlobalHandles::MakeWeak(wrapper.location(),
                          reinterpret_cast<void*>(wrapper.location()),
                          NumberFormat::DeleteNumberFormat);
  return *local_object;
}


RUNTIME_FUNCTION(Runtime_InternalNumberFormat) {
  HandleScope scope(isolate);

  ASSERT(args.length() == 2);

  CONVERT_ARG_HANDLE_CHECKED(JSObject, number_format_holder, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, number, 1);

  Handle<Object> value;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, value, Execution::ToNumber(isolate, number));

  icu::DecimalFormat* number_format =
      NumberFormat::UnpackNumberFormat(isolate, number_format_holder);
  if (!number_format) return isolate->ThrowIllegalOperation();

  icu::UnicodeString result;
  number_format->format(value->Number(), result);

  Handle<String> result_str;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result_str,
      isolate->factory()->NewStringFromTwoByte(
          Vector<const uint16_t>(
              reinterpret_cast<const uint16_t*>(result.getBuffer()),
              result.length())));
  return *result_str;
}


RUNTIME_FUNCTION(Runtime_InternalNumberParse) {
  HandleScope scope(isolate);

  ASSERT(args.length() == 2);

  CONVERT_ARG_HANDLE_CHECKED(JSObject, number_format_holder, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, number_string, 1);

  v8::String::Utf8Value utf8_number(v8::Utils::ToLocal(number_string));
  icu::UnicodeString u_number(icu::UnicodeString::fromUTF8(*utf8_number));
  icu::DecimalFormat* number_format =
      NumberFormat::UnpackNumberFormat(isolate, number_format_holder);
  if (!number_format) return isolate->ThrowIllegalOperation();

  UErrorCode status = U_ZERO_ERROR;
  icu::Formattable result;
  // ICU 4.6 doesn't support parseCurrency call. We need to wait for ICU49
  // to be part of Chrome.
  // TODO(cira): Include currency parsing code using parseCurrency call.
  // We need to check if the formatter parses all currencies or only the
  // one it was constructed with (it will impact the API - how to return ISO
  // code and the value).
  number_format->parse(u_number, result, status);
  if (U_FAILURE(status)) return isolate->heap()->undefined_value();

  switch (result.getType()) {
  case icu::Formattable::kDouble:
    return *isolate->factory()->NewNumber(result.getDouble());
  case icu::Formattable::kLong:
    return *isolate->factory()->NewNumberFromInt(result.getLong());
  case icu::Formattable::kInt64:
    return *isolate->factory()->NewNumber(
        static_cast<double>(result.getInt64()));
  default:
    return isolate->heap()->undefined_value();
  }
}


RUNTIME_FUNCTION(Runtime_CreateCollator) {
  HandleScope scope(isolate);

  ASSERT(args.length() == 3);

  CONVERT_ARG_HANDLE_CHECKED(String, locale, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, options, 1);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, resolved, 2);

  Handle<ObjectTemplateInfo> collator_template = I18N::GetTemplate(isolate);

  // Create an empty object wrapper.
  Handle<JSObject> local_object;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, local_object, Execution::InstantiateObject(collator_template));

  // Set collator as internal field of the resulting JS object.
  icu::Collator* collator = Collator::InitializeCollator(
      isolate, locale, options, resolved);

  if (!collator) return isolate->ThrowIllegalOperation();

  local_object->SetInternalField(0, reinterpret_cast<Smi*>(collator));

  RETURN_FAILURE_ON_EXCEPTION(isolate,
      JSObject::SetLocalPropertyIgnoreAttributes(
          local_object,
          isolate->factory()->NewStringFromStaticAscii("collator"),
          isolate->factory()->NewStringFromStaticAscii("valid"),
          NONE));

  Handle<Object> wrapper = isolate->global_handles()->Create(*local_object);
  GlobalHandles::MakeWeak(wrapper.location(),
                          reinterpret_cast<void*>(wrapper.location()),
                          Collator::DeleteCollator);
  return *local_object;
}


RUNTIME_FUNCTION(Runtime_InternalCompare) {
  HandleScope scope(isolate);

  ASSERT(args.length() == 3);

  CONVERT_ARG_HANDLE_CHECKED(JSObject, collator_holder, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, string1, 1);
  CONVERT_ARG_HANDLE_CHECKED(String, string2, 2);

  icu::Collator* collator = Collator::UnpackCollator(isolate, collator_holder);
  if (!collator) return isolate->ThrowIllegalOperation();

  v8::String::Value string_value1(v8::Utils::ToLocal(string1));
  v8::String::Value string_value2(v8::Utils::ToLocal(string2));
  const UChar* u_string1 = reinterpret_cast<const UChar*>(*string_value1);
  const UChar* u_string2 = reinterpret_cast<const UChar*>(*string_value2);
  UErrorCode status = U_ZERO_ERROR;
  UCollationResult result = collator->compare(u_string1,
                                              string_value1.length(),
                                              u_string2,
                                              string_value2.length(),
                                              status);
  if (U_FAILURE(status)) return isolate->ThrowIllegalOperation();

  return *isolate->factory()->NewNumberFromInt(result);
}


RUNTIME_FUNCTION(Runtime_StringNormalize) {
  HandleScope scope(isolate);
  static const UNormalizationMode normalizationForms[] =
      { UNORM_NFC, UNORM_NFD, UNORM_NFKC, UNORM_NFKD };

  ASSERT(args.length() == 2);

  CONVERT_ARG_HANDLE_CHECKED(String, stringValue, 0);
  CONVERT_NUMBER_CHECKED(int, form_id, Int32, args[1]);
  RUNTIME_ASSERT(form_id >= 0 &&
                 static_cast<size_t>(form_id) < ARRAY_SIZE(normalizationForms));

  v8::String::Value string_value(v8::Utils::ToLocal(stringValue));
  const UChar* u_value = reinterpret_cast<const UChar*>(*string_value);

  // TODO(mnita): check Normalizer2 (not available in ICU 46)
  UErrorCode status = U_ZERO_ERROR;
  icu::UnicodeString result;
  icu::Normalizer::normalize(u_value, normalizationForms[form_id], 0,
      result, status);
  if (U_FAILURE(status)) {
    return isolate->heap()->undefined_value();
  }

  Handle<String> result_str;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result_str,
      isolate->factory()->NewStringFromTwoByte(
          Vector<const uint16_t>(
              reinterpret_cast<const uint16_t*>(result.getBuffer()),
              result.length())));
  return *result_str;
}


RUNTIME_FUNCTION(Runtime_CreateBreakIterator) {
  HandleScope scope(isolate);

  ASSERT(args.length() == 3);

  CONVERT_ARG_HANDLE_CHECKED(String, locale, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, options, 1);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, resolved, 2);

  Handle<ObjectTemplateInfo> break_iterator_template =
      I18N::GetTemplate2(isolate);

  // Create an empty object wrapper.
  Handle<JSObject> local_object;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, local_object,
      Execution::InstantiateObject(break_iterator_template));

  // Set break iterator as internal field of the resulting JS object.
  icu::BreakIterator* break_iterator = BreakIterator::InitializeBreakIterator(
      isolate, locale, options, resolved);

  if (!break_iterator) return isolate->ThrowIllegalOperation();

  local_object->SetInternalField(0, reinterpret_cast<Smi*>(break_iterator));
  // Make sure that the pointer to adopted text is NULL.
  local_object->SetInternalField(1, reinterpret_cast<Smi*>(NULL));

  RETURN_FAILURE_ON_EXCEPTION(isolate,
      JSObject::SetLocalPropertyIgnoreAttributes(
          local_object,
          isolate->factory()->NewStringFromStaticAscii("breakIterator"),
          isolate->factory()->NewStringFromStaticAscii("valid"),
          NONE));

  // Make object handle weak so we can delete the break iterator once GC kicks
  // in.
  Handle<Object> wrapper = isolate->global_handles()->Create(*local_object);
  GlobalHandles::MakeWeak(wrapper.location(),
                          reinterpret_cast<void*>(wrapper.location()),
                          BreakIterator::DeleteBreakIterator);
  return *local_object;
}


RUNTIME_FUNCTION(Runtime_BreakIteratorAdoptText) {
  HandleScope scope(isolate);

  ASSERT(args.length() == 2);

  CONVERT_ARG_HANDLE_CHECKED(JSObject, break_iterator_holder, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, text, 1);

  icu::BreakIterator* break_iterator =
      BreakIterator::UnpackBreakIterator(isolate, break_iterator_holder);
  if (!break_iterator) return isolate->ThrowIllegalOperation();

  icu::UnicodeString* u_text = reinterpret_cast<icu::UnicodeString*>(
      break_iterator_holder->GetInternalField(1));
  delete u_text;

  v8::String::Value text_value(v8::Utils::ToLocal(text));
  u_text = new icu::UnicodeString(
      reinterpret_cast<const UChar*>(*text_value), text_value.length());
  break_iterator_holder->SetInternalField(1, reinterpret_cast<Smi*>(u_text));

  break_iterator->setText(*u_text);

  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_BreakIteratorFirst) {
  HandleScope scope(isolate);

  ASSERT(args.length() == 1);

  CONVERT_ARG_HANDLE_CHECKED(JSObject, break_iterator_holder, 0);

  icu::BreakIterator* break_iterator =
      BreakIterator::UnpackBreakIterator(isolate, break_iterator_holder);
  if (!break_iterator) return isolate->ThrowIllegalOperation();

  return *isolate->factory()->NewNumberFromInt(break_iterator->first());
}


RUNTIME_FUNCTION(Runtime_BreakIteratorNext) {
  HandleScope scope(isolate);

  ASSERT(args.length() == 1);

  CONVERT_ARG_HANDLE_CHECKED(JSObject, break_iterator_holder, 0);

  icu::BreakIterator* break_iterator =
      BreakIterator::UnpackBreakIterator(isolate, break_iterator_holder);
  if (!break_iterator) return isolate->ThrowIllegalOperation();

  return *isolate->factory()->NewNumberFromInt(break_iterator->next());
}


RUNTIME_FUNCTION(Runtime_BreakIteratorCurrent) {
  HandleScope scope(isolate);

  ASSERT(args.length() == 1);

  CONVERT_ARG_HANDLE_CHECKED(JSObject, break_iterator_holder, 0);

  icu::BreakIterator* break_iterator =
      BreakIterator::UnpackBreakIterator(isolate, break_iterator_holder);
  if (!break_iterator) return isolate->ThrowIllegalOperation();

  return *isolate->factory()->NewNumberFromInt(break_iterator->current());
}


RUNTIME_FUNCTION(Runtime_BreakIteratorBreakType) {
  HandleScope scope(isolate);

  ASSERT(args.length() == 1);

  CONVERT_ARG_HANDLE_CHECKED(JSObject, break_iterator_holder, 0);

  icu::BreakIterator* break_iterator =
      BreakIterator::UnpackBreakIterator(isolate, break_iterator_holder);
  if (!break_iterator) return isolate->ThrowIllegalOperation();

  // TODO(cira): Remove cast once ICU fixes base BreakIterator class.
  icu::RuleBasedBreakIterator* rule_based_iterator =
      static_cast<icu::RuleBasedBreakIterator*>(break_iterator);
  int32_t status = rule_based_iterator->getRuleStatus();
  // Keep return values in sync with JavaScript BreakType enum.
  if (status >= UBRK_WORD_NONE && status < UBRK_WORD_NONE_LIMIT) {
    return *isolate->factory()->NewStringFromStaticAscii("none");
  } else if (status >= UBRK_WORD_NUMBER && status < UBRK_WORD_NUMBER_LIMIT) {
    return *isolate->factory()->NewStringFromStaticAscii("number");
  } else if (status >= UBRK_WORD_LETTER && status < UBRK_WORD_LETTER_LIMIT) {
    return *isolate->factory()->NewStringFromStaticAscii("letter");
  } else if (status >= UBRK_WORD_KANA && status < UBRK_WORD_KANA_LIMIT) {
    return *isolate->factory()->NewStringFromStaticAscii("kana");
  } else if (status >= UBRK_WORD_IDEO && status < UBRK_WORD_IDEO_LIMIT) {
    return *isolate->factory()->NewStringFromStaticAscii("ideo");
  } else {
    return *isolate->factory()->NewStringFromStaticAscii("unknown");
  }
}
#endif  // V8_I18N_SUPPORT


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
  return Script::GetWrapper(script);
}


// Get the script object from script data. NOTE: Regarding performance
// see the NOTE for GetScriptFromScriptData.
// args[0]: script data for the script to find the source for
RUNTIME_FUNCTION(Runtime_GetScript) {
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
RUNTIME_FUNCTION(Runtime_CollectStackTrace) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 3);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, error_object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Object, caller, 1);
  CONVERT_NUMBER_CHECKED(int32_t, limit, Int32, args[2]);

  // Optionally capture a more detailed stack trace for the message.
  isolate->CaptureAndSetDetailedStackTrace(error_object);
  // Capture a simple stack trace for the stack property.
  return *isolate->CaptureSimpleStackTrace(error_object, caller, limit);
}


// Retrieve the stack trace.  This is the raw stack trace that yet has to
// be formatted.  Since we only need this once, clear it afterwards.
RUNTIME_FUNCTION(Runtime_GetAndClearOverflowedStackTrace) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, error_object, 0);
  Handle<String> key = isolate->factory()->hidden_stack_trace_string();
  Handle<Object> result(error_object->GetHiddenProperty(key), isolate);
  if (result->IsTheHole()) return isolate->heap()->undefined_value();
  RUNTIME_ASSERT(result->IsJSArray() || result->IsUndefined());
  JSObject::DeleteHiddenProperty(error_object, key);
  return *result;
}


// Returns V8 version as a string.
RUNTIME_FUNCTION(Runtime_GetV8Version) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 0);

  const char* version_string = v8::V8::GetVersion();

  return *isolate->factory()->NewStringFromAsciiChecked(version_string);
}


RUNTIME_FUNCTION(Runtime_Abort) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  CONVERT_SMI_ARG_CHECKED(message_id, 0);
  const char* message = GetBailoutReason(
      static_cast<BailoutReason>(message_id));
  OS::PrintError("abort: %s\n", message);
  isolate->PrintStack(stderr);
  OS::Abort();
  UNREACHABLE();
  return NULL;
}


RUNTIME_FUNCTION(Runtime_AbortJS) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(String, message, 0);
  OS::PrintError("abort: %s\n", message->ToCString().get());
  isolate->PrintStack(stderr);
  OS::Abort();
  UNREACHABLE();
  return NULL;
}


RUNTIME_FUNCTION(Runtime_FlattenString) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(String, str, 0);
  return *String::Flatten(str);
}


RUNTIME_FUNCTION(Runtime_NotifyContextDisposed) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 0);
  isolate->heap()->NotifyContextDisposed();
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_LoadMutableDouble) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 0);
  CONVERT_ARG_HANDLE_CHECKED(Smi, index, 1);
  int idx = index->value() >> 1;
  int inobject_properties = object->map()->inobject_properties();
  if (idx < 0) {
    idx = -idx + inobject_properties - 1;
  }
  int max_idx = object->properties()->length() + inobject_properties;
  RUNTIME_ASSERT(idx < max_idx);
  Handle<Object> raw_value(object->RawFastPropertyAt(idx), isolate);
  RUNTIME_ASSERT(raw_value->IsNumber() || raw_value->IsUninitialized());
  return *Object::NewStorageFor(isolate, raw_value, Representation::Double());
}


RUNTIME_FUNCTION(Runtime_TryMigrateInstance) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, object, 0);
  if (!object->IsJSObject()) return Smi::FromInt(0);
  Handle<JSObject> js_object = Handle<JSObject>::cast(object);
  if (!js_object->map()->is_deprecated()) return Smi::FromInt(0);
  // This call must not cause lazy deopts, because it's called from deferred
  // code where we can't handle lazy deopts for lack of a suitable bailout
  // ID. So we just try migration and signal failure if necessary,
  // which will also trigger a deopt.
  if (!JSObject::TryMigrateInstance(js_object)) return Smi::FromInt(0);
  return *object;
}


RUNTIME_FUNCTION(RuntimeHidden_GetFromCache) {
  SealHandleScope shs(isolate);
  // This is only called from codegen, so checks might be more lax.
  CONVERT_ARG_CHECKED(JSFunctionResultCache, cache, 0);
  CONVERT_ARG_CHECKED(Object, key, 1);

  {
    DisallowHeapAllocation no_alloc;

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
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, value,
        Execution::Call(isolate, factory, receiver, ARRAY_SIZE(argv), argv));
  }

#ifdef VERIFY_HEAP
  if (FLAG_verify_heap) {
    cache_handle->JSFunctionResultCacheVerify();
  }
#endif

  // Function invocation may have cleared the cache.  Reread all the data.
  int finger_index = cache_handle->finger_index();
  int size = cache_handle->size();

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


RUNTIME_FUNCTION(Runtime_MessageGetStartPosition) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_CHECKED(JSMessageObject, message, 0);
  return Smi::FromInt(message->start_position());
}


RUNTIME_FUNCTION(Runtime_MessageGetScript) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_CHECKED(JSMessageObject, message, 0);
  return message->script();
}


#ifdef DEBUG
// ListNatives is ONLY used by the fuzz-natives.js in debug mode
// Exclude the code in release mode.
RUNTIME_FUNCTION(Runtime_ListNatives) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 0);
#define COUNT_ENTRY(Name, argc, ressize) + 1
  int entry_count = 0
      RUNTIME_FUNCTION_LIST(COUNT_ENTRY)
      RUNTIME_HIDDEN_FUNCTION_LIST(COUNT_ENTRY)
      INLINE_FUNCTION_LIST(COUNT_ENTRY);
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
      name = factory->NewStringFromStaticAscii("_" #Name);                   \
    } else {                                                                 \
      name = factory->NewStringFromStaticAscii(#Name);                       \
    }                                                                        \
    Handle<FixedArray> pair_elements = factory->NewFixedArray(2);            \
    pair_elements->set(0, *name);                                            \
    pair_elements->set(1, Smi::FromInt(argc));                               \
    Handle<JSArray> pair = factory->NewJSArrayWithElements(pair_elements);   \
    elements->set(index++, *pair);                                           \
  }
  inline_runtime_functions = false;
  RUNTIME_FUNCTION_LIST(ADD_ENTRY)
  // Calling hidden runtime functions should just throw.
  RUNTIME_HIDDEN_FUNCTION_LIST(ADD_ENTRY)
  inline_runtime_functions = true;
  INLINE_FUNCTION_LIST(ADD_ENTRY)
#undef ADD_ENTRY
  ASSERT_EQ(index, entry_count);
  Handle<JSArray> result = factory->NewJSArrayWithElements(elements);
  return *result;
}
#endif


RUNTIME_FUNCTION(Runtime_IS_VAR) {
  UNREACHABLE();  // implemented as macro in the parser
  return NULL;
}


#define ELEMENTS_KIND_CHECK_RUNTIME_FUNCTION(Name)        \
  RUNTIME_FUNCTION(Runtime_Has##Name) {          \
    CONVERT_ARG_CHECKED(JSObject, obj, 0);                \
    return isolate->heap()->ToBoolean(obj->Has##Name());  \
  }

ELEMENTS_KIND_CHECK_RUNTIME_FUNCTION(FastSmiElements)
ELEMENTS_KIND_CHECK_RUNTIME_FUNCTION(FastObjectElements)
ELEMENTS_KIND_CHECK_RUNTIME_FUNCTION(FastSmiOrObjectElements)
ELEMENTS_KIND_CHECK_RUNTIME_FUNCTION(FastDoubleElements)
ELEMENTS_KIND_CHECK_RUNTIME_FUNCTION(FastHoleyElements)
ELEMENTS_KIND_CHECK_RUNTIME_FUNCTION(DictionaryElements)
ELEMENTS_KIND_CHECK_RUNTIME_FUNCTION(SloppyArgumentsElements)
ELEMENTS_KIND_CHECK_RUNTIME_FUNCTION(ExternalArrayElements)
// Properties test sitting with elements tests - not fooling anyone.
ELEMENTS_KIND_CHECK_RUNTIME_FUNCTION(FastProperties)

#undef ELEMENTS_KIND_CHECK_RUNTIME_FUNCTION


#define TYPED_ARRAYS_CHECK_RUNTIME_FUNCTION(Type, type, TYPE, ctype, size)     \
  RUNTIME_FUNCTION(Runtime_HasExternal##Type##Elements) {             \
    CONVERT_ARG_CHECKED(JSObject, obj, 0);                                     \
    return isolate->heap()->ToBoolean(obj->HasExternal##Type##Elements());     \
  }

TYPED_ARRAYS(TYPED_ARRAYS_CHECK_RUNTIME_FUNCTION)

#undef TYPED_ARRAYS_CHECK_RUNTIME_FUNCTION


#define FIXED_TYPED_ARRAYS_CHECK_RUNTIME_FUNCTION(Type, type, TYPE, ctype, s)  \
  RUNTIME_FUNCTION(Runtime_HasFixed##Type##Elements) {                \
    CONVERT_ARG_CHECKED(JSObject, obj, 0);                                     \
    return isolate->heap()->ToBoolean(obj->HasFixed##Type##Elements());        \
  }

TYPED_ARRAYS(FIXED_TYPED_ARRAYS_CHECK_RUNTIME_FUNCTION)

#undef FIXED_TYPED_ARRAYS_CHECK_RUNTIME_FUNCTION


RUNTIME_FUNCTION(Runtime_HaveSameMap) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 2);
  CONVERT_ARG_CHECKED(JSObject, obj1, 0);
  CONVERT_ARG_CHECKED(JSObject, obj2, 1);
  return isolate->heap()->ToBoolean(obj1->map() == obj2->map());
}


RUNTIME_FUNCTION(Runtime_IsJSGlobalProxy) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_CHECKED(Object, obj, 0);
  return isolate->heap()->ToBoolean(obj->IsJSGlobalProxy());
}


RUNTIME_FUNCTION(Runtime_IsObserved) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);

  if (!args[0]->IsJSReceiver()) return isolate->heap()->false_value();
  CONVERT_ARG_CHECKED(JSReceiver, obj, 0);
  ASSERT(!obj->IsJSGlobalProxy() || !obj->map()->is_observed());
  return isolate->heap()->ToBoolean(obj->map()->is_observed());
}


RUNTIME_FUNCTION(Runtime_SetIsObserved) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, obj, 0);
  ASSERT(!obj->IsJSGlobalProxy());
  if (obj->IsJSProxy())
    return isolate->heap()->undefined_value();

  ASSERT(obj->IsJSObject());
  JSObject::SetObserved(Handle<JSObject>::cast(obj));
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_SetMicrotaskPending) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 1);
  CONVERT_BOOLEAN_ARG_CHECKED(new_state, 0);
  bool old_state = isolate->microtask_pending();
  isolate->set_microtask_pending(new_state);
  return isolate->heap()->ToBoolean(old_state);
}


RUNTIME_FUNCTION(Runtime_RunMicrotasks) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 0);
  if (isolate->microtask_pending()) Execution::RunMicrotasks(isolate);
  return isolate->heap()->undefined_value();
}


RUNTIME_FUNCTION(Runtime_GetMicrotaskState) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 0);
  return isolate->heap()->microtask_state();
}


RUNTIME_FUNCTION(Runtime_GetObservationState) {
  SealHandleScope shs(isolate);
  ASSERT(args.length() == 0);
  return isolate->heap()->observation_state();
}


RUNTIME_FUNCTION(Runtime_ObservationWeakMapCreate) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 0);
  // TODO(adamk): Currently this runtime function is only called three times per
  // isolate. If it's called more often, the map should be moved into the
  // strong root list.
  Handle<Map> map =
      isolate->factory()->NewMap(JS_WEAK_MAP_TYPE, JSWeakMap::kSize);
  Handle<JSWeakMap> weakmap =
      Handle<JSWeakMap>::cast(isolate->factory()->NewJSObjectFromMap(map));
  return *WeakCollectionInitialize(isolate, weakmap);
}


static bool ContextsHaveSameOrigin(Handle<Context> context1,
                                   Handle<Context> context2) {
  return context1->security_token() == context2->security_token();
}


RUNTIME_FUNCTION(Runtime_ObserverObjectAndRecordHaveSameOrigin) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 3);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, observer, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 1);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, record, 2);

  Handle<Context> observer_context(observer->context()->native_context(),
      isolate);
  Handle<Context> object_context(object->GetCreationContext());
  Handle<Context> record_context(record->GetCreationContext());

  return isolate->heap()->ToBoolean(
      ContextsHaveSameOrigin(object_context, observer_context) &&
      ContextsHaveSameOrigin(object_context, record_context));
}


RUNTIME_FUNCTION(Runtime_ObjectWasCreatedInCurrentOrigin) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 0);

  Handle<Context> creation_context(object->GetCreationContext(), isolate);
  return isolate->heap()->ToBoolean(
      ContextsHaveSameOrigin(creation_context, isolate->native_context()));
}


RUNTIME_FUNCTION(Runtime_ObjectObserveInObjectContext) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 3);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, callback, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, accept, 2);
  RUNTIME_ASSERT(accept->IsUndefined() || accept->IsJSObject());

  Handle<Context> context(object->GetCreationContext(), isolate);
  Handle<JSFunction> function(context->native_object_observe(), isolate);
  Handle<Object> call_args[] = { object, callback, accept };
  Handle<Object> result;

  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      Execution::Call(isolate, function,
          handle(context->object_function(), isolate),
          ARRAY_SIZE(call_args), call_args, true));
  return *result;
}


RUNTIME_FUNCTION(Runtime_ObjectGetNotifierInObjectContext) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, object, 0);

  Handle<Context> context(object->GetCreationContext(), isolate);
  Handle<JSFunction> function(context->native_object_get_notifier(), isolate);
  Handle<Object> call_args[] = { object };
  Handle<Object> result;

  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      Execution::Call(isolate, function,
          handle(context->object_function(), isolate),
          ARRAY_SIZE(call_args), call_args, true));
  return *result;
}


RUNTIME_FUNCTION(Runtime_ObjectNotifierPerformChangeInObjectContext) {
  HandleScope scope(isolate);
  ASSERT(args.length() == 3);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, object_info, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, change_type, 1);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, change_fn, 2);

  Handle<Context> context(object_info->GetCreationContext(), isolate);
  Handle<JSFunction> function(context->native_object_notifier_perform_change(),
      isolate);
  Handle<Object> call_args[] = { object_info, change_type, change_fn };
  Handle<Object> result;

  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      Execution::Call(isolate, function, isolate->factory()->undefined_value(),
                      ARRAY_SIZE(call_args), call_args, true));
  return *result;
}


static Object* ArrayConstructorCommon(Isolate* isolate,
                                           Handle<JSFunction> constructor,
                                           Handle<AllocationSite> site,
                                           Arguments* caller_args) {
  Factory* factory = isolate->factory();

  bool holey = false;
  bool can_use_type_feedback = true;
  if (caller_args->length() == 1) {
    Handle<Object> argument_one = caller_args->at<Object>(0);
    if (argument_one->IsSmi()) {
      int value = Handle<Smi>::cast(argument_one)->value();
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

  Handle<JSArray> array;
  if (!site.is_null() && can_use_type_feedback) {
    ElementsKind to_kind = site->GetElementsKind();
    if (holey && !IsFastHoleyElementsKind(to_kind)) {
      to_kind = GetHoleyElementsKind(to_kind);
      // Update the allocation site info to reflect the advice alteration.
      site->SetElementsKind(to_kind);
    }

    // We should allocate with an initial map that reflects the allocation site
    // advice. Therefore we use AllocateJSObjectFromMap instead of passing
    // the constructor.
    Handle<Map> initial_map(constructor->initial_map(), isolate);
    if (to_kind != initial_map->elements_kind()) {
      initial_map = Map::AsElementsKind(initial_map, to_kind);
    }

    // If we don't care to track arrays of to_kind ElementsKind, then
    // don't emit a memento for them.
    Handle<AllocationSite> allocation_site;
    if (AllocationSite::GetMode(to_kind) == TRACK_ALLOCATION_SITE) {
      allocation_site = site;
    }

    array = Handle<JSArray>::cast(factory->NewJSObjectFromMap(
        initial_map, NOT_TENURED, true, allocation_site));
  } else {
    array = Handle<JSArray>::cast(factory->NewJSObject(constructor));

    // We might need to transition to holey
    ElementsKind kind = constructor->initial_map()->elements_kind();
    if (holey && !IsFastHoleyElementsKind(kind)) {
      kind = GetHoleyElementsKind(kind);
      JSObject::TransitionElementsKind(array, kind);
    }
  }

  factory->NewJSArrayStorage(array, 0, 0, DONT_INITIALIZE_ARRAY_ELEMENTS);

  ElementsKind old_kind = array->GetElementsKind();
  RETURN_FAILURE_ON_EXCEPTION(
      isolate, ArrayConstructInitializeElements(array, caller_args));
  if (!site.is_null() &&
      (old_kind != array->GetElementsKind() ||
       !can_use_type_feedback)) {
    // The arguments passed in caused a transition. This kind of complexity
    // can't be dealt with in the inlined hydrogen array constructor case.
    // We must mark the allocationsite as un-inlinable.
    site->SetDoNotInlineCall();
  }
  return *array;
}


RUNTIME_FUNCTION(RuntimeHidden_ArrayConstructor) {
  HandleScope scope(isolate);
  // If we get 2 arguments then they are the stub parameters (constructor, type
  // info).  If we get 4, then the first one is a pointer to the arguments
  // passed by the caller, and the last one is the length of the arguments
  // passed to the caller (redundant, but useful to check on the deoptimizer
  // with an assert).
  Arguments empty_args(0, NULL);
  bool no_caller_args = args.length() == 2;
  ASSERT(no_caller_args || args.length() == 4);
  int parameters_start = no_caller_args ? 0 : 1;
  Arguments* caller_args = no_caller_args
      ? &empty_args
      : reinterpret_cast<Arguments*>(args[0]);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, constructor, parameters_start);
  CONVERT_ARG_HANDLE_CHECKED(Object, type_info, parameters_start + 1);
#ifdef DEBUG
  if (!no_caller_args) {
    CONVERT_SMI_ARG_CHECKED(arg_count, parameters_start + 2);
    ASSERT(arg_count == caller_args->length());
  }
#endif

  Handle<AllocationSite> site;
  if (!type_info.is_null() &&
      *type_info != isolate->heap()->undefined_value()) {
    site = Handle<AllocationSite>::cast(type_info);
    ASSERT(!site->SitePointsToLiteral());
  }

  return ArrayConstructorCommon(isolate,
                                constructor,
                                site,
                                caller_args);
}


RUNTIME_FUNCTION(RuntimeHidden_InternalArrayConstructor) {
  HandleScope scope(isolate);
  Arguments empty_args(0, NULL);
  bool no_caller_args = args.length() == 1;
  ASSERT(no_caller_args || args.length() == 3);
  int parameters_start = no_caller_args ? 0 : 1;
  Arguments* caller_args = no_caller_args
      ? &empty_args
      : reinterpret_cast<Arguments*>(args[0]);
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, constructor, parameters_start);
#ifdef DEBUG
  if (!no_caller_args) {
    CONVERT_SMI_ARG_CHECKED(arg_count, parameters_start + 1);
    ASSERT(arg_count == caller_args->length());
  }
#endif
  return ArrayConstructorCommon(isolate,
                                constructor,
                                Handle<AllocationSite>::null(),
                                caller_args);
}


RUNTIME_FUNCTION(Runtime_MaxSmi) {
  ASSERT(args.length() == 0);
  return Smi::FromInt(Smi::kMaxValue);
}


// ----------------------------------------------------------------------------
// Implementation of Runtime

#define F(name, number_of_args, result_size)                             \
  { Runtime::k##name, Runtime::RUNTIME, #name,   \
    FUNCTION_ADDR(Runtime_##name), number_of_args, result_size },


#define FH(name, number_of_args, result_size)                             \
  { Runtime::kHidden##name, Runtime::RUNTIME_HIDDEN, NULL,   \
    FUNCTION_ADDR(RuntimeHidden_##name), number_of_args, result_size },


#define I(name, number_of_args, result_size)                             \
  { Runtime::kInline##name, Runtime::INLINE,     \
    "_" #name, NULL, number_of_args, result_size },


#define IO(name, number_of_args, result_size) \
  { Runtime::kInlineOptimized##name, Runtime::INLINE_OPTIMIZED, \
    "_" #name, FUNCTION_ADDR(Runtime_##name), number_of_args, result_size },


static const Runtime::Function kIntrinsicFunctions[] = {
  RUNTIME_FUNCTION_LIST(F)
  RUNTIME_HIDDEN_FUNCTION_LIST(FH)
  INLINE_FUNCTION_LIST(I)
  INLINE_OPTIMIZED_FUNCTION_LIST(IO)
};

#undef IO
#undef I
#undef FH
#undef F


void Runtime::InitializeIntrinsicFunctionNames(Isolate* isolate,
                                               Handle<NameDictionary> dict) {
  ASSERT(dict->NumberOfElements() == 0);
  HandleScope scope(isolate);
  for (int i = 0; i < kNumFunctions; ++i) {
    const char* name = kIntrinsicFunctions[i].name;
    if (name == NULL) continue;
    Handle<NameDictionary> new_dict = NameDictionary::Add(
        dict,
        isolate->factory()->InternalizeUtf8String(name),
        Handle<Smi>(Smi::FromInt(i), isolate),
        PropertyDetails(NONE, NORMAL, Representation::None()));
    // The dictionary does not need to grow.
    CHECK(new_dict.is_identical_to(dict));
  }
}


const Runtime::Function* Runtime::FunctionForName(Handle<String> name) {
  Heap* heap = name->GetHeap();
  int entry = heap->intrinsic_function_names()->FindEntry(name);
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

} }  // namespace v8::internal
