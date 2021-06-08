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

bool CanInlinePropertyAccess(Handle<Map> map, AccessMode access_mode) {
  // We can inline property access to prototypes of all primitives, except
  // the special Oddball ones that have no wrapper counterparts (i.e. Null,
  // Undefined and TheHole).
  // We can only inline accesses to dictionary mode holders if the access is a
  // load and the holder is a prototype. The latter ensures a 1:1
  // relationship between the map and the object (and therefore the property
  // dictionary).
  STATIC_ASSERT(ODDBALL_TYPE == LAST_PRIMITIVE_HEAP_OBJECT_TYPE);
  if (map->IsBooleanMap()) return true;
  if (map->instance_type() < LAST_PRIMITIVE_HEAP_OBJECT_TYPE) return true;
  if (map->IsJSObjectMap()) {
    if (map->is_dictionary_map()) {
      if (!V8_DICT_PROPERTY_CONST_TRACKING_BOOL) return false;
      return access_mode == AccessMode::kLoad && map->is_prototype_map();
    }
    return !map->has_named_interceptor() &&
           // TODO(verwaest): Allowlist contexts to which we have access.
           !map->is_access_check_needed();
  }
  return false;
}

#ifdef DEBUG
bool HasFieldRepresentationDependenciesOnMap(
    ZoneVector<CompilationDependency const*>& dependencies,
    Handle<Map> const& field_owner_map) {
  for (auto dep : dependencies) {
    if (dep->IsFieldRepresentationDependencyOnMap(field_owner_map)) {
      return true;
    }
  }
  return false;
}
#endif

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

ElementAccessInfo::ElementAccessInfo(
    ZoneVector<Handle<Map>>&& lookup_start_object_maps,
    ElementsKind elements_kind, Zone* zone)
    : elements_kind_(elements_kind),
      lookup_start_object_maps_(lookup_start_object_maps),
      transition_sources_(zone) {
  CHECK(!lookup_start_object_maps.empty());
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
  DCHECK_IMPLIES(
      field_representation.IsDouble(),
      HasFieldRepresentationDependenciesOnMap(dependencies, field_owner_map));
  return PropertyAccessInfo(kDataField, holder, transition_map, field_index,
                            field_representation, field_type, field_owner_map,
                            field_map, {{receiver_map}, zone},
                            std::move(dependencies));
}

// static
PropertyAccessInfo PropertyAccessInfo::FastDataConstant(
    Zone* zone, Handle<Map> receiver_map,
    ZoneVector<CompilationDependency const*>&& dependencies,
    FieldIndex field_index, Representation field_representation,
    Type field_type, Handle<Map> field_owner_map, MaybeHandle<Map> field_map,
    MaybeHandle<JSObject> holder, MaybeHandle<Map> transition_map) {
  return PropertyAccessInfo(kFastDataConstant, holder, transition_map,
                            field_index, field_representation, field_type,
                            field_owner_map, field_map, {{receiver_map}, zone},
                            std::move(dependencies));
}

// static
PropertyAccessInfo PropertyAccessInfo::FastAccessorConstant(
    Zone* zone, Handle<Map> receiver_map, Handle<Object> constant,
    MaybeHandle<JSObject> holder) {
  return PropertyAccessInfo(zone, kFastAccessorConstant, holder, constant,
                            MaybeHandle<Name>(), {{receiver_map}, zone});
}

// static
PropertyAccessInfo PropertyAccessInfo::ModuleExport(Zone* zone,
                                                    Handle<Map> receiver_map,
                                                    Handle<Cell> cell) {
  return PropertyAccessInfo(zone, kModuleExport, MaybeHandle<JSObject>(), cell,
                            MaybeHandle<Name>{}, {{receiver_map}, zone});
}

// static
PropertyAccessInfo PropertyAccessInfo::StringLength(Zone* zone,
                                                    Handle<Map> receiver_map) {
  return PropertyAccessInfo(zone, kStringLength, MaybeHandle<JSObject>(),
                            {{receiver_map}, zone});
}

// static
PropertyAccessInfo PropertyAccessInfo::DictionaryProtoDataConstant(
    Zone* zone, Handle<Map> receiver_map, Handle<JSObject> holder,
    InternalIndex dictionary_index, Handle<Name> name) {
  return PropertyAccessInfo(zone, kDictionaryProtoDataConstant, holder,
                            {{receiver_map}, zone}, dictionary_index, name);
}

// static
PropertyAccessInfo PropertyAccessInfo::DictionaryProtoAccessorConstant(
    Zone* zone, Handle<Map> receiver_map, MaybeHandle<JSObject> holder,
    Handle<Object> constant, Handle<Name> property_name) {
  return PropertyAccessInfo(zone, kDictionaryProtoAccessorConstant, holder,
                            constant, property_name, {{receiver_map}, zone});
}

// static
MinimorphicLoadPropertyAccessInfo MinimorphicLoadPropertyAccessInfo::DataField(
    int offset, bool is_inobject, Representation field_representation,
    Type field_type) {
  return MinimorphicLoadPropertyAccessInfo(kDataField, offset, is_inobject,
                                           field_representation, field_type);
}

// static
MinimorphicLoadPropertyAccessInfo MinimorphicLoadPropertyAccessInfo::Invalid() {
  return MinimorphicLoadPropertyAccessInfo(
      kInvalid, -1, false, Representation::None(), Type::None());
}

PropertyAccessInfo::PropertyAccessInfo(Zone* zone)
    : kind_(kInvalid),
      lookup_start_object_maps_(zone),
      unrecorded_dependencies_(zone),
      field_representation_(Representation::None()),
      field_type_(Type::None()),
      dictionary_index_(InternalIndex::NotFound()) {}

PropertyAccessInfo::PropertyAccessInfo(
    Zone* zone, Kind kind, MaybeHandle<JSObject> holder,
    ZoneVector<Handle<Map>>&& lookup_start_object_maps)
    : kind_(kind),
      lookup_start_object_maps_(lookup_start_object_maps),
      holder_(holder),
      unrecorded_dependencies_(zone),
      field_representation_(Representation::None()),
      field_type_(Type::None()),
      dictionary_index_(InternalIndex::NotFound()) {}

PropertyAccessInfo::PropertyAccessInfo(
    Zone* zone, Kind kind, MaybeHandle<JSObject> holder,
    Handle<Object> constant, MaybeHandle<Name> property_name,
    ZoneVector<Handle<Map>>&& lookup_start_object_maps)
    : kind_(kind),
      lookup_start_object_maps_(lookup_start_object_maps),
      constant_(constant),
      holder_(holder),
      unrecorded_dependencies_(zone),
      field_representation_(Representation::None()),
      field_type_(Type::Any()),
      dictionary_index_(InternalIndex::NotFound()),
      name_(property_name) {
  DCHECK_IMPLIES(kind == kDictionaryProtoAccessorConstant,
                 !property_name.is_null());
}
PropertyAccessInfo::PropertyAccessInfo(
    Kind kind, MaybeHandle<JSObject> holder, MaybeHandle<Map> transition_map,
    FieldIndex field_index, Representation field_representation,
    Type field_type, Handle<Map> field_owner_map, MaybeHandle<Map> field_map,
    ZoneVector<Handle<Map>>&& lookup_start_object_maps,
    ZoneVector<CompilationDependency const*>&& unrecorded_dependencies)
    : kind_(kind),
      lookup_start_object_maps_(lookup_start_object_maps),
      holder_(holder),
      unrecorded_dependencies_(std::move(unrecorded_dependencies)),
      transition_map_(transition_map),
      field_index_(field_index),
      field_representation_(field_representation),
      field_type_(field_type),
      field_owner_map_(field_owner_map),
      field_map_(field_map),
      dictionary_index_(InternalIndex::NotFound()) {
  DCHECK_IMPLIES(!transition_map.is_null(),
                 field_owner_map.address() == transition_map.address());
}

PropertyAccessInfo::PropertyAccessInfo(
    Zone* zone, Kind kind, MaybeHandle<JSObject> holder,
    ZoneVector<Handle<Map>>&& lookup_start_object_maps,
    InternalIndex dictionary_index, Handle<Name> name)
    : kind_(kind),
      lookup_start_object_maps_(lookup_start_object_maps),
      holder_(holder),
      unrecorded_dependencies_(zone),
      field_representation_(Representation::None()),
      field_type_(Type::Any()),
      dictionary_index_(dictionary_index),
      name_{name} {}

MinimorphicLoadPropertyAccessInfo::MinimorphicLoadPropertyAccessInfo(
    Kind kind, int offset, bool is_inobject,
    Representation field_representation, Type field_type)
    : kind_(kind),
      is_inobject_(is_inobject),
      offset_(offset),
      field_representation_(field_representation),
      field_type_(field_type) {}

bool PropertyAccessInfo::Merge(PropertyAccessInfo const* that,
                               AccessMode access_mode, Zone* zone) {
  if (this->kind_ != that->kind_) return false;
  if (this->holder_.address() != that->holder_.address()) return false;

  switch (this->kind_) {
    case kInvalid:
      return that->kind_ == kInvalid;

    case kDataField:
    case kFastDataConstant: {
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
        this->lookup_start_object_maps_.insert(
            this->lookup_start_object_maps_.end(),
            that->lookup_start_object_maps_.begin(),
            that->lookup_start_object_maps_.end());
        this->unrecorded_dependencies_.insert(
            this->unrecorded_dependencies_.end(),
            that->unrecorded_dependencies_.begin(),
            that->unrecorded_dependencies_.end());
        return true;
      }
      return false;
    }

    case kDictionaryProtoAccessorConstant:
    case kFastAccessorConstant: {
      // Check if we actually access the same constant.
      if (this->constant_.address() == that->constant_.address()) {
        DCHECK(this->unrecorded_dependencies_.empty());
        DCHECK(that->unrecorded_dependencies_.empty());
        this->lookup_start_object_maps_.insert(
            this->lookup_start_object_maps_.end(),
            that->lookup_start_object_maps_.begin(),
            that->lookup_start_object_maps_.end());
        return true;
      }
      return false;
    }

    case kDictionaryProtoDataConstant: {
      DCHECK_EQ(AccessMode::kLoad, access_mode);
      if (this->dictionary_index_ == that->dictionary_index_) {
        this->lookup_start_object_maps_.insert(
            this->lookup_start_object_maps_.end(),
            that->lookup_start_object_maps_.begin(),
            that->lookup_start_object_maps_.end());
        return true;
      }
      return false;
    }

    case kNotFound:
    case kStringLength: {
      DCHECK(this->unrecorded_dependencies_.empty());
      DCHECK(that->unrecorded_dependencies_.empty());
      this->lookup_start_object_maps_.insert(
          this->lookup_start_object_maps_.end(),
          that->lookup_start_object_maps_.begin(),
          that->lookup_start_object_maps_.end());
      return true;
    }
    case kModuleExport:
      return false;
  }
}

ConstFieldInfo PropertyAccessInfo::GetConstFieldInfo() const {
  if (IsFastDataConstant()) {
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
  Handle<DescriptorArray> descriptors(map->instance_descriptors(isolate()),
                                      isolate());
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
    unrecorded_dependencies.push_back(
        dependencies()->FieldRepresentationDependencyOffTheRecord(map_ref,
                                                                  descriptor));
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
  } else if (broker()->is_turboprop() && !map->is_prototype_map() &&
             !IsAnyStore(access_mode)) {
    // The constness feedback is too unstable for the aggresive compilation
    // of turboprop.
    constness = PropertyConstness::kMutable;
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
      return PropertyAccessInfo::FastDataConstant(
          zone(), receiver_map, std::move(unrecorded_dependencies), field_index,
          details_representation, field_type, field_owner_map, field_map,
          holder);
  }
  UNREACHABLE();
}

namespace {
using AccessorsObjectGetter = std::function<Handle<Object>()>;

PropertyAccessInfo AccessorAccessInfoHelper(
    Isolate* isolate, Zone* zone, JSHeapBroker* broker,
    const AccessInfoFactory* ai_factory, Handle<Map> receiver_map,
    Handle<Name> name, Handle<Map> map, MaybeHandle<JSObject> holder,
    AccessMode access_mode, AccessorsObjectGetter get_accessors) {
  if (map->instance_type() == JS_MODULE_NAMESPACE_TYPE) {
    DCHECK(map->is_prototype_map());
    Handle<PrototypeInfo> proto_info(PrototypeInfo::cast(map->prototype_info()),
                                     isolate);
    Handle<JSModuleNamespace> module_namespace(
        JSModuleNamespace::cast(proto_info->module_namespace()), isolate);
    Handle<Cell> cell(Cell::cast(module_namespace->module().exports().Lookup(
                          isolate, name, Smi::ToInt(name->GetHash()))),
                      isolate);
    if (cell->value().IsTheHole(isolate)) {
      // This module has not been fully initialized yet.
      return PropertyAccessInfo::Invalid(zone);
    }
    return PropertyAccessInfo::ModuleExport(zone, receiver_map, cell);
  }
  if (access_mode == AccessMode::kHas) {
    // kHas is not supported for dictionary mode objects.
    DCHECK(!map->is_dictionary_map());

    // HasProperty checks don't call getter/setters, existence is sufficient.
    return PropertyAccessInfo::FastAccessorConstant(zone, receiver_map,
                                                    Handle<Object>(), holder);
  }
  Handle<Object> accessors = get_accessors();
  if (!accessors->IsAccessorPair()) {
    return PropertyAccessInfo::Invalid(zone);
  }
  Handle<Object> accessor(access_mode == AccessMode::kLoad
                              ? Handle<AccessorPair>::cast(accessors)->getter()
                              : Handle<AccessorPair>::cast(accessors)->setter(),
                          isolate);
  if (!accessor->IsJSFunction()) {
    CallOptimization optimization(isolate, accessor);
    if (!optimization.is_simple_api_call() ||
        optimization.IsCrossContextLazyAccessorPair(
            *broker->target_native_context().object(), *map)) {
      return PropertyAccessInfo::Invalid(zone);
    }

    CallOptimization::HolderLookup lookup;
    holder = optimization.LookupHolderOfExpectedType(receiver_map, &lookup);
    if (lookup == CallOptimization::kHolderNotFound) {
      return PropertyAccessInfo::Invalid(zone);
    }
    DCHECK_IMPLIES(lookup == CallOptimization::kHolderIsReceiver,
                   holder.is_null());
    DCHECK_IMPLIES(lookup == CallOptimization::kHolderFound, !holder.is_null());
  }
  if (access_mode == AccessMode::kLoad) {
    Handle<Name> cached_property_name;
    if (FunctionTemplateInfo::TryGetCachedPropertyName(isolate, accessor)
            .ToHandle(&cached_property_name)) {
      PropertyAccessInfo access_info = ai_factory->ComputePropertyAccessInfo(
          map, cached_property_name, access_mode);
      if (!access_info.IsInvalid()) return access_info;
    }
  }
  if (map->is_dictionary_map()) {
    return PropertyAccessInfo::DictionaryProtoAccessorConstant(
        zone, receiver_map, holder, accessor, name);
  } else {
    return PropertyAccessInfo::FastAccessorConstant(zone, receiver_map,
                                                    accessor, holder);
  }
}

}  // namespace

PropertyAccessInfo AccessInfoFactory::ComputeAccessorDescriptorAccessInfo(
    Handle<Map> receiver_map, Handle<Name> name, Handle<Map> holder_map,
    MaybeHandle<JSObject> holder, InternalIndex descriptor,
    AccessMode access_mode) const {
  DCHECK(descriptor.is_found());
  Handle<DescriptorArray> descriptors(
      holder_map->instance_descriptors(kRelaxedLoad), isolate());
  SLOW_DCHECK(descriptor == descriptors->Search(*name, *holder_map));

  auto get_accessors = [&]() {
    return handle(descriptors->GetStrongValue(descriptor), isolate());
  };
  return AccessorAccessInfoHelper(isolate(), zone(), broker(), this,
                                  receiver_map, name, holder_map, holder,
                                  access_mode, get_accessors);
}

PropertyAccessInfo AccessInfoFactory::ComputeDictionaryProtoAccessInfo(
    Handle<Map> receiver_map, Handle<Name> name, Handle<JSObject> holder,
    InternalIndex dictionary_index, AccessMode access_mode,
    PropertyDetails details) const {
  CHECK(V8_DICT_PROPERTY_CONST_TRACKING_BOOL);
  DCHECK(holder->map().is_prototype_map());
  DCHECK_EQ(access_mode, AccessMode::kLoad);

  // We can only inline accesses to constant properties.
  if (details.constness() != PropertyConstness::kConst) {
    return PropertyAccessInfo::Invalid(zone());
  }

  if (details.kind() == PropertyKind::kData) {
    return PropertyAccessInfo::DictionaryProtoDataConstant(
        zone(), receiver_map, holder, dictionary_index, name);
  }

  auto get_accessors = [&]() {
    return JSObject::DictionaryPropertyAt(holder, dictionary_index);
  };
  Handle<Map> holder_map = handle(holder->map(), isolate());
  return AccessorAccessInfoHelper(isolate(), zone(), broker(), this,
                                  receiver_map, name, holder_map, holder,
                                  access_mode, get_accessors);
}

MinimorphicLoadPropertyAccessInfo AccessInfoFactory::ComputePropertyAccessInfo(
    MinimorphicLoadPropertyAccessFeedback const& feedback) const {
  DCHECK(feedback.handler()->IsSmi());
  int handler = Smi::cast(*feedback.handler()).value();
  bool is_inobject = LoadHandler::IsInobjectBits::decode(handler);
  bool is_double = LoadHandler::IsDoubleBits::decode(handler);
  int offset = LoadHandler::FieldIndexBits::decode(handler) * kTaggedSize;
  Representation field_rep =
      is_double ? Representation::Double() : Representation::Tagged();
  Type field_type = is_double ? Type::Number() : Type::Any();
  return MinimorphicLoadPropertyAccessInfo::DataField(offset, is_inobject,
                                                      field_rep, field_type);
}

bool AccessInfoFactory::TryLoadPropertyDetails(
    Handle<Map> map, MaybeHandle<JSObject> maybe_holder, Handle<Name> name,
    InternalIndex* index_out, PropertyDetails* details_out) const {
  if (map->is_dictionary_map()) {
    DCHECK(V8_DICT_PROPERTY_CONST_TRACKING_BOOL);
    DCHECK(map->is_prototype_map());

    DisallowGarbageCollection no_gc;

    if (maybe_holder.is_null()) {
      // TODO(v8:11457) In this situation, we have a dictionary mode prototype
      // as a receiver. Consider other means of obtaining the holder in this
      // situation.

      // Without the holder, we can't get the property details.
      return false;
    }

    Handle<JSObject> holder = maybe_holder.ToHandleChecked();
    if (V8_ENABLE_SWISS_NAME_DICTIONARY_BOOL) {
      SwissNameDictionary dict = holder->property_dictionary_swiss();
      *index_out = dict.FindEntry(isolate(), name);
      if (index_out->is_found()) {
        *details_out = dict.DetailsAt(*index_out);
      }
    } else {
      NameDictionary dict = holder->property_dictionary();
      *index_out = dict.FindEntry(isolate(), name);
      if (index_out->is_found()) {
        *details_out = dict.DetailsAt(*index_out);
      }
    }
  } else {
    DescriptorArray descriptors = map->instance_descriptors(kAcquireLoad);
    *index_out =
        descriptors.Search(*name, *map, broker()->is_concurrent_inlining());
    if (index_out->is_found()) {
      *details_out = descriptors.GetDetails(*index_out);
    }
  }

  return true;
}

PropertyAccessInfo AccessInfoFactory::ComputePropertyAccessInfo(
    Handle<Map> map, Handle<Name> name, AccessMode access_mode) const {
  CHECK(name->IsUniqueName());

  base::SharedMutexGuardIf<base::kShared> mutex_guard(
      isolate()->map_updater_access(), should_lock_mutex());
  MapUpdaterMutexDepthScope mumd_scope(this);

  if (access_mode == AccessMode::kHas && !map->IsJSReceiverMap()) {
    return PropertyAccessInfo::Invalid(zone());
  }

  // Check if it is safe to inline property access for the {map}.
  if (!CanInlinePropertyAccess(map, access_mode)) {
    return PropertyAccessInfo::Invalid(zone());
  }

  // We support fast inline cases for certain JSObject getters.
  if (access_mode == AccessMode::kLoad || access_mode == AccessMode::kHas) {
    PropertyAccessInfo access_info = LookupSpecialFieldAccessor(map, name);
    if (!access_info.IsInvalid()) return access_info;
  }

  // Only relevant if V8_DICT_PROPERTY_CONST_TRACKING enabled.
  bool dictionary_prototype_on_chain = false;
  bool fast_mode_prototype_on_chain = false;

  // Remember the receiver map. We use {map} as loop variable.
  Handle<Map> receiver_map = map;
  MaybeHandle<JSObject> holder;
  while (true) {
    PropertyDetails details = PropertyDetails::Empty();
    InternalIndex index = InternalIndex::NotFound();
    if (!TryLoadPropertyDetails(map, holder, name, &index, &details)) {
      return PropertyAccessInfo::Invalid(zone());
    }

    if (index.is_found()) {
      if (access_mode == AccessMode::kStore ||
          access_mode == AccessMode::kStoreInLiteral) {
        DCHECK(!map->is_dictionary_map());

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
      if (map->is_dictionary_map()) {
        DCHECK(V8_DICT_PROPERTY_CONST_TRACKING_BOOL);

        if (fast_mode_prototype_on_chain) {
          // TODO(v8:11248) While the work on dictionary mode prototypes is in
          // progress, we may still see fast mode objects on the chain prior to
          // reaching a dictionary mode prototype holding the property . Due to
          // this only being an intermediate state, we don't stupport these kind
          // of heterogenous prototype chains.
          return PropertyAccessInfo::Invalid(zone());
        }

        // TryLoadPropertyDetails only succeeds if we know the holder.
        return ComputeDictionaryProtoAccessInfo(receiver_map, name,
                                                holder.ToHandleChecked(), index,
                                                access_mode, details);
      }
      if (dictionary_prototype_on_chain) {
        // If V8_DICT_PROPERTY_CONST_TRACKING_BOOL was disabled, then a
        // dictionary prototype would have caused a bailout earlier.
        DCHECK(V8_DICT_PROPERTY_CONST_TRACKING_BOOL);

        // TODO(v8:11248) We have a fast mode holder, but there was a dictionary
        // mode prototype earlier on the chain. Note that seeing a fast mode
        // prototype even though V8_DICT_PROPERTY_CONST_TRACKING is enabled
        // should only be possible while the implementation of dictionary mode
        // prototypes is work in progress. Eventually, enabling
        // V8_DICT_PROPERTY_CONST_TRACKING will guarantee that all prototypes
        // are always in dictionary mode, making this case unreachable. However,
        // due to the complications of checking dictionary mode prototypes for
        // modification, we don't attempt to support dictionary mode prototypes
        // occuring before a fast mode holder on the chain.
        return PropertyAccessInfo::Invalid(zone());
      }
      if (details.location() == kField) {
        if (details.kind() == kData) {
          return ComputeDataFieldAccessInfo(receiver_map, map, holder, index,
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
                                                   holder, index, access_mode);
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

    if (V8_DICT_PROPERTY_CONST_TRACKING_BOOL && !holder.is_null()) {
      // At this point, we are past the first loop iteration.
      DCHECK(holder.ToHandleChecked()->map().is_prototype_map());
      DCHECK_NE(holder.ToHandleChecked()->map(), *receiver_map);

      fast_mode_prototype_on_chain =
          fast_mode_prototype_on_chain || !map->is_dictionary_map();
      dictionary_prototype_on_chain =
          dictionary_prototype_on_chain || map->is_dictionary_map();
    }

    // Walk up the prototype chain.
    MapRef(broker(), map).SerializePrototype();
    // Acquire synchronously the map's prototype's map to guarantee that every
    // time we use it, we use the same Map.
    Handle<Map> map_prototype_map(map->prototype().synchronized_map(),
                                  isolate());
    if (!map_prototype_map->IsJSObjectMap()) {
      // Perform the implicit ToObject for primitives here.
      // Implemented according to ES6 section 7.3.2 GetV (V, P).
      Handle<JSFunction> constructor;
      if (Map::GetConstructorFunction(
              map, broker()->target_native_context().object())
              .ToHandle(&constructor)) {
        map = handle(constructor->initial_map(), isolate());
        map_prototype_map =
            handle(map->prototype().synchronized_map(), isolate());
        DCHECK(map_prototype_map->IsJSObjectMap());
      } else if (map->prototype().IsNull()) {
        if (dictionary_prototype_on_chain) {
          // TODO(v8:11248) See earlier comment about
          // dictionary_prototype_on_chain. We don't support absent properties
          // with dictionary mode prototypes on the chain, either. This is again
          // just due to how we currently deal with dependencies for dictionary
          // properties during finalization.
          return PropertyAccessInfo::Invalid(zone());
        }

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

    holder = handle(JSObject::cast(map->prototype()), isolate());
    map = map_prototype_map;
    CHECK(!map->is_deprecated());

    if (!CanInlinePropertyAccess(map, access_mode)) {
      return PropertyAccessInfo::Invalid(zone());
    }

    // Successful lookup on prototype chain needs to guarantee that all the
    // prototypes up to the holder have stable maps, except for dictionary-mode
    // prototypes.
    CHECK_IMPLIES(!map->is_dictionary_map(), map->is_stable());
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
      TransitionsAccessor(isolate(), map, broker()->is_concurrent_inlining())
          .SearchTransition(*name, kData, NONE);
  if (transition.is_null()) {
    return PropertyAccessInfo::Invalid(zone());
  }

  Handle<Map> transition_map(transition, isolate());
  InternalIndex const number = transition_map->LastAdded();
  Handle<DescriptorArray> descriptors(
      transition_map->instance_descriptors(kAcquireLoad), isolate());
  PropertyDetails const details = descriptors->GetDetails(number);
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
    transition_map_ref.SerializeOwnDescriptor(number);
    unrecorded_dependencies.push_back(
        dependencies()->FieldRepresentationDependencyOffTheRecord(
            transition_map_ref, number));
  } else if (details_representation.IsHeapObject()) {
    // Extract the field type from the property details (make sure its
    // representation is TaggedPointer to reflect the heap object case).
    Handle<FieldType> descriptors_field_type(descriptors->GetFieldType(number),
                                             isolate());
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
  switch (dependencies()->DependOnFieldConstness(transition_map_ref, number)) {
    case PropertyConstness::kMutable:
      return PropertyAccessInfo::DataField(
          zone(), map, std::move(unrecorded_dependencies), field_index,
          details_representation, field_type, transition_map, field_map, holder,
          transition_map);
    case PropertyConstness::kConst:
      return PropertyAccessInfo::FastDataConstant(
          zone(), map, std::move(unrecorded_dependencies), field_index,
          details_representation, field_type, transition_map, field_map, holder,
          transition_map);
  }
  UNREACHABLE();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
