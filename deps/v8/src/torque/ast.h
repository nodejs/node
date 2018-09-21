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
#include "src/torque/contextual.h"

namespace v8 {
namespace internal {
namespace torque {

enum class SourceId : int {};

struct SourcePosition {
  SourceId source;
  int line;
  int column;
};

DECLARE_CONTEXTUAL_VARIABLE(CurrentSourcePosition, SourcePosition)

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
  V(IncrementDecrementExpression)

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
  V(TryLabelStatement)

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

  AstNode(Kind k, SourcePosition p) : kind(k), pos(p) {}
  virtual ~AstNode() {}

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
  Expression(Kind k, SourcePosition p) : AstNode(k, p) {}
  DEFINE_AST_NODE_INNER_BOILERPLATE(Expression)
};

struct LocationExpression : Expression {
  LocationExpression(Kind k, SourcePosition p) : Expression(k, p) {}
  DEFINE_AST_NODE_INNER_BOILERPLATE(LocationExpression)
};

struct TypeExpression : AstNode {
  TypeExpression(Kind k, SourcePosition p) : AstNode(k, p) {}
  DEFINE_AST_NODE_INNER_BOILERPLATE(TypeExpression)
};

struct Declaration : AstNode {
  Declaration(Kind k, SourcePosition p) : AstNode(k, p) {}
  DEFINE_AST_NODE_INNER_BOILERPLATE(Declaration)
};

struct Statement : AstNode {
  Statement(Kind k, SourcePosition p) : AstNode(k, p) {}
  DEFINE_AST_NODE_INNER_BOILERPLATE(Statement)
};

class Module;

struct ModuleDeclaration : Declaration {
  ModuleDeclaration(AstNode::Kind kind, SourcePosition p,
                    std::vector<Declaration*> d)
      : Declaration(kind, p), module(nullptr), declarations(std::move(d)) {}
  virtual bool IsDefault() const = 0;
  //  virtual std::string GetName() const = 0;
  void SetModule(Module* m) { module = m; }
  Module* GetModule() const { return module; }
  Module* module;
  std::vector<Declaration*> declarations;
};

struct DefaultModuleDeclaration : ModuleDeclaration {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(DefaultModuleDeclaration)
  DefaultModuleDeclaration(SourcePosition p, std::vector<Declaration*> d)
      : ModuleDeclaration(kKind, p, d) {}
  bool IsDefault() const override { return true; }
};

struct ExplicitModuleDeclaration : ModuleDeclaration {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(ExplicitModuleDeclaration)
  ExplicitModuleDeclaration(SourcePosition p, std::string n,
                            std::vector<Declaration*> d)
      : ModuleDeclaration(kKind, p, d), name(std::move(n)) {}
  bool IsDefault() const override { return false; }
  std::string name;
};

class SourceFileMap : public ContextualClass<SourceFileMap> {
 public:
  SourceFileMap() {}
  const std::string& GetSource(SourceId id) const {
    return sources_[static_cast<int>(id)];
  }

  std::string PositionAsString(SourcePosition pos) {
    return GetSource(pos.source) + ":" + std::to_string(pos.line) + ":" +
           std::to_string(pos.column);
  }

  SourceId AddSource(std::string path) {
    sources_.push_back(std::move(path));
    return static_cast<SourceId>(sources_.size() - 1);
  }

 private:
  std::vector<std::string> sources_;
};

inline std::string PositionAsString(SourcePosition pos) {
  return SourceFileMap::Get().PositionAsString(pos);
}

class Ast {
 public:
  Ast() : default_module_{SourcePosition(), {}} {}

  std::vector<Declaration*>& declarations() {
    return default_module_.declarations;
  }
  const std::vector<Declaration*>& declarations() const {
    return default_module_.declarations;
  }
  void AddNode(std::unique_ptr<AstNode> node) {
    nodes_.emplace_back(std::move(node));
  }
  DefaultModuleDeclaration* default_module() { return &default_module_; }

 private:
  DefaultModuleDeclaration default_module_;
  std::vector<std::unique_ptr<AstNode>> nodes_;
};

struct IdentifierExpression : LocationExpression {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(IdentifierExpression)
  IdentifierExpression(SourcePosition p, std::string n,
                       std::vector<TypeExpression*> args)
      : LocationExpression(kKind, p),
        name(std::move(n)),
        generic_arguments(std::move(args)) {}
  std::string name;
  std::vector<TypeExpression*> generic_arguments;
};

struct CallExpression : Expression {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(CallExpression)
  CallExpression(SourcePosition p, std::string c, bool o,
                 std::vector<TypeExpression*> ga, std::vector<Expression*> a,
                 std::vector<std::string> l)
      : Expression(kKind, p),
        callee(p, std::move(c), std::move(ga)),
        is_operator(o),
        arguments(std::move(a)),
        labels(l) {}
  IdentifierExpression callee;
  bool is_operator;
  std::vector<Expression*> arguments;
  std::vector<std::string> labels;
};

struct StructExpression : Expression {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(StructExpression)
  StructExpression(SourcePosition p, std::string n, std::vector<Expression*> e)
      : Expression(kKind, p), name(n), expressions(std::move(e)) {}
  std::string name;
  std::vector<Expression*> expressions;
};

struct LogicalOrExpression : Expression {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(LogicalOrExpression)
  LogicalOrExpression(SourcePosition p, Expression* l, Expression* r)
      : Expression(kKind, p), left(l), right(r) {}
  Expression* left;
  Expression* right;
};

struct LogicalAndExpression : Expression {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(LogicalAndExpression)
  LogicalAndExpression(SourcePosition p, Expression* l, Expression* r)
      : Expression(kKind, p), left(l), right(r) {}
  Expression* left;
  Expression* right;
};

struct ConditionalExpression : Expression {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(ConditionalExpression)
  ConditionalExpression(SourcePosition p, Expression* c, Expression* t,
                        Expression* f)
      : Expression(kKind, p), condition(c), if_true(t), if_false(f) {}
  Expression* condition;
  Expression* if_true;
  Expression* if_false;
};

struct StringLiteralExpression : Expression {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(StringLiteralExpression)
  StringLiteralExpression(SourcePosition p, std::string l)
      : Expression(kKind, p), literal(std::move(l)) {}
  std::string literal;
};

struct NumberLiteralExpression : Expression {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(NumberLiteralExpression)
  NumberLiteralExpression(SourcePosition p, std::string n)
      : Expression(kKind, p), number(std::move(n)) {}
  std::string number;
};

struct ElementAccessExpression : LocationExpression {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(ElementAccessExpression)
  ElementAccessExpression(SourcePosition p, Expression* a, Expression* i)
      : LocationExpression(kKind, p), array(a), index(i) {}
  Expression* array;
  Expression* index;
};

struct FieldAccessExpression : LocationExpression {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(FieldAccessExpression)
  FieldAccessExpression(SourcePosition p, Expression* o, std::string f)
      : LocationExpression(kKind, p), object(o), field(std::move(f)) {}
  Expression* object;
  std::string field;
};

struct AssignmentExpression : Expression {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(AssignmentExpression)
  AssignmentExpression(SourcePosition p, LocationExpression* l,
                       base::Optional<std::string> o, Expression* v)
      : Expression(kKind, p), location(l), op(std::move(o)), value(v) {}
  LocationExpression* location;
  base::Optional<std::string> op;
  Expression* value;
};

enum class IncrementDecrementOperator { kIncrement, kDecrement };

struct IncrementDecrementExpression : Expression {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(IncrementDecrementExpression)
  IncrementDecrementExpression(SourcePosition p, LocationExpression* l,
                               IncrementDecrementOperator o, bool pf)
      : Expression(kKind, p), location(l), op(o), postfix(pf) {}
  LocationExpression* location;
  IncrementDecrementOperator op;
  bool postfix;
};

struct ParameterList {
  std::vector<std::string> names;
  std::vector<TypeExpression*> types;
  bool has_varargs;
  std::string arguments_variable;
};

struct BasicTypeExpression : TypeExpression {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(BasicTypeExpression)
  BasicTypeExpression(SourcePosition p, bool c, std::string n)
      : TypeExpression(kKind, p), is_constexpr(c), name(n) {}
  bool is_constexpr;
  std::string name;
};

struct FunctionTypeExpression : TypeExpression {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(FunctionTypeExpression)
  FunctionTypeExpression(SourcePosition p, ParameterList pl, TypeExpression* r)
      : TypeExpression(kKind, p), parameters(pl), return_type(r) {}
  ParameterList parameters;
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
  ExpressionStatement(SourcePosition p, Expression* e)
      : Statement(kKind, p), expression(e) {}
  Expression* expression;
};

struct IfStatement : Statement {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(IfStatement)
  IfStatement(SourcePosition p, Expression* c, bool cexpr, Statement* t,
              base::Optional<Statement*> f)
      : Statement(kKind, p),
        condition(c),
        is_constexpr(cexpr),
        if_true(t),
        if_false(f) {}
  Expression* condition;
  bool is_constexpr;
  Statement* if_true;
  base::Optional<Statement*> if_false;
};

struct WhileStatement : Statement {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(WhileStatement)
  WhileStatement(SourcePosition p, Expression* c, Statement* b)
      : Statement(kKind, p), condition(c), body(b) {}
  Expression* condition;
  Statement* body;
};

struct ReturnStatement : Statement {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(ReturnStatement)
  ReturnStatement(SourcePosition p, base::Optional<Expression*> v)
      : Statement(kKind, p), value(v) {}
  base::Optional<Expression*> value;
};

struct DebugStatement : Statement {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(DebugStatement)
  DebugStatement(SourcePosition p, const std::string& r, bool n)
      : Statement(kKind, p), reason(r), never_continues(n) {}
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
  TailCallStatement(SourcePosition p, CallExpression* c)
      : Statement(kKind, p), call(c) {}
  CallExpression* call;
};

struct VarDeclarationStatement : Statement {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(VarDeclarationStatement)
  VarDeclarationStatement(SourcePosition p, bool c, std::string n,
                          TypeExpression* t, base::Optional<Expression*> i)
      : Statement(kKind, p),
        const_qualified(c),
        name(std::move(n)),
        type(t),
        initializer(i) {}
  bool const_qualified;
  std::string name;
  TypeExpression* type;
  base::Optional<Expression*> initializer;
};

struct BreakStatement : Statement {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(BreakStatement)
  explicit BreakStatement(SourcePosition p) : Statement(kKind, p) {}
};

struct ContinueStatement : Statement {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(ContinueStatement)
  explicit ContinueStatement(SourcePosition p) : Statement(kKind, p) {}
};

struct GotoStatement : Statement {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(GotoStatement)
  GotoStatement(SourcePosition p, std::string l,
                const std::vector<Expression*>& a)
      : Statement(kKind, p), label(std::move(l)), arguments(std::move(a)) {}
  std::string label;
  std::vector<Expression*> arguments;
};

struct ForLoopStatement : Statement {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(ForLoopStatement)
  ForLoopStatement(SourcePosition p, base::Optional<VarDeclarationStatement*> d,
                   Expression* t, Expression* a, Statement* b)
      : Statement(kKind, p),
        var_declaration(d),
        test(std::move(t)),
        action(std::move(a)),
        body(std::move(b)) {}
  base::Optional<VarDeclarationStatement*> var_declaration;
  Expression* test;
  Expression* action;
  Statement* body;
};

struct ForOfLoopStatement : Statement {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(ForOfLoopStatement)
  ForOfLoopStatement(SourcePosition p, VarDeclarationStatement* d,
                     Expression* i, base::Optional<Expression*> bg,
                     base::Optional<Expression*> e, Statement* bd)
      : Statement(kKind, p),
        var_declaration(d),
        iterable(std::move(i)),
        begin(std::move(bg)),
        end(std::move(e)),
        body(std::move(bd)) {}
  VarDeclarationStatement* var_declaration;
  Expression* iterable;
  base::Optional<Expression*> begin;
  base::Optional<Expression*> end;
  Statement* body;
};

struct LabelBlock : AstNode {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(LabelBlock)
  LabelBlock(SourcePosition p, const std::string& l,
             const ParameterList& p_list, Statement* b)
      : AstNode(kKind, p),
        label(std::move(l)),
        parameters(p_list),
        body(std::move(b)) {}
  std::string label;
  ParameterList parameters;
  Statement* body;
};

struct TryLabelStatement : Statement {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(TryLabelStatement)
  TryLabelStatement(SourcePosition p, Statement* t)
      : Statement(kKind, p), try_block(std::move(t)) {}
  Statement* try_block;
  std::vector<LabelBlock*> label_blocks;
};

struct BlockStatement : Statement {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(BlockStatement)
  BlockStatement(SourcePosition p, bool d, std::vector<Statement*> s)
      : Statement(kKind, p), deferred(d), statements(std::move(s)) {}
  bool deferred;
  std::vector<Statement*> statements;
};

struct TypeDeclaration : Declaration {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(TypeDeclaration)
  TypeDeclaration(SourcePosition p, std::string n,
                  base::Optional<std::string> e, base::Optional<std::string> g)
      : Declaration(kKind, p),
        name(std::move(n)),
        extends(std::move(e)),
        generates(std::move(g)) {}
  std::string name;
  base::Optional<std::string> extends;
  base::Optional<std::string> generates;
  base::Optional<std::string> constexpr_generates;
};

struct TypeAliasDeclaration : Declaration {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(TypeAliasDeclaration)
  TypeAliasDeclaration(SourcePosition p, std::string n, TypeExpression* t)
      : Declaration(kKind, p), name(std::move(n)), type(t) {}
  std::string name;
  TypeExpression* type;
};

struct FieldNameAndType {
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
  CallableNode(AstNode::Kind kind, SourcePosition p, std::string n,
               ParameterList pl, TypeExpression* r,
               const LabelAndTypesVector& l)
      : AstNode(kind, p),
        name(std::move(n)),
        signature(new CallableNodeSignature{pl, r, l}) {}
  DEFINE_AST_NODE_INNER_BOILERPLATE(CallableNode)
  std::string name;
  std::unique_ptr<CallableNodeSignature> signature;
};

struct MacroDeclaration : CallableNode {
  DEFINE_AST_NODE_INNER_BOILERPLATE(MacroDeclaration)
  MacroDeclaration(AstNode::Kind kind, SourcePosition p, std::string n,
                   base::Optional<std::string> o, ParameterList pl,
                   TypeExpression* r, const LabelAndTypesVector& l)
      : CallableNode(kind, p, n, pl, r, l), op(std::move(o)) {}
  base::Optional<std::string> op;
};

struct ExternalMacroDeclaration : MacroDeclaration {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(ExternalMacroDeclaration)
  ExternalMacroDeclaration(SourcePosition p, std::string n,
                           base::Optional<std::string> o, ParameterList pl,
                           TypeExpression* r, const LabelAndTypesVector& l)
      : MacroDeclaration(kKind, p, n, o, pl, r, l) {}
};

struct TorqueMacroDeclaration : MacroDeclaration {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(TorqueMacroDeclaration)
  TorqueMacroDeclaration(SourcePosition p, std::string n, ParameterList pl,
                         TypeExpression* r, const LabelAndTypesVector& l)
      : MacroDeclaration(kKind, p, n, {}, pl, r, l) {}
};

struct BuiltinDeclaration : CallableNode {
  BuiltinDeclaration(AstNode::Kind kind, SourcePosition p, bool j,
                     std::string n, ParameterList pl, TypeExpression* r)
      : CallableNode(kind, p, n, pl, r, {}), javascript_linkage(j) {}
  bool javascript_linkage;
};

struct ExternalBuiltinDeclaration : BuiltinDeclaration {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(ExternalBuiltinDeclaration)
  ExternalBuiltinDeclaration(SourcePosition p, bool j, std::string n,
                             ParameterList pl, TypeExpression* r)
      : BuiltinDeclaration(kKind, p, j, n, pl, r) {}
};

struct TorqueBuiltinDeclaration : BuiltinDeclaration {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(TorqueBuiltinDeclaration)
  TorqueBuiltinDeclaration(SourcePosition p, bool j, std::string n,
                           ParameterList pl, TypeExpression* r)
      : BuiltinDeclaration(kKind, p, j, n, pl, r) {}
};

struct ExternalRuntimeDeclaration : CallableNode {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(ExternalRuntimeDeclaration)
  ExternalRuntimeDeclaration(SourcePosition p, std::string n, ParameterList pl,
                             TypeExpression* r)
      : CallableNode(kKind, p, n, pl, r, {}) {}
};

struct ConstDeclaration : Declaration {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(ConstDeclaration)
  ConstDeclaration(SourcePosition p, std::string n, TypeExpression* r,
                   Expression* e)
      : Declaration(kKind, p), name(std::move(n)), type(r), expression(e) {}
  std::string name;
  TypeExpression* type;
  Expression* expression;
};

struct StandardDeclaration : Declaration {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(StandardDeclaration)
  StandardDeclaration(SourcePosition p, CallableNode* c, Statement* b)
      : Declaration(kKind, p), callable(c), body(b) {}
  CallableNode* callable;
  Statement* body;
};

struct GenericDeclaration : Declaration {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(GenericDeclaration)
  GenericDeclaration(SourcePosition p, CallableNode* c,
                     std::vector<std::string> gp,
                     base::Optional<Statement*> b = base::nullopt)
      : Declaration(kKind, p),
        callable(c),
        generic_parameters(std::move(gp)),
        body(b) {}
  CallableNode* callable;
  std::vector<std::string> generic_parameters;
  base::Optional<Statement*> body;
};

struct SpecializationDeclaration : Declaration {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(SpecializationDeclaration)
  SpecializationDeclaration(SourcePosition p, std::string n, bool e,
                            std::vector<TypeExpression*> gp, ParameterList pl,
                            TypeExpression* r, LabelAndTypesVector l,
                            Statement* b)
      : Declaration(kKind, p),
        name(std::move(n)),
        external(e),
        generic_parameters(gp),
        signature(new CallableNodeSignature{pl, r, l}),
        body(b) {}
  std::string name;
  bool external;
  std::vector<TypeExpression*> generic_parameters;
  std::unique_ptr<CallableNodeSignature> signature;
  Statement* body;
};

struct ExternConstDeclaration : Declaration {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(ExternConstDeclaration)
  ExternConstDeclaration(SourcePosition p, std::string n, TypeExpression* t,
                         std::string l)
      : Declaration(kKind, p),
        name(std::move(n)),
        type(t),
        literal(std::move(l)) {}
  std::string name;
  TypeExpression* type;
  std::string literal;
};

struct StructDeclaration : Declaration {
  DEFINE_AST_NODE_LEAF_BOILERPLATE(StructDeclaration)
  StructDeclaration(SourcePosition p, std::string n)
      : Declaration(kKind, p), name(std::move(n)) {}
  std::string name;
  std::vector<FieldNameAndType> fields;
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
