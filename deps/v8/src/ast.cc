// Copyright 2010 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "v8.h"

#include "ast.h"
#include "data-flow.h"
#include "parser.h"
#include "scopes.h"
#include "string-stream.h"

namespace v8 {
namespace internal {


VariableProxySentinel VariableProxySentinel::this_proxy_(true);
VariableProxySentinel VariableProxySentinel::identifier_proxy_(false);
ValidLeftHandSideSentinel ValidLeftHandSideSentinel::instance_;
Property Property::this_property_(VariableProxySentinel::this_proxy(), NULL, 0);
Call Call::sentinel_(NULL, NULL, 0);


// ----------------------------------------------------------------------------
// All the Accept member functions for each syntax tree node type.

#define DECL_ACCEPT(type)                                       \
  void type::Accept(AstVisitor* v) { v->Visit##type(this); }
AST_NODE_LIST(DECL_ACCEPT)
#undef DECL_ACCEPT


// ----------------------------------------------------------------------------
// Implementation of other node functionality.

Assignment* ExpressionStatement::StatementAsSimpleAssignment() {
  return (expression()->AsAssignment() != NULL &&
          !expression()->AsAssignment()->is_compound())
      ? expression()->AsAssignment()
      : NULL;
}


CountOperation* ExpressionStatement::StatementAsCountOperation() {
  return expression()->AsCountOperation();
}


VariableProxy::VariableProxy(Handle<String> name,
                             bool is_this,
                             bool inside_with)
  : name_(name),
    var_(NULL),
    is_this_(is_this),
    inside_with_(inside_with),
    is_trivial_(false),
    reaching_definitions_(NULL),
    is_primitive_(false) {
  // names must be canonicalized for fast equality checks
  ASSERT(name->IsSymbol());
}


VariableProxy::VariableProxy(bool is_this)
  : is_this_(is_this),
    reaching_definitions_(NULL),
    is_primitive_(false) {
}


void VariableProxy::BindTo(Variable* var) {
  ASSERT(var_ == NULL);  // must be bound only once
  ASSERT(var != NULL);  // must bind
  ASSERT((is_this() && var->is_this()) || name_.is_identical_to(var->name()));
  // Ideally CONST-ness should match. However, this is very hard to achieve
  // because we don't know the exact semantics of conflicting (const and
  // non-const) multiple variable declarations, const vars introduced via
  // eval() etc.  Const-ness and variable declarations are a complete mess
  // in JS. Sigh...
  var_ = var;
  var->set_is_used(true);
}


Token::Value Assignment::binary_op() const {
  switch (op_) {
    case Token::ASSIGN_BIT_OR: return Token::BIT_OR;
    case Token::ASSIGN_BIT_XOR: return Token::BIT_XOR;
    case Token::ASSIGN_BIT_AND: return Token::BIT_AND;
    case Token::ASSIGN_SHL: return Token::SHL;
    case Token::ASSIGN_SAR: return Token::SAR;
    case Token::ASSIGN_SHR: return Token::SHR;
    case Token::ASSIGN_ADD: return Token::ADD;
    case Token::ASSIGN_SUB: return Token::SUB;
    case Token::ASSIGN_MUL: return Token::MUL;
    case Token::ASSIGN_DIV: return Token::DIV;
    case Token::ASSIGN_MOD: return Token::MOD;
    default: UNREACHABLE();
  }
  return Token::ILLEGAL;
}


bool FunctionLiteral::AllowsLazyCompilation() {
  return scope()->AllowsLazyCompilation();
}


ObjectLiteral::Property::Property(Literal* key, Expression* value) {
  key_ = key;
  value_ = value;
  Object* k = *key->handle();
  if (k->IsSymbol() && Heap::Proto_symbol()->Equals(String::cast(k))) {
    kind_ = PROTOTYPE;
  } else if (value_->AsMaterializedLiteral() != NULL) {
    kind_ = MATERIALIZED_LITERAL;
  } else if (value_->AsLiteral() != NULL) {
    kind_ = CONSTANT;
  } else {
    kind_ = COMPUTED;
  }
}


ObjectLiteral::Property::Property(bool is_getter, FunctionLiteral* value) {
  key_ = new Literal(value->name());
  value_ = value;
  kind_ = is_getter ? GETTER : SETTER;
}


bool ObjectLiteral::Property::IsCompileTimeValue() {
  return kind_ == CONSTANT ||
      (kind_ == MATERIALIZED_LITERAL &&
       CompileTimeValue::IsCompileTimeValue(value_));
}


void TargetCollector::AddTarget(BreakTarget* target) {
  // Add the label to the collector, but discard duplicates.
  int length = targets_->length();
  for (int i = 0; i < length; i++) {
    if (targets_->at(i) == target) return;
  }
  targets_->Add(target);
}


bool Expression::GuaranteedSmiResult() {
  BinaryOperation* node = AsBinaryOperation();
  if (node == NULL) return false;
  Token::Value op = node->op();
  switch (op) {
    case Token::COMMA:
    case Token::OR:
    case Token::AND:
    case Token::ADD:
    case Token::SUB:
    case Token::MUL:
    case Token::DIV:
    case Token::MOD:
    case Token::BIT_XOR:
    case Token::SHL:
      return false;
      break;
    case Token::BIT_OR:
    case Token::BIT_AND: {
      Literal* left = node->left()->AsLiteral();
      Literal* right = node->right()->AsLiteral();
      if (left != NULL && left->handle()->IsSmi()) {
        int value = Smi::cast(*left->handle())->value();
        if (op == Token::BIT_OR && ((value & 0xc0000000) == 0xc0000000)) {
          // Result of bitwise or is always a negative Smi.
          return true;
        }
        if (op == Token::BIT_AND && ((value & 0xc0000000) == 0)) {
          // Result of bitwise and is always a positive Smi.
          return true;
        }
      }
      if (right != NULL && right->handle()->IsSmi()) {
        int value = Smi::cast(*right->handle())->value();
        if (op == Token::BIT_OR && ((value & 0xc0000000) == 0xc0000000)) {
          // Result of bitwise or is always a negative Smi.
          return true;
        }
        if (op == Token::BIT_AND && ((value & 0xc0000000) == 0)) {
          // Result of bitwise and is always a positive Smi.
          return true;
        }
      }
      return false;
      break;
    }
    case Token::SAR:
    case Token::SHR: {
      Literal* right = node->right()->AsLiteral();
       if (right != NULL && right->handle()->IsSmi()) {
        int value = Smi::cast(*right->handle())->value();
        if ((value & 0x1F) > 1 ||
            (op == Token::SAR && (value & 0x1F) == 1)) {
          return true;
        }
       }
       return false;
       break;
    }
    default:
      UNREACHABLE();
      break;
  }
  return false;
}

// ----------------------------------------------------------------------------
// Implementation of AstVisitor

bool AstVisitor::CheckStackOverflow() {
  if (stack_overflow_) return true;
  StackLimitCheck check;
  if (!check.HasOverflowed()) return false;
  return (stack_overflow_ = true);
}


void AstVisitor::VisitDeclarations(ZoneList<Declaration*>* declarations) {
  for (int i = 0; i < declarations->length(); i++) {
    Visit(declarations->at(i));
  }
}


void AstVisitor::VisitStatements(ZoneList<Statement*>* statements) {
  for (int i = 0; i < statements->length(); i++) {
    Visit(statements->at(i));
  }
}


void AstVisitor::VisitExpressions(ZoneList<Expression*>* expressions) {
  for (int i = 0; i < expressions->length(); i++) {
    // The variable statement visiting code may pass NULL expressions
    // to this code. Maybe this should be handled by introducing an
    // undefined expression or literal?  Revisit this code if this
    // changes
    Expression* expression = expressions->at(i);
    if (expression != NULL) Visit(expression);
  }
}


// ----------------------------------------------------------------------------
// Regular expressions

#define MAKE_ACCEPT(Name)                                            \
  void* RegExp##Name::Accept(RegExpVisitor* visitor, void* data) {   \
    return visitor->Visit##Name(this, data);                         \
  }
FOR_EACH_REG_EXP_TREE_TYPE(MAKE_ACCEPT)
#undef MAKE_ACCEPT

#define MAKE_TYPE_CASE(Name)                                         \
  RegExp##Name* RegExpTree::As##Name() {                             \
    return NULL;                                                     \
  }                                                                  \
  bool RegExpTree::Is##Name() { return false; }
FOR_EACH_REG_EXP_TREE_TYPE(MAKE_TYPE_CASE)
#undef MAKE_TYPE_CASE

#define MAKE_TYPE_CASE(Name)                                        \
  RegExp##Name* RegExp##Name::As##Name() {                          \
    return this;                                                    \
  }                                                                 \
  bool RegExp##Name::Is##Name() { return true; }
FOR_EACH_REG_EXP_TREE_TYPE(MAKE_TYPE_CASE)
#undef MAKE_TYPE_CASE

RegExpEmpty RegExpEmpty::kInstance;


static Interval ListCaptureRegisters(ZoneList<RegExpTree*>* children) {
  Interval result = Interval::Empty();
  for (int i = 0; i < children->length(); i++)
    result = result.Union(children->at(i)->CaptureRegisters());
  return result;
}


Interval RegExpAlternative::CaptureRegisters() {
  return ListCaptureRegisters(nodes());
}


Interval RegExpDisjunction::CaptureRegisters() {
  return ListCaptureRegisters(alternatives());
}


Interval RegExpLookahead::CaptureRegisters() {
  return body()->CaptureRegisters();
}


Interval RegExpCapture::CaptureRegisters() {
  Interval self(StartRegister(index()), EndRegister(index()));
  return self.Union(body()->CaptureRegisters());
}


Interval RegExpQuantifier::CaptureRegisters() {
  return body()->CaptureRegisters();
}


bool RegExpAssertion::IsAnchored() {
  return type() == RegExpAssertion::START_OF_INPUT;
}


bool RegExpAlternative::IsAnchored() {
  ZoneList<RegExpTree*>* nodes = this->nodes();
  for (int i = 0; i < nodes->length(); i++) {
    RegExpTree* node = nodes->at(i);
    if (node->IsAnchored()) { return true; }
    if (node->max_match() > 0) { return false; }
  }
  return false;
}


bool RegExpDisjunction::IsAnchored() {
  ZoneList<RegExpTree*>* alternatives = this->alternatives();
  for (int i = 0; i < alternatives->length(); i++) {
    if (!alternatives->at(i)->IsAnchored())
      return false;
  }
  return true;
}


bool RegExpLookahead::IsAnchored() {
  return is_positive() && body()->IsAnchored();
}


bool RegExpCapture::IsAnchored() {
  return body()->IsAnchored();
}


// Convert regular expression trees to a simple sexp representation.
// This representation should be different from the input grammar
// in as many cases as possible, to make it more difficult for incorrect
// parses to look as correct ones which is likely if the input and
// output formats are alike.
class RegExpUnparser: public RegExpVisitor {
 public:
  RegExpUnparser();
  void VisitCharacterRange(CharacterRange that);
  SmartPointer<const char> ToString() { return stream_.ToCString(); }
#define MAKE_CASE(Name) virtual void* Visit##Name(RegExp##Name*, void* data);
  FOR_EACH_REG_EXP_TREE_TYPE(MAKE_CASE)
#undef MAKE_CASE
 private:
  StringStream* stream() { return &stream_; }
  HeapStringAllocator alloc_;
  StringStream stream_;
};


RegExpUnparser::RegExpUnparser() : stream_(&alloc_) {
}


void* RegExpUnparser::VisitDisjunction(RegExpDisjunction* that, void* data) {
  stream()->Add("(|");
  for (int i = 0; i <  that->alternatives()->length(); i++) {
    stream()->Add(" ");
    that->alternatives()->at(i)->Accept(this, data);
  }
  stream()->Add(")");
  return NULL;
}


void* RegExpUnparser::VisitAlternative(RegExpAlternative* that, void* data) {
  stream()->Add("(:");
  for (int i = 0; i <  that->nodes()->length(); i++) {
    stream()->Add(" ");
    that->nodes()->at(i)->Accept(this, data);
  }
  stream()->Add(")");
  return NULL;
}


void RegExpUnparser::VisitCharacterRange(CharacterRange that) {
  stream()->Add("%k", that.from());
  if (!that.IsSingleton()) {
    stream()->Add("-%k", that.to());
  }
}



void* RegExpUnparser::VisitCharacterClass(RegExpCharacterClass* that,
                                          void* data) {
  if (that->is_negated())
    stream()->Add("^");
  stream()->Add("[");
  for (int i = 0; i < that->ranges()->length(); i++) {
    if (i > 0) stream()->Add(" ");
    VisitCharacterRange(that->ranges()->at(i));
  }
  stream()->Add("]");
  return NULL;
}


void* RegExpUnparser::VisitAssertion(RegExpAssertion* that, void* data) {
  switch (that->type()) {
    case RegExpAssertion::START_OF_INPUT:
      stream()->Add("@^i");
      break;
    case RegExpAssertion::END_OF_INPUT:
      stream()->Add("@$i");
      break;
    case RegExpAssertion::START_OF_LINE:
      stream()->Add("@^l");
      break;
    case RegExpAssertion::END_OF_LINE:
      stream()->Add("@$l");
       break;
    case RegExpAssertion::BOUNDARY:
      stream()->Add("@b");
      break;
    case RegExpAssertion::NON_BOUNDARY:
      stream()->Add("@B");
      break;
  }
  return NULL;
}


void* RegExpUnparser::VisitAtom(RegExpAtom* that, void* data) {
  stream()->Add("'");
  Vector<const uc16> chardata = that->data();
  for (int i = 0; i < chardata.length(); i++) {
    stream()->Add("%k", chardata[i]);
  }
  stream()->Add("'");
  return NULL;
}


void* RegExpUnparser::VisitText(RegExpText* that, void* data) {
  if (that->elements()->length() == 1) {
    that->elements()->at(0).data.u_atom->Accept(this, data);
  } else {
    stream()->Add("(!");
    for (int i = 0; i < that->elements()->length(); i++) {
      stream()->Add(" ");
      that->elements()->at(i).data.u_atom->Accept(this, data);
    }
    stream()->Add(")");
  }
  return NULL;
}


void* RegExpUnparser::VisitQuantifier(RegExpQuantifier* that, void* data) {
  stream()->Add("(# %i ", that->min());
  if (that->max() == RegExpTree::kInfinity) {
    stream()->Add("- ");
  } else {
    stream()->Add("%i ", that->max());
  }
  stream()->Add(that->is_greedy() ? "g " : that->is_possessive() ? "p " : "n ");
  that->body()->Accept(this, data);
  stream()->Add(")");
  return NULL;
}


void* RegExpUnparser::VisitCapture(RegExpCapture* that, void* data) {
  stream()->Add("(^ ");
  that->body()->Accept(this, data);
  stream()->Add(")");
  return NULL;
}


void* RegExpUnparser::VisitLookahead(RegExpLookahead* that, void* data) {
  stream()->Add("(-> ");
  stream()->Add(that->is_positive() ? "+ " : "- ");
  that->body()->Accept(this, data);
  stream()->Add(")");
  return NULL;
}


void* RegExpUnparser::VisitBackReference(RegExpBackReference* that,
                                         void* data) {
  stream()->Add("(<- %i)", that->index());
  return NULL;
}


void* RegExpUnparser::VisitEmpty(RegExpEmpty* that, void* data) {
  stream()->Put('%');
  return NULL;
}


SmartPointer<const char> RegExpTree::ToString() {
  RegExpUnparser unparser;
  Accept(&unparser, NULL);
  return unparser.ToString();
}


RegExpDisjunction::RegExpDisjunction(ZoneList<RegExpTree*>* alternatives)
    : alternatives_(alternatives) {
  ASSERT(alternatives->length() > 1);
  RegExpTree* first_alternative = alternatives->at(0);
  min_match_ = first_alternative->min_match();
  max_match_ = first_alternative->max_match();
  for (int i = 1; i < alternatives->length(); i++) {
    RegExpTree* alternative = alternatives->at(i);
    min_match_ = Min(min_match_, alternative->min_match());
    max_match_ = Max(max_match_, alternative->max_match());
  }
}


RegExpAlternative::RegExpAlternative(ZoneList<RegExpTree*>* nodes)
    : nodes_(nodes) {
  ASSERT(nodes->length() > 1);
  min_match_ = 0;
  max_match_ = 0;
  for (int i = 0; i < nodes->length(); i++) {
    RegExpTree* node = nodes->at(i);
    min_match_ += node->min_match();
    int node_max_match = node->max_match();
    if (kInfinity - max_match_ < node_max_match) {
      max_match_ = kInfinity;
    } else {
      max_match_ += node->max_match();
    }
  }
}

// IsPrimitive implementation.  IsPrimitive is true if the value of an
// expression is known at compile-time to be any JS type other than Object
// (e.g, it is Undefined, Null, Boolean, String, or Number).

// The following expression types are never primitive because they express
// Object values.
bool FunctionLiteral::IsPrimitive() { return false; }
bool SharedFunctionInfoLiteral::IsPrimitive() { return false; }
bool RegExpLiteral::IsPrimitive() { return false; }
bool ObjectLiteral::IsPrimitive() { return false; }
bool ArrayLiteral::IsPrimitive() { return false; }
bool CatchExtensionObject::IsPrimitive() { return false; }
bool CallNew::IsPrimitive() { return false; }
bool ThisFunction::IsPrimitive() { return false; }


// The following expression types are not always primitive because we do not
// have enough information to conclude that they are.
bool Property::IsPrimitive() { return false; }
bool Call::IsPrimitive() { return false; }
bool CallRuntime::IsPrimitive() { return false; }


// A variable use is not primitive unless the primitive-type analysis
// determines otherwise.
bool VariableProxy::IsPrimitive() {
  ASSERT(!is_primitive_ || (var() != NULL && var()->IsStackAllocated()));
  return is_primitive_;
}

// The value of a conditional is the value of one of the alternatives.  It's
// always primitive if both alternatives are always primitive.
bool Conditional::IsPrimitive() {
  return then_expression()->IsPrimitive() && else_expression()->IsPrimitive();
}


// A literal is primitive when it is not a JSObject.
bool Literal::IsPrimitive() { return !handle()->IsJSObject(); }


// The value of an assignment is the value of its right-hand side.
bool Assignment::IsPrimitive() {
  switch (op()) {
    case Token::INIT_VAR:
    case Token::INIT_CONST:
    case Token::ASSIGN:
      return value()->IsPrimitive();

    default:
      // {|=, ^=, &=, <<=, >>=, >>>=, +=, -=, *=, /=, %=}
      // Arithmetic operations are always primitive.  They express Numbers
      // with the exception of +, which expresses a Number or a String.
      return true;
  }
}


// Throw does not express a value, so it's trivially always primitive.
bool Throw::IsPrimitive() { return true; }


// Unary operations always express primitive values.  delete and ! express
// Booleans, void Undefined, typeof String, +, -, and ~ Numbers.
bool UnaryOperation::IsPrimitive() { return true; }


// Count operations (pre- and post-fix increment and decrement) always
// express primitive values (Numbers).  See ECMA-262-3, 11.3.1, 11.3.2,
// 11.4.4, ane 11.4.5.
bool CountOperation::IsPrimitive() { return true; }


// Binary operations depend on the operator.
bool BinaryOperation::IsPrimitive() {
  switch (op()) {
    case Token::COMMA:
      // Value is the value of the right subexpression.
      return right()->IsPrimitive();

    case Token::OR:
    case Token::AND:
      // Value is the value one of the subexpressions.
      return left()->IsPrimitive() && right()->IsPrimitive();

    default:
      // {|, ^, &, <<, >>, >>>, +, -, *, /, %}
      // Arithmetic operations are always primitive.  They express Numbers
      // with the exception of +, which expresses a Number or a String.
      return true;
  }
}


// Compare operations always express Boolean values.
bool CompareOperation::IsPrimitive() { return true; }


// Overridden IsCritical member functions.  IsCritical is true for AST nodes
// whose evaluation is absolutely required (they are never dead) because
// they are externally visible.

// References to global variables or lookup slots are critical because they
// may have getters.  All others, including parameters rewritten to explicit
// property references, are not critical.
bool VariableProxy::IsCritical() {
  Variable* var = AsVariable();
  return var != NULL &&
      (var->slot() == NULL || var->slot()->type() == Slot::LOOKUP);
}


// Literals are never critical.
bool Literal::IsCritical() { return false; }


// Property assignments and throwing of reference errors are always
// critical.  Assignments to escaping variables are also critical.  In
// addition the operation of compound assignments is critical if either of
// its operands is non-primitive (the arithmetic operations all use one of
// ToPrimitive, ToNumber, ToInt32, or ToUint32 on each of their operands).
// In this case, we mark the entire AST node as critical because there is
// no binary operation node to mark.
bool Assignment::IsCritical() {
  Variable* var = AssignedVariable();
  return var == NULL ||
      !var->IsStackAllocated() ||
      (is_compound() && (!target()->IsPrimitive() || !value()->IsPrimitive()));
}


// Property references are always critical, because they may have getters.
bool Property::IsCritical() { return true; }


// Calls are always critical.
bool Call::IsCritical() { return true; }


// +,- use ToNumber on the value of their operand.
bool UnaryOperation::IsCritical() {
  ASSERT(op() == Token::ADD || op() == Token::SUB);
  return !expression()->IsPrimitive();
}


// Count operations targeting properties and reference errors are always
// critical.  Count operations on escaping variables are critical.  Count
// operations targeting non-primitives are also critical because they use
// ToNumber.
bool CountOperation::IsCritical() {
  Variable* var = AssignedVariable();
  return var == NULL ||
      !var->IsStackAllocated() ||
      !expression()->IsPrimitive();
}


// Arithmetic operations all use one of ToPrimitive, ToNumber, ToInt32, or
// ToUint32 on each of their operands.
bool BinaryOperation::IsCritical() {
  ASSERT(op() != Token::COMMA);
  ASSERT(op() != Token::OR);
  ASSERT(op() != Token::AND);
  return !left()->IsPrimitive() || !right()->IsPrimitive();
}


// <, >, <=, and >= all use ToPrimitive on both their operands.
bool CompareOperation::IsCritical() {
  ASSERT(op() != Token::EQ);
  ASSERT(op() != Token::NE);
  ASSERT(op() != Token::EQ_STRICT);
  ASSERT(op() != Token::NE_STRICT);
  ASSERT(op() != Token::INSTANCEOF);
  ASSERT(op() != Token::IN);
  return !left()->IsPrimitive() || !right()->IsPrimitive();
}


// Implementation of a copy visitor. The visitor create a deep copy
// of ast nodes. Nodes that do not require a deep copy are copied
// with the default copy constructor.

AstNode::AstNode(AstNode* other) : num_(kNoNumber) {
  // AST node number should be unique. Assert that we only copy AstNodes
  // before node numbers are assigned.
  ASSERT(other->num_ == kNoNumber);
}


Statement::Statement(Statement* other)
    : AstNode(other), statement_pos_(other->statement_pos_) {}


Expression::Expression(Expression* other)
    : AstNode(other),
      bitfields_(other->bitfields_),
      type_(other->type_) {}


BreakableStatement::BreakableStatement(BreakableStatement* other)
    : Statement(other), labels_(other->labels_), type_(other->type_) {}


Block::Block(Block* other, ZoneList<Statement*>* statements)
    : BreakableStatement(other),
      statements_(statements->length()),
      is_initializer_block_(other->is_initializer_block_) {
  statements_.AddAll(*statements);
}


ExpressionStatement::ExpressionStatement(ExpressionStatement* other,
                                         Expression* expression)
    : Statement(other), expression_(expression) {}


IfStatement::IfStatement(IfStatement* other,
                         Expression* condition,
                         Statement* then_statement,
                         Statement* else_statement)
    : Statement(other),
      condition_(condition),
      then_statement_(then_statement),
      else_statement_(else_statement) {}


EmptyStatement::EmptyStatement(EmptyStatement* other) : Statement(other) {}


IterationStatement::IterationStatement(IterationStatement* other,
                                       Statement* body)
    : BreakableStatement(other), body_(body) {}


ForStatement::ForStatement(ForStatement* other,
                           Statement* init,
                           Expression* cond,
                           Statement* next,
                           Statement* body)
    : IterationStatement(other, body),
      init_(init),
      cond_(cond),
      next_(next),
      may_have_function_literal_(other->may_have_function_literal_),
      loop_variable_(other->loop_variable_),
      peel_this_loop_(other->peel_this_loop_) {}


Assignment::Assignment(Assignment* other,
                       Expression* target,
                       Expression* value)
    : Expression(other),
      op_(other->op_),
      target_(target),
      value_(value),
      pos_(other->pos_),
      block_start_(other->block_start_),
      block_end_(other->block_end_) {}


Property::Property(Property* other, Expression* obj, Expression* key)
    : Expression(other),
      obj_(obj),
      key_(key),
      pos_(other->pos_),
      type_(other->type_) {}


Call::Call(Call* other,
           Expression* expression,
           ZoneList<Expression*>* arguments)
    : Expression(other),
      expression_(expression),
      arguments_(arguments),
      pos_(other->pos_) {}


UnaryOperation::UnaryOperation(UnaryOperation* other, Expression* expression)
    : Expression(other), op_(other->op_), expression_(expression) {}


BinaryOperation::BinaryOperation(Expression* other,
                                 Token::Value op,
                                 Expression* left,
                                 Expression* right)
    : Expression(other), op_(op), left_(left), right_(right) {}


CountOperation::CountOperation(CountOperation* other, Expression* expression)
    : Expression(other),
      is_prefix_(other->is_prefix_),
      op_(other->op_),
      expression_(expression) {}


CompareOperation::CompareOperation(CompareOperation* other,
                                   Expression* left,
                                   Expression* right)
    : Expression(other),
      op_(other->op_),
      left_(left),
      right_(right) {}


Expression* CopyAstVisitor::DeepCopyExpr(Expression* expr) {
  expr_ = NULL;
  if (expr != NULL) Visit(expr);
  return expr_;
}


Statement* CopyAstVisitor::DeepCopyStmt(Statement* stmt) {
  stmt_ = NULL;
  if (stmt != NULL) Visit(stmt);
  return stmt_;
}


ZoneList<Expression*>* CopyAstVisitor::DeepCopyExprList(
    ZoneList<Expression*>* expressions) {
  ZoneList<Expression*>* copy =
      new ZoneList<Expression*>(expressions->length());
  for (int i = 0; i < expressions->length(); i++) {
    copy->Add(DeepCopyExpr(expressions->at(i)));
  }
  return copy;
}


ZoneList<Statement*>* CopyAstVisitor::DeepCopyStmtList(
    ZoneList<Statement*>* statements) {
  ZoneList<Statement*>* copy = new ZoneList<Statement*>(statements->length());
  for (int i = 0; i < statements->length(); i++) {
    copy->Add(DeepCopyStmt(statements->at(i)));
  }
  return copy;
}


void CopyAstVisitor::VisitBlock(Block* stmt) {
  stmt_ = new Block(stmt,
                    DeepCopyStmtList(stmt->statements()));
}


void CopyAstVisitor::VisitExpressionStatement(
    ExpressionStatement* stmt) {
  stmt_ = new ExpressionStatement(stmt, DeepCopyExpr(stmt->expression()));
}


void CopyAstVisitor::VisitEmptyStatement(EmptyStatement* stmt) {
  stmt_ = new EmptyStatement(stmt);
}


void CopyAstVisitor::VisitIfStatement(IfStatement* stmt) {
  stmt_ = new IfStatement(stmt,
                          DeepCopyExpr(stmt->condition()),
                          DeepCopyStmt(stmt->then_statement()),
                          DeepCopyStmt(stmt->else_statement()));
}


void CopyAstVisitor::VisitContinueStatement(ContinueStatement* stmt) {
  SetStackOverflow();
}


void CopyAstVisitor::VisitBreakStatement(BreakStatement* stmt) {
  SetStackOverflow();
}


void CopyAstVisitor::VisitReturnStatement(ReturnStatement* stmt) {
  SetStackOverflow();
}


void CopyAstVisitor::VisitWithEnterStatement(
    WithEnterStatement* stmt) {
  SetStackOverflow();
}


void CopyAstVisitor::VisitWithExitStatement(WithExitStatement* stmt) {
  SetStackOverflow();
}


void CopyAstVisitor::VisitSwitchStatement(SwitchStatement* stmt) {
  SetStackOverflow();
}


void CopyAstVisitor::VisitDoWhileStatement(DoWhileStatement* stmt) {
  SetStackOverflow();
}


void CopyAstVisitor::VisitWhileStatement(WhileStatement* stmt) {
  SetStackOverflow();
}


void CopyAstVisitor::VisitForStatement(ForStatement* stmt) {
  stmt_ = new ForStatement(stmt,
                           DeepCopyStmt(stmt->init()),
                           DeepCopyExpr(stmt->cond()),
                           DeepCopyStmt(stmt->next()),
                           DeepCopyStmt(stmt->body()));
}


void CopyAstVisitor::VisitForInStatement(ForInStatement* stmt) {
  SetStackOverflow();
}


void CopyAstVisitor::VisitTryCatchStatement(TryCatchStatement* stmt) {
  SetStackOverflow();
}


void CopyAstVisitor::VisitTryFinallyStatement(
    TryFinallyStatement* stmt) {
  SetStackOverflow();
}


void CopyAstVisitor::VisitDebuggerStatement(
    DebuggerStatement* stmt) {
  SetStackOverflow();
}


void CopyAstVisitor::VisitFunctionLiteral(FunctionLiteral* expr) {
  SetStackOverflow();
}


void CopyAstVisitor::VisitSharedFunctionInfoLiteral(
    SharedFunctionInfoLiteral* expr) {
  SetStackOverflow();
}


void CopyAstVisitor::VisitConditional(Conditional* expr) {
  SetStackOverflow();
}


void CopyAstVisitor::VisitSlot(Slot* expr) {
  UNREACHABLE();
}


void CopyAstVisitor::VisitVariableProxy(VariableProxy* expr) {
  expr_ = new VariableProxy(*expr);
}


void CopyAstVisitor::VisitLiteral(Literal* expr) {
  expr_ = new Literal(*expr);
}


void CopyAstVisitor::VisitRegExpLiteral(RegExpLiteral* expr) {
  SetStackOverflow();
}


void CopyAstVisitor::VisitObjectLiteral(ObjectLiteral* expr) {
  SetStackOverflow();
}


void CopyAstVisitor::VisitArrayLiteral(ArrayLiteral* expr) {
  SetStackOverflow();
}


void CopyAstVisitor::VisitCatchExtensionObject(
    CatchExtensionObject* expr) {
  SetStackOverflow();
}


void CopyAstVisitor::VisitAssignment(Assignment* expr) {
  expr_ = new Assignment(expr,
                         DeepCopyExpr(expr->target()),
                         DeepCopyExpr(expr->value()));
}


void CopyAstVisitor::VisitThrow(Throw* expr) {
  SetStackOverflow();
}


void CopyAstVisitor::VisitProperty(Property* expr) {
  expr_ = new Property(expr,
                       DeepCopyExpr(expr->obj()),
                       DeepCopyExpr(expr->key()));
}


void CopyAstVisitor::VisitCall(Call* expr) {
  expr_ = new Call(expr,
                   DeepCopyExpr(expr->expression()),
                   DeepCopyExprList(expr->arguments()));
}


void CopyAstVisitor::VisitCallNew(CallNew* expr) {
  SetStackOverflow();
}


void CopyAstVisitor::VisitCallRuntime(CallRuntime* expr) {
  SetStackOverflow();
}


void CopyAstVisitor::VisitUnaryOperation(UnaryOperation* expr) {
  expr_ = new UnaryOperation(expr, DeepCopyExpr(expr->expression()));
}


void CopyAstVisitor::VisitCountOperation(CountOperation* expr) {
  expr_ = new CountOperation(expr,
                             DeepCopyExpr(expr->expression()));
}


void CopyAstVisitor::VisitBinaryOperation(BinaryOperation* expr) {
  expr_ = new BinaryOperation(expr,
                              expr->op(),
                              DeepCopyExpr(expr->left()),
                              DeepCopyExpr(expr->right()));
}


void CopyAstVisitor::VisitCompareOperation(CompareOperation* expr) {
  expr_ = new CompareOperation(expr,
                               DeepCopyExpr(expr->left()),
                               DeepCopyExpr(expr->right()));
}


void CopyAstVisitor::VisitThisFunction(ThisFunction* expr) {
  SetStackOverflow();
}


void CopyAstVisitor::VisitDeclaration(Declaration* decl) {
  UNREACHABLE();
}


} }  // namespace v8::internal
