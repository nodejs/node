// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_COMPILATION_DEPENDENCIES_H_
#define V8_COMPILER_COMPILATION_DEPENDENCIES_H_

#include "src/compiler/js-heap-broker.h"
#include "src/objects.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {
namespace compiler {

class SlackTrackingPrediction {
 public:
  SlackTrackingPrediction(MapRef initial_map, int instance_size);

  int inobject_property_count() const { return inobject_property_count_; }
  int instance_size() const { return instance_size_; }

 private:
  int instance_size_;
  int inobject_property_count_;
};

// Collects and installs dependencies of the code that is being generated.
class V8_EXPORT_PRIVATE CompilationDependencies : public ZoneObject {
 public:
  CompilationDependencies(Isolate* isolate, Zone* zone);

  V8_WARN_UNUSED_RESULT bool Commit(Handle<Code> code);

  // Return the initial map of {function} and record the assumption that it
  // stays the initial map.
  MapRef DependOnInitialMap(const JSFunctionRef& function);

  // Return the "prototype" property of the given function and record the
  // assumption that it doesn't change.
  ObjectRef DependOnPrototypeProperty(const JSFunctionRef& function);

  // Record the assumption that {map} stays stable.
  void DependOnStableMap(const MapRef& map);

  // Record the assumption that {target_map} can be transitioned to, i.e., that
  // it does not become deprecated.
  void DependOnTransition(const MapRef& target_map);

  // Return the pretenure mode of {site} and record the assumption that it does
  // not change.
  PretenureFlag DependOnPretenureMode(const AllocationSiteRef& site);

  // Record the assumption that the field type of a field does not change. The
  // field is identified by the arguments.
  void DependOnFieldType(const MapRef& map, int descriptor);

  // Record the assumption that neither {cell}'s {CellType} changes, nor the
  // {IsReadOnly()} flag of {cell}'s {PropertyDetails}.
  void DependOnGlobalProperty(const PropertyCellRef& cell);

  // Record the assumption that the protector remains valid.
  void DependOnProtector(const PropertyCellRef& cell);

  // Record the assumption that {site}'s {ElementsKind} doesn't change.
  void DependOnElementsKind(const AllocationSiteRef& site);

  // Depend on the stability of (the maps of) all prototypes of every class in
  // {receiver_type} up to (and including) the {holder}.
  void DependOnStablePrototypeChains(
      JSHeapBroker* broker, std::vector<Handle<Map>> const& receiver_maps,
      const JSObjectRef& holder);

  // Like DependOnElementsKind but also applies to all nested allocation sites.
  void DependOnElementsKinds(const AllocationSiteRef& site);

  // Predict the final instance size for {function}'s initial map and record
  // the assumption that this prediction is correct. In addition, register
  // the initial map dependency. This method returns the {function}'s the
  // predicted minimum slack instance size count (wrapped together with
  // the corresponding in-object property count for convenience).
  SlackTrackingPrediction DependOnInitialMapInstanceSizePrediction(
      const JSFunctionRef& function);

  // Exposed only for testing purposes.
  bool AreValid() const;

  // Exposed only because C++.
  class Dependency;

 private:
  Zone* zone_;
  ZoneForwardList<Dependency*> dependencies_;
  Isolate* isolate_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_COMPILATION_DEPENDENCIES_H_
