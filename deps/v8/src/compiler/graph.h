// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_GRAPH_H_
#define V8_COMPILER_GRAPH_H_

#include <map>
#include <set>

#include "src/compiler/generic-algorithm.h"
#include "src/compiler/node.h"
#include "src/compiler/node-aux-data.h"
#include "src/compiler/source-position.h"

namespace v8 {
namespace internal {
namespace compiler {

class GraphDecorator;


class Graph : public GenericGraph<Node> {
 public:
  explicit Graph(Zone* zone);

  // Base implementation used by all factory methods.
  Node* NewNode(const Operator* op, int input_count, Node** inputs,
                bool incomplete = false);

  // Factories for nodes with static input counts.
  Node* NewNode(const Operator* op) {
    return NewNode(op, 0, static_cast<Node**>(NULL));
  }
  Node* NewNode(const Operator* op, Node* n1) { return NewNode(op, 1, &n1); }
  Node* NewNode(const Operator* op, Node* n1, Node* n2) {
    Node* nodes[] = {n1, n2};
    return NewNode(op, arraysize(nodes), nodes);
  }
  Node* NewNode(const Operator* op, Node* n1, Node* n2, Node* n3) {
    Node* nodes[] = {n1, n2, n3};
    return NewNode(op, arraysize(nodes), nodes);
  }
  Node* NewNode(const Operator* op, Node* n1, Node* n2, Node* n3, Node* n4) {
    Node* nodes[] = {n1, n2, n3, n4};
    return NewNode(op, arraysize(nodes), nodes);
  }
  Node* NewNode(const Operator* op, Node* n1, Node* n2, Node* n3, Node* n4,
                Node* n5) {
    Node* nodes[] = {n1, n2, n3, n4, n5};
    return NewNode(op, arraysize(nodes), nodes);
  }
  Node* NewNode(const Operator* op, Node* n1, Node* n2, Node* n3, Node* n4,
                Node* n5, Node* n6) {
    Node* nodes[] = {n1, n2, n3, n4, n5, n6};
    return NewNode(op, arraysize(nodes), nodes);
  }
  Node* NewNode(const Operator* op, Node* n1, Node* n2, Node* n3, Node* n4,
                Node* n5, Node* n6, Node* n7) {
    Node* nodes[] = {n1, n2, n3, n4, n5, n6, n7};
    return NewNode(op, arraysize(nodes), nodes);
  }

  template <class Visitor>
  void VisitNodeUsesFrom(Node* node, Visitor* visitor);

  template <class Visitor>
  void VisitNodeUsesFromStart(Visitor* visitor);

  template <class Visitor>
  void VisitNodeInputsFromEnd(Visitor* visitor);

  void Decorate(Node* node);

  void AddDecorator(GraphDecorator* decorator) {
    decorators_.push_back(decorator);
  }

  void RemoveDecorator(GraphDecorator* decorator) {
    ZoneVector<GraphDecorator*>::iterator it =
        std::find(decorators_.begin(), decorators_.end(), decorator);
    DCHECK(it != decorators_.end());
    decorators_.erase(it, it + 1);
  }

 private:
  ZoneVector<GraphDecorator*> decorators_;
};


class GraphDecorator : public ZoneObject {
 public:
  virtual ~GraphDecorator() {}
  virtual void Decorate(Node* node) = 0;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_GRAPH_H_
