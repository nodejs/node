// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/graph-visualizer.h"

#include <sstream>
#include <string>

#include "src/code-stubs.h"
#include "src/compiler/generic-node.h"
#include "src/compiler/generic-node-inl.h"
#include "src/compiler/graph.h"
#include "src/compiler/graph-inl.h"
#include "src/compiler/node.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/node-properties-inl.h"
#include "src/compiler/opcodes.h"
#include "src/compiler/operator.h"
#include "src/compiler/register-allocator.h"
#include "src/compiler/schedule.h"
#include "src/compiler/scheduler.h"
#include "src/ostreams.h"

namespace v8 {
namespace internal {
namespace compiler {

static int SafeId(Node* node) { return node == NULL ? -1 : node->id(); }

#define DEAD_COLOR "#999999"

class AllNodes {
 public:
  enum State { kDead, kGray, kLive };

  AllNodes(Zone* local_zone, const Graph* graph)
      : state(graph->NodeCount(), kDead, local_zone),
        live(local_zone),
        gray(local_zone) {
    Node* end = graph->end();
    state[end->id()] = kLive;
    live.push_back(end);
    // Find all live nodes reachable from end.
    for (size_t i = 0; i < live.size(); i++) {
      for (Node* const input : live[i]->inputs()) {
        if (input == NULL) {
          // TODO(titzer): print a warning.
          continue;
        }
        if (input->id() >= graph->NodeCount()) {
          // TODO(titzer): print a warning.
          continue;
        }
        if (state[input->id()] != kLive) {
          live.push_back(input);
          state[input->id()] = kLive;
        }
      }
    }

    // Find all nodes that are not reachable from end that use live nodes.
    for (size_t i = 0; i < live.size(); i++) {
      for (Node* const use : live[i]->uses()) {
        if (state[use->id()] == kDead) {
          gray.push_back(use);
          state[use->id()] = kGray;
        }
      }
    }
  }

  bool IsLive(Node* node) {
    return node != NULL && node->id() < static_cast<int>(state.size()) &&
           state[node->id()] == kLive;
  }

  ZoneVector<State> state;
  NodeVector live;
  NodeVector gray;
};


class Escaped {
 public:
  explicit Escaped(const std::ostringstream& os,
                   const char* escaped_chars = "<>|{}")
      : str_(os.str()), escaped_chars_(escaped_chars) {}

  friend std::ostream& operator<<(std::ostream& os, const Escaped& e) {
    for (std::string::const_iterator i = e.str_.begin(); i != e.str_.end();
         ++i) {
      if (e.needs_escape(*i)) os << "\\";
      os << *i;
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

  const std::string str_;
  const char* const escaped_chars_;
};

class JSONGraphNodeWriter {
 public:
  JSONGraphNodeWriter(std::ostream& os, Zone* zone, const Graph* graph)
      : os_(os), all_(zone, graph), first_node_(true) {}

  void Print() {
    for (Node* const node : all_.live) PrintNode(node);
  }

  void PrintNode(Node* node) {
    if (first_node_) {
      first_node_ = false;
    } else {
      os_ << ",";
    }
    std::ostringstream label;
    label << *node->op();
    os_ << "{\"id\":" << SafeId(node) << ",\"label\":\"" << Escaped(label, "\"")
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
  }

 private:
  std::ostream& os_;
  AllNodes all_;
  bool first_node_;

  DISALLOW_COPY_AND_ASSIGN(JSONGraphNodeWriter);
};


class JSONGraphEdgeWriter {
 public:
  JSONGraphEdgeWriter(std::ostream& os, Zone* zone, const Graph* graph)
      : os_(os), all_(zone, graph), first_edge_(true) {}

  void Print() {
    for (Node* const node : all_.live) PrintEdges(node);
  }

  void PrintEdges(Node* node) {
    for (int i = 0; i < node->InputCount(); i++) {
      Node* input = node->InputAt(i);
      if (input == NULL) continue;
      PrintEdge(node, i, input);
    }
  }

  void PrintEdge(Node* from, int index, Node* to) {
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
    os_ << "{\"source\":" << SafeId(to) << ",\"target\":" << SafeId(from)
        << ",\"index\":" << index << ",\"type\":\"" << edge_type << "\"}";
  }

 private:
  std::ostream& os_;
  AllNodes all_;
  bool first_edge_;

  DISALLOW_COPY_AND_ASSIGN(JSONGraphEdgeWriter);
};


std::ostream& operator<<(std::ostream& os, const AsJSON& ad) {
  Zone tmp_zone(ad.graph.zone()->isolate());
  os << "{\"nodes\":[";
  JSONGraphNodeWriter(os, &tmp_zone, &ad.graph).Print();
  os << "],\"edges\":[";
  JSONGraphEdgeWriter(os, &tmp_zone, &ad.graph).Print();
  os << "]}";
  return os;
}


class GraphVisualizer {
 public:
  GraphVisualizer(std::ostream& os, Zone* zone, const Graph* graph)
      : all_(zone, graph), os_(os) {}

  void Print();

  void PrintNode(Node* node, bool gray);

 private:
  void PrintEdge(Node::Edge edge);

  AllNodes all_;
  std::ostream& os_;

  DISALLOW_COPY_AND_ASSIGN(GraphVisualizer);
};


static Node* GetControlCluster(Node* node) {
  if (OperatorProperties::IsBasicBlockBegin(node->op())) {
    return node;
  } else if (node->op()->ControlInputCount() == 1) {
    Node* control = NodeProperties::GetControlInput(node, 0);
    return control != NULL &&
                   OperatorProperties::IsBasicBlockBegin(control->op())
               ? control
               : NULL;
  } else {
    return NULL;
  }
}


void GraphVisualizer::PrintNode(Node* node, bool gray) {
  Node* control_cluster = GetControlCluster(node);
  if (control_cluster != NULL) {
    os_ << "  subgraph cluster_BasicBlock" << control_cluster->id() << " {\n";
  }
  os_ << "  ID" << SafeId(node) << " [\n";

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

  if (gray) {
    os_ << "    style=\"filled\"\n"
        << "    fillcolor=\"" DEAD_COLOR "\"\n";
  }

  std::ostringstream label;
  label << *node->op();
  os_ << "    label=\"{{#" << SafeId(node) << ":" << Escaped(label);

  InputIter i = node->inputs().begin();
  for (int j = node->op()->ValueInputCount(); j > 0; ++i, j--) {
    os_ << "|<I" << i.index() << ">#" << SafeId(*i);
  }
  for (int j = OperatorProperties::GetContextInputCount(node->op()); j > 0;
       ++i, j--) {
    os_ << "|<I" << i.index() << ">X #" << SafeId(*i);
  }
  for (int j = OperatorProperties::GetFrameStateInputCount(node->op()); j > 0;
       ++i, j--) {
    os_ << "|<I" << i.index() << ">F #" << SafeId(*i);
  }
  for (int j = node->op()->EffectInputCount(); j > 0; ++i, j--) {
    os_ << "|<I" << i.index() << ">E #" << SafeId(*i);
  }

  if (OperatorProperties::IsBasicBlockBegin(node->op()) ||
      GetControlCluster(node) == NULL) {
    for (int j = node->op()->ControlInputCount(); j > 0; ++i, j--) {
      os_ << "|<I" << i.index() << ">C #" << SafeId(*i);
    }
  }
  os_ << "}";

  if (FLAG_trace_turbo_types && NodeProperties::IsTyped(node)) {
    Bounds bounds = NodeProperties::GetBounds(node);
    std::ostringstream upper;
    bounds.upper->PrintTo(upper);
    std::ostringstream lower;
    bounds.lower->PrintTo(lower);
    os_ << "|" << Escaped(upper) << "|" << Escaped(lower);
  }
  os_ << "}\"\n";

  os_ << "  ]\n";
  if (control_cluster != NULL) os_ << "  }\n";
}


static bool IsLikelyBackEdge(Node* from, int index, Node* to) {
  if (from->opcode() == IrOpcode::kPhi ||
      from->opcode() == IrOpcode::kEffectPhi) {
    Node* control = NodeProperties::GetControlInput(from, 0);
    return control != NULL && control->opcode() != IrOpcode::kMerge &&
           control != to && index != 0;
  } else if (from->opcode() == IrOpcode::kLoop) {
    return index != 0;
  } else {
    return false;
  }
}


void GraphVisualizer::PrintEdge(Node::Edge edge) {
  Node* from = edge.from();
  int index = edge.index();
  Node* to = edge.to();

  if (!all_.IsLive(to)) return;  // skip inputs that point to dead or NULL.

  bool unconstrained = IsLikelyBackEdge(from, index, to);
  os_ << "  ID" << SafeId(from);

  if (OperatorProperties::IsBasicBlockBegin(from->op()) ||
      GetControlCluster(from) == NULL ||
      (from->op()->ControlInputCount() > 0 &&
       NodeProperties::GetControlInput(from) != to)) {
    os_ << ":I" << index << ":n -> ID" << SafeId(to) << ":s"
        << "[" << (unconstrained ? "constraint=false, " : "")
        << (NodeProperties::IsControlEdge(edge) ? "style=bold, " : "")
        << (NodeProperties::IsEffectEdge(edge) ? "style=dotted, " : "")
        << (NodeProperties::IsContextEdge(edge) ? "style=dashed, " : "") << "]";
  } else {
    os_ << " -> ID" << SafeId(to) << ":s [color=transparent, "
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
  for (Node* const node : all_.live) PrintNode(node, false);
  for (Node* const node : all_.gray) PrintNode(node, true);

  // With all the nodes written, add the edges.
  for (Node* const node : all_.live) {
    for (UseIter i = node->uses().begin(); i != node->uses().end(); ++i) {
      PrintEdge(i.edge());
    }
  }
  os_ << "}\n";
}


std::ostream& operator<<(std::ostream& os, const AsDOT& ad) {
  Zone tmp_zone(ad.graph.zone()->isolate());
  GraphVisualizer(os, &tmp_zone, &ad.graph).Print();
  return os;
}


class GraphC1Visualizer {
 public:
  GraphC1Visualizer(std::ostream& os, Zone* zone);  // NOLINT

  void PrintCompilation(const CompilationInfo* info);
  void PrintSchedule(const char* phase, const Schedule* schedule,
                     const SourcePositionTable* positions,
                     const InstructionSequence* instructions);
  void PrintAllocator(const char* phase, const RegisterAllocator* allocator);
  Zone* zone() const { return zone_; }

 private:
  void PrintIndent();
  void PrintStringProperty(const char* name, const char* value);
  void PrintLongProperty(const char* name, int64_t value);
  void PrintIntProperty(const char* name, int value);
  void PrintBlockProperty(const char* name, BasicBlock::Id block_id);
  void PrintNodeId(Node* n);
  void PrintNode(Node* n);
  void PrintInputs(Node* n);
  void PrintInputs(InputIter* i, int count, const char* prefix);
  void PrintType(Node* node);

  void PrintLiveRange(LiveRange* range, const char* type);
  class Tag FINAL BASE_EMBEDDED {
   public:
    Tag(GraphC1Visualizer* visualizer, const char* name) {
      name_ = name;
      visualizer_ = visualizer;
      visualizer->PrintIndent();
      visualizer_->os_ << "begin_" << name << "\n";
      visualizer->indent_++;
    }

    ~Tag() {
      visualizer_->indent_--;
      visualizer_->PrintIndent();
      visualizer_->os_ << "end_" << name_ << "\n";
      DCHECK(visualizer_->indent_ >= 0);
    }

   private:
    GraphC1Visualizer* visualizer_;
    const char* name_;
  };

  std::ostream& os_;
  int indent_;
  Zone* zone_;

  DISALLOW_COPY_AND_ASSIGN(GraphC1Visualizer);
};


void GraphC1Visualizer::PrintIndent() {
  for (int i = 0; i < indent_; i++) {
    os_ << "  ";
  }
}


GraphC1Visualizer::GraphC1Visualizer(std::ostream& os, Zone* zone)
    : os_(os), indent_(0), zone_(zone) {}


void GraphC1Visualizer::PrintStringProperty(const char* name,
                                            const char* value) {
  PrintIndent();
  os_ << name << " \"" << value << "\"\n";
}


void GraphC1Visualizer::PrintLongProperty(const char* name, int64_t value) {
  PrintIndent();
  os_ << name << " " << static_cast<int>(value / 1000) << "\n";
}


void GraphC1Visualizer::PrintBlockProperty(const char* name,
                                           BasicBlock::Id block_id) {
  PrintIndent();
  os_ << name << " \"B" << block_id << "\"\n";
}


void GraphC1Visualizer::PrintIntProperty(const char* name, int value) {
  PrintIndent();
  os_ << name << " " << value << "\n";
}


void GraphC1Visualizer::PrintCompilation(const CompilationInfo* info) {
  Tag tag(this, "compilation");
  if (info->IsOptimizing()) {
    Handle<String> name = info->function()->debug_name();
    PrintStringProperty("name", name->ToCString().get());
    PrintIndent();
    os_ << "method \"" << name->ToCString().get() << ":"
        << info->optimization_id() << "\"\n";
  } else {
    CodeStub::Major major_key = info->code_stub()->MajorKey();
    PrintStringProperty("name", CodeStub::MajorName(major_key, false));
    PrintStringProperty("method", "stub");
  }
  PrintLongProperty("date",
                    static_cast<int64_t>(base::OS::TimeCurrentMillis()));
}


void GraphC1Visualizer::PrintNodeId(Node* n) { os_ << "n" << SafeId(n); }


void GraphC1Visualizer::PrintNode(Node* n) {
  PrintNodeId(n);
  os_ << " " << *n->op() << " ";
  PrintInputs(n);
}


void GraphC1Visualizer::PrintInputs(InputIter* i, int count,
                                    const char* prefix) {
  if (count > 0) {
    os_ << prefix;
  }
  while (count > 0) {
    os_ << " ";
    PrintNodeId(**i);
    ++(*i);
    count--;
  }
}


void GraphC1Visualizer::PrintInputs(Node* node) {
  InputIter i = node->inputs().begin();
  PrintInputs(&i, node->op()->ValueInputCount(), " ");
  PrintInputs(&i, OperatorProperties::GetContextInputCount(node->op()),
              " Ctx:");
  PrintInputs(&i, OperatorProperties::GetFrameStateInputCount(node->op()),
              " FS:");
  PrintInputs(&i, node->op()->EffectInputCount(), " Eff:");
  PrintInputs(&i, node->op()->ControlInputCount(), " Ctrl:");
}


void GraphC1Visualizer::PrintType(Node* node) {
  if (NodeProperties::IsTyped(node)) {
    Bounds bounds = NodeProperties::GetBounds(node);
    os_ << " type:";
    bounds.upper->PrintTo(os_);
    os_ << "..";
    bounds.lower->PrintTo(os_);
  }
}


void GraphC1Visualizer::PrintSchedule(const char* phase,
                                      const Schedule* schedule,
                                      const SourcePositionTable* positions,
                                      const InstructionSequence* instructions) {
  Tag tag(this, "cfg");
  PrintStringProperty("name", phase);
  const BasicBlockVector* rpo = schedule->rpo_order();
  for (size_t i = 0; i < rpo->size(); i++) {
    BasicBlock* current = (*rpo)[i];
    Tag block_tag(this, "block");
    PrintBlockProperty("name", current->id());
    PrintIntProperty("from_bci", -1);
    PrintIntProperty("to_bci", -1);

    PrintIndent();
    os_ << "predecessors";
    for (BasicBlock::Predecessors::iterator j = current->predecessors_begin();
         j != current->predecessors_end(); ++j) {
      os_ << " \"B" << (*j)->id() << "\"";
    }
    os_ << "\n";

    PrintIndent();
    os_ << "successors";
    for (BasicBlock::Successors::iterator j = current->successors_begin();
         j != current->successors_end(); ++j) {
      os_ << " \"B" << (*j)->id() << "\"";
    }
    os_ << "\n";

    PrintIndent();
    os_ << "xhandlers\n";

    PrintIndent();
    os_ << "flags\n";

    if (current->dominator() != NULL) {
      PrintBlockProperty("dominator", current->dominator()->id());
    }

    PrintIntProperty("loop_depth", current->loop_depth());

    const InstructionBlock* instruction_block =
        instructions->InstructionBlockAt(current->GetRpoNumber());
    if (instruction_block->code_start() >= 0) {
      int first_index = instruction_block->first_instruction_index();
      int last_index = instruction_block->last_instruction_index();
      PrintIntProperty("first_lir_id", LifetimePosition::FromInstructionIndex(
                                           first_index).Value());
      PrintIntProperty("last_lir_id", LifetimePosition::FromInstructionIndex(
                                          last_index).Value());
    }

    {
      Tag states_tag(this, "states");
      Tag locals_tag(this, "locals");
      int total = 0;
      for (BasicBlock::const_iterator i = current->begin(); i != current->end();
           ++i) {
        if ((*i)->opcode() == IrOpcode::kPhi) total++;
      }
      PrintIntProperty("size", total);
      PrintStringProperty("method", "None");
      int index = 0;
      for (BasicBlock::const_iterator i = current->begin(); i != current->end();
           ++i) {
        if ((*i)->opcode() != IrOpcode::kPhi) continue;
        PrintIndent();
        os_ << index << " ";
        PrintNodeId(*i);
        os_ << " [";
        PrintInputs(*i);
        os_ << "]\n";
        index++;
      }
    }

    {
      Tag HIR_tag(this, "HIR");
      for (BasicBlock::const_iterator i = current->begin(); i != current->end();
           ++i) {
        Node* node = *i;
        if (node->opcode() == IrOpcode::kPhi) continue;
        int uses = node->UseCount();
        PrintIndent();
        os_ << "0 " << uses << " ";
        PrintNode(node);
        if (FLAG_trace_turbo_types) {
          os_ << " ";
          PrintType(node);
        }
        if (positions != NULL) {
          SourcePosition position = positions->GetSourcePosition(node);
          if (!position.IsUnknown()) {
            DCHECK(!position.IsInvalid());
            os_ << " pos:" << position.raw();
          }
        }
        os_ << " <|@\n";
      }

      BasicBlock::Control control = current->control();
      if (control != BasicBlock::kNone) {
        PrintIndent();
        os_ << "0 0 ";
        if (current->control_input() != NULL) {
          PrintNode(current->control_input());
        } else {
          os_ << -1 - current->id().ToInt() << " Goto";
        }
        os_ << " ->";
        for (BasicBlock::Successors::iterator j = current->successors_begin();
             j != current->successors_end(); ++j) {
          os_ << " B" << (*j)->id();
        }
        if (FLAG_trace_turbo_types && current->control_input() != NULL) {
          os_ << " ";
          PrintType(current->control_input());
        }
        os_ << " <|@\n";
      }
    }

    if (instructions != NULL) {
      Tag LIR_tag(this, "LIR");
      for (int j = instruction_block->first_instruction_index();
           j <= instruction_block->last_instruction_index(); j++) {
        PrintIndent();
        PrintableInstruction printable = {RegisterConfiguration::ArchDefault(),
                                          instructions->InstructionAt(j)};
        os_ << j << " " << printable << " <|@\n";
      }
    }
  }
}


void GraphC1Visualizer::PrintAllocator(const char* phase,
                                       const RegisterAllocator* allocator) {
  Tag tag(this, "intervals");
  PrintStringProperty("name", phase);

  for (auto range : allocator->fixed_double_live_ranges()) {
    PrintLiveRange(range, "fixed");
  }

  for (auto range : allocator->fixed_live_ranges()) {
    PrintLiveRange(range, "fixed");
  }

  for (auto range : allocator->live_ranges()) {
    PrintLiveRange(range, "object");
  }
}


void GraphC1Visualizer::PrintLiveRange(LiveRange* range, const char* type) {
  if (range != NULL && !range->IsEmpty()) {
    PrintIndent();
    os_ << range->id() << " " << type;
    if (range->HasRegisterAssigned()) {
      InstructionOperand* op = range->CreateAssignedOperand(zone());
      int assigned_reg = op->index();
      if (op->IsDoubleRegister()) {
        os_ << " \"" << DoubleRegister::AllocationIndexToString(assigned_reg)
            << "\"";
      } else {
        DCHECK(op->IsRegister());
        os_ << " \"" << Register::AllocationIndexToString(assigned_reg) << "\"";
      }
    } else if (range->IsSpilled()) {
      InstructionOperand* op = range->TopLevel()->GetSpillOperand();
      if (op->IsDoubleStackSlot()) {
        os_ << " \"double_stack:" << op->index() << "\"";
      } else if (op->IsStackSlot()) {
        os_ << " \"stack:" << op->index() << "\"";
      } else {
        DCHECK(op->IsConstant());
        os_ << " \"const(nostack):" << op->index() << "\"";
      }
    }
    int parent_index = -1;
    if (range->IsChild()) {
      parent_index = range->parent()->id();
    } else {
      parent_index = range->id();
    }
    InstructionOperand* op = range->FirstHint();
    int hint_index = -1;
    if (op != NULL && op->IsUnallocated()) {
      hint_index = UnallocatedOperand::cast(op)->virtual_register();
    }
    os_ << " " << parent_index << " " << hint_index;
    UseInterval* cur_interval = range->first_interval();
    while (cur_interval != NULL && range->Covers(cur_interval->start())) {
      os_ << " [" << cur_interval->start().Value() << ", "
          << cur_interval->end().Value() << "[";
      cur_interval = cur_interval->next();
    }

    UsePosition* current_pos = range->first_pos();
    while (current_pos != NULL) {
      if (current_pos->RegisterIsBeneficial() || FLAG_trace_all_uses) {
        os_ << " " << current_pos->pos().Value() << " M";
      }
      current_pos = current_pos->next();
    }

    os_ << " \"\"\n";
  }
}


std::ostream& operator<<(std::ostream& os, const AsC1VCompilation& ac) {
  Zone tmp_zone(ac.info_->isolate());
  GraphC1Visualizer(os, &tmp_zone).PrintCompilation(ac.info_);
  return os;
}


std::ostream& operator<<(std::ostream& os, const AsC1V& ac) {
  Zone tmp_zone(ac.schedule_->zone()->isolate());
  GraphC1Visualizer(os, &tmp_zone)
      .PrintSchedule(ac.phase_, ac.schedule_, ac.positions_, ac.instructions_);
  return os;
}


std::ostream& operator<<(std::ostream& os, const AsC1VAllocator& ac) {
  Zone tmp_zone(ac.allocator_->code()->zone()->isolate());
  GraphC1Visualizer(os, &tmp_zone).PrintAllocator(ac.phase_, ac.allocator_);
  return os;
}
}
}
}  // namespace v8::internal::compiler
