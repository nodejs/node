/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "src/trace_processor/importers/proto/heap_graph_walker.h"
#include "perfetto/base/logging.h"

#include <deque>

namespace perfetto {
namespace trace_processor {
namespace {

void AddChild(std::map<int64_t, int64_t>* component_to_node,
              uint64_t count,
              int64_t child_component_id,
              int64_t last_node_row) {
  if (count > 1) {
    // We have multiple edges from this component to the target component.
    // This cannot possibly be uniquely retained by one node in this
    // component.
    (*component_to_node)[child_component_id] = -1;
  } else {
    // Check if the node that owns grand_component via child_component_id
    // is the same as the node that owns it through all other
    // child_component_ids.
    auto it = component_to_node->find(child_component_id);
    if (it == component_to_node->end())
      (*component_to_node)[child_component_id] = last_node_row;
    else if (it->second != last_node_row)
      it->second = -1;
  }
}

bool IsUniqueOwner(const std::map<int64_t, int64_t>& component_to_node,
                   uint64_t count,
                   int64_t child_component_id,
                   int64_t last_node_row) {
  if (count > 1)
    return false;

  auto it = component_to_node.find(child_component_id);
  return it == component_to_node.end() || it->second == last_node_row;
}

}  // namespace

HeapGraphWalker::Delegate::~Delegate() = default;

void HeapGraphWalker::AddNode(int64_t row,
                              uint64_t size,
                              ClassNameId class_name) {
  if (static_cast<size_t>(row) >= nodes_.size())
    nodes_.resize(static_cast<size_t>(row) + 1);
  Node& node = GetNode(row);
  node.self_size = size;
  node.row = row;
  node.class_name = class_name;
}

void HeapGraphWalker::AddEdge(int64_t owner_row, int64_t owned_row) {
  Node& owner_node = GetNode(owner_row);
  Node& owned_node = GetNode(owned_row);

  owner_node.children.emplace_back(&owned_node);
  owned_node.parents.emplace_back(&owner_node);
}

void HeapGraphWalker::MarkRoot(int64_t row) {
  Node* node = &GetNode(row);
  roots_.emplace_back(node);

  // Calculate shortest distance to a GC root.
  std::deque<std::pair<int32_t, Node*>> reachable_nodes{{0, node}};
  while (!reachable_nodes.empty()) {
    Node* cur_node;
    int32_t distance;
    std::tie(distance, cur_node) = reachable_nodes.front();
    reachable_nodes.pop_front();
    if (cur_node->distance_to_root == -1 ||
        cur_node->distance_to_root > distance) {
      if (cur_node->distance_to_root == -1)
        delegate_->MarkReachable(cur_node->row);
      cur_node->distance_to_root = distance;
      for (Node* child_node : cur_node->children) {
        if (child_node->distance_to_root == -1 ||
            child_node->distance_to_root > distance + 1)
          reachable_nodes.emplace_back(distance + 1, child_node);
      }
    }
  }
}

void HeapGraphWalker::CalculateRetained() {
  for (Node& n : nodes_) {
    if (n.reachable() && n.node_index == 0)
      FindSCC(&n);
  }

  // Sanity check that we have processed all edges.
  for (const auto& c : components_)
    PERFETTO_CHECK(c.incoming_edges == 0);
}

int64_t HeapGraphWalker::RetainedSize(const Component& component) {
  int64_t retained_size =
      static_cast<int64_t>(component.unique_retained_size) +
      static_cast<int64_t>(component.unique_retained_root_size);
  for (const int64_t child_component_id : component.children_components) {
    const Component& child_component =
        components_[static_cast<size_t>(child_component_id)];
    retained_size += child_component.unique_retained_size;
  }
  return retained_size;
}

void HeapGraphWalker::FoundSCC(Node* node) {
  // We have discovered a new connected component.
  int64_t component_id = static_cast<int64_t>(components_.size());
  components_.emplace_back();
  Component& component = components_.back();
  component.lowlink = node->lowlink;

  std::vector<Node*> component_nodes;

  // A struct representing all direct children from this component.
  struct DirectChild {
    // Number of edges from current component_id to this component.
    size_t edges_from_current_component = 0;
    // If edges_from_current_component == 1, this is the row of the node that
    // has an outgoing edge to it.
    int64_t last_node_row = 0;
  };
  std::map<int64_t, DirectChild> direct_children_rows;

  Node* stack_elem;
  do {
    stack_elem = node_stack_.back();
    component_nodes.emplace_back(stack_elem);
    node_stack_.pop_back();
    for (Node* child : stack_elem->children) {
      if (!child->on_stack) {
        // If the node is not on the stack, but is a child of a node on the
        // stack, it must have already been explored (and assigned a
        // component).
        PERFETTO_CHECK(child->component != -1);
        if (child->component != component_id) {
          DirectChild& dc = direct_children_rows[child->component];
          dc.edges_from_current_component++;
          dc.last_node_row = stack_elem->row;
        }
      }
      // If the node is on the stack, it must be part of this SCC and will be
      // handled by the loop.
      // This node being on the stack means there exist a path from it to the
      // current node. If it also is a child of this node, there is a loop.
    }
    stack_elem->on_stack = false;
    // A node can never be part of two components.
    PERFETTO_CHECK(stack_elem->component == -1);
    stack_elem->component = component_id;
    if (stack_elem->root())
      component.root = true;
  } while (stack_elem != node);

  for (Node* elem : component_nodes) {
    component.unique_retained_size += elem->self_size;
    for (Node* parent : elem->parents) {
      // We do not count intra-component edges.
      if (parent->reachable() && parent->component != component_id)
        component.incoming_edges++;
    }
    component.orig_incoming_edges = component.incoming_edges;
    component.pending_nodes = component.orig_incoming_edges;
  }

  std::map<int64_t, int64_t> unique_retained_by_node;
  // Map from child component to node in this component that uniquely owns it,
  // or -1 if non-unique.
  std::map<int64_t, int64_t> component_to_node;
  for (const auto& p : direct_children_rows) {
    int64_t child_component_id = p.first;
    const DirectChild& dc = p.second;
    size_t count = dc.edges_from_current_component;
    PERFETTO_CHECK(child_component_id != component_id);

    AddChild(&component_to_node, count, child_component_id, dc.last_node_row);

    Component& child_component =
        components_[static_cast<size_t>(child_component_id)];

    for (int64_t grand_component_id : child_component.children_components) {
      AddChild(&component_to_node, count, grand_component_id, dc.last_node_row);
      Component& grand_component =
          components_[static_cast<size_t>(grand_component_id)];
      grand_component.pending_nodes -= count;
      if (grand_component.pending_nodes == 0) {
        component.unique_retained_root_size +=
            grand_component.unique_retained_root_size;
        if (grand_component.root) {
          component.unique_retained_root_size +=
              grand_component.unique_retained_size;
        } else {
          component.unique_retained_size +=
              grand_component.unique_retained_size;

          if (IsUniqueOwner(component_to_node, count, grand_component_id,
                            dc.last_node_row)) {
            unique_retained_by_node[dc.last_node_row] +=
                grand_component.unique_retained_size;
          }
        }
        grand_component.children_components.clear();
        component.children_components.erase(grand_component_id);
      } else {
        component.children_components.emplace(grand_component_id);
      }
    }

    child_component.incoming_edges -= count;
    child_component.pending_nodes -= count;

    if (child_component.pending_nodes == 0) {
      PERFETTO_CHECK(child_component.incoming_edges == 0);

      component.unique_retained_root_size +=
          child_component.unique_retained_root_size;
      if (child_component.root) {
        component.unique_retained_root_size +=
            child_component.unique_retained_size;
      } else {
        component.unique_retained_size += child_component.unique_retained_size;

        if (IsUniqueOwner(component_to_node, count, child_component_id,
                          dc.last_node_row)) {
          unique_retained_by_node[dc.last_node_row] +=
              child_component.unique_retained_size;
        }
      }
      component.children_components.erase(child_component_id);
    } else {
      component.children_components.emplace(child_component_id);
    }

    if (child_component.incoming_edges == 0)
      child_component.children_components.clear();
  }

  size_t parents = component.orig_incoming_edges;
  // If this has no parents, but does not retain a node, we know that no other
  // node can uniquely retain this node. Add 1 to poison that node.
  // If this is a root, but it does not retain a node, we also know that no
  // node can uniquely retain that node.
  if (parents == 0 || component.root)
    parents += 1;
  for (const int64_t child_component_id : component.children_components) {
    Component& child_component =
        components_[static_cast<size_t>(child_component_id)];
    PERFETTO_CHECK(child_component.pending_nodes > 0);
    child_component.pending_nodes += parents;
  }

  int64_t retained_size = RetainedSize(component);
  for (Node* n : component_nodes) {
    int64_t unique_retained_size = 0;
    auto it = unique_retained_by_node.find(n->row);
    if (it != unique_retained_by_node.end())
      unique_retained_size = it->second;

    delegate_->SetRetained(
        n->row, static_cast<int64_t>(retained_size),
        static_cast<int64_t>(n->self_size) + unique_retained_size);
  }
}

void HeapGraphWalker::FindSCC(Node* node) {
  std::vector<Node*> walk_stack;
  std::vector<size_t> walk_child;

  walk_stack.emplace_back(node);
  walk_child.emplace_back(0);

  while (!walk_stack.empty()) {
    node = walk_stack.back();
    size_t& child_idx = walk_child.back();

    if (child_idx == 0) {
      node->node_index = node->lowlink = next_node_index_++;
      node_stack_.push_back(node);
      node->on_stack = true;
    } else {
      Node* prev_child = node->children[child_idx - 1];
      if (prev_child->node_index > node->node_index &&
          prev_child->lowlink < node->lowlink)
        node->lowlink = prev_child->lowlink;
    }

    if (child_idx == node->children.size()) {
      if (node->lowlink == node->node_index)
        FoundSCC(node);
      walk_stack.pop_back();
      walk_child.pop_back();
    } else {
      Node* child = node->children[child_idx++];
      PERFETTO_CHECK(child->reachable());
      if (child->node_index == 0) {
        walk_stack.emplace_back(child);
        walk_child.emplace_back(0);
      } else if (child->on_stack && child->node_index < node->lowlink) {
        node->lowlink = child->node_index;
      }
    }
  }
}

HeapGraphWalker::PathFromRoot HeapGraphWalker::FindPathsFromRoot() {
  PathFromRoot path;
  for (Node* root : roots_)
    FindPathFromRoot(root, &path);
  for (Node& node : nodes_)
    node.find_paths_from_root_visited = false;
  return path;
}

// TODO(fmayer): Teach this to handle field names.
void HeapGraphWalker::FindPathFromRoot(Node* first_node, PathFromRoot* path) {
  // We have long retention chains (e.g. from LinkedList). If we use the stack
  // here, we risk running out of stack space. This is why we use a vector to
  // simulate the stack.
  struct StackElem {
    Node* node;        // Node in the original graph.
    size_t parent_id;  // id of parent node in the result tree.
    size_t i;          // Index of the next child of this node to handle.
    uint32_t depth;    // Depth in the resulting tree
                       // (including artifical root).
  };

  std::vector<StackElem> stack{{first_node, PathFromRoot::kRoot, 0, 0}};

  while (!stack.empty()) {
    Node* n = stack.back().node;
    size_t parent_id = stack.back().parent_id;
    uint32_t depth = stack.back().depth;
    size_t& i = stack.back().i;

    auto it = path->nodes[parent_id].children.find(n->class_name);
    if (it == path->nodes[parent_id].children.end()) {
      size_t id = path->nodes.size();
      path->nodes.emplace_back(PathFromRoot::Node{});
      std::tie(it, std::ignore) =
          path->nodes[parent_id].children.emplace(n->class_name, id);
      path->nodes.back().class_name = n->class_name;
      path->nodes.back().depth = depth;
      path->nodes.back().parent_id = parent_id;
    }
    size_t id = it->second;
    PathFromRoot::Node* output_tree_node = &path->nodes[id];

    if (i == 0) {
      // This is the first time we are looking at this node, so add its
      // size to the relevant node in the resulting tree.
      output_tree_node->size += n->self_size;
      output_tree_node->count++;
    }
    // Otherwise we have already handled this node and just need to get its
    // i-th child.
    if (!n->children.empty()) {
      Node* child = n->children[i];
      if (++i == n->children.size())
        stack.pop_back();

      if (child->distance_to_root == n->distance_to_root + 1 &&
          !child->find_paths_from_root_visited) {
        // Mark as visited in case there is another path with the same distance
        // from a root.
        child->find_paths_from_root_visited = true;
        stack.emplace_back(StackElem{child, id, 0, depth + 1});
      }
    } else {
      stack.pop_back();
    }
  }
}

}  // namespace trace_processor
}  // namespace perfetto
