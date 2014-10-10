// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/value-numbering-reducer.h"

#include "src/compiler/node.h"

namespace v8 {
namespace internal {
namespace compiler {

namespace {

size_t HashCode(Node* node) { return node->op()->HashCode(); }


bool Equals(Node* a, Node* b) {
  DCHECK_NOT_NULL(a);
  DCHECK_NOT_NULL(b);
  DCHECK_NOT_NULL(a->op());
  DCHECK_NOT_NULL(b->op());
  if (!a->op()->Equals(b->op())) return false;
  if (a->InputCount() != b->InputCount()) return false;
  for (int j = 0; j < a->InputCount(); ++j) {
    DCHECK_NOT_NULL(a->InputAt(j));
    DCHECK_NOT_NULL(b->InputAt(j));
    if (a->InputAt(j)->id() != b->InputAt(j)->id()) return false;
  }
  return true;
}

}  // namespace


class ValueNumberingReducer::Entry FINAL : public ZoneObject {
 public:
  Entry(Node* node, Entry* next) : node_(node), next_(next) {}

  Node* node() const { return node_; }
  Entry* next() const { return next_; }

 private:
  Node* node_;
  Entry* next_;
};


ValueNumberingReducer::ValueNumberingReducer(Zone* zone) : zone_(zone) {
  for (size_t i = 0; i < arraysize(buckets_); ++i) {
    buckets_[i] = NULL;
  }
}


ValueNumberingReducer::~ValueNumberingReducer() {}


Reduction ValueNumberingReducer::Reduce(Node* node) {
  Entry** head = &buckets_[HashCode(node) % arraysize(buckets_)];
  for (Entry* entry = *head; entry; entry = entry->next()) {
    if (entry->node()->IsDead()) continue;
    if (entry->node() == node) return NoChange();
    if (Equals(node, entry->node())) {
      return Replace(entry->node());
    }
  }
  *head = new (zone()) Entry(node, *head);
  return NoChange();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
