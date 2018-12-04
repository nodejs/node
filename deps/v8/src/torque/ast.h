// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TORQUE_AST_H_
#define V8_TORQUE_AST_H_

#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "src/base/optional.h"
#include "src/torque/source-positions.h"

namespace v8 {
namespace internal {
namespace torque {

#define AST_EXPRESSION_NODE_KIND_LIST(V) \
  V(CallExpression)                      \
  V(StructExpression)                    \
  V(LogicalOrExpression)                 \
  V(LogicalAndExpression)                \
  V(ConditionalExpression)               \
  V(IdentifierExpression)                \
  V(StringLiteralExpression)             \
  V(NumberLiteralExpression)             \
  V(FieldAccessExpression)               \
  V(ElementAccessExpression)             \
  V(AssignmentExpression)                \
  V(IncrementDecrementExpression)        \
  V(AssumeTypeImpossibleExpression)      \
  V(StatementExpression)                 \
  V(TryLabelExpression)

#define AST_TYPE_EXPRESSION_NODE_KIND_LIST(V) \
  V(BasicTypeExpression)                      \
  V(FunctionTypeExpression)                   \
  V(UnionTypeExpression)

#define AST_STATEMENT_NODE_KIND_LIST(V) \
  V(BlockStatement)                     \
  V(ExpressionStatement)                \
  V(IfStatement)                        \
  V(WhileStatement)                     \
  V(ForLoopStatement)                   \
  V(ForOfLoopStatement)                 \
  V(BreakStatement)                     \
  V(ContinueStatement)                  \
  V(ReturnStatement)                    \
  V(DebugStatement)                     \
  V(AssertStatement)                    \
  V(TailCallStatement)                  \
  V(VarDeclarationStatement)            \
  V(GotoStatement)                      \

#define AST_DECLARATION_NODE_KIND_LIST(V) \
  V(TypeDeclaration)                      \
  V(TypeAliasDeclaration)                 \
  V(StandardDeclaration)                  \
  V(GenericDeclaration)                   \
  V(SpecializationDeclaration)            \
  V(ExternConstDeclaration)               \
  V(StructDeclaration)                    \
  V(DefaultModuleDeclaration)             \
  V(ExplicitModuleDeclaration)            \
  V(ConstDeclaration)

#define AST_CALLABLE_NODE_KIND_LIST(V) \
  V(TorqueMacroDeclaration)            \
  V(TorqueBuiltinDeclaration)          \
  V(ExternalMacroDeclaration)          \
  V(ExternalBuiltinDeclaration)        \
  V(ExternalRuntimeDeclaration)

#define AST_NODE_KIND_LIST(V)           \
  AST_EXPRESSION_NODE_KIND_LIST(V)      \
  AST_TYPE_EXPRESSION_NODE_KIND_LIST(V) \
  AST_STATEMENT_NODE_KIND_LIST(V)       \
  AST_DECLARATION_NODE_KIND_LIST(V)     \
  AST_CALLABLE_NODE_KIND_LIST(V)        \
  V(LabelBlock)

struct AstNode {
 public:
  enum class Kind {
#define ENUM_ITEM(name) k##name,
    AST_NODE_KIND_LIST(ENUM_ITEM)
#undef ENUM_ITEM
  };

  AstNode(Kind kind, SourcePosition pos) : kind(kind), pos(pos) {}
  virtual ~AstNode() = default;

  const Kind kind;
  SourcePosition pos;
};

struct AstNodeClassCheck {
  template <class T>
  static bool IsInstanceOf(AstNode* node);
};

// Boilerplate for most derived classes.
#define DEFINE_AST_NODE_LEAF_BOILERPLATE(T)                        \
  static const Kind kKind = Kind::k##T;                            \
  static T* cast(AstNode* node) {                                  \
    if (node->kind != kKind) return nullptr;                       \
    return static_cast<T*>(node);                                  \
  }                                                                \
  static T* DynamicCast(AstNode* node) {                           \
    if (!node) return nullptr;                                     \
    if (!AstNodeClassCheck::IsInstanceOf<T>(node)) return nullptr; \
    return static_cast<T*>(node);                                  \
  }

// Boilerplate for classes with subclasses.
#define DEFINE_AST_NODE_INNER_BOILERPLATE(T)                       \
  static T* cast(AstNode* node) {                                  \
    DCHECK(AstNodeClassCheck::IsInstanceOf<T>(node));              \
    return static_cast<T*>(node);                                  \
  }                                                                \
  static T* DynamicCast(AstNode* node) {                           \
    if (!node) return nullptr;                                     \
    if (!AstNodeClassCheck::IsInstanceOf<T>(node)) return nullptr; \
    return static_cast<T*>(node);                                  \
  }

struct Expression : AstNode {
  Expression(Kind kind, SourcePosition pos) : AstNode(kind, pos) {}
  DEFINE_AST_NODE_INNER_BOILERPLATE(Expression)
};

struct LocationExpression : Expression {
  LocationExpression(Kind kind, SourcePosition pos) : Expression(kind, pos) {}
  DEFINE_AST_NODE_INNER_BOILERPLATE(LocationExpression)
};

struct TypeExpression : AstNode {
  TypeExpression(Kind kind, SourcePosition pos) : AstNode(kind, pos) {}
  DEFINE_AST_NODE_INNER_BOILERPLATE(TypeExpression)
};

struct Declaration : AstNode {
  Declaration(Kind kind, SourcePosition pos) : AstNode(kind, pos) {}
  DEFINE_AST_NODE_INNER_BOILERPLATE(Declaration)
};

struct Statement : AstNode {
  Statement(Kind kind, SourcePosition pos) : AstNode(kind, pos) {}
  DEFINE_AST_NODE_INNER_BOILERPLATE(Statement)
};

class Module;

struct ModuleDeclaration : Declaration {
  ModuleDeclaration(AstNode::Kind kind, SourcePosition pos,
                    std::vector<Declaration*> declarations)
      : Declaration(kind, pos),
        module(nullptr),
        declarations(std::move(declarations)) {}
  virtual bool IsDefault() const = 0;
  //  virtual std::string GetName() const = 0;
  void SetModule(Module* m) { module = m; }
  Module* GetModule() const { return module; }
  Module* module;
  std::vector<Declaration*> declarations;
};

struct DefaultModuleDeclaration : ModuleDeclaration {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(DefaultModuleDeclaration)
  DefaultModuleDeclaration(SourcePosition pos,
                           std::vector<Declaration*> declarations)
      : ModuleDeclaration(kKind, pos, std::move(declarations)) {}
  bool IsDefault() const override { return true; }
};

struct ExplicitModuleDeclaration : ModuleDeclaration {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(ExplicitModuleDeclaration)
  ExplicitModuleDeclaration(SourcePosition pos, std::string name,
                            std::vector<Declaration*> declarations)
      : ModuleDeclaration(kKind, pos, std::move(declarations)),
        name(std::move(name)) {}
  bool IsDefault() const override { return false; }
  std::string name;
};

class Ast {
 public:
  Ast() : default_module_{SourcePosition{CurrentSourceFile::Get(), 0, 0}, {}} {}

  std::vector<Declaration*>& declarations() {
    return default_module_.declarations;
  }
  const std::vector<Declaration*>& declarations() const {
    return default_module_.declarations;
  }
  template <class T>
  T* AddNode(std::unique_ptr<T> node) {
    T* result = node.get();
    nodes_.push_back(std::move(node));
    return result;
  }
  DefaultModuleDeclaration* default_module() { return &default_module_; }

 private:
  DefaultModuleDeclaration default_module_;
  std::vector<std::unique_ptr<AstNode>> nodes_;
};

struct IdentifierExpression : LocationExpression {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(IdentifierExpression)
  IdentifierExpression(SourcePosition pos, std::string name,
                       std::vector<TypeExpression*> args = {})
      : LocationExpression(kKind, pos),
        name(std::move(name)),
        generic_arguments(std::move(args)) {}
  std::string name;
  std::vector<TypeExpression*> generic_arguments;
};

struct CallExpression : Expression {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(CallExpression)
  CallExpression(SourcePosition pos, std::string callee, bool is_operator,
                 std::vector<TypeExpression*> generic_arguments,
                 std::vector<Expression*> arguments,
                 std::vector<std::string> labels)
      : Expression(kKind, pos),
        callee(pos, std::move(callee), std::move(generic_arguments)),
        is_operator(is_operator),
        arguments(std::move(arguments)),
        labels(std::move(labels)) {}
  IdentifierExpression callee;
  bool is_operator;
  std::vector<Expression*> arguments;
  std::vector<std::string> labels;
};

struct StructExpression : Expression {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(StructExpression)
  StructExpression(SourcePosition pos, std::string name,
                   std::vector<Expression*> expressions)
      : Expression(kKind, pos),
        name(std::move(name)),
        expressions(std::move(expressions)) {}
  std::string name;
  std::vector<Expression*> expressions;
};

struct LogicalOrExpression : Expression {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(LogicalOrExpression)
  LogicalOrExpression(SourcePosition pos, Expression* left, Expression* right)
      : Expression(kKind, pos), left(left), right(right) {}
  Expression* left;
  Expression* right;
};

struct LogicalAndExpression : Expression {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(LogicalAndExpression)
  LogicalAndExpression(SourcePosition pos, Expression* left, Expression* right)
      : Expression(kKind, pos), left(left), right(right) {}
  Expression* left;
  Expression* right;
};

struct ConditionalExpression : Expression {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(ConditionalExpression)
  ConditionalExpression(SourcePosition pos, Expression* condition,
                        Expression* if_true, Expression* if_false)
      : Expression(kKind, pos),
        condition(condition),
        if_true(if_true),
        if_false(if_false) {}
  Expression* condition;
  Expression* if_true;
  Expression* if_false;
};

struct StringLiteralExpression : Expression {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(StringLiteralExpression)
  StringLiteralExpression(SourcePosition pos, std::string literal)
      : Expression(kKind, pos), literal(std::move(literal)) {}
  std::string literal;
};

struct NumberLiteralExpression : Expression {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(NumberLiteralExpression)
  NumberLiteralExpression(SourcePosition pos, std::string name)
      : Expression(kKind, pos), number(std::move(name)) {}
  std::string number;
};

struct ElementAccessExpression : LocationExpression {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(ElementAccessExpression)
  ElementAccessExpression(SourcePosition pos, Expression* array,
                          Expression* index)
      : LocationExpression(kKind, pos), array(array), index(index) {}
  Expression* array;
  Expression* index;
};

struct FieldAccessExpression : LocationExpression {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(FieldAccessExpression)
  FieldAccessExpression(SourcePosition pos, Expression* object,
                        std::string field)
      : LocationExpression(kKind, pos),
        object(object),
        field(std::move(field)) {}
  Expression* object;
  std::string field;
};

struct AssignmentExpression : Expression {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(AssignmentExpression)
  AssignmentExpression(SourcePosition pos, LocationExpression* location,
                       base::Optional<std::string> op, Expression* value)
      : Expression(kKind, pos),
        location(location),
        op(std::move(op)),
        value(value) {}
  LocationExpression* location;
  base::Optional<std::string> op;
  Expression* value;
};

enum class IncrementDecrementOperator { kIncrement, kDecrement };

struct IncrementDecrementExpression : Expression {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(IncrementDecrementExpression)
  IncrementDecrementExpression(SourcePosition pos, LocationExpression* location,
                               IncrementDecrementOperator op, bool postfix)
      : Expression(kKind, pos), location(location), op(op), postfix(postfix) {}
  LocationExpression* location;
  IncrementDecrementOperator op;
  bool postfix;
};

// This expression is only used in the desugaring of typeswitch, and it allows
// to bake in the static information that certain types are impossible at a
// certain position in the control flow.
// The result type is the type of {expression} minus the provided type.
struct AssumeTypeImpossibleExpression : Expression {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(AssumeTypeImpossibleExpression)
  AssumeTypeImpossibleExpression(SourcePosition pos,
                                 TypeExpression* excluded_type,
                                 Expression* expression)
      : Expression(kKind, pos),
        excluded_type(excluded_type),
        expression(expression) {}
  TypeExpression* excluded_type;
  Expression* expression;
};

struct ParameterList {
  std::vector<std::string> names;
  std::vector<TypeExpression*> types;
  bool has_varargs;
  std::string arguments_variable;

  static ParameterList Empty() { return ParameterList{{}, {}, false, ""}; }
};

struct BasicTypeExpression : TypeExpression {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(BasicTypeExpression)
  BasicTypeExpression(SourcePosition pos, bool is_constexpr, std::string name)
      : TypeExpression(kKind, pos),
        is_constexpr(is_constexpr),
        name(std::move(name)) {}
  bool is_constexpr;
  std::string name;
};

struct FunctionTypeExpression : TypeExpression {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(FunctionTypeExpression)
  FunctionTypeExpression(SourcePosition pos,
                         std::vector<TypeExpression*> parameters,
                         TypeExpression* return_type)
      : TypeExpression(kKind, pos),
        parameters(std::move(parameters)),
        return_type(return_type) {}
  std::vector<TypeExpression*> parameters;
  TypeExpression* return_type;
};

struct UnionTypeExpression : TypeExpression {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(UnionTypeExpression)
  UnionTypeExpression(SourcePosition pos, TypeExpression* a, TypeExpression* b)
      : TypeExpression(kKind, pos), a(a), b(b) {}
  TypeExpression* a;
  TypeExpression* b;
};

struct ExpressionStatement : Statement {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(ExpressionStatement)
  ExpressionStatement(SourcePosition pos, Expression* expression)
      : Statement(kKind, pos), expression(expression) {}
  Expression* expression;
};

struct IfStatement : Statement {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(IfStatement)
  IfStatement(SourcePosition pos, bool is_constexpr, Expression* condition,
              Statement* if_true, base::Optional<Statement*> if_false)
      : Statement(kKind, pos),
        condition(condition),
        is_constexpr(is_constexpr),
        if_true(if_true),
        if_false(if_false) {}
  Expression* condition;
  bool is_constexpr;
  Statement* if_true;
  base::Optional<Statement*> if_false;
};

struct WhileStatement : Statement {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(WhileStatement)
  WhileStatement(SourcePosition pos, Expression* condition, Statement* body)
      : Statement(kKind, pos), condition(condition), body(body) {}
  Expression* condition;
  Statement* body;
};

struct ReturnStatement : Statement {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(ReturnStatement)
  ReturnStatement(SourcePosition pos, base::Optional<Expression*> value)
      : Statement(kKind, pos), value(value) {}
  base::Optional<Expression*> value;
};

struct DebugStatement : Statement {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(DebugStatement)
  DebugStatement(SourcePosition pos, const std::string& reason,
                 bool never_continues)
      : Statement(kKind, pos),
        reason(reason),
        never_continues(never_continues) {}
  std::string reason;
  bool never_continues;
};

struct AssertStatement : Statement {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(AssertStatement)
  AssertStatement(SourcePosition pos, bool debug_only, Expression* expression,
                  std::string source)
      : Statement(kKind, pos),
        debug_only(debug_only),
        expression(expression),
        source(std::move(source)) {}
  bool debug_only;
  Expression* expression;
  std::string source;
};

struct TailCallStatement : Statement {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(TailCallStatement)
  TailCallStatement(SourcePosition pos, CallExpression* call)
      : Statement(kKind, pos), call(call) {}
  CallExpression* call;
};

struct VarDeclarationStatement : Statement {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(VarDeclarationStatement)
  VarDeclarationStatement(
      SourcePosition pos, bool const_qualified, std::string name,
      base::Optional<TypeExpression*> type,
      base::Optional<Expression*> initializer = base::nullopt)
      : Statement(kKind, pos),
        const_qualified(const_qualified),
        name(std::move(name)),
        type(type),
        initializer(initializer) {}
  bool const_qualified;
  std::string name;
  base::Optional<TypeExpression*> type;
  base::Optional<Expression*> initializer;
};

struct BreakStatement : Statement {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(BreakStatement)
  explicit BreakStatement(SourcePosition pos) : Statement(kKind, pos) {}
};

struct ContinueStatement : Statement {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(ContinueStatement)
  explicit ContinueStatement(SourcePosition pos) : Statement(kKind, pos) {}
};

struct GotoStatement : Statement {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(GotoStatement)
  GotoStatement(SourcePosition pos, std::string label,
                const std::vector<Expression*>& arguments)
      : Statement(kKind, pos),
        label(std::move(label)),
        arguments(std::move(arguments)) {}
  std::string label;
  std::vector<Expression*> arguments;
};

struct ForLoopStatement : Statement {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(ForLoopStatement)
  ForLoopStatement(SourcePosition pos, base::Optional<Statement*> declaration,
                   base::Optional<Expression*> test,
                   base::Optional<Expression*> action, Statement* body)
      : Statement(kKind, pos),
        var_declaration(),
        test(std::move(test)),
        action(std::move(action)),
        body(std::move(body)) {
    if (declaration)
      var_declaration = VarDeclarationStatement::cast(*declaration);
  }
  base::Optional<VarDeclarationStatement*> var_declaration;
  base::Optional<Expression*> test;
  base::Optional<Expression*> action;
  Statement* body;
};

struct RangeExpression {
  base::Optional<Expression*> begin;
  base::Optional<Expression*> end;
};

struct ForOfLoopStatement : Statement {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(ForOfLoopStatement)
  ForOfLoopStatement(SourcePosition pos, Statement* decl, Expression* iterable,
                     base::Optional<RangeExpression> range, Statement* body)
      : Statement(kKind, pos),
        var_declaration(VarDeclarationStatement::cast(decl)),
        iterable(iterable),
        body(body) {
    if (range) {
      begin = range->begin;
      end = range->end;
    }
  }
  VarDeclarationStatement* var_declaration;
  Expression* iterable;
  base::Optional<Expression*> begin;
  base::Optional<Expression*> end;
  Statement* body;
};

struct LabelBlock : AstNode {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(LabelBlock)
  LabelBlock(SourcePosition pos, const std::string& label,
             const ParameterList& parameters, Statement* body)
      : AstNode(kKind, pos),
        label(std::move(label)),
        parameters(parameters),
        body(std::move(body)) {}
  std::string label;
  ParameterList parameters;
  Statement* body;
};

struct StatementExpression : Expression {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(StatementExpression)
  StatementExpression(SourcePosition pos, Statement* statement)
      : Expression(kKind, pos), statement(statement) {}
  Statement* statement;
};

struct TryLabelExpression : Expression {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(TryLabelExpression)
  TryLabelExpression(SourcePosition pos, Expression* try_expression,
                     LabelBlock* label_block)
      : Expression(kKind, pos),
        try_expression(try_expression),
        label_block(label_block) {}
  Expression* try_expression;
  LabelBlock* label_block;
};

struct BlockStatement : Statement {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(BlockStatement)
  explicit BlockStatement(SourcePosition pos, bool deferred = false,
                          std::vector<Statement*> statements = {})
      : Statement(kKind, pos),
        deferred(deferred),
        statements(std::move(statements)) {}
  bool deferred;
  std::vector<Statement*> statements;
};

struct TypeDeclaration : Declaration {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(TypeDeclaration)
  TypeDeclaration(SourcePosition pos, std::string name,
                  base::Optional<std::string> extends,
                  base::Optional<std::string> generates,
                  base::Optional<std::string> constexpr_generates)
      : Declaration(kKind, pos),
        name(std::move(name)),
        extends(std::move(extends)),
        generates(std::move(generates)),
        constexpr_generates(std::move(constexpr_generates)) {}
  std::string name;
  base::Optional<std::string> extends;
  base::Optional<std::string> generates;
  base::Optional<std::string> constexpr_generates;
};

struct TypeAliasDeclaration : Declaration {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(TypeAliasDeclaration)
  TypeAliasDeclaration(SourcePosition pos, std::string name,
                       TypeExpression* type)
      : Declaration(kKind, pos), name(std::move(name)), type(type) {}
  std::string name;
  TypeExpression* type;
};

struct NameAndTypeExpression {
  std::string name;
  TypeExpression* type;
};

struct LabelAndTypes {
  std::string name;
  std::vector<TypeExpression*> types;
};

typedef std::vector<LabelAndTypes> LabelAndTypesVector;

struct CallableNodeSignature {
  ParameterList parameters;
  TypeExpression* return_type;
  LabelAndTypesVector labels;
};

struct CallableNode : AstNode {
  CallableNode(AstNode::Kind kind, SourcePosition pos, std::string name,
               ParameterList parameters, TypeExpression* return_type,
               const LabelAndTypesVector& labels)
      : AstNode(kind, pos),
        name(std::move(name)),
        signature(new CallableNodeSignature{parameters, return_type, labels}) {}
  DEFINE_AST_NODE_INNER_BOILERPLATE(CallableNode)
  std::string name;
  std::unique_ptr<CallableNodeSignature> signature;
};

struct MacroDeclaration : CallableNode {
  DEFINE_AST_NODE_INNER_BOILERPLATE(MacroDeclaration)
  MacroDeclaration(AstNode::Kind kind, SourcePosition pos, std::string name,
                   base::Optional<std::string> op, ParameterList parameters,
                   TypeExpression* return_type,
                   const LabelAndTypesVector& labels)
      : CallableNode(kind, pos, std::move(name), std::move(parameters),
                     return_type, labels),
        op(std::move(op)) {}
  base::Optional<std::string> op;
};

struct ExternalMacroDeclaration : MacroDeclaration {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(ExternalMacroDeclaration)
  ExternalMacroDeclaration(SourcePosition pos, std::string name,
                           base::Optional<std::string> op,
                           ParameterList parameters,
                           TypeExpression* return_type,
                           const LabelAndTypesVector& labels)
      : MacroDeclaration(kKind, pos, std::move(name), std::move(op),
                         std::move(parameters), return_type, labels) {}
};

struct TorqueMacroDeclaration : MacroDeclaration {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(TorqueMacroDeclaration)
  TorqueMacroDeclaration(SourcePosition pos, std::string name,
                         base::Optional<std::string> op,
                         ParameterList parameters, TypeExpression* return_type,
                         const LabelAndTypesVector& labels)
      : MacroDeclaration(kKind, pos, std::move(name), std::move(op),
                         std::move(parameters), return_type, labels) {}
};

struct BuiltinDeclaration : CallableNode {
  BuiltinDeclaration(AstNode::Kind kind, SourcePosition pos,
                     bool javascript_linkage, std::string name,
                     ParameterList parameters, TypeExpression* return_type)
      : CallableNode(kind, pos, std::move(name), std::move(parameters),
                     return_type, {}),
        javascript_linkage(javascript_linkage) {}
  bool javascript_linkage;
};

struct ExternalBuiltinDeclaration : BuiltinDeclaration {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(ExternalBuiltinDeclaration)
  ExternalBuiltinDeclaration(SourcePosition pos, bool javascript_linkage,
                             std::string name, ParameterList parameters,
                             TypeExpression* return_type)
      : BuiltinDeclaration(kKind, pos, javascript_linkage, std::move(name),
                           std::move(parameters), return_type) {}
};

struct TorqueBuiltinDeclaration : BuiltinDeclaration {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(TorqueBuiltinDeclaration)
  TorqueBuiltinDeclaration(SourcePosition pos, bool javascript_linkage,
                           std::string name, ParameterList parameters,
                           TypeExpression* return_type)
      : BuiltinDeclaration(kKind, pos, javascript_linkage, std::move(name),
                           std::move(parameters), return_type) {}
};

struct ExternalRuntimeDeclaration : CallableNode {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(ExternalRuntimeDeclaration)
  ExternalRuntimeDeclaration(SourcePosition pos, std::string name,
                             ParameterList parameters,
                             TypeExpression* return_type)
      : CallableNode(kKind, pos, name, parameters, return_type, {}) {}
};

struct ConstDeclaration : Declaration {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(ConstDeclaration)
  ConstDeclaration(SourcePosition pos, std::string name, TypeExpression* type,
                   Expression* expression)
      : Declaration(kKind, pos),
        name(std::move(name)),
        type(type),
        expression(expression) {}
  std::string name;
  TypeExpression* type;
  Expression* expression;
};

struct StandardDeclaration : Declaration {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(StandardDeclaration)
  StandardDeclaration(SourcePosition pos, CallableNode* callable,
                      Statement* body)
      : Declaration(kKind, pos), callable(callable), body(body) {}
  CallableNode* callable;
  Statement* body;
};

struct GenericDeclaration : Declaration {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(GenericDeclaration)
  GenericDeclaration(SourcePosition pos, CallableNode* callable,
                     std::vector<std::string> generic_parameters,
                     base::Optional<Statement*> body = base::nullopt)
      : Declaration(kKind, pos),
        callable(callable),
        generic_parameters(std::move(generic_parameters)),
        body(body) {}
  CallableNode* callable;
  std::vector<std::string> generic_parameters;
  base::Optional<Statement*> body;
};

struct SpecializationDeclaration : Declaration {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(SpecializationDeclaration)
  SpecializationDeclaration(SourcePosition pos, std::string name,
                            std::vector<TypeExpression*> generic_parameters,
                            ParameterList parameters,
                            TypeExpression* return_type,
                            LabelAndTypesVector labels, Statement* b)
      : Declaration(kKind, pos),
        name(std::move(name)),
        external(false),
        generic_parameters(std::move(generic_parameters)),
        signature(new CallableNodeSignature{std::move(parameters), return_type,
                                            std::move(labels)}),
        body(b) {}
  std::string name;
  bool external;
  std::vector<TypeExpression*> generic_parameters;
  std::unique_ptr<CallableNodeSignature> signature;
  Statement* body;
};

struct ExternConstDeclaration : Declaration {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(ExternConstDeclaration)
  ExternConstDeclaration(SourcePosition pos, std::string name,
                         TypeExpression* type, std::string literal)
      : Declaration(kKind, pos),
        name(std::move(name)),
        type(type),
        literal(std::move(literal)) {}
  std::string name;
  TypeExpression* type;
  std::string literal;
};

struct StructDeclaration : Declaration {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(StructDeclaration)
  StructDeclaration(SourcePosition pos, std::string name,
                    std::vector<NameAndTypeExpression> fields)
      : Declaration(kKind, pos),
        name(std::move(name)),
        fields(std::move(fields)) {}
  std::string name;
  std::vector<NameAndTypeExpression> fields;
};

#define ENUM_ITEM(name)                     \
  case AstNode::Kind::k##name:              \
    return std::is_base_of<T, name>::value; \
    break;

template <class T>
bool AstNodeClassCheck::IsInstanceOf(AstNode* node) {
  switch (node->kind) {
    AST_NODE_KIND_LIST(ENUM_ITEM)
    default:
      UNIMPLEMENTED();
  }
  return true;
}

#undef ENUM_ITEM

}  // namespace torque
}  // namespace internal
}  // namespace v8

#endif  // V8_TORQUE_AST_H_
