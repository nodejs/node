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
namespace regexp {

void AstNodePrinter::VisitCharacterRange(CharacterRange that) {
  os() << AsUC32(that.from());
  if (!that.IsSingleton()) {
    os() << "-" << AsUC32(that.to());
  }
}

void* AstNodePrinter::VisitDisjunction(Disjunction* that, void* data) {
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

void* AstNodePrinter::VisitAlternative(Alternative* that, void* data) {
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

void* AstNodePrinter::VisitClassRanges(ClassRanges* that, void* data) {
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

void* AstNodePrinter::VisitClassSetOperand(ClassSetOperand* that, void* data) {
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

void* AstNodePrinter::VisitClassSetExpression(ClassSetExpression* that,
                                              void* data) {
  RegisterNode(that);
  PrintNodeLabel(that);
  switch (that->operation()) {
    case ClassSetExpression::OperationType::kUnion:
      os() << "++";
      break;
    case ClassSetExpression::OperationType::kIntersection:
      os() << "&&";
      break;
    case ClassSetExpression::OperationType::kSubtraction:
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

void* AstNodePrinter::VisitAssertion(Assertion* that, void* data) {
  RegisterNode(that);
  PrintNodeLabel(that);
  switch (that->assertion_type()) {
    case Assertion::Type::START_OF_INPUT:
      os() << "@^i";
      break;
    case Assertion::Type::END_OF_INPUT:
      os() << "@$i";
      break;
    case Assertion::Type::START_OF_LINE:
      os() << "@^l";
      break;
    case Assertion::Type::END_OF_LINE:
      os() << "@$l";
      break;
    case Assertion::Type::BOUNDARY:
      os() << "@b";
      break;
    case Assertion::Type::NON_BOUNDARY:
      os() << "@B";
      break;
  }
  return nullptr;
}

void* AstNodePrinter::VisitAtom(Atom* that, void* data) {
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

void* AstNodePrinter::VisitText(Text* that, void* data) {
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

void* AstNodePrinter::VisitQuantifier(Quantifier* that, void* data) {
  RegisterNode(that);
  PrintNodeLabel(that);
  os() << "(# " << that->min() << " ";
  if (that->max() == Tree::kInfinity) {
    os() << "- ";
  } else {
    os() << that->max() << " ";
  }
  os() << (that->is_greedy() ? "g " : that->is_possessive() ? "p " : "n ");
  that->body()->Accept(this, data);
  os() << ")";
  return nullptr;
}

void* AstNodePrinter::VisitCapture(Capture* that, void* data) {
  RegisterNode(that);
  PrintNodeLabel(that);
  os() << "(^ ";
  that->body()->Accept(this, data);
  os() << ")";
  return nullptr;
}

void* AstNodePrinter::VisitGroup(Group* that, void* data) {
  RegisterNode(that);
  PrintNodeLabel(that);
  os() << "(?" << that->flags() << ": ";
  that->body()->Accept(this, data);
  os() << ")";
  return nullptr;
}

void* AstNodePrinter::VisitLookaround(Lookaround* that, void* data) {
  RegisterNode(that);
  PrintNodeLabel(that);
  os() << "(";
  os() << (that->type() == Lookaround::LOOKAHEAD ? "->" : "<-");
  os() << (that->is_positive() ? " + " : " - ");
  that->body()->Accept(this, data);
  os() << ")";
  return nullptr;
}

void* AstNodePrinter::VisitBackReference(BackReference* that, void* data) {
  RegisterNode(that);
  PrintNodeLabel(that);
  os() << "(<- " << that->captures()->first()->index();
  for (int i = 1; i < that->captures()->length(); ++i) {
    os() << "," << that->captures()->at(i)->index();
  }
  os() << ")";
  return nullptr;
}

void* AstNodePrinter::VisitEmpty(Empty* that, void* data) {
  RegisterNode(that);
  PrintNodeLabel(that);
  os() << '%';
  return nullptr;
}

void AstNodePrinter::Print(Tree* tree) { tree->Accept(this, nullptr); }

TraceTreeScope::TraceTreeScope(Diagnostics* diagnostics)
    : parent_(diagnostics ? diagnostics->trace_tree_scope() : nullptr),
      depth_(parent_ != nullptr ? parent_->depth_ + 1 : -1),
      diagnostics_(diagnostics) {
  if (diagnostics_) {
    diagnostics_->set_trace_tree_scope(this);
  }
}

TraceTreeScope::~TraceTreeScope() {
  if (diagnostics_) {
    diagnostics_->set_trace_tree_scope(parent_);
  }
}

void TraceTreeScope::PrintTree(Tree* tree) {
  std::ostream& o = os();
  AstNodePrinter* printer = diagnostics_->ast_printer();
  printer->set_color(PrinterBase::Color::kRed);
  o << "- ";
  printer->Print(tree);
  printer->set_color(PrinterBase::Color::kDefault);
  o << std::endl;
}

std::ostream& TraceTreeScope::os() {
  std::ostream& os = diagnostics_->os();
  for (int i = 0; i < depth_; i++) os << "  ";
  return os;
}

}  // namespace regexp
}  // namespace internal
}  // namespace v8

#endif  // V8_ENABLE_REGEXP_DIAGNOSTICS
