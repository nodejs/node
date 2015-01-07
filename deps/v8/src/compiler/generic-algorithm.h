// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_GENERIC_ALGORITHM_H_
#define V8_COMPILER_GENERIC_ALGORITHM_H_

#include <stack>
#include <vector>

#include "src/compiler/graph.h"
#include "src/compiler/node.h"
#include "src/zone-containers.h"

namespace v8 {
namespace internal {
namespace compiler {

class Graph;
class Node;

// GenericGraphVisit allows visitation of graphs of nodes and edges in pre- and
// post-order. Visitation uses an explicitly allocated stack rather than the
// execution stack to avoid stack overflow.
class GenericGraphVisit {
 public:
  // struct Visitor {
  //   void Pre(Node* current);
  //   void Post(Node* current);
  //   void PreEdge(Node* from, int index, Node* to);
  //   void PostEdge(Node* from, int index, Node* to);
  // }
  template <class Visitor>
  static void Visit(Graph* graph, Zone* zone, Node** root_begin,
                    Node** root_end, Visitor* visitor) {
    typedef typename Node::InputEdges::iterator Iterator;
    typedef std::pair<Iterator, Iterator> NodeState;
    typedef std::stack<NodeState, ZoneDeque<NodeState> > NodeStateStack;
    NodeStateStack stack((ZoneDeque<NodeState>(zone)));
    BoolVector visited(graph->NodeCount(), false, zone);
    Node* current = *root_begin;
    while (true) {
      DCHECK(current != NULL);
      const int id = current->id();
      DCHECK(id >= 0);
      DCHECK(id < graph->NodeCount());  // Must be a valid id.
      bool visit = !GetVisited(&visited, id);
      if (visit) {
        visitor->Pre(current);
        SetVisited(&visited, id);
      }
      Iterator begin(visit ? current->input_edges().begin()
                           : current->input_edges().end());
      Iterator end(current->input_edges().end());
      stack.push(NodeState(begin, end));
      Node* post_order_node = current;
      while (true) {
        NodeState top = stack.top();
        if (top.first == top.second) {
          if (visit) {
            visitor->Post(post_order_node);
            SetVisited(&visited, post_order_node->id());
          }
          stack.pop();
          if (stack.empty()) {
            if (++root_begin == root_end) return;
            current = *root_begin;
            break;
          }
          post_order_node = (*stack.top().first).from();
          visit = true;
        } else {
          visitor->PreEdge((*top.first).from(), (*top.first).index(),
                           (*top.first).to());
          current = (*top.first).to();
          if (!GetVisited(&visited, current->id())) break;
        }
        top = stack.top();
        visitor->PostEdge((*top.first).from(), (*top.first).index(),
                          (*top.first).to());
        ++stack.top().first;
      }
    }
  }

  template <class Visitor>
  static void Visit(Graph* graph, Zone* zone, Node* current, Visitor* visitor) {
    Node* array[] = {current};
    Visit<Visitor>(graph, zone, &array[0], &array[1], visitor);
  }

  struct NullNodeVisitor {
    void Pre(Node* node) {}
    void Post(Node* node) {}
    void PreEdge(Node* from, int index, Node* to) {}
    void PostEdge(Node* from, int index, Node* to) {}
  };

 private:
  static void SetVisited(BoolVector* visited, int id) {
    if (id >= static_cast<int>(visited->size())) {
      // Resize and set all values to unvisited.
      visited->resize((3 * id) / 2, false);
    }
    visited->at(id) = true;
  }

  static bool GetVisited(BoolVector* visited, int id) {
    if (id >= static_cast<int>(visited->size())) return false;
    return visited->at(id);
  }
};

typedef GenericGraphVisit::NullNodeVisitor NullNodeVisitor;

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_GENERIC_ALGORITHM_H_
