// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/graph-visualizer.h"

#include "src/compiler/generic-algorithm.h"
#include "src/compiler/generic-node.h"
#include "src/compiler/generic-node-inl.h"
#include "src/compiler/graph.h"
#include "src/compiler/graph-inl.h"
#include "src/compiler/node.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/node-properties-inl.h"
#include "src/compiler/opcodes.h"
#include "src/compiler/operator.h"
#include "src/ostreams.h"

namespace v8 {
namespace internal {
namespace compiler {

#define DEAD_COLOR "#999999"

class Escaped {
 public:
  explicit Escaped(const OStringStream& os, const char* escaped_chars = "<>|{}")
      : str_(os.c_str()), escaped_chars_(escaped_chars) {}

  friend OStream& operator<<(OStream& os, const Escaped& e) {
    for (const char* s = e.str_; *s != '\0'; ++s) {
      if (e.needs_escape(*s)) os << "\\";
      os << *s;
    }
    return os;
  }

 private:
  bool needs_escape(char ch) const {
    for (size_t i = 0; i < strlen(escaped_chars_); ++i) {
      if (ch == escaped_chars_[i]) return true;
    }
    return false;
  }

  const char* const str_;
  const char* const escaped_chars_;
};

class JSONGraphNodeWriter : public NullNodeVisitor {
 public:
  JSONGraphNodeWriter(OStream& os, Zone* zone, const Graph* graph)  // NOLINT
      : os_(os),
        graph_(graph),
        first_node_(true) {}

  void Print() { const_cast<Graph*>(graph_)->VisitNodeInputsFromEnd(this); }

  GenericGraphVisit::Control Pre(Node* node);

 private:
  OStream& os_;
  const Graph* const graph_;
  bool first_node_;

  DISALLOW_COPY_AND_ASSIGN(JSONGraphNodeWriter);
};


GenericGraphVisit::Control JSONGraphNodeWriter::Pre(Node* node) {
  if (first_node_) {
    first_node_ = false;
  } else {
    os_ << ",";
  }
  OStringStream label;
  label << *node->op();
  os_ << "{\"id\":" << node->id() << ",\"label\":\"" << Escaped(label, "\"")
      << "\"";
  IrOpcode::Value opcode = node->opcode();
  if (opcode == IrOpcode::kPhi || opcode == IrOpcode::kEffectPhi) {
    os_ << ",\"rankInputs\":[0," << NodeProperties::FirstControlIndex(node)
        << "]";
    os_ << ",\"rankWithInput\":[" << NodeProperties::FirstControlIndex(node)
        << "]";
  } else if (opcode == IrOpcode::kIfTrue || opcode == IrOpcode::kIfFalse ||
             opcode == IrOpcode::kLoop) {
    os_ << ",\"rankInputs\":[" << NodeProperties::FirstControlIndex(node)
        << "]";
  }
  if (opcode == IrOpcode::kBranch) {
    os_ << ",\"rankInputs\":[0]";
  }
  os_ << ",\"opcode\":\"" << IrOpcode::Mnemonic(node->opcode()) << "\"";
  os_ << ",\"control\":" << (NodeProperties::IsControl(node) ? "true"
                                                             : "false");
  os_ << "}";
  return GenericGraphVisit::CONTINUE;
}


class JSONGraphEdgeWriter : public NullNodeVisitor {
 public:
  JSONGraphEdgeWriter(OStream& os, Zone* zone, const Graph* graph)  // NOLINT
      : os_(os),
        graph_(graph),
        first_edge_(true) {}

  void Print() { const_cast<Graph*>(graph_)->VisitNodeInputsFromEnd(this); }

  GenericGraphVisit::Control PreEdge(Node* from, int index, Node* to);

 private:
  OStream& os_;
  const Graph* const graph_;
  bool first_edge_;

  DISALLOW_COPY_AND_ASSIGN(JSONGraphEdgeWriter);
};


GenericGraphVisit::Control JSONGraphEdgeWriter::PreEdge(Node* from, int index,
                                                        Node* to) {
  if (first_edge_) {
    first_edge_ = false;
  } else {
    os_ << ",";
  }
  const char* edge_type = NULL;
  if (index < NodeProperties::FirstValueIndex(from)) {
    edge_type = "unknown";
  } else if (index < NodeProperties::FirstContextIndex(from)) {
    edge_type = "value";
  } else if (index < NodeProperties::FirstFrameStateIndex(from)) {
    edge_type = "context";
  } else if (index < NodeProperties::FirstEffectIndex(from)) {
    edge_type = "frame-state";
  } else if (index < NodeProperties::FirstControlIndex(from)) {
    edge_type = "effect";
  } else {
    edge_type = "control";
  }
  os_ << "{\"source\":" << to->id() << ",\"target\":" << from->id()
      << ",\"index\":" << index << ",\"type\":\"" << edge_type << "\"}";
  return GenericGraphVisit::CONTINUE;
}


OStream& operator<<(OStream& os, const AsJSON& ad) {
  Zone tmp_zone(ad.graph.zone()->isolate());
  os << "{\"nodes\":[";
  JSONGraphNodeWriter(os, &tmp_zone, &ad.graph).Print();
  os << "],\"edges\":[";
  JSONGraphEdgeWriter(os, &tmp_zone, &ad.graph).Print();
  os << "]}";
  return os;
}


class GraphVisualizer : public NullNodeVisitor {
 public:
  GraphVisualizer(OStream& os, Zone* zone, const Graph* graph);  // NOLINT

  void Print();

  GenericGraphVisit::Control Pre(Node* node);
  GenericGraphVisit::Control PreEdge(Node* from, int index, Node* to);

 private:
  void AnnotateNode(Node* node);
  void PrintEdge(Node::Edge edge);

  Zone* zone_;
  NodeSet all_nodes_;
  NodeSet white_nodes_;
  bool use_to_def_;
  OStream& os_;
  const Graph* const graph_;

  DISALLOW_COPY_AND_ASSIGN(GraphVisualizer);
};


static Node* GetControlCluster(Node* node) {
  if (OperatorProperties::IsBasicBlockBegin(node->op())) {
    return node;
  } else if (OperatorProperties::GetControlInputCount(node->op()) == 1) {
    Node* control = NodeProperties::GetControlInput(node, 0);
    return OperatorProperties::IsBasicBlockBegin(control->op()) ? control
                                                                : NULL;
  } else {
    return NULL;
  }
}


GenericGraphVisit::Control GraphVisualizer::Pre(Node* node) {
  if (all_nodes_.count(node) == 0) {
    Node* control_cluster = GetControlCluster(node);
    if (control_cluster != NULL) {
      os_ << "  subgraph cluster_BasicBlock" << control_cluster->id() << " {\n";
    }
    os_ << "  ID" << node->id() << " [\n";
    AnnotateNode(node);
    os_ << "  ]\n";
    if (control_cluster != NULL) os_ << "  }\n";
    all_nodes_.insert(node);
    if (use_to_def_) white_nodes_.insert(node);
  }
  return GenericGraphVisit::CONTINUE;
}


GenericGraphVisit::Control GraphVisualizer::PreEdge(Node* from, int index,
                                                    Node* to) {
  if (use_to_def_) return GenericGraphVisit::CONTINUE;
  // When going from def to use, only consider white -> other edges, which are
  // the dead nodes that use live nodes. We're probably not interested in
  // dead nodes that only use other dead nodes.
  if (white_nodes_.count(from) > 0) return GenericGraphVisit::CONTINUE;
  return GenericGraphVisit::SKIP;
}


static bool IsLikelyBackEdge(Node* from, int index, Node* to) {
  if (from->opcode() == IrOpcode::kPhi ||
      from->opcode() == IrOpcode::kEffectPhi) {
    Node* control = NodeProperties::GetControlInput(from, 0);
    return control->opcode() != IrOpcode::kMerge && control != to && index != 0;
  } else if (from->opcode() == IrOpcode::kLoop) {
    return index != 0;
  } else {
    return false;
  }
}


void GraphVisualizer::AnnotateNode(Node* node) {
  if (!use_to_def_) {
    os_ << "    style=\"filled\"\n"
        << "    fillcolor=\"" DEAD_COLOR "\"\n";
  }

  os_ << "    shape=\"record\"\n";
  switch (node->opcode()) {
    case IrOpcode::kEnd:
    case IrOpcode::kDead:
    case IrOpcode::kStart:
      os_ << "    style=\"diagonals\"\n";
      break;
    case IrOpcode::kMerge:
    case IrOpcode::kIfTrue:
    case IrOpcode::kIfFalse:
    case IrOpcode::kLoop:
      os_ << "    style=\"rounded\"\n";
      break;
    default:
      break;
  }

  OStringStream label;
  label << *node->op();
  os_ << "    label=\"{{#" << node->id() << ":" << Escaped(label);

  InputIter i = node->inputs().begin();
  for (int j = OperatorProperties::GetValueInputCount(node->op()); j > 0;
       ++i, j--) {
    os_ << "|<I" << i.index() << ">#" << (*i)->id();
  }
  for (int j = OperatorProperties::GetContextInputCount(node->op()); j > 0;
       ++i, j--) {
    os_ << "|<I" << i.index() << ">X #" << (*i)->id();
  }
  for (int j = OperatorProperties::GetFrameStateInputCount(node->op()); j > 0;
       ++i, j--) {
    os_ << "|<I" << i.index() << ">F #" << (*i)->id();
  }
  for (int j = OperatorProperties::GetEffectInputCount(node->op()); j > 0;
       ++i, j--) {
    os_ << "|<I" << i.index() << ">E #" << (*i)->id();
  }

  if (!use_to_def_ || OperatorProperties::IsBasicBlockBegin(node->op()) ||
      GetControlCluster(node) == NULL) {
    for (int j = OperatorProperties::GetControlInputCount(node->op()); j > 0;
         ++i, j--) {
      os_ << "|<I" << i.index() << ">C #" << (*i)->id();
    }
  }
  os_ << "}";

  if (FLAG_trace_turbo_types && !NodeProperties::IsControl(node)) {
    Bounds bounds = NodeProperties::GetBounds(node);
    OStringStream upper;
    bounds.upper->PrintTo(upper);
    OStringStream lower;
    bounds.lower->PrintTo(lower);
    os_ << "|" << Escaped(upper) << "|" << Escaped(lower);
  }
  os_ << "}\"\n";
}


void GraphVisualizer::PrintEdge(Node::Edge edge) {
  Node* from = edge.from();
  int index = edge.index();
  Node* to = edge.to();
  bool unconstrained = IsLikelyBackEdge(from, index, to);
  os_ << "  ID" << from->id();
  if (all_nodes_.count(to) == 0) {
    os_ << ":I" << index << ":n -> DEAD_INPUT";
  } else if (OperatorProperties::IsBasicBlockBegin(from->op()) ||
             GetControlCluster(from) == NULL ||
             (OperatorProperties::GetControlInputCount(from->op()) > 0 &&
              NodeProperties::GetControlInput(from) != to)) {
    os_ << ":I" << index << ":n -> ID" << to->id() << ":s"
        << "[" << (unconstrained ? "constraint=false, " : "")
        << (NodeProperties::IsControlEdge(edge) ? "style=bold, " : "")
        << (NodeProperties::IsEffectEdge(edge) ? "style=dotted, " : "")
        << (NodeProperties::IsContextEdge(edge) ? "style=dashed, " : "") << "]";
  } else {
    os_ << " -> ID" << to->id() << ":s [color=transparent, "
        << (unconstrained ? "constraint=false, " : "")
        << (NodeProperties::IsControlEdge(edge) ? "style=dashed, " : "") << "]";
  }
  os_ << "\n";
}


void GraphVisualizer::Print() {
  os_ << "digraph D {\n"
      << "  node [fontsize=8,height=0.25]\n"
      << "  rankdir=\"BT\"\n"
      << "  ranksep=\"1.2 equally\"\n"
      << "  overlap=\"false\"\n"
      << "  splines=\"true\"\n"
      << "  concentrate=\"true\"\n"
      << "  \n";

  // Make sure all nodes have been output before writing out the edges.
  use_to_def_ = true;
  // TODO(svenpanne) Remove the need for the const_casts.
  const_cast<Graph*>(graph_)->VisitNodeInputsFromEnd(this);
  white_nodes_.insert(const_cast<Graph*>(graph_)->start());

  // Visit all uses of white nodes.
  use_to_def_ = false;
  GenericGraphVisit::Visit<GraphVisualizer, NodeUseIterationTraits<Node> >(
      const_cast<Graph*>(graph_), zone_, white_nodes_.begin(),
      white_nodes_.end(), this);

  os_ << "  DEAD_INPUT [\n"
      << "    style=\"filled\" \n"
      << "    fillcolor=\"" DEAD_COLOR "\"\n"
      << "  ]\n"
      << "\n";

  // With all the nodes written, add the edges.
  for (NodeSetIter i = all_nodes_.begin(); i != all_nodes_.end(); ++i) {
    Node::Inputs inputs = (*i)->inputs();
    for (Node::Inputs::iterator iter(inputs.begin()); iter != inputs.end();
         ++iter) {
      PrintEdge(iter.edge());
    }
  }
  os_ << "}\n";
}


GraphVisualizer::GraphVisualizer(OStream& os, Zone* zone,
                                 const Graph* graph)  // NOLINT
    : zone_(zone),
      all_nodes_(NodeSet::key_compare(), NodeSet::allocator_type(zone)),
      white_nodes_(NodeSet::key_compare(), NodeSet::allocator_type(zone)),
      use_to_def_(true),
      os_(os),
      graph_(graph) {}


OStream& operator<<(OStream& os, const AsDOT& ad) {
  Zone tmp_zone(ad.graph.zone()->isolate());
  GraphVisualizer(os, &tmp_zone, &ad.graph).Print();
  return os;
}
}
}
}  // namespace v8::internal::compiler
