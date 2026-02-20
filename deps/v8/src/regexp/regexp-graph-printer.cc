// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef V8_ENABLE_REGEXP_DIAGNOSTICS

#include "src/regexp/regexp-graph-printer.h"

#include <iomanip>

#include "src/regexp/regexp-ast-printer.h"
#include "src/regexp/regexp-compiler.h"
#include "src/regexp/regexp-node-printer.h"
#include "src/utils/ostreams.h"

namespace v8 {
namespace internal {

class RegExpGraphPrinter::Scheduler final : public NodeVisitor {
 public:
  auto begin() { return schedule_.begin(); }
  auto end() { return schedule_.end(); }
  size_t size() { return schedule_.size(); }
  auto operator[](size_t index) { return schedule_[index]; }
  RegExpNode* at(size_t index) { return schedule_[index]; }
  const std::vector<RegExpNode*>& successors(RegExpNode* node) {
    return successors_[node];
  }
  // Returns true iff |n1| is scheduled before |n2|.
  bool ScheduledBefore(const RegExpNode* n1, const RegExpNode* n2) {
    auto it1 = std::find(begin(), end(), n1);
    if (it1 == end()) return false;
    auto it2 = std::find(begin(), end(), n2);
    if (it2 == end()) return false;
    return std::distance(it1, it2) > 0;
  }

#define DECLARE_VISIT(Type) void Visit##Type(Type##Node* that) override;
  FOR_EACH_NODE_TYPE(DECLARE_VISIT)
#undef DECLARE_VISIT
  void Visit(RegExpNode* node);
  void CreateSchedule(RegExpNode* root) {
    // TODO(pthier): Improve schedule to reduce number of edges in printer (I.e.
    // prefer to schedule success nodes directly after nodes).
    Visit(root);
    std::set<RegExpNode*> visited;
    std::queue<RegExpNode*> queue;
    for (auto n : nodes_) {
      auto it = depends_on_.find(n);
      if (it == depends_on_.end() || it->second.size() == 0) {
        visited.insert(n);
        queue.push(n);
      }
    }
    while (!queue.empty()) {
      RegExpNode* node = queue.front();
      queue.pop();
      schedule_.push_back(node);
      for (auto successor : successors_[node]) {
        depends_on_[successor].erase(node);
        if (depends_on_[successor].size() == 0) {
          if (!visited.contains(successor)) {
            visited.insert(successor);
            queue.push(successor);
          }
        }
      }
    }
    DCHECK_EQ(nodes_.size(), visited.size());
  }

 private:
  void VisitSeqRegExpNode(SeqRegExpNode* node);
  void VisitOther(RegExpNode* node);
  void AddEdge(RegExpNode* from, RegExpNode* to) {
    if (IsReachable(to, from)) return;
    successors_[from].push_back(to);
    depends_on_[to].insert(from);
  }

  bool IsReachable(RegExpNode* start, RegExpNode* end) {
    if (start == end) return true;

    std::queue<RegExpNode*> queue;
    std::unordered_set<RegExpNode*> visited;

    queue.push(start);
    visited.insert(start);

    while (!queue.empty()) {
      RegExpNode* current = queue.front();
      queue.pop();

      if (current == end) return true;

      auto it = successors_.find(current);
      if (it != successors_.end()) {
        for (RegExpNode* successor : it->second) {
          if (visited.find(successor) == visited.end()) {
            visited.insert(successor);
            queue.push(successor);
          }
        }
      }
    }
    return false;
  }
  std::unordered_set<RegExpNode*> visited_;
  std::vector<RegExpNode*> nodes_;
  std::unordered_map<RegExpNode*, std::set<RegExpNode*>> depends_on_;
  // This is not strictly necessary. We could also use the NodeVisitor again to
  // iterate successors, but it's easier if we just have them independent of the
  // node type in one place.
  std::unordered_map<RegExpNode*, std::vector<RegExpNode*>> successors_;
  std::vector<RegExpNode*> schedule_;
};

void RegExpGraphPrinter::Scheduler::Visit(RegExpNode* node) {
  if (visited_.contains(node)) return;
  visited_.insert(node);
  nodes_.push_back(node);
  node->Accept(this);
}

void RegExpGraphPrinter::Scheduler::VisitSeqRegExpNode(SeqRegExpNode* node) {
  AddEdge(node, node->on_success());
  Visit(node->on_success());
}

void RegExpGraphPrinter::Scheduler::VisitOther(RegExpNode* node) {
  // Nothing to do.
}

void RegExpGraphPrinter::Scheduler::VisitEnd(EndNode* node) {
  VisitOther(node);
}

void RegExpGraphPrinter::Scheduler::VisitAction(ActionNode* node) {
  VisitSeqRegExpNode(node->AsSeqRegExpNode());
}

void RegExpGraphPrinter::Scheduler::VisitChoice(ChoiceNode* node) {
  ZoneList<GuardedAlternative>* alternatives = node->alternatives();
  for (int i = 0; i < alternatives->length(); i++) {
    const GuardedAlternative& guard = alternatives->at(i);
    AddEdge(node, guard.node());
    Visit(guard.node());
  }
}

void RegExpGraphPrinter::Scheduler::VisitLoopChoice(LoopChoiceNode* node) {
  AddEdge(node, node->loop_node());
  Visit(node->loop_node());
  AddEdge(node, node->continue_node());
  Visit(node->continue_node());
}

void RegExpGraphPrinter::Scheduler::VisitNegativeLookaroundChoice(
    NegativeLookaroundChoiceNode* node) {
  VisitChoice(node);
}

void RegExpGraphPrinter::Scheduler::VisitBackReference(
    BackReferenceNode* node) {
  VisitSeqRegExpNode(node->AsSeqRegExpNode());
}

void RegExpGraphPrinter::Scheduler::VisitAssertion(AssertionNode* node) {
  VisitSeqRegExpNode(node->AsSeqRegExpNode());
}

void RegExpGraphPrinter::Scheduler::VisitText(TextNode* node) {
  VisitSeqRegExpNode(node->AsSeqRegExpNode());
}

namespace {

// TODO(pthier): Some of the helpers are equal or at least very similar to
// helpers in the Maglev graph printer. Factor out a common header.

// Add a target to the target list in the first non-null position from the end.
// This might have to extend the target list if there is no free spot.
size_t AddTarget(std::vector<RegExpNode*>& targets, RegExpNode* target) {
  if (targets.size() == 0 || (targets.back() != nullptr)) {
    targets.push_back(target);
    return targets.size() - 1;
  }

  size_t i = targets.size();
  while (i > 0) {
    if (targets[i - 1] != nullptr) break;
    i--;
  }
  targets[i] = target;
  return i;
}

// If the target is not a fallthrough, add i to the target list in the first
// non-null position from the end. This might have to extend the target list if
// there is no free spot. Returns true if it was added, false if it was a
// fallthrough.
bool AddTargetIfNotNext(std::vector<RegExpNode*>& targets, RegExpNode* target,
                        RegExpNode* next,
                        std::set<size_t>* arrows_starting_here = nullptr) {
  if (next == target) return false;
  size_t index = AddTarget(targets, target);
  if (arrows_starting_here != nullptr) arrows_starting_here->insert(index);
  return true;
}

enum ConnectionLocation {
  kTop = 1 << 0,
  kLeft = 1 << 1,
  kRight = 1 << 2,
  kBottom = 1 << 3,
  kHorizontal = kLeft | kRight,
  kVertical = kTop | kBottom
};

struct Connection {
  void Connect(ConnectionLocation loc) { connected |= loc; }
  bool IsConnected(ConnectionLocation loc) { return (connected & loc) == loc; }

  void AddHorizontal() { Connect(kHorizontal); }

  void AddVertical() { Connect(kVertical); }

  const char* ToString() const {
    switch (connected) {
      case 0:
        return " ";
      case kTop:
        return "╵";
      case kLeft:
        return "╴";
      case kRight:
        return "╶";
      case kBottom:
        return "╷";
      case kTop | kLeft:
        return "╯";
      case kTop | kRight:
        return "╰";
      case kBottom | kLeft:
        return "╮";
      case kBottom | kRight:
        return "╭";
      case kTop | kBottom:
        return "│";
      case kLeft | kRight:
        return "─";
      case kTop | kBottom | kLeft:
        return "┤";
      case kTop | kBottom | kRight:
        return "├";
      case kLeft | kRight | kTop:
        return "┴";
      case kLeft | kRight | kBottom:
        return "┬";
      case kTop | kLeft | kRight | kBottom:
        return "┼";
    }
    UNREACHABLE();
  }

  uint8_t connected = 0;
};

std::ostream& operator<<(std::ostream& os, const Connection& c) {
  return os << c.ToString();
}

int IntWidth(int val) {
  if (val == -1) return 2;
  return std::ceil(std::log10(val + 1));
}

int MaxIdWidth(int max_node_id, int padding_adjustment = 0) {
  int max_width = IntWidth(max_node_id) + 2;
  return max_width + padding_adjustment;
}

void PrintPaddedId(std::ostream& os, RegExpGraphLabeller<RegExpNode>* labeller,
                   int max_node_id, RegExpNode* node, std::string padding = " ",
                   int padding_adjustment = 0) {
  int id_width = 0;
  if (node) {
    int id = labeller->NodeId(node);
    id_width = IntWidth(id);
  }
  int max_width = MaxIdWidth(max_node_id, padding_adjustment);
  int padding_width = std::max(0, max_width - id_width);
  for (int i = 0; i < padding_width; ++i) {
    os << padding;
  }
}

void PrintPadding(std::ostream& os, int max_node_id, std::string padding = " ",
                  int padding_adjustment = 0) {
  int id_width = 0;
  int max_width = MaxIdWidth(max_node_id, padding_adjustment);
  int padding_width = std::max(0, max_width - id_width);
  for (int i = 0; i < padding_width; ++i) {
    os << padding;
  }
}

}  // namespace

RegExpGraphPrinter::RegExpGraphPrinter(
    std::unique_ptr<RegExpNodePrinter<RegExpNode>> printer)
    : printer_(std::move(printer)) {}

RegExpGraphPrinter::~RegExpGraphPrinter() = default;

size_t RegExpGraphPrinter::AddEdge(RegExpNode* from, RegExpNode* to) {
  if (edges_.size() == 0 || (edges_.back().IsValid())) {
    edges_.push_back({from, to});
    return edges_.size() - 1;
  }

  size_t i = edges_.size();
  while (i > 0) {
    if (edges_[i - 1].IsValid()) break;
    i--;
  }
  edges_[i] = {from, to};
  return i;
}

bool RegExpGraphPrinter::AddEdgeIfNotNext(
    RegExpNode* from, RegExpNode* to, RegExpNode* next,
    std::set<size_t>* arrows_starting_here) {
  if (next == to) return false;
  size_t index = AddEdge(from, to);
  if (arrows_starting_here != nullptr) arrows_starting_here->insert(index);
  return true;
}

void RegExpGraphPrinter::PreSizeTargets() {
  std::fill(targets_.begin(), targets_.end(), nullptr);
  for (size_t i = 0; i < schedule_->size(); i++) {
    RegExpNode* node = schedule_->at(i);
    RegExpNode* next =
        i + 1 < schedule_->size() ? schedule_->at(i + 1) : nullptr;
    max_node_id_ = std::max(max_node_id_, labeller()->NodeId(node));
    std::replace(targets_.begin(), targets_.end(), node,
                 static_cast<RegExpNode*>(nullptr));
    std::for_each(edges_.begin(), edges_.end(), [&](Edge& e) {
      if (EdgeEndsAt(e, node)) {
        e.Invalidate();
      }
    });
    for (auto successor : schedule_->successors(node)) {
      bool is_loop = false;
      if (LoopChoiceNode* loop = successor->AsLoopChoiceNode();
          loop != nullptr) {
        if (loop->loop_node() == node) {
          is_loop = true;
          std::replace(targets_.begin(), targets_.end(),
                       static_cast<RegExpNode*>(loop),
                       static_cast<RegExpNode*>(nullptr));
        }
      }
      if (!is_loop) {
        AddTargetIfNotNext(targets_, successor, next);
        AddEdgeIfNotNext(node, successor, next);
      }
    }
    if (LoopChoiceNode* loop_header = node->AsLoopChoiceNode();
        loop_header != nullptr) {
      AddTarget(targets_, loop_header);
      AddEdge(loop_header->loop_node(), node);
    }
  }
  DCHECK(std::all_of(edges_.begin(), edges_.end(),
                     [](Edge e) { return !e.IsValid(); }));
}

bool RegExpGraphPrinter::IsBackEdge(const Edge& edge) const {
  return schedule_->ScheduledBefore(edge.to(), edge.from());
}

bool RegExpGraphPrinter::EdgeStartsAt(const Edge& edge,
                                      const RegExpNode* node) const {
  if (!edge.IsValid() || node == nullptr) return false;
  const RegExpNode* start = IsBackEdge(edge) ? edge.to() : edge.from();
  return start == node;
}

bool RegExpGraphPrinter::EdgeEndsAt(const Edge& edge,
                                    const RegExpNode* node) const {
  if (!edge.IsValid() || node == nullptr) return false;
  const RegExpNode* end = IsBackEdge(edge) ? edge.from() : edge.to();
  return end == node;
}

bool RegExpGraphPrinter::IsTarget(const RegExpNode* node) const {
  return node != nullptr &&
         std::find_if(edges_.begin(), edges_.end(), [node](Edge e) {
           return e.to() == node;
         }) != edges_.end();
}
bool RegExpGraphPrinter::IsStart(const RegExpNode* node) const {
  return node != nullptr &&
         std::find_if(edges_.begin(), edges_.end(), [node](Edge e) {
           return e.from() == node;
         }) != edges_.end();
}

void RegExpGraphPrinter::PrintVerticalArrows(RegExpNode* current) {
  bool saw_start = false;
  bool saw_target = false;
  bool is_loop = false;
  int line_color = -1;
  int current_color = -1;
  for (size_t i = 0; i < edges_.size(); ++i) {
    Edge& e = edges_[i];
    int desired_color = line_color;
    Connection c;
    bool will_print_below = false;
    if (saw_start || saw_target) {
      c.AddHorizontal();
    }
    if (current != nullptr && e.from() == current) {
      if (IsTarget(current)) {
        will_print_below = true;
      } else {
        desired_color = (i % 6) + 1;
        line_color = desired_color;
        c.Connect(kRight);
        c.Connect(IsBackEdge(e) ? kTop : kBottom);
        saw_start = true;
        if (IsBackEdge(e)) {
          is_loop = true;
        }
      }
    }
    if (current != nullptr && e.to() == current) {
      desired_color = (i % 6) + 1;
      line_color = desired_color;
      c.Connect(kRight);
      c.Connect(IsBackEdge(e) ? kBottom : kTop);
      saw_target = true;
    }

    // Only add the vertical connection if there was no other connection.
    if (c.connected == 0 && edges_[i].IsValid() && !will_print_below) {
      desired_color = (i % 6) + 1;
      c.AddVertical();
    }
    if (v8_flags.log_colour && desired_color != current_color &&
        desired_color != -1) {
      os() << "\033[0;3" << desired_color << "m";
      current_color = desired_color;
    }
    os() << c;
  }
  if (saw_start && is_loop) {
    os() << "◄─";
    PrintPaddedId(os(), labeller(), max_node_id_, current,
                  !saw_target ? " " : "─", -2);
  } else {
    PrintPaddedId(os(), labeller(), max_node_id_, current,
                  !saw_target && !saw_start ? " " : "─");
  }
  os() << (saw_target ? "► " : (!IsTarget(current) ? "  " : "─ "));
}

void RegExpGraphPrinter::PrintVerticalArrowsBelow(RegExpNode* current,
                                                  bool has_fallthrough) {
  if (!IsTarget(current) || !IsStart(current)) return;
  bool saw_start = false;
  bool saw_target = false;
  int line_color = -1;
  int current_color = -1;
  for (size_t i = 0; i < edges_.size(); ++i) {
    Edge& e = edges_[i];
    int desired_color = line_color;
    Connection c;
    if (saw_start || saw_target) {
      c.AddHorizontal();
    }
    if (e.from() == current) {
      if (IsTarget(current)) {
        desired_color = (i % 6) + 1;
        line_color = desired_color;
        c.Connect(kRight);
        c.Connect(kBottom);
        saw_start = true;
      }
    }

    // Only add the vertical connection if there was no other connection.
    if (c.connected == 0 && e.IsValid() && !EdgeEndsAt(e, current)) {
      desired_color = (i % 6) + 1;
      c.AddVertical();
    }
    if (v8_flags.log_colour && desired_color != current_color &&
        desired_color != -1) {
      os() << "\033[0;3" << desired_color << "m";
      current_color = desired_color;
    }
    os() << c;
  }
  if (saw_start) {
    PrintPadding(os(), max_node_id_, "─", 2);
    Connection c;
    c.Connect(kTop);
    c.Connect(kLeft);
    if (has_fallthrough) {
      c.Connect(kBottom);
    }
    os() << c;
  } else {
    if (v8_flags.log_colour) {
      os() << "\033[0m";
    }
  }
  os() << std::endl;
}

void RegExpGraphPrinter::PrintGraph(RegExpNode* root) {
  schedule_ = std::make_unique<Scheduler>();
  schedule_->CreateSchedule(root);
  PreSizeTargets();

  // Print Graph.
  for (size_t i = 0; i < schedule_->size(); i++) {
    RegExpNode* node = schedule_->at(i);
    max_node_id_ = std::max(max_node_id_, labeller()->NodeId(node));
    RegExpNode* next =
        i + 1 < schedule_->size() ? schedule_->at(i + 1) : nullptr;
    bool has_fallthrough = false;
    for (auto successor : schedule_->successors(node)) {
      if (LoopChoiceNode* loop = successor->AsLoopChoiceNode();
          loop != nullptr) {
        if (loop->loop_node() == node) {
          // The loop header is already a target, so we can get the target id
          // and skip adding it as a target below.
          continue;
        }
      }
      has_fallthrough |= !AddEdgeIfNotNext(node, successor, next);
    }
    if (LoopChoiceNode* loop_header = node->AsLoopChoiceNode();
        loop_header != nullptr) {
      AddEdge(loop_header->loop_node(), node);
    }
    PrintVerticalArrows(node);
    PrintNode(node);
    PrintVerticalArrowsBelow(node, has_fallthrough);
    std::for_each(edges_.begin(), edges_.end(), [&](Edge& e) {
      if (EdgeEndsAt(e, node)) e.Invalidate();
    });
    if (has_fallthrough) {
      PrintVerticalArrows();
      os() << std::setfill(' ') << std::setw(MaxIdWidth(max_node_id_, -4))
           << "";
      if (v8_flags.log_colour) {
        os() << "\033[0m";
      }
      if (IsStart(node) && IsTarget(node)) {
        if (v8_flags.log_colour) {
          int color = 0;
          for (size_t e = edges_.size() - 1; e >= 0; --e) {
            if (edges_[e].from() == node) {
              color = (e % 6) + 1;
              break;
            }
          }
          os() << "\033[0;3" << color << "m";
        }
        os() << "▼";
        if (v8_flags.log_colour) {
          os() << "\033[0m";
        }
      } else {
        os() << "↓";
      }
      os() << std::endl;
    }
  }
}

void RegExpGraphPrinter::PrintNode(RegExpNode* node) {
  PrintNodeNoNewline(node);
  os() << std::endl;
}

void RegExpGraphPrinter::PrintNodeNoNewline(RegExpNode* node) {
  node->Accept(printer());
  set_color(RegExpPrinterBase::Color::kDefault);
}

void RegExpGraphPrinter::PrintTrace(Trace* trace) {
  if (trace->is_trivial()) return;

  os() << "Trace: ";
  bool first = true;
  for (const Trace* trace_part = trace; trace_part != nullptr;
       trace_part = trace_part->next()) {
    if (trace_part->is_trivial()) continue;
    if (!first) {
      os() << " -> ";
    }
    first = false;
    os() << "{";
    os() << "cp:" << trace_part->cp_offset();
    if (trace_part->at_start() != Trace::UNKNOWN) {
      os() << ", at_start:"
           << (trace_part->at_start() == Trace::TRUE_VALUE ? "T" : "F");
    }
    if (trace_part->characters_preloaded() > 0) {
      os() << ", preloaded:" << trace_part->characters_preloaded();
    }
    if (trace_part->backtrack() != nullptr) {
      os() << ", backtrack:label[" << std::hex << std::setw(8)
           << std::setfill('0')
           << static_cast<int>(
                  reinterpret_cast<intptr_t>(trace_part->backtrack()))
           << std::dec << "]";
    }
    if (trace_part->action() != nullptr) {
      os() << ", action:";
      RegExpNode* action_node = trace_part->action();
      PrintNodeLabel(action_node);
    }
    if (trace_part->special_loop_state() != nullptr) {
      os() << ", special_loop";
    }
    os() << "}";
  }
}

void RegExpGraphPrinter::PrintBoyerMoorePositionInfo(
    const BoyerMoorePositionInfo* pi) {
  os() << "[";
  bool first = true;
  for (int i = 0; i < BoyerMoorePositionInfo::kMapSize; ++i) {
    if (pi->at(i)) {
      if (!first) os() << ",";
      os() << AsUC16(i);
      first = false;
    }
  }
  os() << "]";
}

void RegExpGraphPrinter::PrintBoyerMooreLookahead(
    const BoyerMooreLookahead* bm) {
  os() << "BM Info:" << std::endl;
  for (int i = 0; i < bm->length(); ++i) {
    os() << "     " << i << ": ";
    PrintBoyerMoorePositionInfo(bm->at(i));
    os() << std::endl;
  }
}

}  // namespace internal
}  // namespace v8

#endif  // V8_ENABLE_REGEXP_DIAGNOSTICS
