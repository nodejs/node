// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_PROPERTY_ACCESS_BUILDER_H_
#define V8_COMPILER_PROPERTY_ACCESS_BUILDER_H_

#include <vector>

#include "src/codegen/machine-type.h"
#include "src/compiler/js-heap-broker.h"
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
class Node;
class PropertyAccessInfo;
class SimplifiedOperatorBuilder;

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

  void BuildCheckMaps(Node* receiver, Node** effect, Node* control,
                      ZoneVector<Handle<Map>> const& receiver_maps);
  Node* BuildCheckValue(Node* receiver, Node** effect, Node* control,
                        Handle<HeapObject> value);

  // Builds the actual load for data-field and data-constant-field
  // properties (without heap-object or map checks).
  Node* BuildLoadDataField(NameRef const& name,
                           PropertyAccessInfo const& access_info,
                           Node* receiver, Node** effect, Node** control);

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

  Node* TryBuildLoadConstantDataField(NameRef const& name,
                                      PropertyAccessInfo const& access_info,
                                      Node* receiver);
  // Returns a node with the holder for the property access described by
  // {access_info}.
  Node* ResolveHolder(PropertyAccessInfo const& access_info, Node* receiver);

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
