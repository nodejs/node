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
  V(CallMethodExpression)                \
  V(IntrinsicCallExpression)             \
  V(StructExpression)                    \
  V(LogicalOrExpression)                 \
  V(LogicalAndExpression)                \
  V(SpreadExpression)                    \
  V(ConditionalExpression)               \
  V(IdentifierExpression)                \
  V(StringLiteralExpression)             \
  V(NumberLiteralExpression)             \
  V(FieldAccessExpression)               \
  V(ElementAccessExpression)             \
  V(DereferenceExpression)               \
  V(AssignmentExpression)                \
  V(IncrementDecrementExpression)        \
  V(NewExpression)                       \
  V(AssumeTypeImpossibleExpression)      \
  V(StatementExpression)                 \
  V(TryLabelExpression)

#define AST_TYPE_EXPRESSION_NODE_KIND_LIST(V) \
  V(BasicTypeExpression)                      \
  V(FunctionTypeExpression)                   \
  V(UnionTypeExpression)                      \
  V(ReferenceTypeExpression)

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
  V(GotoStatement)

#define AST_DECLARATION_NODE_KIND_LIST(V) \
  V(TypeDeclaration)                      \
  V(TypeAliasDeclaration)                 \
  V(StandardDeclaration)                  \
  V(GenericDeclaration)                   \
  V(SpecializationDeclaration)            \
  V(ExternConstDeclaration)               \
  V(ClassDeclaration)                     \
  V(StructDeclaration)                    \
  V(NamespaceDeclaration)                 \
  V(ConstDeclaration)                     \
  V(CppIncludeDeclaration)

#define AST_CALLABLE_NODE_KIND_LIST(V) \
  V(TorqueMacroDeclaration)            \
  V(TorqueBuiltinDeclaration)          \
  V(ExternalMacroDeclaration)          \
  V(ExternalBuiltinDeclaration)        \
  V(ExternalRuntimeDeclaration)        \
  V(IntrinsicDeclaration)

#define AST_NODE_KIND_LIST(V)           \
  AST_EXPRESSION_NODE_KIND_LIST(V)      \
  AST_TYPE_EXPRESSION_NODE_KIND_LIST(V) \
  AST_STATEMENT_NODE_KIND_LIST(V)       \
  AST_DECLARATION_NODE_KIND_LIST(V)     \
  AST_CALLABLE_NODE_KIND_LIST(V)        \
  V(Identifier)                         \
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

class Namespace;

struct NamespaceDeclaration : Declaration {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(NamespaceDeclaration)
  NamespaceDeclaration(SourcePosition pos, std::string name,
                       std::vector<Declaration*> declarations)
      : Declaration(kKind, pos),
        declarations(std::move(declarations)),
        name(name) {}
  std::vector<Declaration*> declarations;
  std::string name;
};

class Ast {
 public:
  Ast() {}

  std::vector<Declaration*>& declarations() { return declarations_; }
  const std::vector<Declaration*>& declarations() const {
    return declarations_;
  }
  template <class T>
  T* AddNode(std::unique_ptr<T> node) {
    T* result = node.get();
    nodes_.push_back(std::move(node));
    return result;
  }

 private:
  std::vector<Declaration*> declarations_;
  std::vector<std::unique_ptr<AstNode>> nodes_;
};

static const char* const kThisParameterName = "this";

// A Identifier is a string with a SourcePosition attached.
struct Identifier : AstNode {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(Identifier)
  Identifier(SourcePosition pos, std::string identifier)
      : AstNode(kKind, pos), value(std::move(identifier)) {}
  std::string value;
};

struct IdentifierExpression : LocationExpression {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(IdentifierExpression)
  IdentifierExpression(SourcePosition pos,
                       std::vector<std::string> namespace_qualification,
                       Identifier* name, std::vector<TypeExpression*> args = {})
      : LocationExpression(kKind, pos),
        namespace_qualification(std::move(namespace_qualification)),
        name(name),
        generic_arguments(std::move(args)) {}
  IdentifierExpression(SourcePosition pos, Identifier* name,
                       std::vector<TypeExpression*> args = {})
      : IdentifierExpression(pos, {}, name, std::move(args)) {}
  bool IsThis() const { return name->value == kThisParameterName; }
  std::vector<std::string> namespace_qualification;
  Identifier* name;
  std::vector<TypeExpression*> generic_arguments;
};

struct IntrinsicCallExpression : Expression {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(IntrinsicCallExpression)
  IntrinsicCallExpression(SourcePosition pos, std::string name,
                          std::vector<TypeExpression*> generic_arguments,
                          std::vector<Expression*> arguments)
      : Expression(kKind, pos),
        name(std::move(name)),
        generic_arguments(std::move(generic_arguments)),
        arguments(std::move(arguments)) {}
  std::string name;
  std::vector<TypeExpression*> generic_arguments;
  std::vector<Expression*> arguments;
};

struct CallMethodExpression : Expression {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(CallMethodExpression)
  CallMethodExpression(SourcePosition pos, Expression* target,
                       IdentifierExpression* method,
                       std::vector<Expression*> arguments,
                       std::vector<std::string> labels)
      : Expression(kKind, pos),
        target(target),
        method(method),
        arguments(std::move(arguments)),
        labels(std::move(labels)) {}
  Expression* target;
  IdentifierExpression* method;
  std::vector<Expression*> arguments;
  std::vector<std::string> labels;
};

struct CallExpression : Expression {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(CallExpression)
  CallExpression(SourcePosition pos, IdentifierExpression* callee,
                 std::vector<Expression*> arguments,
                 std::vector<std::string> labels)
      : Expression(kKind, pos),
        callee(callee),
        arguments(std::move(arguments)),
        labels(std::move(labels)) {}
  IdentifierExpression* callee;
  std::vector<Expression*> arguments;
  std::vector<std::string> labels;
};

struct NameAndExpression {
  Identifier* name;
  Expression* expression;
};

struct StructExpression : Expression {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(StructExpression)
  StructExpression(SourcePosition pos, TypeExpression* type,
                   std::vector<NameAndExpression> initializers)
      : Expression(kKind, pos),
        type(type),
        initializers(std::move(initializers)) {}
  TypeExpression* type;
  std::vector<NameAndExpression> initializers;
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

struct SpreadExpression : Expression {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(SpreadExpression)
  SpreadExpression(SourcePosition pos, Expression* spreadee)
      : Expression(kKind, pos), spreadee(spreadee) {}
  Expression* spreadee;
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
                        Identifier* field)
      : LocationExpression(kKind, pos), object(object), field(field) {}
  Expression* object;
  Identifier* field;
};

struct DereferenceExpression : LocationExpression {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(DereferenceExpression)
  DereferenceExpression(SourcePosition pos, Expression* reference)
      : LocationExpression(kKind, pos), reference(reference) {}
  Expression* reference;
};

struct AssignmentExpression : Expression {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(AssignmentExpression)
  AssignmentExpression(SourcePosition pos, Expression* location,
                       Expression* value)
      : AssignmentExpression(pos, location, base::nullopt, value) {}
  AssignmentExpression(SourcePosition pos, Expression* location,
                       base::Optional<std::string> op, Expression* value)
      : Expression(kKind, pos),
        location(location),
        op(std::move(op)),
        value(value) {}
  Expression* location;
  base::Optional<std::string> op;
  Expression* value;
};

enum class IncrementDecrementOperator { kIncrement, kDecrement };

struct IncrementDecrementExpression : Expression {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(IncrementDecrementExpression)
  IncrementDecrementExpression(SourcePosition pos, Expression* location,
                               IncrementDecrementOperator op, bool postfix)
      : Expression(kKind, pos), location(location), op(op), postfix(postfix) {}
  Expression* location;
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

struct NewExpression : Expression {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(NewExpression)
  NewExpression(SourcePosition pos, TypeExpression* type,
                std::vector<NameAndExpression> initializers)
      : Expression(kKind, pos),
        type(type),
        initializers(std::move(initializers)) {}
  TypeExpression* type;
  std::vector<NameAndExpression> initializers;
};

struct ParameterList {
  std::vector<Identifier*> names;
  std::vector<TypeExpression*> types;
  size_t implicit_count;
  bool has_varargs;
  std::string arguments_variable;

  static ParameterList Empty() { return ParameterList{{}, {}, 0, false, ""}; }
  std::vector<TypeExpression*> GetImplicitTypes() {
    return std::vector<TypeExpression*>(types.begin(),
                                        types.begin() + implicit_count);
  }
  std::vector<TypeExpression*> GetExplicitTypes() {
    return std::vector<TypeExpression*>(types.begin() + implicit_count,
                                        types.end());
  }
};

struct BasicTypeExpression : TypeExpression {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(BasicTypeExpression)
  BasicTypeExpression(SourcePosition pos,
                      std::vector<std::string> namespace_qualification,
                      bool is_constexpr, std::string name)
      : TypeExpression(kKind, pos),
        namespace_qualification(std::move(namespace_qualification)),
        is_constexpr(is_constexpr),
        name(std::move(name)) {}
  std::vector<std::string> namespace_qualification;
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

struct ReferenceTypeExpression : TypeExpression {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(ReferenceTypeExpression)
  ReferenceTypeExpression(SourcePosition pos, TypeExpression* referenced_type)
      : TypeExpression(kKind, pos), referenced_type(referenced_type) {}
  TypeExpression* referenced_type;
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
      SourcePosition pos, bool const_qualified, Identifier* name,
      base::Optional<TypeExpression*> type,
      base::Optional<Expression*> initializer = base::nullopt)
      : Statement(kKind, pos),
        const_qualified(const_qualified),
        name(name),
        type(type),
        initializer(initializer) {}
  bool const_qualified;
  Identifier* name;
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
                   base::Optional<Statement*> action, Statement* body)
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
  base::Optional<Statement*> action;
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
  LabelBlock(SourcePosition pos, std::string label,
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
  TryLabelExpression(SourcePosition pos, bool catch_exceptions,
                     Expression* try_expression, LabelBlock* label_block)
      : Expression(kKind, pos),
        catch_exceptions(catch_exceptions),
        try_expression(try_expression),
        label_block(label_block) {}
  bool catch_exceptions;
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
  TypeDeclaration(SourcePosition pos, Identifier* name, bool transient,
                  base::Optional<Identifier*> extends,
                  base::Optional<std::string> generates,
                  base::Optional<std::string> constexpr_generates)
      : Declaration(kKind, pos),
        name(name),
        transient(transient),
        extends(extends),
        generates(std::move(generates)),
        constexpr_generates(std::move(constexpr_generates)) {}
  Identifier* name;
  bool transient;
  base::Optional<Identifier*> extends;
  base::Optional<std::string> generates;
  base::Optional<std::string> constexpr_generates;
};

struct TypeAliasDeclaration : Declaration {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(TypeAliasDeclaration)
  TypeAliasDeclaration(SourcePosition pos, Identifier* name,
                       TypeExpression* type)
      : Declaration(kKind, pos), name(name), type(type) {}
  Identifier* name;
  TypeExpression* type;
};

struct NameAndTypeExpression {
  Identifier* name;
  TypeExpression* type;
};

struct StructFieldExpression {
  NameAndTypeExpression name_and_type;
  bool const_qualified;
};

struct ClassFieldExpression {
  NameAndTypeExpression name_and_type;
  base::Optional<std::string> index;
  bool weak;
  bool const_qualified;
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
  CallableNode(AstNode::Kind kind, SourcePosition pos, bool transitioning,
               std::string name, ParameterList parameters,
               TypeExpression* return_type, const LabelAndTypesVector& labels)
      : AstNode(kind, pos),
        transitioning(transitioning),
        name(std::move(name)),
        signature(new CallableNodeSignature{parameters, return_type, labels}) {}
  DEFINE_AST_NODE_INNER_BOILERPLATE(CallableNode)
  bool transitioning;
  std::string name;
  std::unique_ptr<CallableNodeSignature> signature;
};

struct MacroDeclaration : CallableNode {
  DEFINE_AST_NODE_INNER_BOILERPLATE(MacroDeclaration)
  MacroDeclaration(AstNode::Kind kind, SourcePosition pos, bool transitioning,
                   std::string name, base::Optional<std::string> op,
                   ParameterList parameters, TypeExpression* return_type,
                   const LabelAndTypesVector& labels)
      : CallableNode(kind, pos, transitioning, std::move(name),
                     std::move(parameters), return_type, labels),
        op(std::move(op)) {}
  base::Optional<std::string> op;
};

struct ExternalMacroDeclaration : MacroDeclaration {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(ExternalMacroDeclaration)
  ExternalMacroDeclaration(SourcePosition pos, bool transitioning,
                           std::string external_assembler_name,
                           std::string name, base::Optional<std::string> op,
                           ParameterList parameters,
                           TypeExpression* return_type,
                           const LabelAndTypesVector& labels)
      : MacroDeclaration(kKind, pos, transitioning, std::move(name),
                         std::move(op), std::move(parameters), return_type,
                         labels),
        external_assembler_name(std::move(external_assembler_name)) {}
  std::string external_assembler_name;
};

struct IntrinsicDeclaration : CallableNode {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(IntrinsicDeclaration)
  IntrinsicDeclaration(SourcePosition pos, std::string name,
                       ParameterList parameters, TypeExpression* return_type)
      : CallableNode(kKind, pos, false, std::move(name), std::move(parameters),
                     return_type, {}) {}
};

struct TorqueMacroDeclaration : MacroDeclaration {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(TorqueMacroDeclaration)
  TorqueMacroDeclaration(SourcePosition pos, bool transitioning,
                         std::string name, base::Optional<std::string> op,
                         ParameterList parameters, TypeExpression* return_type,
                         const LabelAndTypesVector& labels)
      : MacroDeclaration(kKind, pos, transitioning, std::move(name),
                         std::move(op), std::move(parameters), return_type,
                         labels) {}
};

struct BuiltinDeclaration : CallableNode {
  DEFINE_AST_NODE_INNER_BOILERPLATE(BuiltinDeclaration)
  BuiltinDeclaration(AstNode::Kind kind, SourcePosition pos,
                     bool javascript_linkage, bool transitioning,
                     std::string name, ParameterList parameters,
                     TypeExpression* return_type)
      : CallableNode(kind, pos, transitioning, std::move(name),
                     std::move(parameters), return_type, {}),
        javascript_linkage(javascript_linkage) {}
  bool javascript_linkage;
};

struct ExternalBuiltinDeclaration : BuiltinDeclaration {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(ExternalBuiltinDeclaration)
  ExternalBuiltinDeclaration(SourcePosition pos, bool transitioning,
                             bool javascript_linkage, std::string name,
                             ParameterList parameters,
                             TypeExpression* return_type)
      : BuiltinDeclaration(kKind, pos, javascript_linkage, transitioning,
                           std::move(name), std::move(parameters),
                           return_type) {}
};

struct TorqueBuiltinDeclaration : BuiltinDeclaration {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(TorqueBuiltinDeclaration)
  TorqueBuiltinDeclaration(SourcePosition pos, bool transitioning,
                           bool javascript_linkage, std::string name,
                           ParameterList parameters,
                           TypeExpression* return_type)
      : BuiltinDeclaration(kKind, pos, javascript_linkage, transitioning,
                           std::move(name), std::move(parameters),
                           return_type) {}
};

struct ExternalRuntimeDeclaration : CallableNode {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(ExternalRuntimeDeclaration)
  ExternalRuntimeDeclaration(SourcePosition pos, bool transitioning,
                             std::string name, ParameterList parameters,
                             TypeExpression* return_type)
      : CallableNode(kKind, pos, transitioning, name, parameters, return_type,
                     {}) {}
};

struct ConstDeclaration : Declaration {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(ConstDeclaration)
  ConstDeclaration(SourcePosition pos, Identifier* name, TypeExpression* type,
                   Expression* expression)
      : Declaration(kKind, pos),
        name(name),
        type(type),
        expression(expression) {}
  Identifier* name;
  TypeExpression* type;
  Expression* expression;
};

struct StandardDeclaration : Declaration {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(StandardDeclaration)
  StandardDeclaration(SourcePosition pos, CallableNode* callable,
                      base::Optional<Statement*> body)
      : Declaration(kKind, pos), callable(callable), body(body) {}
  CallableNode* callable;
  base::Optional<Statement*> body;
};

struct GenericDeclaration : Declaration {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(GenericDeclaration)
  GenericDeclaration(SourcePosition pos, CallableNode* callable,
                     std::vector<Identifier*> generic_parameters,
                     base::Optional<Statement*> body = base::nullopt)
      : Declaration(kKind, pos),
        callable(callable),
        generic_parameters(std::move(generic_parameters)),
        body(body) {}
  CallableNode* callable;
  std::vector<Identifier*> generic_parameters;
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
  ExternConstDeclaration(SourcePosition pos, Identifier* name,
                         TypeExpression* type, std::string literal)
      : Declaration(kKind, pos),
        name(name),
        type(type),
        literal(std::move(literal)) {}
  Identifier* name;
  TypeExpression* type;
  std::string literal;
};

struct StructDeclaration : Declaration {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(StructDeclaration)
  StructDeclaration(SourcePosition pos, Identifier* name,
                    std::vector<Declaration*> methods,
                    std::vector<StructFieldExpression> fields)
      : Declaration(kKind, pos),
        name(name),
        methods(std::move(methods)),
        fields(std::move(fields)) {}
  Identifier* name;
  std::vector<Declaration*> methods;
  std::vector<StructFieldExpression> fields;
};

struct ClassDeclaration : Declaration {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(ClassDeclaration)
  ClassDeclaration(SourcePosition pos, Identifier* name, bool is_extern,
                   bool generate_print, bool transient,
                   base::Optional<std::string> super,
                   base::Optional<std::string> generates,
                   std::vector<Declaration*> methods,
                   std::vector<ClassFieldExpression> fields)
      : Declaration(kKind, pos),
        name(name),
        is_extern(is_extern),
        generate_print(generate_print),
        transient(transient),
        super(std::move(super)),
        generates(std::move(generates)),
        methods(std::move(methods)),
        fields(std::move(fields)) {}
  Identifier* name;
  bool is_extern;
  bool generate_print;
  bool transient;
  base::Optional<std::string> super;
  base::Optional<std::string> generates;
  std::vector<Declaration*> methods;
  std::vector<ClassFieldExpression> fields;
};

struct CppIncludeDeclaration : Declaration {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(CppIncludeDeclaration)
  CppIncludeDeclaration(SourcePosition pos, std::string include_path)
      : Declaration(kKind, pos), include_path(std::move(include_path)) {}
  std::string include_path;
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

inline bool IsDeferred(Statement* stmt) {
  if (auto* block = BlockStatement::DynamicCast(stmt)) {
    return block->deferred;
  }
  return false;
}

DECLARE_CONTEXTUAL_VARIABLE(CurrentAst, Ast);

template <class T, class... Args>
T* MakeNode(Args... args) {
  return CurrentAst::Get().AddNode(std::unique_ptr<T>(
      new T(CurrentSourcePosition::Get(), std::move(args)...)));
}

}  // namespace torque
}  // namespace internal
}  // namespace v8

#endif  // V8_TORQUE_AST_H_
