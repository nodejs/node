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
  virtual bool IsSane() const = 0;
  virtual bool IsValid() const = 0;
  virtual void Install(Isolate* isolate, Handle<WeakCell> code) = 0;
};

class InitialMapDependency final : public CompilationDependencies::Dependency {
 public:
  InitialMapDependency(const JSFunctionRef& function, const MapRef& initial_map)
      : function_(function), initial_map_(initial_map) {
    DCHECK(IsSane());
  }

  bool IsSane() const override {
    DisallowHeapAccess no_heap_access;
    CHECK(function_.has_initial_map());
    return function_.initial_map().equals(initial_map_);
  }

  bool IsValid() const override {
    Handle<JSFunction> function = function_.object<JSFunction>();
    return function->has_initial_map() &&
           function->initial_map() == *initial_map_.object<Map>();
  }

  void Install(Isolate* isolate, Handle<WeakCell> code) override {
    DCHECK(IsValid());
    DependentCode::InstallDependency(isolate, code, initial_map_.object<Map>(),
                                     DependentCode::kInitialMapChangedGroup);
  }

 private:
  JSFunctionRef function_;
  MapRef initial_map_;
};

class StableMapDependency final : public CompilationDependencies::Dependency {
 public:
  explicit StableMapDependency(const MapRef& map) : map_(map) {
    DCHECK(IsSane());
  }

  bool IsSane() const override {
    DisallowHeapAccess no_heap_access;
    return map_.is_stable();
  }

  bool IsValid() const override { return map_.object<Map>()->is_stable(); }

  void Install(Isolate* isolate, Handle<WeakCell> code) override {
    DCHECK(IsValid());
    DependentCode::InstallDependency(isolate, code, map_.object<Map>(),
                                     DependentCode::kPrototypeCheckGroup);
  }

 private:
  MapRef map_;
};

class TransitionDependency final : public CompilationDependencies::Dependency {
 public:
  explicit TransitionDependency(const MapRef& map) : map_(map) {
    DCHECK(IsSane());
  }

  bool IsSane() const override {
    DisallowHeapAccess no_heap_access;
    return !map_.is_deprecated();
  }

  bool IsValid() const override { return !map_.object<Map>()->is_deprecated(); }

  void Install(Isolate* isolate, Handle<WeakCell> code) override {
    DCHECK(IsValid());
    DependentCode::InstallDependency(isolate, code, map_.object<Map>(),
                                     DependentCode::kTransitionGroup);
  }

 private:
  MapRef map_;
};

class PretenureModeDependency final
    : public CompilationDependencies::Dependency {
 public:
  PretenureModeDependency(const AllocationSiteRef& site, PretenureFlag mode)
      : site_(site), mode_(mode) {
    DCHECK(IsSane());
  }

  bool IsSane() const override {
    DisallowHeapAccess no_heap_access;
    return mode_ == site_.GetPretenureMode();
  }

  bool IsValid() const override {
    return mode_ == site_.object<AllocationSite>()->GetPretenureMode();
  }

  void Install(Isolate* isolate, Handle<WeakCell> code) override {
    DCHECK(IsValid());
    DependentCode::InstallDependency(
        isolate, code, site_.object<AllocationSite>(),
        DependentCode::kAllocationSiteTenuringChangedGroup);
  }

 private:
  AllocationSiteRef site_;
  PretenureFlag mode_;
};

class FieldTypeDependency final : public CompilationDependencies::Dependency {
 public:
  FieldTypeDependency(const MapRef& owner, int descriptor,
                      const FieldTypeRef& type)
      : owner_(owner), descriptor_(descriptor), type_(type) {
    DCHECK(IsSane());
  }

  bool IsSane() const override {
    DisallowHeapAccess no_heap_access;
    CHECK(owner_.equals(owner_.FindFieldOwner(descriptor_)));
    return type_.equals(owner_.GetFieldType(descriptor_));
  }

  bool IsValid() const override {
    DisallowHeapAllocation no_heap_allocation;
    Handle<Map> owner = owner_.object<Map>();
    Handle<FieldType> type = type_.object<FieldType>();
    return *type == owner->instance_descriptors()->GetFieldType(descriptor_);
  }

  void Install(Isolate* isolate, Handle<WeakCell> code) override {
    DCHECK(IsValid());
    DependentCode::InstallDependency(isolate, code, owner_.object<Map>(),
                                     DependentCode::kFieldOwnerGroup);
  }

 private:
  MapRef owner_;
  int descriptor_;
  FieldTypeRef type_;
};

class GlobalPropertyDependency final
    : public CompilationDependencies::Dependency {
 public:
  GlobalPropertyDependency(const PropertyCellRef& cell, PropertyCellType type,
                           bool read_only)
      : cell_(cell), type_(type), read_only_(read_only) {
    DCHECK(IsSane());
  }

  bool IsSane() const override {
    DisallowHeapAccess no_heap_access;
    return type_ == cell_.property_details().cell_type() &&
           read_only_ == cell_.property_details().IsReadOnly();
  }

  bool IsValid() const override {
    Handle<PropertyCell> cell = cell_.object<PropertyCell>();
    return type_ == cell->property_details().cell_type() &&
           read_only_ == cell->property_details().IsReadOnly();
  }

  void Install(Isolate* isolate, Handle<WeakCell> code) override {
    DCHECK(IsValid());
    DependentCode::InstallDependency(isolate, code,
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
    DCHECK(IsSane());
  }

  bool IsSane() const override {
    DisallowHeapAccess no_heap_access;
    return cell_.value().IsSmi() &&
           cell_.value().AsSmi() == Isolate::kProtectorValid;
  }

  bool IsValid() const override {
    Handle<PropertyCell> cell = cell_.object<PropertyCell>();
    return cell->value() == Smi::FromInt(Isolate::kProtectorValid);
  }

  void Install(Isolate* isolate, Handle<WeakCell> code) override {
    DCHECK(IsValid());
    DependentCode::InstallDependency(isolate, code,
                                     cell_.object<PropertyCell>(),
                                     DependentCode::kPropertyCellChangedGroup);
  }

 private:
  PropertyCellRef cell_;
};

class ElementsKindDependency final
    : public CompilationDependencies::Dependency {
 public:
  ElementsKindDependency(const AllocationSiteRef& site, ElementsKind kind)
      : site_(site), kind_(kind) {
    DCHECK(IsSane());
  }

  bool IsSane() const override {
    DisallowHeapAccess no_heap_access;
    DCHECK(AllocationSite::ShouldTrack(kind_));
    ElementsKind kind = site_.PointsToLiteral()
                            ? site_.boilerplate().GetElementsKind()
                            : site_.GetElementsKind();
    return kind_ == kind;
  }

  bool IsValid() const override {
    Handle<AllocationSite> site = site_.object<AllocationSite>();
    ElementsKind kind = site->PointsToLiteral()
                            ? site->boilerplate()->GetElementsKind()
                            : site->GetElementsKind();
    return kind_ == kind;
  }

  void Install(Isolate* isolate, Handle<WeakCell> code) override {
    DCHECK(IsValid());
    DependentCode::InstallDependency(
        isolate, code, site_.object<AllocationSite>(),
        DependentCode::kAllocationSiteTransitionChangedGroup);
  }

 private:
  AllocationSiteRef site_;
  ElementsKind kind_;
};

MapRef CompilationDependencies::DependOnInitialMap(
    const JSFunctionRef& function) {
  MapRef map = function.initial_map();
  dependencies_.push_front(new (zone_) InitialMapDependency(function, map));
  return map;
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
  FieldTypeRef type = owner.GetFieldType(descriptor);
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
                          ? site.boilerplate().GetElementsKind()
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
  Isolate* isolate = code->GetIsolate();

  // Check validity of all dependencies first, such that we can abort before
  // installing anything.
  if (!AreValid()) {
    dependencies_.clear();
    return false;
  }

  Handle<WeakCell> cell = Code::WeakCellFor(code);
  for (auto dep : dependencies_) {
    dep->Install(isolate, cell);
  }
  dependencies_.clear();
  return true;
}

namespace {
void DependOnStablePrototypeChain(const JSHeapBroker* broker,
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
    const JSHeapBroker* broker, Handle<Context> native_context,
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

}  // namespace compiler
}  // namespace internal
}  // namespace v8
