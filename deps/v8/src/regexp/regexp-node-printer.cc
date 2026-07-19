// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef V8_ENABLE_REGEXP_DIAGNOSTICS

#include "src/regexp/regexp-node-printer.h"

#include "src/regexp/regexp-ast-printer.h"
#include "src/regexp/regexp-compiler.h"

namespace v8 {
namespace internal {

void RegExpNodePrinter<RegExpNode>::PrintNodeLabel(const RegExpNode* node,
                                                   const char* name) {
  set_color(RegExpPrinterBase::Color::kBlue);
  PrintNodeLabel(node);
  os() << ": " << name << " ";
}

void RegExpNodePrinter<RegExpNode>::PrintSuccess(const SeqRegExpNode* node) {
  set_color(RegExpPrinterBase::Color::kGreen);
  os() << " ⇝ ";
  PrintNodeLabel(node->on_success());
  set_color(RegExpPrinterBase::Color::kDefault);
}

void RegExpNodePrinter<RegExpNode>::VisitEnd(EndNode* node) {
  PrintNodeLabel(node, "End");
  switch (node->action()) {
    case EndNode::ACCEPT:
      os() << "Accept";
      break;
    case EndNode::BACKTRACK:
      os() << "Backtrack";
      break;
    case EndNode::NEGATIVE_SUBMATCH_SUCCESS: {
      NegativeSubmatchSuccess* nss_node = node->AsNegativeSubmatchSuccess();
      os() << "[stack: " << nss_node->stack_pointer_register_
           << "; pos: " << nss_node->current_position_register_;
      if (nss_node->clear_capture_count_ > 0) {
        const int clear_capture_end =
            nss_node->clear_capture_start_ + nss_node->clear_capture_count_ - 1;
        os() << "; clear captures " << nss_node->clear_capture_start_ << " -- "
             << clear_capture_end;
      }
      os() << "]";
    }
  }
}

void RegExpNodePrinter<RegExpNode>::VisitAction(ActionNode* node) {
  PrintNodeLabel(node, "Action");
  switch (node->action_type()) {
    case ActionNode::SET_REGISTER_FOR_LOOP:
      os() << "set register for loop " << node->register_from()
           << " := " << node->value();
      break;
    case ActionNode::INCREMENT_REGISTER:
      os() << "increment register " << node->register_from();
      break;
    case ActionNode::STORE_POSITION:
      os() << "store position " << node->register_from();
      break;
    case ActionNode::RESTORE_POSITION:
      os() << "restore position " << node->register_from();
      break;
    case ActionNode::BEGIN_POSITIVE_SUBMATCH:
      os() << "begin positive submatch [stack: "
           << node->data_.u_submatch.stack_pointer_register
           << "; pos: " << node->data_.u_submatch.current_position_register
           << "; ⇝ ";
      PrintNodeLabel(node->success_node());
      os() << "]";
      break;
    case ActionNode::BEGIN_NEGATIVE_SUBMATCH:
      os() << "begin negative submatch [stack: "
           << node->data_.u_submatch.stack_pointer_register
           << "; pos: " << node->data_.u_submatch.current_position_register
           << "]";
      break;
    case ActionNode::POSITIVE_SUBMATCH_SUCCESS:
      os() << "positive submatch success [stack: "
           << node->data_.u_submatch.stack_pointer_register
           << "; pos: " << node->data_.u_submatch.current_position_register;
      if (node->data_.u_submatch.clear_register_count != 0) {
        const int clear_to = node->data_.u_submatch.clear_register_from +
                             node->data_.u_submatch.clear_register_count - 1;
        os() << "; clear reg " << node->data_.u_submatch.clear_register_from
             << " -- " << clear_to;
      }
      os() << "]";
      break;
    case ActionNode::EMPTY_MATCH_CHECK:
      os() << "empty match check [start reg: "
           << node->data_.u_empty_match_check.start_register
           << "; repetition: reg "
           << node->data_.u_empty_match_check.start_register << " / limit "
           << node->data_.u_empty_match_check.repetition_limit << "]";
      break;
    case ActionNode::CLEAR_CAPTURES:
      os() << "clear captures " << node->register_from() << " -- "
           << node->register_to();
      break;
    case ActionNode::MODIFY_FLAGS:
      os() << "modify flags " << RegExpFlags(node->data_.u_modify_flags.flags);
      break;
    case ActionNode::EATS_AT_LEAST:
      os() << "eats at least " << node->data_.u_eats_at_least.characters;
      break;
  }
  PrintSuccess(node);
}

void RegExpNodePrinter<RegExpNode>::PrintGuard(const Guard* guard) {
  os() << guard->reg() << " ";
  switch (guard->op()) {
    case Guard::LT:
      os() << "<";
      break;
    case Guard::GEQ:
      os() << ">=";
      break;
  }
  os() << " " << guard->value();
}

void RegExpNodePrinter<RegExpNode>::VisitChoice(ChoiceNode* node) {
  PrintNodeLabel(node, "Choice");
  if (node->not_at_start()) {
    os() << "!^ ";
  }
  for (int i = 0; i < node->alternatives()->length(); i++) {
    const GuardedAlternative& alt = node->alternatives()->at(i);
    if (i != 0) os() << " ";
    PrintNodeLabel(alt.node());
    if (alt.guards() != nullptr && alt.guards()->length() != 0) {
      for (int g = 0; g < alt.guards()->length(); g++) {
        if (g != 0) os() << " && ";
        PrintGuard(alt.guards()->at(i));
      }
    }
  }
}

void RegExpNodePrinter<RegExpNode>::VisitLoopChoice(LoopChoiceNode* node) {
  PrintNodeLabel(node, "LoopChoice");
  if (node->read_backward()) {
    os() << "↩ ";
  }
  os() << "{ ";
  PrintNodeLabel(node->loop_node());
  os() << " } [";
  os() << "greedy: "
       << (node->alternatives()->at(0).node() == node->loop_node());
  os() << "; body can be empty: " << node->body_can_be_zero_length();
  os() << "]";
  set_color(RegExpPrinterBase::Color::kGreen);
  os() << " ⇝ ";
  PrintNodeLabel(node->continue_node());
  set_color(RegExpPrinterBase::Color::kDefault);
}

void RegExpNodePrinter<RegExpNode>::VisitNegativeLookaroundChoice(
    NegativeLookaroundChoiceNode* node) {
  PrintNodeLabel(node, "NegativeLookaroundChoice");
  if (node->not_at_start()) {
    os() << "!^ ";
  }
  for (int i = 0; i < node->alternatives()->length(); i++) {
    const GuardedAlternative& alt = node->alternatives()->at(i);
    if (i != 0) os() << " ";
    PrintNodeLabel(alt.node());
    if (alt.guards() != nullptr && alt.guards()->length() != 0) {
      for (int g = 0; g < alt.guards()->length(); g++) {
        if (g != 0) os() << " && ";
        PrintGuard(alt.guards()->at(i));
      }
    }
  }
}

void RegExpNodePrinter<RegExpNode>::VisitBackReference(
    BackReferenceNode* node) {
  PrintNodeLabel(node, "BackReference");
  if (node->read_backward()) {
    os() << "↩ ";
  }
  os() << node->start_register() << " -- " << node->end_register();
  PrintSuccess(node);
}

void RegExpNodePrinter<RegExpNode>::VisitAssertion(AssertionNode* node) {
  PrintNodeLabel(node, "Assertion");
  switch (node->assertion_type()) {
    case AssertionNode::AT_END:
      os() << "at end";
      break;
    case AssertionNode::AT_START:
      os() << "at start";
      break;
    case AssertionNode::AT_BOUNDARY:
      os() << "at boundary";
      break;
    case AssertionNode::AT_NON_BOUNDARY:
      os() << "at non-boundary";
      break;
    case AssertionNode::AFTER_NEWLINE:
      os() << "after newline";
      break;
  }
  PrintSuccess(node);
}

void RegExpNodePrinter<RegExpNode>::VisitText(TextNode* node) {
  PrintNodeLabel(node, "Text");
  if (node->read_backward()) {
    os() << "↩ ";
  }
  for (int i = 0; i < node->elements()->length(); i++) {
    const TextElement& elm = node->elements()->at(i);
    if (i != 0) os() << " ";
    RegExpAstNodePrinter tree_printer(*this, nullptr);
    tree_printer.Print(elm.tree());
  }
  os() << "; Eats: " << node->EatsAtLeast(false) << "/"
       << node->EatsAtLeast(true);
  PrintSuccess(node);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_ENABLE_REGEXP_DIAGNOSTICS
