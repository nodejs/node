// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_GENERIC_ALGORITHM_H_
#define V8_COMPILER_GENERIC_ALGORITHM_H_

#include <stack>

#include "src/compiler/generic-graph.h"
#include "src/compiler/generic-node.h"
#include "src/zone-containers.h"

namespace v8 {
namespace internal {
namespace compiler {

// GenericGraphVisit allows visitation of graphs of nodes and edges in pre- and
// post-order. Visitation uses an explicitly allocated stack rather than the
// execution stack to avoid stack overflow. Although GenericGraphVisit is
// primarily intended to traverse networks of nodes through their
// dependencies and uses, it also can be used to visit any graph-like network
// by specifying custom traits.
class GenericGraphVisit {
 public:
  // struct Visitor {
  //   void Pre(Traits::Node* current);
  //   void Post(Traits::Node* current);
  //   void PreEdge(Traits::Node* from, int index, Traits::Node* to);
  //   void PostEdge(Traits::Node* from, int index, Traits::Node* to);
  // }
  template <class Visitor, class Traits, class RootIterator>
  static void Visit(GenericGraphBase* graph, Zone* zone,
                    RootIterator root_begin, RootIterator root_end,
                    Visitor* visitor) {
    typedef typename Traits::Node Node;
    typedef typename Traits::Iterator Iterator;
    typedef std::pair<Iterator, Iterator> NodeState;
    typedef std::stack<NodeState, ZoneDeque<NodeState> > NodeStateStack;
    NodeStateStack stack((ZoneDeque<NodeState>(zone)));
    BoolVector visited(Traits::max_id(graph), false, zone);
    Node* current = *root_begin;
    while (true) {
      DCHECK(current != NULL);
      const int id = current->id();
      DCHECK(id >= 0);
      DCHECK(id < Traits::max_id(graph));  // Must be a valid id.
      bool visit = !GetVisited(&visited, id);
      if (visit) {
        visitor->Pre(current);
        SetVisited(&visited, id);
      }
      Iterator begin(visit ? Traits::begin(current) : Traits::end(current));
      Iterator end(Traits::end(current));
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
          post_order_node = Traits::from(stack.top().first);
          visit = true;
        } else {
          visitor->PreEdge(Traits::from(top.first), top.first.edge().index(),
                           Traits::to(top.first));
          current = Traits::to(top.first);
          if (!GetVisited(&visited, current->id())) break;
        }
        top = stack.top();
        visitor->PostEdge(Traits::from(top.first), top.first.edge().index(),
                          Traits::to(top.first));
        ++stack.top().first;
      }
    }
  }

  template <class Visitor, class Traits>
  static void Visit(GenericGraphBase* graph, Zone* zone,
                    typename Traits::Node* current, Visitor* visitor) {
    typename Traits::Node* array[] = {current};
    Visit<Visitor, Traits>(graph, zone, &array[0], &array[1], visitor);
  }

  template <class B, class S>
  struct NullNodeVisitor {
    void Pre(GenericNode<B, S>* node) {}
    void Post(GenericNode<B, S>* node) {}
    void PreEdge(GenericNode<B, S>* from, int index, GenericNode<B, S>* to) {}
    void PostEdge(GenericNode<B, S>* from, int index, GenericNode<B, S>* to) {}
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
}
}
}  // namespace v8::internal::compiler

#endif  // V8_COMPILER_GENERIC_ALGORITHM_H_
