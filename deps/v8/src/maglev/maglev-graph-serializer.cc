// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/maglev/maglev-graph-serializer.h"

#include <algorithm>
#include <iomanip>
#include <ostream>
#include <sstream>
#include <unordered_map>
#include <vector>

#include "src/base/platform/platform.h"
#include "src/base/string-format.h"
#include "src/base/vector.h"
#include "src/flags/flags.h"
#include "src/maglev/maglev-basic-block.h"
#include "src/maglev/maglev-compilation-info.h"
#include "src/maglev/maglev-graph-labeller.h"
#include "src/maglev/maglev-graph-processor.h"
#include "src/maglev/maglev-graph.h"
#include "src/maglev/maglev-ir.h"
#include "src/objects/bytecode-array.h"
#include "src/objects/code-inl.h"
#include "src/objects/feedback-vector.h"
#include "src/objects/js-function-inl.h"
#include "src/objects/script-inl.h"
#include "src/objects/shared-function-info-inl.h"
#include "src/objects/string-inl.h"
#include "src/objects/string.h"
#include "src/utils/ostreams.h"
#include "src/utils/utils.h"

namespace v8 {
namespace internal {
namespace maglev {

namespace {

const char* RepresentationSuperscript(ValueRepresentation repr) {
  switch (repr) {
    case ValueRepresentation::kTagged:
      return "\\u1D40";
    case ValueRepresentation::kInt32:
      return "\\u1D35";
    case ValueRepresentation::kUint32:
      return "\\u1D41";
    case ValueRepresentation::kFloat64:
      return "\\u1DA0";
    case ValueRepresentation::kHoleyFloat64:
      return "\\u02B0\\u1DA0";
    case ValueRepresentation::kIntPtr:
    case ValueRepresentation::kRawPtr:
    case ValueRepresentation::kNone:
      UNREACHABLE();
  }
}

std::unique_ptr<char[]> GetMaglevVisualizerLogFileName(
    MaglevCompilationInfo* info, const char* optional_base_dir,
    const char* phase, const char* suffix) {
  base::EmbeddedVector<char, 256> filename(0);
  std::string debug_name = info->function_name();
  const char* file_prefix = info->is_turbolev()
                                ? v8_flags.trace_turbo_file_prefix.value()
                                : v8_flags.trace_maglev_file_prefix.value();
  int optimization_id = info->optimization_id();

  if (debug_name.length() > 0) {
    SNPrintF(filename, "%s-%s-%i", file_prefix, debug_name.c_str(),
             optimization_id);
  } else {
    SNPrintF(filename, "%s-none-%i", file_prefix, optimization_id);
  }

  std::replace(filename.begin(), filename.begin() + filename.size(), '/', '_');
  std::replace(filename.begin(), filename.begin() + filename.size(), ' ', '_');
  std::replace(filename.begin(), filename.begin() + filename.size(), ':', '-');
#if V8_OS_WIN
  std::replace(filename.begin(), filename.begin() + filename.size(), '<', '{');
  std::replace(filename.begin(), filename.begin() + filename.size(), '>', '}');
#endif

  base::EmbeddedVector<char, 256> base_dir;
  if (optional_base_dir != nullptr) {
    SNPrintF(base_dir, "%s%c", optional_base_dir,
             base::OS::DirectorySeparator());
  } else {
    base_dir[0] = '\0';
  }

  base::EmbeddedVector<char, 256> full_filename;
  if (phase == nullptr) {
    SNPrintF(full_filename, "%s%s.%s", base_dir.begin(), filename.begin(),
             suffix);
  } else {
    SNPrintF(full_filename, "%s%s-%s.%s", base_dir.begin(), filename.begin(),
             phase, suffix);
  }

  char* buffer = new char[full_filename.size()];
  memcpy(buffer, full_filename.begin(), full_filename.size());
  return std::unique_ptr<char[]>(buffer);
}

class JSONMaglevGraphWriter {
 public:
  JSONMaglevGraphWriter(std::ostream& os, Graph* graph)
      : os_(os), graph_(graph), labeller_(graph->graph_labeller()) {
    DCHECK_NOT_NULL(labeller_);
  }

  void Print() {
    InitializeVirtualNodeIds();
    os_ << "{\n\"nodes\":[";
    PrintNodes();
    os_ << "\n],\n\"edges\":[";
    PrintEdges();
    os_ << "\n],\n\"blocks\":[";
    PrintBlocks();
    os_ << "\n]}";
  }

 private:
  class VirtualIdAssigner;
  class NodePrinter;

  void PrintDeoptFrameAndDependencies(const DeoptFrame* frame,
                                      const BasicBlock* block, bool* first) {
    if (frame == nullptr) return;
    if (printed_deopt_frames_.find(frame) != printed_deopt_frames_.end()) {
      return;
    }

    if (frame->parent()) {
      PrintDeoptFrameAndDependencies(frame->parent(), block, first);
    }

    VirtualObjectList vos = frame->GetVirtualObjects();
    for (VirtualObject* vo : vos) {
      PrintVirtualObjectAndDependencies(vo, block, first);
    }

    int id = deopt_frame_ids_[frame];
    PrintDeoptFrameNode(frame, id, block, first);
    printed_deopt_frames_.insert(frame);
  }

  void PrintVirtualObjectAndDependencies(const VirtualObject* vo,
                                         const BasicBlock* block, bool* first) {
    if (vo == nullptr) return;
    if (printed_virtual_objects_.find(vo) != printed_virtual_objects_.end()) {
      return;
    }

    for (int i = 0; i < vo->slot_count(); ++i) {
      ValueNode* val = vo->get_by_index(i);
      if (val && val->Is<VirtualObject>()) {
        PrintVirtualObjectAndDependencies(val->Cast<VirtualObject>(), block,
                                          first);
      }
    }

    int id = virtual_object_ids_[vo];
    PrintVirtualObjectNode(vo, id, block, first);
    printed_virtual_objects_.insert(vo);
  }
  void PrintNodeAndDependencies(const NodeBase* node, const BasicBlock* block,
                                bool* first) {
    if (node->properties().has_eager_deopt_info()) {
      PrintDeoptFrameAndDependencies(&node->eager_deopt_info()->top_frame(),
                                     block, first);
    }
    if (node->properties().can_lazy_deopt()) {
      PrintDeoptFrameAndDependencies(&node->lazy_deopt_info()->top_frame(),
                                     block, first);
    }
    PrintNode(node, block, first);
  }

  void PrintNodes() {
    GraphProcessor<NodePrinter> printer(this);
    printer.ProcessGraph(graph_);
  }

  void PrintNode(const NodeBase* node, const BasicBlock* block, bool* first) {
    if (!*first) os_ << ",\n";
    *first = false;

    int id = labeller_->NodeId(node);
    os_ << "{\"id\":" << id << ",";
    if (node->Is<Phi>()) {
      const Phi* phi = node->Cast<Phi>();
      os_ << "\"title\":\"Phi"
          << RepresentationSuperscript(phi->value_representation());
      if (phi->is_exception_phi()) {
        os_ << "\\u2091";
      }
      os_ << "\",";
    } else {
      os_ << "\"title\":\"" << OpcodeToString(node->opcode()) << "\",";
    }
    if (block) {
      os_ << "\"block_id\":" << block->id() << ",";
    } else {
      os_ << "\"block_id\":0,";
    }
    if (node->Is<ControlNode>() || node->properties().can_throw()) {
      os_ << "\"control\":true,";
    }
    os_ << "\"op_effects\":\"\"";
    if (node->Is<Phi>()) {
      const Phi* phi = node->Cast<Phi>();
      os_ << ", \"properties\":\"Owner: " << phi->owner().ToString() << "\"";
    }

    const MaglevGraphLabeller::Provenance& provenance =
        labeller_->GetNodeProvenance(node);
    if (provenance.position.IsKnown()) {
      os_ << ", \"sourcePosition\":{\"scriptOffset\":"
          << provenance.position.ScriptOffset()
          << ",\"inliningId\":" << provenance.position.InliningId() << "}";
    }
    os_ << "}";
  }

  void PrintEdges() {
    bool first = true;
    for (BasicBlock* block : *graph_) {
      if (block->is_dead()) continue;

      if (block->has_phi()) {
        for (const Phi* phi : *block->phis()) {
          PrintEdgesForNode(phi, block, &first);
        }
      }

      for (const Node* node : block->nodes()) {
        if (node == nullptr) continue;
        PrintEdgesForNode(node, block, &first);
      }

      if (block->control_node()) {
        PrintEdgesForNode(block->control_node(), block, &first);
      }
    }

    for (auto const& [frame, id] : deopt_frame_ids_) {
      PrintEdgesForDeoptFrame(frame, id, &first);
    }

    for (auto const& [vo, id] : virtual_object_ids_) {
      PrintEdgesForVirtualObject(vo, id, &first);
    }
  }

  void PrintEdgesForNode(const NodeBase* node, const BasicBlock* block,
                         bool* first) {
    int target_id = labeller_->NodeId(node);
    for (int i = 0; i < node->input_count(); ++i) {
      const ValueNode* input = node->input_node(i);
      int source_id = labeller_->NodeId(input);
      if (!*first) os_ << ",\n";
      *first = false;
      os_ << "{\"source\":" << source_id << ",";
      os_ << "\"target\":" << target_id << ",";
      os_ << "\"type\":\"value\"}";
    }

    if (node->Is<Phi>()) {
      const Phi* phi = node->Cast<Phi>();
      if (phi->is_exception_phi()) {
        auto it = exception_sources_.find(block);
        if (it != exception_sources_.end()) {
          std::unordered_set<int> printed_sources;
          for (const NodeBase* source : it->second) {
            const NodeBase* value_source = nullptr;
            if (phi->owner() == interpreter::Register::virtual_accumulator()) {
              if (source->opcode() == Opcode::kThrow) {
                value_source = source->input_node(0);
              } else {
                value_source = source;
              }
            } else if (source->properties().can_lazy_deopt()) {
              const ExceptionHandlerInfo* handler_info =
                  source->exception_handler_info();
              NodeBase* non_const_source = const_cast<NodeBase*>(source);
              const InterpretedDeoptFrame& lazy_frame =
                  non_const_source->lazy_deopt_info()
                      ->GetFrameForExceptionHandler(handler_info);
              value_source = lazy_frame.frame_state()->GetValueOf(
                  phi->owner(), lazy_frame.unit());
            }
            if (value_source != nullptr) {
              int source_id = labeller_->NodeId(value_source);
              if (source_id >= 0 &&
                  printed_sources.find(source_id) == printed_sources.end()) {
                PrintVirtualEdge(source_id, target_id, "value", first);
                printed_sources.insert(source_id);
              }
            }
          }
        }
      }
    }

    if (node->properties().has_eager_deopt_info()) {
      int frame_id = deopt_frame_ids_[&node->eager_deopt_info()->top_frame()];
      PrintVirtualEdge(frame_id, target_id, "frame-state", first);
    }
    if (node->properties().can_lazy_deopt()) {
      int frame_id = deopt_frame_ids_[&node->lazy_deopt_info()->top_frame()];
      PrintVirtualEdge(frame_id, target_id, "frame-state", first);
    }

    if (node->properties().can_throw()) {
      const ExceptionHandlerInfo* info = node->exception_handler_info();
      if (info->HasExceptionHandler() && !info->ShouldLazyDeopt()) {
        BasicBlock* catch_block = info->catch_block();
        if (catch_block != nullptr) {
          int catch_target_id = labeller_->NodeId(catch_block->control_node());
          PrintVirtualEdge(target_id, catch_target_id, "control", first);
        }
      }
    }
  }

  void PrintBlocks() {
    bool first = true;
    for (BasicBlock* block : *graph_) {
      if (block->is_dead()) continue;
      if (!first) os_ << ",\n";
      first = false;
      os_ << "{\"id\":" << block->id() << ",";

      const char* type = "BLOCK";
      if (block->is_loop()) {
        type = "LOOP";
      } else if (block->is_merge_block()) {
        type = "MERGE";
      }
      os_ << "\"type\":\"" << type << "\",";
      os_ << "\"exception\":"
          << (block->is_exception_handler_block() ? "true" : "false") << ",";
      os_ << "\"deferred\":" << (block->is_deferred() ? "true" : "false")
          << ",";
      os_ << "\"predecessors\":[";
      bool first_pred = true;
      block->ForEachPredecessor([&](BasicBlock* pred) {
        if (!first_pred) os_ << ", ";
        first_pred = false;
        os_ << pred->id();
      });
      if (block->is_exception_handler_block()) {
        auto it = exception_sources_.find(block);
        if (it != exception_sources_.end()) {
          std::unordered_set<int> pred_ids;
          for (const NodeBase* source : it->second) {
            auto block_it = node_to_block_.find(source);
            if (block_it != node_to_block_.end()) {
              pred_ids.insert(block_it->second->id());
            }
          }
          for (int pred_id : pred_ids) {
            if (!first_pred) os_ << ", ";
            first_pred = false;
            os_ << pred_id;
          }
        }
      }
      os_ << "]}";
    }
  }

  void InitializeVirtualNodeIds() {
    next_virtual_id_ = labeller_->max_node_id() + 1;
    GraphProcessor<VirtualIdAssigner> assigner(this);
    assigner.ProcessGraph(graph_);
  }

  void RegisterExceptionInfo(const NodeBase* node, const BasicBlock* block) {
    if (node->properties().can_throw()) {
      const ExceptionHandlerInfo* info = node->exception_handler_info();
      if (info->HasExceptionHandler() && !info->ShouldLazyDeopt()) {
        BasicBlock* catch_block = info->catch_block();
        if (catch_block) {
          exception_sources_[catch_block].push_back(node);
        }
      }
    }
  }

  void RegisterDeoptInfo(const NodeBase* node) {
    if (node->properties().has_eager_deopt_info()) {
      RegisterDeoptFrame(&node->eager_deopt_info()->top_frame());
    }
    if (node->properties().can_lazy_deopt()) {
      RegisterDeoptFrame(&node->lazy_deopt_info()->top_frame());
    }
  }

  void RegisterDeoptFrame(const DeoptFrame* frame) {
    if (frame == nullptr) return;
    if (deopt_frame_ids_.find(frame) != deopt_frame_ids_.end()) return;
    deopt_frame_ids_[frame] = next_virtual_id_++;

    VirtualObjectList vos = frame->GetVirtualObjects();
    for (VirtualObject* vo : vos) {
      RegisterVirtualObject(vo);
    }

    if (frame->parent()) {
      RegisterDeoptFrame(frame->parent());
    }
  }

  void RegisterVirtualObject(const VirtualObject* vo) {
    if (vo == nullptr) return;
    if (virtual_object_ids_.find(vo) != virtual_object_ids_.end()) return;
    virtual_object_ids_[vo] = next_virtual_id_++;

    for (int i = 0; i < vo->slot_count(); i++) {
      ValueNode* val = vo->get_by_index(i);
      if (val && val->Is<VirtualObject>()) {
        RegisterVirtualObject(val->Cast<VirtualObject>());
      }
    }
  }

  int GetValueNodeId(const ValueNode* val, const VirtualObjectList& vos = {}) {
    val = val->UnwrapIdentities();
    if (val->Is<VirtualObject>()) {
      auto it = virtual_object_ids_.find(val->Cast<VirtualObject>());
      DCHECK(it != virtual_object_ids_.end());
      return it->second;
    }
    if (val->opcode() == Opcode::kInlinedAllocation && !vos.is_empty()) {
      const InlinedAllocation* alloc = val->Cast<InlinedAllocation>();
      if (alloc->HasBeenAnalysed() && alloc->HasBeenElided()) {
        VirtualObject* vo = vos.FindAllocatedWith(alloc);
        if (vo) {
          auto it = virtual_object_ids_.find(vo);
          DCHECK(it != virtual_object_ids_.end());
          return it->second;
        }
      }
    }
    return labeller_->NodeId(val);
  }

  void PrintDeoptFrameNode(const DeoptFrame* frame, int id,
                           const BasicBlock* block, bool* first) {
    if (!*first) os_ << ",\n";
    *first = false;

    os_ << "{\"id\":" << id << ",";
    os_ << "\"title\":\"*" << FrameTypeToString(frame->type()) << "\",";
    os_ << "\"block_id\":" << block->id() << ",";
    os_ << "\"op_effects\":\"\",";

    os_ << "\"properties\":\"";
    os_ << "Type: " << FrameTypeToString(frame->type()) << "\\n";
    if (frame->type() == DeoptFrame::FrameType::kInterpretedFrame) {
      os_ << "Bytecode offset: " << frame->as_interpreted().bytecode_position()
          << "\\n";
    }
    os_ << "Func: "
        << frame->GetSharedFunctionInfo().object()->DebugNameCStr().get()
        << "\\n";
    os_ << "\"";

    SourcePosition pos = frame->GetSourcePosition();
    if (pos.IsKnown()) {
      os_ << ", \"sourcePosition\":{\"scriptOffset\":" << pos.ScriptOffset()
          << ",\"inliningId\":" << pos.InliningId() << "}";
    }
    os_ << "}";
  }

  const char* FrameTypeToString(DeoptFrame::FrameType type) {
    switch (type) {
      case DeoptFrame::FrameType::kInterpretedFrame:
        return "FrameState";
      case DeoptFrame::FrameType::kInlinedArgumentsFrame:
        return "InlinedArgumentsFrame";
      case DeoptFrame::FrameType::kConstructInvokeStubFrame:
        return "ConstructInvokeStubFrame";
      case DeoptFrame::FrameType::kBuiltinContinuationFrame:
        return "BuiltinContinuationFrame";
    }
    return "UnknownFrame";
  }

  void PrintVirtualObjectNode(const VirtualObject* vo, int id,
                              const BasicBlock* block, bool* first) {
    if (!*first) os_ << ",\n";
    *first = false;

    os_ << "{\"id\":" << id << ",";
    os_ << "\"title\":\"*VirtualObject[#" << vo->id() << "]\",";
    os_ << "\"block_id\":" << block->id() << ",";
    os_ << "\"op_effects\":\"\",";

    os_ << "\"properties\":\"";
    os_ << "Slots: " << vo->slot_count() << "\\n";
    for (int i = 0; i < vo->slot_count(); ++i) {
      ValueNode* val = vo->get_by_index(i);
      os_ << "  slot " << i << ": ";
      if (val) {
        if (val->Is<VirtualObject>()) {
          os_ << "VirtualObject[#" << val->Cast<VirtualObject>()->id() << "]";
        } else {
          os_ << OpcodeToString(val->opcode()) << "n" << labeller_->NodeId(val);
        }
      } else {
        os_ << "empty";
      }
      os_ << "\\n";
    }
    os_ << "\"";
    os_ << "}";
  }

  void PrintVirtualEdge(int source_id, int target_id, const char* type,
                        bool* first) {
    if (!*first) os_ << ",\n";
    *first = false;
    os_ << "{\"source\":" << source_id << ",";
    os_ << "\"target\":" << target_id << ",";
    os_ << "\"type\":\"" << type << "\"}";
  }

  void PrintEdgesForDeoptFrame(const DeoptFrame* frame, int id, bool* first) {
    if (frame->parent()) {
      int parent_id = deopt_frame_ids_[frame->parent()];
      PrintVirtualEdge(parent_id, id, "frame-state", first);
    }

    VirtualObjectList vos = frame->GetVirtualObjects();

    if (frame->type() == DeoptFrame::FrameType::kInterpretedFrame) {
      const CompactInterpreterFrameState* state =
          frame->as_interpreted().frame_state();
      const MaglevCompilationUnit& unit = frame->as_interpreted().unit();

      state->ForEachParameter(
          unit, [&](ValueNode* val, interpreter::Register reg) {
            if (val) {
              int val_id = GetValueNodeId(val, vos);
              PrintVirtualEdge(val_id, id, "frame-state", first);
            }
          });
      state->ForEachLocal(unit, [&](ValueNode* val, interpreter::Register reg) {
        if (val) {
          int val_id = GetValueNodeId(val, vos);
          PrintVirtualEdge(val_id, id, "frame-state", first);
        }
      });
      if (state->liveness()->AccumulatorIsLive()) {
        ValueNode* val = state->accumulator(unit);
        if (val) {
          int val_id = GetValueNodeId(val, vos);
          PrintVirtualEdge(val_id, id, "frame-state", first);
        }
      }
      ValueNode* closure = frame->as_interpreted().closure();
      if (closure) {
        int closure_id = GetValueNodeId(closure, vos);
        PrintVirtualEdge(closure_id, id, "frame-state", first);
      }
    } else if (frame->type() == DeoptFrame::FrameType::kInlinedArgumentsFrame) {
      ValueNode* closure = frame->as_inlined_arguments().closure();
      if (closure) {
        int closure_id = GetValueNodeId(closure, vos);
        PrintVirtualEdge(closure_id, id, "frame-state", first);
      }
      for (ValueNode* val : frame->as_inlined_arguments().arguments()) {
        if (val) {
          int val_id = GetValueNodeId(val, vos);
          PrintVirtualEdge(val_id, id, "frame-state", first);
        }
      }
    } else if (frame->type() ==
               DeoptFrame::FrameType::kConstructInvokeStubFrame) {
      ValueNode* receiver = frame->as_construct_stub().receiver();
      if (receiver) {
        int receiver_id = GetValueNodeId(receiver, vos);
        PrintVirtualEdge(receiver_id, id, "frame-state", first);
      }
      ValueNode* context = frame->as_construct_stub().context();
      if (context) {
        int context_id = GetValueNodeId(context, vos);
        PrintVirtualEdge(context_id, id, "frame-state", first);
      }
    } else if (frame->type() ==
               DeoptFrame::FrameType::kBuiltinContinuationFrame) {
      for (ValueNode* val : frame->as_builtin_continuation().parameters()) {
        if (val) {
          int val_id = GetValueNodeId(val, vos);
          PrintVirtualEdge(val_id, id, "frame-state", first);
        }
      }
      ValueNode* context = frame->as_builtin_continuation().context();
      if (context) {
        int context_id = GetValueNodeId(context, vos);
        PrintVirtualEdge(context_id, id, "frame-state", first);
      }
    }
  }

  void PrintEdgesForVirtualObject(const VirtualObject* vo, int id,
                                  bool* first) {
    for (int i = 0; i < vo->slot_count(); ++i) {
      ValueNode* val = vo->get_by_index(i);
      if (val) {
        int val_id = GetValueNodeId(val);
        PrintVirtualEdge(val_id, id, "frame-state", first);
      }
    }
  }

  class VirtualIdAssigner {
   public:
    explicit VirtualIdAssigner(JSONMaglevGraphWriter* writer)
        : writer_(writer) {}
    void PreProcessGraph(Graph* graph) {}
    BlockProcessResult PreProcessBasicBlock(BasicBlock* block) {
      return BlockProcessResult::kContinue;
    }
    void PostPhiProcessing() {}
    BlockProcessResult PostProcessBasicBlock(BasicBlock* block) {
      return BlockProcessResult::kContinue;
    }
    void PostProcessGraph(Graph* graph) {}
    ProcessResult Process(NodeBase* node, const ProcessingState& state) {
      writer_->node_to_block_[node] = state.block();
      writer_->RegisterDeoptInfo(node);
      writer_->RegisterExceptionInfo(node, state.block());
      return ProcessResult::kContinue;
    }

   private:
    JSONMaglevGraphWriter* writer_;
  };

  class NodePrinter {
   public:
    explicit NodePrinter(JSONMaglevGraphWriter* writer) : writer_(writer) {}
    void PreProcessGraph(Graph* graph) {}
    BlockProcessResult PreProcessBasicBlock(BasicBlock* block) {
      return BlockProcessResult::kContinue;
    }
    void PostPhiProcessing() {}
    BlockProcessResult PostProcessBasicBlock(BasicBlock* block) {
      return BlockProcessResult::kContinue;
    }
    void PostProcessGraph(Graph* graph) {}
    ProcessResult Process(NodeBase* node, const ProcessingState& state) {
      writer_->PrintNodeAndDependencies(node, state.block(), &first_);
      return ProcessResult::kContinue;
    }

   private:
    JSONMaglevGraphWriter* writer_;
    bool first_ = true;
  };

  std::unordered_map<const DeoptFrame*, int> deopt_frame_ids_;
  std::unordered_map<const VirtualObject*, int> virtual_object_ids_;
  std::unordered_set<const DeoptFrame*> printed_deopt_frames_;
  std::unordered_set<const VirtualObject*> printed_virtual_objects_;
  std::unordered_map<const BasicBlock*, std::vector<const NodeBase*>>
      exception_sources_;
  std::unordered_map<const NodeBase*, const BasicBlock*> node_to_block_;
  int next_virtual_id_ = 0;

  std::ostream& os_;
  Graph* graph_;
  MaglevGraphLabeller* labeller_;
};

class SourceIdAssigner {
 public:
  explicit SourceIdAssigner(size_t reserve) { printed_.reserve(reserve); }
  int GetIdFor(Handle<SharedFunctionInfo> shared) {
    for (unsigned i = 0; i < printed_.size(); i++) {
      if (printed_.at(i).is_identical_to(shared)) {
        return i;
      }
    }
    const int source_id = static_cast<int>(printed_.size());
    printed_.push_back(shared);
    return source_id;
  }

 private:
  std::vector<Handle<SharedFunctionInfo>> printed_;
};

void JsonPrintAllSourceWithPositions(std::ostream& os,
                                     MaglevCompilationInfo* info, Graph* graph,
                                     Isolate* isolate) {
  os << "\"sources\" : {";
  Handle<SharedFunctionInfo> shared_info =
      info->toplevel_compilation_unit()->shared_function_info().object();
  DirectHandle<Script> script =
      (shared_info.is_null() || shared_info->script() == Tagged<Object>())
          ? DirectHandle<Script>()
          : direct_handle(Cast<Script>(shared_info->script()), isolate);

  JsonPrintFunctionSource(os, -1, info->function_name(), script, isolate,
                          shared_info, true);

  const auto& inlined = graph->inlined_functions();
  SourceIdAssigner id_assigner(graph->inlined_functions().size());
  for (unsigned id = 0; id < inlined.size(); id++) {
    os << ", ";
    Handle<SharedFunctionInfo> shared = inlined[id].shared_info;
    const int source_id = id_assigner.GetIdFor(shared);
    JsonPrintFunctionSource(
        os, source_id, shared->DebugNameCStr().get(),
        direct_handle(Cast<Script>(shared->script()), isolate), isolate, shared,
        true);
  }
  os << "}";
}

void JsonPrintBytecodeSource(std::ostream& os, int source_id,
                             const std::string& function_name,
                             DirectHandle<BytecodeArray> bytecode_array,
                             Tagged<FeedbackVector> feedback_vector = {}) {
  os << "\"" << source_id << "\" : {";
  os << "\"sourceId\": " << source_id;
  os << ", \"functionName\": \"" << function_name << "\"";
  os << ", \"bytecodeSource\": ";
  bytecode_array->PrintJson(os);
  os << ", \"feedbackVector\": \"";
  if (!feedback_vector.is_null()) {
    std::stringstream stream;
    Print(feedback_vector, stream);
    std::string s = stream.str();
    for (char c : s) {
      if (c == '\n') {
        os << "\\n";
      } else if (c == '"') {
        os << "\\\"";
      } else if (c == '\\') {
        os << "\\\\";
      } else {
        os << c;
      }
    }
  }
  os << "\"}";
}

void JsonPrintAllBytecodeSources(std::ostream& os, MaglevCompilationInfo* info,
                                 Graph* graph, Isolate* isolate) {
  os << "\"bytecodeSources\" : {";

  DirectHandle<BytecodeArray> bytecode_array =
      info->toplevel_compilation_unit()->bytecode().object();

  Tagged<FeedbackVector> feedback_vector =
      info->toplevel_function()->feedback_vector();

  JsonPrintBytecodeSource(os, -1, info->function_name(), bytecode_array,
                          feedback_vector);

  const auto& inlined = graph->inlined_functions();
  SourceIdAssigner id_assigner(graph->inlined_functions().size());

  for (unsigned id = 0; id < inlined.size(); id++) {
    Handle<SharedFunctionInfo> shared = inlined[id].shared_info;
    os << ", ";
    const int source_id = id_assigner.GetIdFor(shared);
    JsonPrintBytecodeSource(os, source_id, shared->DebugNameCStr().get(),
                            inlined[id].bytecode_array);
  }

  os << "}";
}

}  // namespace

void JsonPrintFunctionSource(std::ostream& os, int source_id,
                             const std::string& function_name,
                             DirectHandle<Script> script, Isolate* isolate,
                             DirectHandle<SharedFunctionInfo> shared,
                             bool with_key) {
  if (with_key) os << "\"" << source_id << "\" : ";

  os << "{ ";
  os << "\"sourceId\": " << source_id;
  os << ", \"functionName\": \"" << function_name << "\" ";

  int start = 0;
  int end = 0;
  if (!script.is_null() && !IsUndefined(*script) && !shared.is_null()) {
    Tagged<Object> source_name = script->name();
    os << ", \"sourceName\": \"";
    if (IsString(source_name)) {
      std::ostringstream escaped_name;
      escaped_name << Cast<String>(source_name)->ToCString().get();
      os << base::JSONEscaped(escaped_name);
    }
    os << "\"";
    {
      start = shared->StartPosition();
      end = shared->EndPosition();
      os << ", \"sourceText\": \"";
      if (!IsUndefined(script->source())) {
        DisallowGarbageCollection no_gc;
        int len = shared->EndPosition() - start;
        SubStringRange source(Cast<String>(script->source()), no_gc, start,
                              len);
        for (auto c : source) {
          os << AsEscapedUC16ForJSON(c);
        }
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

MaglevJsonFile::MaglevJsonFile(MaglevCompilationInfo* info,
                               std::ios_base::openmode mode)
    : std::ofstream(GetMaglevVisualizerLogFileName(
                        info, v8_flags.trace_turbo_path, nullptr, "json")
                        .get(),
                    mode) {}

MaglevJsonFile::~MaglevJsonFile() { flush(); }

std::ostream& operator<<(std::ostream& os, const MaglevGraphAsJSON& ad) {
  JSONMaglevGraphWriter writer(os, ad.graph);
  writer.Print();
  return os;
}

void PrintMaglevGraphAsJSON(MaglevCompilationInfo* info, Graph* graph,
                            MaglevPhase phase) {
  if (info->trace_json_enabled()) {
    MaglevJsonFile json_of(info, std::ios_base::app);
    json_of << "{\"name\":\"" << PhaseName(phase)
            << "\",\"type\":\"maglev_graph\",\"data\":" << AsJSON(graph)
            << "},\n";
  }
}

// Visualizer hook to finalize the JSON file.
// We write disassembly and sources here.
void FinalizeMaglevLogging(Isolate* isolate, MaglevCompilationInfo* info,
                           Graph* graph, Handle<Code> code) {
  if (!info->trace_json_enabled()) return;

  MaglevJsonFile json_of(info, std::ios_base::app);

  // Write disassembly
  json_of << "{\"name\":\"disassembly\",\"type\":\"disassembly\""
          << ",\"data\":\"";
#ifdef ENABLE_DISASSEMBLER
  std::stringstream disassembly_stream;
  std::unique_ptr<char[]> debug_name =
      info->toplevel_function()->shared()->DebugNameCStr();
  code->Disassemble(debug_name.get(), disassembly_stream, isolate);
  std::string disassembly_string(disassembly_stream.str());
  for (const auto& c : disassembly_string) {
    json_of << AsEscapedUC16ForJSON(c);
  }
#endif
  json_of << "\"}\n],\n";  // Close phases array

  // Write sources
  JsonPrintAllSourceWithPositions(json_of, info, graph, isolate);
  json_of << ",\n";
  JsonPrintAllBytecodeSources(json_of, info, graph, isolate);
  json_of << "\n}";
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8
