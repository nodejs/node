// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_REDLINE_FAST_TREE_H_
#define V8_EXPERIMENTAL_REDLINE_FAST_TREE_H_

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

#include "experimental/redline/fast/arena.h"
#include "experimental/redline/fast/ast.h"

namespace redline {
namespace fast {

// Owns one program: its arena, the original source text (if any), the root
// Program node and the node id counter.
class Tree {
 public:
  Tree() = default;
  Tree(const Tree&) = delete;
  Tree& operator=(const Tree&) = delete;

  Arena& arena() { return arena_; }
  Program* root() const { return root_; }
  void set_root(Program* root) { root_ = root; }

  // Allocates a node with a fresh id.
  template <typename T>
  T* New(int32_t start = -1, int32_t end = -1) {
    T* node = arena_.New<T>();
    node->id = next_id_++;
    node->start = start;
    node->end = end;
    return node;
  }

  // The source this tree was parsed from (empty for generated programs).
  std::string_view source() const { return source_; }
  void SetSource(std::string_view source) {
    source_ = arena_.CopyString(source);
  }

  // Copies `text` into the arena (for names and raw literal text).
  std::string_view Intern(std::string_view text) {
    return arena_.CopyString(text);
  }

  // Number of ids handed out so far (upper bound for ids in the tree).
  uint32_t id_limit() const { return next_id_; }

  // Deep-copies `node` (from this or any other tree) into this tree. All
  // nodes get fresh ids; string views are re-interned here.
  Node* Clone(const Node* node) { return CloneImpl(node, /*keep_ids=*/false); }
  template <typename T>
  T* Clone(const T* node) {
    return static_cast<T*>(Clone(static_cast<const Node*>(node)));
  }

  // Deep copy of the whole tree. Node ids are preserved, so ids seen in one
  // view of a program remain valid in the next mutation step, and new nodes
  // get ids above `id_limit()`. The copy has no source text (strings are
  // re-interned into the copy's arena).
  std::unique_ptr<Tree> Copy() const;

  // Finds the node with the given id (linear walk), or nullptr.
  Node* FindNode(uint32_t id) const;

 private:
  Node* CloneImpl(const Node* node, bool keep_ids);

  Arena arena_;
  Program* root_ = nullptr;
  std::string_view source_;
  uint32_t next_id_ = 0;
};

}  // namespace fast
}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_FAST_TREE_H_
