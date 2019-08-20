// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/ast/ast.h"
#include "src/execution/arguments-inl.h"
#include "src/execution/isolate-inl.h"
#include "src/logging/counters.h"
#include "src/objects/allocation-site-scopes-inl.h"
#include "src/objects/hash-table-inl.h"
#include "src/objects/heap-number-inl.h"
#include "src/objects/heap-object-inl.h"
#include "src/objects/js-regexp-inl.h"
#include "src/objects/literal-objects-inl.h"
#include "src/runtime/runtime-utils.h"
#include "src/runtime/runtime.h"

namespace v8 {
namespace internal {

namespace {

bool IsUninitializedLiteralSite(Object literal_site) {
  return literal_site == Smi::kZero;
}

bool HasBoilerplate(Handle<Object> literal_site) {
  return !literal_site->IsSmi();
}

void PreInitializeLiteralSite(Handle<FeedbackVector> vector,
                              FeedbackSlot slot) {
  vector->Set(slot, Smi::FromInt(1));
}

enum DeepCopyHints { kNoHints = 0, kObjectIsShallow = 1 };

template <class ContextObject>
class JSObjectWalkVisitor {
 public:
  JSObjectWalkVisitor(ContextObject* site_context, DeepCopyHints hints)
      : site_context_(site_context), hints_(hints) {}

  V8_WARN_UNUSED_RESULT MaybeHandle<JSObject> StructureWalk(
      Handle<JSObject> object);

 protected:
  V8_WARN_UNUSED_RESULT inline MaybeHandle<JSObject> VisitElementOrProperty(
      Handle<JSObject> object, Handle<JSObject> value) {
    // Dont create allocation sites for nested object literals
    if (!value->IsJSArray()) {
      return StructureWalk(value);
    }

    Handle<AllocationSite> current_site = site_context()->EnterNewScope();
    MaybeHandle<JSObject> copy_of_value = StructureWalk(value);
    site_context()->ExitScope(current_site, value);
    return copy_of_value;
  }

  inline ContextObject* site_context() { return site_context_; }
  inline Isolate* isolate() { return site_context()->isolate(); }

 private:
  ContextObject* site_context_;
  const DeepCopyHints hints_;
};

template <class ContextObject>
MaybeHandle<JSObject> JSObjectWalkVisitor<ContextObject>::StructureWalk(
    Handle<JSObject> object) {
  Isolate* isolate = this->isolate();
  bool copying = ContextObject::kCopying;
  bool shallow = hints_ == kObjectIsShallow;

  if (!shallow) {
    StackLimitCheck check(isolate);

    if (check.HasOverflowed()) {
      isolate->StackOverflow();
      return MaybeHandle<JSObject>();
    }
  }

  if (object->map(isolate).is_deprecated()) {
    JSObject::MigrateInstance(isolate, object);
  }

  Handle<JSObject> copy;
  if (copying) {
    // JSFunction objects are not allowed to be in normal boilerplates at all.
    DCHECK(!object->IsJSFunction(isolate));
    Handle<AllocationSite> site_to_pass;
    if (site_context()->ShouldCreateMemento(object)) {
      site_to_pass = site_context()->current();
    }
    copy = isolate->factory()->CopyJSObjectWithAllocationSite(object,
                                                              site_to_pass);
  } else {
    copy = object;
  }

  DCHECK(copying || copy.is_identical_to(object));

  if (shallow) return copy;

  HandleScope scope(isolate);

  // Deep copy own properties. Arrays only have 1 property "length".
  if (!copy->IsJSArray(isolate)) {
    if (copy->HasFastProperties(isolate)) {
      Handle<DescriptorArray> descriptors(
          copy->map(isolate).instance_descriptors(isolate), isolate);
      int limit = copy->map(isolate).NumberOfOwnDescriptors();
      for (int i = 0; i < limit; i++) {
        DCHECK_EQ(kField, descriptors->GetDetails(i).location());
        DCHECK_EQ(kData, descriptors->GetDetails(i).kind());
        FieldIndex index = FieldIndex::ForDescriptor(copy->map(isolate), i);
        if (copy->IsUnboxedDoubleField(isolate, index)) continue;
        Object raw = copy->RawFastPropertyAt(isolate, index);
        if (raw.IsJSObject(isolate)) {
          Handle<JSObject> value(JSObject::cast(raw), isolate);
          ASSIGN_RETURN_ON_EXCEPTION(
              isolate, value, VisitElementOrProperty(copy, value), JSObject);
          if (copying) copy->FastPropertyAtPut(index, *value);
        } else if (copying && raw.IsMutableHeapNumber(isolate)) {
          DCHECK(descriptors->GetDetails(i).representation().IsDouble());
          uint64_t double_value = MutableHeapNumber::cast(raw).value_as_bits();
          auto value =
              isolate->factory()->NewMutableHeapNumberFromBits(double_value);
          copy->FastPropertyAtPut(index, *value);
        }
      }
    } else {
      Handle<NameDictionary> dict(copy->property_dictionary(isolate), isolate);
      int capacity = dict->Capacity();
      for (int i = 0; i < capacity; i++) {
        Object raw = dict->ValueAt(isolate, i);
        if (!raw.IsJSObject(isolate)) continue;
        DCHECK(dict->KeyAt(isolate, i).IsName());
        Handle<JSObject> value(JSObject::cast(raw), isolate);
        ASSIGN_RETURN_ON_EXCEPTION(
            isolate, value, VisitElementOrProperty(copy, value), JSObject);
        if (copying) dict->ValueAtPut(i, *value);
      }
    }

    // Assume non-arrays don't end up having elements.
    if (copy->elements(isolate).length() == 0) return copy;
  }

  // Deep copy own elements.
  switch (copy->GetElementsKind(isolate)) {
    case PACKED_ELEMENTS:
    case PACKED_FROZEN_ELEMENTS:
    case PACKED_SEALED_ELEMENTS:
    case HOLEY_FROZEN_ELEMENTS:
    case HOLEY_SEALED_ELEMENTS:
    case HOLEY_ELEMENTS: {
      Handle<FixedArray> elements(FixedArray::cast(copy->elements(isolate)),
                                  isolate);
      if (elements->map(isolate) ==
          ReadOnlyRoots(isolate).fixed_cow_array_map()) {
#ifdef DEBUG
        for (int i = 0; i < elements->length(); i++) {
          DCHECK(!elements->get(i).IsJSObject());
        }
#endif
      } else {
        for (int i = 0; i < elements->length(); i++) {
          Object raw = elements->get(isolate, i);
          if (!raw.IsJSObject(isolate)) continue;
          Handle<JSObject> value(JSObject::cast(raw), isolate);
          ASSIGN_RETURN_ON_EXCEPTION(
              isolate, value, VisitElementOrProperty(copy, value), JSObject);
          if (copying) elements->set(i, *value);
        }
      }
      break;
    }
    case DICTIONARY_ELEMENTS: {
      Handle<NumberDictionary> element_dictionary(
          copy->element_dictionary(isolate), isolate);
      int capacity = element_dictionary->Capacity();
      for (int i = 0; i < capacity; i++) {
        Object raw = element_dictionary->ValueAt(isolate, i);
        if (!raw.IsJSObject(isolate)) continue;
        Handle<JSObject> value(JSObject::cast(raw), isolate);
        ASSIGN_RETURN_ON_EXCEPTION(
            isolate, value, VisitElementOrProperty(copy, value), JSObject);
        if (copying) element_dictionary->ValueAtPut(i, *value);
      }
      break;
    }
    case FAST_SLOPPY_ARGUMENTS_ELEMENTS:
    case SLOW_SLOPPY_ARGUMENTS_ELEMENTS:
      UNIMPLEMENTED();
      break;
    case FAST_STRING_WRAPPER_ELEMENTS:
    case SLOW_STRING_WRAPPER_ELEMENTS:
      UNREACHABLE();

#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype) case TYPE##_ELEMENTS:

      TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE
      // Typed elements cannot be created using an object literal.
      UNREACHABLE();

    case PACKED_SMI_ELEMENTS:
    case HOLEY_SMI_ELEMENTS:
    case PACKED_DOUBLE_ELEMENTS:
    case HOLEY_DOUBLE_ELEMENTS:
    case NO_ELEMENTS:
      // No contained objects, nothing to do.
      break;
  }

  return copy;
}

class DeprecationUpdateContext {
 public:
  explicit DeprecationUpdateContext(Isolate* isolate) { isolate_ = isolate; }
  Isolate* isolate() { return isolate_; }
  bool ShouldCreateMemento(Handle<JSObject> object) { return false; }
  inline void ExitScope(Handle<AllocationSite> scope_site,
                        Handle<JSObject> object) {}
  Handle<AllocationSite> EnterNewScope() { return Handle<AllocationSite>(); }
  Handle<AllocationSite> current() {
    UNREACHABLE();
    return Handle<AllocationSite>();
  }

  static const bool kCopying = false;

 private:
  Isolate* isolate_;
};

// AllocationSiteCreationContext aids in the creation of AllocationSites to
// accompany object literals.
class AllocationSiteCreationContext : public AllocationSiteContext {
 public:
  explicit AllocationSiteCreationContext(Isolate* isolate)
      : AllocationSiteContext(isolate) {}

  Handle<AllocationSite> EnterNewScope() {
    Handle<AllocationSite> scope_site;
    if (top().is_null()) {
      // We are creating the top level AllocationSite as opposed to a nested
      // AllocationSite.
      InitializeTraversal(isolate()->factory()->NewAllocationSite(true));
      scope_site = Handle<AllocationSite>(*top(), isolate());
      if (FLAG_trace_creation_allocation_sites) {
        PrintF("*** Creating top level %s AllocationSite %p\n", "Fat",
               reinterpret_cast<void*>(scope_site->ptr()));
      }
    } else {
      DCHECK(!current().is_null());
      scope_site = isolate()->factory()->NewAllocationSite(false);
      if (FLAG_trace_creation_allocation_sites) {
        PrintF(
            "*** Creating nested %s AllocationSite (top, current, new) (%p, "
            "%p, "
            "%p)\n",
            "Slim", reinterpret_cast<void*>(top()->ptr()),
            reinterpret_cast<void*>(current()->ptr()),
            reinterpret_cast<void*>(scope_site->ptr()));
      }
      current()->set_nested_site(*scope_site);
      update_current_site(*scope_site);
    }
    DCHECK(!scope_site.is_null());
    return scope_site;
  }
  void ExitScope(Handle<AllocationSite> scope_site, Handle<JSObject> object) {
    if (object.is_null()) return;
    scope_site->set_boilerplate(*object);
    if (FLAG_trace_creation_allocation_sites) {
      bool top_level =
          !scope_site.is_null() && top().is_identical_to(scope_site);
      if (top_level) {
        PrintF("*** Setting AllocationSite %p transition_info %p\n",
               reinterpret_cast<void*>(scope_site->ptr()),
               reinterpret_cast<void*>(object->ptr()));
      } else {
        PrintF("*** Setting AllocationSite (%p, %p) transition_info %p\n",
               reinterpret_cast<void*>(top()->ptr()),
               reinterpret_cast<void*>(scope_site->ptr()),
               reinterpret_cast<void*>(object->ptr()));
      }
    }
  }
  static const bool kCopying = false;
};

MaybeHandle<JSObject> DeepWalk(Handle<JSObject> object,
                               DeprecationUpdateContext* site_context) {
  JSObjectWalkVisitor<DeprecationUpdateContext> v(site_context, kNoHints);
  MaybeHandle<JSObject> result = v.StructureWalk(object);
  Handle<JSObject> for_assert;
  DCHECK(!result.ToHandle(&for_assert) || for_assert.is_identical_to(object));
  return result;
}

MaybeHandle<JSObject> DeepWalk(Handle<JSObject> object,
                               AllocationSiteCreationContext* site_context) {
  JSObjectWalkVisitor<AllocationSiteCreationContext> v(site_context, kNoHints);
  MaybeHandle<JSObject> result = v.StructureWalk(object);
  Handle<JSObject> for_assert;
  DCHECK(!result.ToHandle(&for_assert) || for_assert.is_identical_to(object));
  return result;
}

MaybeHandle<JSObject> DeepCopy(Handle<JSObject> object,
                               AllocationSiteUsageContext* site_context,
                               DeepCopyHints hints) {
  JSObjectWalkVisitor<AllocationSiteUsageContext> v(site_context, hints);
  MaybeHandle<JSObject> copy = v.StructureWalk(object);
  Handle<JSObject> for_assert;
  DCHECK(!copy.ToHandle(&for_assert) || !for_assert.is_identical_to(object));
  return copy;
}

Handle<JSObject> CreateObjectLiteral(
    Isolate* isolate,
    Handle<ObjectBoilerplateDescription> object_boilerplate_description,
    int flags, AllocationType allocation);

Handle<JSObject> CreateArrayLiteral(
    Isolate* isolate,
    Handle<ArrayBoilerplateDescription> array_boilerplate_description,
    AllocationType allocation);

struct ObjectLiteralHelper {
  static inline Handle<JSObject> Create(Isolate* isolate,
                                        Handle<HeapObject> description,
                                        int flags, AllocationType allocation) {
    Handle<ObjectBoilerplateDescription> object_boilerplate_description =
        Handle<ObjectBoilerplateDescription>::cast(description);
    return CreateObjectLiteral(isolate, object_boilerplate_description, flags,
                               allocation);
  }
};

struct ArrayLiteralHelper {
  static inline Handle<JSObject> Create(Isolate* isolate,
                                        Handle<HeapObject> description,
                                        int flags_not_used,
                                        AllocationType allocation) {
    Handle<ArrayBoilerplateDescription> array_boilerplate_description =
        Handle<ArrayBoilerplateDescription>::cast(description);
    return CreateArrayLiteral(isolate, array_boilerplate_description,
                              allocation);
  }
};

Handle<JSObject> CreateObjectLiteral(
    Isolate* isolate,
    Handle<ObjectBoilerplateDescription> object_boilerplate_description,
    int flags, AllocationType allocation) {
  Handle<NativeContext> native_context = isolate->native_context();
  bool use_fast_elements = (flags & ObjectLiteral::kFastElements) != 0;
  bool has_null_prototype = (flags & ObjectLiteral::kHasNullPrototype) != 0;

  // In case we have function literals, we want the object to be in
  // slow properties mode for now. We don't go in the map cache because
  // maps with constant functions can't be shared if the functions are
  // not the same (which is the common case).
  int number_of_properties =
      object_boilerplate_description->backing_store_size();

  // Ignoring number_of_properties for force dictionary map with
  // __proto__:null.
  Handle<Map> map =
      has_null_prototype
          ? handle(native_context->slow_object_with_null_prototype_map(),
                   isolate)
          : isolate->factory()->ObjectLiteralMapFromCache(native_context,
                                                          number_of_properties);

  Handle<JSObject> boilerplate =
      isolate->factory()->NewFastOrSlowJSObjectFromMap(
          map, number_of_properties, allocation);

  // Normalize the elements of the boilerplate to save space if needed.
  if (!use_fast_elements) JSObject::NormalizeElements(boilerplate);

  // Add the constant properties to the boilerplate.
  int length = object_boilerplate_description->size();
  // TODO(verwaest): Support tracking representations in the boilerplate.
  for (int index = 0; index < length; index++) {
    Handle<Object> key(object_boilerplate_description->name(isolate, index),
                       isolate);
    Handle<Object> value(object_boilerplate_description->value(isolate, index),
                         isolate);

    if (value->IsHeapObject()) {
      if (HeapObject::cast(*value).IsArrayBoilerplateDescription(isolate)) {
        Handle<ArrayBoilerplateDescription> boilerplate =
            Handle<ArrayBoilerplateDescription>::cast(value);
        value = CreateArrayLiteral(isolate, boilerplate, allocation);

      } else if (HeapObject::cast(*value).IsObjectBoilerplateDescription(
                     isolate)) {
        Handle<ObjectBoilerplateDescription> boilerplate =
            Handle<ObjectBoilerplateDescription>::cast(value);
        value = CreateObjectLiteral(isolate, boilerplate, boilerplate->flags(),
                                    allocation);
      }
    }

    uint32_t element_index = 0;
    if (key->ToArrayIndex(&element_index)) {
      // Array index (uint32).
      if (value->IsUninitialized(isolate)) {
        value = handle(Smi::kZero, isolate);
      }
      JSObject::SetOwnElementIgnoreAttributes(boilerplate, element_index, value,
                                              NONE)
          .Check();
    } else {
      Handle<String> name = Handle<String>::cast(key);
      DCHECK(!name->AsArrayIndex(&element_index));
      JSObject::SetOwnPropertyIgnoreAttributes(boilerplate, name, value, NONE)
          .Check();
    }
  }

  if (map->is_dictionary_map() && !has_null_prototype) {
    // TODO(cbruni): avoid making the boilerplate fast again, the clone stub
    // supports dict-mode objects directly.
    JSObject::MigrateSlowToFast(
        boilerplate, boilerplate->map().UnusedPropertyFields(), "FastLiteral");
  }
  return boilerplate;
}

Handle<JSObject> CreateArrayLiteral(
    Isolate* isolate,
    Handle<ArrayBoilerplateDescription> array_boilerplate_description,
    AllocationType allocation) {
  ElementsKind constant_elements_kind =
      array_boilerplate_description->elements_kind();

  Handle<FixedArrayBase> constant_elements_values(
      array_boilerplate_description->constant_elements(isolate), isolate);

  // Create the JSArray.
  Handle<FixedArrayBase> copied_elements_values;
  if (IsDoubleElementsKind(constant_elements_kind)) {
    copied_elements_values = isolate->factory()->CopyFixedDoubleArray(
        Handle<FixedDoubleArray>::cast(constant_elements_values));
  } else {
    DCHECK(IsSmiOrObjectElementsKind(constant_elements_kind));
    const bool is_cow = (constant_elements_values->map(isolate) ==
                         ReadOnlyRoots(isolate).fixed_cow_array_map());
    if (is_cow) {
      copied_elements_values = constant_elements_values;
      if (DEBUG_BOOL) {
        Handle<FixedArray> fixed_array_values =
            Handle<FixedArray>::cast(copied_elements_values);
        for (int i = 0; i < fixed_array_values->length(); i++) {
          DCHECK(!fixed_array_values->get(i).IsFixedArray());
        }
      }
    } else {
      Handle<FixedArray> fixed_array_values =
          Handle<FixedArray>::cast(constant_elements_values);
      Handle<FixedArray> fixed_array_values_copy =
          isolate->factory()->CopyFixedArray(fixed_array_values);
      copied_elements_values = fixed_array_values_copy;
      for (int i = 0; i < fixed_array_values->length(); i++) {
        Object value = fixed_array_values_copy->get(isolate, i);
        HeapObject value_heap_object;
        if (value.GetHeapObject(isolate, &value_heap_object)) {
          if (value_heap_object.IsArrayBoilerplateDescription(isolate)) {
            HandleScope sub_scope(isolate);
            Handle<ArrayBoilerplateDescription> boilerplate(
                ArrayBoilerplateDescription::cast(value_heap_object), isolate);
            Handle<JSObject> result =
                CreateArrayLiteral(isolate, boilerplate, allocation);
            fixed_array_values_copy->set(i, *result);

          } else if (value_heap_object.IsObjectBoilerplateDescription(
                         isolate)) {
            HandleScope sub_scope(isolate);
            Handle<ObjectBoilerplateDescription> boilerplate(
                ObjectBoilerplateDescription::cast(value_heap_object), isolate);
            Handle<JSObject> result = CreateObjectLiteral(
                isolate, boilerplate, boilerplate->flags(), allocation);
            fixed_array_values_copy->set(i, *result);
          }
        }
      }
    }
  }
  return isolate->factory()->NewJSArrayWithElements(
      copied_elements_values, constant_elements_kind,
      copied_elements_values->length(), allocation);
}

inline DeepCopyHints DecodeCopyHints(int flags) {
  DeepCopyHints copy_hints =
      (flags & AggregateLiteral::kIsShallow) ? kObjectIsShallow : kNoHints;
  if (FLAG_track_double_fields && !FLAG_unbox_double_fields) {
    // Make sure we properly clone mutable heap numbers on 32-bit platforms.
    copy_hints = kNoHints;
  }
  return copy_hints;
}

template <typename LiteralHelper>
MaybeHandle<JSObject> CreateLiteralWithoutAllocationSite(
    Isolate* isolate, Handle<HeapObject> description, int flags) {
  Handle<JSObject> literal = LiteralHelper::Create(isolate, description, flags,
                                                   AllocationType::kYoung);
  DeepCopyHints copy_hints = DecodeCopyHints(flags);
  if (copy_hints == kNoHints) {
    DeprecationUpdateContext update_context(isolate);
    RETURN_ON_EXCEPTION(isolate, DeepWalk(literal, &update_context), JSObject);
  }
  return literal;
}

template <typename LiteralHelper>
MaybeHandle<JSObject> CreateLiteral(Isolate* isolate,
                                    MaybeHandle<FeedbackVector> maybe_vector,
                                    int literals_index,
                                    Handle<HeapObject> description, int flags) {
  if (maybe_vector.is_null()) {
    return CreateLiteralWithoutAllocationSite<LiteralHelper>(
        isolate, description, flags);
  }

  Handle<FeedbackVector> vector = maybe_vector.ToHandleChecked();
  FeedbackSlot literals_slot(FeedbackVector::ToSlot(literals_index));
  CHECK(literals_slot.ToInt() < vector->length());
  Handle<Object> literal_site(vector->Get(literals_slot)->cast<Object>(),
                              isolate);
  DeepCopyHints copy_hints = DecodeCopyHints(flags);

  Handle<AllocationSite> site;
  Handle<JSObject> boilerplate;

  if (HasBoilerplate(literal_site)) {
    site = Handle<AllocationSite>::cast(literal_site);
    boilerplate = Handle<JSObject>(site->boilerplate(), isolate);
  } else {
    // Eagerly create AllocationSites for literals that contain an Array.
    bool needs_initial_allocation_site =
        (flags & AggregateLiteral::kNeedsInitialAllocationSite) != 0;
    if (!needs_initial_allocation_site &&
        IsUninitializedLiteralSite(*literal_site)) {
      PreInitializeLiteralSite(vector, literals_slot);
      return CreateLiteralWithoutAllocationSite<LiteralHelper>(
          isolate, description, flags);
    } else {
      boilerplate = LiteralHelper::Create(isolate, description, flags,
                                          AllocationType::kOld);
    }
    // Install AllocationSite objects.
    AllocationSiteCreationContext creation_context(isolate);
    site = creation_context.EnterNewScope();
    RETURN_ON_EXCEPTION(isolate, DeepWalk(boilerplate, &creation_context),
                        JSObject);
    creation_context.ExitScope(site, boilerplate);

    vector->Set(literals_slot, *site);
  }

  STATIC_ASSERT(static_cast<int>(ObjectLiteral::kDisableMementos) ==
                static_cast<int>(ArrayLiteral::kDisableMementos));
  bool enable_mementos = (flags & ObjectLiteral::kDisableMementos) == 0;

  // Copy the existing boilerplate.
  AllocationSiteUsageContext usage_context(isolate, site, enable_mementos);
  usage_context.EnterNewScope();
  MaybeHandle<JSObject> copy =
      DeepCopy(boilerplate, &usage_context, copy_hints);
  usage_context.ExitScope(site, boilerplate);
  return copy;
}

}  // namespace

RUNTIME_FUNCTION(Runtime_CreateObjectLiteral) {
  HandleScope scope(isolate);
  DCHECK_EQ(4, args.length());
  CONVERT_ARG_HANDLE_CHECKED(HeapObject, maybe_vector, 0);
  CONVERT_SMI_ARG_CHECKED(literals_index, 1);
  CONVERT_ARG_HANDLE_CHECKED(ObjectBoilerplateDescription, description, 2);
  CONVERT_SMI_ARG_CHECKED(flags, 3);
  Handle<FeedbackVector> vector = Handle<FeedbackVector>();
  if (!maybe_vector->IsUndefined()) {
    DCHECK(maybe_vector->IsFeedbackVector());
    vector = Handle<FeedbackVector>::cast(maybe_vector);
  }
  RETURN_RESULT_OR_FAILURE(
      isolate, CreateLiteral<ObjectLiteralHelper>(
                   isolate, vector, literals_index, description, flags));
}

RUNTIME_FUNCTION(Runtime_CreateObjectLiteralWithoutAllocationSite) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(ObjectBoilerplateDescription, description, 0);
  CONVERT_SMI_ARG_CHECKED(flags, 1);
  RETURN_RESULT_OR_FAILURE(
      isolate, CreateLiteralWithoutAllocationSite<ObjectLiteralHelper>(
                   isolate, description, flags));
}

RUNTIME_FUNCTION(Runtime_CreateArrayLiteralWithoutAllocationSite) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(ArrayBoilerplateDescription, description, 0);
  CONVERT_SMI_ARG_CHECKED(flags, 1);
  RETURN_RESULT_OR_FAILURE(
      isolate, CreateLiteralWithoutAllocationSite<ArrayLiteralHelper>(
                   isolate, description, flags));
}

RUNTIME_FUNCTION(Runtime_CreateArrayLiteral) {
  HandleScope scope(isolate);
  DCHECK_EQ(4, args.length());
  CONVERT_ARG_HANDLE_CHECKED(HeapObject, maybe_vector, 0);
  CONVERT_SMI_ARG_CHECKED(literals_index, 1);
  CONVERT_ARG_HANDLE_CHECKED(ArrayBoilerplateDescription, elements, 2);
  CONVERT_SMI_ARG_CHECKED(flags, 3);
  Handle<FeedbackVector> vector = Handle<FeedbackVector>();
  if (!maybe_vector->IsUndefined()) {
    DCHECK(maybe_vector->IsFeedbackVector());
    vector = Handle<FeedbackVector>::cast(maybe_vector);
  }
  RETURN_RESULT_OR_FAILURE(
      isolate, CreateLiteral<ArrayLiteralHelper>(
                   isolate, vector, literals_index, elements, flags));
}

RUNTIME_FUNCTION(Runtime_CreateRegExpLiteral) {
  HandleScope scope(isolate);
  DCHECK_EQ(4, args.length());
  CONVERT_ARG_HANDLE_CHECKED(HeapObject, maybe_vector, 0);
  CONVERT_SMI_ARG_CHECKED(index, 1);
  CONVERT_ARG_HANDLE_CHECKED(String, pattern, 2);
  CONVERT_SMI_ARG_CHECKED(flags, 3);
  FeedbackSlot literal_slot(FeedbackVector::ToSlot(index));
  Handle<FeedbackVector> vector = Handle<FeedbackVector>();
  if (!maybe_vector->IsUndefined()) {
    DCHECK(maybe_vector->IsFeedbackVector());
    vector = Handle<FeedbackVector>::cast(maybe_vector);
  }
  Handle<Object> boilerplate;
  if (vector.is_null()) {
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, boilerplate,
        JSRegExp::New(isolate, pattern, JSRegExp::Flags(flags)));
    return *JSRegExp::Copy(Handle<JSRegExp>::cast(boilerplate));
  }

  // Check if boilerplate exists. If not, create it first.
  Handle<Object> literal_site(vector->Get(literal_slot)->cast<Object>(),
                              isolate);
  if (!HasBoilerplate(literal_site)) {
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, boilerplate,
        JSRegExp::New(isolate, pattern, JSRegExp::Flags(flags)));
    if (IsUninitializedLiteralSite(*literal_site)) {
      PreInitializeLiteralSite(vector, literal_slot);
      return *boilerplate;
    }
    vector->Set(literal_slot, *boilerplate);
  }
  return *JSRegExp::Copy(Handle<JSRegExp>::cast(boilerplate));
}

}  // namespace internal
}  // namespace v8
