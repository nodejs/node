// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/redline/fast/printer.h"

#include <cmath>
#include <cstring>

#include "experimental/redline/base/log.h"
#include "experimental/redline/fast/lexer.h"
#include "src/base/vector.h"
#include "src/numbers/conversions.h"

namespace redline {
namespace fast {

namespace {

// Expression precedence levels used by the printer (higher binds tighter).
enum Precedence : int {
  kPrecSequence = 0,
  kPrecAssign = 1,  // assignment, arrow, yield
  kPrecConditional = 2,
  kPrecNullish = 3,
  kPrecOr = 4,
  kPrecAnd = 5,
  kPrecBitOr = 6,
  kPrecBitXor = 7,
  kPrecBitAnd = 8,
  kPrecEquality = 9,
  kPrecRelational = 10,
  kPrecShift = 11,
  kPrecAdditive = 12,
  kPrecMultiplicative = 13,
  kPrecExponent = 14,
  kPrecUnary = 15,
  kPrecPostfix = 16,
  kPrecCall = 18,    // call, optional chain
  kPrecMember = 19,  // member, new with arguments, tagged template
  kPrecPrimary = 20,
};

int BinaryPrec(BinaryOp op) { return 2 + BinaryPrecedence(op); }
int LogicalPrec(LogicalOp op) { return 2 + LogicalPrecedence(op); }

bool IsAllDigits(std::string_view text) {
  if (text.empty()) {
    return false;
  }
  for (char c : text) {
    if (!(c >= '0' && c <= '9') && c != '_') {
      return false;
    }
  }
  return true;
}

class Printer {
 public:
  Printer(const PrintOptions& options, std::vector<NodeSpan>* spans)
      : options_(options), spans_(spans) {}

  std::string Finish() { return std::move(out_); }

  void PrintNode(const Node* node) {
    if (node->IsExpression()) {
      PrintExpression(node->Cast<Expression>(), kPrecSequence);
    } else if (node->IsStatement()) {
      PrintStatement(node->Cast<Statement>());
    } else {
      switch (node->kind) {
        case NodeKind::kProgram:
          PrintProgram(node->Cast<Program>());
          break;
        case NodeKind::kVariableDeclarator:
          PrintDeclarator(node->Cast<VariableDeclarator>());
          break;
        case NodeKind::kSwitchCase:
          PrintSwitchCase(node->Cast<SwitchCase>());
          break;
        case NodeKind::kCatchClause:
          PrintCatchClause(node->Cast<CatchClause>());
          break;
        case NodeKind::kProperty:
          PrintProperty(node->Cast<Property>());
          break;
        case NodeKind::kClassBody:
          PrintClassBody(node->Cast<ClassBody>());
          break;
        case NodeKind::kMethodDefinition:
        case NodeKind::kPropertyDefinition:
        case NodeKind::kStaticBlock:
          PrintClassMember(node);
          break;
        case NodeKind::kTemplateElement:
          Emit(node->Cast<TemplateElement>()->raw);
          break;
        default:
          Emit("/* ? */");
          break;
      }
    }
  }

 private:
  struct SpanScope {
    SpanScope(Printer* printer, const Node* node)
        : printer_(printer),
          node_(node),
          start_(static_cast<int32_t>(printer->out_.size())) {}
    ~SpanScope() {
      if (printer_->spans_ != nullptr) {
        printer_->spans_->push_back(
            {node_->id, start_, static_cast<int32_t>(printer_->out_.size())});
      }
    }
    Printer* printer_;
    const Node* node_;
    int32_t start_;
  };

  // Clears the "no `in`" restriction inside brackets/braces/function bodies.
  struct AllowIn {
    explicit AllowIn(Printer* printer)
        : printer_(printer), saved_(printer->no_in_) {
      printer_->no_in_ = false;
    }
    ~AllowIn() { printer_->no_in_ = saved_; }
    Printer* printer_;
    bool saved_;
  };

  void Emit(std::string_view text) { out_.append(text); }
  void Emit(char c) { out_.push_back(c); }

  void Newline() {
    if (!options_.pretty) {
      Emit(' ');
      return;
    }
    Emit('\n');
    for (int i = 0; i < indent_; i++) {
      Emit("  ");
    }
  }

  // With PrintOptions::node_ids, simple statements get a trailing
  // `// nN` comment and compound statements (blocks, loops, functions, ...)
  // a leading `/*nN*/` marker, so that every statement id is visible.
  static bool IsSimpleStatement(NodeKind kind) {
    switch (kind) {
      case NodeKind::kExpressionStatement:
      case NodeKind::kVariableDeclaration:
      case NodeKind::kReturnStatement:
      case NodeKind::kThrowStatement:
      case NodeKind::kBreakStatement:
      case NodeKind::kContinueStatement:
      case NodeKind::kEmptyStatement:
      case NodeKind::kDebuggerStatement:
        return true;
      default:
        return false;
    }
  }
  void EmitLeadingNodeId(const Node* node) {
    if (options_.node_ids && !IsSimpleStatement(node->kind)) {
      Emit(StringPrintf("/*n%u*/ ", node->id));
    }
  }
  void EmitTrailingNodeId(const Node* node) {
    if (options_.node_ids && IsSimpleStatement(node->kind)) {
      Emit(StringPrintf("  // n%u", node->id));
    }
  }

  // --- program & statements ---

  void PrintProgram(const Program* program) {
    SpanScope span(this, program);
    for (size_t i = 0; i < program->body.size(); i++) {
      if (i > 0) {
        Newline();
      }
      PrintStatement(program->body[i]);
    }
    if (options_.pretty && !program->body.empty()) {
      Emit('\n');
    }
  }

  void PrintStatementList(const std::vector<Statement*>& statements) {
    indent_++;
    for (const Statement* statement : statements) {
      Newline();
      PrintStatement(statement);
    }
    indent_--;
  }

  void PrintBlock(const BlockStatement* block) {
    SpanScope span(this, block);
    AllowIn allow_in(this);
    Emit('{');
    if (block->body.empty()) {
      Emit('}');
      return;
    }
    PrintStatementList(block->body);
    Newline();
    Emit('}');
  }

  // Prints the body of if/loops/with/labeled statements.
  void PrintBody(const Statement* body) {
    if (body->Is<BlockStatement>()) {
      Emit(' ');
      PrintBlock(body->Cast<BlockStatement>());
      return;
    }
    if (body->Is<EmptyStatement>()) {
      Emit(';');
      return;
    }
    indent_++;
    Newline();
    PrintStatement(body);
    indent_--;
  }

  // True if `statement` ends with an `if` without `else` (so that an `else`
  // following it would bind to the wrong `if`).
  static bool EndsWithDanglingIf(const Statement* statement) {
    while (statement != nullptr) {
      switch (statement->kind) {
        case NodeKind::kIfStatement: {
          auto* if_statement = statement->Cast<IfStatement>();
          if (if_statement->alternate == nullptr) {
            return true;
          }
          statement = if_statement->alternate;
          break;
        }
        case NodeKind::kLabeledStatement:
          statement = statement->Cast<LabeledStatement>()->body;
          break;
        case NodeKind::kWithStatement:
          statement = statement->Cast<WithStatement>()->body;
          break;
        case NodeKind::kWhileStatement:
          statement = statement->Cast<WhileStatement>()->body;
          break;
        case NodeKind::kForStatement:
          statement = statement->Cast<ForStatement>()->body;
          break;
        case NodeKind::kForInStatement:
          statement = statement->Cast<ForInStatement>()->body;
          break;
        case NodeKind::kForOfStatement:
          statement = statement->Cast<ForOfStatement>()->body;
          break;
        default:
          return false;
      }
    }
    return false;
  }

  void PrintStatement(const Statement* statement) {
    SpanScope span(this, statement);
    EmitLeadingNodeId(statement);
    switch (statement->kind) {
      case NodeKind::kExpressionStatement: {
        auto* s = statement->Cast<ExpressionStatement>();
        if (s->is_directive && s->expression->Is<StringLiteral>() &&
            !s->expression->Cast<StringLiteral>()->raw.empty()) {
          Emit(s->expression->Cast<StringLiteral>()->raw);
        } else if (StartsWithForbiddenToken(s->expression)) {
          Emit('(');
          {
            AllowIn allow_in(this);
            PrintExpression(s->expression, kPrecSequence);
          }
          Emit(')');
        } else {
          PrintExpression(s->expression, kPrecSequence);
        }
        Emit(';');
        break;
      }
      case NodeKind::kBlockStatement:
        PrintBlock(statement->Cast<BlockStatement>());
        break;
      case NodeKind::kEmptyStatement:
        Emit(';');
        break;
      case NodeKind::kDebuggerStatement:
        Emit("debugger;");
        break;
      case NodeKind::kWithStatement: {
        auto* s = statement->Cast<WithStatement>();
        Emit("with (");
        PrintExpression(s->object, kPrecSequence);
        Emit(')');
        PrintBody(s->body);
        break;
      }
      case NodeKind::kReturnStatement: {
        auto* s = statement->Cast<ReturnStatement>();
        Emit("return");
        if (s->argument != nullptr) {
          Emit(' ');
          PrintExpression(s->argument, kPrecSequence);
        }
        Emit(';');
        break;
      }
      case NodeKind::kLabeledStatement: {
        auto* s = statement->Cast<LabeledStatement>();
        Emit(s->label->name);
        Emit(':');
        if (s->body->Is<BlockStatement>()) {
          Emit(' ');
          PrintBlock(s->body->Cast<BlockStatement>());
        } else {
          Emit(' ');
          PrintStatement(s->body);
        }
        break;
      }
      case NodeKind::kBreakStatement: {
        auto* s = statement->Cast<BreakStatement>();
        Emit("break");
        if (s->label != nullptr) {
          Emit(' ');
          Emit(s->label->name);
        }
        Emit(';');
        break;
      }
      case NodeKind::kContinueStatement: {
        auto* s = statement->Cast<ContinueStatement>();
        Emit("continue");
        if (s->label != nullptr) {
          Emit(' ');
          Emit(s->label->name);
        }
        Emit(';');
        break;
      }
      case NodeKind::kIfStatement: {
        auto* s = statement->Cast<IfStatement>();
        Emit("if (");
        PrintExpression(s->test, kPrecSequence);
        Emit(')');
        if (s->alternate != nullptr && !s->consequent->Is<BlockStatement>() &&
            EndsWithDanglingIf(s->consequent)) {
          Emit(" {");
          indent_++;
          Newline();
          PrintStatement(s->consequent);
          indent_--;
          Newline();
          Emit('}');
        } else {
          PrintBody(s->consequent);
        }
        if (s->alternate != nullptr) {
          if (s->consequent->Is<BlockStatement>()) {
            Emit(' ');
          } else {
            Newline();
          }
          Emit("else");
          if (s->alternate->Is<IfStatement>()) {
            Emit(' ');
            PrintStatement(s->alternate);
          } else {
            PrintBody(s->alternate);
          }
        }
        break;
      }
      case NodeKind::kSwitchStatement: {
        auto* s = statement->Cast<SwitchStatement>();
        Emit("switch (");
        PrintExpression(s->discriminant, kPrecSequence);
        Emit(") {");
        indent_++;
        for (const SwitchCase* switch_case : s->cases) {
          Newline();
          PrintSwitchCase(switch_case);
        }
        indent_--;
        Newline();
        Emit('}');
        break;
      }
      case NodeKind::kThrowStatement: {
        auto* s = statement->Cast<ThrowStatement>();
        Emit("throw ");
        PrintExpression(s->argument, kPrecSequence);
        Emit(';');
        break;
      }
      case NodeKind::kTryStatement: {
        auto* s = statement->Cast<TryStatement>();
        Emit("try ");
        PrintBlock(s->block);
        if (s->handler != nullptr) {
          Emit(' ');
          PrintCatchClause(s->handler);
        }
        if (s->finalizer != nullptr) {
          Emit(" finally ");
          PrintBlock(s->finalizer);
        }
        break;
      }
      case NodeKind::kWhileStatement: {
        auto* s = statement->Cast<WhileStatement>();
        Emit("while (");
        PrintExpression(s->test, kPrecSequence);
        Emit(')');
        PrintBody(s->body);
        break;
      }
      case NodeKind::kDoWhileStatement: {
        auto* s = statement->Cast<DoWhileStatement>();
        Emit("do");
        PrintBody(s->body);
        if (s->body->Is<BlockStatement>()) {
          Emit(' ');
        } else {
          Newline();
        }
        Emit("while (");
        PrintExpression(s->test, kPrecSequence);
        Emit(");");
        break;
      }
      case NodeKind::kForStatement: {
        auto* s = statement->Cast<ForStatement>();
        Emit("for (");
        if (s->init != nullptr) {
          bool saved = no_in_;
          no_in_ = true;
          if (s->init->Is<VariableDeclaration>()) {
            PrintVariableDeclaration(s->init->Cast<VariableDeclaration>(),
                                     false);
          } else {
            PrintExpression(s->init->Cast<Expression>(), kPrecSequence);
          }
          no_in_ = saved;
        }
        Emit(';');
        if (s->test != nullptr) {
          Emit(' ');
          PrintExpression(s->test, kPrecSequence);
        }
        Emit(';');
        if (s->update != nullptr) {
          Emit(' ');
          PrintExpression(s->update, kPrecSequence);
        }
        Emit(')');
        PrintBody(s->body);
        break;
      }
      case NodeKind::kForInStatement: {
        auto* s = statement->Cast<ForInStatement>();
        Emit("for (");
        PrintForEachLeft(s->left, false);
        Emit(" in ");
        PrintExpression(s->right, kPrecSequence);
        Emit(')');
        PrintBody(s->body);
        break;
      }
      case NodeKind::kForOfStatement: {
        auto* s = statement->Cast<ForOfStatement>();
        Emit(s->is_await ? "for await (" : "for (");
        PrintForEachLeft(s->left, true);
        Emit(" of ");
        PrintExpression(s->right, kPrecAssign);
        Emit(')');
        PrintBody(s->body);
        break;
      }
      case NodeKind::kFunctionDeclaration:
        PrintFunction(statement->Cast<FunctionDeclaration>(), statement);
        break;
      case NodeKind::kVariableDeclaration:
        PrintVariableDeclaration(statement->Cast<VariableDeclaration>(), true);
        break;
      case NodeKind::kClassDeclaration:
        PrintClass(statement->Cast<ClassDeclaration>(), statement);
        break;
      default:
        Emit("/* unknown statement */;");
        break;
    }
    EmitTrailingNodeId(statement);
  }

  void PrintForEachLeft(const Node* left, bool for_of) {
    bool saved = no_in_;
    no_in_ = true;
    if (left->Is<VariableDeclaration>()) {
      PrintVariableDeclaration(left->Cast<VariableDeclaration>(), false);
    } else {
      // `for (async of ...)` is forbidden, but `for ((async) of ...)` is
      // fine; the caller tells us whether this is a for-of head.
      auto* expression = left->Cast<Expression>();
      bool is_async_identifier =
          expression->Is<Identifier>() &&
          expression->Cast<Identifier>()->name == "async";
      if (is_async_identifier && for_of) {
        Emit('(');
        PrintExpression(expression, kPrecSequence);
        Emit(')');
      } else {
        PrintExpression(expression, kPrecCall);
      }
    }
    no_in_ = saved;
  }

  void PrintSwitchCase(const SwitchCase* switch_case) {
    SpanScope span(this, switch_case);
    if (switch_case->test != nullptr) {
      Emit("case ");
      PrintExpression(switch_case->test, kPrecSequence);
      Emit(':');
    } else {
      Emit("default:");
    }
    PrintStatementList(switch_case->consequent);
  }

  void PrintCatchClause(const CatchClause* handler) {
    SpanScope span(this, handler);
    Emit("catch");
    if (handler->param != nullptr) {
      Emit(" (");
      PrintPattern(handler->param);
      Emit(')');
    }
    Emit(' ');
    PrintBlock(handler->body);
  }

  void PrintVariableDeclaration(const VariableDeclaration* declaration,
                                bool with_semicolon) {
    SpanScope span(this, declaration);
    Emit(VariableKindText(declaration->declaration_kind));
    Emit(' ');
    for (size_t i = 0; i < declaration->declarations.size(); i++) {
      if (i > 0) {
        Emit(", ");
      }
      PrintDeclarator(declaration->declarations[i]);
    }
    if (with_semicolon) {
      Emit(';');
    }
  }

  void PrintDeclarator(const VariableDeclarator* declarator) {
    SpanScope span(this, declarator);
    PrintPattern(declarator->target);
    if (declarator->init != nullptr) {
      Emit(" = ");
      PrintExpression(declarator->init, kPrecAssign);
    }
  }

  // --- functions & classes ---

  void PrintParams(const std::vector<Expression*>& params) {
    AllowIn allow_in(this);
    Emit('(');
    for (size_t i = 0; i < params.size(); i++) {
      if (i > 0) {
        Emit(", ");
      }
      PrintPattern(params[i]);
    }
    Emit(')');
  }

  void PrintFunctionBody(const BlockStatement* body) {
    AllowIn allow_in(this);
    PrintBlock(body);
  }

  void PrintFunction(const FunctionData* fn, const Node* node) {
    SpanScope span(this, node);
    if (fn->is_async) {
      Emit("async ");
    }
    Emit("function");
    if (fn->is_generator) {
      Emit('*');
    }
    if (fn->name != nullptr) {
      Emit(' ');
      Emit(fn->name->name);
    }
    PrintParams(fn->params);
    Emit(' ');
    PrintFunctionBody(fn->body);
  }

  void PrintArrow(const ArrowFunctionExpression* arrow) {
    if (arrow->is_async) {
      Emit("async ");
    }
    PrintParams(arrow->params);
    Emit(" => ");
    if (arrow->body != nullptr) {
      PrintFunctionBody(arrow->body);
      return;
    }
    const Expression* body = arrow->expression_body;
    if (body->Is<ObjectExpression>() || StartsWithObjectLiteral(body)) {
      Emit('(');
      AllowIn allow_in(this);
      PrintExpression(body, kPrecSequence);
      Emit(')');
    } else {
      PrintExpression(body, kPrecAssign);
    }
  }

  void PrintClass(const ClassData* cls, const Node* node) {
    SpanScope span(this, node);
    AllowIn allow_in(this);
    Emit("class");
    if (cls->name != nullptr) {
      Emit(' ');
      Emit(cls->name->name);
    }
    if (cls->super_class != nullptr) {
      Emit(" extends ");
      PrintExpression(cls->super_class, kPrecCall);
    }
    Emit(' ');
    PrintClassBody(cls->body);
  }

  void PrintClassBody(const ClassBody* body) {
    SpanScope span(this, body);
    Emit('{');
    if (body->members.empty()) {
      Emit('}');
      return;
    }
    indent_++;
    for (const Node* member : body->members) {
      Newline();
      PrintClassMember(member);
    }
    indent_--;
    Newline();
    Emit('}');
  }

  void PrintPropertyKey(const Expression* key, bool computed) {
    if (computed) {
      Emit('[');
      AllowIn allow_in(this);
      PrintExpression(key, kPrecAssign);
      Emit(']');
      return;
    }
    switch (key->kind) {
      case NodeKind::kIdentifier:
        Emit(key->Cast<Identifier>()->name);
        break;
      case NodeKind::kPrivateIdentifier:
        Emit('#');
        Emit(key->Cast<PrivateIdentifier>()->name);
        break;
      case NodeKind::kStringLiteral:
        PrintStringLiteral(key->Cast<StringLiteral>());
        break;
      case NodeKind::kNumericLiteral:
        PrintNumericLiteral(key->Cast<NumericLiteral>(), false);
        break;
      case NodeKind::kBigIntLiteral:
        Emit(key->Cast<BigIntLiteral>()->raw);
        break;
      default:
        // Not a valid non-computed key; print computed to stay parseable.
        Emit('[');
        PrintExpression(key, kPrecAssign);
        Emit(']');
        break;
    }
  }

  void PrintMethodSignatureAndBody(const FunctionExpression* fn) {
    PrintParams(fn->params);
    Emit(' ');
    PrintFunctionBody(fn->body);
  }

  void PrintClassMember(const Node* member) {
    SpanScope span(this, member);
    switch (member->kind) {
      case NodeKind::kMethodDefinition: {
        auto* m = member->Cast<MethodDefinition>();
        if (m->is_static) {
          Emit("static ");
        }
        if (m->method_kind == MethodKind::kGet) {
          Emit("get ");
        }
        if (m->method_kind == MethodKind::kSet) {
          Emit("set ");
        }
        if (m->value->is_async) {
          Emit("async ");
        }
        if (m->value->is_generator) {
          Emit('*');
        }
        PrintPropertyKey(m->key, m->computed);
        PrintMethodSignatureAndBody(m->value);
        break;
      }
      case NodeKind::kPropertyDefinition: {
        auto* p = member->Cast<PropertyDefinition>();
        if (p->is_static) {
          Emit("static ");
        }
        if (p->is_auto_accessor) {
          Emit("accessor ");
        }
        PrintPropertyKey(p->key, p->computed);
        if (p->value != nullptr) {
          Emit(" = ");
          AllowIn allow_in(this);
          PrintExpression(p->value, kPrecAssign);
        }
        Emit(';');
        break;
      }
      case NodeKind::kStaticBlock: {
        auto* block = member->Cast<StaticBlock>();
        Emit("static {");
        PrintStatementList(block->body);
        Newline();
        Emit('}');
        break;
      }
      default:
        Emit("/* ? */;");
        break;
    }
  }

  // --- object literal properties and patterns ---

  void PrintProperty(const Property* property) {
    SpanScope span(this, property);
    if (property->property_kind == PropertyKind::kGet ||
        property->property_kind == PropertyKind::kSet || property->method) {
      auto* fn = property->value->Cast<FunctionExpression>();
      if (property->property_kind == PropertyKind::kGet) {
        Emit("get ");
      }
      if (property->property_kind == PropertyKind::kSet) {
        Emit("set ");
      }
      if (fn->is_async) {
        Emit("async ");
      }
      if (fn->is_generator) {
        Emit('*');
      }
      PrintPropertyKey(property->key, property->computed);
      PrintMethodSignatureAndBody(fn);
      return;
    }
    if (property->shorthand && !property->computed &&
        property->key->Is<Identifier>()) {
      // `{a}` or `{a = 1}` (in patterns).
      if (auto* with_default = property->value->As<AssignmentPattern>()) {
        if (auto* id = with_default->left->As<Identifier>()) {
          if (id->name == property->key->Cast<Identifier>()->name) {
            Emit(id->name);
            Emit(" = ");
            PrintExpression(with_default->right, kPrecAssign);
            return;
          }
        }
      } else if (auto* id = property->value->As<Identifier>()) {
        if (id->name == property->key->Cast<Identifier>()->name) {
          Emit(id->name);
          return;
        }
      }
    }
    PrintPropertyKey(property->key, property->computed);
    Emit(": ");
    PrintPattern(property->value);
  }

  // Prints a binding/assignment target (patterns or plain expressions).
  void PrintPattern(const Expression* pattern) {
    switch (pattern->kind) {
      case NodeKind::kObjectPattern: {
        SpanScope span(this, pattern);
        AllowIn allow_in(this);
        auto* p = pattern->Cast<ObjectPattern>();
        Emit('{');
        for (size_t i = 0; i < p->properties.size(); i++) {
          if (i > 0) {
            Emit(", ");
          }
          const Node* entry = p->properties[i];
          if (auto* rest = entry->As<RestElement>()) {
            SpanScope rest_span(this, rest);
            Emit("...");
            PrintPattern(rest->argument);
          } else {
            PrintProperty(entry->Cast<Property>());
          }
        }
        Emit('}');
        break;
      }
      case NodeKind::kArrayPattern: {
        SpanScope span(this, pattern);
        AllowIn allow_in(this);
        auto* p = pattern->Cast<ArrayPattern>();
        Emit('[');
        for (size_t i = 0; i < p->elements.size(); i++) {
          if (i > 0) {
            Emit(", ");
          }
          const Expression* element = p->elements[i];
          if (element == nullptr) {
            if (i + 1 == p->elements.size()) {
              Emit(',');
            }
            continue;
          }
          PrintPattern(element);
        }
        Emit(']');
        break;
      }
      case NodeKind::kRestElement: {
        SpanScope span(this, pattern);
        Emit("...");
        PrintPattern(pattern->Cast<RestElement>()->argument);
        break;
      }
      case NodeKind::kAssignmentPattern: {
        SpanScope span(this, pattern);
        auto* p = pattern->Cast<AssignmentPattern>();
        PrintPattern(p->left);
        Emit(" = ");
        PrintExpression(p->right, kPrecAssign);
        break;
      }
      default:
        PrintExpression(pattern, kPrecAssign);
        break;
    }
  }

  // --- expressions ---

  static int PrecedenceOf(const Expression* e) {
    switch (e->kind) {
      case NodeKind::kSequenceExpression:
        return kPrecSequence;
      case NodeKind::kAssignmentExpression:
      case NodeKind::kArrowFunctionExpression:
      case NodeKind::kYieldExpression:
        return kPrecAssign;
      case NodeKind::kConditionalExpression:
        return kPrecConditional;
      case NodeKind::kLogicalExpression:
        return LogicalPrec(e->Cast<LogicalExpression>()->op);
      case NodeKind::kBinaryExpression:
        return BinaryPrec(e->Cast<BinaryExpression>()->op);
      case NodeKind::kUnaryExpression:
      case NodeKind::kAwaitExpression:
        return kPrecUnary;
      case NodeKind::kUpdateExpression:
        return e->Cast<UpdateExpression>()->prefix ? kPrecUnary : kPrecPostfix;
      case NodeKind::kCallExpression:
      case NodeKind::kChainExpression:
      case NodeKind::kImportExpression:
        return kPrecCall;
      case NodeKind::kMemberExpression:
      case NodeKind::kNewExpression:
      case NodeKind::kTaggedTemplateExpression:
      case NodeKind::kMetaProperty:
        return kPrecMember;
      case NodeKind::kNumericLiteral:
        return e->Cast<NumericLiteral>()->value < 0 ||
                       std::signbit(e->Cast<NumericLiteral>()->value)
                   ? kPrecUnary
                   : kPrecPrimary;
      default:
        return kPrecPrimary;
    }
  }

  // Expression statements may not start with `{`, `function`, `class`,
  // `async function` or `let [`.
  static bool StartsWithForbiddenToken(const Expression* e) {
    const Expression* parent = nullptr;
    while (true) {
      switch (e->kind) {
        case NodeKind::kObjectExpression:
        case NodeKind::kObjectPattern:
        case NodeKind::kFunctionExpression:
        case NodeKind::kClassExpression:
          return true;
        case NodeKind::kIdentifier:
          return e->Cast<Identifier>()->name == "let" && parent != nullptr &&
                 parent->Is<MemberExpression>() &&
                 parent->Cast<MemberExpression>()->computed;
        case NodeKind::kSequenceExpression:
          parent = e;
          e = e->Cast<SequenceExpression>()->expressions.front();
          break;
        case NodeKind::kAssignmentExpression:
          parent = e;
          e = e->Cast<AssignmentExpression>()->left;
          break;
        case NodeKind::kConditionalExpression:
          parent = e;
          e = e->Cast<ConditionalExpression>()->test;
          break;
        case NodeKind::kBinaryExpression:
          parent = e;
          e = e->Cast<BinaryExpression>()->left;
          break;
        case NodeKind::kLogicalExpression:
          parent = e;
          e = e->Cast<LogicalExpression>()->left;
          break;
        case NodeKind::kCallExpression:
          parent = e;
          e = e->Cast<CallExpression>()->callee;
          break;
        case NodeKind::kMemberExpression:
          parent = e;
          e = e->Cast<MemberExpression>()->object;
          break;
        case NodeKind::kTaggedTemplateExpression:
          parent = e;
          e = e->Cast<TaggedTemplateExpression>()->tag;
          break;
        case NodeKind::kChainExpression:
          parent = e;
          e = e->Cast<ChainExpression>()->expression;
          break;
        case NodeKind::kUpdateExpression:
          if (e->Cast<UpdateExpression>()->prefix) {
            return false;
          }
          parent = e;
          e = e->Cast<UpdateExpression>()->argument;
          break;
        default:
          return false;
      }
    }
  }

  static bool StartsWithObjectLiteral(const Expression* e) {
    while (true) {
      switch (e->kind) {
        case NodeKind::kObjectExpression:
        case NodeKind::kObjectPattern:
          return true;
        case NodeKind::kSequenceExpression:
          e = e->Cast<SequenceExpression>()->expressions.front();
          break;
        case NodeKind::kAssignmentExpression:
          e = e->Cast<AssignmentExpression>()->left;
          break;
        case NodeKind::kConditionalExpression:
          e = e->Cast<ConditionalExpression>()->test;
          break;
        case NodeKind::kBinaryExpression:
          e = e->Cast<BinaryExpression>()->left;
          break;
        case NodeKind::kLogicalExpression:
          e = e->Cast<LogicalExpression>()->left;
          break;
        case NodeKind::kCallExpression:
          e = e->Cast<CallExpression>()->callee;
          break;
        case NodeKind::kMemberExpression:
          e = e->Cast<MemberExpression>()->object;
          break;
        case NodeKind::kTaggedTemplateExpression:
          e = e->Cast<TaggedTemplateExpression>()->tag;
          break;
        case NodeKind::kChainExpression:
          e = e->Cast<ChainExpression>()->expression;
          break;
        case NodeKind::kUpdateExpression:
          if (e->Cast<UpdateExpression>()->prefix) {
            return false;
          }
          e = e->Cast<UpdateExpression>()->argument;
          break;
        default:
          return false;
      }
    }
  }

  // True if the member/call spine of a `new` callee contains a call, which
  // would otherwise capture the `new` arguments.
  static bool NewCalleeNeedsParens(const Expression* e) {
    while (true) {
      switch (e->kind) {
        case NodeKind::kCallExpression:
        case NodeKind::kChainExpression:
        case NodeKind::kImportExpression:
          return true;
        case NodeKind::kMemberExpression:
          e = e->Cast<MemberExpression>()->object;
          break;
        case NodeKind::kTaggedTemplateExpression:
          e = e->Cast<TaggedTemplateExpression>()->tag;
          break;
        default:
          return false;
      }
    }
  }

  void PrintParenthesized(const Expression* e) {
    Emit('(');
    AllowIn allow_in(this);
    PrintExpression(e, kPrecSequence);
    Emit(')');
  }

  void PrintExpression(const Expression* e, int min_precedence) {
    SpanScope span(this, e);
    if (PrecedenceOf(e) < min_precedence) {
      PrintParenthesized(e);
      return;
    }
    switch (e->kind) {
      case NodeKind::kIdentifier:
        Emit(e->Cast<Identifier>()->name);
        break;
      case NodeKind::kPrivateIdentifier:
        Emit('#');
        Emit(e->Cast<PrivateIdentifier>()->name);
        break;
      case NodeKind::kNullLiteral:
        Emit("null");
        break;
      case NodeKind::kBooleanLiteral:
        Emit(e->Cast<BooleanLiteral>()->value ? "true" : "false");
        break;
      case NodeKind::kNumericLiteral:
        PrintNumericLiteral(e->Cast<NumericLiteral>(), true);
        break;
      case NodeKind::kBigIntLiteral:
        Emit(e->Cast<BigIntLiteral>()->raw);
        break;
      case NodeKind::kStringLiteral:
        PrintStringLiteral(e->Cast<StringLiteral>());
        break;
      case NodeKind::kRegExpLiteral: {
        auto* r = e->Cast<RegExpLiteral>();
        Emit('/');
        Emit(r->pattern);
        Emit('/');
        Emit(r->regexp_flags);
        break;
      }
      case NodeKind::kThisExpression:
        Emit("this");
        break;
      case NodeKind::kSuper:
        Emit("super");
        break;
      case NodeKind::kArrayExpression: {
        auto* a = e->Cast<ArrayExpression>();
        AllowIn allow_in(this);
        Emit('[');
        for (size_t i = 0; i < a->elements.size(); i++) {
          if (i > 0) {
            Emit(", ");
          }
          const Expression* element = a->elements[i];
          if (element == nullptr) {
            if (i + 1 == a->elements.size()) {
              Emit(',');
            }
            continue;
          }
          PrintExpression(element, kPrecAssign);
        }
        Emit(']');
        break;
      }
      case NodeKind::kObjectExpression: {
        auto* o = e->Cast<ObjectExpression>();
        AllowIn allow_in(this);
        Emit('{');
        for (size_t i = 0; i < o->properties.size(); i++) {
          if (i > 0) {
            Emit(", ");
          }
          const Node* entry = o->properties[i];
          if (auto* spread = entry->As<SpreadElement>()) {
            SpanScope spread_span(this, spread);
            Emit("...");
            PrintExpression(spread->argument, kPrecAssign);
          } else {
            PrintProperty(entry->Cast<Property>());
          }
        }
        Emit('}');
        break;
      }
      case NodeKind::kFunctionExpression:
        PrintFunction(e->Cast<FunctionExpression>(), e);
        break;
      case NodeKind::kArrowFunctionExpression:
        PrintArrow(e->Cast<ArrowFunctionExpression>());
        break;
      case NodeKind::kClassExpression:
        PrintClass(e->Cast<ClassExpression>(), e);
        break;
      case NodeKind::kUnaryExpression: {
        auto* u = e->Cast<UnaryExpression>();
        const char* op = OperatorText(u->op);
        Emit(op);
        bool keyword = u->op == UnaryOp::kTypeof || u->op == UnaryOp::kVoid ||
                       u->op == UnaryOp::kDelete;
        bool same_sign = false;
        if (auto* inner = u->argument->As<UnaryExpression>()) {
          same_sign =
              (u->op == UnaryOp::kMinus && inner->op == UnaryOp::kMinus) ||
              (u->op == UnaryOp::kPlus && inner->op == UnaryOp::kPlus);
        } else if (auto* update = u->argument->As<UpdateExpression>()) {
          same_sign =
              update->prefix &&
              ((u->op == UnaryOp::kMinus &&
                update->op == UpdateOp::kDecrement) ||
               (u->op == UnaryOp::kPlus && update->op == UpdateOp::kIncrement));
        } else if (auto* number = u->argument->As<NumericLiteral>()) {
          same_sign = u->op == UnaryOp::kMinus && std::signbit(number->value);
        }
        if (keyword || same_sign) {
          Emit(' ');
        }
        PrintExpression(u->argument, kPrecUnary);
        break;
      }
      case NodeKind::kUpdateExpression: {
        auto* u = e->Cast<UpdateExpression>();
        if (u->prefix) {
          Emit(OperatorText(u->op));
          PrintExpression(u->argument, kPrecUnary);
        } else {
          PrintExpression(u->argument, kPrecPostfix);
          Emit(OperatorText(u->op));
        }
        break;
      }
      case NodeKind::kBinaryExpression: {
        auto* b = e->Cast<BinaryExpression>();
        if (b->op == BinaryOp::kIn && no_in_) {
          PrintParenthesized(e);
          break;
        }
        int prec = BinaryPrec(b->op);
        if (b->op == BinaryOp::kExp) {
          // Right-associative; a unary expression on the left is a syntax
          // error without parentheses.
          bool left_parens =
              b->left->Is<UnaryExpression>() || b->left->Is<AwaitExpression>();
          if (left_parens) {
            PrintParenthesized(b->left);
          } else {
            PrintExpression(b->left, prec + 1);
          }
          Emit(" ** ");
          PrintExpression(b->right, prec);
        } else {
          PrintExpression(b->left, prec);
          Emit(' ');
          Emit(OperatorText(b->op));
          Emit(' ');
          PrintExpression(b->right, prec + 1);
        }
        break;
      }
      case NodeKind::kLogicalExpression: {
        auto* l = e->Cast<LogicalExpression>();
        int prec = LogicalPrec(l->op);
        auto needs_mixing_parens = [&](const Expression* child) {
          auto* inner = child->As<LogicalExpression>();
          if (inner == nullptr) {
            return false;
          }
          bool outer_nullish = l->op == LogicalOp::kNullish;
          bool inner_nullish = inner->op == LogicalOp::kNullish;
          return outer_nullish != inner_nullish;
        };
        if (needs_mixing_parens(l->left)) {
          PrintParenthesized(l->left);
        } else {
          PrintExpression(l->left, prec);
        }
        Emit(' ');
        Emit(OperatorText(l->op));
        Emit(' ');
        if (needs_mixing_parens(l->right)) {
          PrintParenthesized(l->right);
        } else {
          PrintExpression(l->right, prec + 1);
        }
        break;
      }
      case NodeKind::kAssignmentExpression: {
        auto* a = e->Cast<AssignmentExpression>();
        PrintPattern(a->left);
        Emit(' ');
        Emit(OperatorText(a->op));
        Emit(' ');
        PrintExpression(a->right, kPrecAssign);
        break;
      }
      case NodeKind::kMemberExpression: {
        auto* m = e->Cast<MemberExpression>();
        PrintMemberObject(m->object);
        if (m->computed) {
          Emit(m->optional ? "?.[" : "[");
          AllowIn allow_in(this);
          PrintExpression(m->property, kPrecSequence);
          Emit(']');
        } else {
          Emit(m->optional ? "?." : ".");
          if (auto* priv = m->property->As<PrivateIdentifier>()) {
            Emit('#');
            Emit(priv->name);
          } else {
            Emit(m->property->Cast<Identifier>()->name);
          }
        }
        break;
      }
      case NodeKind::kConditionalExpression: {
        auto* c = e->Cast<ConditionalExpression>();
        PrintExpression(c->test, kPrecNullish);
        Emit(" ? ");
        {
          AllowIn allow_in(this);
          PrintExpression(c->consequent, kPrecAssign);
        }
        Emit(" : ");
        PrintExpression(c->alternate, kPrecAssign);
        break;
      }
      case NodeKind::kCallExpression: {
        auto* c = e->Cast<CallExpression>();
        if (c->callee->Is<ChainExpression>()) {
          PrintParenthesized(c->callee);
        } else {
          PrintExpression(c->callee, kPrecCall);
        }
        if (c->optional) {
          Emit("?.");
        }
        PrintArguments(c->arguments);
        break;
      }
      case NodeKind::kNewExpression: {
        auto* n = e->Cast<NewExpression>();
        Emit("new ");
        if (NewCalleeNeedsParens(n->callee)) {
          PrintParenthesized(n->callee);
        } else {
          PrintExpression(n->callee, kPrecMember);
        }
        PrintArguments(n->arguments);
        break;
      }
      case NodeKind::kSequenceExpression: {
        auto* s = e->Cast<SequenceExpression>();
        for (size_t i = 0; i < s->expressions.size(); i++) {
          if (i > 0) {
            Emit(", ");
          }
          PrintExpression(s->expressions[i], kPrecAssign);
        }
        break;
      }
      case NodeKind::kYieldExpression: {
        auto* y = e->Cast<YieldExpression>();
        Emit(y->delegate ? "yield*" : "yield");
        if (y->argument != nullptr) {
          Emit(' ');
          PrintExpression(y->argument, kPrecAssign);
        }
        break;
      }
      case NodeKind::kAwaitExpression:
        Emit("await ");
        PrintExpression(e->Cast<AwaitExpression>()->argument, kPrecUnary);
        break;
      case NodeKind::kTemplateLiteral:
        PrintTemplate(e->Cast<TemplateLiteral>());
        break;
      case NodeKind::kTaggedTemplateExpression: {
        auto* t = e->Cast<TaggedTemplateExpression>();
        if (t->tag->Is<ChainExpression>()) {
          PrintParenthesized(t->tag);
        } else {
          PrintExpression(t->tag, kPrecCall);
        }
        PrintTemplate(t->quasi);
        break;
      }
      case NodeKind::kMetaProperty:
        Emit(e->Cast<MetaProperty>()->meta_kind == MetaPropertyKind::kNewTarget
                 ? "new.target"
                 : "import.meta");
        break;
      case NodeKind::kImportExpression: {
        auto* i = e->Cast<ImportExpression>();
        AllowIn allow_in(this);
        Emit(i->is_defer ? "import.defer(" : "import(");
        PrintExpression(i->source, kPrecAssign);
        if (i->options != nullptr) {
          Emit(", ");
          PrintExpression(i->options, kPrecAssign);
        }
        Emit(')');
        break;
      }
      case NodeKind::kChainExpression:
        PrintExpression(e->Cast<ChainExpression>()->expression, kPrecCall);
        break;
      case NodeKind::kSpreadElement:
        Emit("...");
        PrintExpression(e->Cast<SpreadElement>()->argument, kPrecAssign);
        break;
      case NodeKind::kObjectPattern:
      case NodeKind::kArrayPattern:
      case NodeKind::kRestElement:
      case NodeKind::kAssignmentPattern:
        PrintPattern(e);
        break;
      case NodeKind::kNativeCallExpression: {
        auto* n = e->Cast<NativeCallExpression>();
        Emit('%');
        Emit(n->name);
        PrintArguments(n->arguments);
        break;
      }
      default:
        Emit("/* ? */");
        break;
    }
  }

  void PrintMemberObject(const Expression* object) {
    if (object->Is<ChainExpression>()) {
      PrintParenthesized(object);
      return;
    }
    if (auto* number = object->As<NumericLiteral>()) {
      std::string_view raw = number->raw;
      if (raw.empty() || IsAllDigits(raw)) {
        PrintParenthesized(object);
        return;
      }
    }
    PrintExpression(object, kPrecCall);
  }

  void PrintArguments(const std::vector<Expression*>& arguments) {
    AllowIn allow_in(this);
    Emit('(');
    for (size_t i = 0; i < arguments.size(); i++) {
      if (i > 0) {
        Emit(", ");
      }
      PrintExpression(arguments[i], kPrecAssign);
    }
    Emit(')');
  }

  void PrintTemplate(const TemplateLiteral* t) {
    SpanScope span(this, t);
    AllowIn allow_in(this);
    Emit('`');
    for (size_t i = 0; i < t->quasis.size(); i++) {
      Emit(t->quasis[i]->raw);
      if (i < t->expressions.size()) {
        Emit("${");
        PrintExpression(t->expressions[i], kPrecSequence);
        Emit('}');
      }
    }
    Emit('`');
  }

  void PrintStringLiteral(const StringLiteral* s) {
    if (!s->raw.empty()) {
      Emit(s->raw);
    } else {
      Emit(QuoteJsString(s->value));
    }
  }

  void PrintNumericLiteral(const NumericLiteral* n, bool allow_parens) {
    if (!n->raw.empty()) {
      Emit(n->raw);
      return;
    }
    std::string text = NumberToJs(n->value);
    if (text[0] == '-' && allow_parens) {
      Emit('(');
      Emit(text);
      Emit(')');
    } else {
      Emit(text);
    }
  }

  const PrintOptions& options_;
  std::vector<NodeSpan>* spans_;
  std::string out_;
  int indent_ = 0;
  bool no_in_ = false;
};

}  // namespace

std::string QuoteJsString(std::string_view value) {
  std::string out;
  out.reserve(value.size() + 2);
  out.push_back('"');
  size_t pos = 0;
  while (pos < value.size()) {
    size_t length;
    uint32_t c = DecodeUtf8(value, pos, &length);
    pos += length;
    switch (c) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      case '\b':
        out += "\\b";
        break;
      case '\f':
        out += "\\f";
        break;
      case '\v':
        out += "\\v";
        break;
      case 0:
        out += "\\0";
        break;
      case 0x2028:
        out += "\\u2028";
        break;
      case 0x2029:
        out += "\\u2029";
        break;
      default:
        if (c < 0x20 || c == 0x7F) {
          out += StringPrintf("\\x%02X", c);
        } else if (c >= 0xD800 && c <= 0xDFFF) {
          // Lone surrogate (WTF-8): must be escaped.
          out += StringPrintf("\\u%04X", c);
        } else if (c < 0x80) {
          out.push_back(static_cast<char>(c));
        } else if (c == 0xFFFD && length == 1) {
          // Malformed input byte; escape it so the output is valid UTF-8.
          out += StringPrintf(
              "\\x%02X",
              static_cast<unsigned>(static_cast<uint8_t>(value[pos - 1])));
        } else {
          AppendUtf8(&out, c);
        }
        break;
    }
  }
  out.push_back('"');
  return out;
}

std::string NumberToJs(double value) {
  if (std::isnan(value)) {
    return "NaN";
  }
  if (std::isinf(value)) {
    return value < 0 ? "-Infinity" : "Infinity";
  }
  if (value == 0) {
    return std::signbit(value) ? "-0" : "0";
  }
  char buffer[v8::internal::kDoubleToStringMinBufferSize];
  std::string_view text = v8::internal::DoubleToStringView(
      value, v8::base::Vector<char>(buffer, sizeof(buffer)));
  return std::string(text);
}

std::string Print(const Node* node, const PrintOptions& options) {
  return Print(node, options, nullptr);
}

std::string Print(const Node* node, const PrintOptions& options,
                  std::vector<NodeSpan>* spans) {
  Printer printer(options, spans);
  if (node != nullptr) {
    printer.PrintNode(node);
  }
  return printer.Finish();
}

}  // namespace fast
}  // namespace redline
