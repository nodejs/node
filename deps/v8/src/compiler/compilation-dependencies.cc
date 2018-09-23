// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/compilation-dependencies.h"

#include "src/handles-inl.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {
namespace compiler {

CompilationDependencies::CompilationDependencies(Isolate* isolate, Zone* zone)
    : zone_(zone), dependencies_(zone) {}

class CompilationDependencies::Dependency : public ZoneObject {
 public:
  virtual bool IsValid() const = 0;
  virtual void Install(MaybeObjectHandle code) = 0;
};

class InitialMapDependency final : public CompilationDependencies::Dependency {
 public:
  // TODO(neis): Once the concurrent compiler frontend is always-on, we no
  // longer need to explicitly store the initial map.
  InitialMapDependency(const JSFunctionRef& function, const MapRef& initial_map)
      : function_(function), initial_map_(initial_map) {
    DCHECK(function_.has_initial_map());
    DCHECK(function_.initial_map().equals(initial_map_));
  }

  bool IsValid() const override {
    Handle<JSFunction> function = function_.object<JSFunction>();
    return function->has_initial_map() &&
           function->initial_map() == *initial_map_.object<Map>();
  }

  void Install(MaybeObjectHandle code) override {
    SLOW_DCHECK(IsValid());
    DependentCode::InstallDependency(function_.isolate(), code,
                                     initial_map_.object<Map>(),
                                     DependentCode::kInitialMapChangedGroup);
  }

 private:
  JSFunctionRef function_;
  MapRef initial_map_;
};

class PrototypePropertyDependency final
    : public CompilationDependencies::Dependency {
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
    Handle<JSFunction> function = function_.object<JSFunction>();
    return function->has_prototype_slot() && function->has_prototype() &&
           !function->PrototypeRequiresRuntimeLookup() &&
           function->prototype() == *prototype_.object();
  }

  void Install(MaybeObjectHandle code) override {
    SLOW_DCHECK(IsValid());
    Handle<JSFunction> function = function_.object<JSFunction>();
    if (!function->has_initial_map()) JSFunction::EnsureHasInitialMap(function);
    Handle<Map> initial_map(function->initial_map(), function_.isolate());
    DependentCode::InstallDependency(function_.isolate(), code, initial_map,
                                     DependentCode::kInitialMapChangedGroup);
  }

 private:
  JSFunctionRef function_;
  ObjectRef prototype_;
};

class StableMapDependency final : public CompilationDependencies::Dependency {
 public:
  explicit StableMapDependency(const MapRef& map) : map_(map) {
    DCHECK(map_.is_stable());
  }

  bool IsValid() const override { return map_.object<Map>()->is_stable(); }

  void Install(MaybeObjectHandle code) override {
    SLOW_DCHECK(IsValid());
    DependentCode::InstallDependency(map_.isolate(), code, map_.object<Map>(),
                                     DependentCode::kPrototypeCheckGroup);
  }

 private:
  MapRef map_;
};

class TransitionDependency final : public CompilationDependencies::Dependency {
 public:
  explicit TransitionDependency(const MapRef& map) : map_(map) {
    DCHECK(!map_.is_deprecated());
  }

  bool IsValid() const override { return !map_.object<Map>()->is_deprecated(); }

  void Install(MaybeObjectHandle code) override {
    SLOW_DCHECK(IsValid());
    DependentCode::InstallDependency(map_.isolate(), code, map_.object<Map>(),
                                     DependentCode::kTransitionGroup);
  }

 private:
  MapRef map_;
};

class PretenureModeDependency final
    : public CompilationDependencies::Dependency {
 public:
  // TODO(neis): Once the concurrent compiler frontend is always-on, we no
  // longer need to explicitly store the mode.
  PretenureModeDependency(const AllocationSiteRef& site, PretenureFlag mode)
      : site_(site), mode_(mode) {
    DCHECK_EQ(mode_, site_.GetPretenureMode());
  }

  bool IsValid() const override {
    return mode_ == site_.object<AllocationSite>()->GetPretenureMode();
  }

  void Install(MaybeObjectHandle code) override {
    SLOW_DCHECK(IsValid());
    DependentCode::InstallDependency(
        site_.isolate(), code, site_.object<AllocationSite>(),
        DependentCode::kAllocationSiteTenuringChangedGroup);
  }

 private:
  AllocationSiteRef site_;
  PretenureFlag mode_;
};

class FieldTypeDependency final : public CompilationDependencies::Dependency {
 public:
  // TODO(neis): Once the concurrent compiler frontend is always-on, we no
  // longer need to explicitly store the type.
  FieldTypeDependency(const MapRef& owner, int descriptor,
                      const ObjectRef& type)
      : owner_(owner), descriptor_(descriptor), type_(type) {
    DCHECK(owner_.equals(owner_.FindFieldOwner(descriptor_)));
    DCHECK(type_.equals(owner_.GetFieldType(descriptor_)));
  }

  bool IsValid() const override {
    DisallowHeapAllocation no_heap_allocation;
    Handle<Map> owner = owner_.object<Map>();
    Handle<FieldType> type = type_.object<FieldType>();
    return *type == owner->instance_descriptors()->GetFieldType(descriptor_);
  }

  void Install(MaybeObjectHandle code) override {
    SLOW_DCHECK(IsValid());
    DependentCode::InstallDependency(owner_.isolate(), code,
                                     owner_.object<Map>(),
                                     DependentCode::kFieldOwnerGroup);
  }

 private:
  MapRef owner_;
  int descriptor_;
  ObjectRef type_;
};

class GlobalPropertyDependency final
    : public CompilationDependencies::Dependency {
 public:
  // TODO(neis): Once the concurrent compiler frontend is always-on, we no
  // longer need to explicitly store the type and the read_only flag.
  GlobalPropertyDependency(const PropertyCellRef& cell, PropertyCellType type,
                           bool read_only)
      : cell_(cell), type_(type), read_only_(read_only) {
    DCHECK_EQ(type_, cell_.property_details().cell_type());
    DCHECK_EQ(read_only_, cell_.property_details().IsReadOnly());
  }

  bool IsValid() const override {
    Handle<PropertyCell> cell = cell_.object<PropertyCell>();
    return type_ == cell->property_details().cell_type() &&
           read_only_ == cell->property_details().IsReadOnly();
  }

  void Install(MaybeObjectHandle code) override {
    SLOW_DCHECK(IsValid());
    DependentCode::InstallDependency(cell_.isolate(), code,
                                     cell_.object<PropertyCell>(),
                                     DependentCode::kPropertyCellChangedGroup);
  }

 private:
  PropertyCellRef cell_;
  PropertyCellType type_;
  bool read_only_;
};

class ProtectorDependency final : public CompilationDependencies::Dependency {
 public:
  explicit ProtectorDependency(const PropertyCellRef& cell) : cell_(cell) {
    DCHECK_EQ(cell_.value().AsSmi(), Isolate::kProtectorValid);
  }

  bool IsValid() const override {
    Handle<PropertyCell> cell = cell_.object<PropertyCell>();
    return cell->value() == Smi::FromInt(Isolate::kProtectorValid);
  }

  void Install(MaybeObjectHandle code) override {
    SLOW_DCHECK(IsValid());
    DependentCode::InstallDependency(cell_.isolate(), code,
                                     cell_.object<PropertyCell>(),
                                     DependentCode::kPropertyCellChangedGroup);
  }

 private:
  PropertyCellRef cell_;
};

class ElementsKindDependency final
    : public CompilationDependencies::Dependency {
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
    Handle<AllocationSite> site = site_.object<AllocationSite>();
    ElementsKind kind = site->PointsToLiteral()
                            ? site->boilerplate()->GetElementsKind()
                            : site->GetElementsKind();
    return kind_ == kind;
  }

  void Install(MaybeObjectHandle code) override {
    SLOW_DCHECK(IsValid());
    DependentCode::InstallDependency(
        site_.isolate(), code, site_.object<AllocationSite>(),
        DependentCode::kAllocationSiteTransitionChangedGroup);
  }

 private:
  AllocationSiteRef site_;
  ElementsKind kind_;
};

class InitialMapInstanceSizePredictionDependency final
    : public CompilationDependencies::Dependency {
 public:
  InitialMapInstanceSizePredictionDependency(const JSFunctionRef& function,
                                             int instance_size)
      : function_(function), instance_size_(instance_size) {}

  bool IsValid() const override {
    // The dependency is valid if the prediction is the same as the current
    // slack tracking result.
    int instance_size =
        function_.object<JSFunction>()->ComputeInstanceSizeWithMinSlack(
            function_.isolate());
    return instance_size == instance_size_;
  }

  void Install(MaybeObjectHandle code) override {
    DCHECK(IsValid());
    // Finish the slack tracking.
    function_.object<JSFunction>()->CompleteInobjectSlackTrackingIfActive();
  }

 private:
  JSFunctionRef function_;
  int instance_size_;
};

MapRef CompilationDependencies::DependOnInitialMap(
    const JSFunctionRef& function) {
  MapRef map = function.initial_map();
  dependencies_.push_front(new (zone_) InitialMapDependency(function, map));
  return map;
}

ObjectRef CompilationDependencies::DependOnPrototypeProperty(
    const JSFunctionRef& function) {
  ObjectRef prototype = function.prototype();
  dependencies_.push_front(
      new (zone_) PrototypePropertyDependency(function, prototype));
  return prototype;
}

void CompilationDependencies::DependOnStableMap(const MapRef& map) {
  if (map.CanTransition()) {
    dependencies_.push_front(new (zone_) StableMapDependency(map));
  } else {
    DCHECK(map.is_stable());
  }
}

void CompilationDependencies::DependOnTransition(const MapRef& target_map) {
  if (target_map.CanBeDeprecated()) {
    dependencies_.push_front(new (zone_) TransitionDependency(target_map));
  } else {
    DCHECK(!target_map.is_deprecated());
  }
}

PretenureFlag CompilationDependencies::DependOnPretenureMode(
    const AllocationSiteRef& site) {
  PretenureFlag mode = site.GetPretenureMode();
  dependencies_.push_front(new (zone_) PretenureModeDependency(site, mode));
  return mode;
}

void CompilationDependencies::DependOnFieldType(const MapRef& map,
                                                int descriptor) {
  MapRef owner = map.FindFieldOwner(descriptor);
  ObjectRef type = owner.GetFieldType(descriptor);
  DCHECK(type.equals(map.GetFieldType(descriptor)));
  dependencies_.push_front(new (zone_)
                               FieldTypeDependency(owner, descriptor, type));
}

void CompilationDependencies::DependOnGlobalProperty(
    const PropertyCellRef& cell) {
  PropertyCellType type = cell.property_details().cell_type();
  bool read_only = cell.property_details().IsReadOnly();
  dependencies_.push_front(new (zone_)
                               GlobalPropertyDependency(cell, type, read_only));
}

void CompilationDependencies::DependOnProtector(const PropertyCellRef& cell) {
  dependencies_.push_front(new (zone_) ProtectorDependency(cell));
}

void CompilationDependencies::DependOnElementsKind(
    const AllocationSiteRef& site) {
  // Do nothing if the object doesn't have any useful element transitions left.
  ElementsKind kind = site.PointsToLiteral()
                          ? site.boilerplate().value().GetElementsKind()
                          : site.GetElementsKind();
  if (AllocationSite::ShouldTrack(kind)) {
    dependencies_.push_front(new (zone_) ElementsKindDependency(site, kind));
  }
}

bool CompilationDependencies::AreValid() const {
  for (auto dep : dependencies_) {
    if (!dep->IsValid()) return false;
  }
  return true;
}

bool CompilationDependencies::Commit(Handle<Code> code) {
  // Check validity of all dependencies first, such that we can avoid installing
  // anything when there's already an invalid dependency.
  if (!AreValid()) {
    dependencies_.clear();
    return false;
  }

  for (auto dep : dependencies_) {
    // Check each dependency's validity again right before installing it,
    // because a GC can trigger invalidation for some dependency kinds.
    if (!dep->IsValid()) {
      dependencies_.clear();
      return false;
    }
    dep->Install(MaybeObjectHandle::Weak(code));
  }
  dependencies_.clear();
  return true;
}

namespace {
void DependOnStablePrototypeChain(JSHeapBroker* broker,
                                  CompilationDependencies* deps,
                                  Handle<Map> map,
                                  MaybeHandle<JSReceiver> last_prototype) {
  for (PrototypeIterator i(broker->isolate(), map); !i.IsAtEnd(); i.Advance()) {
    Handle<JSReceiver> const current =
        PrototypeIterator::GetCurrent<JSReceiver>(i);
    deps->DependOnStableMap(
        MapRef(broker, handle(current->map(), broker->isolate())));
    Handle<JSReceiver> last;
    if (last_prototype.ToHandle(&last) && last.is_identical_to(current)) {
      break;
    }
  }
}
}  // namespace

void CompilationDependencies::DependOnStablePrototypeChains(
    JSHeapBroker* broker, Handle<Context> native_context,
    std::vector<Handle<Map>> const& receiver_maps, Handle<JSObject> holder) {
  Isolate* isolate = holder->GetIsolate();
  // Determine actual holder and perform prototype chain checks.
  for (auto map : receiver_maps) {
    // Perform the implicit ToObject for primitives here.
    // Implemented according to ES6 section 7.3.2 GetV (V, P).
    Handle<JSFunction> constructor;
    if (Map::GetConstructorFunction(map, native_context)
            .ToHandle(&constructor)) {
      map = handle(constructor->initial_map(), isolate);
    }
    DependOnStablePrototypeChain(broker, this, map, holder);
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

SlackTrackingPrediction::SlackTrackingPrediction(MapRef initial_map,
                                                 int instance_size)
    : instance_size_(instance_size),
      inobject_property_count_(
          (instance_size >> kPointerSizeLog2) -
          initial_map.GetInObjectPropertiesStartInWords()) {}

SlackTrackingPrediction
CompilationDependencies::DependOnInitialMapInstanceSizePrediction(
    const JSFunctionRef& function) {
  MapRef initial_map = DependOnInitialMap(function);
  int instance_size = function.InitialMapInstanceSizeWithMinSlack();
  // Currently, we always install the prediction dependency. If this turns out
  // to be too expensive, we can only install the dependency if slack
  // tracking is active.
  dependencies_.push_front(
      new (zone_)
          InitialMapInstanceSizePredictionDependency(function, instance_size));
  DCHECK_LE(instance_size, function.initial_map().instance_size());
  return SlackTrackingPrediction(initial_map, instance_size);
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
