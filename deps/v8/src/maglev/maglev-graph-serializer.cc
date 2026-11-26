// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/maglev/maglev-graph-serializer.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <iomanip>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "src/base/platform/platform.h"
#include "src/base/string-format.h"
#include "src/base/vector.h"
#include "src/flags/flags.h"
#include "src/heap/local-heap.h"
#include "src/heap/parked-scope.h"
#include "src/maglev/maglev-basic-block.h"
#include "src/maglev/maglev-compilation-info.h"
#include "src/maglev/maglev-deopt-frame-visitor.h"
#include "src/maglev/maglev-graph-labeller.h"
#include "src/maglev/maglev-graph-processor.h"
#include "src/maglev/maglev-graph.h"
#include "src/maglev/maglev-ir.h"
#include "src/maglev/maglev-node-type.h"
#include "src/maglev/maglev-range.h"
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
      return "\\u1D35\\u1D56";
    case ValueRepresentation::kRawPtr:
    case ValueRepresentation::kNone:
      return "";
  }
}

class MaybeUnparkForPrint {
 public:
  MaybeUnparkForPrint() {
    LocalHeap* local_heap = LocalHeap::Current();
    if (local_heap && local_heap->IsParked()) {
      scope_.emplace(local_heap);
    }
  }

 private:
  std::optional<UnparkedScope> scope_;
};

std::string PrintNodeParamsToString(const NodeBase* node) {
  std::ostringstream os;
  switch (node->opcode()) {
#define V(Name)                          \
  case Opcode::k##Name:                  \
    node->Cast<Name>()->PrintParams(os); \
    break;
    NODE_BASE_LIST(V)
#undef V
  }
  return os.str();
}

std::string CollapseAndTrimWhitespace(std::string_view input) {
  std::string out;
  out.reserve(input.size());
  bool prev_space = true;
  for (char c : input) {
    if (c == '\n' || c == '\r' || c == '\t' || c == ' ') {
      if (!prev_space) {
        out.push_back(' ');
        prev_space = true;
      }
    } else {
      out.push_back(c);
      prev_space = false;
    }
  }
  if (!out.empty() && out.back() == ' ') {
    out.pop_back();
  }
  return out;
}

// Removes leading "0x<hex> " addresses right before "<...>" (such as HeapObject
// ShortPrint prefixes like "0x1234abcd <NativeContext[305]>") so inline node
// titles stay compact while full addresses remain in Properties custom data.
std::string StripHeapAddressesBeforeAngles(std::string_view input) {
  std::string out;
  out.reserve(input.size());
  for (size_t i = 0; i < input.size();) {
    if (i + 2 < input.size() && input[i] == '0' && input[i + 1] == 'x' &&
        (i == 0 || (!std::isalnum(static_cast<unsigned char>(input[i - 1])) &&
                    input[i - 1] != '_'))) {
      size_t j = i + 2;
      while (j < input.size() &&
             std::isxdigit(static_cast<unsigned char>(input[j]))) {
        ++j;
      }
      if (j - (i + 2) >= 4 && j + 1 < input.size() && input[j] == ' ' &&
          input[j + 1] == '<') {
        i = j + 1;
        continue;
      }
    }
    out.push_back(input[i]);
    ++i;
  }
  return out;
}

std::string TruncateUtf8Inline(std::string_view s, size_t max_len) {
  if (s.size() <= max_len) return std::string(s);
  size_t cut = max_len;
  while (cut > 0 && (static_cast<unsigned char>(s[cut]) & 0xC0) == 0x80) {
    --cut;
  }
  std::string out(s.substr(0, cut));
  out += "...";
  return out;
}

bool HasSingleOuterBrackets(std::string_view s) {
  if (s.size() < 2) return false;
  const char open = s.front();
  const char close = s.back();
  if (!((open == '(' && close == ')') || (open == '[' && close == ']'))) {
    return false;
  }
  int depth = 0;
  for (size_t i = 0; i < s.size() - 1; ++i) {
    if (s[i] == open) {
      ++depth;
    } else if (s[i] == close) {
      --depth;
      if (depth == 0) return false;
    }
  }
  return depth == 1;
}

// Turbolizer inserts operation titles via d3 `.html(...)` inside SVG <tspan>,
// so unescaped `<` / `>` would be parsed as HTML tags by existing Turbolizer
// builds and swallow the remainder of the title (including closing `)`/`]`).
std::string ReplaceAngleBracketsForTitle(std::string_view input) {
  std::string out;
  out.reserve(input.size());
  for (char c : input) {
    if (c == '<') {
      out +=
          "\xE2\x80\xB9";  // U+2039 SINGLE LEFT-POINTING ANGLE QUOTATION MARK
    } else if (c == '>') {
      out +=
          "\xE2\x80\xBA";  // U+203A SINGLE RIGHT-POINTING ANGLE QUOTATION MARK
    } else {
      out.push_back(c);
    }
  }
  return out;
}

std::string FormatInlineParams(const NodeBase* node,
                               const std::string& raw_params) {
  std::string cleaned = CollapseAndTrimWhitespace(raw_params);
  if (cleaned.empty()) return "";
  cleaned = StripHeapAddressesBeforeAngles(cleaned);

  std::string_view inner = cleaned;
  if (HasSingleOuterBrackets(inner)) {
    inner = inner.substr(1, inner.size() - 2);
  }
  if (inner.empty()) return "";

  const bool use_parens = node->input_count() == 0 && !node->Is<Phi>();
  const size_t max_len = use_parens ? 64 : 48;
  std::string body =
      ReplaceAngleBracketsForTitle(TruncateUtf8Inline(inner, max_len));
  return use_parens ? ("(" + body + ")") : ("[" + body + "]");
}

std::string FormatOpEffects(OpProperties props) {
  std::ostringstream os;
  bool first = true;
  auto add = [&](const char* name) {
    if (!first) os << ", ";
    first = false;
    os << name;
  };
  if (props.can_read()) add("can_read");
  if (props.can_write()) add("can_write");
  if (props.can_allocate()) add("can_allocate");
  if (props.can_eager_deopt()) add("can_eager_deopt");
  if (props.can_lazy_deopt()) add("can_lazy_deopt");
  if (props.is_deopt_checkpoint()) add("deopt_checkpoint");
  if (props.can_throw()) add("can_throw");
  if (props.is_call()) add("call");
  if (props.is_deferred_call()) add("deferred_call");
  if (props.not_idempotent()) add("not_idempotent");
  if (props.is_conversion()) add("conversion");
  if (first && props.is_pure()) add("pure");
  return os.str();
}

bool IsStoreToNonEscapedObject(const NodeBase* node) {
  if (CanBeStoreToNonEscapedObject(node->opcode())) {
    DCHECK_GT(node->input_count(), 0);
    if (const InlinedAllocation* alloc =
            node->input(0).node()->TryCast<InlinedAllocation>()) {
      return alloc->HasBeenAnalysed() && alloc->HasBeenElided();
    }
  }
  return false;
}

void PrintControlTargets(std::ostream& os, const ControlNode* node) {
  if (const auto* uncond = node->TryCast<UnconditionalControlNode>()) {
    os << "target: b" << uncond->target()->id();
  } else if (const auto* branch = node->TryCast<BranchControlNode>()) {
    os << "true: b" << branch->if_true()->id() << ", false: b"
       << branch->if_false()->id();
  } else if (const auto* sw = node->TryCast<Switch>()) {
    os << "targets: [";
    for (int i = 0; i < sw->size(); ++i) {
      if (i > 0) os << ", ";
      os << "b" << sw->targets()[i].block_ptr()->id();
    }
    os << "]";
    if (sw->has_fallthrough()) {
      os << ", fallthrough: b" << sw->fallthrough()->id();
    }
  }
}

std::unique_ptr<char[]> GetMaglevVisualizerLogFileName(
    MaglevCompilationInfo* info, const char* optional_base_dir,
    const char* phase, const char* suffix) {
  std::array<char, 256> filename = {};
  std::string debug_name = info->function_name();
  const char* file_prefix = info->is_turbolev()
                                ? v8_flags.trace_turbo_file_prefix.value()
                                : v8_flags.trace_maglev_file_prefix.value();
  int optimization_id = info->optimization_id();

  if (debug_name.length() > 0) {
    base::SNPrintF(filename, "%s-%s-%i", file_prefix, debug_name.c_str(),
                   optimization_id);
  } else {
    base::SNPrintF(filename, "%s-none-%i", file_prefix, optimization_id);
  }

  std::replace(filename.begin(), filename.begin() + filename.size(), '/', '_');
  std::replace(filename.begin(), filename.begin() + filename.size(), ' ', '_');
  std::replace(filename.begin(), filename.begin() + filename.size(), ':', '-');
#if V8_OS_WIN
  std::replace(filename.begin(), filename.begin() + filename.size(), '<', '{');
  std::replace(filename.begin(), filename.begin() + filename.size(), '>', '}');
#endif

  std::array<char, 256> base_dir;
  if (optional_base_dir != nullptr) {
    base::SNPrintF(base_dir, "%s%c", optional_base_dir,
                   base::OS::DirectorySeparator());
  } else {
    base_dir[0] = '\0';
  }

  std::array<char, 256> full_filename;
  if (phase == nullptr) {
    base::SNPrintF(full_filename, "%s%s.%s", base_dir.data(), filename.data(),
                   suffix);
  } else {
    base::SNPrintF(full_filename, "%s%s-%s.%s", base_dir.data(),
                   filename.data(), phase, suffix);
  }

  char* buffer = new char[full_filename.size()];
  memcpy(buffer, full_filename.data(), full_filename.size());
  return std::unique_ptr<char[]>(buffer);
}

class JSONMaglevGraphWriter {
 public:
  JSONMaglevGraphWriter(std::ostream& os, Graph* graph)
      : os_(os), graph_(graph), labeller_(graph->graph_labeller()) {
    DCHECK_NOT_NULL(labeller_);
  }

  void Print() {
    MaybeUnparkForPrint unpark;
    std::optional<MaglevGraphLabellerScope> labeller_scope;
    if (GetCurrentGraphLabeller() == nullptr) {
      labeller_scope.emplace(labeller_);
    }
    InitializeVirtualNodeIds();
    os_ << "{\n\"nodes\":[";
    PrintNodes();
    os_ << "\n],\n\"edges\":[";
    PrintEdges();
    os_ << "\n],\n\"blocks\":[";
    PrintBlocks();
    os_ << "\n]}";
  }

  void PrintCustomData(MaglevPhase phase) {
    MaybeUnparkForPrint unpark;
    std::optional<MaglevGraphLabellerScope> labeller_scope;
    if (GetCurrentGraphLabeller() == nullptr) {
      labeller_scope.emplace(labeller_);
    }

    const bool has_regalloc_data = !graph_->compilation_info()->is_turbolev() &&
                                   phase >= MaglevPhase::kAnyUseMarking;

    using CustomEntry = std::pair<int, std::string>;
    std::vector<CustomEntry> properties_data;
    std::vector<CustomEntry> representations_data;
    std::vector<CustomEntry> use_count_data;
    std::vector<CustomEntry> regalloc_data;
    std::vector<CustomEntry> types_data;
    std::vector<CustomEntry> ranges_data;
    std::vector<CustomEntry> block_properties_data;

    auto record_node = [&](const NodeBase* node) {
      const int id = labeller_->NodeId(node);

      {
        std::ostringstream os;
        bool first = true;
        auto sep = [&]() {
          if (!first) os << ", ";
          first = false;
        };

        std::string raw_params =
            CollapseAndTrimWhitespace(PrintNodeParamsToString(node));
        if (!raw_params.empty()) {
          sep();
          os << raw_params;
        }
        if (const Phi* phi = node->TryCast<Phi>()) {
          if (phi->is_exception_phi()) {
            sep();
            os << "exception";
          }
          if (phi->is_loop_phi()) {
            sep();
            os << "loop";
          }
          UseRepresentationSet hints = const_cast<Phi*>(phi)->use_repr_hints();
          if (!hints.empty()) {
            sep();
            os << "hints: " << hints;
          }
          UseRepresentationSet same_loop_hints =
              const_cast<Phi*>(phi)->same_loop_use_repr_hints();
          if (!same_loop_hints.empty()) {
            sep();
            os << "same_loop_hints: " << same_loop_hints;
          }
        }
        if (const ControlNode* ctrl = node->TryCast<ControlNode>()) {
          std::ostringstream target_os;
          PrintControlTargets(target_os, ctrl);
          if (!target_os.str().empty()) {
            sep();
            os << target_os.str();
          }
        }
        if (const ValueNode* val = node->TryCast<ValueNode>()) {
          if (val->is_tagged()) {
            sep();
            os << (val->decompresses_tagged_result() ? "decompressed"
                                                     : "compressed");
          } else if (!val->Is<Float64Constant>() &&
                     !val->Is<ChangeInt32ToFloat64>()) {
            sep();
            os << (val->can_truncate_to_int32() ? "can truncate to int32"
                                                : "cannot truncate to int32");
          }
          if (const InlinedAllocation* alloc =
                  val->TryCast<InlinedAllocation>()) {
            if (alloc->HasBeenAnalysed()) {
              sep();
              os << (alloc->HasBeenElided() ? "elided" : "escaping");
            }
          }
        }
        if (has_regalloc_data && node->has_inputs()) {
          sep();
          os << "inputs: [";
          for (int i = 0; i < node->input_count(); ++i) {
            if (i > 0) os << ", ";
            os << node->input(i).operand();
          }
          os << "]";
        }
        if (IsStoreToNonEscapedObject(node)) {
          sep();
          os << "elided store";
        }
        std::string prop_str = os.str();
        if (!prop_str.empty()) {
          properties_data.emplace_back(id, std::move(prop_str));
        }
      }

      if (const ValueNode* val = node->TryCast<ValueNode>()) {
        {
          std::ostringstream os;
          os << val->value_representation();
          if (val->is_tagged()) {
            os << (val->decompresses_tagged_result() ? " (decompressed)"
                                                     : " (compressed)");
          }
          representations_data.emplace_back(id, os.str());
        }

        {
          std::ostringstream os;
          if (val->unused_inputs_were_visited()) {
            os << "dead";
          } else {
            os << val->use_count() << " uses";
            if (!val->is_used()) {
              if (val->opcode() != Opcode::kAllocationBlock &&
                  val->properties().is_required_when_unused()) {
                os << " (required)";
              } else {
                os << " (dead)";
              }
            }
            if (const InlinedAllocation* alloc =
                    val->TryCast<InlinedAllocation>()) {
              os << ", " << alloc->non_escaping_use_count()
                 << " non-escaping uses";
              if (alloc->HasBeenAnalysed() && alloc->HasBeenElided()) {
                os << " (elided)";
              }
            }
          }
          use_count_data.emplace_back(id, os.str());
        }

        if (has_regalloc_data) {
          std::ostringstream os;
          bool first = true;
          auto sep = [&]() {
            if (!first) os << ", ";
            first = false;
          };
          if (val->has_id()) {
            sep();
            os << "#" << val->regalloc_info()->id();
          }
          RegallocValueNodeInfo* node_info = val->regalloc_info();
          if (!node_info->result().operand().IsInvalid()) {
            sep();
            os << "-> " << node_info->result().operand();
            if (node_info->result().operand().IsAllocated() &&
                node_info->is_spilled() &&
                node_info->spill_slot() != node_info->result().operand()) {
              os << " (spilled: " << node_info->spill_slot() << ")";
            }
          }
          if (node_info->has_valid_live_range()) {
            sep();
            os << "live range: [" << node_info->live_range().start << "-"
               << node_info->live_range().end << "]";
          }
          std::string reg_str = os.str();
          if (!reg_str.empty()) {
            regalloc_data.emplace_back(id, std::move(reg_str));
          }
        }

        if (const Phi* phi = val->TryCast<Phi>()) {
          if (phi->type() != NodeType::kUnknown) {
            std::ostringstream os;
            os << phi->type();
            if (phi->is_loop_phi() &&
                phi->post_loop_type() != NodeType::kUnknown &&
                phi->post_loop_type() != phi->type()) {
              os << " (post_loop: " << phi->post_loop_type() << ")";
            }
            types_data.emplace_back(id, os.str());
          }
        } else {
          const ValueNode* unwrapped = val->UnwrapAliases();
          if (!unwrapped->Is<Phi>() &&
              unwrapped->value_representation() !=
                  ValueRepresentation::kRawPtr &&
              unwrapped->value_representation() != ValueRepresentation::kNone) {
            NodeType static_type =
                const_cast<ValueNode*>(unwrapped)->GetStaticType(
                    graph_->broker());
            if (static_type != NodeType::kUnknown) {
              std::ostringstream os;
              os << static_type;
              types_data.emplace_back(id, os.str());
            }
          }

          const ValueNode* range_base = val;
          while (range_base->is_conversion()) {
            range_base = range_base->input_node(0);
          }
          if (!range_base->Is<Phi>()) {
            Range r = val->GetStaticRange();
            if (!r.is_all()) {
              std::ostringstream os;
              os << r;
              ranges_data.emplace_back(id, os.str());
            }
          }
        }
      } else {
        if (IsStoreToNonEscapedObject(node)) {
          use_count_data.emplace_back(id, "elided");
        }
        if (has_regalloc_data && node->has_id()) {
          std::ostringstream os;
          os << "#" << node->regalloc_info()->id();
          regalloc_data.emplace_back(id, os.str());
        }
      }
    };

    for (const BasicBlock* block : *graph_) {
      if (block->is_dead()) continue;

      {
        std::ostringstream os;
        bool first = true;
        auto sep = [&]() {
          if (!first) os << ", ";
          first = false;
        };
        if (block->is_loop()) {
          sep();
          if (block->state()->is_loop_with_peeled_iteration()) {
            os << "peeled loop";
          } else if (block->has_state() &&
                     block->state()->is_resumable_loop()) {
            os << "resumable loop";
          } else {
            os << "loop header";
          }
        }
        if (block->is_edge_split_block()) {
          sep();
          os << "edge split";
        }
        if (block->is_start_block_of_switch_case()) {
          sep();
          os << "switch case";
        }
        if (block->is_exception_handler_block()) {
          sep();
          os << "exception handler";
        }
        if (block->is_deferred()) {
          sep();
          os << "deferred";
        }
        std::string blk_str = os.str();
        if (!blk_str.empty()) {
          block_properties_data.emplace_back(block->id(), std::move(blk_str));
        }
      }

      if (block->has_phi()) {
        for (const Phi* phi : *block->phis()) {
          record_node(phi);
        }
      }
      for (const Node* node : block->nodes()) {
        if (node == nullptr) continue;
        record_node(node);
      }
      if (block->control_node()) {
        record_node(block->control_node());
      }
    }

    auto emit_custom_data = [&](const char* name, const char* target,
                                const std::vector<CustomEntry>& entries) {
      if (entries.empty()) return;
      os_ << "{\"name\":\"" << name
          << "\", \"type\":\"turboshaft_custom_data\", "
             "\"data_target\":\""
          << target << "\", \"data\":[";
      bool first = true;
      for (const auto& [key, value] : entries) {
        os_ << (first ? "\n" : ",\n") << "{\"key\":" << key << ", \"value\":\""
            << base::JSONEscaped(value) << "\"}";
        first = false;
      }
      os_ << "]},\n";
    };

    emit_custom_data("Properties", "operations", properties_data);
    emit_custom_data("Representations", "operations", representations_data);
    emit_custom_data("Use Count", "operations", use_count_data);
    if (has_regalloc_data) {
      emit_custom_data("Regalloc", "operations", regalloc_data);
    }
    emit_custom_data("Types", "operations", types_data);
    emit_custom_data("Range", "operations", ranges_data);
    emit_custom_data("Properties", "blocks", block_properties_data);
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

    frame->ForEachValue([&](ValueNode* val) {
      if (!val) return;
      if (const VirtualObject* vo = val->TryGetVirtualObject()) {
        PrintVirtualObjectAndDependencies(vo, block, first);
      }
    });

    int id = deopt_frame_ids_[frame];
    PrintDeoptFrameNode(frame, id, block, first);
    printed_deopt_frames_.insert(frame);
  }

  void PrintVirtualObjectAndDependencies(const VirtualObject* vo,
                                         const BasicBlock* block, bool* first) {
    if (vo == nullptr) return;
    if (!printed_virtual_objects_.insert(vo).second) {
      return;
    }

    for (int i = 0; i < vo->slot_count(); ++i) {
      ValueNode* val = vo->get_by_index(i);
      if (val) {
        if (const VirtualObject* nested = val->TryGetVirtualObject()) {
          PrintVirtualObjectAndDependencies(nested, block, first);
        }
      }
    }

    int id = virtual_object_ids_[vo];
    PrintVirtualObjectNode(vo, id, block, first);
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
    std::string inline_params =
        FormatInlineParams(node, PrintNodeParamsToString(node));
    if (node->Is<Phi>()) {
      const Phi* phi = node->Cast<Phi>();
      os_ << "\"title\":\"Phi"
          << RepresentationSuperscript(phi->value_representation());
      if (phi->is_exception_phi()) {
        os_ << "\\u2091";
      }
      if (!inline_params.empty()) {
        os_ << base::JSONEscaped(inline_params);
      }
      os_ << "\",";
    } else {
      std::ostringstream title_stream;
      title_stream << OpcodeToString(node->opcode());
      if (!inline_params.empty()) {
        title_stream << inline_params;
      }
      os_ << "\"title\":\"" << base::JSONEscaped(title_stream) << "\",";
    }
    if (block) {
      os_ << "\"block_id\":" << block->id() << ",";
    } else {
      os_ << "\"block_id\":0,";
    }
    if (node->Is<ControlNode>() || node->properties().can_throw()) {
      os_ << "\"control\":true,";
    }
    os_ << "\"op_effects\":\""
        << base::JSONEscaped(FormatOpEffects(node->properties())) << "\"";

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

    frame->ForEachValue([&](ValueNode* val) {
      if (!val) return;
      if (const VirtualObject* vo = val->TryGetVirtualObject()) {
        RegisterVirtualObject(vo);
      }
    });

    if (frame->parent()) {
      RegisterDeoptFrame(frame->parent());
    }
  }

  void RegisterVirtualObject(const VirtualObject* vo) {
    if (vo == nullptr) return;
    if (!virtual_object_ids_.emplace(vo, next_virtual_id_).second) return;
    ++next_virtual_id_;

    for (int i = 0; i < vo->slot_count(); i++) {
      ValueNode* val = vo->get_by_index(i);
      if (val) {
        if (const VirtualObject* nested = val->TryGetVirtualObject()) {
          RegisterVirtualObject(nested);
        }
      }
    }
  }

  int GetValueNodeId(ValueNode* val) {
    if (const VirtualObject* vo = val->TryGetVirtualObject()) {
      auto it = virtual_object_ids_.find(vo);
      DCHECK(it != virtual_object_ids_.end());
      return it->second;
    }
    val = val->UnwrapAliases();
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
        << base::JSONEscaped(
               frame->GetSharedFunctionInfo().object()->DebugNameCStr().get())
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
    if (vo->has_static_map()) {
      std::ostringstream map_os;
      map_os << *vo->map()->object();
      os_ << "Map: "
          << base::JSONEscaped(CollapseAndTrimWhitespace(map_os.str()))
          << "\\n";
    }
    os_ << "Slots: " << vo->slot_count() << "\\n";
    for (int i = 0; i < vo->slot_count(); ++i) {
      ValueNode* val = vo->get_by_index(i);
      os_ << "  slot " << i << ": ";
      if (val) {
        if (const VirtualObject* nested_vo = val->TryGetVirtualObject()) {
          os_ << "VirtualObject[#" << nested_vo->id() << "]";
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

    frame->ForEachValue([&](ValueNode* val) {
      if (!val) return;
      int val_id = GetValueNodeId(val);
      PrintVirtualEdge(val_id, id, "frame-state", first);
    });
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
  os << ", \"functionName\": \"" << base::JSONEscaped(function_name) << "\"";
  os << ", \"bytecodeSource\": ";
  bytecode_array->PrintJson(os);
  os << ", \"feedbackVector\": \"";
  if (!feedback_vector.is_null()) {
    std::stringstream stream;
    Print(feedback_vector, stream);
    os << base::JSONEscaped(stream.str());
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
  os << ", \"functionName\": \"" << base::JSONEscaped(function_name) << "\" ";

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

std::ostream& operator<<(std::ostream& os, const MaglevCustomDataAsJSON& ad) {
  JSONMaglevGraphWriter writer(os, ad.graph);
  writer.PrintCustomData(ad.phase);
  return os;
}

void PrintMaglevGraphAsJSON(MaglevCompilationInfo* info, Graph* graph,
                            MaglevPhase phase) {
  if (info->trace_json_enabled()) {
    MaglevJsonFile json_of(info, std::ios_base::app);
    json_of << "{\"name\":\"" << PhaseName(phase)
            << "\",\"type\":\"maglev_graph\",\"data\":" << AsJSON(graph)
            << "},\n"
            << CustomDataAsJSON(graph, phase);
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
  json_of << base::JSONEscaped(disassembly_stream.str());
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
