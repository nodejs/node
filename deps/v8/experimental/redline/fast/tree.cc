// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/redline/fast/tree.h"

namespace redline {
namespace fast {

namespace {

// Shallow copy of one node into `arena` (children still point into the
// source tree; the caller re-clones them).
Node* CopyShallow(Arena* arena, const Node* node) {
  switch (node->kind) {
#define COPY_CASE(name)   \
  case NodeKind::k##name: \
    return arena->New<name>(*node->Cast<name>());
    FAST_NODE_KIND_LIST(COPY_CASE)
#undef COPY_CASE
  }
  return nullptr;
}

}  // namespace

Node* Tree::CloneImpl(const Node* node, bool keep_ids) {
  if (node == nullptr) {
    return nullptr;
  }
  Node* copy = CopyShallow(&arena_, node);
  copy->id = keep_ids ? node->id : next_id_++;
  // Re-intern string views that may point into another tree's arena.
  switch (copy->kind) {
    case NodeKind::kIdentifier: {
      auto* n = copy->Cast<Identifier>();
      n->name = Intern(n->name);
      break;
    }
    case NodeKind::kPrivateIdentifier: {
      auto* n = copy->Cast<PrivateIdentifier>();
      n->name = Intern(n->name);
      break;
    }
    case NodeKind::kNumericLiteral: {
      auto* n = copy->Cast<NumericLiteral>();
      n->raw = Intern(n->raw);
      break;
    }
    case NodeKind::kBigIntLiteral: {
      auto* n = copy->Cast<BigIntLiteral>();
      n->raw = Intern(n->raw);
      break;
    }
    case NodeKind::kStringLiteral: {
      auto* n = copy->Cast<StringLiteral>();
      n->raw = Intern(n->raw);
      break;
    }
    case NodeKind::kRegExpLiteral: {
      auto* n = copy->Cast<RegExpLiteral>();
      n->pattern = Intern(n->pattern);
      n->regexp_flags = Intern(n->regexp_flags);
      break;
    }
    case NodeKind::kTemplateElement: {
      auto* n = copy->Cast<TemplateElement>();
      n->raw = Intern(n->raw);
      break;
    }
    case NodeKind::kNativeCallExpression: {
      auto* n = copy->Cast<NativeCallExpression>();
      n->name = Intern(n->name);
      break;
    }
    case NodeKind::kExpressionStatement: {
      auto* n = copy->Cast<ExpressionStatement>();
      n->directive = Intern(n->directive);
      break;
    }
    default:
      break;
  }
  ForEachChildSlot(copy, [&](Node** slot) {
    if (*slot != nullptr) {
      *slot = CloneImpl(*slot, keep_ids);
    }
  });
  return copy;
}

std::unique_ptr<Tree> Tree::Copy() const {
  // The source text is not copied: the clone re-interns every string it
  // needs, and node positions are only compared with each other (and
  // renumbered before every scope analysis). The parser is the only reader
  // of source(), on the tree it builds.
  auto tree = std::make_unique<Tree>();
  if (root_ != nullptr) {
    tree->set_root(
        static_cast<Program*>(tree->CloneImpl(root_, /*keep_ids=*/true)));
  }
  tree->next_id_ = next_id_;
  return tree;
}

Node* Tree::FindNode(uint32_t id) const {
  Node* found = nullptr;
  Walk(root_, [&](Node* node) {
    if (found != nullptr) {
      return false;
    }
    if (node->id == id) {
      found = node;
      return false;
    }
    return true;
  });
  return found;
}

}  // namespace fast
}  // namespace redline
