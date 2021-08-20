// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_PROPERTY_ACCESS_BUILDER_H_
#define V8_COMPILER_PROPERTY_ACCESS_BUILDER_H_

#include <vector>

#include "src/codegen/machine-type.h"
#include "src/compiler/js-heap-broker.h"
#include "src/compiler/node.h"
#include "src/handles/handles.h"
#include "src/objects/map.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {
namespace compiler {

class CommonOperatorBuilder;
class CompilationDependencies;
class Graph;
class JSGraph;
class JSHeapBroker;
class PropertyAccessInfo;
class SimplifiedOperatorBuilder;
struct FieldAccess;

class PropertyAccessBuilder {
 public:
  PropertyAccessBuilder(JSGraph* jsgraph, JSHeapBroker* broker,
                        CompilationDependencies* dependencies)
      : jsgraph_(jsgraph), broker_(broker), dependencies_(dependencies) {}

  // Builds the appropriate string check if the maps are only string
  // maps.
  bool TryBuildStringCheck(JSHeapBroker* broker,
                           ZoneVector<Handle<Map>> const& maps, Node** receiver,
                           Node** effect, Node* control);
  // Builds a number check if all maps are number maps.
  bool TryBuildNumberCheck(JSHeapBroker* broker,
                           ZoneVector<Handle<Map>> const& maps, Node** receiver,
                           Node** effect, Node* control);

  // TODO(jgruber): Remove the untyped version once all uses are
  // updated.
  void BuildCheckMaps(Node* object, Node** effect, Node* control,
                      ZoneVector<Handle<Map>> const& maps);
  void BuildCheckMaps(Node* object, Effect* effect, Control control,
                      ZoneVector<Handle<Map>> const& maps) {
    Node* e = *effect;
    Node* c = control;
    BuildCheckMaps(object, &e, c, maps);
    *effect = e;
  }
  Node* BuildCheckValue(Node* receiver, Effect* effect, Control control,
                        Handle<HeapObject> value);

  // Builds the actual load for data-field and data-constant-field
  // properties (without heap-object or map checks).
  Node* BuildLoadDataField(NameRef const& name,
                           PropertyAccessInfo const& access_info,
                           Node* lookup_start_object, Node** effect,
                           Node** control);

  // Loads a constant value from a prototype object in dictionary mode and
  // constant-folds it.
  Node* FoldLoadDictPrototypeConstant(PropertyAccessInfo const& access_info);

  // Builds the load for data-field access for minimorphic loads that use
  // dynamic map checks. These cannot depend on any information from the maps.
  Node* BuildMinimorphicLoadDataField(
      NameRef const& name, MinimorphicLoadPropertyAccessInfo const& access_info,
      Node* lookup_start_object, Node** effect, Node** control);

  static MachineRepresentation ConvertRepresentation(
      Representation representation);

 private:
  JSGraph* jsgraph() const { return jsgraph_; }
  JSHeapBroker* broker() const { return broker_; }
  CompilationDependencies* dependencies() const { return dependencies_; }
  Graph* graph() const;
  Isolate* isolate() const;
  CommonOperatorBuilder* common() const;
  SimplifiedOperatorBuilder* simplified() const;

  Node* TryFoldLoadConstantDataField(NameRef const& name,
                                     PropertyAccessInfo const& access_info,
                                     Node* lookup_start_object);
  // Returns a node with the holder for the property access described by
  // {access_info}.
  Node* ResolveHolder(PropertyAccessInfo const& access_info,
                      Node* lookup_start_object);

  Node* BuildLoadDataField(NameRef const& name, Node* holder,
                           FieldAccess& field_access, bool is_inobject,
                           Node** effect, Node** control);

  JSGraph* jsgraph_;
  JSHeapBroker* broker_;
  CompilationDependencies* dependencies_;
};

bool HasOnlyStringMaps(JSHeapBroker* broker,
                       ZoneVector<Handle<Map>> const& maps);

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_PROPERTY_ACCESS_BUILDER_H_
