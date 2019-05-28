// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_NODE_ORIGIN_TABLE_H_
#define V8_COMPILER_NODE_ORIGIN_TABLE_H_

#include <limits>

#include "src/base/compiler-specific.h"
#include "src/compiler/node-aux-data.h"
#include "src/globals.h"
#include "src/source-position.h"

namespace v8 {
namespace internal {
namespace compiler {

class NodeOrigin {
 public:
  enum OriginKind { kWasmBytecode, kGraphNode };
  NodeOrigin(const char* phase_name, const char* reducer_name,
             NodeId created_from)
      : phase_name_(phase_name),
        reducer_name_(reducer_name),
        origin_kind_(kGraphNode),
        created_from_(created_from) {}

  NodeOrigin(const char* phase_name, const char* reducer_name,
             OriginKind origin_kind, uint64_t created_from)
      : phase_name_(phase_name),
        reducer_name_(reducer_name),
        origin_kind_(origin_kind),
        created_from_(created_from) {}

  NodeOrigin(const NodeOrigin& other) V8_NOEXCEPT = default;
  static NodeOrigin Unknown() { return NodeOrigin(); }

  bool IsKnown() { return created_from_ >= 0; }
  int64_t created_from() const { return created_from_; }
  const char* reducer_name() const { return reducer_name_; }
  const char* phase_name() const { return phase_name_; }

  OriginKind origin_kind() const { return origin_kind_; }

  bool operator==(const NodeOrigin& o) const {
    return reducer_name_ == o.reducer_name_ && created_from_ == o.created_from_;
  }

  void PrintJson(std::ostream& out) const;

 private:
  NodeOrigin()
      : phase_name_(""),
        reducer_name_(""),
        created_from_(std::numeric_limits<int64_t>::min()) {}
  const char* phase_name_;
  const char* reducer_name_;
  OriginKind origin_kind_;
  int64_t created_from_;
};

inline bool operator!=(const NodeOrigin& lhs, const NodeOrigin& rhs) {
  return !(lhs == rhs);
}

class V8_EXPORT_PRIVATE NodeOriginTable final
    : public NON_EXPORTED_BASE(ZoneObject) {
 public:
  class Scope final {
   public:
    Scope(NodeOriginTable* origins, const char* reducer_name, Node* node)
        : origins_(origins), prev_origin_(NodeOrigin::Unknown()) {
      if (origins) {
        prev_origin_ = origins->current_origin_;
        origins->current_origin_ =
            NodeOrigin(origins->current_phase_name_, reducer_name, node->id());
      }
    }

    ~Scope() {
      if (origins_) origins_->current_origin_ = prev_origin_;
    }

   private:
    NodeOriginTable* const origins_;
    NodeOrigin prev_origin_;
    DISALLOW_COPY_AND_ASSIGN(Scope);
  };

  class PhaseScope final {
   public:
    PhaseScope(NodeOriginTable* origins, const char* phase_name)
        : origins_(origins) {
      if (origins != nullptr) {
        prev_phase_name_ = origins->current_phase_name_;
        origins->current_phase_name_ =
            phase_name == nullptr ? "unnamed" : phase_name;
      }
    }

    ~PhaseScope() {
      if (origins_) origins_->current_phase_name_ = prev_phase_name_;
    }

   private:
    NodeOriginTable* const origins_;
    const char* prev_phase_name_;
    DISALLOW_COPY_AND_ASSIGN(PhaseScope);
  };

  explicit NodeOriginTable(Graph* graph);

  void AddDecorator();
  void RemoveDecorator();

  NodeOrigin GetNodeOrigin(Node* node) const;
  void SetNodeOrigin(Node* node, const NodeOrigin& no);

  void SetCurrentPosition(const NodeOrigin& no) { current_origin_ = no; }

  void PrintJson(std::ostream& os) const;

 private:
  class Decorator;

  Graph* const graph_;
  Decorator* decorator_;
  NodeOrigin current_origin_;

  const char* current_phase_name_;
  NodeAuxData<NodeOrigin, NodeOrigin::Unknown> table_;

  DISALLOW_COPY_AND_ASSIGN(NodeOriginTable);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_NODE_ORIGIN_TABLE_H_
