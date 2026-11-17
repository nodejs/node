// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/redline/fast/scope.h"

#include <algorithm>
#include <functional>

#include "experimental/redline/base/log.h"
#include "experimental/redline/fast/tree.h"

namespace redline {
namespace fast {

const char* ScopeKindName(ScopeKind kind) {
  switch (kind) {
    case ScopeKind::kScript:
      return "script";
    case ScopeKind::kFunction:
      return "function";
    case ScopeKind::kArrow:
      return "arrow";
    case ScopeKind::kFunctionBody:
      return "function-body";
    case ScopeKind::kBlock:
      return "block";
    case ScopeKind::kCatch:
      return "catch";
    case ScopeKind::kClass:
      return "class";
    case ScopeKind::kStaticBlock:
      return "static-block";
    case ScopeKind::kFieldInit:
      return "field-init";
    case ScopeKind::kWith:
      return "with";
  }
  return "?";
}

const char* BindingKindName(BindingKind kind) {
  switch (kind) {
    case BindingKind::kVar:
      return "var";
    case BindingKind::kLet:
      return "let";
    case BindingKind::kConst:
      return "const";
    case BindingKind::kUsing:
      return "using";
    case BindingKind::kAwaitUsing:
      return "await-using";
    case BindingKind::kFunction:
      return "function";
    case BindingKind::kClass:
      return "class";
    case BindingKind::kParameter:
      return "param";
    case BindingKind::kCatchParameter:
      return "catch-param";
    case BindingKind::kFunctionName:
      return "function-name";
    case BindingKind::kClassName:
      return "class-name";
    case BindingKind::kImplicit:
      return "implicit";
  }
  return "?";
}

bool Binding::safe_to_rename() const {
  if (kind == BindingKind::kImplicit) {
    return false;
  }
  if (pinned || has_dynamic_reference) {
    return false;
  }
  if (scope == nullptr) {
    return false;
  }
  if (scope->is_dynamic() || scope->dynamic_inside) {
    return false;
  }
  // Declaring `arguments` changes whether and where the arguments object
  // exists; leave the name alone in either direction.
  if (name == "arguments") {
    return false;
  }
  // Script-level bindings are reachable by name from outside the program
  // text: other scripts, `new Function` bodies, indirect eval, and for
  // `var`/function declarations also as properties of the global object.
  if (scope->kind == ScopeKind::kScript) {
    return false;
  }
  return true;
}

bool Scope::is_dynamic() const {
  for (const Scope* s = this; s != nullptr; s = s->parent) {
    if (s->kind == ScopeKind::kWith || s->has_direct_eval) {
      return true;
    }
  }
  return false;
}

Scope* Scope::var_scope() {
  Scope* s = this;
  while (!s->is_var_scope() && s->parent != nullptr) {
    s = s->parent;
  }
  return s;
}

const Scope* Scope::var_scope() const {
  return const_cast<Scope*>(this)->var_scope();
}

const Scope* Scope::this_scope() const {
  for (const Scope* s = this; s != nullptr; s = s->parent) {
    if (s->kind == ScopeKind::kFunction || s->kind == ScopeKind::kStaticBlock ||
        s->kind == ScopeKind::kFieldInit) {
      return s;
    }
  }
  return nullptr;
}

Binding* Scope::Lookup(std::string_view name) const {
  for (const Scope* s = this; s != nullptr; s = s->parent) {
    auto it = s->by_name.find(name);
    if (it != s->by_name.end()) {
      return it->second;
    }
  }
  return nullptr;
}

int Scope::Depth() const {
  int depth = 0;
  for (const Scope* s = parent; s != nullptr; s = s->parent) {
    depth++;
  }
  return depth;
}

void CollectBoundIdentifiers(Node* pattern, std::vector<Identifier*>* out) {
  if (pattern == nullptr) {
    return;
  }
  switch (pattern->kind) {
    case NodeKind::kIdentifier:
      out->push_back(pattern->Cast<Identifier>());
      return;
    case NodeKind::kObjectPattern:
      for (Node* property : pattern->Cast<ObjectPattern>()->properties) {
        if (auto* p = property->As<Property>()) {
          CollectBoundIdentifiers(p->value, out);
        } else {
          CollectBoundIdentifiers(property, out);
        }
      }
      return;
    case NodeKind::kArrayPattern:
      for (Expression* element : pattern->Cast<ArrayPattern>()->elements) {
        CollectBoundIdentifiers(element, out);
      }
      return;
    case NodeKind::kAssignmentPattern:
      CollectBoundIdentifiers(pattern->Cast<AssignmentPattern>()->left, out);
      return;
    case NodeKind::kRestElement:
      CollectBoundIdentifiers(pattern->Cast<RestElement>()->argument, out);
      return;
    default:
      return;
  }
}

// ---------------------------------------------------------------------------

class ScopeBuilder {
 public:
  explicit ScopeBuilder(ScopeAnalysis* analysis) : analysis_(analysis) {}

  void Build(Tree* tree) {
    Program* program = tree->root();
    Scope* root = NewScope(ScopeKind::kScript, program);
    root->strict = program->strict;
    analysis_->root_ = root;
    current_ = root;
    for (Statement* statement : program->body) {
      VisitStatement(statement);
    }
    Resolve();
    PinAnnexBFunctions();
    for (Identifier* identifier : names_function_ids_) {
      if (Binding* binding = analysis_->BindingOf(identifier)) {
        binding->names_function = true;
      }
    }
    FillMissingScopes(program);
    PropagateDynamic(root);
  }

 private:
  struct PendingReference {
    Identifier* identifier;
    Scope* scope;
    bool is_write;
  };

  // --- scopes and bindings ---

  Scope* NewScope(ScopeKind kind, Node* node) {
    Scope* scope = analysis_->NewScope();
    scope->kind = kind;
    scope->node = node;
    scope->parent = current_;
    scope->start = node->start;
    scope->end = node->end;
    if (current_ != nullptr) {
      scope->strict = current_->strict;
      scope->is_async = current_->is_async;
      scope->is_generator = current_->is_generator;
      current_->children.push_back(scope);
    }
    if (node != nullptr) {
      analysis_->SetScopeOf(node, scope);
    }
    return scope;
  }

  struct ScopeGuard {
    ScopeGuard(ScopeBuilder* builder, Scope* scope)
        : builder_(builder), saved_(builder->current_) {
      builder_->current_ = scope;
    }
    ~ScopeGuard() { builder_->current_ = saved_; }
    ScopeBuilder* builder_;
    Scope* saved_;
  };

  void Enter(Node* node) {
    if (node != nullptr) {
      analysis_->SetScopeIfUnset(node, current_);
    }
  }

  void NoteName(std::string_view name) { analysis_->all_names_.insert(name); }

  Binding* Declare(Identifier* identifier, BindingKind kind, Node* declaration,
                   Scope* scope) {
    NoteName(identifier->name);
    analysis_->AddFlag(identifier, ScopeAnalysis::IdFlag::kIsDeclaration);
    if (kind == BindingKind::kVar) {
      if (Binding* merged = VarMergeTarget(identifier->name, scope)) {
        merged->declarations.push_back(identifier);
        analysis_->SetBindingOf(identifier, merged);
        PinCatchParameterRedeclaration(merged);
        return merged;
      }
    }
    Binding* result = nullptr;
    auto it = scope->by_name.find(identifier->name);
    if (it != scope->by_name.end()) {
      Binding* existing = it->second;
      bool existing_is_inner_name =
          existing->kind == BindingKind::kFunctionName ||
          existing->kind == BindingKind::kClassName;
      if (!existing_is_inner_name) {
        // Redeclaration (`var x; var x;`, or an early error we tolerate):
        // one binding, several declaration sites. `var x; function x(){}`
        // is a function binding (its name is observable).
        existing->declarations.push_back(identifier);
        if (kind == BindingKind::kFunction &&
            existing->kind == BindingKind::kVar) {
          existing->kind = BindingKind::kFunction;
        }
        analysis_->SetBindingOf(identifier, existing);
        result = existing;
      }
      // Parameters and body declarations shadow a function expression's
      // own name; keep the inner-name binding around but hide it.
    }
    if (result == nullptr) {
      Binding* binding = analysis_->NewBinding();
      binding->name = identifier->name;
      binding->kind = kind;
      binding->scope = scope;
      binding->identifier = identifier;
      binding->declaration = declaration;
      binding->declarations.push_back(identifier);
      result = binding;
      scope->bindings.push_back(result);
      scope->by_name[result->name] = result;
      analysis_->SetBindingOf(identifier, result);
    }
    if (kind == BindingKind::kVar) {
      PinCatchParameterRedeclaration(result);
    }
    return result;
  }

  // A `var` does not always create a binding of its own:
  // - in the separate body scope of a function with non-simple parameters,
  //   `var x` where `x` is a parameter creates a body binding that is
  //   initialized from the parameter; treating both as one binding keeps
  //   renaming sound (closures in parameter defaults do not observe body
  //   writes, which a later dataflow analysis may want to model);
  // - `var arguments` in a function keeps the implicit arguments object.
  Binding* VarMergeTarget(std::string_view name, Scope* var_scope) {
    Scope* function_scope = var_scope;
    if (var_scope->kind == ScopeKind::kFunctionBody) {
      function_scope = var_scope->parent;
      auto it = function_scope->by_name.find(name);
      if (it != function_scope->by_name.end() &&
          it->second->kind == BindingKind::kParameter) {
        // Unless a function declaration in the body owns the name: then
        // the body binding starts out as that function, not the parameter.
        auto names = body_function_names_.find(var_scope);
        if (names == body_function_names_.end() || !names->second.count(name)) {
          return it->second;
        }
      }
    }
    if (name == "arguments" && function_scope->kind == ScopeKind::kFunction) {
      auto it = function_scope->by_name.find(name);
      if (it == function_scope->by_name.end()) {
        return ImplicitArguments(function_scope);
      }
      if (it->second->kind == BindingKind::kImplicit) {
        return it->second;
      }
    }
    return nullptr;
  }

  // Annex B.3.4: `var e` inside `catch (e)` is allowed in sloppy and strict
  // code alike; the declaration hoists as usual but its initializer assigns
  // the catch parameter. Neither binding can be renamed on its own.
  void PinCatchParameterRedeclaration(Binding* var_binding) {
    for (Scope* s = current_; s != nullptr && s != var_binding->scope;
         s = s->parent) {
      if (s->kind != ScopeKind::kCatch) {
        continue;
      }
      auto it = s->by_name.find(var_binding->name);
      if (it != s->by_name.end() &&
          it->second->kind == BindingKind::kCatchParameter) {
        it->second->pinned = true;
        var_binding->pinned = true;
      }
    }
  }

  void Reference(Identifier* identifier, bool is_write) {
    NoteName(identifier->name);
    Enter(identifier);
    analysis_->AddFlag(identifier, ScopeAnalysis::IdFlag::kIsReference);
    pending_.push_back({identifier, current_, is_write});
  }

  // --- statements ---

  void VisitStatements(const std::vector<Statement*>& statements) {
    for (Statement* statement : statements) {
      VisitStatement(statement);
    }
  }

  void VisitBlockBody(BlockStatement* block) {
    // The statements of a block whose scope was already created.
    Enter(block);
    VisitStatements(block->body);
  }

  void VisitBlock(BlockStatement* block) {
    Scope* scope = NewScope(ScopeKind::kBlock, block);
    ScopeGuard guard(this, scope);
    VisitStatements(block->body);
  }

  void VisitStatement(Statement* statement) {
    if (statement == nullptr) {
      return;
    }
    Enter(statement);
    switch (statement->kind) {
      case NodeKind::kExpressionStatement:
        VisitExpression(statement->Cast<ExpressionStatement>()->expression);
        break;
      case NodeKind::kBlockStatement:
        VisitBlock(statement->Cast<BlockStatement>());
        break;
      case NodeKind::kEmptyStatement:
      case NodeKind::kDebuggerStatement:
        break;
      case NodeKind::kWithStatement: {
        auto* s = statement->Cast<WithStatement>();
        VisitExpression(s->object);
        Scope* scope = NewScope(ScopeKind::kWith, s);
        ScopeGuard guard(this, scope);
        VisitStatement(s->body);
        break;
      }
      case NodeKind::kReturnStatement:
        VisitExpression(statement->Cast<ReturnStatement>()->argument);
        break;
      case NodeKind::kThrowStatement:
        VisitExpression(statement->Cast<ThrowStatement>()->argument);
        break;
      case NodeKind::kLabeledStatement: {
        auto* s = statement->Cast<LabeledStatement>();
        NoteName(s->label->name);
        Enter(s->label);
        VisitStatement(s->body);
        break;
      }
      case NodeKind::kBreakStatement: {
        auto* s = statement->Cast<BreakStatement>();
        if (s->label != nullptr) {
          NoteName(s->label->name);
          Enter(s->label);
        }
        break;
      }
      case NodeKind::kContinueStatement: {
        auto* s = statement->Cast<ContinueStatement>();
        if (s->label != nullptr) {
          NoteName(s->label->name);
          Enter(s->label);
        }
        break;
      }
      case NodeKind::kIfStatement: {
        auto* s = statement->Cast<IfStatement>();
        VisitExpression(s->test);
        VisitStatement(s->consequent);
        VisitStatement(s->alternate);
        break;
      }
      case NodeKind::kSwitchStatement: {
        auto* s = statement->Cast<SwitchStatement>();
        VisitExpression(s->discriminant);
        Scope* scope = NewScope(ScopeKind::kBlock, s);
        ScopeGuard guard(this, scope);
        for (SwitchCase* switch_case : s->cases) {
          Enter(switch_case);
          VisitExpression(switch_case->test);
          VisitStatements(switch_case->consequent);
        }
        break;
      }
      case NodeKind::kTryStatement: {
        auto* s = statement->Cast<TryStatement>();
        VisitBlock(s->block);
        if (s->handler != nullptr) {
          Scope* scope = NewScope(ScopeKind::kCatch, s->handler);
          ScopeGuard guard(this, scope);
          if (s->handler->param != nullptr) {
            VisitBindingPattern(s->handler->param, BindingKind::kCatchParameter,
                                s->handler);
          }
          VisitBlock(s->handler->body);
        }
        if (s->finalizer != nullptr) {
          VisitBlock(s->finalizer);
        }
        break;
      }
      case NodeKind::kWhileStatement: {
        auto* s = statement->Cast<WhileStatement>();
        VisitExpression(s->test);
        VisitStatement(s->body);
        break;
      }
      case NodeKind::kDoWhileStatement: {
        auto* s = statement->Cast<DoWhileStatement>();
        VisitStatement(s->body);
        VisitExpression(s->test);
        break;
      }
      case NodeKind::kForStatement: {
        auto* s = statement->Cast<ForStatement>();
        Scope* scope = HasLexicalDeclaration(s->init)
                           ? NewScope(ScopeKind::kBlock, s)
                           : current_;
        ScopeGuard guard(this, scope);
        if (s->init != nullptr) {
          if (auto* declaration = s->init->As<VariableDeclaration>()) {
            VisitVariableDeclaration(declaration);
          } else {
            VisitExpression(s->init->Cast<Expression>());
          }
        }
        VisitExpression(s->test);
        VisitExpression(s->update);
        VisitStatement(s->body);
        break;
      }
      case NodeKind::kForInStatement: {
        auto* s = statement->Cast<ForInStatement>();
        VisitForEach(s, s->left, s->right, s->body);
        break;
      }
      case NodeKind::kForOfStatement: {
        auto* s = statement->Cast<ForOfStatement>();
        VisitForEach(s, s->left, s->right, s->body);
        break;
      }
      case NodeKind::kFunctionDeclaration: {
        auto* fn = statement->Cast<FunctionDeclaration>();
        if (fn->name != nullptr) {
          Binding* binding =
              Declare(fn->name, BindingKind::kFunction, fn, current_);
          Enter(fn->name);
          // Annex B.3.3: in sloppy code a plain function declared in a block
          // is also hoisted as a `var` to the enclosing function scope,
          // unless a lexical declaration of the same name is in the way.
          if (!current_->strict && !current_->is_var_scope() && !fn->is_async &&
              !fn->is_generator) {
            binding->pinned = true;
            annex_b_functions_.push_back(
                {current_->var_scope(), fn->name->name});
          }
        }
        VisitFunction(fn, fn, /*is_arrow=*/false, /*is_expression=*/false);
        break;
      }
      case NodeKind::kVariableDeclaration:
        VisitVariableDeclaration(statement->Cast<VariableDeclaration>());
        break;
      case NodeKind::kClassDeclaration: {
        auto* cls = statement->Cast<ClassDeclaration>();
        if (cls->name != nullptr) {
          Declare(cls->name, BindingKind::kClass, cls, current_);
          Enter(cls->name);
        }
        VisitClass(cls, cls, /*is_expression=*/false);
        break;
      }
      default:
        REDLINE_WARN("scope", "Unhandled statement kind %s",
                     NodeKindName(statement->kind));
        break;
    }
  }

  static bool HasLexicalDeclaration(Node* node) {
    auto* declaration =
        node == nullptr ? nullptr : node->As<VariableDeclaration>();
    return declaration != nullptr &&
           declaration->declaration_kind != VariableKind::kVar;
  }

  void VisitForEach(Statement* loop, Node* left, Expression* right,
                    Statement* body) {
    Scope* scope = HasLexicalDeclaration(left)
                       ? NewScope(ScopeKind::kBlock, loop)
                       : current_;
    ScopeGuard guard(this, scope);
    if (auto* declaration = left->As<VariableDeclaration>()) {
      VisitVariableDeclaration(declaration);
    } else {
      VisitAssignmentTarget(left->Cast<Expression>());
    }
    VisitExpression(right);
    VisitStatement(body);
  }

  void VisitVariableDeclaration(VariableDeclaration* declaration) {
    Enter(declaration);
    BindingKind kind = BindingKind::kVar;
    switch (declaration->declaration_kind) {
      case VariableKind::kVar:
        kind = BindingKind::kVar;
        break;
      case VariableKind::kLet:
        kind = BindingKind::kLet;
        break;
      case VariableKind::kConst:
        kind = BindingKind::kConst;
        break;
      case VariableKind::kUsing:
        kind = BindingKind::kUsing;
        break;
      case VariableKind::kAwaitUsing:
        kind = BindingKind::kAwaitUsing;
        break;
    }
    for (VariableDeclarator* declarator : declaration->declarations) {
      Enter(declarator);
      VisitBindingPattern(declarator->target, kind, declarator);
      VisitExpression(declarator->init);
      NoteNameInference(declarator->target, declarator->init);
    }
  }

  // `x = function() {}`, `let x = class {}`, `[x = () => {}] = a`: the
  // function's `name` property is taken from the binding.
  static bool IsAnonymousFunctionDefinition(Expression* e) {
    if (e == nullptr) {
      return false;
    }
    switch (e->kind) {
      case NodeKind::kArrowFunctionExpression:
        return true;
      case NodeKind::kFunctionExpression:
        return e->Cast<FunctionExpression>()->name == nullptr;
      case NodeKind::kClassExpression:
        return e->Cast<ClassExpression>()->name == nullptr;
      default:
        return false;
    }
  }

  void NoteNameInference(Node* target, Expression* value) {
    if (target == nullptr || target->kind != NodeKind::kIdentifier) {
      return;
    }
    if (!IsAnonymousFunctionDefinition(value)) {
      return;
    }
    names_function_ids_.push_back(target->Cast<Identifier>());
  }

  // Declares the identifiers of a binding pattern; default values and
  // computed keys are expressions evaluated in the current scope.
  void VisitBindingPattern(Node* pattern, BindingKind kind, Node* declaration) {
    if (pattern == nullptr) {
      return;
    }
    Enter(pattern);
    switch (pattern->kind) {
      case NodeKind::kIdentifier: {
        Scope* scope =
            kind == BindingKind::kVar ? current_->var_scope() : current_;
        Declare(pattern->Cast<Identifier>(), kind, declaration, scope);
        break;
      }
      case NodeKind::kObjectPattern:
        for (Node* property : pattern->Cast<ObjectPattern>()->properties) {
          Enter(property);
          if (auto* p = property->As<Property>()) {
            VisitPropertyKey(p->key, p->computed);
            VisitBindingPattern(p->value, kind, declaration);
          } else {
            VisitBindingPattern(property, kind, declaration);
          }
        }
        break;
      case NodeKind::kArrayPattern:
        for (Expression* element : pattern->Cast<ArrayPattern>()->elements) {
          VisitBindingPattern(element, kind, declaration);
        }
        break;
      case NodeKind::kAssignmentPattern: {
        auto* p = pattern->Cast<AssignmentPattern>();
        VisitBindingPattern(p->left, kind, declaration);
        VisitExpression(p->right);
        NoteNameInference(p->left, p->right);
        break;
      }
      case NodeKind::kRestElement:
        VisitBindingPattern(pattern->Cast<RestElement>()->argument, kind,
                            declaration);
        break;
      default:
        // Not a pattern (e.g. a MemberExpression in a sloppy for-in head
        // that the parser let through): treat as an assignment target.
        VisitAssignmentTarget(pattern->Cast<Expression>());
        break;
    }
  }

  // --- expressions ---

  void VisitPropertyKey(Expression* key, bool computed) {
    if (key == nullptr) {
      return;
    }
    if (computed) {
      VisitExpression(key);
      return;
    }
    Enter(key);
    if (auto* id = key->As<Identifier>()) {
      NoteName(id->name);
    }
    if (auto* id = key->As<PrivateIdentifier>()) {
      NoteName(id->name);
    }
  }

  void VisitExpressions(const std::vector<Expression*>& expressions) {
    for (Expression* expression : expressions) {
      VisitExpression(expression);
    }
  }

  // An expression in assignment-target position: identifiers are writes,
  // patterns bind nothing (they assign to existing targets).
  void VisitAssignmentTarget(Expression* target) {
    if (target == nullptr) {
      return;
    }
    Enter(target);
    switch (target->kind) {
      case NodeKind::kIdentifier:
        Reference(target->Cast<Identifier>(), /*is_write=*/true);
        break;
      case NodeKind::kObjectPattern:
        for (Node* property : target->Cast<ObjectPattern>()->properties) {
          Enter(property);
          if (auto* p = property->As<Property>()) {
            VisitPropertyKey(p->key, p->computed);
            VisitAssignmentTarget(p->value);
          } else {
            VisitAssignmentTarget(property->Cast<Expression>());
          }
        }
        break;
      case NodeKind::kArrayPattern:
        for (Expression* element : target->Cast<ArrayPattern>()->elements) {
          VisitAssignmentTarget(element);
        }
        break;
      case NodeKind::kAssignmentPattern: {
        auto* p = target->Cast<AssignmentPattern>();
        VisitAssignmentTarget(p->left);
        VisitExpression(p->right);
        NoteNameInference(p->left, p->right);
        break;
      }
      case NodeKind::kRestElement:
        VisitAssignmentTarget(target->Cast<RestElement>()->argument);
        break;
      default:
        VisitExpression(target);
        break;
    }
  }

  void VisitExpression(Expression* e) {
    if (e == nullptr) {
      return;
    }
    Enter(e);
    switch (e->kind) {
      case NodeKind::kIdentifier:
        Reference(e->Cast<Identifier>(), /*is_write=*/false);
        break;
      case NodeKind::kPrivateIdentifier:
        NoteName(e->Cast<PrivateIdentifier>()->name);
        break;
      case NodeKind::kNullLiteral:
      case NodeKind::kBooleanLiteral:
      case NodeKind::kNumericLiteral:
      case NodeKind::kBigIntLiteral:
      case NodeKind::kStringLiteral:
      case NodeKind::kRegExpLiteral:
      case NodeKind::kThisExpression:
      case NodeKind::kSuper:
      case NodeKind::kMetaProperty:
        break;
      case NodeKind::kArrayExpression:
        VisitExpressions(e->Cast<ArrayExpression>()->elements);
        break;
      case NodeKind::kObjectExpression:
        for (Node* property : e->Cast<ObjectExpression>()->properties) {
          Enter(property);
          if (auto* p = property->As<Property>()) {
            VisitPropertyKey(p->key, p->computed);
            VisitExpression(p->value);
          } else {
            VisitExpression(property->Cast<Expression>());
          }
        }
        break;
      case NodeKind::kFunctionExpression:
        VisitFunction(e->Cast<FunctionExpression>(), e,
                      /*is_arrow=*/false,
                      /*is_expression=*/true);
        break;
      case NodeKind::kArrowFunctionExpression:
        VisitFunction(e->Cast<ArrowFunctionExpression>(), e,
                      /*is_arrow=*/true,
                      /*is_expression=*/true);
        break;
      case NodeKind::kClassExpression:
        VisitClass(e->Cast<ClassExpression>(), e, /*is_expression=*/true);
        break;
      case NodeKind::kUnaryExpression:
        VisitExpression(e->Cast<UnaryExpression>()->argument);
        break;
      case NodeKind::kUpdateExpression:
        VisitAssignmentTarget(e->Cast<UpdateExpression>()->argument);
        break;
      case NodeKind::kBinaryExpression: {
        auto* b = e->Cast<BinaryExpression>();
        VisitExpression(b->left);
        VisitExpression(b->right);
        break;
      }
      case NodeKind::kLogicalExpression: {
        auto* l = e->Cast<LogicalExpression>();
        VisitExpression(l->left);
        VisitExpression(l->right);
        break;
      }
      case NodeKind::kAssignmentExpression: {
        auto* a = e->Cast<AssignmentExpression>();
        VisitAssignmentTarget(a->left);
        VisitExpression(a->right);
        if (a->op == AssignOp::kAssign || a->op == AssignOp::kOrAssign ||
            a->op == AssignOp::kAndAssign ||
            a->op == AssignOp::kNullishAssign) {
          NoteNameInference(a->left, a->right);
        }
        break;
      }
      case NodeKind::kMemberExpression: {
        auto* m = e->Cast<MemberExpression>();
        VisitExpression(m->object);
        VisitPropertyKey(m->property, m->computed);
        break;
      }
      case NodeKind::kConditionalExpression: {
        auto* c = e->Cast<ConditionalExpression>();
        VisitExpression(c->test);
        VisitExpression(c->consequent);
        VisitExpression(c->alternate);
        break;
      }
      case NodeKind::kCallExpression: {
        auto* c = e->Cast<CallExpression>();
        if (auto* callee = c->callee->As<Identifier>()) {
          if (callee->name == "eval") {
            current_->has_direct_eval = true;
          }
        }
        VisitExpression(c->callee);
        VisitExpressions(c->arguments);
        break;
      }
      case NodeKind::kNewExpression: {
        auto* n = e->Cast<NewExpression>();
        VisitExpression(n->callee);
        VisitExpressions(n->arguments);
        break;
      }
      case NodeKind::kSequenceExpression:
        VisitExpressions(e->Cast<SequenceExpression>()->expressions);
        break;
      case NodeKind::kYieldExpression:
        VisitExpression(e->Cast<YieldExpression>()->argument);
        break;
      case NodeKind::kAwaitExpression:
        VisitExpression(e->Cast<AwaitExpression>()->argument);
        break;
      case NodeKind::kTemplateLiteral: {
        auto* t = e->Cast<TemplateLiteral>();
        for (TemplateElement* quasi : t->quasis) {
          Enter(quasi);
        }
        VisitExpressions(t->expressions);
        break;
      }
      case NodeKind::kTaggedTemplateExpression: {
        auto* t = e->Cast<TaggedTemplateExpression>();
        VisitExpression(t->tag);
        VisitExpression(t->quasi);
        break;
      }
      case NodeKind::kImportExpression: {
        auto* i = e->Cast<ImportExpression>();
        VisitExpression(i->source);
        VisitExpression(i->options);
        break;
      }
      case NodeKind::kChainExpression:
        VisitExpression(e->Cast<ChainExpression>()->expression);
        break;
      case NodeKind::kSpreadElement:
        VisitExpression(e->Cast<SpreadElement>()->argument);
        break;
      case NodeKind::kNativeCallExpression:
        VisitExpressions(e->Cast<NativeCallExpression>()->arguments);
        break;
      case NodeKind::kObjectPattern:
      case NodeKind::kArrayPattern:
      case NodeKind::kAssignmentPattern:
      case NodeKind::kRestElement:
        // Patterns only occur in target positions; be robust anyway.
        VisitAssignmentTarget(e);
        break;
      default:
        REDLINE_WARN("scope", "Unhandled expression kind %s",
                     NodeKindName(e->kind));
        break;
    }
  }

  // --- functions and classes ---

  static bool HasSimpleParameters(const FunctionData* fn) {
    for (const Expression* param : fn->params) {
      if (param == nullptr || param->kind != NodeKind::kIdentifier) {
        return false;
      }
    }
    return true;
  }

  void VisitFunction(FunctionData* fn, Node* node, bool is_arrow,
                     bool is_expression) {
    Scope* scope =
        NewScope(is_arrow ? ScopeKind::kArrow : ScopeKind::kFunction, node);
    scope->strict = scope->strict || fn->strict;
    scope->is_async = fn->is_async;
    scope->is_generator = fn->is_generator;
    ScopeGuard guard(this, scope);
    if (is_expression && fn->name != nullptr) {
      Declare(fn->name, BindingKind::kFunctionName, node, scope);
      Enter(fn->name);
    }
    for (Expression* param : fn->params) {
      VisitBindingPattern(param, BindingKind::kParameter, node);
    }
    if (fn->body == nullptr) {
      VisitExpression(fn->expression_body);
    } else if (HasSimpleParameters(fn)) {
      VisitBlockBody(fn->body);
    } else {
      // Parameter expressions (defaults, computed keys) must not see the
      // body's declarations: the body gets a var scope of its own.
      Scope* body_scope = NewScope(ScopeKind::kFunctionBody, fn->body);
      ScopeGuard body_guard(this, body_scope);
      for (Statement* statement : fn->body->body) {
        if (auto* declaration = statement->As<FunctionDeclaration>()) {
          if (declaration->name != nullptr) {
            body_function_names_[body_scope].insert(declaration->name->name);
          }
        }
      }
      VisitStatements(fn->body->body);
    }
  }

  void VisitClass(ClassData* cls, Node* node, bool is_expression) {
    Scope* scope = NewScope(ScopeKind::kClass, node);
    scope->strict = true;
    ScopeGuard guard(this, scope);
    if (is_expression && cls->name != nullptr) {
      Declare(cls->name, BindingKind::kClassName, node, scope);
      Enter(cls->name);
    }
    // The heritage is evaluated inside the class scope, where the class's
    // own name is in its temporal dead zone (`class x extends x {}`).
    VisitExpression(cls->super_class);
    if (cls->body == nullptr) {
      return;
    }
    Enter(cls->body);
    for (Node* member : cls->body->members) {
      Enter(member);
      switch (member->kind) {
        case NodeKind::kMethodDefinition: {
          auto* m = member->Cast<MethodDefinition>();
          VisitPropertyKey(m->key, m->computed);
          if (m->value != nullptr) {
            VisitFunction(m->value, m->value,
                          /*is_arrow=*/false,
                          /*is_expression=*/false);
          }
          break;
        }
        case NodeKind::kPropertyDefinition: {
          auto* p = member->Cast<PropertyDefinition>();
          VisitPropertyKey(p->key, p->computed);
          if (p->value != nullptr) {
            Scope* init_scope = NewScope(ScopeKind::kFieldInit, p);
            ScopeGuard init_guard(this, init_scope);
            VisitExpression(p->value);
          }
          break;
        }
        case NodeKind::kStaticBlock: {
          auto* block = member->Cast<StaticBlock>();
          Scope* block_scope = NewScope(ScopeKind::kStaticBlock, block);
          ScopeGuard block_guard(this, block_scope);
          VisitStatements(block->body);
          break;
        }
        default:
          break;
      }
    }
  }

  // --- resolution ---

  void Resolve() {
    for (const PendingReference& reference : pending_) {
      Binding* binding = nullptr;
      bool dynamic = false;
      for (Scope* s = reference.scope; s != nullptr; s = s->parent) {
        if (s->kind == ScopeKind::kWith || s->has_direct_eval) {
          dynamic = true;
        }
        auto it = s->by_name.find(reference.identifier->name);
        if (it != s->by_name.end()) {
          binding = it->second;
          break;
        }
        if (reference.identifier->name == "arguments" &&
            s->kind == ScopeKind::kFunction) {
          binding = ImplicitArguments(s);
          break;
        }
      }
      if (binding == nullptr) {
        analysis_->unresolved_.push_back(reference.identifier);
        continue;
      }
      analysis_->SetBindingOf(reference.identifier, binding);
      binding->references.push_back(reference.identifier);
      if (reference.is_write) {
        binding->assigned = true;
      }
      if (dynamic) {
        binding->has_dynamic_reference = true;
      }
    }
  }

  Binding* ImplicitArguments(Scope* scope) {
    Binding* binding = analysis_->NewBinding();
    binding->name = "arguments";
    binding->kind = BindingKind::kImplicit;
    binding->scope = scope;
    scope->bindings.push_back(binding);
    scope->by_name[binding->name] = binding;
    return binding;
  }

  // Annex B.3.3, second half: whether a block-level function is hoisted
  // depends on the lexical bindings of the same name between the block and
  // the function scope, and the hoisted `var` merges with any var-scoped
  // binding of that name. Pin all of them.
  void PinAnnexBFunctions() {
    for (const AnnexBFunction& fn : annex_b_functions_) {
      for (Binding* binding : analysis_->bindings_) {
        if (binding->name != fn.name) {
          continue;
        }
        for (Scope* s = binding->scope; s != nullptr; s = s->parent) {
          if (s == fn.var_scope) {
            binding->pinned = true;
            break;
          }
        }
      }
    }
  }

  // Nodes the explicit visitor did not touch (holes, template elements,
  // ...) inherit the scope of their parent.
  void FillMissingScopes(Node* root) {
    Walk(root, [this](Node* node) {
      Scope* scope = analysis_->ScopeOf(node);
      if (scope == nullptr) {
        scope = current_;
      }
      ForEachChild(
          node, [&](Node* child) { analysis_->SetScopeIfUnset(child, scope); });
      return true;
    });
  }

  static bool PropagateDynamic(Scope* scope) {
    bool dynamic = scope->kind == ScopeKind::kWith || scope->has_direct_eval;
    for (Scope* child : scope->children) {
      if (PropagateDynamic(child)) {
        dynamic = true;
      }
    }
    scope->dynamic_inside = dynamic;
    return dynamic;
  }

  struct AnnexBFunction {
    Scope* var_scope;
    std::string_view name;
  };

  ScopeAnalysis* analysis_;
  Scope* current_ = nullptr;
  std::vector<PendingReference> pending_;
  std::vector<AnnexBFunction> annex_b_functions_;
  std::vector<Identifier*> names_function_ids_;
  // Names of the function declarations directly in a kFunctionBody scope.
  absl::flat_hash_map<Scope*, absl::flat_hash_set<std::string_view>>
      body_function_names_;
};

// ---------------------------------------------------------------------------

ScopeAnalysis::ScopeAnalysis() = default;
ScopeAnalysis::~ScopeAnalysis() = default;

std::unique_ptr<ScopeAnalysis> ScopeAnalysis::Analyze(Tree* tree) {
  auto analysis = std::make_unique<ScopeAnalysis>();
  size_t ids = tree->id_limit();
  analysis->scope_of_.assign(ids, nullptr);
  analysis->binding_of_.assign(ids, nullptr);
  analysis->id_flags_.assign(ids, 0);
  ScopeBuilder builder(analysis.get());
  builder.Build(tree);
  return analysis;
}

// The tables are sized for the tree at Analyze() time; a node with a larger
// id was created afterwards (not a legal input, but keep the lookups total).
void ScopeAnalysis::SetScopeOf(const Node* node, Scope* scope) {
  if (node->id >= scope_of_.size()) {
    scope_of_.resize(node->id + 1, nullptr);
  }
  scope_of_[node->id] = scope;
}

void ScopeAnalysis::SetScopeIfUnset(const Node* node, Scope* scope) {
  if (ScopeOf(node) == nullptr) {
    SetScopeOf(node, scope);
  }
}

void ScopeAnalysis::SetBindingOf(const Identifier* identifier,
                                 Binding* binding) {
  if (identifier->id >= binding_of_.size()) {
    binding_of_.resize(identifier->id + 1, nullptr);
  }
  binding_of_[identifier->id] = binding;
}

void ScopeAnalysis::AddFlag(const Identifier* identifier, IdFlag flag) {
  if (identifier->id >= id_flags_.size()) {
    id_flags_.resize(identifier->id + 1, 0);
  }
  id_flags_[identifier->id] = static_cast<uint8_t>(id_flags_[identifier->id] |
                                                   static_cast<uint8_t>(flag));
}

bool ScopeAnalysis::HasFlag(const Identifier* identifier, IdFlag flag) const {
  return identifier->id < id_flags_.size() &&
         (id_flags_[identifier->id] & static_cast<uint8_t>(flag)) != 0;
}

Scope* ScopeAnalysis::NewScope() {
  scopes_.push_back(&scope_storage_.emplace_back());
  return scopes_.back();
}

Binding* ScopeAnalysis::NewBinding() {
  bindings_.push_back(&binding_storage_.emplace_back());
  return bindings_.back();
}

Scope* ScopeAnalysis::ScopeOf(const Node* node) const {
  return node->id < scope_of_.size() ? scope_of_[node->id] : nullptr;
}

Binding* ScopeAnalysis::BindingOf(const Identifier* identifier) const {
  return identifier->id < binding_of_.size() ? binding_of_[identifier->id]
                                             : nullptr;
}

bool ScopeAnalysis::IsReference(const Identifier* identifier) const {
  return HasFlag(identifier, IdFlag::kIsReference);
}

bool ScopeAnalysis::IsDeclaration(const Identifier* identifier) const {
  return HasFlag(identifier, IdFlag::kIsDeclaration);
}

std::vector<Binding*> ScopeAnalysis::CollectVisible(
    Scope* scope, int32_t position, bool initialized_only) const {
  std::vector<Binding*> result;
  absl::flat_hash_set<std::string_view> seen;
  for (Scope* s = scope; s != nullptr; s = s->parent) {
    for (Binding* binding : s->bindings) {
      if (seen.count(binding->name)) {
        continue;
      }
      // Hidden inner names (shadowed function expression names).
      auto it = s->by_name.find(binding->name);
      if (it == s->by_name.end() || it->second != binding) {
        continue;
      }
      if (initialized_only && binding->is_lexical() &&
          binding->identifier != nullptr && position >= 0 &&
          binding->identifier->start > position) {
        seen.insert(binding->name);  // shadows outer ones all the same (TDZ)
        continue;
      }
      seen.insert(binding->name);
      result.push_back(binding);
    }
  }
  return result;
}

namespace {

// True if evaluation of code in `inner` is deferred relative to `outer`
// (a function boundary lies between them), so that a `let` whose
// initializer contains the code may already be initialized when it runs.
bool CrossesFunction(const Scope* inner, const Scope* outer) {
  for (const Scope* s = inner; s != nullptr && s != outer; s = s->parent) {
    if (s->kind == ScopeKind::kFunction || s->kind == ScopeKind::kArrow ||
        s->kind == ScopeKind::kFunctionBody) {
      return true;
    }
  }
  return false;
}

}  // namespace

std::vector<Binding*> ScopeAnalysis::VisibleAt(const Node* node,
                                               bool initialized_only) const {
  Scope* scope = ScopeOf(node);
  // A node that introduces a scope is itself evaluated in the enclosing one.
  if (scope != nullptr && scope->node == node && scope->parent != nullptr) {
    scope = scope->parent;
  }
  std::vector<Binding*> result =
      CollectVisible(scope, node->start, initialized_only);
  if (!initialized_only || node->start < 0) {
    return result;
  }
  // `let x = ...node...`: x is in its TDZ while the initializer runs, unless
  // the node sits in a nested function that runs later.
  result.erase(std::remove_if(result.begin(), result.end(),
                              [&](Binding* binding) {
                                if (!binding->is_lexical() ||
                                    binding->declaration == nullptr) {
                                  return false;
                                }
                                const Node* declaration = binding->declaration;
                                if (!declaration->Is<VariableDeclarator>()) {
                                  return false;
                                }
                                if (declaration->start < 0 ||
                                    declaration->start > node->start ||
                                    node->end > declaration->end) {
                                  return false;
                                }
                                return !CrossesFunction(scope, binding->scope);
                              }),
               result.end());
  return result;
}

std::vector<Binding*> ScopeAnalysis::VisibleIn(const Node* owner,
                                               int32_t position,
                                               bool initialized_only) const {
  return CollectVisible(ScopeOf(owner), position, initialized_only);
}

bool ScopeAnalysis::IsNameFreeAt(const Node* node,
                                 std::string_view name) const {
  Scope* scope = ScopeOf(node);
  if (scope == nullptr) {
    return !all_names_.count(name);
  }
  if (scope->Lookup(name) != nullptr) {
    return false;
  }
  // Any reference to `name` from inside this scope would be captured.
  for (const Identifier* reference : unresolved_) {
    if (reference->name != name) {
      continue;
    }
    for (Scope* s = ScopeOf(reference); s != nullptr; s = s->parent) {
      if (s == scope) {
        return false;
      }
    }
  }
  for (const Binding* binding : bindings_) {
    if (binding->name != name) {
      continue;
    }
    for (Identifier* reference : binding->references) {
      for (Scope* s = ScopeOf(reference); s != nullptr; s = s->parent) {
        if (s == scope) {
          return false;
        }
      }
    }
  }
  return true;
}

std::string ScopeAnalysis::FreshName(std::string_view prefix) const {
  for (int i = 0;; i++) {
    std::string candidate = std::string(prefix) + std::to_string(i);
    if (!all_names_.count(candidate)) {
      return candidate;
    }
  }
}

void RenameBinding(Tree* tree, Binding* binding, std::string_view new_name) {
  std::string_view interned = tree->Intern(new_name);
  binding->scope->by_name.erase(binding->name);
  binding->name = interned;
  binding->scope->by_name[interned] = binding;
  for (Identifier* identifier : binding->declarations) {
    identifier->name = interned;
  }
  for (Identifier* identifier : binding->references) {
    identifier->name = interned;
  }
}

size_t RenameAllBindings(Tree* tree, ScopeAnalysis* analysis,
                         std::string_view prefix, RenameScope which) {
  size_t renamed = 0;
  int counter = 0;
  for (Binding* binding : analysis->bindings()) {
    if (!binding->safe_to_rename()) {
      continue;
    }
    bool is_function_like = binding->kind == BindingKind::kFunction ||
                            binding->kind == BindingKind::kClass ||
                            binding->kind == BindingKind::kFunctionName ||
                            binding->kind == BindingKind::kClassName ||
                            binding->names_function;
    if (which == RenameScope::kVariables && is_function_like) {
      continue;
    }
    std::string candidate;
    do {
      candidate = std::string(prefix) + std::to_string(counter++);
    } while (analysis->all_names().count(candidate));
    RenameBinding(tree, binding, candidate);
    renamed++;
  }
  return renamed;
}

std::string ScopeAnalysis::Dump() const {
  std::string out;
  std::function<void(const Scope*, int)> dump = [&](const Scope* scope,
                                                    int indent) {
    out.append(static_cast<size_t>(indent) * 2, ' ');
    out += StringPrintf(
        "%s scope n%u (%s)%s%s%s%s\n", ScopeKindName(scope->kind),
        scope->node->id, NodeKindName(scope->node->kind),
        scope->strict ? " strict" : "", scope->is_async ? " async" : "",
        scope->is_generator ? " generator" : "",
        scope->has_direct_eval ? " direct-eval" : "");
    for (const Binding* binding : scope->bindings) {
      out.append(static_cast<size_t>(indent) * 2 + 2, ' ');
      out += StringPrintf("%s %.*s", BindingKindName(binding->kind),
                          static_cast<int>(binding->name.size()),
                          binding->name.data());
      if (binding->identifier != nullptr) {
        out += StringPrintf(" (n%u)", binding->identifier->id);
      }
      out += StringPrintf(": %zu refs%s%s%s", binding->references.size(),
                          binding->assigned ? ", assigned" : "",
                          binding->names_function ? ", names-function" : "",
                          binding->safe_to_rename() ? "" : ", pinned");
      if (!binding->references.empty()) {
        out += " [";
        for (size_t i = 0; i < binding->references.size() && i < 8; i++) {
          if (i > 0) {
            out += ' ';
          }
          out += StringPrintf("n%u", binding->references[i]->id);
        }
        if (binding->references.size() > 8) {
          out += " ...";
        }
        out += ']';
      }
      out += '\n';
    }
    for (const Scope* child : scope->children) {
      dump(child, indent + 1);
    }
  };
  if (root_ != nullptr) {
    dump(root_, 0);
  }
  if (!unresolved_.empty()) {
    absl::flat_hash_set<std::string_view> names;
    out += "globals:";
    for (const Identifier* id : unresolved_) {
      if (names.insert(id->name).second) {
        out += StringPrintf(" %.*s", static_cast<int>(id->name.size()),
                            id->name.data());
      }
    }
    out += '\n';
  }
  return out;
}

}  // namespace fast
}  // namespace redline
