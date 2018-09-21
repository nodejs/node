// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_PROPERTY_ACCESS_BUILDER_H_
#define V8_COMPILER_PROPERTY_ACCESS_BUILDER_H_

#include <vector>

#include "src/handles.h"
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
  PropertyAccessBuilder(JSGraph* jsgraph, JSHeapBroker* js_heap_broker,
                        CompilationDependencies* dependencies)
      : jsgraph_(jsgraph),
        js_heap_broker_(js_heap_broker),
        dependencies_(dependencies) {}

  // Builds the appropriate string check if the maps are only string
  // maps.
  bool TryBuildStringCheck(MapHandles const& maps, Node** receiver,
                           Node** effect, Node* control);
  // Builds a number check if all maps are number maps.
  bool TryBuildNumberCheck(MapHandles const& maps, Node** receiver,
                           Node** effect, Node* control);

  Node* BuildCheckHeapObject(Node* receiver, Node** effect, Node* control);
  void BuildCheckMaps(Node* receiver, Node** effect, Node* control,
                      std::vector<Handle<Map>> const& receiver_maps);
  Node* BuildCheckValue(Node* receiver, Node** effect, Node* control,
                        Handle<HeapObject> value);

  // Builds the actual load for data-field and data-constant-field
  // properties (without heap-object or map checks).
  Node* BuildLoadDataField(Handle<Name> name,
                           PropertyAccessInfo const& access_info,
                           Node* receiver, Node** effect, Node** control);

 private:
  JSGraph* jsgraph() const { return jsgraph_; }
  JSHeapBroker* js_heap_broker() const { return js_heap_broker_; }
  CompilationDependencies* dependencies() const { return dependencies_; }
  Graph* graph() const;
  Isolate* isolate() const;
  CommonOperatorBuilder* common() const;
  SimplifiedOperatorBuilder* simplified() const;

  Node* TryBuildLoadConstantDataField(Handle<Name> name,
                                      PropertyAccessInfo const& access_info,
                                      Node* receiver);
  // Returns a node with the holder for the property access described by
  // {access_info}.
  Node* ResolveHolder(PropertyAccessInfo const& access_info, Node* receiver);

  JSGraph* jsgraph_;
  JSHeapBroker* js_heap_broker_;
  CompilationDependencies* dependencies_;
};

bool HasOnlyStringMaps(MapHandles const& maps);

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_PROPERTY_ACCESS_BUILDER_H_
