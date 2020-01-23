// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ostream>

#include "src/compiler/access-info.h"

#include "src/builtins/accessors.h"
#include "src/compiler/compilation-dependencies.h"
#include "src/compiler/compilation-dependency.h"
#include "src/compiler/simplified-operator.h"
#include "src/compiler/type-cache.h"
#include "src/ic/call-optimization.h"
#include "src/logging/counters.h"
#include "src/objects/cell-inl.h"
#include "src/objects/field-index-inl.h"
#include "src/objects/field-type.h"
#include "src/objects/module-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/struct-inl.h"
#include "src/objects/templates.h"

namespace v8 {
namespace internal {
namespace compiler {

namespace {

bool CanInlinePropertyAccess(Handle<Map> map) {
  // We can inline property access to prototypes of all primitives, except
  // the special Oddball ones that have no wrapper counterparts (i.e. Null,
  // Undefined and TheHole).
  STATIC_ASSERT(ODDBALL_TYPE == LAST_PRIMITIVE_HEAP_OBJECT_TYPE);
  if (map->IsBooleanMap()) return true;
  if (map->instance_type() < LAST_PRIMITIVE_HEAP_OBJECT_TYPE) return true;
  return map->IsJSObjectMap() && !map->is_dictionary_map() &&
         !map->has_named_interceptor() &&
         // TODO(verwaest): Whitelist contexts to which we have access.
         !map->is_access_check_needed();
}

}  // namespace


std::ostream& operator<<(std::ostream& os, AccessMode access_mode) {
  switch (access_mode) {
    case AccessMode::kLoad:
      return os << "Load";
    case AccessMode::kStore:
      return os << "Store";
    case AccessMode::kStoreInLiteral:
      return os << "StoreInLiteral";
    case AccessMode::kHas:
      return os << "Has";
  }
  UNREACHABLE();
}

ElementAccessInfo::ElementAccessInfo(ZoneVector<Handle<Map>>&& receiver_maps,
                                     ElementsKind elements_kind, Zone* zone)
    : elements_kind_(elements_kind),
      receiver_maps_(receiver_maps),
      transition_sources_(zone) {
  CHECK(!receiver_maps.empty());
}

// static
PropertyAccessInfo PropertyAccessInfo::Invalid(Zone* zone) {
  return PropertyAccessInfo(zone);
}

// static
PropertyAccessInfo PropertyAccessInfo::NotFound(Zone* zone,
                                                Handle<Map> receiver_map,
                                                MaybeHandle<JSObject> holder) {
  return PropertyAccessInfo(zone, kNotFound, holder, {{receiver_map}, zone});
}

// static
PropertyAccessInfo PropertyAccessInfo::DataField(
    Zone* zone, Handle<Map> receiver_map,
    ZoneVector<CompilationDependency const*>&& dependencies,
    FieldIndex field_index, Representation field_representation,
    Type field_type, Handle<Map> field_owner_map, MaybeHandle<Map> field_map,
    MaybeHandle<JSObject> holder, MaybeHandle<Map> transition_map) {
  return PropertyAccessInfo(kDataField, holder, transition_map, field_index,
                            field_representation, field_type, field_owner_map,
                            field_map, {{receiver_map}, zone},
                            std::move(dependencies));
}

// static
PropertyAccessInfo PropertyAccessInfo::DataConstant(
    Zone* zone, Handle<Map> receiver_map,
    ZoneVector<CompilationDependency const*>&& dependencies,
    FieldIndex field_index, Representation field_representation,
    Type field_type, Handle<Map> field_owner_map, MaybeHandle<Map> field_map,
    MaybeHandle<JSObject> holder, MaybeHandle<Map> transition_map) {
  return PropertyAccessInfo(kDataConstant, holder, transition_map, field_index,
                            field_representation, field_type, field_owner_map,
                            field_map, {{receiver_map}, zone},
                            std::move(dependencies));
}

// static
PropertyAccessInfo PropertyAccessInfo::AccessorConstant(
    Zone* zone, Handle<Map> receiver_map, Handle<Object> constant,
    MaybeHandle<JSObject> holder) {
  return PropertyAccessInfo(zone, kAccessorConstant, holder, constant,
                            {{receiver_map}, zone});
}

// static
PropertyAccessInfo PropertyAccessInfo::ModuleExport(Zone* zone,
                                                    Handle<Map> receiver_map,
                                                    Handle<Cell> cell) {
  return PropertyAccessInfo(zone, kModuleExport, MaybeHandle<JSObject>(), cell,
                            {{receiver_map}, zone});
}

// static
PropertyAccessInfo PropertyAccessInfo::StringLength(Zone* zone,
                                                    Handle<Map> receiver_map) {
  return PropertyAccessInfo(zone, kStringLength, MaybeHandle<JSObject>(),
                            {{receiver_map}, zone});
}

PropertyAccessInfo::PropertyAccessInfo(Zone* zone)
    : kind_(kInvalid),
      receiver_maps_(zone),
      unrecorded_dependencies_(zone),
      field_representation_(Representation::None()),
      field_type_(Type::None()) {}

PropertyAccessInfo::PropertyAccessInfo(Zone* zone, Kind kind,
                                       MaybeHandle<JSObject> holder,
                                       ZoneVector<Handle<Map>>&& receiver_maps)
    : kind_(kind),
      receiver_maps_(receiver_maps),
      unrecorded_dependencies_(zone),
      holder_(holder),
      field_representation_(Representation::None()),
      field_type_(Type::None()) {}

PropertyAccessInfo::PropertyAccessInfo(Zone* zone, Kind kind,
                                       MaybeHandle<JSObject> holder,
                                       Handle<Object> constant,
                                       ZoneVector<Handle<Map>>&& receiver_maps)
    : kind_(kind),
      receiver_maps_(receiver_maps),
      unrecorded_dependencies_(zone),
      constant_(constant),
      holder_(holder),
      field_representation_(Representation::None()),
      field_type_(Type::Any()) {}

PropertyAccessInfo::PropertyAccessInfo(
    Kind kind, MaybeHandle<JSObject> holder, MaybeHandle<Map> transition_map,
    FieldIndex field_index, Representation field_representation,
    Type field_type, Handle<Map> field_owner_map, MaybeHandle<Map> field_map,
    ZoneVector<Handle<Map>>&& receiver_maps,
    ZoneVector<CompilationDependency const*>&& unrecorded_dependencies)
    : kind_(kind),
      receiver_maps_(receiver_maps),
      unrecorded_dependencies_(std::move(unrecorded_dependencies)),
      transition_map_(transition_map),
      holder_(holder),
      field_index_(field_index),
      field_representation_(field_representation),
      field_type_(field_type),
      field_owner_map_(field_owner_map),
      field_map_(field_map) {
  DCHECK_IMPLIES(!transition_map.is_null(),
                 field_owner_map.address() == transition_map.address());
}

bool PropertyAccessInfo::Merge(PropertyAccessInfo const* that,
                               AccessMode access_mode, Zone* zone) {
  if (this->kind_ != that->kind_) return false;
  if (this->holder_.address() != that->holder_.address()) return false;

  switch (this->kind_) {
    case kInvalid:
      return that->kind_ == kInvalid;

    case kDataField:
    case kDataConstant: {
      // Check if we actually access the same field (we use the
      // GetFieldAccessStubKey method here just like the ICs do
      // since that way we only compare the relevant bits of the
      // field indices).
      if (this->field_index_.GetFieldAccessStubKey() ==
          that->field_index_.GetFieldAccessStubKey()) {
        switch (access_mode) {
          case AccessMode::kHas:
          case AccessMode::kLoad: {
            if (!this->field_representation_.Equals(
                    that->field_representation_)) {
              if (this->field_representation_.IsDouble() ||
                  that->field_representation_.IsDouble()) {
                return false;
              }
              this->field_representation_ = Representation::Tagged();
            }
            if (this->field_map_.address() != that->field_map_.address()) {
              this->field_map_ = MaybeHandle<Map>();
            }
            break;
          }
          case AccessMode::kStore:
          case AccessMode::kStoreInLiteral: {
            // For stores, the field map and field representation information
            // must match exactly, otherwise we cannot merge the stores. We
            // also need to make sure that in case of transitioning stores,
            // the transition targets match.
            if (this->field_map_.address() != that->field_map_.address() ||
                !this->field_representation_.Equals(
                    that->field_representation_) ||
                this->transition_map_.address() !=
                    that->transition_map_.address()) {
              return false;
            }
            break;
          }
        }
        this->field_type_ =
            Type::Union(this->field_type_, that->field_type_, zone);
        this->receiver_maps_.insert(this->receiver_maps_.end(),
                                    that->receiver_maps_.begin(),
                                    that->receiver_maps_.end());
        this->unrecorded_dependencies_.insert(
            this->unrecorded_dependencies_.end(),
            that->unrecorded_dependencies_.begin(),
            that->unrecorded_dependencies_.end());
        return true;
      }
      return false;
    }

    case kAccessorConstant: {
      // Check if we actually access the same constant.
      if (this->constant_.address() == that->constant_.address()) {
        DCHECK(this->unrecorded_dependencies_.empty());
        DCHECK(that->unrecorded_dependencies_.empty());
        this->receiver_maps_.insert(this->receiver_maps_.end(),
                                    that->receiver_maps_.begin(),
                                    that->receiver_maps_.end());
        return true;
      }
      return false;
    }

    case kNotFound:
    case kStringLength: {
      DCHECK(this->unrecorded_dependencies_.empty());
      DCHECK(that->unrecorded_dependencies_.empty());
      this->receiver_maps_.insert(this->receiver_maps_.end(),
                                  that->receiver_maps_.begin(),
                                  that->receiver_maps_.end());
      return true;
    }
    case kModuleExport:
      return false;
  }
}

ConstFieldInfo PropertyAccessInfo::GetConstFieldInfo() const {
  if (IsDataConstant()) {
    return ConstFieldInfo(field_owner_map_.ToHandleChecked());
  }
  return ConstFieldInfo::None();
}

AccessInfoFactory::AccessInfoFactory(JSHeapBroker* broker,
                                     CompilationDependencies* dependencies,
                                     Zone* zone)
    : broker_(broker),
      dependencies_(dependencies),
      type_cache_(TypeCache::Get()),
      zone_(zone) {}

base::Optional<ElementAccessInfo> AccessInfoFactory::ComputeElementAccessInfo(
    Handle<Map> map, AccessMode access_mode) const {
  // Check if it is safe to inline element access for the {map}.
  MapRef map_ref(broker(), map);
  if (!CanInlineElementAccess(map_ref)) return base::nullopt;
  ElementsKind const elements_kind = map_ref.elements_kind();
  return ElementAccessInfo({{map}, zone()}, elements_kind, zone());
}

bool AccessInfoFactory::ComputeElementAccessInfos(
    ElementAccessFeedback const& feedback,
    ZoneVector<ElementAccessInfo>* access_infos) const {
  AccessMode access_mode = feedback.keyed_mode().access_mode();
  if (access_mode == AccessMode::kLoad || access_mode == AccessMode::kHas) {
    // For polymorphic loads of similar elements kinds (i.e. all tagged or all
    // double), always use the "worst case" code without a transition.  This is
    // much faster than transitioning the elements to the worst case, trading a
    // TransitionElementsKind for a CheckMaps, avoiding mutation of the array.
    base::Optional<ElementAccessInfo> access_info =
        ConsolidateElementLoad(feedback);
    if (access_info.has_value()) {
      access_infos->push_back(*access_info);
      return true;
    }
  }

  for (auto const& group : feedback.transition_groups()) {
    DCHECK(!group.empty());
    Handle<Map> target = group.front();
    base::Optional<ElementAccessInfo> access_info =
        ComputeElementAccessInfo(target, access_mode);
    if (!access_info.has_value()) return false;

    for (size_t i = 1; i < group.size(); ++i) {
      access_info->AddTransitionSource(group[i]);
    }
    access_infos->push_back(*access_info);
  }
  return true;
}

PropertyAccessInfo AccessInfoFactory::ComputeDataFieldAccessInfo(
    Handle<Map> receiver_map, Handle<Map> map, MaybeHandle<JSObject> holder,
    InternalIndex descriptor, AccessMode access_mode) const {
  DCHECK(descriptor.is_found());
  Handle<DescriptorArray> descriptors(map->instance_descriptors(), isolate());
  PropertyDetails const details = descriptors->GetDetails(descriptor);
  int index = descriptors->GetFieldIndex(descriptor);
  Representation details_representation = details.representation();
  if (details_representation.IsNone()) {
    // The ICs collect feedback in PREMONOMORPHIC state already,
    // but at this point the {receiver_map} might still contain
    // fields for which the representation has not yet been
    // determined by the runtime. So we need to catch this case
    // here and fall back to use the regular IC logic instead.
    return PropertyAccessInfo::Invalid(zone());
  }
  FieldIndex field_index =
      FieldIndex::ForPropertyIndex(*map, index, details_representation);
  Type field_type = Type::NonInternal();
  MaybeHandle<Map> field_map;
  MapRef map_ref(broker(), map);
  ZoneVector<CompilationDependency const*> unrecorded_dependencies(zone());
  map_ref.SerializeOwnDescriptor(descriptor);
  if (details_representation.IsSmi()) {
    field_type = Type::SignedSmall();
    unrecorded_dependencies.push_back(
        dependencies()->FieldRepresentationDependencyOffTheRecord(map_ref,
                                                                  descriptor));
  } else if (details_representation.IsDouble()) {
    field_type = type_cache_->kFloat64;
    if (!FLAG_unbox_double_fields) {
      unrecorded_dependencies.push_back(
          dependencies()->FieldRepresentationDependencyOffTheRecord(
              map_ref, descriptor));
    }
  } else if (details_representation.IsHeapObject()) {
    // Extract the field type from the property details (make sure its
    // representation is TaggedPointer to reflect the heap object case).
    Handle<FieldType> descriptors_field_type(
        descriptors->GetFieldType(descriptor), isolate());
    if (descriptors_field_type->IsNone()) {
      // Store is not safe if the field type was cleared.
      if (access_mode == AccessMode::kStore) {
        return PropertyAccessInfo::Invalid(zone());
      }

      // The field type was cleared by the GC, so we don't know anything
      // about the contents now.
    }
    unrecorded_dependencies.push_back(
        dependencies()->FieldRepresentationDependencyOffTheRecord(map_ref,
                                                                  descriptor));
    if (descriptors_field_type->IsClass()) {
      // Remember the field map, and try to infer a useful type.
      Handle<Map> map(descriptors_field_type->AsClass(), isolate());
      field_type = Type::For(MapRef(broker(), map));
      field_map = MaybeHandle<Map>(map);
    }
  } else {
    CHECK(details_representation.IsTagged());
  }
  // TODO(turbofan): We may want to do this only depending on the use
  // of the access info.
  unrecorded_dependencies.push_back(
      dependencies()->FieldTypeDependencyOffTheRecord(map_ref, descriptor));

  PropertyConstness constness;
  if (details.IsReadOnly() && !details.IsConfigurable()) {
    constness = PropertyConstness::kConst;
  } else {
    map_ref.SerializeOwnDescriptor(descriptor);
    constness = dependencies()->DependOnFieldConstness(map_ref, descriptor);
  }
  Handle<Map> field_owner_map(map->FindFieldOwner(isolate(), descriptor),
                              isolate());
  switch (constness) {
    case PropertyConstness::kMutable:
      return PropertyAccessInfo::DataField(
          zone(), receiver_map, std::move(unrecorded_dependencies), field_index,
          details_representation, field_type, field_owner_map, field_map,
          holder);
    case PropertyConstness::kConst:
      return PropertyAccessInfo::DataConstant(
          zone(), receiver_map, std::move(unrecorded_dependencies), field_index,
          details_representation, field_type, field_owner_map, field_map,
          holder);
  }
  UNREACHABLE();
}

PropertyAccessInfo AccessInfoFactory::ComputeAccessorDescriptorAccessInfo(
    Handle<Map> receiver_map, Handle<Name> name, Handle<Map> map,
    MaybeHandle<JSObject> holder, InternalIndex descriptor,
    AccessMode access_mode) const {
  DCHECK(descriptor.is_found());
  Handle<DescriptorArray> descriptors(map->instance_descriptors(), isolate());
  SLOW_DCHECK(descriptor == descriptors->Search(*name, *map));
  if (map->instance_type() == JS_MODULE_NAMESPACE_TYPE) {
    DCHECK(map->is_prototype_map());
    Handle<PrototypeInfo> proto_info(PrototypeInfo::cast(map->prototype_info()),
                                     isolate());
    Handle<JSModuleNamespace> module_namespace(
        JSModuleNamespace::cast(proto_info->module_namespace()), isolate());
    Handle<Cell> cell(
        Cell::cast(module_namespace->module().exports().Lookup(
            ReadOnlyRoots(isolate()), name, Smi::ToInt(name->GetHash()))),
        isolate());
    if (cell->value().IsTheHole(isolate())) {
      // This module has not been fully initialized yet.
      return PropertyAccessInfo::Invalid(zone());
    }
    return PropertyAccessInfo::ModuleExport(zone(), receiver_map, cell);
  }
  if (access_mode == AccessMode::kHas) {
    // HasProperty checks don't call getter/setters, existence is sufficient.
    return PropertyAccessInfo::AccessorConstant(zone(), receiver_map,
                                                Handle<Object>(), holder);
  }
  Handle<Object> accessors(descriptors->GetStrongValue(descriptor), isolate());
  if (!accessors->IsAccessorPair()) {
    return PropertyAccessInfo::Invalid(zone());
  }
  Handle<Object> accessor(access_mode == AccessMode::kLoad
                              ? Handle<AccessorPair>::cast(accessors)->getter()
                              : Handle<AccessorPair>::cast(accessors)->setter(),
                          isolate());
  if (!accessor->IsJSFunction()) {
    CallOptimization optimization(isolate(), accessor);
    if (!optimization.is_simple_api_call() ||
        optimization.IsCrossContextLazyAccessorPair(
            *broker()->target_native_context().object(), *map)) {
      return PropertyAccessInfo::Invalid(zone());
    }

    CallOptimization::HolderLookup lookup;
    holder = optimization.LookupHolderOfExpectedType(receiver_map, &lookup);
    if (lookup == CallOptimization::kHolderNotFound) {
      return PropertyAccessInfo::Invalid(zone());
    }
    DCHECK_IMPLIES(lookup == CallOptimization::kHolderIsReceiver,
                   holder.is_null());
    DCHECK_IMPLIES(lookup == CallOptimization::kHolderFound, !holder.is_null());
  }
  if (access_mode == AccessMode::kLoad) {
    Handle<Name> cached_property_name;
    if (FunctionTemplateInfo::TryGetCachedPropertyName(isolate(), accessor)
            .ToHandle(&cached_property_name)) {
      PropertyAccessInfo access_info =
          ComputePropertyAccessInfo(map, cached_property_name, access_mode);
      if (!access_info.IsInvalid()) return access_info;
    }
  }
  return PropertyAccessInfo::AccessorConstant(zone(), receiver_map, accessor,
                                              holder);
}

PropertyAccessInfo AccessInfoFactory::ComputePropertyAccessInfo(
    Handle<Map> map, Handle<Name> name, AccessMode access_mode) const {
  CHECK(name->IsUniqueName());

  if (access_mode == AccessMode::kHas && !map->IsJSReceiverMap()) {
    return PropertyAccessInfo::Invalid(zone());
  }

  // Check if it is safe to inline property access for the {map}.
  if (!CanInlinePropertyAccess(map)) {
    return PropertyAccessInfo::Invalid(zone());
  }

  // We support fast inline cases for certain JSObject getters.
  if (access_mode == AccessMode::kLoad || access_mode == AccessMode::kHas) {
    PropertyAccessInfo access_info = LookupSpecialFieldAccessor(map, name);
    if (!access_info.IsInvalid()) return access_info;
  }

  // Remember the receiver map. We use {map} as loop variable.
  Handle<Map> receiver_map = map;
  MaybeHandle<JSObject> holder;
  while (true) {
    // Lookup the named property on the {map}.
    Handle<DescriptorArray> descriptors(map->instance_descriptors(), isolate());
    InternalIndex const number = descriptors->Search(*name, *map);
    if (number.is_found()) {
      PropertyDetails const details = descriptors->GetDetails(number);
      if (access_mode == AccessMode::kStore ||
          access_mode == AccessMode::kStoreInLiteral) {
        // Don't bother optimizing stores to read-only properties.
        if (details.IsReadOnly()) {
          return PropertyAccessInfo::Invalid(zone());
        }
        if (details.kind() == kData && !holder.is_null()) {
          // This is a store to a property not found on the receiver but on a
          // prototype. According to ES6 section 9.1.9 [[Set]], we need to
          // create a new data property on the receiver. We can still optimize
          // if such a transition already exists.
          return LookupTransition(receiver_map, name, holder);
        }
      }
      if (details.location() == kField) {
        if (details.kind() == kData) {
          return ComputeDataFieldAccessInfo(receiver_map, map, holder, number,
                                            access_mode);
        } else {
          DCHECK_EQ(kAccessor, details.kind());
          // TODO(turbofan): Add support for general accessors?
          return PropertyAccessInfo::Invalid(zone());
        }
      } else {
        DCHECK_EQ(kDescriptor, details.location());
        DCHECK_EQ(kAccessor, details.kind());
        return ComputeAccessorDescriptorAccessInfo(receiver_map, name, map,
                                                   holder, number, access_mode);
      }
      UNREACHABLE();
    }

    // The property wasn't found on {map}. Look on the prototype if appropriate.

    // Don't search on the prototype chain for special indices in case of
    // integer indexed exotic objects (see ES6 section 9.4.5).
    if (map->IsJSTypedArrayMap() && name->IsString() &&
        IsSpecialIndex(String::cast(*name))) {
      return PropertyAccessInfo::Invalid(zone());
    }

    // Don't search on the prototype when storing in literals.
    if (access_mode == AccessMode::kStoreInLiteral) {
      return LookupTransition(receiver_map, name, holder);
    }

    // Don't lookup private symbols on the prototype chain.
    if (name->IsPrivate()) {
      return PropertyAccessInfo::Invalid(zone());
    }

    // Walk up the prototype chain.
    MapRef(broker(), map).SerializePrototype();
    if (!map->prototype().IsJSObject()) {
      // Perform the implicit ToObject for primitives here.
      // Implemented according to ES6 section 7.3.2 GetV (V, P).
      Handle<JSFunction> constructor;
      if (Map::GetConstructorFunction(
              map, broker()->target_native_context().object())
              .ToHandle(&constructor)) {
        map = handle(constructor->initial_map(), isolate());
        DCHECK(map->prototype().IsJSObject());
      } else if (map->prototype().IsNull(isolate())) {
        // Store to property not found on the receiver or any prototype, we need
        // to transition to a new data property.
        // Implemented according to ES6 section 9.1.9 [[Set]] (P, V, Receiver)
        if (access_mode == AccessMode::kStore) {
          return LookupTransition(receiver_map, name, holder);
        }
        // The property was not found (access returns undefined or throws
        // depending on the language mode of the load operation.
        // Implemented according to ES6 section 9.1.8 [[Get]] (P, Receiver)
        return PropertyAccessInfo::NotFound(zone(), receiver_map, holder);
      } else {
        return PropertyAccessInfo::Invalid(zone());
      }
    }
    Handle<JSObject> map_prototype(JSObject::cast(map->prototype()), isolate());
    if (map_prototype->map().is_deprecated()) {
      // Try to migrate the prototype object so we don't embed the deprecated
      // map into the optimized code.
      JSObject::TryMigrateInstance(isolate(), map_prototype);
    }
    map = handle(map_prototype->map(), isolate());
    holder = map_prototype;

    if (!CanInlinePropertyAccess(map)) {
      return PropertyAccessInfo::Invalid(zone());
    }

    // Successful lookup on prototype chain needs to guarantee that all
    // the prototypes up to the holder have stable maps. Let us make sure
    // the prototype maps are stable here.
    CHECK(map->is_stable());
  }
  UNREACHABLE();
}

PropertyAccessInfo AccessInfoFactory::FinalizePropertyAccessInfosAsOne(
    ZoneVector<PropertyAccessInfo> access_infos, AccessMode access_mode) const {
  ZoneVector<PropertyAccessInfo> merged_access_infos(zone());
  MergePropertyAccessInfos(access_infos, access_mode, &merged_access_infos);
  if (merged_access_infos.size() == 1) {
    PropertyAccessInfo& result = merged_access_infos.front();
    if (!result.IsInvalid()) {
      result.RecordDependencies(dependencies());
      return result;
    }
  }
  return PropertyAccessInfo::Invalid(zone());
}

void AccessInfoFactory::ComputePropertyAccessInfos(
    MapHandles const& maps, Handle<Name> name, AccessMode access_mode,
    ZoneVector<PropertyAccessInfo>* access_infos) const {
  DCHECK(access_infos->empty());
  for (Handle<Map> map : maps) {
    access_infos->push_back(ComputePropertyAccessInfo(map, name, access_mode));
  }
}

void PropertyAccessInfo::RecordDependencies(
    CompilationDependencies* dependencies) {
  for (CompilationDependency const* d : unrecorded_dependencies_) {
    dependencies->RecordDependency(d);
  }
  unrecorded_dependencies_.clear();
}

bool AccessInfoFactory::FinalizePropertyAccessInfos(
    ZoneVector<PropertyAccessInfo> access_infos, AccessMode access_mode,
    ZoneVector<PropertyAccessInfo>* result) const {
  if (access_infos.empty()) return false;
  MergePropertyAccessInfos(access_infos, access_mode, result);
  for (PropertyAccessInfo const& info : *result) {
    if (info.IsInvalid()) return false;
  }
  for (PropertyAccessInfo& info : *result) {
    info.RecordDependencies(dependencies());
  }
  return true;
}

void AccessInfoFactory::MergePropertyAccessInfos(
    ZoneVector<PropertyAccessInfo> infos, AccessMode access_mode,
    ZoneVector<PropertyAccessInfo>* result) const {
  DCHECK(result->empty());
  for (auto it = infos.begin(), end = infos.end(); it != end; ++it) {
    bool merged = false;
    for (auto ot = it + 1; ot != end; ++ot) {
      if (ot->Merge(&(*it), access_mode, zone())) {
        merged = true;
        break;
      }
    }
    if (!merged) result->push_back(*it);
  }
  CHECK(!result->empty());
}

Isolate* AccessInfoFactory::isolate() const { return broker()->isolate(); }

namespace {

Maybe<ElementsKind> GeneralizeElementsKind(ElementsKind this_kind,
                                           ElementsKind that_kind) {
  if (IsHoleyElementsKind(this_kind)) {
    that_kind = GetHoleyElementsKind(that_kind);
  } else if (IsHoleyElementsKind(that_kind)) {
    this_kind = GetHoleyElementsKind(this_kind);
  }
  if (this_kind == that_kind) return Just(this_kind);
  if (IsDoubleElementsKind(that_kind) == IsDoubleElementsKind(this_kind)) {
    if (IsMoreGeneralElementsKindTransition(that_kind, this_kind)) {
      return Just(this_kind);
    }
    if (IsMoreGeneralElementsKindTransition(this_kind, that_kind)) {
      return Just(that_kind);
    }
  }
  return Nothing<ElementsKind>();
}

}  // namespace

base::Optional<ElementAccessInfo> AccessInfoFactory::ConsolidateElementLoad(
    ElementAccessFeedback const& feedback) const {
  if (feedback.transition_groups().empty()) return base::nullopt;

  DCHECK(!feedback.transition_groups().front().empty());
  MapRef first_map(broker(), feedback.transition_groups().front().front());
  InstanceType instance_type = first_map.instance_type();
  ElementsKind elements_kind = first_map.elements_kind();

  ZoneVector<Handle<Map>> maps(zone());
  for (auto const& group : feedback.transition_groups()) {
    for (Handle<Map> map_handle : group) {
      MapRef map(broker(), map_handle);
      if (map.instance_type() != instance_type ||
          !CanInlineElementAccess(map)) {
        return base::nullopt;
      }
      if (!GeneralizeElementsKind(elements_kind, map.elements_kind())
               .To(&elements_kind)) {
        return base::nullopt;
      }
      maps.push_back(map.object());
    }
  }

  return ElementAccessInfo(std::move(maps), elements_kind, zone());
}

PropertyAccessInfo AccessInfoFactory::LookupSpecialFieldAccessor(
    Handle<Map> map, Handle<Name> name) const {
  // Check for String::length field accessor.
  if (map->IsStringMap()) {
    if (Name::Equals(isolate(), name, isolate()->factory()->length_string())) {
      return PropertyAccessInfo::StringLength(zone(), map);
    }
    return PropertyAccessInfo::Invalid(zone());
  }
  // Check for special JSObject field accessors.
  FieldIndex field_index;
  if (Accessors::IsJSObjectFieldAccessor(isolate(), map, name, &field_index)) {
    Type field_type = Type::NonInternal();
    Representation field_representation = Representation::Tagged();
    if (map->IsJSArrayMap()) {
      DCHECK(
          Name::Equals(isolate(), isolate()->factory()->length_string(), name));
      // The JSArray::length property is a smi in the range
      // [0, FixedDoubleArray::kMaxLength] in case of fast double
      // elements, a smi in the range [0, FixedArray::kMaxLength]
      // in case of other fast elements, and [0, kMaxUInt32] in
      // case of other arrays.
      if (IsDoubleElementsKind(map->elements_kind())) {
        field_type = type_cache_->kFixedDoubleArrayLengthType;
        field_representation = Representation::Smi();
      } else if (IsFastElementsKind(map->elements_kind())) {
        field_type = type_cache_->kFixedArrayLengthType;
        field_representation = Representation::Smi();
      } else {
        field_type = type_cache_->kJSArrayLengthType;
      }
    }
    // Special fields are always mutable.
    return PropertyAccessInfo::DataField(zone(), map, {{}, zone()}, field_index,
                                         field_representation, field_type, map);
  }
  return PropertyAccessInfo::Invalid(zone());
}

PropertyAccessInfo AccessInfoFactory::LookupTransition(
    Handle<Map> map, Handle<Name> name, MaybeHandle<JSObject> holder) const {
  // Check if the {map} has a data transition with the given {name}.
  Map transition =
      TransitionsAccessor(isolate(), map).SearchTransition(*name, kData, NONE);
  if (transition.is_null()) {
    return PropertyAccessInfo::Invalid(zone());
  }

  Handle<Map> transition_map(transition, isolate());
  InternalIndex const number = transition_map->LastAdded();
  PropertyDetails const details =
      transition_map->instance_descriptors().GetDetails(number);
  // Don't bother optimizing stores to read-only properties.
  if (details.IsReadOnly()) {
    return PropertyAccessInfo::Invalid(zone());
  }
  // TODO(bmeurer): Handle transition to data constant?
  if (details.location() != kField) {
    return PropertyAccessInfo::Invalid(zone());
  }
  int const index = details.field_index();
  Representation details_representation = details.representation();
  FieldIndex field_index = FieldIndex::ForPropertyIndex(*transition_map, index,
                                                        details_representation);
  Type field_type = Type::NonInternal();
  MaybeHandle<Map> field_map;
  MapRef transition_map_ref(broker(), transition_map);
  ZoneVector<CompilationDependency const*> unrecorded_dependencies(zone());
  if (details_representation.IsSmi()) {
    field_type = Type::SignedSmall();
    transition_map_ref.SerializeOwnDescriptor(number);
    unrecorded_dependencies.push_back(
        dependencies()->FieldRepresentationDependencyOffTheRecord(
            transition_map_ref, number));
  } else if (details_representation.IsDouble()) {
    field_type = type_cache_->kFloat64;
    if (!FLAG_unbox_double_fields) {
      transition_map_ref.SerializeOwnDescriptor(number);
      unrecorded_dependencies.push_back(
          dependencies()->FieldRepresentationDependencyOffTheRecord(
              transition_map_ref, number));
    }
  } else if (details_representation.IsHeapObject()) {
    // Extract the field type from the property details (make sure its
    // representation is TaggedPointer to reflect the heap object case).
    Handle<FieldType> descriptors_field_type(
        transition_map->instance_descriptors().GetFieldType(number), isolate());
    if (descriptors_field_type->IsNone()) {
      // Store is not safe if the field type was cleared.
      return PropertyAccessInfo::Invalid(zone());
    }
    transition_map_ref.SerializeOwnDescriptor(number);
    unrecorded_dependencies.push_back(
        dependencies()->FieldRepresentationDependencyOffTheRecord(
            transition_map_ref, number));
    if (descriptors_field_type->IsClass()) {
      unrecorded_dependencies.push_back(
          dependencies()->FieldTypeDependencyOffTheRecord(transition_map_ref,
                                                          number));
      // Remember the field map, and try to infer a useful type.
      Handle<Map> map(descriptors_field_type->AsClass(), isolate());
      field_type = Type::For(MapRef(broker(), map));
      field_map = MaybeHandle<Map>(map);
    }
  }
  unrecorded_dependencies.push_back(
      dependencies()->TransitionDependencyOffTheRecord(
          MapRef(broker(), transition_map)));
  transition_map_ref.SerializeBackPointer();  // For BuildPropertyStore.
  // Transitioning stores *may* store to const fields. The resulting
  // DataConstant access infos can be distinguished from later, i.e. redundant,
  // stores to the same constant field by the presence of a transition map.
  switch (details.constness()) {
    case PropertyConstness::kMutable:
      return PropertyAccessInfo::DataField(
          zone(), map, std::move(unrecorded_dependencies), field_index,
          details_representation, field_type, transition_map, field_map, holder,
          transition_map);
    case PropertyConstness::kConst:
      return PropertyAccessInfo::DataConstant(
          zone(), map, std::move(unrecorded_dependencies), field_index,
          details_representation, field_type, transition_map, field_map, holder,
          transition_map);
  }
  UNREACHABLE();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
