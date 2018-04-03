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

class CompilationDependencies;

namespace compiler {

class CommonOperatorBuilder;
class Graph;
class JSGraph;
class Node;
class PropertyAccessInfo;
class SimplifiedOperatorBuilder;

class PropertyAccessBuilder {
 public:
  PropertyAccessBuilder(JSGraph* jsgraph, CompilationDependencies* dependencies)
      : jsgraph_(jsgraph), dependencies_(dependencies) {}

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

  // Adds stability dependencies on all prototypes of every class in
  // {receiver_type} up to (and including) the {holder}.
  void AssumePrototypesStable(Handle<Context> native_context,
                              std::vector<Handle<Map>> const& receiver_maps,
                              Handle<JSObject> holder);

  // Builds the actual load for data-field and data-constant-field
  // properties (without heap-object or map checks).
  Node* BuildLoadDataField(Handle<Name> name,
                           PropertyAccessInfo const& access_info,
                           Node* receiver, Node** effect, Node** control);

 private:
  JSGraph* jsgraph() const { return jsgraph_; }
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
  CompilationDependencies* dependencies_;
};

bool HasOnlyStringMaps(MapHandles const& maps);

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_PROPERTY_ACCESS_BUILDER_H_
