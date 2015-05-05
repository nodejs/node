// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/allocation-site-scopes.h"
#include "src/arguments.h"
#include "src/ast.h"
#include "src/parser.h"
#include "src/runtime/runtime.h"
#include "src/runtime/runtime-utils.h"

namespace v8 {
namespace internal {

static Handle<Map> ComputeObjectLiteralMap(
    Handle<Context> context, Handle<FixedArray> constant_properties,
    bool* is_result_from_cache) {
  int properties_length = constant_properties->length();
  int number_of_properties = properties_length / 2;

  for (int p = 0; p != properties_length; p += 2) {
    Object* key = constant_properties->get(p);
    uint32_t element_index = 0;
    if (key->ToArrayIndex(&element_index)) {
      // An index key does not require space in the property backing store.
      number_of_properties--;
    }
  }
  Isolate* isolate = context->GetIsolate();
  return isolate->factory()->ObjectLiteralMapFromCache(
      context, number_of_properties, is_result_from_cache);
}

MUST_USE_RESULT static MaybeHandle<Object> CreateLiteralBoilerplate(
    Isolate* isolate, Handle<FixedArray> literals,
    Handle<FixedArray> constant_properties);


MUST_USE_RESULT static MaybeHandle<Object> CreateObjectLiteralBoilerplate(
    Isolate* isolate, Handle<FixedArray> literals,
    Handle<FixedArray> constant_properties, bool should_have_fast_elements,
    bool has_function_literal) {
  Handle<Context> context = isolate->native_context();

  // In case we have function literals, we want the object to be in
  // slow properties mode for now. We don't go in the map cache because
  // maps with constant functions can't be shared if the functions are
  // not the same (which is the common case).
  bool is_result_from_cache = false;
  Handle<Map> map = has_function_literal
                        ? Handle<Map>(context->object_function()->initial_map())
                        : ComputeObjectLiteralMap(context, constant_properties,
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
  bool should_normalize = should_transform || has_function_literal;
  if (should_normalize) {
    // TODO(verwaest): We might not want to ever normalize here.
    JSObject::NormalizeProperties(boilerplate, KEEP_INOBJECT_PROPERTIES,
                                  length / 2, "Boilerplate");
  }
  // TODO(verwaest): Support tracking representations in the boilerplate.
  for (int index = 0; index < length; index += 2) {
    Handle<Object> key(constant_properties->get(index + 0), isolate);
    Handle<Object> value(constant_properties->get(index + 1), isolate);
    if (value->IsFixedArray()) {
      // The value contains the constant_properties of a
      // simple object or array literal.
      Handle<FixedArray> array = Handle<FixedArray>::cast(value);
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, value, CreateLiteralBoilerplate(isolate, literals, array),
          Object);
    }
    MaybeHandle<Object> maybe_result;
    uint32_t element_index = 0;
    if (key->IsInternalizedString()) {
      if (Handle<String>::cast(key)->AsArrayIndex(&element_index)) {
        // Array index as string (uint32).
        if (value->IsUninitialized()) value = handle(Smi::FromInt(0), isolate);
        maybe_result =
            JSObject::SetOwnElement(boilerplate, element_index, value, SLOPPY);
      } else {
        Handle<String> name(String::cast(*key));
        DCHECK(!name->AsArrayIndex(&element_index));
        maybe_result = JSObject::SetOwnPropertyIgnoreAttributes(
            boilerplate, name, value, NONE);
      }
    } else if (key->ToArrayIndex(&element_index)) {
      // Array index (uint32).
      if (value->IsUninitialized()) value = handle(Smi::FromInt(0), isolate);
      maybe_result =
          JSObject::SetOwnElement(boilerplate, element_index, value, SLOPPY);
    } else {
      // Non-uint32 number.
      DCHECK(key->IsNumber());
      double num = key->Number();
      char arr[100];
      Vector<char> buffer(arr, arraysize(arr));
      const char* str = DoubleToCString(num, buffer);
      Handle<String> name = isolate->factory()->NewStringFromAsciiChecked(str);
      maybe_result = JSObject::SetOwnPropertyIgnoreAttributes(boilerplate, name,
                                                              value, NONE);
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
    JSObject::MigrateSlowToFast(boilerplate,
                                boilerplate->map()->unused_property_fields(),
                                "FastLiteral");
  }
  return boilerplate;
}


MaybeHandle<Object> Runtime::CreateArrayLiteralBoilerplate(
    Isolate* isolate, Handle<FixedArray> literals,
    Handle<FixedArray> elements) {
  // Create the JSArray.
  Handle<JSFunction> constructor = isolate->array_function();

  PretenureFlag pretenure_flag =
      isolate->heap()->InNewSpace(*literals) ? NOT_TENURED : TENURED;

  Handle<JSArray> object = Handle<JSArray>::cast(
      isolate->factory()->NewJSObject(constructor, pretenure_flag));

  ElementsKind constant_elements_kind =
      static_cast<ElementsKind>(Smi::cast(elements->get(0))->value());
  Handle<FixedArrayBase> constant_elements_values(
      FixedArrayBase::cast(elements->get(1)));

  {
    DisallowHeapAllocation no_gc;
    DCHECK(IsFastElementsKind(constant_elements_kind));
    Context* native_context = isolate->context()->native_context();
    Object* maps_array = native_context->js_array_maps();
    DCHECK(!maps_array->IsUndefined());
    Object* map = FixedArray::cast(maps_array)->get(constant_elements_kind);
    object->set_map(Map::cast(map));
  }

  Handle<FixedArrayBase> copied_elements_values;
  if (IsFastDoubleElementsKind(constant_elements_kind)) {
    copied_elements_values = isolate->factory()->CopyFixedDoubleArray(
        Handle<FixedDoubleArray>::cast(constant_elements_values));
  } else {
    DCHECK(IsFastSmiOrObjectElementsKind(constant_elements_kind));
    const bool is_cow = (constant_elements_values->map() ==
                         isolate->heap()->fixed_cow_array_map());
    if (is_cow) {
      copied_elements_values = constant_elements_values;
#if DEBUG
      Handle<FixedArray> fixed_array_values =
          Handle<FixedArray>::cast(copied_elements_values);
      for (int i = 0; i < fixed_array_values->length(); i++) {
        DCHECK(!fixed_array_values->get(i)->IsFixedArray());
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
              isolate, result, CreateLiteralBoilerplate(isolate, literals, fa),
              Object);
          fixed_array_values_copy->set(i, *result);
        }
      }
    }
  }
  object->set_elements(*copied_elements_values);
  object->set_length(Smi::FromInt(copied_elements_values->length()));

  JSObject::ValidateElements(object);
  return object;
}


MUST_USE_RESULT static MaybeHandle<Object> CreateLiteralBoilerplate(
    Isolate* isolate, Handle<FixedArray> literals, Handle<FixedArray> array) {
  Handle<FixedArray> elements = CompileTimeValue::GetElements(array);
  const bool kHasNoFunctionLiteral = false;
  switch (CompileTimeValue::GetLiteralType(array)) {
    case CompileTimeValue::OBJECT_LITERAL_FAST_ELEMENTS:
      return CreateObjectLiteralBoilerplate(isolate, literals, elements, true,
                                            kHasNoFunctionLiteral);
    case CompileTimeValue::OBJECT_LITERAL_SLOW_ELEMENTS:
      return CreateObjectLiteralBoilerplate(isolate, literals, elements, false,
                                            kHasNoFunctionLiteral);
    case CompileTimeValue::ARRAY_LITERAL:
      return Runtime::CreateArrayLiteralBoilerplate(isolate, literals,
                                                    elements);
    default:
      UNREACHABLE();
      return MaybeHandle<Object>();
  }
}


RUNTIME_FUNCTION(Runtime_CreateObjectLiteral) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 4);
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
        CreateObjectLiteralBoilerplate(isolate, literals, constant_properties,
                                       should_have_fast_elements,
                                       has_function_literal));
    boilerplate = Handle<JSObject>::cast(raw_boilerplate);

    AllocationSiteCreationContext creation_context(isolate);
    site = creation_context.EnterNewScope();
    RETURN_FAILURE_ON_EXCEPTION(
        isolate, JSObject::DeepWalk(boilerplate, &creation_context));
    creation_context.ExitScope(site, boilerplate);

    // Update the functions literal and return the boilerplate.
    literals->set(literals_index, *site);
  } else {
    site = Handle<AllocationSite>::cast(literal_site);
    boilerplate =
        Handle<JSObject>(JSObject::cast(site->transition_info()), isolate);
  }

  AllocationSiteUsageContext usage_context(isolate, site, true);
  usage_context.EnterNewScope();
  MaybeHandle<Object> maybe_copy =
      JSObject::DeepCopy(boilerplate, &usage_context);
  usage_context.ExitScope(site, boilerplate);
  Handle<Object> copy;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, copy, maybe_copy);
  return *copy;
}


MUST_USE_RESULT static MaybeHandle<AllocationSite> GetLiteralAllocationSite(
    Isolate* isolate, Handle<FixedArray> literals, int literals_index,
    Handle<FixedArray> elements) {
  // Check if boilerplate exists. If not, create it first.
  Handle<Object> literal_site(literals->get(literals_index), isolate);
  Handle<AllocationSite> site;
  if (*literal_site == isolate->heap()->undefined_value()) {
    DCHECK(*elements != isolate->heap()->empty_fixed_array());
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
  RUNTIME_ASSERT_HANDLIFIED(
      literals_index >= 0 && literals_index < literals->length(), JSObject);
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
                                      : JSObject::kObjectIsShallow;
  MaybeHandle<JSObject> copy =
      JSObject::DeepCopy(boilerplate, &usage_context, hints);
  usage_context.ExitScope(site, boilerplate);
  return copy;
}


RUNTIME_FUNCTION(Runtime_CreateArrayLiteral) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 4);
  CONVERT_ARG_HANDLE_CHECKED(FixedArray, literals, 0);
  CONVERT_SMI_ARG_CHECKED(literals_index, 1);
  CONVERT_ARG_HANDLE_CHECKED(FixedArray, elements, 2);
  CONVERT_SMI_ARG_CHECKED(flags, 3);

  Handle<JSObject> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result, CreateArrayLiteralImpl(isolate, literals, literals_index,
                                              elements, flags));
  return *result;
}


RUNTIME_FUNCTION(Runtime_CreateArrayLiteralStubBailout) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 3);
  CONVERT_ARG_HANDLE_CHECKED(FixedArray, literals, 0);
  CONVERT_SMI_ARG_CHECKED(literals_index, 1);
  CONVERT_ARG_HANDLE_CHECKED(FixedArray, elements, 2);

  Handle<JSObject> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result,
      CreateArrayLiteralImpl(isolate, literals, literals_index, elements,
                             ArrayLiteral::kShallowElements));
  return *result;
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
  DCHECK(IsFastElementsKind(elements_kind));
  // Smis should never trigger transitions.
  DCHECK(!value->IsSmi());

  if (value->IsNumber()) {
    DCHECK(IsFastSmiElementsKind(elements_kind));
    ElementsKind transitioned_kind = IsFastHoleyElementsKind(elements_kind)
                                         ? FAST_HOLEY_DOUBLE_ELEMENTS
                                         : FAST_DOUBLE_ELEMENTS;
    if (IsMoreGeneralElementsKindTransition(
            boilerplate_object->GetElementsKind(), transitioned_kind)) {
      JSObject::TransitionElementsKind(boilerplate_object, transitioned_kind);
    }
    JSObject::TransitionElementsKind(object, transitioned_kind);
    DCHECK(IsFastDoubleElementsKind(object->GetElementsKind()));
    FixedDoubleArray* double_array = FixedDoubleArray::cast(object->elements());
    HeapNumber* number = HeapNumber::cast(*value);
    double_array->set(store_index, number->Number());
  } else {
    if (!IsFastObjectElementsKind(elements_kind)) {
      ElementsKind transitioned_kind = IsFastHoleyElementsKind(elements_kind)
                                           ? FAST_HOLEY_ELEMENTS
                                           : FAST_ELEMENTS;
      JSObject::TransitionElementsKind(object, transitioned_kind);
      ElementsKind boilerplate_elements_kind =
          boilerplate_object->GetElementsKind();
      if (IsMoreGeneralElementsKindTransition(boilerplate_elements_kind,
                                              transitioned_kind)) {
        JSObject::TransitionElementsKind(boilerplate_object, transitioned_kind);
      }
    }
    FixedArray* object_array = FixedArray::cast(object->elements());
    object_array->set(store_index, *value);
  }
  return *object;
}
}
}  // namespace v8::internal
