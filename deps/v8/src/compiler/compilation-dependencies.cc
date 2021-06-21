// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/compilation-dependencies.h"

#include "src/compiler/compilation-dependency.h"
#include "src/execution/protectors.h"
#include "src/handles/handles-inl.h"
#include "src/objects/allocation-site-inl.h"
#include "src/objects/js-function-inl.h"
#include "src/objects/objects-inl.h"
#include "src/zone/zone-handle-set.h"

namespace v8 {
namespace internal {
namespace compiler {

CompilationDependencies::CompilationDependencies(JSHeapBroker* broker,
                                                 Zone* zone)
    : zone_(zone), broker_(broker), dependencies_(zone) {}

class InitialMapDependency final : public CompilationDependency {
 public:
  // TODO(neis): Once the concurrent compiler frontend is always-on, we no
  // longer need to explicitly store the initial map.
  InitialMapDependency(const JSFunctionRef& function, const MapRef& initial_map)
      : function_(function), initial_map_(initial_map) {
    DCHECK(function_.has_initial_map());
    DCHECK(function_.initial_map().equals(initial_map_));
  }

  bool IsValid() const override {
    Handle<JSFunction> function = function_.object();
    return function->has_initial_map() &&
           function->initial_map() == *initial_map_.object();
  }

  void Install(const MaybeObjectHandle& code) const override {
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
  // TODO(neis): Once the concurrent compiler frontend is always-on, we no
  // longer need to explicitly store the prototype.
  PrototypePropertyDependency(const JSFunctionRef& function,
                              const ObjectRef& prototype)
      : function_(function), prototype_(prototype) {
    DCHECK(function_.has_prototype());
    DCHECK(!function_.PrototypeRequiresRuntimeLookup());
    DCHECK(function_.prototype().equals(prototype_));
  }

  bool IsValid() const override {
    Handle<JSFunction> function = function_.object();
    return function->has_prototype_slot() && function->has_prototype() &&
           !function->PrototypeRequiresRuntimeLookup() &&
           function->prototype() == *prototype_.object();
  }

  void PrepareInstall() const override {
    SLOW_DCHECK(IsValid());
    Handle<JSFunction> function = function_.object();
    if (!function->has_initial_map()) JSFunction::EnsureHasInitialMap(function);
  }

  void Install(const MaybeObjectHandle& code) const override {
    SLOW_DCHECK(IsValid());
    Handle<JSFunction> function = function_.object();
    DCHECK(function->has_initial_map());
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
  explicit StableMapDependency(const MapRef& map) : map_(map) {
    DCHECK(map_.is_stable());
  }

  bool IsValid() const override { return map_.object()->is_stable(); }

  void Install(const MaybeObjectHandle& code) const override {
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

  void Install(const MaybeObjectHandle& code) const override {
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

class TransitionDependency final : public CompilationDependency {
 public:
  explicit TransitionDependency(const MapRef& map) : map_(map) {
    DCHECK(!map_.is_deprecated());
  }

  bool IsValid() const override { return !map_.object()->is_deprecated(); }

  void Install(const MaybeObjectHandle& code) const override {
    SLOW_DCHECK(IsValid());
    DependentCode::InstallDependency(map_.isolate(), code, map_.object(),
                                     DependentCode::kTransitionGroup);
  }

 private:
  MapRef map_;
};

class PretenureModeDependency final : public CompilationDependency {
 public:
  // TODO(neis): Once the concurrent compiler frontend is always-on, we no
  // longer need to explicitly store the mode.
  PretenureModeDependency(const AllocationSiteRef& site,
                          AllocationType allocation)
      : site_(site), allocation_(allocation) {
    DCHECK_EQ(allocation, site_.GetAllocationType());
  }

  bool IsValid() const override {
    return allocation_ == site_.object()->GetAllocationType();
  }

  void Install(const MaybeObjectHandle& code) const override {
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
  // TODO(neis): Once the concurrent compiler frontend is always-on, we no
  // longer need to explicitly store the representation.
  FieldRepresentationDependency(const MapRef& owner, InternalIndex descriptor,
                                Representation representation)
      : owner_(owner),
        descriptor_(descriptor),
        representation_(representation) {
    DCHECK(owner_.equals(owner_.FindFieldOwner(descriptor_)));
    DCHECK(representation_.Equals(
        owner_.GetPropertyDetails(descriptor_).representation()));
  }

  bool IsValid() const override {
    DisallowGarbageCollection no_heap_allocation;
    Handle<Map> owner = owner_.object();
    return representation_.Equals(owner->instance_descriptors(owner_.isolate())
                                      .GetDetails(descriptor_)
                                      .representation());
  }

  void Install(const MaybeObjectHandle& code) const override {
    SLOW_DCHECK(IsValid());
    DependentCode::InstallDependency(owner_.isolate(), code, owner_.object(),
                                     DependentCode::kFieldRepresentationGroup);
  }

#ifdef DEBUG
  bool IsFieldRepresentationDependencyOnMap(
      Handle<Map> const& receiver_map) const override {
    return owner_.object().equals(receiver_map);
  }
#endif

 private:
  MapRef owner_;
  InternalIndex descriptor_;
  Representation representation_;
};

class FieldTypeDependency final : public CompilationDependency {
 public:
  // TODO(neis): Once the concurrent compiler frontend is always-on, we no
  // longer need to explicitly store the type.
  FieldTypeDependency(const MapRef& owner, InternalIndex descriptor,
                      const ObjectRef& type)
      : owner_(owner), descriptor_(descriptor), type_(type) {
    DCHECK(owner_.equals(owner_.FindFieldOwner(descriptor_)));
    DCHECK(type_.equals(owner_.GetFieldType(descriptor_)));
  }

  bool IsValid() const override {
    DisallowGarbageCollection no_heap_allocation;
    Handle<Map> owner = owner_.object();
    Handle<Object> type = type_.object();
    return *type == owner->instance_descriptors(owner_.isolate())
                        .GetFieldType(descriptor_);
  }

  void Install(const MaybeObjectHandle& code) const override {
    SLOW_DCHECK(IsValid());
    DependentCode::InstallDependency(owner_.isolate(), code, owner_.object(),
                                     DependentCode::kFieldTypeGroup);
  }

 private:
  MapRef owner_;
  InternalIndex descriptor_;
  ObjectRef type_;
};

class FieldConstnessDependency final : public CompilationDependency {
 public:
  FieldConstnessDependency(const MapRef& owner, InternalIndex descriptor)
      : owner_(owner), descriptor_(descriptor) {
    DCHECK(owner_.equals(owner_.FindFieldOwner(descriptor_)));
    DCHECK_EQ(PropertyConstness::kConst,
              owner_.GetPropertyDetails(descriptor_).constness());
  }

  bool IsValid() const override {
    DisallowGarbageCollection no_heap_allocation;
    Handle<Map> owner = owner_.object();
    return PropertyConstness::kConst ==
           owner->instance_descriptors(owner_.isolate())
               .GetDetails(descriptor_)
               .constness();
  }

  void Install(const MaybeObjectHandle& code) const override {
    SLOW_DCHECK(IsValid());
    DependentCode::InstallDependency(owner_.isolate(), code, owner_.object(),
                                     DependentCode::kFieldConstGroup);
  }

 private:
  MapRef owner_;
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

  void Install(const MaybeObjectHandle& code) const override {
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
  explicit ProtectorDependency(const PropertyCellRef& cell) : cell_(cell) {
    DCHECK_EQ(cell_.value().AsSmi(), Protectors::kProtectorValid);
  }

  bool IsValid() const override {
    Handle<PropertyCell> cell = cell_.object();
    return cell->value() == Smi::FromInt(Protectors::kProtectorValid);
  }

  void Install(const MaybeObjectHandle& code) const override {
    SLOW_DCHECK(IsValid());
    DependentCode::InstallDependency(cell_.isolate(), code, cell_.object(),
                                     DependentCode::kPropertyCellChangedGroup);
  }

 private:
  PropertyCellRef cell_;
};

class ElementsKindDependency final : public CompilationDependency {
 public:
  // TODO(neis): Once the concurrent compiler frontend is always-on, we no
  // longer need to explicitly store the elements kind.
  ElementsKindDependency(const AllocationSiteRef& site, ElementsKind kind)
      : site_(site), kind_(kind) {
    DCHECK(AllocationSite::ShouldTrack(kind_));
    DCHECK_EQ(kind_, site_.PointsToLiteral()
                         ? site_.boilerplate().value().GetElementsKind()
                         : site_.GetElementsKind());
  }

  bool IsValid() const override {
    Handle<AllocationSite> site = site_.object();
    ElementsKind kind = site->PointsToLiteral()
                            ? site->boilerplate().GetElementsKind()
                            : site->GetElementsKind();
    return kind_ == kind;
  }

  void Install(const MaybeObjectHandle& code) const override {
    SLOW_DCHECK(IsValid());
    DependentCode::InstallDependency(
        site_.isolate(), code, site_.object(),
        DependentCode::kAllocationSiteTransitionChangedGroup);
  }

 private:
  AllocationSiteRef site_;
  ElementsKind kind_;
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

  void Install(const MaybeObjectHandle& code) const override {
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
  DCHECK(!function.IsNeverSerializedHeapObject());
  MapRef map = function.initial_map();
  RecordDependency(zone_->New<InitialMapDependency>(function, map));
  return map;
}

ObjectRef CompilationDependencies::DependOnPrototypeProperty(
    const JSFunctionRef& function) {
  DCHECK(!function.IsNeverSerializedHeapObject());
  ObjectRef prototype = function.prototype();
  RecordDependency(
      zone_->New<PrototypePropertyDependency>(function, prototype));
  return prototype;
}

void CompilationDependencies::DependOnStableMap(const MapRef& map) {
  DCHECK(!map.is_dictionary_map());
  DCHECK(!map.IsNeverSerializedHeapObject());
  if (map.CanTransition()) {
    RecordDependency(zone_->New<StableMapDependency>(map));
  } else {
    DCHECK(map.is_stable());
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
  DCHECK(!site.IsNeverSerializedHeapObject());
  AllocationType allocation = site.GetAllocationType();
  RecordDependency(zone_->New<PretenureModeDependency>(site, allocation));
  return allocation;
}

PropertyConstness CompilationDependencies::DependOnFieldConstness(
    const MapRef& map, InternalIndex descriptor) {
  DCHECK(!map.IsNeverSerializedHeapObject());
  MapRef owner = map.FindFieldOwner(descriptor);
  DCHECK(!owner.IsNeverSerializedHeapObject());
  PropertyConstness constness =
      owner.GetPropertyDetails(descriptor).constness();
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
  RecordDependency(zone_->New<FieldConstnessDependency>(owner, descriptor));
  return PropertyConstness::kConst;
}

void CompilationDependencies::DependOnGlobalProperty(
    const PropertyCellRef& cell) {
  PropertyCellType type = cell.property_details().cell_type();
  bool read_only = cell.property_details().IsReadOnly();
  RecordDependency(zone_->New<GlobalPropertyDependency>(cell, type, read_only));
}

bool CompilationDependencies::DependOnProtector(const PropertyCellRef& cell) {
  cell.SerializeAsProtector();
  if (cell.value().AsSmi() != Protectors::kProtectorValid) return false;
  RecordDependency(zone_->New<ProtectorDependency>(cell));
  return true;
}

bool CompilationDependencies::DependOnArrayBufferDetachingProtector() {
  return DependOnProtector(PropertyCellRef(
      broker_,
      broker_->isolate()->factory()->array_buffer_detaching_protector()));
}

bool CompilationDependencies::DependOnArrayIteratorProtector() {
  return DependOnProtector(PropertyCellRef(
      broker_, broker_->isolate()->factory()->array_iterator_protector()));
}

bool CompilationDependencies::DependOnArraySpeciesProtector() {
  return DependOnProtector(PropertyCellRef(
      broker_, broker_->isolate()->factory()->array_species_protector()));
}

bool CompilationDependencies::DependOnNoElementsProtector() {
  return DependOnProtector(PropertyCellRef(
      broker_, broker_->isolate()->factory()->no_elements_protector()));
}

bool CompilationDependencies::DependOnPromiseHookProtector() {
  return DependOnProtector(PropertyCellRef(
      broker_, broker_->isolate()->factory()->promise_hook_protector()));
}

bool CompilationDependencies::DependOnPromiseSpeciesProtector() {
  return DependOnProtector(PropertyCellRef(
      broker_, broker_->isolate()->factory()->promise_species_protector()));
}

bool CompilationDependencies::DependOnPromiseThenProtector() {
  return DependOnProtector(PropertyCellRef(
      broker_, broker_->isolate()->factory()->promise_then_protector()));
}

void CompilationDependencies::DependOnElementsKind(
    const AllocationSiteRef& site) {
  DCHECK(!site.IsNeverSerializedHeapObject());
  // Do nothing if the object doesn't have any useful element transitions left.
  ElementsKind kind = site.PointsToLiteral()
                          ? site.boilerplate().value().GetElementsKind()
                          : site.GetElementsKind();
  if (AllocationSite::ShouldTrack(kind)) {
    RecordDependency(zone_->New<ElementsKindDependency>(site, kind));
  }
}

bool CompilationDependencies::Commit(Handle<Code> code) {
  // Dependencies are context-dependent. In the future it may be possible to
  // restore them in the consumer native context, but for now they are
  // disabled.
  CHECK_IMPLIES(broker_->is_native_context_independent(),
                dependencies_.empty());

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
    dep->Install(MaybeObjectHandle::Weak(code));
  }

  // It is even possible that a GC during the above installations invalidated
  // one of the dependencies. However, this should only affect pretenure mode
  // dependencies, which we assert below. It is safe to return successfully in
  // these cases, because once the code gets executed it will do a stack check
  // that triggers its deoptimization.
  if (FLAG_stress_gc_during_compilation) {
    broker_->isolate()->heap()->PreciseCollectAllGarbage(
        Heap::kForcedGC, GarbageCollectionReason::kTesting, kNoGCCallbackFlags);
  }
#ifdef DEBUG
  for (auto dep : dependencies_) {
    CHECK_IMPLIES(!dep->IsValid(), dep->IsPretenureModeDependency());
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
    HeapObjectRef proto = map.prototype();
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

template <class MapContainer>
void CompilationDependencies::DependOnStablePrototypeChains(
    MapContainer const& receiver_maps, WhereToStart start,
    base::Optional<JSObjectRef> last_prototype) {
  for (auto map : receiver_maps) {
    MapRef receiver_map(broker_, map);
    if (start == kStartAtReceiver) DependOnStableMap(receiver_map);
    if (receiver_map.IsPrimitiveMap()) {
      // Perform the implicit ToObject for primitives here.
      // Implemented according to ES6 section 7.3.2 GetV (V, P).
      base::Optional<JSFunctionRef> constructor =
          broker_->target_native_context().GetConstructorFunction(receiver_map);
      if (constructor.has_value()) receiver_map = constructor->initial_map();
    }
    DependOnStablePrototypeChain(this, receiver_map, last_prototype);
  }
}
template void CompilationDependencies::DependOnStablePrototypeChains(
    ZoneVector<Handle<Map>> const& receiver_maps, WhereToStart start,
    base::Optional<JSObjectRef> last_prototype);
template void CompilationDependencies::DependOnStablePrototypeChains(
    ZoneHandleSet<Map> const& receiver_maps, WhereToStart start,
    base::Optional<JSObjectRef> last_prototype);

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
  int instance_size = function.InitialMapInstanceSizeWithMinSlack();
  // Currently, we always install the prediction dependency. If this turns out
  // to be too expensive, we can only install the dependency if slack
  // tracking is active.
  RecordDependency(zone_->New<InitialMapInstanceSizePredictionDependency>(
      function, instance_size));
  DCHECK_LE(instance_size, function.initial_map().instance_size());
  return SlackTrackingPrediction(initial_map, instance_size);
}

CompilationDependency const*
CompilationDependencies::TransitionDependencyOffTheRecord(
    const MapRef& target_map) const {
  DCHECK(!target_map.IsNeverSerializedHeapObject());
  if (target_map.CanBeDeprecated()) {
    return zone_->New<TransitionDependency>(target_map);
  } else {
    DCHECK(!target_map.is_deprecated());
    return nullptr;
  }
}

CompilationDependency const*
CompilationDependencies::FieldRepresentationDependencyOffTheRecord(
    const MapRef& map, InternalIndex descriptor) const {
  DCHECK(!map.IsNeverSerializedHeapObject());
  MapRef owner = map.FindFieldOwner(descriptor);
  DCHECK(!owner.IsNeverSerializedHeapObject());
  PropertyDetails details = owner.GetPropertyDetails(descriptor);
  DCHECK(details.representation().Equals(
      map.GetPropertyDetails(descriptor).representation()));
  return zone_->New<FieldRepresentationDependency>(owner, descriptor,
                                                   details.representation());
}

CompilationDependency const*
CompilationDependencies::FieldTypeDependencyOffTheRecord(
    const MapRef& map, InternalIndex descriptor) const {
  DCHECK(!map.IsNeverSerializedHeapObject());
  MapRef owner = map.FindFieldOwner(descriptor);
  DCHECK(!owner.IsNeverSerializedHeapObject());
  ObjectRef type = owner.GetFieldType(descriptor);
  DCHECK(type.equals(map.GetFieldType(descriptor)));
  return zone_->New<FieldTypeDependency>(owner, descriptor, type);
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
