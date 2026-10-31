// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/redline/core/ast_builder.h"

#include <string>

namespace redline {
namespace mk {

using namespace fast;  // NOLINT(build/namespaces)

Identifier* Id(Tree* t, std::string_view name) {
  Identifier* id = t->New<Identifier>();
  id->name = t->Intern(name);
  return id;
}

NumericLiteral* Num(Tree* t, double value) {
  NumericLiteral* n = t->New<NumericLiteral>();
  n->value = value;
  return n;
}

StringLiteral* Str(Tree* t, std::string_view value) {
  StringLiteral* s = t->New<StringLiteral>();
  s->value = std::string(value);
  return s;
}

BooleanLiteral* Bool(Tree* t, bool value) {
  BooleanLiteral* b = t->New<BooleanLiteral>();
  b->value = value;
  return b;
}

NullLiteral* Null(Tree* t) { return t->New<NullLiteral>(); }

Identifier* Undefined(Tree* t) { return Id(t, "undefined"); }

BigIntLiteral* BigInt(Tree* t, std::string_view digits) {
  BigIntLiteral* b = t->New<BigIntLiteral>();
  b->raw = t->Intern(std::string(digits) + "n");
  return b;
}

RegExpLiteral* RegExp(Tree* t, std::string_view pattern,
                      std::string_view flags) {
  RegExpLiteral* r = t->New<RegExpLiteral>();
  r->pattern = t->Intern(pattern);
  r->regexp_flags = t->Intern(flags);
  return r;
}

ThisExpression* This(Tree* t) { return t->New<ThisExpression>(); }

MetaProperty* NewTarget(Tree* t) {
  MetaProperty* m = t->New<MetaProperty>();
  m->meta_kind = MetaPropertyKind::kNewTarget;
  return m;
}

TemplateLiteral* Template(Tree* t, std::vector<std::string_view> strings,
                          std::vector<Expression*> expressions) {
  TemplateLiteral* tl = t->New<TemplateLiteral>();
  for (size_t i = 0; i < strings.size(); i++) {
    TemplateElement* e = t->New<TemplateElement>();
    e->raw = t->Intern(strings[i]);
    e->cooked = std::string(strings[i]);
    e->tail = i + 1 == strings.size();
    tl->quasis.push_back(e);
  }
  tl->expressions = std::move(expressions);
  return tl;
}

ArrayExpression* Array(Tree* t, std::vector<Expression*> elements) {
  ArrayExpression* a = t->New<ArrayExpression>();
  a->elements = std::move(elements);
  return a;
}

ObjectExpression* Object(Tree* t, std::vector<Node*> properties) {
  ObjectExpression* o = t->New<ObjectExpression>();
  o->properties = std::move(properties);
  return o;
}

Property* Prop(Tree* t, std::string_view key, Expression* value) {
  Property* p = t->New<Property>();
  p->key = Id(t, key);
  p->value = value;
  return p;
}

Property* Prop(Tree* t, Expression* key, Expression* value, bool computed) {
  Property* p = t->New<Property>();
  p->key = key;
  p->value = value;
  p->computed = computed;
  return p;
}

Property* Accessor(Tree* t, std::string_view key, FunctionExpression* fn,
                   bool getter) {
  Property* p = Prop(t, key, fn);
  p->property_kind = getter ? PropertyKind::kGet : PropertyKind::kSet;
  return p;
}

Property* Method(Tree* t, std::string_view key, FunctionExpression* fn) {
  Property* p = Prop(t, key, fn);
  p->method = true;
  return p;
}

SpreadElement* Spread(Tree* t, Expression* argument) {
  SpreadElement* s = t->New<SpreadElement>();
  s->argument = argument;
  return s;
}

UnaryExpression* Unary(Tree* t, UnaryOp op, Expression* argument) {
  UnaryExpression* u = t->New<UnaryExpression>();
  u->op = op;
  u->argument = argument;
  return u;
}

UpdateExpression* Update(Tree* t, UpdateOp op, bool prefix,
                         Expression* argument) {
  UpdateExpression* u = t->New<UpdateExpression>();
  u->op = op;
  u->prefix = prefix;
  u->argument = argument;
  return u;
}

BinaryExpression* Binary(Tree* t, BinaryOp op, Expression* left,
                         Expression* right) {
  BinaryExpression* b = t->New<BinaryExpression>();
  b->op = op;
  b->left = left;
  b->right = right;
  return b;
}

LogicalExpression* Logical(Tree* t, LogicalOp op, Expression* left,
                           Expression* right) {
  LogicalExpression* l = t->New<LogicalExpression>();
  l->op = op;
  l->left = left;
  l->right = right;
  return l;
}

AssignmentExpression* Assign(Tree* t, Expression* target, Expression* value,
                             AssignOp op) {
  AssignmentExpression* a = t->New<AssignmentExpression>();
  a->op = op;
  a->left = target;
  a->right = value;
  return a;
}

ConditionalExpression* Cond(Tree* t, Expression* test, Expression* consequent,
                            Expression* alternate) {
  ConditionalExpression* c = t->New<ConditionalExpression>();
  c->test = test;
  c->consequent = consequent;
  c->alternate = alternate;
  return c;
}

SequenceExpression* Seq(Tree* t, std::vector<Expression*> expressions) {
  SequenceExpression* s = t->New<SequenceExpression>();
  s->expressions = std::move(expressions);
  return s;
}

MemberExpression* Member(Tree* t, Expression* object, std::string_view name) {
  MemberExpression* m = t->New<MemberExpression>();
  m->object = object;
  m->property = Id(t, name);
  return m;
}

MemberExpression* Index(Tree* t, Expression* object, Expression* index) {
  MemberExpression* m = t->New<MemberExpression>();
  m->object = object;
  m->property = index;
  m->computed = true;
  return m;
}

CallExpression* Call(Tree* t, Expression* callee,
                     std::vector<Expression*> args) {
  CallExpression* c = t->New<CallExpression>();
  c->callee = callee;
  c->arguments = std::move(args);
  return c;
}

CallExpression* CallMethod(Tree* t, Expression* object, std::string_view name,
                           std::vector<Expression*> args) {
  return Call(t, Member(t, object, name), std::move(args));
}

NewExpression* New(Tree* t, Expression* callee, std::vector<Expression*> args) {
  NewExpression* n = t->New<NewExpression>();
  n->callee = callee;
  n->arguments = std::move(args);
  return n;
}

NativeCallExpression* Native(Tree* t, std::string_view name,
                             std::vector<Expression*> args) {
  NativeCallExpression* n = t->New<NativeCallExpression>();
  n->name = t->Intern(name);
  n->arguments = std::move(args);
  return n;
}

AwaitExpression* Await(Tree* t, Expression* argument) {
  AwaitExpression* a = t->New<AwaitExpression>();
  a->argument = argument;
  return a;
}

YieldExpression* Yield(Tree* t, Expression* argument, bool delegate) {
  YieldExpression* y = t->New<YieldExpression>();
  y->argument = argument;
  y->delegate = delegate;
  return y;
}

ChainExpression* Chain(Tree* t, Expression* expression) {
  ChainExpression* c = t->New<ChainExpression>();
  c->expression = expression;
  return c;
}

namespace {

void FillFunction(Tree* t, FunctionData* fn, std::string_view name,
                  std::vector<std::string_view>& params,
                  std::vector<Statement*>* body, bool is_async,
                  bool is_generator) {
  if (!name.empty()) {
    fn->name = Id(t, name);
  }
  for (std::string_view param : params) {
    fn->params.push_back(Id(t, param));
  }
  if (body != nullptr) {
    fn->body = Block(t, std::move(*body));
  }
  fn->is_async = is_async;
  fn->is_generator = is_generator;
}

}  // namespace

FunctionExpression* Function(Tree* t, std::string_view name,
                             std::vector<std::string_view> params,
                             std::vector<Statement*> body, bool is_async,
                             bool is_generator) {
  FunctionExpression* fn = t->New<FunctionExpression>();
  FillFunction(t, fn, name, params, &body, is_async, is_generator);
  return fn;
}

ArrowFunctionExpression* Arrow(Tree* t, std::vector<std::string_view> params,
                               std::vector<Statement*> body, bool is_async) {
  ArrowFunctionExpression* fn = t->New<ArrowFunctionExpression>();
  FillFunction(t, fn, {}, params, &body, is_async, false);
  fn->is_arrow = true;
  return fn;
}

ArrowFunctionExpression* ArrowExpr(Tree* t,
                                   std::vector<std::string_view> params,
                                   Expression* body, bool is_async) {
  ArrowFunctionExpression* fn = t->New<ArrowFunctionExpression>();
  FillFunction(t, fn, {}, params, nullptr, is_async, false);
  fn->is_arrow = true;
  fn->expression_body = body;
  return fn;
}

FunctionDeclaration* FunctionDecl(Tree* t, std::string_view name,
                                  std::vector<std::string_view> params,
                                  std::vector<Statement*> body, bool is_async,
                                  bool is_generator) {
  FunctionDeclaration* fn = t->New<FunctionDeclaration>();
  FillFunction(t, fn, name, params, &body, is_async, is_generator);
  return fn;
}

MethodDefinition* ClassMethod(Tree* t, std::string_view name,
                              FunctionExpression* fn, MethodKind kind,
                              bool is_static) {
  return ClassMethod(t, Id(t, name), fn, kind, is_static);
}

PropertyDefinition* ClassField(Tree* t, std::string_view name,
                               Expression* value, bool is_static) {
  return ClassField(t, Id(t, name), value, is_static);
}

MethodDefinition* ClassMethod(Tree* t, Expression* key, FunctionExpression* fn,
                              MethodKind kind, bool is_static) {
  MethodDefinition* m = t->New<MethodDefinition>();
  m->key = key;
  m->value = fn;
  m->method_kind = kind;
  m->is_static = is_static;
  return m;
}

PropertyDefinition* ClassField(Tree* t, Expression* key, Expression* value,
                               bool is_static) {
  PropertyDefinition* p = t->New<PropertyDefinition>();
  p->key = key;
  p->value = value;
  p->is_static = is_static;
  return p;
}

PrivateIdentifier* PrivateId(Tree* t, std::string_view name) {
  PrivateIdentifier* p = t->New<PrivateIdentifier>();
  p->name = t->Intern(name);
  return p;
}

MemberExpression* PrivateMember(Tree* t, Expression* object,
                                std::string_view name) {
  MemberExpression* m = t->New<MemberExpression>();
  m->object = object;
  m->property = PrivateId(t, name);
  return m;
}

namespace {

void FillClass(Tree* t, ClassData* cls, std::string_view name,
               Expression* super_class, std::vector<Node*>& members) {
  if (!name.empty()) {
    cls->name = Id(t, name);
  }
  cls->super_class = super_class;
  cls->body = t->New<ClassBody>();
  cls->body->members = std::move(members);
}

}  // namespace

ClassDeclaration* ClassDecl(Tree* t, std::string_view name,
                            Expression* super_class,
                            std::vector<Node*> members) {
  ClassDeclaration* cls = t->New<ClassDeclaration>();
  FillClass(t, cls, name, super_class, members);
  return cls;
}

ClassExpression* Class(Tree* t, std::string_view name, Expression* super_class,
                       std::vector<Node*> members) {
  ClassExpression* cls = t->New<ClassExpression>();
  FillClass(t, cls, name, super_class, members);
  return cls;
}

ExpressionStatement* ExprStmt(Tree* t, Expression* expression) {
  ExpressionStatement* s = t->New<ExpressionStatement>();
  s->expression = expression;
  return s;
}

VariableDeclaration* Declare(Tree* t, VariableKind kind, std::string_view name,
                             Expression* init) {
  VariableDeclaration* d = t->New<VariableDeclaration>();
  d->declaration_kind = kind;
  VariableDeclarator* declarator = t->New<VariableDeclarator>();
  declarator->target = Id(t, name);
  declarator->init = init;
  d->declarations.push_back(declarator);
  return d;
}

VariableDeclaration* Let(Tree* t, std::string_view name, Expression* init) {
  return Declare(t, VariableKind::kLet, name, init);
}
VariableDeclaration* Const(Tree* t, std::string_view name, Expression* init) {
  return Declare(t, VariableKind::kConst, name, init);
}
VariableDeclaration* Var(Tree* t, std::string_view name, Expression* init) {
  return Declare(t, VariableKind::kVar, name, init);
}

BlockStatement* Block(Tree* t, std::vector<Statement*> body) {
  BlockStatement* b = t->New<BlockStatement>();
  b->body = std::move(body);
  return b;
}

BlockStatement* AsBlock(Tree* t, Statement* statement) {
  if (auto* block = statement->As<BlockStatement>()) {
    return block;
  }
  return Block(t, {statement});
}

IfStatement* If(Tree* t, Expression* test, Statement* consequent,
                Statement* alternate) {
  IfStatement* s = t->New<IfStatement>();
  s->test = test;
  s->consequent = consequent;
  s->alternate = alternate;
  return s;
}

ReturnStatement* Return(Tree* t, Expression* argument) {
  ReturnStatement* s = t->New<ReturnStatement>();
  s->argument = argument;
  return s;
}

ThrowStatement* Throw(Tree* t, Expression* argument) {
  ThrowStatement* s = t->New<ThrowStatement>();
  s->argument = argument;
  return s;
}

ForStatement* For(Tree* t, Node* init, Expression* test, Expression* update,
                  Statement* body) {
  ForStatement* s = t->New<ForStatement>();
  s->init = init;
  s->test = test;
  s->update = update;
  s->body = body;
  return s;
}

ForStatement* CountedLoop(Tree* t, std::string_view counter, Expression* count,
                          Statement* body) {
  return For(t, Let(t, counter, Num(t, 0)),
             Binary(t, BinaryOp::kLessThan, Id(t, counter), count),
             Update(t, UpdateOp::kIncrement, false, Id(t, counter)), body);
}

WhileStatement* While(Tree* t, Expression* test, Statement* body) {
  WhileStatement* s = t->New<WhileStatement>();
  s->test = test;
  s->body = body;
  return s;
}

DoWhileStatement* DoWhile(Tree* t, Statement* body, Expression* test) {
  DoWhileStatement* s = t->New<DoWhileStatement>();
  s->body = body;
  s->test = test;
  return s;
}

ForOfStatement* ForOf(Tree* t, VariableKind kind, std::string_view name,
                      Expression* iterable, Statement* body, bool is_await) {
  ForOfStatement* s = t->New<ForOfStatement>();
  s->left = Declare(t, kind, name, nullptr);
  s->right = iterable;
  s->body = body;
  s->is_await = is_await;
  return s;
}

ForInStatement* ForIn(Tree* t, VariableKind kind, std::string_view name,
                      Expression* object, Statement* body) {
  ForInStatement* s = t->New<ForInStatement>();
  s->left = Declare(t, kind, name, nullptr);
  s->right = object;
  s->body = body;
  return s;
}

TryStatement* Try(Tree* t, std::vector<Statement*> block,
                  std::string_view catch_param, std::vector<Statement*> handler,
                  std::vector<Statement*>* finalizer) {
  TryStatement* s = t->New<TryStatement>();
  s->block = Block(t, std::move(block));
  CatchClause* clause = t->New<CatchClause>();
  if (!catch_param.empty()) {
    clause->param = Id(t, catch_param);
  }
  clause->body = Block(t, std::move(handler));
  s->handler = clause;
  if (finalizer != nullptr) {
    s->finalizer = Block(t, std::move(*finalizer));
  }
  return s;
}

SwitchStatement* Switch(Tree* t, Expression* discriminant,
                        std::vector<SwitchCase*> cases) {
  SwitchStatement* s = t->New<SwitchStatement>();
  s->discriminant = discriminant;
  s->cases = std::move(cases);
  return s;
}

SwitchCase* Case(Tree* t, Expression* test, std::vector<Statement*> body) {
  SwitchCase* c = t->New<SwitchCase>();
  c->test = test;
  c->consequent = std::move(body);
  return c;
}

BreakStatement* Break(Tree* t, std::string_view label) {
  BreakStatement* s = t->New<BreakStatement>();
  if (!label.empty()) {
    s->label = Id(t, label);
  }
  return s;
}

ContinueStatement* Continue(Tree* t, std::string_view label) {
  ContinueStatement* s = t->New<ContinueStatement>();
  if (!label.empty()) {
    s->label = Id(t, label);
  }
  return s;
}

LabeledStatement* Labeled(Tree* t, std::string_view label, Statement* body) {
  LabeledStatement* s = t->New<LabeledStatement>();
  s->label = Id(t, label);
  s->body = body;
  return s;
}

EmptyStatement* Empty(Tree* t) { return t->New<EmptyStatement>(); }

void Anchor(Node* node, int32_t anchor) {
  Walk(node, [anchor](Node* n) {
    n->start = anchor;
    n->end = anchor;
    return true;
  });
}

}  // namespace mk
}  // namespace redline
