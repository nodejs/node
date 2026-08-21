// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_GRAPH_LABELLER_H_
#define V8_MAGLEV_MAGLEV_GRAPH_LABELLER_H_

#include <map>

#include "src/maglev/maglev-ir.h"
#include "src/utils/utils.h"

namespace v8 {
namespace internal {
namespace maglev {

class MaglevGraphLabeller {
 public:
  struct Provenance {
    const MaglevCompilationUnit* unit = nullptr;
    BytecodeOffset bytecode_offset = BytecodeOffset::None();
    SourcePosition position = SourcePosition::Unknown();
  };
  struct NodeInfo {
    int label = -1;
    Provenance provenance;
  };

  void RegisterNode(const NodeBase* node, const MaglevCompilationUnit* unit,
                    BytecodeOffset bytecode_offset, SourcePosition position) {
    if (nodes_
            .emplace(node, NodeInfo{next_node_label_,
                                    {unit, bytecode_offset, position}})
            .second) {
      next_node_label_++;
    }
  }
  void RegisterNode(const NodeBase* node, const Provenance* provenance) {
    RegisterNode(node, provenance->unit, provenance->bytecode_offset,
                 provenance->position);
  }
  void RegisterNode(const NodeBase* node) {
    RegisterNode(node, nullptr, BytecodeOffset::None(),
                 SourcePosition::Unknown());
  }

  int NodeId(const NodeBase* node) { return nodes_[node].label; }
  const Provenance& GetNodeProvenance(const NodeBase* node) {
    return nodes_[node].provenance;
  }

  int max_node_id() const { return next_node_label_ - 1; }

  void PrintNodeLabel(std::ostream& os, const NodeBase* node,
                      bool has_regalloc_data) {
    if (node != nullptr && node->Is<VirtualObject>()) {
      // VirtualObjects are unregisted nodes, since they are not attached to
      // the graph, but its inlined allocation is.
      const VirtualObject* vo = node->Cast<VirtualObject>();
      os << "VO{" << vo->id() << "}:";
      node = vo->allocation();
    }
    auto node_id_it = nodes_.find(node);

    if (node_id_it == nodes_.end()) {
      os << "<unregistered node " << node << ">";
      return;
    }

    if (has_regalloc_data) {
      os << "v" << node->id() << "/";
    }
    os << "n" << node_id_it->second.label;

    if (node->Is<Identity>()) {
      os << ":ID[";
      PrintNodeLabel(os, node->input(0).node(), has_regalloc_data);
      os << "]";
      return;
    }
  }

  void PrintInput(std::ostream& os, ConstInput input, bool has_regalloc_data) {
    PrintNodeLabel(os, input.node(), has_regalloc_data);
    if (has_regalloc_data) {
      os << ":" << input.operand();
    }
  }

 private:
  std::map<const NodeBase*, NodeInfo> nodes_;
  int next_node_label_ = 1;
};

class MaglevGraphLabellerScope {
 public:
  explicit MaglevGraphLabellerScope(MaglevGraphLabeller* graph_labeller);
  ~MaglevGraphLabellerScope();
};

extern thread_local MaglevGraphLabeller* thread_graph_labeller;

MaglevGraphLabeller* GetCurrentGraphLabeller();

#ifdef V8_ENABLE_MAGLEV_GRAPH_PRINTER

class PrintNode {
 public:
  explicit PrintNode(const NodeBase* node, bool has_regalloc_data = false,
                     bool skip_targets = false)
      : node_(node),
        has_regalloc_data_(has_regalloc_data),
        skip_targets_(skip_targets) {}

  void Print(std::ostream& os) const;

 private:
  const NodeBase* node_;
  bool has_regalloc_data_;
  // This is used when tracing graph building, since targets might not exist
  // yet.
  const bool skip_targets_;
};

class PrintNodeLabel {
 public:
  explicit PrintNodeLabel(const NodeBase* node) : node_(node) {}

  void Print(std::ostream& os) const;

 private:
  const NodeBase* node_;
};

#else

class PrintNode {
 public:
  explicit PrintNode(const NodeBase* node, bool has_regalloc_data = false,
                     bool skip_targets = false) {}
  void Print(std::ostream& os) const {}
};

class PrintNodeLabel {
 public:
  explicit PrintNodeLabel(const NodeBase* node) {}
  void Print(std::ostream& os) const {}
};

#endif  // V8_ENABLE_MAGLEV_GRAPH_PRINTER

inline std::ostream& operator<<(std::ostream& os, const PrintNode& printer) {
  printer.Print(os);
  return os;
}

inline std::ostream& operator<<(std::ostream& os,
                                const PrintNodeLabel& printer) {
  printer.Print(os);
  return os;
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_GRAPH_LABELLER_H_
