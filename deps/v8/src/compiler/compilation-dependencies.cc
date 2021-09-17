// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/compilation-dependencies.h"

#include "src/base/optional.h"
#include "src/compiler/compilation-dependency.h"
#include "src/execution/protectors.h"
#include "src/handles/handles-inl.h"
#include "src/objects/allocation-site-inl.h"
#include "src/objects/internal-index.h"
#include "src/objects/js-array-inl.h"
#include "src/objects/js-function-inl.h"
#include "src/objects/objects-inl.h"
#include "src/zone/zone-handle-set.h"

namespace v8 {
namespace internal {
namespace compiler {

CompilationDependencies::CompilationDependencies(JSHeapBroker* broker,
                                                 Zone* zone)
    : zone_(zone), broker_(broker), dependencies_(zone) {
  broker->set_dependencies(this);
}

class InitialMapDependency final : public CompilationDependency {
 public:
  InitialMapDependency(JSHeapBroker* broker, const JSFunctionRef& function,
                       const MapRef& initial_map)
      : function_(function), initial_map_(initial_map) {
  }

  bool IsValid() const override {
    Handle<JSFunction> function = function_.object();
    return function->has_initial_map() &&
           function->initial_map() == *initial_map_.object();
  }

  void Install(Handle<Code> code) const override {
    SLOW_DCHECK(IsValid());
    DependentCode::InstallDependency(function_.isolate(), code,
                                     initial_map_.object(),
                                     DependentCode::kInitialMapChangedGroup);
  }

 private:
  JSFunctionRef function_;
  MapRef initial_map_;
};

class PrototypePropertyDependency final : public CompilationDependency {
 public:
  PrototypePropertyDependency(JSHeapBroker* broker,
                              const JSFunctionRef& function,
                              const ObjectRef& prototype)
      : function_(function), prototype_(prototype) {
    DCHECK(function_.has_instance_prototype(broker->dependencies()));
    DCHECK(!function_.PrototypeRequiresRuntimeLookup(broker->dependencies()));
    DCHECK(function_.instance_prototype(broker->dependencies())
               .equals(prototype_));
  }

  bool IsValid() const override {
    Handle<JSFunction> function = function_.object();
    return function->has_prototype_slot() &&
           function->has_instance_prototype() &&
           !function->PrototypeRequiresRuntimeLookup() &&
           function->instance_prototype() == *prototype_.object();
  }

  void PrepareInstall() const override {
    SLOW_DCHECK(IsValid());
    Handle<JSFunction> function = function_.object();
    if (!function->has_initial_map()) JSFunction::EnsureHasInitialMap(function);
  }

  void Install(Handle<Code> code) const override {
    SLOW_DCHECK(IsValid());
    Handle<JSFunction> function = function_.object();
    CHECK(function->has_initial_map());
    Handle<Map> initial_map(function->initial_map(), function_.isolate());
    DependentCode::InstallDependency(function_.isolate(), code, initial_map,
                                     DependentCode::kInitialMapChangedGroup);
  }

 private:
  JSFunctionRef function_;
  ObjectRef prototype_;
};

class StableMapDependency final : public CompilationDependency {
 public:
  explicit StableMapDependency(const MapRef& map) : map_(map) {}

  bool IsValid() const override {
    // TODO(v8:11670): Consider turn this back into a CHECK inside the
    // constructor and DependOnStableMap, if possible in light of concurrent
    // heap state modifications.
    return !map_.object()->is_dictionary_map() && map_.object()->is_stable();
  }

  void Install(Handle<Code> code) const override {
    SLOW_DCHECK(IsValid());
    DependentCode::InstallDependency(map_.isolate(), code, map_.object(),
                                     DependentCode::kPrototypeCheckGroup);
  }

 private:
  MapRef map_;
};

class ConstantInDictionaryPrototypeChainDependency final
    : public CompilationDependency {
 public:
  explicit ConstantInDictionaryPrototypeChainDependency(
      const MapRef receiver_map, const NameRef property_name,
      const ObjectRef constant, PropertyKind kind)
      : receiver_map_(receiver_map),
        property_name_{property_name},
        constant_{constant},
        kind_{kind} {
    DCHECK(V8_DICT_PROPERTY_CONST_TRACKING_BOOL);
  }

  // Checks that |constant_| is still the value of accessing |property_name_|
  // starting at |receiver_map_|.
  bool IsValid() const override { return !GetHolderIfValid().is_null(); }

  void Install(Handle<Code> code) const override {
    SLOW_DCHECK(IsValid());
    Isolate* isolate = receiver_map_.isolate();
    Handle<JSObject> holder = GetHolderIfValid().ToHandleChecked();
    Handle<Map> map = receiver_map_.object();

    while (map->prototype() != *holder) {
      map = handle(map->prototype().map(), isolate);
      DCHECK(map->IsJSObjectMap());  // Due to IsValid holding.
      DependentCode::InstallDependency(isolate, code, map,
                                       DependentCode::kPrototypeCheckGroup);
    }

    DCHECK(map->prototype().map().IsJSObjectMap());  // Due to IsValid holding.
    DependentCode::InstallDependency(isolate, code,
                                     handle(map->prototype().map(), isolate),
                                     DependentCode::kPrototypeCheckGroup);
  }

 private:
  // If the dependency is still valid, returns holder of the constant. Otherwise
  // returns null.
  // TODO(neis) Currently, invoking IsValid and then Install duplicates the call
  // to GetHolderIfValid. Instead, consider letting IsValid change the state
  // (and store the holder), or merge IsValid and Install.
  MaybeHandle<JSObject> GetHolderIfValid() const {
    DisallowGarbageCollection no_gc;
    Isolate* isolate = receiver_map_.isolate();

    Handle<Object> holder;
    HeapObject prototype = receiver_map_.object()->prototype();

    enum class ValidationResult { kFoundCorrect, kFoundIncorrect, kNotFound };
    auto try_load = [&](auto dictionary) -> ValidationResult {
      InternalIndex entry =
          dictionary.FindEntry(isolate, property_name_.object());
      if (entry.is_not_found()) {
        return ValidationResult::kNotFound;
      }

      PropertyDetails details = dictionary.DetailsAt(entry);
      if (details.constness() != PropertyConstness::kConst) {
        return ValidationResult::kFoundIncorrect;
      }

      Object dictionary_value = dictionary.ValueAt(entry);
      Object value;
      // We must be able to detect the case that the property |property_name_|
      // of |holder_| was originally a plain function |constant_| (when creating
      // this dependency) and has since become an accessor whose getter is
      // |constant_|. Therefore, we cannot just look at the property kind of
      // |details|, because that reflects the current situation, not the one
      // when creating this dependency.
      if (details.kind() != kind_) {
        return ValidationResult::kFoundIncorrect;
      }
      if (kind_ == PropertyKind::kAccessor) {
        if (!dictionary_value.IsAccessorPair()) {
          return ValidationResult::kFoundIncorrect;
        }
        // Only supporting loading at the moment, so we only ever want the
        // getter.
        value = AccessorPair::cast(dictionary_value)
                    .get(AccessorComponent::ACCESSOR_GETTER);
      } else {
        value = dictionary_value;
      }
      return value == *constant_.object() ? ValidationResult::kFoundCorrect
                                          : ValidationResult::kFoundIncorrect;
    };

    while (prototype.IsJSObject()) {
      // We only care about JSObjects because that's the only type of holder
      // (and types of prototypes on the chain to the holder) that
      // AccessInfoFactory::ComputePropertyAccessInfo allows.
      JSObject object = JSObject::cast(prototype);

      // We only support dictionary mode prototypes on the chain for this kind
      // of dependency.
      CHECK(!object.HasFastProperties());

      ValidationResult result =
          V8_ENABLE_SWISS_NAME_DICTIONARY_BOOL
              ? try_load(object.property_dictionary_swiss())
              : try_load(object.property_dictionary());

      if (result == ValidationResult::kFoundCorrect) {
        return handle(object, isolate);
      } else if (result == ValidationResult::kFoundIncorrect) {
        return MaybeHandle<JSObject>();
      }

      // In case of kNotFound, continue walking up the chain.
      prototype = object.map().prototype();
    }

    return MaybeHandle<JSObject>();
  }

  MapRef receiver_map_;
  NameRef property_name_;
  ObjectRef constant_;
  PropertyKind kind_;
};

class OwnConstantDataPropertyDependency final : public CompilationDependency {
 public:
  OwnConstantDataPropertyDependency(JSHeapBroker* broker,
                                    const JSObjectRef& holder,
                                    const MapRef& map,
                                    Representation representation,
                                    FieldIndex index, const ObjectRef& value)
      : broker_(broker),
        holder_(holder),
        map_(map),
        representation_(representation),
        index_(index),
        value_(value) {}

  bool IsValid() const override {
    if (holder_.object()->map() != *map_.object()) {
      TRACE_BROKER_MISSING(broker_,
                           "Map change detected in " << holder_.object());
      return false;
    }
    DisallowGarbageCollection no_heap_allocation;
    Object current_value = holder_.object()->RawFastPropertyAt(index_);
    Object used_value = *value_.object();
    if (representation_.IsDouble()) {
      // Compare doubles by bit pattern.
      if (!current_value.IsHeapNumber() || !used_value.IsHeapNumber() ||
          HeapNumber::cast(current_value).value_as_bits(kRelaxedLoad) !=
              HeapNumber::cast(used_value).value_as_bits(kRelaxedLoad)) {
        TRACE_BROKER_MISSING(broker_,
                             "Constant Double property value changed in "
                                 << holder_.object() << " at FieldIndex "
                                 << index_.property_index());
        return false;
      }
    } else if (current_value != used_value) {
      TRACE_BROKER_MISSING(broker_, "Constant property value changed in "
                                        << holder_.object() << " at FieldIndex "
                                        << index_.property_index());
      return false;
    }
    return true;
  }

  void Install(Handle<Code> code) const override {}

 private:
  JSHeapBroker* const broker_;
  JSObjectRef const holder_;
  MapRef const map_;
  Representation const representation_;
  FieldIndex const index_;
  ObjectRef const value_;
};

class OwnConstantDictionaryPropertyDependency final
    : public CompilationDependency {
 public:
  OwnConstantDictionaryPropertyDependency(JSHeapBroker* broker,
                                          const JSObjectRef& holder,
                                          InternalIndex index,
                                          const ObjectRef& value)
      : broker_(broker),
        holder_(holder),
        map_(holder.map()),
        index_(index),
        value_(value) {
    // We depend on map() being cached.
    STATIC_ASSERT(ref_traits<JSObject>::ref_serialization_kind !=
                  RefSerializationKind::kNeverSerialized);
  }

  bool IsValid() const override {
    if (holder_.object()->map() != *map_.object()) {
      TRACE_BROKER_MISSING(broker_,
                           "Map change detected in " << holder_.object());
      return false;
    }

    base::Optional<Object> maybe_value = JSObject::DictionaryPropertyAt(
        holder_.object(), index_, broker_->isolate()->heap());

    if (!maybe_value) {
      TRACE_BROKER_MISSING(
          broker_, holder_.object()
                       << "has a value that might not safe to read at index "
                       << index_.as_int());
      return false;
    }

    if (*maybe_value != *value_.object()) {
      TRACE_BROKER_MISSING(broker_, "Constant property value changed in "
                                        << holder_.object()
                                        << " at InternalIndex "
                                        << index_.as_int());
      return false;
    }
    return true;
  }

  void Install(Handle<Code> code) const override {}

 private:
  JSHeapBroker* const broker_;
  JSObjectRef const holder_;
  MapRef const map_;
  InternalIndex const index_;
  ObjectRef const value_;
};

class ConsistentJSFunctionViewDependency final : public CompilationDependency {
 public:
  explicit ConsistentJSFunctionViewDependency(const JSFunctionRef& function)
      : function_(function) {}

  bool IsValid() const override {
    return function_.IsConsistentWithHeapState();
  }

  void Install(Handle<Code> code) const override {}

#ifdef DEBUG
  bool IsConsistentJSFunctionViewDependency() const override { return true; }
#endif

 private:
  const JSFunctionRef function_;
};

class TransitionDependency final : public CompilationDependency {
 public:
  explicit TransitionDependency(const MapRef& map) : map_(map) {
    DCHECK(map_.CanBeDeprecated());
  }

  bool IsValid() const override { return !map_.object()->is_deprecated(); }

  void Install(Handle<Code> code) const override {
    SLOW_DCHECK(IsValid());
    DependentCode::InstallDependency(map_.isolate(), code, map_.object(),
                                     DependentCode::kTransitionGroup);
  }

 private:
  MapRef map_;
};

class PretenureModeDependency final : public CompilationDependency {
 public:
  PretenureModeDependency(const AllocationSiteRef& site,
                          AllocationType allocation)
      : site_(site), allocation_(allocation) {}

  bool IsValid() const override {
    return allocation_ == site_.object()->GetAllocationType();
  }

  void Install(Handle<Code> code) const override {
    SLOW_DCHECK(IsValid());
    DependentCode::InstallDependency(
        site_.isolate(), code, site_.object(),
        DependentCode::kAllocationSiteTenuringChangedGroup);
  }

#ifdef DEBUG
  bool IsPretenureModeDependency() const override { return true; }
#endif

 private:
  AllocationSiteRef site_;
  AllocationType allocation_;
};

class FieldRepresentationDependency final : public CompilationDependency {
 public:
  FieldRepresentationDependency(const MapRef& map, InternalIndex descriptor,
                                Representation representation)
      : map_(map), descriptor_(descriptor), representation_(representation) {}

  bool IsValid() const override {
    DisallowGarbageCollection no_heap_allocation;
    if (map_.object()->is_deprecated()) return false;
    return representation_.Equals(map_.object()
                                      ->instance_descriptors(map_.isolate())
                                      .GetDetails(descriptor_)
                                      .representation());
  }

  void Install(Handle<Code> code) const override {
    SLOW_DCHECK(IsValid());
    Isolate* isolate = map_.isolate();
    Handle<Map> owner(map_.object()->FindFieldOwner(isolate, descriptor_),
                      isolate);
    CHECK(!owner->is_deprecated());
    CHECK(representation_.Equals(owner->instance_descriptors(isolate)
                                     .GetDetails(descriptor_)
                                     .representation()));
    DependentCode::InstallDependency(isolate, code, owner,
                                     DependentCode::kFieldRepresentationGroup);
  }

#ifdef DEBUG
  bool IsFieldRepresentationDependencyOnMap(
      Handle<Map> const& receiver_map) const override {
    return map_.object().equals(receiver_map);
  }
#endif

 private:
  MapRef map_;
  InternalIndex descriptor_;
  Representation representation_;
};

class FieldTypeDependency final : public CompilationDependency {
 public:
  FieldTypeDependency(const MapRef& map, InternalIndex descriptor,
                      const ObjectRef& type)
      : map_(map), descriptor_(descriptor), type_(type) {}

  bool IsValid() const override {
    DisallowGarbageCollection no_heap_allocation;
    if (map_.object()->is_deprecated()) return false;
    return *type_.object() == map_.object()
                                  ->instance_descriptors(map_.isolate())
                                  .GetFieldType(descriptor_);
  }

  void Install(Handle<Code> code) const override {
    SLOW_DCHECK(IsValid());
    Isolate* isolate = map_.isolate();
    Handle<Map> owner(map_.object()->FindFieldOwner(isolate, descriptor_),
                      isolate);
    CHECK(!owner->is_deprecated());
    CHECK_EQ(*type_.object(),
             owner->instance_descriptors(isolate).GetFieldType(descriptor_));
    DependentCode::InstallDependency(isolate, code, owner,
                                     DependentCode::kFieldTypeGroup);
  }

 private:
  MapRef map_;
  InternalIndex descriptor_;
  ObjectRef type_;
};

class FieldConstnessDependency final : public CompilationDependency {
 public:
  FieldConstnessDependency(const MapRef& map, InternalIndex descriptor)
      : map_(map), descriptor_(descriptor) {}

  bool IsValid() const override {
    DisallowGarbageCollection no_heap_allocation;
    if (map_.object()->is_deprecated()) return false;
    return PropertyConstness::kConst ==
           map_.object()
               ->instance_descriptors(map_.isolate())
               .GetDetails(descriptor_)
               .constness();
  }

  void Install(Handle<Code> code) const override {
    SLOW_DCHECK(IsValid());
    Isolate* isolate = map_.isolate();
    Handle<Map> owner(map_.object()->FindFieldOwner(isolate, descriptor_),
                      isolate);
    CHECK(!owner->is_deprecated());
    CHECK_EQ(PropertyConstness::kConst, owner->instance_descriptors(isolate)
                                            .GetDetails(descriptor_)
                                            .constness());
    DependentCode::InstallDependency(isolate, code, owner,
                                     DependentCode::kFieldConstGroup);
  }

 private:
  MapRef map_;
  InternalIndex descriptor_;
};

class GlobalPropertyDependency final : public CompilationDependency {
 public:
  GlobalPropertyDependency(const PropertyCellRef& cell, PropertyCellType type,
                           bool read_only)
      : cell_(cell), type_(type), read_only_(read_only) {
    DCHECK_EQ(type_, cell_.property_details().cell_type());
    DCHECK_EQ(read_only_, cell_.property_details().IsReadOnly());
  }

  bool IsValid() const override {
    Handle<PropertyCell> cell = cell_.object();
    // The dependency is never valid if the cell is 'invalidated'. This is
    // marked by setting the value to the hole.
    if (cell->value() == *(cell_.isolate()->factory()->the_hole_value())) {
      return false;
    }
    return type_ == cell->property_details().cell_type() &&
           read_only_ == cell->property_details().IsReadOnly();
  }

  void Install(Handle<Code> code) const override {
    SLOW_DCHECK(IsValid());
    DependentCode::InstallDependency(cell_.isolate(), code, cell_.object(),
                                     DependentCode::kPropertyCellChangedGroup);
  }

 private:
  PropertyCellRef cell_;
  PropertyCellType type_;
  bool read_only_;
};

class ProtectorDependency final : public CompilationDependency {
 public:
  explicit ProtectorDependency(const PropertyCellRef& cell) : cell_(cell) {}

  bool IsValid() const override {
    Handle<PropertyCell> cell = cell_.object();
    return cell->value() == Smi::FromInt(Protectors::kProtectorValid);
  }

  void Install(Handle<Code> code) const override {
    SLOW_DCHECK(IsValid());
    DependentCode::InstallDependency(cell_.isolate(), code, cell_.object(),
                                     DependentCode::kPropertyCellChangedGroup);
  }

 private:
  PropertyCellRef cell_;
};

class ElementsKindDependency final : public CompilationDependency {
 public:
  ElementsKindDependency(const AllocationSiteRef& site, ElementsKind kind)
      : site_(site), kind_(kind) {
    DCHECK(AllocationSite::ShouldTrack(kind_));
  }

  bool IsValid() const override {
    Handle<AllocationSite> site = site_.object();
    ElementsKind kind =
        site->PointsToLiteral()
            ? site->boilerplate(kAcquireLoad).map().elements_kind()
            : site->GetElementsKind();
    return kind_ == kind;
  }

  void Install(Handle<Code> code) const override {
    SLOW_DCHECK(IsValid());
    DependentCode::InstallDependency(
        site_.isolate(), code, site_.object(),
        DependentCode::kAllocationSiteTransitionChangedGroup);
  }

 private:
  AllocationSiteRef site_;
  ElementsKind kind_;
};

// Only valid if the holder can use direct reads, since validation uses
// GetOwnConstantElementFromHeap.
class OwnConstantElementDependency final : public CompilationDependency {
 public:
  OwnConstantElementDependency(const JSObjectRef& holder, uint32_t index,
                               const ObjectRef& element)
      : holder_(holder), index_(index), element_(element) {}

  bool IsValid() const override {
    DisallowGarbageCollection no_gc;
    JSObject holder = *holder_.object();
    base::Optional<Object> maybe_element =
        holder_.GetOwnConstantElementFromHeap(holder.elements(),
                                              holder.GetElementsKind(), index_);
    if (!maybe_element.has_value()) return false;

    return maybe_element.value() == *element_.object();
  }

  void Install(Handle<Code> code) const override {
    // This dependency has no effect after code finalization.
  }

 private:
  const JSObjectRef holder_;
  const uint32_t index_;
  const ObjectRef element_;
};

class InitialMapInstanceSizePredictionDependency final
    : public CompilationDependency {
 public:
  InitialMapInstanceSizePredictionDependency(const JSFunctionRef& function,
                                             int instance_size)
      : function_(function), instance_size_(instance_size) {}

  bool IsValid() const override {
    // The dependency is valid if the prediction is the same as the current
    // slack tracking result.
    if (!function_.object()->has_initial_map()) return false;
    int instance_size = function_.object()->ComputeInstanceSizeWithMinSlack(
        function_.isolate());
    return instance_size == instance_size_;
  }

  void PrepareInstall() const override {
    SLOW_DCHECK(IsValid());
    function_.object()->CompleteInobjectSlackTrackingIfActive();
  }

  void Install(Handle<Code> code) const override {
    SLOW_DCHECK(IsValid());
    DCHECK(
        !function_.object()->initial_map().IsInobjectSlackTrackingInProgress());
  }

 private:
  JSFunctionRef function_;
  int instance_size_;
};

void CompilationDependencies::RecordDependency(
    CompilationDependency const* dependency) {
  if (dependency != nullptr) dependencies_.push_front(dependency);
}

MapRef CompilationDependencies::DependOnInitialMap(
    const JSFunctionRef& function) {
  MapRef map = function.initial_map(this);
  RecordDependency(zone_->New<InitialMapDependency>(broker_, function, map));
  return map;
}

ObjectRef CompilationDependencies::DependOnPrototypeProperty(
    const JSFunctionRef& function) {
  ObjectRef prototype = function.instance_prototype(this);
  RecordDependency(
      zone_->New<PrototypePropertyDependency>(broker_, function, prototype));
  return prototype;
}

void CompilationDependencies::DependOnStableMap(const MapRef& map) {
  if (map.CanTransition()) {
    RecordDependency(zone_->New<StableMapDependency>(map));
  }
}

void CompilationDependencies::DependOnConstantInDictionaryPrototypeChain(
    const MapRef& receiver_map, const NameRef& property_name,
    const ObjectRef& constant, PropertyKind kind) {
  RecordDependency(zone_->New<ConstantInDictionaryPrototypeChainDependency>(
      receiver_map, property_name, constant, kind));
}

AllocationType CompilationDependencies::DependOnPretenureMode(
    const AllocationSiteRef& site) {
  if (!FLAG_allocation_site_pretenuring) return AllocationType::kYoung;
  AllocationType allocation = site.GetAllocationType();
  RecordDependency(zone_->New<PretenureModeDependency>(site, allocation));
  return allocation;
}

PropertyConstness CompilationDependencies::DependOnFieldConstness(
    const MapRef& map, InternalIndex descriptor) {
  PropertyConstness constness = map.GetPropertyDetails(descriptor).constness();
  if (constness == PropertyConstness::kMutable) return constness;

  // If the map can have fast elements transitions, then the field can be only
  // considered constant if the map does not transition.
  if (Map::CanHaveFastTransitionableElementsKind(map.instance_type())) {
    // If the map can already transition away, let us report the field as
    // mutable.
    if (!map.is_stable()) {
      return PropertyConstness::kMutable;
    }
    DependOnStableMap(map);
  }

  DCHECK_EQ(constness, PropertyConstness::kConst);
  RecordDependency(zone_->New<FieldConstnessDependency>(map, descriptor));
  return PropertyConstness::kConst;
}

void CompilationDependencies::DependOnGlobalProperty(
    const PropertyCellRef& cell) {
  PropertyCellType type = cell.property_details().cell_type();
  bool read_only = cell.property_details().IsReadOnly();
  RecordDependency(zone_->New<GlobalPropertyDependency>(cell, type, read_only));
}

bool CompilationDependencies::DependOnProtector(const PropertyCellRef& cell) {
  cell.CacheAsProtector();
  if (cell.value().AsSmi() != Protectors::kProtectorValid) return false;
  RecordDependency(zone_->New<ProtectorDependency>(cell));
  return true;
}

bool CompilationDependencies::DependOnArrayBufferDetachingProtector() {
  return DependOnProtector(MakeRef(
      broker_,
      broker_->isolate()->factory()->array_buffer_detaching_protector()));
}

bool CompilationDependencies::DependOnArrayIteratorProtector() {
  return DependOnProtector(MakeRef(
      broker_, broker_->isolate()->factory()->array_iterator_protector()));
}

bool CompilationDependencies::DependOnArraySpeciesProtector() {
  return DependOnProtector(MakeRef(
      broker_, broker_->isolate()->factory()->array_species_protector()));
}

bool CompilationDependencies::DependOnNoElementsProtector() {
  return DependOnProtector(
      MakeRef(broker_, broker_->isolate()->factory()->no_elements_protector()));
}

bool CompilationDependencies::DependOnPromiseHookProtector() {
  return DependOnProtector(MakeRef(
      broker_, broker_->isolate()->factory()->promise_hook_protector()));
}

bool CompilationDependencies::DependOnPromiseSpeciesProtector() {
  return DependOnProtector(MakeRef(
      broker_, broker_->isolate()->factory()->promise_species_protector()));
}

bool CompilationDependencies::DependOnPromiseThenProtector() {
  return DependOnProtector(MakeRef(
      broker_, broker_->isolate()->factory()->promise_then_protector()));
}

void CompilationDependencies::DependOnElementsKind(
    const AllocationSiteRef& site) {
  ElementsKind kind = site.PointsToLiteral()
                          ? site.boilerplate().value().map().elements_kind()
                          : site.GetElementsKind();
  if (AllocationSite::ShouldTrack(kind)) {
    RecordDependency(zone_->New<ElementsKindDependency>(site, kind));
  }
}

void CompilationDependencies::DependOnOwnConstantElement(
    const JSObjectRef& holder, uint32_t index, const ObjectRef& element) {
  // Only valid if the holder can use direct reads, since validation uses
  // GetOwnConstantElementFromHeap.
  DCHECK(holder.should_access_heap() || broker_->is_concurrent_inlining());
  RecordDependency(
      zone_->New<OwnConstantElementDependency>(holder, index, element));
}

void CompilationDependencies::DependOnOwnConstantDataProperty(
    const JSObjectRef& holder, const MapRef& map, Representation representation,
    FieldIndex index, const ObjectRef& value) {
  RecordDependency(zone_->New<OwnConstantDataPropertyDependency>(
      broker_, holder, map, representation, index, value));
}

void CompilationDependencies::DependOnOwnConstantDictionaryProperty(
    const JSObjectRef& holder, InternalIndex index, const ObjectRef& value) {
  RecordDependency(zone_->New<OwnConstantDictionaryPropertyDependency>(
      broker_, holder, index, value));
}

bool CompilationDependencies::Commit(Handle<Code> code) {
  for (auto dep : dependencies_) {
    if (!dep->IsValid()) {
      dependencies_.clear();
      return false;
    }
    dep->PrepareInstall();
  }

  DisallowCodeDependencyChange no_dependency_change;
  for (auto dep : dependencies_) {
    // Check each dependency's validity again right before installing it,
    // because the first iteration above might have invalidated some
    // dependencies. For example, PrototypePropertyDependency::PrepareInstall
    // can call EnsureHasInitialMap, which can invalidate a StableMapDependency
    // on the prototype object's map.
    if (!dep->IsValid()) {
      dependencies_.clear();
      return false;
    }
    dep->Install(code);
  }

  // It is even possible that a GC during the above installations invalidated
  // one of the dependencies. However, this should only affect
  //
  // 1. pretenure mode dependencies, or
  // 2. function consistency dependencies,
  //
  // which we assert below. It is safe to return successfully in these cases,
  // because
  //
  // 1. once the code gets executed it will do a stack check that triggers its
  //    deoptimization.
  // 2. since the function state was deemed consistent above, that means the
  //    compilation saw a self-consistent state of the jsfunction.
  if (FLAG_stress_gc_during_compilation) {
    broker_->isolate()->heap()->PreciseCollectAllGarbage(
        Heap::kForcedGC, GarbageCollectionReason::kTesting, kNoGCCallbackFlags);
  }
#ifdef DEBUG
  for (auto dep : dependencies_) {
    CHECK_IMPLIES(!dep->IsValid(),
                  dep->IsPretenureModeDependency() ||
                      dep->IsConsistentJSFunctionViewDependency());
  }
#endif

  dependencies_.clear();
  return true;
}

namespace {
// This function expects to never see a JSProxy.
void DependOnStablePrototypeChain(CompilationDependencies* deps, MapRef map,
                                  base::Optional<JSObjectRef> last_prototype) {
  while (true) {
    HeapObjectRef proto = map.prototype().value();
    if (!proto.IsJSObject()) {
      CHECK_EQ(proto.map().oddball_type(), OddballType::kNull);
      break;
    }
    map = proto.map();
    deps->DependOnStableMap(map);
    if (last_prototype.has_value() && proto.equals(*last_prototype)) break;
  }
}
}  // namespace

void CompilationDependencies::DependOnStablePrototypeChains(
    ZoneVector<MapRef> const& receiver_maps, WhereToStart start,
    base::Optional<JSObjectRef> last_prototype) {
  for (MapRef receiver_map : receiver_maps) {
    if (receiver_map.IsPrimitiveMap()) {
      // Perform the implicit ToObject for primitives here.
      // Implemented according to ES6 section 7.3.2 GetV (V, P).
      // Note: Keep sync'd with AccessInfoFactory::ComputePropertyAccessInfo.
      base::Optional<JSFunctionRef> constructor =
          broker_->target_native_context().GetConstructorFunction(receiver_map);
      receiver_map = constructor.value().initial_map(this);
    }
    if (start == kStartAtReceiver) DependOnStableMap(receiver_map);
    DependOnStablePrototypeChain(this, receiver_map, last_prototype);
  }
}

void CompilationDependencies::DependOnElementsKinds(
    const AllocationSiteRef& site) {
  AllocationSiteRef current = site;
  while (true) {
    DependOnElementsKind(current);
    if (!current.nested_site().IsAllocationSite()) break;
    current = current.nested_site().AsAllocationSite();
  }
  CHECK_EQ(current.nested_site().AsSmi(), 0);
}

void CompilationDependencies::DependOnConsistentJSFunctionView(
    const JSFunctionRef& function) {
  DCHECK(broker_->is_concurrent_inlining());
  RecordDependency(zone_->New<ConsistentJSFunctionViewDependency>(function));
}

SlackTrackingPrediction::SlackTrackingPrediction(MapRef initial_map,
                                                 int instance_size)
    : instance_size_(instance_size),
      inobject_property_count_(
          (instance_size >> kTaggedSizeLog2) -
          initial_map.GetInObjectPropertiesStartInWords()) {}

SlackTrackingPrediction
CompilationDependencies::DependOnInitialMapInstanceSizePrediction(
    const JSFunctionRef& function) {
  MapRef initial_map = DependOnInitialMap(function);
  int instance_size = function.InitialMapInstanceSizeWithMinSlack(this);
  // Currently, we always install the prediction dependency. If this turns out
  // to be too expensive, we can only install the dependency if slack
  // tracking is active.
  RecordDependency(zone_->New<InitialMapInstanceSizePredictionDependency>(
      function, instance_size));
  CHECK_LE(instance_size, function.initial_map(this).instance_size());
  return SlackTrackingPrediction(initial_map, instance_size);
}

CompilationDependency const*
CompilationDependencies::TransitionDependencyOffTheRecord(
    const MapRef& target_map) const {
  if (target_map.CanBeDeprecated()) {
    return zone_->New<TransitionDependency>(target_map);
  } else {
    DCHECK(!target_map.is_deprecated());
    return nullptr;
  }
}

CompilationDependency const*
CompilationDependencies::FieldRepresentationDependencyOffTheRecord(
    const MapRef& map, InternalIndex descriptor,
    Representation representation) const {
  return zone_->New<FieldRepresentationDependency>(map, descriptor,
                                                   representation);
}

CompilationDependency const*
CompilationDependencies::FieldTypeDependencyOffTheRecord(
    const MapRef& map, InternalIndex descriptor, const ObjectRef& type) const {
  return zone_->New<FieldTypeDependency>(map, descriptor, type);
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
