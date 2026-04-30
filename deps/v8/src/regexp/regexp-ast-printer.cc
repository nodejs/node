// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef V8_ENABLE_REGEXP_DIAGNOSTICS

#include "src/regexp/regexp-ast-printer.h"

#include "src/regexp/regexp-ast.h"
#include "src/regexp/regexp-graph-printer.h"
#include "src/utils/ostreams.h"

namespace v8 {
namespace internal {

void RegExpAstNodePrinter::VisitCharacterRange(CharacterRange that) {
  os() << AsUC32(that.from());
  if (!that.IsSingleton()) {
    os() << "-" << AsUC32(that.to());
  }
}

void* RegExpAstNodePrinter::VisitDisjunction(RegExpDisjunction* that,
                                             void* data) {
  RegisterNode(that);
  PrintNodeLabel(that);
  os() << "(|";
  for (int i = 0; i < that->alternatives()->length(); i++) {
    os() << " ";
    that->alternatives()->at(i)->Accept(this, data);
  }
  os() << ")";
  return nullptr;
}

void* RegExpAstNodePrinter::VisitAlternative(RegExpAlternative* that,
                                             void* data) {
  RegisterNode(that);
  PrintNodeLabel(that);
  os() << "(:";
  for (int i = 0; i < that->nodes()->length(); i++) {
    os() << " ";
    that->nodes()->at(i)->Accept(this, data);
  }
  os() << ")";
  return nullptr;
}

void* RegExpAstNodePrinter::VisitClassRanges(RegExpClassRanges* that,
                                             void* data) {
  RegisterNode(that);
  PrintNodeLabel(that);
  if (that->is_negated()) os() << "^";
  os() << "[";
  for (int i = 0; i < that->ranges(zone())->length(); i++) {
    if (i > 0) os() << " ";
    VisitCharacterRange(that->ranges(zone())->at(i));
  }
  os() << "]";
  return nullptr;
}

void* RegExpAstNodePrinter::VisitClassSetOperand(RegExpClassSetOperand* that,
                                                 void* data) {
  RegisterNode(that);
  PrintNodeLabel(that);
  os() << "![";
  for (int i = 0; i < that->ranges()->length(); i++) {
    if (i > 0) os() << " ";
    VisitCharacterRange(that->ranges()->at(i));
  }
  if (that->has_strings()) {
    for (auto iter : *that->strings()) {
      os() << " '";
      os() << std::string(iter.first.begin(), iter.first.end());
      os() << "'";
    }
  }
  os() << "]";
  return nullptr;
}

void* RegExpAstNodePrinter::VisitClassSetExpression(
    RegExpClassSetExpression* that, void* data) {
  RegisterNode(that);
  PrintNodeLabel(that);
  switch (that->operation()) {
    case RegExpClassSetExpression::OperationType::kUnion:
      os() << "++";
      break;
    case RegExpClassSetExpression::OperationType::kIntersection:
      os() << "&&";
      break;
    case RegExpClassSetExpression::OperationType::kSubtraction:
      os() << "--";
      break;
  }
  if (that->is_negated()) os() << "^";
  os() << "[";
  for (int i = 0; i < that->operands()->length(); i++) {
    if (i > 0) os() << " ";
    that->operands()->at(i)->Accept(this, data);
  }
  os() << "]";
  return nullptr;
}

void* RegExpAstNodePrinter::VisitAssertion(RegExpAssertion* that, void* data) {
  RegisterNode(that);
  PrintNodeLabel(that);
  switch (that->assertion_type()) {
    case RegExpAssertion::Type::START_OF_INPUT:
      os() << "@^i";
      break;
    case RegExpAssertion::Type::END_OF_INPUT:
      os() << "@$i";
      break;
    case RegExpAssertion::Type::START_OF_LINE:
      os() << "@^l";
      break;
    case RegExpAssertion::Type::END_OF_LINE:
      os() << "@$l";
      break;
    case RegExpAssertion::Type::BOUNDARY:
      os() << "@b";
      break;
    case RegExpAssertion::Type::NON_BOUNDARY:
      os() << "@B";
      break;
  }
  return nullptr;
}

void* RegExpAstNodePrinter::VisitAtom(RegExpAtom* that, void* data) {
  RegisterNode(that);
  PrintNodeLabel(that);
  os() << "'";
  base::Vector<const base::uc16> chardata = that->data();
  for (int i = 0; i < chardata.length(); i++) {
    os() << AsUC16(chardata[i]);
  }
  os() << "'";
  return nullptr;
}

void* RegExpAstNodePrinter::VisitText(RegExpText* that, void* data) {
  RegisterNode(that);
  PrintNodeLabel(that);
  if (that->elements()->length() == 1) {
    that->elements()->at(0).tree()->Accept(this, data);
  } else {
    os() << "(!";
    for (int i = 0; i < that->elements()->length(); i++) {
      os() << " ";
      that->elements()->at(i).tree()->Accept(this, data);
    }
    os() << ")";
  }
  return nullptr;
}

void* RegExpAstNodePrinter::VisitQuantifier(RegExpQuantifier* that,
                                            void* data) {
  RegisterNode(that);
  PrintNodeLabel(that);
  os() << "(# " << that->min() << " ";
  if (that->max() == RegExpTree::kInfinity) {
    os() << "- ";
  } else {
    os() << that->max() << " ";
  }
  os() << (that->is_greedy() ? "g " : that->is_possessive() ? "p " : "n ");
  that->body()->Accept(this, data);
  os() << ")";
  return nullptr;
}

void* RegExpAstNodePrinter::VisitCapture(RegExpCapture* that, void* data) {
  RegisterNode(that);
  PrintNodeLabel(that);
  os() << "(^ ";
  that->body()->Accept(this, data);
  os() << ")";
  return nullptr;
}

void* RegExpAstNodePrinter::VisitGroup(RegExpGroup* that, void* data) {
  RegisterNode(that);
  PrintNodeLabel(that);
  os() << "(?" << that->flags() << ": ";
  that->body()->Accept(this, data);
  os() << ")";
  return nullptr;
}

void* RegExpAstNodePrinter::VisitLookaround(RegExpLookaround* that,
                                            void* data) {
  RegisterNode(that);
  PrintNodeLabel(that);
  os() << "(";
  os() << (that->type() == RegExpLookaround::LOOKAHEAD ? "->" : "<-");
  os() << (that->is_positive() ? " + " : " - ");
  that->body()->Accept(this, data);
  os() << ")";
  return nullptr;
}

void* RegExpAstNodePrinter::VisitBackReference(RegExpBackReference* that,
                                               void* data) {
  RegisterNode(that);
  PrintNodeLabel(that);
  os() << "(<- " << that->captures()->first()->index();
  for (int i = 1; i < that->captures()->length(); ++i) {
    os() << "," << that->captures()->at(i)->index();
  }
  os() << ")";
  return nullptr;
}

void* RegExpAstNodePrinter::VisitEmpty(RegExpEmpty* that, void* data) {
  RegisterNode(that);
  PrintNodeLabel(that);
  os() << '%';
  return nullptr;
}

void RegExpAstNodePrinter::Print(RegExpTree* tree) {
  tree->Accept(this, nullptr);
}

TraceRegExpTreeScope::TraceRegExpTreeScope(RegExpDiagnostics* diagnostics)
    : parent_(diagnostics ? diagnostics->trace_tree_scope() : nullptr),
      depth_(parent_ != nullptr ? parent_->depth_ + 1 : -1),
      diagnostics_(diagnostics) {
  if (diagnostics_) {
    diagnostics_->set_trace_tree_scope(this);
  }
}

TraceRegExpTreeScope::~TraceRegExpTreeScope() {
  if (diagnostics_) {
    diagnostics_->set_trace_tree_scope(parent_);
  }
}

void TraceRegExpTreeScope::PrintTree(RegExpTree* tree) {
  std::ostream& o = os();
  RegExpAstNodePrinter* printer = diagnostics_->ast_printer();
  printer->set_color(RegExpPrinterBase::Color::kRed);
  o << "- ";
  printer->Print(tree);
  printer->set_color(RegExpPrinterBase::Color::kDefault);
  o << std::endl;
}

std::ostream& TraceRegExpTreeScope::os() {
  std::ostream& os = diagnostics_->os();
  for (int i = 0; i < depth_; i++) os << "  ";
  return os;
}

}  // namespace internal
}  // namespace v8

#endif  // V8_ENABLE_REGEXP_DIAGNOSTICS
