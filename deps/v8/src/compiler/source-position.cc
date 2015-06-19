// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/source-position.h"
#include "src/compiler/graph.h"
#include "src/compiler/node-aux-data.h"

namespace v8 {
namespace internal {
namespace compiler {

class SourcePositionTable::Decorator final : public GraphDecorator {
 public:
  explicit Decorator(SourcePositionTable* source_positions)
      : source_positions_(source_positions) {}

  void Decorate(Node* node, bool incomplete) final {
    DCHECK(!source_positions_->current_position_.IsInvalid());
    source_positions_->table_.Set(node, source_positions_->current_position_);
  }

 private:
  SourcePositionTable* source_positions_;
};


SourcePositionTable::SourcePositionTable(Graph* graph)
    : graph_(graph),
      decorator_(nullptr),
      current_position_(SourcePosition::Invalid()),
      table_(graph->zone()) {}


void SourcePositionTable::AddDecorator() {
  DCHECK_NULL(decorator_);
  decorator_ = new (graph_->zone()) Decorator(this);
  graph_->AddDecorator(decorator_);
}


void SourcePositionTable::RemoveDecorator() {
  DCHECK_NOT_NULL(decorator_);
  graph_->RemoveDecorator(decorator_);
  decorator_ = nullptr;
}


SourcePosition SourcePositionTable::GetSourcePosition(Node* node) const {
  return table_.Get(node);
}


void SourcePositionTable::Print(std::ostream& os) const {
  os << "{";
  bool needs_comma = false;
  for (auto i : table_) {
    SourcePosition pos = i.second;
    if (!pos.IsUnknown()) {
      if (needs_comma) {
        os << ",";
      }
      os << "\"" << i.first << "\""
         << ":" << pos.raw();
      needs_comma = true;
    }
  }
  os << "}";
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
