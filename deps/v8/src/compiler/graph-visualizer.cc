// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/graph-visualizer.h"

#include <memory>
#include <sstream>
#include <string>

#include "src/code-stubs.h"
#include "src/compiler/all-nodes.h"
#include "src/compiler/compiler-source-position-table.h"
#include "src/compiler/graph.h"
#include "src/compiler/node-origin-table.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/node.h"
#include "src/compiler/opcodes.h"
#include "src/compiler/operator-properties.h"
#include "src/compiler/operator.h"
#include "src/compiler/register-allocator.h"
#include "src/compiler/schedule.h"
#include "src/compiler/scheduler.h"
#include "src/interpreter/bytecodes.h"
#include "src/objects/script-inl.h"
#include "src/objects/shared-function-info.h"
#include "src/optimized-compilation-info.h"
#include "src/ostreams.h"
#include "src/source-position.h"

namespace v8 {
namespace internal {
namespace compiler {

const char* get_cached_trace_turbo_filename(OptimizedCompilationInfo* info) {
  if (!info->trace_turbo_filename()) {
    info->set_trace_turbo_filename(
        GetVisualizerLogFileName(info, FLAG_trace_turbo_path, nullptr, "json"));
  }
  return info->trace_turbo_filename();
}

TurboJsonFile::TurboJsonFile(OptimizedCompilationInfo* info,
                             std::ios_base::openmode mode)
    : std::ofstream(get_cached_trace_turbo_filename(info), mode) {}

TurboJsonFile::~TurboJsonFile() { flush(); }

TurboCfgFile::TurboCfgFile(Isolate* isolate)
    : std::ofstream(Isolate::GetTurboCfgFileName(isolate).c_str(),
                    std::ios_base::app) {}

TurboCfgFile::~TurboCfgFile() { flush(); }

std::ostream& operator<<(std::ostream& out,
                         const SourcePositionAsJSON& asJSON) {
  asJSON.sp.PrintJson(out);
  return out;
}

std::ostream& operator<<(std::ostream& out, const NodeOriginAsJSON& asJSON) {
  asJSON.no.PrintJson(out);
  return out;
}

void JsonPrintFunctionSource(std::ostream& os, int source_id,
                             std::unique_ptr<char[]> function_name,
                             Handle<Script> script, Isolate* isolate,
                             Handle<SharedFunctionInfo> shared, bool with_key) {
  if (with_key) os << "\"" << source_id << "\" : ";

  os << "{ ";
  os << "\"sourceId\": " << source_id;
  os << ", \"functionName\": \"" << function_name.get() << "\" ";

  int start = 0;
  int end = 0;
  if (!script.is_null() && !script->IsUndefined(isolate) && !shared.is_null()) {
    Object* source_name = script->name();
    os << ", \"sourceName\": \"";
    if (source_name->IsString()) {
      os << String::cast(source_name)->ToCString().get();
    }
    os << "\"";
    {
      DisallowHeapAllocation no_allocation;
      start = shared->StartPosition();
      end = shared->EndPosition();
      os << ", \"sourceText\": \"";
      int len = shared->EndPosition() - start;
      String::SubStringRange source(String::cast(script->source()), start, len);
      for (const auto& c : source) {
        os << AsEscapedUC16ForJSON(c);
      }
      os << "\"";
    }
  } else {
    os << ", \"sourceName\": \"\"";
    os << ", \"sourceText\": \"\"";
  }
  os << ", \"startPosition\": " << start;
  os << ", \"endPosition\": " << end;
  os << "}";
}

int SourceIdAssigner::GetIdFor(Handle<SharedFunctionInfo> shared) {
  for (unsigned i = 0; i < printed_.size(); i++) {
    if (printed_.at(i).is_identical_to(shared)) {
      source_ids_.push_back(i);
      return i;
    }
  }
  const int source_id = static_cast<int>(printed_.size());
  printed_.push_back(shared);
  source_ids_.push_back(source_id);
  return source_id;
}

namespace {

void JsonPrintInlinedFunctionInfo(
    std::ostream& os, int source_id, int inlining_id,
    const OptimizedCompilationInfo::InlinedFunctionHolder& h) {
  os << "\"" << inlining_id << "\" : ";
  os << "{ \"inliningId\" : " << inlining_id;
  os << ", \"sourceId\" : " << source_id;
  const SourcePosition position = h.position.position;
  if (position.IsKnown()) {
    os << ", \"inliningPosition\" : " << AsJSON(position);
  }
  os << "}";
}

}  // namespace

void JsonPrintAllSourceWithPositions(std::ostream& os,
                                     OptimizedCompilationInfo* info,
                                     Isolate* isolate) {
  AllowDeferredHandleDereference allow_deference_for_print_code;
  os << "\"sources\" : {";
  Handle<Script> script =
      (info->shared_info().is_null() || !info->shared_info()->script())
          ? Handle<Script>()
          : handle(Script::cast(info->shared_info()->script()), isolate);
  JsonPrintFunctionSource(os, -1,
                          info->shared_info().is_null()
                              ? std::unique_ptr<char[]>(new char[1]{0})
                              : info->shared_info()->DebugName()->ToCString(),
                          script, isolate, info->shared_info(), true);
  const auto& inlined = info->inlined_functions();
  SourceIdAssigner id_assigner(info->inlined_functions().size());
  for (unsigned id = 0; id < inlined.size(); id++) {
    os << ", ";
    Handle<SharedFunctionInfo> shared = inlined[id].shared_info;
    const int source_id = id_assigner.GetIdFor(shared);
    JsonPrintFunctionSource(os, source_id, shared->DebugName()->ToCString(),
                            handle(Script::cast(shared->script()), isolate),
                            isolate, shared, true);
  }
  os << "}, ";
  os << "\"inlinings\" : {";
  bool need_comma = false;
  for (unsigned id = 0; id < inlined.size(); id++) {
    if (need_comma) os << ", ";
    const int source_id = id_assigner.GetIdAt(id);
    JsonPrintInlinedFunctionInfo(os, source_id, id, inlined[id]);
    need_comma = true;
  }
  os << "}";
}

std::unique_ptr<char[]> GetVisualizerLogFileName(OptimizedCompilationInfo* info,
                                                 const char* optional_base_dir,
                                                 const char* phase,
                                                 const char* suffix) {
  EmbeddedVector<char, 256> filename(0);
  std::unique_ptr<char[]> debug_name = info->GetDebugName();
  int optimization_id = info->IsOptimizing() ? info->optimization_id() : 0;
  if (strlen(debug_name.get()) > 0) {
    SNPrintF(filename, "turbo-%s-%i", debug_name.get(), optimization_id);
  } else if (info->has_shared_info()) {
    SNPrintF(filename, "turbo-%p-%i",
             reinterpret_cast<void*>(info->shared_info()->address()),
             optimization_id);
  } else {
    SNPrintF(filename, "turbo-none-%i", optimization_id);
  }
  EmbeddedVector<char, 256> source_file(0);
  bool source_available = false;
  if (FLAG_trace_file_names && info->has_shared_info() &&
      info->shared_info()->script()->IsScript()) {
    Object* source_name = Script::cast(info->shared_info()->script())->name();
    if (source_name->IsString()) {
      String* str = String::cast(source_name);
      if (str->length() > 0) {
        SNPrintF(source_file, "%s", str->ToCString().get());
        std::replace(source_file.start(),
                     source_file.start() + source_file.length(), '/', '_');
        source_available = true;
      }
    }
  }
  std::replace(filename.start(), filename.start() + filename.length(), ' ',
               '_');

  EmbeddedVector<char, 256> base_dir;
  if (optional_base_dir != nullptr) {
    SNPrintF(base_dir, "%s%c", optional_base_dir,
             base::OS::DirectorySeparator());
  } else {
    base_dir[0] = '\0';
  }

  EmbeddedVector<char, 256> full_filename;
  if (phase == nullptr && !source_available) {
    SNPrintF(full_filename, "%s%s.%s", base_dir.start(), filename.start(),
             suffix);
  } else if (phase != nullptr && !source_available) {
    SNPrintF(full_filename, "%s%s-%s.%s", base_dir.start(), filename.start(),
             phase, suffix);
  } else if (phase == nullptr && source_available) {
    SNPrintF(full_filename, "%s%s_%s.%s", base_dir.start(), filename.start(),
             source_file.start(), suffix);
  } else {
    SNPrintF(full_filename, "%s%s_%s-%s.%s", base_dir.start(), filename.start(),
             source_file.start(), phase, suffix);
  }

  char* buffer = new char[full_filename.length() + 1];
  memcpy(buffer, full_filename.start(), full_filename.length());
  buffer[full_filename.length()] = '\0';
  return std::unique_ptr<char[]>(buffer);
}


static int SafeId(Node* node) { return node == nullptr ? -1 : node->id(); }
static const char* SafeMnemonic(Node* node) {
  return node == nullptr ? "null" : node->op()->mnemonic();
}

class JSONEscaped {
 public:
  explicit JSONEscaped(const std::ostringstream& os) : str_(os.str()) {}

  friend std::ostream& operator<<(std::ostream& os, const JSONEscaped& e) {
    for (char c : e.str_) PipeCharacter(os, c);
    return os;
  }

 private:
  static std::ostream& PipeCharacter(std::ostream& os, char c) {
    if (c == '"') return os << "\\\"";
    if (c == '\\') return os << "\\\\";
    if (c == '\b') return os << "\\b";
    if (c == '\f') return os << "\\f";
    if (c == '\n') return os << "\\n";
    if (c == '\r') return os << "\\r";
    if (c == '\t') return os << "\\t";
    return os << c;
  }

  const std::string str_;
};

class JSONGraphNodeWriter {
 public:
  JSONGraphNodeWriter(std::ostream& os, Zone* zone, const Graph* graph,
                      const SourcePositionTable* positions,
                      const NodeOriginTable* origins)
      : os_(os),
        all_(zone, graph, false),
        live_(zone, graph, true),
        positions_(positions),
        origins_(origins),
        first_node_(true) {}

  void Print() {
    for (Node* const node : all_.reachable) PrintNode(node);
    os_ << "\n";
  }

  void PrintNode(Node* node) {
    if (first_node_) {
      first_node_ = false;
    } else {
      os_ << ",\n";
    }
    std::ostringstream label, title, properties;
    node->op()->PrintTo(label, Operator::PrintVerbosity::kSilent);
    node->op()->PrintTo(title, Operator::PrintVerbosity::kVerbose);
    node->op()->PrintPropsTo(properties);
    os_ << "{\"id\":" << SafeId(node) << ",\"label\":\"" << JSONEscaped(label)
        << "\""
        << ",\"title\":\"" << JSONEscaped(title) << "\""
        << ",\"live\": " << (live_.IsLive(node) ? "true" : "false")
        << ",\"properties\":\"" << JSONEscaped(properties) << "\"";
    IrOpcode::Value opcode = node->opcode();
    if (IrOpcode::IsPhiOpcode(opcode)) {
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
    if (positions_ != nullptr) {
      SourcePosition position = positions_->GetSourcePosition(node);
      if (position.IsKnown()) {
        os_ << ", \"sourcePosition\" : " << AsJSON(position);
      }
    }
    if (origins_) {
      NodeOrigin origin = origins_->GetNodeOrigin(node);
      if (origin.IsKnown()) {
        os_ << ", \"origin\" : " << AsJSON(origin);
      }
    }
    os_ << ",\"opcode\":\"" << IrOpcode::Mnemonic(node->opcode()) << "\"";
    os_ << ",\"control\":" << (NodeProperties::IsControl(node) ? "true"
                                                               : "false");
    os_ << ",\"opinfo\":\"" << node->op()->ValueInputCount() << " v "
        << node->op()->EffectInputCount() << " eff "
        << node->op()->ControlInputCount() << " ctrl in, "
        << node->op()->ValueOutputCount() << " v "
        << node->op()->EffectOutputCount() << " eff "
        << node->op()->ControlOutputCount() << " ctrl out\"";
    if (NodeProperties::IsTyped(node)) {
      Type type = NodeProperties::GetType(node);
      std::ostringstream type_out;
      type.PrintTo(type_out);
      os_ << ",\"type\":\"" << JSONEscaped(type_out) << "\"";
    }
    os_ << "}";
  }

 private:
  std::ostream& os_;
  AllNodes all_;
  AllNodes live_;
  const SourcePositionTable* positions_;
  const NodeOriginTable* origins_;
  bool first_node_;

  DISALLOW_COPY_AND_ASSIGN(JSONGraphNodeWriter);
};


class JSONGraphEdgeWriter {
 public:
  JSONGraphEdgeWriter(std::ostream& os, Zone* zone, const Graph* graph)
      : os_(os), all_(zone, graph, false), first_edge_(true) {}

  void Print() {
    for (Node* const node : all_.reachable) PrintEdges(node);
    os_ << "\n";
  }

  void PrintEdges(Node* node) {
    for (int i = 0; i < node->InputCount(); i++) {
      Node* input = node->InputAt(i);
      if (input == nullptr) continue;
      PrintEdge(node, i, input);
    }
  }

  void PrintEdge(Node* from, int index, Node* to) {
    if (first_edge_) {
      first_edge_ = false;
    } else {
      os_ << ",\n";
    }
    const char* edge_type = nullptr;
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

std::ostream& operator<<(std::ostream& os, const GraphAsJSON& ad) {
  AccountingAllocator allocator;
  Zone tmp_zone(&allocator, ZONE_NAME);
  os << "{\n\"nodes\":[";
  JSONGraphNodeWriter(os, &tmp_zone, &ad.graph, ad.positions, ad.origins)
      .Print();
  os << "],\n\"edges\":[";
  JSONGraphEdgeWriter(os, &tmp_zone, &ad.graph).Print();
  os << "]}";
  return os;
}


class GraphC1Visualizer {
 public:
  GraphC1Visualizer(std::ostream& os, Zone* zone);  // NOLINT

  void PrintCompilation(const OptimizedCompilationInfo* info);
  void PrintSchedule(const char* phase, const Schedule* schedule,
                     const SourcePositionTable* positions,
                     const InstructionSequence* instructions);
  void PrintLiveRanges(const char* phase, const RegisterAllocationData* data);
  Zone* zone() const { return zone_; }

 private:
  void PrintIndent();
  void PrintStringProperty(const char* name, const char* value);
  void PrintLongProperty(const char* name, int64_t value);
  void PrintIntProperty(const char* name, int value);
  void PrintBlockProperty(const char* name, int rpo_number);
  void PrintNodeId(Node* n);
  void PrintNode(Node* n);
  void PrintInputs(Node* n);
  template <typename InputIterator>
  void PrintInputs(InputIterator* i, int count, const char* prefix);
  void PrintType(Node* node);

  void PrintLiveRange(const LiveRange* range, const char* type, int vreg);
  void PrintLiveRangeChain(const TopLevelLiveRange* range, const char* type);

  class Tag final {
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
      DCHECK_LE(0, visualizer_->indent_);
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


void GraphC1Visualizer::PrintBlockProperty(const char* name, int rpo_number) {
  PrintIndent();
  os_ << name << " \"B" << rpo_number << "\"\n";
}


void GraphC1Visualizer::PrintIntProperty(const char* name, int value) {
  PrintIndent();
  os_ << name << " " << value << "\n";
}

void GraphC1Visualizer::PrintCompilation(const OptimizedCompilationInfo* info) {
  Tag tag(this, "compilation");
  std::unique_ptr<char[]> name = info->GetDebugName();
  if (info->IsOptimizing()) {
    PrintStringProperty("name", name.get());
    PrintIndent();
    os_ << "method \"" << name.get() << ":" << info->optimization_id()
        << "\"\n";
  } else {
    PrintStringProperty("name", name.get());
    PrintStringProperty("method", "stub");
  }
  PrintLongProperty(
      "date",
      static_cast<int64_t>(V8::GetCurrentPlatform()->CurrentClockTimeMillis()));
}


void GraphC1Visualizer::PrintNodeId(Node* n) { os_ << "n" << SafeId(n); }


void GraphC1Visualizer::PrintNode(Node* n) {
  PrintNodeId(n);
  os_ << " " << *n->op() << " ";
  PrintInputs(n);
}


template <typename InputIterator>
void GraphC1Visualizer::PrintInputs(InputIterator* i, int count,
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
  auto i = node->inputs().begin();
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
    Type type = NodeProperties::GetType(node);
    os_ << " type:" << type;
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
    PrintBlockProperty("name", current->rpo_number());
    PrintIntProperty("from_bci", -1);
    PrintIntProperty("to_bci", -1);

    PrintIndent();
    os_ << "predecessors";
    for (BasicBlock* predecessor : current->predecessors()) {
      os_ << " \"B" << predecessor->rpo_number() << "\"";
    }
    os_ << "\n";

    PrintIndent();
    os_ << "successors";
    for (BasicBlock* successor : current->successors()) {
      os_ << " \"B" << successor->rpo_number() << "\"";
    }
    os_ << "\n";

    PrintIndent();
    os_ << "xhandlers\n";

    PrintIndent();
    os_ << "flags\n";

    if (current->dominator() != nullptr) {
      PrintBlockProperty("dominator", current->dominator()->rpo_number());
    }

    PrintIntProperty("loop_depth", current->loop_depth());

    const InstructionBlock* instruction_block =
        instructions->InstructionBlockAt(
            RpoNumber::FromInt(current->rpo_number()));
    if (instruction_block->code_start() >= 0) {
      int first_index = instruction_block->first_instruction_index();
      int last_index = instruction_block->last_instruction_index();
      PrintIntProperty(
          "first_lir_id",
          LifetimePosition::GapFromInstructionIndex(first_index).value());
      PrintIntProperty("last_lir_id",
                       LifetimePosition::InstructionFromInstructionIndex(
                           last_index).value());
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
        if (positions != nullptr) {
          SourcePosition position = positions->GetSourcePosition(node);
          if (position.IsKnown()) {
            os_ << " pos:";
            if (position.isInlined()) {
              os_ << "inlining(" << position.InliningId() << "),";
            }
            os_ << position.ScriptOffset();
          }
        }
        os_ << " <|@\n";
      }

      BasicBlock::Control control = current->control();
      if (control != BasicBlock::kNone) {
        PrintIndent();
        os_ << "0 0 ";
        if (current->control_input() != nullptr) {
          PrintNode(current->control_input());
        } else {
          os_ << -1 - current->rpo_number() << " Goto";
        }
        os_ << " ->";
        for (BasicBlock* successor : current->successors()) {
          os_ << " B" << successor->rpo_number();
        }
        if (FLAG_trace_turbo_types && current->control_input() != nullptr) {
          os_ << " ";
          PrintType(current->control_input());
        }
        os_ << " <|@\n";
      }
    }

    if (instructions != nullptr) {
      Tag LIR_tag(this, "LIR");
      for (int j = instruction_block->first_instruction_index();
           j <= instruction_block->last_instruction_index(); j++) {
        PrintIndent();
        PrintableInstruction printable = {RegisterConfiguration::Default(),
                                          instructions->InstructionAt(j)};
        os_ << j << " " << printable << " <|@\n";
      }
    }
  }
}


void GraphC1Visualizer::PrintLiveRanges(const char* phase,
                                        const RegisterAllocationData* data) {
  Tag tag(this, "intervals");
  PrintStringProperty("name", phase);

  for (const TopLevelLiveRange* range : data->fixed_double_live_ranges()) {
    PrintLiveRangeChain(range, "fixed");
  }

  for (const TopLevelLiveRange* range : data->fixed_live_ranges()) {
    PrintLiveRangeChain(range, "fixed");
  }

  for (const TopLevelLiveRange* range : data->live_ranges()) {
    PrintLiveRangeChain(range, "object");
  }
}

void GraphC1Visualizer::PrintLiveRangeChain(const TopLevelLiveRange* range,
                                            const char* type) {
  if (range == nullptr || range->IsEmpty()) return;
  int vreg = range->vreg();
  for (const LiveRange* child = range; child != nullptr;
       child = child->next()) {
    PrintLiveRange(child, type, vreg);
  }
}

void GraphC1Visualizer::PrintLiveRange(const LiveRange* range, const char* type,
                                       int vreg) {
  if (range != nullptr && !range->IsEmpty()) {
    PrintIndent();
    os_ << vreg << ":" << range->relative_id() << " " << type;
    if (range->HasRegisterAssigned()) {
      AllocatedOperand op = AllocatedOperand::cast(range->GetAssignedOperand());
      const auto config = RegisterConfiguration::Default();
      if (op.IsRegister()) {
        os_ << " \"" << config->GetGeneralRegisterName(op.register_code())
            << "\"";
      } else if (op.IsDoubleRegister()) {
        os_ << " \"" << config->GetDoubleRegisterName(op.register_code())
            << "\"";
      } else {
        DCHECK(op.IsFloatRegister());
        os_ << " \"" << config->GetFloatRegisterName(op.register_code())
            << "\"";
      }
    } else if (range->spilled()) {
      const TopLevelLiveRange* top = range->TopLevel();
      int index = -1;
      if (top->HasSpillRange()) {
        index = kMaxInt;  // This hasn't been set yet.
      } else if (top->GetSpillOperand()->IsConstant()) {
        os_ << " \"const(nostack):"
            << ConstantOperand::cast(top->GetSpillOperand())->virtual_register()
            << "\"";
      } else {
        index = AllocatedOperand::cast(top->GetSpillOperand())->index();
        if (IsFloatingPoint(top->representation())) {
          os_ << " \"fp_stack:" << index << "\"";
        } else {
          os_ << " \"stack:" << index << "\"";
        }
      }
    }

    // The toplevel range is always suffixed with :0. Use that as parent.
    os_ << " " << vreg << ":0";

    // TODO(herhut) Find something useful to print for the hint field
    os_ << " unknown";

    for (const UseInterval* interval = range->first_interval();
         interval != nullptr; interval = interval->next()) {
      os_ << " [" << interval->start().value() << ", "
          << interval->end().value() << "[";
    }

    UsePosition* current_pos = range->first_pos();
    while (current_pos != nullptr) {
      if (current_pos->RegisterIsBeneficial() || FLAG_trace_all_uses) {
        os_ << " " << current_pos->pos().value() << " M";
      }
      current_pos = current_pos->next();
    }

    os_ << " \"\"\n";
  }
}


std::ostream& operator<<(std::ostream& os, const AsC1VCompilation& ac) {
  AccountingAllocator allocator;
  Zone tmp_zone(&allocator, ZONE_NAME);
  GraphC1Visualizer(os, &tmp_zone).PrintCompilation(ac.info_);
  return os;
}


std::ostream& operator<<(std::ostream& os, const AsC1V& ac) {
  AccountingAllocator allocator;
  Zone tmp_zone(&allocator, ZONE_NAME);
  GraphC1Visualizer(os, &tmp_zone)
      .PrintSchedule(ac.phase_, ac.schedule_, ac.positions_, ac.instructions_);
  return os;
}


std::ostream& operator<<(std::ostream& os,
                         const AsC1VRegisterAllocationData& ac) {
  AccountingAllocator allocator;
  Zone tmp_zone(&allocator, ZONE_NAME);
  GraphC1Visualizer(os, &tmp_zone).PrintLiveRanges(ac.phase_, ac.data_);
  return os;
}

const int kUnvisited = 0;
const int kOnStack = 1;
const int kVisited = 2;

std::ostream& operator<<(std::ostream& os, const AsRPO& ar) {
  AccountingAllocator allocator;
  Zone local_zone(&allocator, ZONE_NAME);

  // Do a post-order depth-first search on the RPO graph. For every node,
  // print:
  //
  //   - the node id
  //   - the operator mnemonic
  //   - in square brackets its parameter (if present)
  //   - in parentheses the list of argument ids and their mnemonics
  //   - the node type (if it is typed)

  // Post-order guarantees that all inputs of a node will be printed before
  // the node itself, if there are no cycles. Any cycles are broken
  // arbitrarily.

  ZoneVector<byte> state(ar.graph.NodeCount(), kUnvisited, &local_zone);
  ZoneStack<Node*> stack(&local_zone);

  stack.push(ar.graph.end());
  state[ar.graph.end()->id()] = kOnStack;
  while (!stack.empty()) {
    Node* n = stack.top();
    bool pop = true;
    for (Node* const i : n->inputs()) {
      if (state[i->id()] == kUnvisited) {
        state[i->id()] = kOnStack;
        stack.push(i);
        pop = false;
        break;
      }
    }
    if (pop) {
      state[n->id()] = kVisited;
      stack.pop();
      os << "#" << n->id() << ":" << *n->op() << "(";
      // Print the inputs.
      int j = 0;
      for (Node* const i : n->inputs()) {
        if (j++ > 0) os << ", ";
        os << "#" << SafeId(i) << ":" << SafeMnemonic(i);
      }
      os << ")";
      // Print the node type, if any.
      if (NodeProperties::IsTyped(n)) {
        os << "  [Type: " << NodeProperties::GetType(n) << "]";
      }
      os << std::endl;
    }
  }
  return os;
}

namespace {

void PrintIndent(std::ostream& os, int indent) {
  os << "     ";
  for (int i = 0; i < indent; i++) {
    os << ". ";
  }
}

void PrintScheduledNode(std::ostream& os, int indent, Node* n) {
  PrintIndent(os, indent);
  os << "#" << n->id() << ":" << *n->op() << "(";
  // Print the inputs.
  int j = 0;
  for (Node* const i : n->inputs()) {
    if (j++ > 0) os << ", ";
    os << "#" << SafeId(i) << ":" << SafeMnemonic(i);
  }
  os << ")";
  // Print the node type, if any.
  if (NodeProperties::IsTyped(n)) {
    os << "  [Type: " << NodeProperties::GetType(n) << "]";
  }
}

void PrintScheduledGraph(std::ostream& os, const Schedule* schedule) {
  const BasicBlockVector* rpo = schedule->rpo_order();
  for (size_t i = 0; i < rpo->size(); i++) {
    BasicBlock* current = (*rpo)[i];
    int indent = current->loop_depth();

    os << "  + Block B" << current->rpo_number() << " (pred:";
    for (BasicBlock* predecessor : current->predecessors()) {
      os << " B" << predecessor->rpo_number();
    }
    if (current->IsLoopHeader()) {
      os << ", loop until B" << current->loop_end()->rpo_number();
    } else if (current->loop_header()) {
      os << ", in loop B" << current->loop_header()->rpo_number();
    }
    os << ")" << std::endl;

    for (BasicBlock::const_iterator i = current->begin(); i != current->end();
         ++i) {
      Node* node = *i;
      PrintScheduledNode(os, indent, node);
      os << std::endl;
    }

    if (current->SuccessorCount() > 0) {
      if (current->control_input() != nullptr) {
        PrintScheduledNode(os, indent, current->control_input());
      } else {
        PrintIndent(os, indent);
        os << "Goto";
      }
      os << " ->";

      bool isFirst = true;
      for (BasicBlock* successor : current->successors()) {
        if (isFirst) {
          isFirst = false;
        } else {
          os << ",";
        }
        os << " B" << successor->rpo_number();
      }
      os << std::endl;
    } else {
      DCHECK_NULL(current->control_input());
    }
  }
}

}  // namespace

std::ostream& operator<<(std::ostream& os, const AsScheduledGraph& scheduled) {
  PrintScheduledGraph(os, scheduled.schedule);
  return os;
}

std::ostream& operator<<(std::ostream& os, const InstructionOperandAsJSON& o) {
  const RegisterConfiguration* conf = o.register_configuration_;
  const InstructionOperand* op = o.op_;
  const InstructionSequence* code = o.code_;
  os << "{";
  switch (op->kind()) {
    case InstructionOperand::UNALLOCATED: {
      const UnallocatedOperand* unalloc = UnallocatedOperand::cast(op);
      os << "\"type\": \"unallocated\", ";
      os << "\"text\": \"v" << unalloc->virtual_register() << "\"";
      if (unalloc->basic_policy() == UnallocatedOperand::FIXED_SLOT) {
        os << ",\"tooltip\": \"FIXED_SLOT: " << unalloc->fixed_slot_index()
           << "\"";
        break;
      }
      switch (unalloc->extended_policy()) {
        case UnallocatedOperand::NONE:
          break;
        case UnallocatedOperand::FIXED_REGISTER: {
          os << ",\"tooltip\": \"FIXED_REGISTER: "
             << conf->GetGeneralRegisterName(unalloc->fixed_register_index())
             << "\"";
          break;
        }
        case UnallocatedOperand::FIXED_FP_REGISTER: {
          os << ",\"tooltip\": \"FIXED_FP_REGISTER: "
             << conf->GetDoubleRegisterName(unalloc->fixed_register_index())
             << "\"";
          break;
        }
        case UnallocatedOperand::MUST_HAVE_REGISTER: {
          os << ",\"tooltip\": \"MUST_HAVE_REGISTER\"";
          break;
        }
        case UnallocatedOperand::MUST_HAVE_SLOT: {
          os << ",\"tooltip\": \"MUST_HAVE_SLOT\"";
          break;
        }
        case UnallocatedOperand::SAME_AS_FIRST_INPUT: {
          os << ",\"tooltip\": \"SAME_AS_FIRST_INPUT\"";
          break;
        }
        case UnallocatedOperand::REGISTER_OR_SLOT: {
          os << ",\"tooltip\": \"REGISTER_OR_SLOT\"";
          break;
        }
        case UnallocatedOperand::REGISTER_OR_SLOT_OR_CONSTANT: {
          os << ",\"tooltip\": \"REGISTER_OR_SLOT_OR_CONSTANT\"";
          break;
        }
      }
      break;
    }
    case InstructionOperand::CONSTANT: {
      int vreg = ConstantOperand::cast(op)->virtual_register();
      os << "\"type\": \"constant\", ";
      os << "\"text\": \"v" << vreg << "\",";
      os << "\"tooltip\": \"";
      std::stringstream tooltip;
      tooltip << code->GetConstant(vreg);
      for (const auto& c : tooltip.str()) {
        os << AsEscapedUC16ForJSON(c);
      }
      os << "\"";
      break;
    }
    case InstructionOperand::IMMEDIATE: {
      os << "\"type\": \"immediate\", ";
      const ImmediateOperand* imm = ImmediateOperand::cast(op);
      switch (imm->type()) {
        case ImmediateOperand::INLINE: {
          os << "\"text\": \"#" << imm->inline_value() << "\"";
          break;
        }
        case ImmediateOperand::INDEXED: {
          int index = imm->indexed_value();
          os << "\"text\": \"imm:" << index << "\",";
          os << "\"tooltip\": \"";
          std::stringstream tooltip;
          tooltip << code->GetImmediate(imm);
          for (const auto& c : tooltip.str()) {
            os << AsEscapedUC16ForJSON(c);
          }
          os << "\"";
          break;
        }
      }
      break;
    }
    case InstructionOperand::EXPLICIT:
    case InstructionOperand::ALLOCATED: {
      const LocationOperand* allocated = LocationOperand::cast(op);
      os << "\"type\": ";
      if (allocated->IsExplicit()) {
        os << "\"explicit\", ";
      } else {
        os << "\"allocated\", ";
      }
      os << "\"text\": \"";
      if (op->IsStackSlot()) {
        os << "stack:" << allocated->index();
      } else if (op->IsFPStackSlot()) {
        os << "fp_stack:" << allocated->index();
      } else if (op->IsRegister()) {
        os << conf->GetGeneralOrSpecialRegisterName(allocated->register_code());
      } else if (op->IsDoubleRegister()) {
        os << conf->GetDoubleRegisterName(allocated->register_code());
      } else if (op->IsFloatRegister()) {
        os << conf->GetFloatRegisterName(allocated->register_code());
      } else {
        DCHECK(op->IsSimd128Register());
        os << conf->GetSimd128RegisterName(allocated->register_code());
      }
      os << "\",";
      os << "\"tooltip\": \""
         << MachineReprToString(allocated->representation()) << "\"";
      break;
    }
    case InstructionOperand::INVALID:
      UNREACHABLE();
  }
  os << "}";
  return os;
}

std::ostream& operator<<(std::ostream& os, const InstructionAsJSON& i) {
  const Instruction* instr = i.instr_;
  InstructionOperandAsJSON json_op = {i.register_configuration_, nullptr,
                                      i.code_};

  os << "{";
  os << "\"id\": " << i.index_ << ",";
  os << "\"opcode\": \"" << ArchOpcodeField::decode(instr->opcode()) << "\",";
  os << "\"flags\": \"";
  FlagsMode fm = FlagsModeField::decode(instr->opcode());
  AddressingMode am = AddressingModeField::decode(instr->opcode());
  if (am != kMode_None) {
    os << " : " << AddressingModeField::decode(instr->opcode());
  }
  if (fm != kFlags_none) {
    os << " && " << fm << " if "
       << FlagsConditionField::decode(instr->opcode());
  }
  os << "\",";

  os << "\"gaps\": [";
  for (int i = Instruction::FIRST_GAP_POSITION;
       i <= Instruction::LAST_GAP_POSITION; i++) {
    if (i != Instruction::FIRST_GAP_POSITION) os << ",";
    os << "[";
    const ParallelMove* pm = instr->parallel_moves()[i];
    if (pm == nullptr) {
      os << "]";
      continue;
    }
    bool first = true;
    for (MoveOperands* move : *pm) {
      if (move->IsEliminated()) continue;
      if (!first) os << ",";
      first = false;
      json_op.op_ = &move->destination();
      os << "[" << json_op << ",";
      json_op.op_ = &move->source();
      os << json_op << "]";
    }
    os << "]";
  }
  os << "],";

  os << "\"outputs\": [";
  bool need_comma = false;
  for (size_t i = 0; i < instr->OutputCount(); i++) {
    if (need_comma) os << ",";
    need_comma = true;
    json_op.op_ = instr->OutputAt(i);
    os << json_op;
  }
  os << "],";

  os << "\"inputs\": [";
  need_comma = false;
  for (size_t i = 0; i < instr->InputCount(); i++) {
    if (need_comma) os << ",";
    need_comma = true;
    json_op.op_ = instr->InputAt(i);
    os << json_op;
  }
  os << "],";

  os << "\"temps\": [";
  need_comma = false;
  for (size_t i = 0; i < instr->TempCount(); i++) {
    if (need_comma) os << ",";
    need_comma = true;
    json_op.op_ = instr->TempAt(i);
    os << json_op;
  }
  os << "]";
  os << "}";

  return os;
}

std::ostream& operator<<(std::ostream& os, const InstructionBlockAsJSON& b) {
  const InstructionBlock* block = b.block_;
  const InstructionSequence* code = b.code_;
  os << "{";
  os << "\"id\": " << block->rpo_number() << ",";
  os << "\"deferred\": " << block->IsDeferred() << ",";
  os << "\"loop_header\": " << block->IsLoopHeader() << ",";
  if (block->IsLoopHeader()) {
    os << "\"loop_end\": " << block->loop_end() << ",";
  }
  os << "\"predecessors\": [";
  bool need_comma = false;
  for (RpoNumber pred : block->predecessors()) {
    if (need_comma) os << ",";
    need_comma = true;
    os << pred.ToInt();
  }
  os << "],";
  os << "\"successors\": [";
  need_comma = false;
  for (RpoNumber succ : block->successors()) {
    if (need_comma) os << ",";
    need_comma = true;
    os << succ.ToInt();
  }
  os << "],";
  os << "\"phis\": [";
  bool needs_comma = false;
  InstructionOperandAsJSON json_op = {b.register_configuration_, nullptr, code};
  for (const PhiInstruction* phi : block->phis()) {
    if (needs_comma) os << ",";
    needs_comma = true;
    json_op.op_ = &phi->output();
    os << "{\"output\" : " << json_op << ",";
    os << "\"operands\": [";
    bool op_needs_comma = false;
    for (int input : phi->operands()) {
      if (op_needs_comma) os << ",";
      op_needs_comma = true;
      os << "\"v" << input << "\"";
    }
    os << "]}";
  }
  os << "],";

  os << "\"instructions\": [";
  InstructionAsJSON json_instr = {b.register_configuration_, -1, nullptr, code};
  need_comma = false;
  for (int j = block->first_instruction_index();
       j <= block->last_instruction_index(); j++) {
    if (need_comma) os << ",";
    need_comma = true;
    json_instr.index_ = j;
    json_instr.instr_ = code->InstructionAt(j);
    os << json_instr;
  }
  os << "]";
  os << "}";

  return os;
}

std::ostream& operator<<(std::ostream& os, const InstructionSequenceAsJSON& s) {
  const InstructionSequence* code = s.sequence_;

  os << "\"blocks\": [";
  InstructionBlockAsJSON json_block = {s.register_configuration_, nullptr,
                                       code};

  bool need_comma = false;
  for (int i = 0; i < code->InstructionBlockCount(); i++) {
    if (need_comma) os << ",";
    need_comma = true;
    json_block.block_ = code->InstructionBlockAt(RpoNumber::FromInt(i));
    os << json_block;
  }
  os << "]";

  return os;
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
