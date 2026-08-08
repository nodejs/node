// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/ast/ast.h"
#include "src/base/logging.h"
#include "src/common/globals.h"
#include "src/execution/arguments-inl.h"
#include "src/execution/isolate-inl.h"
#include "src/handles/maybe-handles.h"
#include "src/objects/allocation-site-scopes-inl.h"
#include "src/objects/casting.h"
#include "src/objects/dictionary-inl.h"
#include "src/objects/field-type.h"
#include "src/objects/hash-table-inl.h"
#include "src/objects/heap-number-inl.h"
#include "src/objects/heap-object.h"
#include "src/objects/js-data-object-builder-inl.h"
#include "src/objects/js-regexp-inl.h"
#include "src/objects/literal-objects-inl.h"
#include "src/objects/lookup.h"
#include "src/objects/map-updater.h"
#include "src/objects/objects.h"
#include "src/objects/property-descriptor-object.h"
#include "src/objects/property-descriptor.h"
#include "src/objects/property-details.h"
#include "src/objects/transitions-inl.h"
#include "src/runtime/runtime.h"

namespace v8 {
namespace internal {

namespace {

bool IsUninitializedLiteralSite(Tagged<Object> literal_site) {
  return literal_site == Smi::zero();
}

bool HasBoilerplate(DirectHandle<Object> literal_site) {
  return !IsSmi(*literal_site);
}

void PreInitializeLiteralSite(DirectHandle<FeedbackVector> vector,
                              FeedbackSlot slot) {
  vector->SynchronizedSet(slot, Smi::FromInt(1));
}

template <class ContextObject>
class JSObjectWalkVisitor {
 public:
  explicit JSObjectWalkVisitor(ContextObject* site_context)
      : site_context_(site_context) {}

  V8_WARN_UNUSED_RESULT MaybeHandle<JSObject> StructureWalk(
      Handle<JSObject> object);

 protected:
  V8_WARN_UNUSED_RESULT inline MaybeHandle<JSObject> VisitElementOrProperty(
      DirectHandle<JSObject> object, Handle<JSObject> value) {
    // Dont create allocation sites for nested object literals
    if (!IsJSArray(*value)) {
      return StructureWalk(value);
    }

    DirectHandle<AllocationSite> current_site = site_context()->EnterNewScope();
    MaybeHandle<JSObject> copy_of_value = StructureWalk(value);
    site_context()->ExitScope(current_site, value);
    return copy_of_value;
  }

  inline ContextObject* site_context() { return site_context_; }
  inline Isolate* isolate() { return site_context()->isolate(); }

 private:
  ContextObject* site_context_;
};

template <class ContextObject>
MaybeHandle<JSObject> JSObjectWalkVisitor<ContextObject>::StructureWalk(
    Handle<JSObject> object) {
  Isolate* isolate = this->isolate();
  bool copying = ContextObject::kCopying;

  {
    StackLimitCheck check(isolate);

    if (check.HasOverflowed()) {
      isolate->StackOverflow();
      return MaybeHandle<JSObject>();
    }
  }

  if (object->map(isolate)->is_deprecated()) {
    base::MutexGuard mutex_guard(isolate->boilerplate_migration_access());
    JSObject::MigrateInstance(isolate, object);
  }

  Handle<JSObject> copy;
  if (copying) {
    // JSFunction objects are not allowed to be in normal boilerplates at all.
    DCHECK(!IsJSFunction(*object, isolate));
    DirectHandle<AllocationSite> site_to_pass;
    if (site_context()->ShouldCreateMemento(object)) {
      site_to_pass = site_context()->current();
    }
    copy = isolate->factory()->CopyJSObjectWithAllocationSite(object,
                                                              site_to_pass);
  } else {
    copy = object;
  }

  DCHECK(copying || copy.is_identical_to(object));

  HandleScope scope(isolate);

  // Deep copy own properties. Arrays only have 1 property "length".
  if (!IsJSArray(*copy, isolate)) {
    if (copy->HasFastProperties(isolate)) {
      DirectHandle<DescriptorArray> descriptors(
          copy->map(isolate)->instance_descriptors(isolate), isolate);
      for (InternalIndex i : copy->map(isolate)->IterateOwnDescriptors()) {
        PropertyDetails details = descriptors->GetDetails(i);
        DCHECK_EQ(PropertyLocation::kField, details.location());
        DCHECK_EQ(PropertyKind::kData, details.kind());
        FieldIndex index = FieldIndex::ForDetails(copy->map(isolate), details);

        Tagged<Object> raw = copy->RawFastPropertyAt(isolate, index);
        if (IsJSObject(raw, isolate)) {
          Handle<JSObject> value(Cast<JSObject>(raw), isolate);
          ASSIGN_RETURN_ON_EXCEPTION(isolate, value,
                                     VisitElementOrProperty(copy, value));
          if (copying) copy->FastPropertyAtPut(index, *value);
        } else if (copying && details.representation().IsDouble()) {
          uint64_t double_value = Cast<HeapNumber>(raw)->value_as_bits();
          auto value = isolate->factory()->NewHeapNumberFromBits(double_value);
          copy->FastPropertyAtPut(index, *value);
        }
      }
    } else {
      if (V8_ENABLE_SWISS_NAME_DICTIONARY_BOOL) {
        DirectHandle<SwissNameDictionary> dict(
            copy->property_dictionary_swiss(isolate), isolate);
        for (InternalIndex i : dict->IterateEntries()) {
          Tagged<Object> raw = dict->ValueAt(i);
          if (!IsJSObject(raw, isolate)) continue;
          DCHECK(IsName(dict->KeyAt(i)));
          Handle<JSObject> value(Cast<JSObject>(raw), isolate);
          ASSIGN_RETURN_ON_EXCEPTION(isolate, value,
                                     VisitElementOrProperty(copy, value));
          if (copying) dict->ValueAtPut(i, *value);
        }
      } else {
        DirectHandle<NameDictionary> dict(copy->property_dictionary(isolate),
                                          isolate);
        for (InternalIndex i : dict->IterateEntries()) {
          Tagged<Object> raw = dict->ValueAt(isolate, i);
          if (!IsJSObject(raw, isolate)) continue;
          DCHECK(IsName(dict->KeyAt(isolate, i)));
          Handle<JSObject> value(Cast<JSObject>(raw), isolate);
          ASSIGN_RETURN_ON_EXCEPTION(isolate, value,
                                     VisitElementOrProperty(copy, value));
          if (copying) dict->ValueAtPut(i, *value);
        }
      }
    }

    // Assume non-arrays don't end up having elements.
    if (copy->elements(isolate)->ulength().value() == 0) return copy;
  }

  // Deep copy own elements.
  switch (copy->GetElementsKind(isolate)) {
    case PACKED_ELEMENTS:
    case PACKED_FROZEN_ELEMENTS:
    case PACKED_SEALED_ELEMENTS:
    case PACKED_NONEXTENSIBLE_ELEMENTS:
    case HOLEY_FROZEN_ELEMENTS:
    case HOLEY_SEALED_ELEMENTS:
    case HOLEY_NONEXTENSIBLE_ELEMENTS:
    case HOLEY_ELEMENTS:
    case SHARED_ARRAY_ELEMENTS: {
      DirectHandle<FixedArray> elements(
          Cast<FixedArray>(copy->elements(isolate)), isolate);
      uint32_t elements_len = elements->ulength().value();
      if (elements->map() == ReadOnlyRoots(isolate).fixed_cow_array_map()) {
#ifdef DEBUG
        for (uint32_t i = 0; i < elements_len; i++) {
          DCHECK(!IsJSObject(elements->get(i)));
        }
#endif
      } else {
        for (uint32_t i = 0; i < elements_len; i++) {
          Tagged<Object> raw = elements->get(i);
          if (!IsJSObject(raw, isolate)) continue;
          Handle<JSObject> value(Cast<JSObject>(raw), isolate);
          ASSIGN_RETURN_ON_EXCEPTION(isolate, value,
                                     VisitElementOrProperty(copy, value));
          if (copying) elements->set(i, *value);
        }
      }
      break;
    }
    case DICTIONARY_ELEMENTS: {
      DirectHandle<NumberDictionary> element_dictionary(
          copy->element_dictionary(isolate), isolate);
      for (InternalIndex i : element_dictionary->IterateEntries()) {
        Tagged<Object> raw = element_dictionary->ValueAt(isolate, i);
        if (!IsJSObject(raw, isolate)) continue;
        Handle<JSObject> value(Cast<JSObject>(raw), isolate);
        ASSIGN_RETURN_ON_EXCEPTION(isolate, value,
                                   VisitElementOrProperty(copy, value));
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
    case WASM_ARRAY_ELEMENTS:
      UNREACHABLE();

#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype) case TYPE##_ELEMENTS:

      TYPED_ARRAYS(TYPED_ARRAY_CASE)
      RAB_GSAB_TYPED_ARRAYS(TYPED_ARRAY_CASE)
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
      if (v8_flags.trace_creation_allocation_sites) {
        PrintF("*** Creating top level %s AllocationSite %p\n", "Fat",
               reinterpret_cast<void*>(scope_site->ptr()));
      }
    } else {
      DCHECK(!current().is_null());
      scope_site = isolate()->factory()->NewAllocationSite(false);
      if (v8_flags.trace_creation_allocation_sites) {
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
  void ExitScope(DirectHandle<AllocationSite> scope_site,
                 DirectHandle<JSObject> object) {
    if (object.is_null()) return;
    scope_site->set_boilerplate(*object, kReleaseStore);
    if (v8_flags.trace_creation_allocation_sites) {
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

MaybeDirectHandle<JSObject> DeepWalk(
    Handle<JSObject> object, AllocationSiteCreationContext* site_context) {
  JSObjectWalkVisitor<AllocationSiteCreationContext> v(site_context);
  MaybeDirectHandle<JSObject> result = v.StructureWalk(object);
  DirectHandle<JSObject> for_assert;
  DCHECK(!result.ToHandle(&for_assert) || for_assert.is_identical_to(object));
  return result;
}

MaybeDirectHandle<JSObject> DeepCopy(Handle<JSObject> object,
                                     AllocationSiteUsageContext* site_context) {
  JSObjectWalkVisitor<AllocationSiteUsageContext> v(site_context);
  MaybeDirectHandle<JSObject> copy = v.StructureWalk(object);
  DirectHandle<JSObject> for_assert;
  DCHECK(!copy.ToHandle(&for_assert) || !for_assert.is_identical_to(object));
  return copy;
}

Handle<JSObject> CreateObjectLiteral(
    Isolate* isolate,
    DirectHandle<ObjectBoilerplateDescription> object_boilerplate_description,
    int flags, AllocationType allocation);

Handle<JSObject> CreateArrayLiteral(
    Isolate* isolate,
    DirectHandle<ArrayBoilerplateDescription> array_boilerplate_description,
    AllocationType allocation);

struct ObjectLiteralHelper {
  static inline Handle<JSObject> Create(Isolate* isolate,
                                        Handle<HeapObject> description,
                                        int flags, AllocationType allocation) {
    auto object_boilerplate_description =
        Cast<ObjectBoilerplateDescription>(description);
    return CreateObjectLiteral(isolate, object_boilerplate_description, flags,
                               allocation);
  }
};

struct ArrayLiteralHelper {
  static inline Handle<JSObject> Create(Isolate* isolate,
                                        Handle<HeapObject> description,
                                        int flags_not_used,
                                        AllocationType allocation) {
    auto array_boilerplate_description =
        Cast<ArrayBoilerplateDescription>(description);
    return CreateArrayLiteral(isolate, array_boilerplate_description,
                              allocation);
  }
};

Handle<JSObject> CreateObjectLiteralWithNullProto(
    Isolate* isolate,
    DirectHandle<ObjectBoilerplateDescription> object_boilerplate_description,
    int flags, AllocationType allocation) {
  DirectHandle<NativeContext> native_context = isolate->native_context();
  bool use_fast_elements = (flags & ObjectLiteral::kFastElements) != 0;
  int number_of_properties =
      object_boilerplate_description->backing_store_size();

  // Ignoring number_of_properties for force dictionary map with
  // __proto__:null.
  DirectHandle<Map> map = direct_handle(
      native_context->slow_object_with_null_prototype_map(), isolate);
  Handle<JSObject> boilerplate =
      isolate->factory()->NewFastOrSlowJSObjectFromMap(
          map, number_of_properties, allocation);

  // Normalize the elements of the boilerplate to save space if needed.
  if (!use_fast_elements) JSObject::NormalizeElements(isolate, boilerplate);

  // TODO(leszeks): This path could be faster, e.g. we could populate the
  // property dictionary directly.
  int length = object_boilerplate_description->boilerplate_properties_count();
  for (int index = 0; index < length; index++) {
    DirectHandle<ObjectBoilerplateDescription::KeyT> key(
        object_boilerplate_description->name(index), isolate);
    DirectHandle<Object> value(object_boilerplate_description->value(index),
                               isolate);
    if (DirectHandle<HeapObject> ho_value; TryCast(value, &ho_value)) {
      if (DirectHandle<ArrayBoilerplateDescription> array_desc;
          TryCast(ho_value, &array_desc)) {
        value = CreateArrayLiteral(isolate, array_desc, allocation);
      } else if (DirectHandle<ObjectBoilerplateDescription> obj_desc;
                 TryCast(ho_value, &obj_desc)) {
        value = CreateObjectLiteral(isolate, obj_desc, obj_desc->flags(),
                                    allocation);
      }
    }

    uint32_t element_index = 0;
    if (Object::ToArrayIndex(*key, &element_index)) {
      // Array index (uint32).
      JSObject::SetOwnElementIgnoreAttributes(boilerplate, element_index, value,
                                              NONE)
          .Check();
    } else {
      DirectHandle<String> name = Cast<InternalizedString>(key);
      DCHECK(!name->AsArrayIndex(&element_index));
      JSObject::SetOwnPropertyIgnoreAttributes(boilerplate, name, value, NONE)
          .Check();
    }
  }
  return boilerplate;
}

// Helper class for iterating an ObjectBoilerplateDescription for use with
// JSDataObjectBuilder.
class ObjectBoilerplateDescriptionIterator {
 public:
  static constexpr bool kSupportsRawKeys = false;
  static constexpr bool kMayHaveDuplicateKeys = false;

  ObjectBoilerplateDescriptionIterator(
      DirectHandle<ObjectBoilerplateDescription> object_boilerplate_description,
      Isolate* isolate, AllocationType allocation)
      : object_boilerplate_description_(object_boilerplate_description),
        isolate_(isolate),
        allocation_(allocation) {
    for (; i_ < length_; ++i_) {
      if (IsInternalizedString(object_boilerplate_description->name(i_))) {
        break;
      }
    }
    start_ = i_;
  }

  Handle<InternalizedString> GetKey() {
    return handle(
        Cast<InternalizedString>(object_boilerplate_description_->name(i_)),
        isolate_);
  }

  Handle<Object> GetValue(bool will_revisit) {
    Handle<Object> value(object_boilerplate_description_->value(i_), isolate_);
    if (Handle<HeapObject> ho_value; TryCast(value, &ho_value)) {
      if (Handle<ArrayBoilerplateDescription> array_desc;
          TryCast(ho_value, &array_desc)) {
        value = CreateArrayLiteral(isolate_, array_desc, allocation_);
        if (will_revisit) {
          materialized_values_.push_back(value);
        }
      } else if (Handle<ObjectBoilerplateDescription> obj_desc;
                 TryCast(ho_value, &obj_desc)) {
        value = CreateObjectLiteral(isolate_, obj_desc, obj_desc->flags(),
                                    allocation_);
        if (will_revisit) {
          materialized_values_.push_back(value);
        }
      }
    }
    return value;
  }

  void Advance() {
    if (i_ >= length_) return;
    for (++i_; i_ < length_; ++i_) {
      if (IsInternalizedString(object_boilerplate_description_->name(i_))) {
        break;
      }
    }
  }

  bool Done() { return i_ >= length_; }

  // Helper class for revisiting values already iterated by this iterator, in
  // particular avoiding re-materialising nested values.
  struct RevisitValueIterator {
    DirectHandle<ObjectBoilerplateDescription> object_boilerplate_description;
    base::SmallVector<Handle<Object>, 4>::iterator materialized_values_it;
    Isolate* isolate;
    int i;
    int length = object_boilerplate_description->boilerplate_properties_count();

    Tagged<Object> GetNext() {
      DCHECK(IsInternalizedString(object_boilerplate_description->name(i)));

      Tagged<Object> value = object_boilerplate_description->value(i);

      // Potentially re-use an already materialized value.
      if (IsArrayBoilerplateDescription(value) ||
          IsObjectBoilerplateDescription(value)) {
        value = **materialized_values_it++;
      }

      // Advance to the next string name.
      for (++i; i < length; ++i) {
        if (IsInternalizedString(object_boilerplate_description->name(i))) {
          break;
        }
      }
      return value;
    }
  };

  RevisitValueIterator RevisitValues() {
    return RevisitValueIterator{object_boilerplate_description_,
                                materialized_values_.begin(), isolate_, start_};
  }

 private:
  DirectHandle<ObjectBoilerplateDescription> object_boilerplate_description_;
  Isolate* isolate_;
  AllocationType allocation_;
  int i_ = 0;
  int start_ = 0;
  int length_ = object_boilerplate_description_->boilerplate_properties_count();
  base::SmallVector<Handle<Object>, 4> materialized_values_ = {};
};

Handle<JSObject> CreateObjectLiteral(
    Isolate* isolate,
    DirectHandle<ObjectBoilerplateDescription> object_boilerplate_description,
    int flags, AllocationType allocation) {
  DirectHandle<NativeContext> native_context = isolate->native_context();
  bool has_null_prototype = (flags & ObjectLiteral::kHasNullPrototype) != 0;

  if (has_null_prototype) {
    return CreateObjectLiteralWithNullProto(
        isolate, object_boilerplate_description, flags, allocation);
  }
  // Fast path using manual elements initialisation and JSDataObjectBuilder.

  bool use_fast_elements = (flags & ObjectLiteral::kFastElements) != 0;
  int number_of_properties =
      object_boilerplate_description->backing_store_size();

  int length = object_boilerplate_description->boilerplate_properties_count();

  // First iterate to count the elements and find the max element index.
  // TODO(leszeks): We could already at parse-time figure out if there are any
  // elements and avoid this step.
  int element_count = 0;
  uint32_t max_element_index = 0;
  for (int i = 0; i < length; i++) {
    uint32_t element_index = 0;
    if (Object::ToArrayIndex(object_boilerplate_description->name(i),
                             &element_index)) {
      element_count++;
      if (element_index > max_element_index) max_element_index = element_index;
    }
  }

  // Then allocate the appropriate element container.
  DirectHandle<FixedArray> elements;
  ElementsKind elements_kind;
  if (use_fast_elements) {
    elements_kind = HOLEY_ELEMENTS;
    if (element_count > 0) {
      elements =
          isolate->factory()->NewFixedArrayWithHoles(max_element_index + 1);
    } else {
      elements = isolate->factory()->empty_fixed_array();
    }
  } else {
    elements_kind = DICTIONARY_ELEMENTS;
    elements = NumberDictionary::New(isolate, element_count);
  }

  // Fill the element container with values.
  for (int i = 0; i < length; i++) {
    uint32_t element_index = 0;
    if (!Object::ToArrayIndex(object_boilerplate_description->name(i),
                              &element_index))
      continue;

    DirectHandle<Object> value =
        direct_handle(object_boilerplate_description->value(i), isolate);

    if (DirectHandle<HeapObject> ho_value; TryCast(value, &ho_value)) {
      if (DirectHandle<ArrayBoilerplateDescription> array_desc;
          TryCast(ho_value, &array_desc)) {
        value = CreateArrayLiteral(isolate, array_desc, allocation);
      } else if (DirectHandle<ObjectBoilerplateDescription> obj_desc;
                 TryCast(ho_value, &obj_desc)) {
        value = CreateObjectLiteral(isolate, obj_desc, obj_desc->flags(),
                                    allocation);
      }
    }

    if (use_fast_elements) {
      elements->set(element_index, *value);
    } else {
      DirectHandle<NumberDictionary> dict = Cast<NumberDictionary>(elements);
      PropertyDetails details(PropertyKind::kData, NONE,
                              PropertyConstness::kConst, 0);
      NumberDictionary::UncheckedAdd(isolate, dict, element_index, value,
                                     details);
    }
  }

  if (!use_fast_elements) {
    DirectHandle<NumberDictionary> dict = Cast<NumberDictionary>(elements);
    dict->SetInitialNumberOfElements(element_count);
    dict->UpdateMaxNumberKey(max_element_index, Handle<JSObject>::null());
  }

  // Finally, use JSDataObjectBuilder to build the object itself.
  JSDataObjectBuilder builder(isolate, elements_kind, number_of_properties,
                              DirectHandle<Map>(),
                              JSDataObjectBuilder::kNormalHeapNumbers);

  return builder.BuildFromIterator(
      ObjectBoilerplateDescriptionIterator{object_boilerplate_description,
                                           isolate, allocation},
      elements);
}

Handle<JSObject> CreateArrayLiteral(
    Isolate* isolate,
    DirectHandle<ArrayBoilerplateDescription> array_boilerplate_description,
    AllocationType allocation) {
  ElementsKind constant_elements_kind =
      array_boilerplate_description->elements_kind();

  Handle<FixedArrayBase> constant_elements_values(
      array_boilerplate_description->constant_elements(), isolate);

  // Create the JSArray.
  Handle<FixedArrayBase> copied_elements_values;
  if (IsDoubleElementsKind(constant_elements_kind)) {
    copied_elements_values = isolate->factory()->CopyFixedDoubleArray(
        Cast<FixedDoubleArray>(constant_elements_values));
  } else {
    DCHECK(IsSmiOrObjectElementsKind(constant_elements_kind));
    const bool is_cow = (constant_elements_values->map() ==
                         ReadOnlyRoots(isolate).fixed_cow_array_map());
    if (is_cow) {
      copied_elements_values = constant_elements_values;
      if (DEBUG_BOOL) {
        auto fixed_array_values = Cast<FixedArray>(copied_elements_values);
        uint32_t fixed_array_values_len = fixed_array_values->ulength().value();
        for (uint32_t i = 0; i < fixed_array_values_len; i++) {
          DCHECK(!IsFixedArray(fixed_array_values->get(i)));
        }
      }
    } else {
      Handle<FixedArray> fixed_array_values =
          Cast<FixedArray>(constant_elements_values);
      Handle<FixedArray> fixed_array_values_copy =
          isolate->factory()->CopyFixedArray(fixed_array_values);
      copied_elements_values = fixed_array_values_copy;
      uint32_t fixed_array_values_len = fixed_array_values->ulength().value();
      for (uint32_t i = 0; i < fixed_array_values_len; i++) {
        Tagged<Object> value = fixed_array_values_copy->get(i);
        Tagged<HeapObject> value_heap_object;
        if (!value.GetHeapObject(isolate, &value_heap_object)) continue;
        if (IsAnyHole(value_heap_object)) continue;

        if (IsArrayBoilerplateDescription(value_heap_object, isolate)) {
          HandleScope sub_scope(isolate);
          DirectHandle<ArrayBoilerplateDescription> boilerplate(
              Cast<ArrayBoilerplateDescription>(value_heap_object), isolate);
          DirectHandle<JSObject> result =
              CreateArrayLiteral(isolate, boilerplate, allocation);
          fixed_array_values_copy->set(i, *result);

        } else if (IsObjectBoilerplateDescription(value_heap_object, isolate)) {
          HandleScope sub_scope(isolate);
          DirectHandle<ObjectBoilerplateDescription> boilerplate(
              Cast<ObjectBoilerplateDescription>(value_heap_object), isolate);
          DirectHandle<JSObject> result = CreateObjectLiteral(
              isolate, boilerplate, boilerplate->flags(), allocation);
          fixed_array_values_copy->set(i, *result);
        }
      }
    }
  }
  return isolate->factory()->NewJSArrayWithElements(
      copied_elements_values, constant_elements_kind,
      copied_elements_values->ulength().value(), allocation);
}

template <typename LiteralHelper>
MaybeDirectHandle<JSObject> CreateLiteralWithoutAllocationSite(
    Isolate* isolate, Handle<HeapObject> description, int flags) {
  return LiteralHelper::Create(isolate, description, flags,
                               AllocationType::kYoung);
}

template <typename LiteralHelper>
MaybeDirectHandle<JSObject> CreateLiteral(Isolate* isolate,
                                          Handle<HeapObject> maybe_vector,
                                          int literals_index,
                                          Handle<HeapObject> description,
                                          int flags) {
  if (!IsFeedbackVector(*maybe_vector)) {
    DCHECK(IsUndefined(*maybe_vector));
    return CreateLiteralWithoutAllocationSite<LiteralHelper>(
        isolate, description, flags);
  }
  auto vector = Cast<FeedbackVector>(maybe_vector);
  FeedbackSlot literals_slot(FeedbackVector::ToSlot(literals_index));
  CHECK(literals_slot.ToInt() < vector->length());
  Handle<Object> literal_site(Cast<Object>(vector->Get(literals_slot)),
                              isolate);
  Handle<AllocationSite> site;
  Handle<JSObject> boilerplate;

  if (HasBoilerplate(literal_site)) {
    site = Cast<AllocationSite>(literal_site);
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
    RETURN_ON_EXCEPTION(isolate, DeepWalk(boilerplate, &creation_context));
    creation_context.ExitScope(site, boilerplate);

    vector->SynchronizedSet(literals_slot, *site);
  }

  static_assert(static_cast<int>(ObjectLiteral::kDisableMementos) ==
                static_cast<int>(ArrayLiteral::kDisableMementos));
  bool enable_mementos = (flags & ObjectLiteral::kDisableMementos) == 0;

  // Copy the existing boilerplate.
  AllocationSiteUsageContext usage_context(isolate, site, enable_mementos);
  usage_context.EnterNewScope();
  MaybeDirectHandle<JSObject> copy = DeepCopy(boilerplate, &usage_context);
  usage_context.ExitScope(site, boilerplate);
  return copy;
}

DirectHandle<Object> InstantiateIfSharedFunctionInfo(
    DirectHandle<Context> context, Isolate* isolate,
    DirectHandle<JSObject> js_proto, DirectHandle<Object> value,
    DirectHandle<ClosureFeedbackCellArray> feedback_cell_array,
    Handle<ObjectBoilerplateDescription> object_boilerplate_description,
    int start_slot, int& current_slot) {
  DirectHandle<SharedFunctionInfo> shared;
  if (!TryCast<SharedFunctionInfo>(value, &shared)) {
    return value;
  }

  if (!v8_flags.proto_assign_seq_lazy_func_opt ||
      !base::IsInRange(current_slot, 0, kMaxUInt16)) {
    DirectHandle<FeedbackCell> feedback_cell(
        feedback_cell_array->get(current_slot), isolate);
    value = Factory::JSFunctionBuilder{isolate, shared, context}
                .set_feedback_cell(feedback_cell)
                .set_allocation_type(AllocationType::kYoung)
                .Build();
    ++current_slot;
    return value;
  }

  DirectHandle<Map> proto_map = direct_handle(js_proto->map(), isolate);
  if (Tagged<PrototypeSharedClosureInfo> closure_info;
      proto_map->TryGetPrototypeSharedClosureInfo(&closure_info)) {
    // We already have closure infos on this prototype, this means we
    // already called SetPrototypeProperties on it and some closures were
    // set up. We can only take the lazy closure path if the context
    // is the same.
    if (closure_info->context() == *context &&
        *object_boilerplate_description ==
            closure_info->boilerplate_description()) {
      // fast path
      shared->set_feedback_slot(current_slot);
    } else {
      // not lazy allocation
      DirectHandle<FeedbackCell> feedback_cell(
          feedback_cell_array->get(current_slot), isolate);
      value = Factory::JSFunctionBuilder{isolate, shared, context}
                  .set_feedback_cell(feedback_cell)
                  .set_allocation_type(AllocationType::kYoung)
                  .Build();
    }
  } else {
    // We do not have closure_info
    auto val = *isolate->factory()->NewPrototypeSharedClosureInfo(
        object_boilerplate_description, context, feedback_cell_array);

    proto_map->SetPrototypeSharedClosureInfo(val);
    shared->set_feedback_slot(current_slot);
  }
  ++current_slot;
  return value;
}

}  // namespace

RUNTIME_FUNCTION(Runtime_CreateObjectLiteral) {
  HandleScope scope(isolate);
  DCHECK_EQ(4, args.length());
  Handle<HeapObject> maybe_vector = args.at<HeapObject>(0);
  int literals_index = args.tagged_index_value_at(1);
  Handle<ObjectBoilerplateDescription> description =
      args.at<ObjectBoilerplateDescription>(2);
  int flags = args.smi_value_at(3);
  RETURN_RESULT_OR_FAILURE(
      isolate, CreateLiteral<ObjectLiteralHelper>(
                   isolate, maybe_vector, literals_index, description, flags));
}

static MaybeDirectHandle<Object> SetPrototypePropertiesSlow(
    Isolate* isolate, DirectHandle<Context> context, DirectHandle<JSAny> obj,
    Handle<ObjectBoilerplateDescription> object_boilerplate_description,
    DirectHandle<ClosureFeedbackCellArray> feedback_cell_array,
    int& current_slot, int start_index = 0) {
  MaybeDirectHandle<Object> result;

  int length = object_boilerplate_description->boilerplate_properties_count();
  for (int index = start_index; index < length; index++) {
    DirectHandle<Object> proto;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, proto,
        Runtime::GetObjectProperty(isolate, obj,
                                   isolate->factory()->prototype_string()));

    DirectHandle<Object> key(object_boilerplate_description->name(index),
                             isolate);
    DirectHandle<Object> value(object_boilerplate_description->value(index),
                               isolate);

    if (DirectHandle<SharedFunctionInfo> shared;
        TryCast<SharedFunctionInfo>(value, &shared)) {
      DirectHandle<FeedbackCell> feedback_cell(
          feedback_cell_array->get(current_slot++), isolate);
      value = Factory::JSFunctionBuilder{isolate, shared, context}
                  .set_feedback_cell(feedback_cell)
                  .set_allocation_type(AllocationType::kYoung)
                  .Build();
    }

    RETURN_ON_EXCEPTION(
        isolate, Runtime::SetObjectProperty(isolate, Cast<JSAny>(proto), key,
                                            value, StoreOrigin::kNamed));

    result = value;
  }

  return result.ToHandleChecked();
}

static bool IsDefaultFunctionPrototype(DirectHandle<JSObject> js_proto,
                                       Isolate* isolate) {
  // Object function prototype's map.
  Tagged<Map> proto_map = js_proto->map();

  // Check that given function.prototype object has a default initial state:
  // it's extensible.
  if (!proto_map->is_extensible()) {
    return false;
  }

  // it's in dictionary mode.
  if (!proto_map->is_dictionary_map()) {
    return false;
  }

  // it has exactly one "constructor" property installed.
  if (js_proto->property_dictionary()->NumberOfElements() != 1) {
    return false;
  }
  if (js_proto->property_dictionary()
          ->FindEntry(isolate, isolate->factory()->constructor_string())
          .is_not_found()) {
    return false;
  }

  // its prototype is the original and unmodified Object.prototype object.
  if (proto_map->prototype()->map() !=
      *isolate->object_function_prototype_map()) {
    return false;
  }

  return true;
}

RUNTIME_FUNCTION(Runtime_SetPrototypeProperties) {
  HandleScope scope(isolate);
  DCHECK_EQ(4, args.length());
  DirectHandle<Context> context(isolate->context(), isolate);
  DirectHandle<JSAny> obj = args.at<JSAny>(0);  // acc JS Object
  Handle<ObjectBoilerplateDescription> object_boilerplate_description =
      args.at<ObjectBoilerplateDescription>(1);
  DirectHandle<ClosureFeedbackCellArray> feedback_cell_array =
      args.at<ClosureFeedbackCellArray>(2);
  int current_slot = args.smi_value_at(3);
  int start_slot = current_slot;

  // Proxy and any non-function not welcome
  if (!IsJSFunction(*obj)) {
    RETURN_RESULT_OR_FAILURE(
        isolate, SetPrototypePropertiesSlow(isolate, context, obj,
                                            object_boilerplate_description,
                                            feedback_cell_array, current_slot));
  }

  DirectHandle<JSFunction> acc_fun = Cast<JSFunction>(obj);
  if (!acc_fun->has_prototype_slot()) {
    RETURN_RESULT_OR_FAILURE(
        isolate, SetPrototypePropertiesSlow(isolate, context, obj,
                                            object_boilerplate_description,
                                            feedback_cell_array, current_slot));
  }

  DirectHandle<Object> prototype =
      JSFunction::GetFunctionPrototype(isolate, acc_fun);

  DCHECK_EQ(*prototype,
            *Runtime::GetObjectProperty(isolate, obj,
                                        isolate->factory()->prototype_string())
                 .ToHandleChecked());

  if (IsNull(*prototype)) {
    RETURN_RESULT_OR_FAILURE(
        isolate, SetPrototypePropertiesSlow(isolate, context, obj,
                                            object_boilerplate_description,
                                            feedback_cell_array, current_slot));
  }

  DirectHandle<JSObject> js_proto;
  if (!TryCast<JSObject>(prototype, &js_proto)) {
    RETURN_RESULT_OR_FAILURE(
        isolate, SetPrototypePropertiesSlow(isolate, context, obj,
                                            object_boilerplate_description,
                                            feedback_cell_array, current_slot));
  }

  if (IsSpecialReceiverMap(js_proto->map())) {
    RETURN_RESULT_OR_FAILURE(
        isolate, SetPrototypePropertiesSlow(isolate, context, obj,
                                            object_boilerplate_description,
                                            feedback_cell_array, current_slot));
  }

  if (!JSObject::IsExtensible(isolate, js_proto)) {
    RETURN_RESULT_OR_FAILURE(
        isolate, SetPrototypePropertiesSlow(isolate, context, obj,
                                            object_boilerplate_description,
                                            feedback_cell_array, current_slot));
  }

  bool is_default_func_prototype =
      IsDefaultFunctionPrototype(js_proto, isolate);

  // It should now be safe to perform a fast merge
  MaybeDirectHandle<Object> result;
  int length = object_boilerplate_description->boilerplate_properties_count();
  if (is_default_func_prototype) {
    for (int index = 0; index < length; index++) {
      DirectHandle<Object> key(object_boilerplate_description->name(index),
                               isolate);
      DirectHandle<Object> value(object_boilerplate_description->value(index),
                                 isolate);

      value = InstantiateIfSharedFunctionInfo(
          context, isolate, js_proto, value, feedback_cell_array,
          object_boilerplate_description, start_slot, current_slot);

      DirectHandle<String> name = Cast<String>(key);
      DCHECK(!name->IsArrayIndex());
      DCHECK(!IsTheHole(*value));
      LookupIterator it(isolate, js_proto, name, LookupIterator::OWN);

      if (IsSharedFunctionInfo(*value)) {
        DirectHandle<AccessorInfo> accessor_info =
            isolate->factory()->lazy_closure_accessor();

        JSObject::SetAccessor(js_proto, name, accessor_info,
                              PropertyAttributes::NONE)
            .Check();
      } else {
        Object::TransitionAndWriteDataProperty(
            &it, value, NONE, Just(kDontThrow), StoreOrigin::kNamed)
            .Check();
      }
      result = value;
    }
  } else {
    // Make sure None of the keys we are writing to are setters/getters
    // TODO(rherouart): if prototype is empty we can skip these checks
    for (int index = 0; index < length; index++) {
      PropertyDescriptor desc;
      DirectHandle<Object> key(object_boilerplate_description->name(index),
                               isolate);
      DirectHandle<Object> value(object_boilerplate_description->value(index),
                                 isolate);

      CHECK(IsName(*key));
      PropertyKey lookup_key(isolate, key);

      LookupIterator it(isolate, js_proto, lookup_key,
                        LookupIterator::PROTOTYPE_CHAIN);

      LookupIterator::State it_state = it.state();
      if (it_state != LookupIterator::NOT_FOUND &&
          (it_state != LookupIterator::DATA || it.IsReadOnly())) {
        RETURN_RESULT_OR_FAILURE(
            isolate, SetPrototypePropertiesSlow(
                         isolate, context, obj, object_boilerplate_description,
                         feedback_cell_array, current_slot, index));
      }
      DCHECK(!IsTheHole(*value));

      if (it_state == LookupIterator::DATA &&
          it.HolderIsReceiverOrHiddenPrototype()) {
        DirectHandle<SharedFunctionInfo> shared;
        if (TryCast<SharedFunctionInfo>(value, &shared)) {
          // If we were to set an existing property to a SharedFunctionInfo,
          // there would be the risk of it being returned from IC without being
          // instantiated.
          DirectHandle<FeedbackCell> feedback_cell(
              feedback_cell_array->get(current_slot), isolate);
          value = Factory::JSFunctionBuilder{isolate, shared, context}
                      .set_feedback_cell(feedback_cell)
                      .set_allocation_type(AllocationType::kYoung)
                      .Build();
          current_slot++;
        }
        Object::SetDataProperty(&it, value).Check();
      } else {
        value = InstantiateIfSharedFunctionInfo(
            context, isolate, js_proto, value, feedback_cell_array,
            object_boilerplate_description, start_slot, current_slot);
        if (IsSharedFunctionInfo(*value)) {
          DirectHandle<AccessorInfo> accessor_info =
              isolate->factory()->lazy_closure_accessor();

          JSObject::SetAccessor(js_proto, Cast<Name>(key), accessor_info,
                                PropertyAttributes::NONE)
              .Check();
        } else {
          Object::TransitionAndWriteDataProperty(
              &it, value, NONE, Just(kDontThrow), StoreOrigin::kNamed)
              .Check();
        }
      }
      result = value;
    }
  }

  return *result.ToHandleChecked();
}

RUNTIME_FUNCTION(Runtime_CreateArrayLiteral) {
  HandleScope scope(isolate);
  DCHECK_EQ(4, args.length());
  Handle<HeapObject> maybe_vector = args.at<HeapObject>(0);
  int literals_index = args.tagged_index_value_at(1);
  Handle<ArrayBoilerplateDescription> elements =
      args.at<ArrayBoilerplateDescription>(2);
  int flags = args.smi_value_at(3);
  RETURN_RESULT_OR_FAILURE(
      isolate, CreateLiteral<ArrayLiteralHelper>(
                   isolate, maybe_vector, literals_index, elements, flags));
}

RUNTIME_FUNCTION(Runtime_CreateRegExpLiteral) {
  HandleScope scope(isolate);
  DCHECK_EQ(4, args.length());
  Handle<HeapObject> maybe_vector = args.at<HeapObject>(0);
  int index = args.tagged_index_value_at(1);
  DirectHandle<String> pattern = args.at<String>(2);
  int flags = args.smi_value_at(3);

  if (IsUndefined(*maybe_vector)) {
    // We don't have a vector; don't create a boilerplate, simply construct a
    // plain JSRegExp instance and return it.
    RETURN_RESULT_OR_FAILURE(
        isolate, JSRegExp::New(isolate, pattern, JSRegExp::Flags(flags)));
  }

  auto vector = Cast<FeedbackVector>(maybe_vector);
  FeedbackSlot literal_slot(FeedbackVector::ToSlot(index));
  DirectHandle<Object> literal_site(Cast<Object>(vector->Get(literal_slot)),
                                    isolate);

  // This function must not be called when a boilerplate already exists (if it
  // exists, callers should instead copy the boilerplate into a new JSRegExp
  // instance).
  CHECK(!HasBoilerplate(literal_site));

  DirectHandle<JSRegExp> regexp_instance;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, regexp_instance,
      JSRegExp::New(isolate, pattern, JSRegExp::Flags(flags)));

  // JSRegExp literal sites are initialized in a two-step process:
  // Uninitialized-Preinitialized, and Preinitialized-Initialized.
  if (IsUninitializedLiteralSite(*literal_site)) {
    PreInitializeLiteralSite(vector, literal_slot);
    return *regexp_instance;
  }

  DirectHandle<RegExpData> data(regexp_instance->data(isolate), isolate);
  DirectHandle<RegExpBoilerplateDescription> boilerplate =
      isolate->factory()->NewRegExpBoilerplateDescription(
          data, Smi::FromInt(static_cast<int>(regexp_instance->flags())));

  vector->SynchronizedSet(literal_slot, *boilerplate);
  DCHECK(HasBoilerplate(
      direct_handle(Cast<Object>(vector->Get(literal_slot)), isolate)));

  return *regexp_instance;
}

}  // namespace internal
}  // namespace v8
