// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/runtime/runtime-utils.h"

#include "src/allocation-site-scopes.h"
#include "src/arguments.h"
#include "src/ast/ast.h"
#include "src/ast/compile-time-value.h"
#include "src/isolate-inl.h"
#include "src/runtime/runtime.h"

namespace v8 {
namespace internal {

MUST_USE_RESULT static MaybeHandle<Object> CreateLiteralBoilerplate(
    Isolate* isolate, Handle<FeedbackVector> vector,
    Handle<FixedArray> compile_time_value);

MUST_USE_RESULT static MaybeHandle<Object> CreateObjectLiteralBoilerplate(
    Isolate* isolate, Handle<FeedbackVector> vector,
    Handle<BoilerplateDescription> boilerplate_description,
    bool use_fast_elements, bool has_null_prototype) {
  Handle<Context> native_context = isolate->native_context();

  // In case we have function literals, we want the object to be in
  // slow properties mode for now. We don't go in the map cache because
  // maps with constant functions can't be shared if the functions are
  // not the same (which is the common case).
  int number_of_properties = boilerplate_description->backing_store_size();

  // Ignoring number_of_properties for force dictionary map with __proto__:null.
  Handle<Map> map =
      has_null_prototype
          ? handle(native_context->slow_object_with_null_prototype_map(),
                   isolate)
          : isolate->factory()->ObjectLiteralMapFromCache(native_context,
                                                          number_of_properties);

  PretenureFlag pretenure_flag =
      isolate->heap()->InNewSpace(*vector) ? NOT_TENURED : TENURED;

  Handle<JSObject> boilerplate =
      map->is_dictionary_map()
          ? isolate->factory()->NewSlowJSObjectFromMap(
                map, number_of_properties, pretenure_flag)
          : isolate->factory()->NewJSObjectFromMap(map, pretenure_flag);

  // Normalize the elements of the boilerplate to save space if needed.
  if (!use_fast_elements) JSObject::NormalizeElements(boilerplate);

  // Add the constant properties to the boilerplate.
  int length = boilerplate_description->size();
  // TODO(verwaest): Support tracking representations in the boilerplate.
  for (int index = 0; index < length; index++) {
    Handle<Object> key(boilerplate_description->name(index), isolate);
    Handle<Object> value(boilerplate_description->value(index), isolate);
    if (value->IsFixedArray()) {
      // The value contains the CompileTimeValue with the boilerplate properties
      // of a simple object or array literal.
      Handle<FixedArray> compile_time_value = Handle<FixedArray>::cast(value);
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, value,
          CreateLiteralBoilerplate(isolate, vector, compile_time_value),
          Object);
    }
    MaybeHandle<Object> maybe_result;
    uint32_t element_index = 0;
    if (key->ToArrayIndex(&element_index)) {
      // Array index (uint32).
      if (value->IsUninitialized(isolate)) {
        value = handle(Smi::kZero, isolate);
      }
      maybe_result = JSObject::SetOwnElementIgnoreAttributes(
          boilerplate, element_index, value, NONE);
    } else {
      Handle<String> name = Handle<String>::cast(key);
      DCHECK(!name->AsArrayIndex(&element_index));
      maybe_result = JSObject::SetOwnPropertyIgnoreAttributes(boilerplate, name,
                                                              value, NONE);
    }
    RETURN_ON_EXCEPTION(isolate, maybe_result, Object);
  }

  if (map->is_dictionary_map() && !has_null_prototype) {
    // TODO(cbruni): avoid making the boilerplate fast again, the clone stub
    // supports dict-mode objects directly.
    JSObject::MigrateSlowToFast(boilerplate,
                                boilerplate->map()->unused_property_fields(),
                                "FastLiteral");
  }
  return boilerplate;
}

static MaybeHandle<Object> CreateArrayLiteralBoilerplate(
    Isolate* isolate, Handle<FeedbackVector> vector,
    Handle<ConstantElementsPair> elements) {
  // Create the JSArray.
  Handle<JSFunction> constructor = isolate->array_function();

  PretenureFlag pretenure_flag =
      isolate->heap()->InNewSpace(*vector) ? NOT_TENURED : TENURED;

  Handle<JSArray> object = Handle<JSArray>::cast(
      isolate->factory()->NewJSObject(constructor, pretenure_flag));

  ElementsKind constant_elements_kind =
      static_cast<ElementsKind>(elements->elements_kind());
  Handle<FixedArrayBase> constant_elements_values(elements->constant_values());

  {
    DisallowHeapAllocation no_gc;
    DCHECK(IsFastElementsKind(constant_elements_kind));
    Context* native_context = isolate->context()->native_context();
    Object* map =
        native_context->get(Context::ArrayMapIndex(constant_elements_kind));
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
      FOR_WITH_HANDLE_SCOPE(
          isolate, int, i = 0, i, i < fixed_array_values->length(), i++, {
            if (fixed_array_values->get(i)->IsFixedArray()) {
              // The value contains the CompileTimeValue with the
              // boilerplate description of a simple object or
              // array literal.
              Handle<FixedArray> compile_time_value(
                  FixedArray::cast(fixed_array_values->get(i)));
              Handle<Object> result;
              ASSIGN_RETURN_ON_EXCEPTION(
                  isolate, result,
                  CreateLiteralBoilerplate(isolate, vector, compile_time_value),
                  Object);
              fixed_array_values_copy->set(i, *result);
            }
          });
    }
  }
  object->set_elements(*copied_elements_values);
  object->set_length(Smi::FromInt(copied_elements_values->length()));

  JSObject::ValidateElements(object);
  return object;
}

MUST_USE_RESULT static MaybeHandle<Object> CreateLiteralBoilerplate(
    Isolate* isolate, Handle<FeedbackVector> vector,
    Handle<FixedArray> compile_time_value) {
  Handle<HeapObject> elements =
      CompileTimeValue::GetElements(compile_time_value);
  int flags = CompileTimeValue::GetLiteralTypeFlags(compile_time_value);
  if (flags == CompileTimeValue::kArrayLiteralFlag) {
    Handle<ConstantElementsPair> elems =
        Handle<ConstantElementsPair>::cast(elements);
    return CreateArrayLiteralBoilerplate(isolate, vector, elems);
  }
  Handle<BoilerplateDescription> props =
      Handle<BoilerplateDescription>::cast(elements);
  bool use_fast_elements = (flags & ObjectLiteral::kFastElements) != 0;
  bool has_null_prototype = (flags & ObjectLiteral::kHasNullPrototype) != 0;
  return CreateObjectLiteralBoilerplate(isolate, vector, props,
                                        use_fast_elements, has_null_prototype);
}


RUNTIME_FUNCTION(Runtime_CreateRegExpLiteral) {
  HandleScope scope(isolate);
  DCHECK_EQ(4, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, closure, 0);
  CONVERT_SMI_ARG_CHECKED(index, 1);
  CONVERT_ARG_HANDLE_CHECKED(String, pattern, 2);
  CONVERT_SMI_ARG_CHECKED(flags, 3);
  FeedbackSlot literal_slot(FeedbackVector::ToSlot(index));

  // Check if boilerplate exists. If not, create it first.
  Handle<Object> boilerplate(closure->feedback_vector()->Get(literal_slot),
                             isolate);
  if (boilerplate->IsUndefined(isolate)) {
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, boilerplate, JSRegExp::New(pattern, JSRegExp::Flags(flags)));
    closure->feedback_vector()->Set(literal_slot, *boilerplate);
  }
  return *JSRegExp::Copy(Handle<JSRegExp>::cast(boilerplate));
}


RUNTIME_FUNCTION(Runtime_CreateObjectLiteral) {
  HandleScope scope(isolate);
  DCHECK_EQ(4, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, closure, 0);
  CONVERT_SMI_ARG_CHECKED(literals_index, 1);
  CONVERT_ARG_HANDLE_CHECKED(BoilerplateDescription, boilerplate_description,
                             2);
  CONVERT_SMI_ARG_CHECKED(flags, 3);
  Handle<FeedbackVector> vector(closure->feedback_vector(), isolate);
  bool use_fast_elements = (flags & ObjectLiteral::kFastElements) != 0;
  bool enable_mementos = (flags & ObjectLiteral::kDisableMementos) == 0;
  bool has_null_prototype = (flags & ObjectLiteral::kHasNullPrototype) != 0;

  FeedbackSlot literals_slot(FeedbackVector::ToSlot(literals_index));
  CHECK(literals_slot.ToInt() < vector->slot_count());

  // Check if boilerplate exists. If not, create it first.
  Handle<Object> literal_site(vector->Get(literals_slot), isolate);
  Handle<AllocationSite> site;
  Handle<JSObject> boilerplate;
  if (literal_site->IsUndefined(isolate)) {
    Handle<Object> raw_boilerplate;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, raw_boilerplate,
        CreateObjectLiteralBoilerplate(isolate, vector, boilerplate_description,
                                       use_fast_elements, has_null_prototype));
    boilerplate = Handle<JSObject>::cast(raw_boilerplate);

    AllocationSiteCreationContext creation_context(isolate);
    site = creation_context.EnterNewScope();
    RETURN_FAILURE_ON_EXCEPTION(
        isolate, JSObject::DeepWalk(boilerplate, &creation_context));
    creation_context.ExitScope(site, boilerplate);

    // Update the functions literal and return the boilerplate.
    vector->Set(literals_slot, *site);
  } else {
    site = Handle<AllocationSite>::cast(literal_site);
    boilerplate =
        Handle<JSObject>(JSObject::cast(site->transition_info()), isolate);
  }

  AllocationSiteUsageContext usage_context(isolate, site, enable_mementos);
  usage_context.EnterNewScope();
  MaybeHandle<Object> maybe_copy =
      JSObject::DeepCopy(boilerplate, &usage_context);
  usage_context.ExitScope(site, boilerplate);
  RETURN_RESULT_OR_FAILURE(isolate, maybe_copy);
}

MUST_USE_RESULT static MaybeHandle<AllocationSite> GetLiteralAllocationSite(
    Isolate* isolate, Handle<FeedbackVector> vector, FeedbackSlot literals_slot,
    Handle<ConstantElementsPair> elements) {
  // Check if boilerplate exists. If not, create it first.
  Handle<Object> literal_site(vector->Get(literals_slot), isolate);
  Handle<AllocationSite> site;
  if (literal_site->IsUndefined(isolate)) {
    Handle<Object> boilerplate;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, boilerplate,
        CreateArrayLiteralBoilerplate(isolate, vector, elements),
        AllocationSite);

    AllocationSiteCreationContext creation_context(isolate);
    site = creation_context.EnterNewScope();
    if (JSObject::DeepWalk(Handle<JSObject>::cast(boilerplate),
                           &creation_context).is_null()) {
      return Handle<AllocationSite>::null();
    }
    creation_context.ExitScope(site, Handle<JSObject>::cast(boilerplate));

    vector->Set(literals_slot, *site);
  } else {
    site = Handle<AllocationSite>::cast(literal_site);
  }

  return site;
}

static MaybeHandle<JSObject> CreateArrayLiteralImpl(
    Isolate* isolate, Handle<FeedbackVector> vector, FeedbackSlot literals_slot,
    Handle<ConstantElementsPair> elements, int flags) {
  CHECK(literals_slot.ToInt() < vector->slot_count());
  Handle<AllocationSite> site;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, site,
      GetLiteralAllocationSite(isolate, vector, literals_slot, elements),
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
  DCHECK_EQ(4, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, closure, 0);
  CONVERT_SMI_ARG_CHECKED(literals_index, 1);
  CONVERT_ARG_HANDLE_CHECKED(ConstantElementsPair, elements, 2);
  CONVERT_SMI_ARG_CHECKED(flags, 3);

  FeedbackSlot literals_slot(FeedbackVector::ToSlot(literals_index));
  Handle<FeedbackVector> vector(closure->feedback_vector(), isolate);
  RETURN_RESULT_OR_FAILURE(
      isolate,
      CreateArrayLiteralImpl(isolate, vector, literals_slot, elements, flags));
}


RUNTIME_FUNCTION(Runtime_CreateArrayLiteralStubBailout) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSFunction, closure, 0);
  CONVERT_SMI_ARG_CHECKED(literals_index, 1);
  CONVERT_ARG_HANDLE_CHECKED(ConstantElementsPair, elements, 2);

  Handle<FeedbackVector> vector(closure->feedback_vector(), isolate);
  FeedbackSlot literals_slot(FeedbackVector::ToSlot(literals_index));
  RETURN_RESULT_OR_FAILURE(
      isolate, CreateArrayLiteralImpl(isolate, vector, literals_slot, elements,
                                      ArrayLiteral::kShallowElements));
}

}  // namespace internal
}  // namespace v8
