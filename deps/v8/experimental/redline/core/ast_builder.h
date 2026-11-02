// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_REDLINE_CORE_AST_BUILDER_H_
#define V8_EXPERIMENTAL_REDLINE_CORE_AST_BUILDER_H_

#include <initializer_list>
#include <string_view>
#include <vector>

#include "experimental/redline/fast/ast.h"
#include "experimental/redline/fast/tree.h"

// Short factory functions for synthesizing FAST nodes. Every generator and
// mutator builds code through these, so generated code stays uniform (no
// source positions, names interned in the owning tree).
namespace redline {
namespace mk {

using fast::ArrayExpression;
using fast::ArrowFunctionExpression;
using fast::AssignmentExpression;
using fast::AssignOp;
using fast::AwaitExpression;
using fast::BigIntLiteral;
using fast::BinaryExpression;
using fast::BinaryOp;
using fast::BlockStatement;
using fast::BooleanLiteral;
using fast::BreakStatement;
using fast::CallExpression;
using fast::ChainExpression;
using fast::ClassDeclaration;
using fast::ClassExpression;
using fast::ConditionalExpression;
using fast::ContinueStatement;
using fast::DoWhileStatement;
using fast::EmptyStatement;
using fast::Expression;
using fast::ExpressionStatement;
using fast::ForInStatement;
using fast::ForOfStatement;
using fast::ForStatement;
using fast::FunctionDeclaration;
using fast::FunctionExpression;
using fast::Identifier;
using fast::IfStatement;
using fast::LabeledStatement;
using fast::LogicalExpression;
using fast::LogicalOp;
using fast::MemberExpression;
using fast::MetaProperty;
using fast::MethodDefinition;
using fast::MethodKind;
using fast::NativeCallExpression;
using fast::NewExpression;
using fast::Node;
using fast::NullLiteral;
using fast::NumericLiteral;
using fast::ObjectExpression;
using fast::PrivateIdentifier;
using fast::Property;
using fast::PropertyDefinition;
using fast::RegExpLiteral;
using fast::ReturnStatement;
using fast::SequenceExpression;
using fast::SpreadElement;
using fast::Statement;
using fast::StringLiteral;
using fast::SwitchCase;
using fast::SwitchStatement;
using fast::TemplateLiteral;
using fast::ThisExpression;
using fast::ThrowStatement;
using fast::Tree;
using fast::TryStatement;
using fast::UnaryExpression;
using fast::UnaryOp;
using fast::UpdateExpression;
using fast::UpdateOp;
using fast::VariableDeclaration;
using fast::VariableKind;
using fast::WhileStatement;
using fast::YieldExpression;

Identifier* Id(Tree* t, std::string_view name);
NumericLiteral* Num(Tree* t, double value);
StringLiteral* Str(Tree* t, std::string_view value);
BooleanLiteral* Bool(Tree* t, bool value);
NullLiteral* Null(Tree* t);
Identifier* Undefined(Tree* t);
BigIntLiteral* BigInt(Tree* t, std::string_view digits);  // without 'n'
RegExpLiteral* RegExp(Tree* t, std::string_view pattern,
                      std::string_view flags);
ThisExpression* This(Tree* t);
MetaProperty* NewTarget(Tree* t);
TemplateLiteral* Template(Tree* t, std::vector<std::string_view> strings,
                          std::vector<Expression*> expressions);

ArrayExpression* Array(Tree* t, std::vector<Expression*> elements);
ObjectExpression* Object(Tree* t, std::vector<Node*> properties);
Property* Prop(Tree* t, std::string_view key, Expression* value);
Property* Prop(Tree* t, Expression* key, Expression* value, bool computed);
Property* Accessor(Tree* t, std::string_view key, FunctionExpression* fn,
                   bool getter);
Property* Method(Tree* t, std::string_view key, FunctionExpression* fn);
SpreadElement* Spread(Tree* t, Expression* argument);

UnaryExpression* Unary(Tree* t, UnaryOp op, Expression* argument);
UpdateExpression* Update(Tree* t, UpdateOp op, bool prefix,
                         Expression* argument);
BinaryExpression* Binary(Tree* t, BinaryOp op, Expression* left,
                         Expression* right);
LogicalExpression* Logical(Tree* t, LogicalOp op, Expression* left,
                           Expression* right);
AssignmentExpression* Assign(Tree* t, Expression* target, Expression* value,
                             AssignOp op = AssignOp::kAssign);
ConditionalExpression* Cond(Tree* t, Expression* test, Expression* consequent,
                            Expression* alternate);
SequenceExpression* Seq(Tree* t, std::vector<Expression*> expressions);
MemberExpression* Member(Tree* t, Expression* object, std::string_view name);
// `object.#name` (name without the '#').
MemberExpression* PrivateMember(Tree* t, Expression* object,
                                std::string_view name);
MemberExpression* Index(Tree* t, Expression* object, Expression* index);
CallExpression* Call(Tree* t, Expression* callee,
                     std::vector<Expression*> args);
CallExpression* CallMethod(Tree* t, Expression* object, std::string_view name,
                           std::vector<Expression*> args);
NewExpression* New(Tree* t, Expression* callee, std::vector<Expression*> args);
NativeCallExpression* Native(Tree* t, std::string_view name,
                             std::vector<Expression*> args);
AwaitExpression* Await(Tree* t, Expression* argument);
YieldExpression* Yield(Tree* t, Expression* argument, bool delegate = false);
ChainExpression* Chain(Tree* t, Expression* expression);

// Functions. `body` statements are moved into a fresh block.
FunctionExpression* Function(Tree* t, std::string_view name,
                             std::vector<std::string_view> params,
                             std::vector<Statement*> body,
                             bool is_async = false, bool is_generator = false);
ArrowFunctionExpression* Arrow(Tree* t, std::vector<std::string_view> params,
                               std::vector<Statement*> body,
                               bool is_async = false);
ArrowFunctionExpression* ArrowExpr(Tree* t,
                                   std::vector<std::string_view> params,
                                   Expression* body, bool is_async = false);
FunctionDeclaration* FunctionDecl(Tree* t, std::string_view name,
                                  std::vector<std::string_view> params,
                                  std::vector<Statement*> body,
                                  bool is_async = false,
                                  bool is_generator = false);
MethodDefinition* ClassMethod(Tree* t, std::string_view name,
                              FunctionExpression* fn,
                              MethodKind kind = MethodKind::kMethod,
                              bool is_static = false);
PropertyDefinition* ClassField(Tree* t, std::string_view name,
                               Expression* value, bool is_static = false);
// The same with an explicit key node (a PrivateIdentifier for `#name`
// members, or any expression for computed keys).
MethodDefinition* ClassMethod(Tree* t, Expression* key, FunctionExpression* fn,
                              MethodKind kind, bool is_static);
PropertyDefinition* ClassField(Tree* t, Expression* key, Expression* value,
                               bool is_static);
PrivateIdentifier* PrivateId(Tree* t,
                             std::string_view name);  // name without the '#'
ClassDeclaration* ClassDecl(Tree* t, std::string_view name,
                            Expression* super_class,
                            std::vector<Node*> members);
ClassExpression* Class(Tree* t, std::string_view name, Expression* super_class,
                       std::vector<Node*> members);

// Statements.
ExpressionStatement* ExprStmt(Tree* t, Expression* expression);
VariableDeclaration* Declare(Tree* t, VariableKind kind, std::string_view name,
                             Expression* init);
VariableDeclaration* Let(Tree* t, std::string_view name, Expression* init);
VariableDeclaration* Const(Tree* t, std::string_view name, Expression* init);
VariableDeclaration* Var(Tree* t, std::string_view name, Expression* init);
BlockStatement* Block(Tree* t, std::vector<Statement*> body);
// Wraps a statement into a block unless it already is one.
BlockStatement* AsBlock(Tree* t, Statement* statement);
IfStatement* If(Tree* t, Expression* test, Statement* consequent,
                Statement* alternate = nullptr);
ReturnStatement* Return(Tree* t, Expression* argument);
ThrowStatement* Throw(Tree* t, Expression* argument);
// for (let i = 0; i < count; i++) body
ForStatement* CountedLoop(Tree* t, std::string_view counter, Expression* count,
                          Statement* body);
ForStatement* For(Tree* t, Node* init, Expression* test, Expression* update,
                  Statement* body);
WhileStatement* While(Tree* t, Expression* test, Statement* body);
DoWhileStatement* DoWhile(Tree* t, Statement* body, Expression* test);
ForOfStatement* ForOf(Tree* t, VariableKind kind, std::string_view name,
                      Expression* iterable, Statement* body,
                      bool is_await = false);
ForInStatement* ForIn(Tree* t, VariableKind kind, std::string_view name,
                      Expression* object, Statement* body);
TryStatement* Try(Tree* t, std::vector<Statement*> block,
                  std::string_view catch_param, std::vector<Statement*> handler,
                  std::vector<Statement*>* finalizer = nullptr);
SwitchStatement* Switch(Tree* t, Expression* discriminant,
                        std::vector<SwitchCase*> cases);
SwitchCase* Case(Tree* t, Expression* test, std::vector<Statement*> body);
BreakStatement* Break(Tree* t, std::string_view label = {});
ContinueStatement* Continue(Tree* t, std::string_view label = {});
LabeledStatement* Labeled(Tree* t, std::string_view label, Statement* body);
EmptyStatement* Empty(Tree* t);

// Sets the source position of every node in `node`'s subtree to
// [anchor, anchor] so that textual-order based analyses (TDZ
// approximation) treat synthesized code as located at `anchor`.
void Anchor(Node* node, int32_t anchor);

}  // namespace mk
}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_CORE_AST_BUILDER_H_
