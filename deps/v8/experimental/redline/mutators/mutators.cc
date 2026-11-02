// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <cmath>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "experimental/redline/base/log.h"
#include "experimental/redline/core/adaptive_selector.h"
#include "experimental/redline/core/ast_builder.h"
#include "experimental/redline/core/tunable.h"
#include "experimental/redline/core/weighted_selector.h"
#include "experimental/redline/fast/printer.h"
#include "experimental/redline/generators/codegen.h"
#include "experimental/redline/mutators/explore.h"
#include "experimental/redline/mutators/mutator.h"
#include "experimental/redline/mutators/probing.h"

namespace redline {

using namespace fast;  // NOLINT(build/namespaces)

namespace {

// --- subtree analysis
// ------------------------------------------------------------

// What a piece of code needs from its surroundings to stay syntactically
// valid when moved elsewhere.
struct Requirements {
  bool needs_function = false;      // `return` outside a nested function
  bool needs_async = false;         // `await` / for-await outside a nested fn
  bool needs_generator = false;     // `yield`
  bool needs_break = false;         // unlabeled `break` outside own loop/switch
  bool needs_continue = false;      // unlabeled `continue` outside own loop
  bool needs_sloppy = false;        // `with`, `delete x`, legacy octal
  bool needs_arguments = false;     // `arguments` outside own non-arrow fn
  bool needs_super = false;         // `super` outside own method
  bool needs_new_target = false;    // `new.target` outside own function
  bool has_external_label = false;  // labeled jump to a label not inside
  bool has_lexical_declaration = false;  // top-level let/const/class
  // Labels declared by the node itself (outside nested functions): the
  // destination must not already be inside a label of the same name.
  std::vector<std::string_view> declared_labels;
};

bool IsLegacyOctal(std::string_view raw) {
  if (raw.size() < 2 || raw[0] != '0') {
    return false;
  }
  return raw[1] >= '0' && raw[1] <= '9';
}

class RequirementsAnalyzer {
 public:
  Requirements Analyze(Node* root) {
    req_ = Requirements();
    if (root->Is<VariableDeclaration>()) {
      VariableKind k = root->Cast<VariableDeclaration>()->declaration_kind;
      req_.has_lexical_declaration = k != VariableKind::kVar;
    } else if (root->Is<ClassDeclaration>()) {
      req_.has_lexical_declaration = true;
    }
    Visit(root, 0, 0, 0, 0);
    return req_;
  }

 private:
  void Visit(Node* node, int fn_depth, int this_fn_depth, int loop_depth,
             int switch_depth) {
    if (node == nullptr) {
      return;
    }
    switch (node->kind) {
      case NodeKind::kReturnStatement:
        if (fn_depth == 0) {
          req_.needs_function = true;
        }
        break;
      case NodeKind::kAwaitExpression:
        if (fn_depth == 0) {
          req_.needs_async = true;
        }
        break;
      case NodeKind::kYieldExpression:
        if (fn_depth == 0) {
          req_.needs_generator = true;
        }
        break;
      case NodeKind::kForOfStatement:
        if (node->Cast<ForOfStatement>()->is_await && fn_depth == 0) {
          req_.needs_async = true;
        }
        break;
      case NodeKind::kBreakStatement: {
        Identifier* label = node->Cast<BreakStatement>()->label;
        if (label != nullptr) {
          if (!HasLabel(label->name)) {
            req_.has_external_label = true;
          }
        } else if (loop_depth == 0 && switch_depth == 0) {
          req_.needs_break = true;
        }
        return;
      }
      case NodeKind::kContinueStatement: {
        Identifier* label = node->Cast<ContinueStatement>()->label;
        if (label != nullptr) {
          if (!HasLabel(label->name)) {
            req_.has_external_label = true;
          }
        } else if (loop_depth == 0) {
          req_.needs_continue = true;
        }
        return;
      }
      case NodeKind::kWithStatement:
        req_.needs_sloppy = true;
        break;
      case NodeKind::kUnaryExpression: {
        auto* u = node->Cast<UnaryExpression>();
        if (u->op == UnaryOp::kDelete && u->argument->Is<Identifier>()) {
          req_.needs_sloppy = true;
        }
        break;
      }
      case NodeKind::kNumericLiteral:
        if (IsLegacyOctal(node->Cast<NumericLiteral>()->raw)) {
          req_.needs_sloppy = true;
        }
        return;
      case NodeKind::kIdentifier:
        if (node->Cast<Identifier>()->name == "arguments" &&
            this_fn_depth == 0) {
          req_.needs_arguments = true;
        }
        return;
      case NodeKind::kSuper:
        if (this_fn_depth == 0) {
          req_.needs_super = true;
        }
        return;
      case NodeKind::kMetaProperty:
        if (node->Cast<MetaProperty>()->meta_kind ==
                MetaPropertyKind::kNewTarget &&
            this_fn_depth == 0) {
          req_.needs_new_target = true;
        }
        return;
      case NodeKind::kFunctionDeclaration:
      case NodeKind::kFunctionExpression:
      case NodeKind::kArrowFunctionExpression: {
        const FunctionData* fn = AsFunctionData(node);
        int next_this = this_fn_depth + (fn->is_arrow ? 0 : 1);
        std::vector<std::string_view> saved = std::move(labels_);
        labels_.clear();
        ForEachChild(node, [&](Node* child) {
          Visit(child, fn_depth + 1, next_this, 0, 0);
        });
        labels_ = std::move(saved);
        return;
      }
      case NodeKind::kPropertyDefinition:
      case NodeKind::kStaticBlock: {
        std::vector<std::string_view> saved = std::move(labels_);
        labels_.clear();
        ForEachChild(node, [&](Node* child) {
          Visit(child, fn_depth + 1, this_fn_depth + 1, 0, 0);
        });
        labels_ = std::move(saved);
        return;
      }
      case NodeKind::kLabeledStatement: {
        auto* l = node->Cast<LabeledStatement>();
        if (fn_depth == 0) {
          req_.declared_labels.push_back(l->label->name);
        }
        labels_.push_back(l->label->name);
        Visit(l->body, fn_depth, this_fn_depth, loop_depth, switch_depth);
        labels_.pop_back();
        return;
      }
      case NodeKind::kWhileStatement:
      case NodeKind::kDoWhileStatement:
      case NodeKind::kForStatement:
      case NodeKind::kForInStatement:
        ForEachChild(node, [&](Node* child) {
          Visit(child, fn_depth, this_fn_depth, loop_depth + 1, switch_depth);
        });
        return;
      case NodeKind::kSwitchStatement:
        ForEachChild(node, [&](Node* child) {
          Visit(child, fn_depth, this_fn_depth, loop_depth, switch_depth + 1);
        });
        return;
      default:
        break;
    }
    if (node->Is<ForOfStatement>()) {
      ForEachChild(node, [&](Node* child) {
        Visit(child, fn_depth, this_fn_depth, loop_depth + 1, switch_depth);
      });
      return;
    }
    ForEachChild(node, [&](Node* child) {
      Visit(child, fn_depth, this_fn_depth, loop_depth, switch_depth);
    });
  }

  bool HasLabel(std::string_view name) const {
    return std::find(labels_.begin(), labels_.end(), name) != labels_.end();
  }

  Requirements req_;
  std::vector<std::string_view> labels_;
};

Requirements AnalyzeRequirements(Node* node) {
  RequirementsAnalyzer analyzer;
  return analyzer.Analyze(node);
}

bool Compatible(const Requirements& req, const Builder::Context& ctx) {
  if (req.has_external_label) {
    return false;
  }
  for (std::string_view label : req.declared_labels) {
    if (std::find(ctx.labels.begin(), ctx.labels.end(), label) !=
        ctx.labels.end()) {
      return false;
    }
  }
  if (req.needs_function &&
      (!ctx.in_function || ctx.in_static_block || ctx.in_class_field)) {
    return false;
  }
  if (req.needs_async && (!ctx.in_async || ctx.in_static_block)) {
    return false;
  }
  if (req.needs_generator && !ctx.in_generator) {
    return false;
  }
  if (req.needs_break && !ctx.break_allowed()) {
    return false;
  }
  if (req.needs_continue && !ctx.in_loop) {
    return false;
  }
  if (req.needs_sloppy && ctx.strict) {
    return false;
  }
  if (req.needs_arguments && !ctx.has_arguments) {
    return false;
  }
  if (req.needs_super && !ctx.in_method) {
    return false;
  }
  if (req.needs_new_target &&
      !(ctx.has_arguments || ctx.in_class_field || ctx.in_static_block)) {
    return false;
  }
  return true;
}

// Visits two subtrees of identical shape in parallel.
template <typename F>
void ParallelWalk(Node* a, Node* b, F&& f) {
  if (a == nullptr || b == nullptr) {
    return;
  }
  f(a, b);
  std::vector<Node*> ca, cb;
  ForEachChild(a, [&](Node* c) { ca.push_back(c); });
  ForEachChild(b, [&](Node* c) { cb.push_back(c); });
  for (size_t i = 0; i < ca.size() && i < cb.size(); i++) {
    ParallelWalk(ca[i], cb[i], f);
  }
}

bool IsLexicalDeclaration(const Statement* s) {
  if (auto* v = s->As<VariableDeclaration>()) {
    return v->declaration_kind != VariableKind::kVar;
  }
  return s->Is<ClassDeclaration>();
}

bool IsDirective(const Statement* s) {
  auto* e = s->As<ExpressionStatement>();
  return e != nullptr && e->is_directive;
}

// True if `root` mentions a private name (`o.#p`, `#p in o`) without being
// the class that declares it: moved anywhere else, such code is an early
// error ("Private field must be declared in an enclosing class").
bool UsesPrivateNameOfEnclosingClass(Node* root) {
  if (root->IsClass()) {
    return false;
  }
  bool found = false;
  Walk(root, [&](Node* n) {
    if (found || n->IsClass()) {
      return false;  // a nested class declares its own names
    }
    if (n->Is<PrivateIdentifier>()) {
      found = true;
    }
    return true;
  });
  return found;
}

// True if `root` contains a reference to one of `bindings`.
bool Uses(Builder& b, Node* root, const std::vector<Binding*>& bindings) {
  if (bindings.empty()) {
    return false;
  }
  absl::flat_hash_set<const Binding*> set(bindings.begin(), bindings.end());
  bool found = false;
  Walk(root, [&](Node* n) {
    if (found) {
      return false;
    }
    auto* id = n->As<Identifier>();
    if (id != nullptr && b.scopes().IsReference(id) &&
        set.count(b.scopes().BindingOf(id))) {
      found = true;
    }
    return !found;
  });
  return found;
}

// Builds a generation context at a statement slot.
GenContext ContextForSlot(Builder& b, const Builder::Slot& slot,
                          MutationContext& ctx) {
  GenContext gen(b, *ctx.generators, *ctx.env);
  gen.slot = slot;
  gen.syntax = b.ContextAtSlot(slot);
  gen.visible = b.VisibleAtSlot(slot);
  return gen;
}

// Builds a generation context at an arbitrary node.
GenContext ContextForNode(Builder& b, Node* node, MutationContext& ctx) {
  GenContext gen(b, *ctx.generators, *ctx.env);
  Statement* enclosing = b.EnclosingStatement(node);
  if (enclosing != nullptr) {
    gen.slot = b.SlotOf(enclosing);
  }
  gen.syntax = b.ContextAt(node);
  gen.visible = b.VisibleAt(node);
  return gen;
}

Node* ReplaceAnchored(Builder& b, Node* old_node, Node* new_node) {
  if (old_node->start >= 0) {
    mk::Anchor(new_node, old_node->start);
  }
  return b.Replace(old_node, new_node) ? new_node : nullptr;
}

std::string Describe(Node* node) {
  std::string s = Print(node, fast::PrintOptions{}, nullptr);
  if (s.size() > 60) {
    s = s.substr(0, 57) + "...";
  }
  for (char& c : s) {
    if (c == '\n') {
      c = ' ';
    }
  }
  return s;
}

// --- Operation mutator
// --------------------------------------------------------------

REDLINE_TUNABLE(kOperationEmptyStringCandidateChance,
                "mutators.Operation.empty_string_candidate_chance", 0.2, 0, 1,
                "probability that an empty string literal is kept as an "
                "Operation candidate");
REDLINE_TUNABLE(kOperationMaxAttempts, "mutators.Operation.max_attempts", 8, 1,
                16,
                "candidate nodes the Operation mutator tries before giving up");
REDLINE_TUNABLE(
    kOperationBinaryOpAttempts, "mutators.Operation.binary_op_attempts", 4, 1,
    16, "draws to find a binary operator different from the current one");
REDLINE_TUNABLE(kOperationPlainAssignChance,
                "mutators.Operation.plain_assign_chance", 0.2, 0, 1,
                "probability that a mutated assignment operator becomes plain "
                "`=` rather than a compound one");
REDLINE_TUNABLE(kOperationRandomNumberMax,
                "mutators.Operation.random_number_max", 100, 1, 1024,
                "upper bound of the uniform random replacement for a numeric "
                "literal (0..max)");
REDLINE_TUNABLE(kOperationPropertyNameAttempts,
                "mutators.Operation.property_name_attempts", 4, 1, 16,
                "draws to find a property name different from the current one");

class OperationMutator final : public Mutator {
 public:
  // Fuzzilli: OperationMutator, weight 1, up to 7 simultaneous.
  OperationMutator()
      : Mutator("Operation",
                "changes an operator, literal value or property name in place",
                1, false, 7) {}

  bool Mutate(Builder& b, MutationContext& ctx) override {
    Random& r = b.rng();
    std::vector<Node*> candidates = b.Collect([&](Node* n) {
      switch (n->kind) {
        case NodeKind::kBinaryExpression:
        case NodeKind::kLogicalExpression:
        case NodeKind::kUnaryExpression:
        case NodeKind::kUpdateExpression:
        case NodeKind::kAssignmentExpression:
        case NodeKind::kNumericLiteral:
        case NodeKind::kBooleanLiteral:
        case NodeKind::kMemberExpression:
          return true;
        case NodeKind::kStringLiteral:
          return !n->Cast<StringLiteral>()->value.empty() ||
                 r.Chance(kOperationEmptyStringCandidateChance());
        default:
          return false;
      }
    });
    for (int attempt = 0;
         attempt < kOperationMaxAttempts.AsInt() && !candidates.empty();
         attempt++) {
      Node* node = candidates[r.Index(candidates.size())];
      if (MutateNode(b, node, ctx)) {
        return true;
      }
    }
    return false;
  }

 private:
  bool MutateNode(Builder& b, Node* node, MutationContext& ctx) {
    Random& r = b.rng();
    Tree* t = b.tree();
    switch (node->kind) {
      case NodeKind::kBinaryExpression: {
        auto* e = node->Cast<BinaryExpression>();
        if (e->left->Is<PrivateIdentifier>()) {
          return false;
        }
        BinaryOp op = e->op;
        for (int i = 0; i < kOperationBinaryOpAttempts.AsInt() && op == e->op;
             i++) {
          op = CodeGen::RandomBinaryOp(r);
        }
        if (op == BinaryOp::kExp && (e->left->Is<UnaryExpression>() ||
                                     e->left->Is<AwaitExpression>())) {
          return false;
        }
        ctx.detail = std::string("binary ") + OperatorText(e->op) + " -> " +
                     OperatorText(op);
        e->op = op;
        return true;
      }
      case NodeKind::kLogicalExpression: {
        auto* e = node->Cast<LogicalExpression>();
        LogicalOp op = e->op;
        while (op == e->op) {
          op = CodeGen::RandomLogicalOp(r);
        }
        // `a ?? b || c` without parentheses is an error; the printer adds
        // them when precedences mix, so this is safe.
        ctx.detail = std::string("logical ") + OperatorText(e->op) + " -> " +
                     OperatorText(op);
        e->op = op;
        return true;
      }
      case NodeKind::kUnaryExpression: {
        auto* e = node->Cast<UnaryExpression>();
        if (e->op == UnaryOp::kDelete) {
          return false;
        }
        UnaryOp op = e->op;
        while (op == e->op) {
          op = CodeGen::RandomUnaryOp(r);
        }
        Node* parent = b.ParentOf(node);
        if (parent != nullptr && parent->Is<BinaryExpression>() &&
            parent->Cast<BinaryExpression>()->op == BinaryOp::kExp &&
            parent->Cast<BinaryExpression>()->left == node) {
          return false;  // `-x ** 2`
        }
        ctx.detail = std::string("unary ") + OperatorText(e->op) + " -> " +
                     OperatorText(op);
        e->op = op;
        return true;
      }
      case NodeKind::kUpdateExpression: {
        auto* e = node->Cast<UpdateExpression>();
        if (r.Coin()) {
          e->op = e->op == UpdateOp::kIncrement ? UpdateOp::kDecrement
                                                : UpdateOp::kIncrement;
          ctx.detail = "update operator flipped";
        } else {
          e->prefix = !e->prefix;
          ctx.detail = "update prefix flipped";
        }
        return true;
      }
      case NodeKind::kAssignmentExpression: {
        auto* e = node->Cast<AssignmentExpression>();
        if (!e->left->Is<Identifier>() && !e->left->Is<MemberExpression>()) {
          return false;
        }
        AssignOp op = r.Chance(kOperationPlainAssignChance())
                          ? AssignOp::kAssign
                          : CodeGen::RandomCompoundAssignOp(r);
        if (op == e->op) {
          return false;
        }
        ctx.detail = std::string("assignment ") + OperatorText(e->op) + " -> " +
                     OperatorText(op);
        e->op = op;
        return true;
      }
      case NodeKind::kNumericLiteral: {
        auto* n = node->Cast<NumericLiteral>();
        double old = n->value;
        double v;  // Assigned on every path of the switch below.
        switch (r.Index(8)) {
          case 0:
            v = old + 1;
            break;
          case 1:
            v = old - 1;
            break;
          case 2:
            v = -old;
            break;
          case 3:
            v = old * 2;
            break;
          case 4:
            v = r.Pick(ctx.env->numbers());
            break;
          case 5:
            v = static_cast<double>(
                r.Range(0, kOperationRandomNumberMax.AsInt()));
            break;
          case 6:
            v = old / 2;
            break;
          default:
            v = static_cast<double>(r.Next() >> 32);
            break;
        }
        if (v == old) {
          return false;
        }
        // Negative numbers as object keys or member names are not literals.
        Node* parent = b.ParentOf(node);
        bool is_key =
            parent != nullptr &&
            ((parent->Is<Property>() && parent->Cast<Property>()->key == node &&
              !parent->Cast<Property>()->computed) ||
             (parent->Is<MethodDefinition>() &&
              parent->Cast<MethodDefinition>()->key == node &&
              !parent->Cast<MethodDefinition>()->computed) ||
             (parent->Is<PropertyDefinition>() &&
              parent->Cast<PropertyDefinition>()->key == node &&
              !parent->Cast<PropertyDefinition>()->computed));
        if (is_key && (v < 0 || v != v || std::isinf(v))) {
          v = std::abs(v == v ? v : 1);
        }
        if (is_key && std::isinf(v)) {
          v = 1;
        }
        n->value = v;
        n->raw = {};
        ctx.detail = StringPrintf("number %g -> %g", old, v);
        return true;
      }
      case NodeKind::kBooleanLiteral: {
        auto* lit = node->Cast<BooleanLiteral>();
        lit->value = !lit->value;
        ctx.detail = "boolean flipped";
        return true;
      }
      case NodeKind::kStringLiteral: {
        auto* s = node->Cast<StringLiteral>();
        Node* parent = b.ParentOf(node);
        if (parent != nullptr && parent->Is<ExpressionStatement>() &&
            parent->Cast<ExpressionStatement>()->is_directive) {
          return false;
        }
        if (parent != nullptr && parent->Is<ImportExpression>()) {
          return false;
        }
        std::string old = s->value;
        std::string v;
        switch (r.Index(5)) {
          case 0:
            v = std::string(r.Pick(ctx.env->strings()));
            break;
          case 1:
            v = std::string(r.Pick(ctx.env->property_names()));
            break;
          case 2:
            v = old + old;
            break;
          case 3:
            v = old.substr(0, old.size() / 2);
            break;
          default:
            v = old + "x";
            break;
        }
        if (v == old) {
          return false;
        }
        s->value = v;
        s->raw = {};
        ctx.detail =
            "string \"" + (old.size() > 20 ? old.substr(0, 20) + "..." : old) +
            "\" -> \"" + (v.size() > 20 ? v.substr(0, 20) + "..." : v) + "\"";
        return true;
      }
      case NodeKind::kMemberExpression: {
        auto* m = node->Cast<MemberExpression>();
        if (m->computed || !m->property->Is<Identifier>() ||
            m->object->Is<Super>()) {
          return false;
        }
        std::string_view old = m->property->Cast<Identifier>()->name;
        std::string_view name = old;
        // Type-directed names when the object is a known variable.
        JsType type = Builder::GuessType(m->object);
        const BuiltinType* bt = ctx.env->ForType(type);
        for (int i = 0;
             i < kOperationPropertyNameAttempts.AsInt() && name == old; i++) {
          if (bt != nullptr && r.Coin()) {
            if (!bt->methods.empty() && r.Coin()) {
              name = r.Pick(bt->methods).name;
            } else if (!bt->properties.empty()) {
              name = r.Pick(bt->properties);
            }
          } else {
            name = r.Pick(ctx.env->property_names());
          }
          if (!IsIdentifierName(name)) {
            name = old;
          }
        }
        if (name == old) {
          return false;
        }
        m->property = mk::Id(t, name);
        ctx.detail =
            "property ." + std::string(old) + " -> ." + std::string(name);
        return true;
      }
      default:
        return false;
    }
  }
};

// --- Input mutator
// -------------------------------------------------------------------

REDLINE_TUNABLE(kInputSkipUndefinedChance,
                "mutators.Input.skip_undefined_chance", 0.7, 0, 1,
                "probability that a reference to `undefined` is skipped as an "
                "Input candidate");
REDLINE_TUNABLE(
    kInputMaxAttempts, "mutators.Input.max_attempts", 8, 1, 16,
    "identifier references the Input mutator tries before giving up");
REDLINE_TUNABLE(kInputTypeMismatchChance, "mutators.Input.type_mismatch_chance",
                0.2, 0, 1,
                "probability that a visible variable of a different type is an "
                "acceptable replacement");
REDLINE_TUNABLE(kInputLiteralChance, "mutators.Input.literal_chance", 0.15, 0,
                1,
                "probability that a value reference is replaced by a literal "
                "rather than another variable");

class InputMutator final : public Mutator {
 public:
  // Fuzzilli: InputMutator (loose 2 + aware 1), up to 7 simultaneous.
  InputMutator()
      : Mutator("Input",
                "replaces a variable reference by another visible variable", 3,
                false, 7) {}

  bool Mutate(Builder& b, MutationContext& ctx) override {
    Random& r = b.rng();
    ScopeAnalysis& scopes = b.scopes();
    std::vector<Node*> candidates = b.Collect([&](Node* n) {
      if (!n->Is<Identifier>()) {
        return false;
      }
      auto* id = n->Cast<Identifier>();
      if (!scopes.IsReference(id)) {
        return false;
      }
      if (id->name == "undefined" && r.Chance(kInputSkipUndefinedChance())) {
        return false;
      }
      return b.RoleOf(n) != ExpressionRole::kNotReplaceable;
    });
    for (int attempt = 0;
         attempt < kInputMaxAttempts.AsInt() && !candidates.empty();
         attempt++) {
      auto* id = candidates[r.Index(candidates.size())]->Cast<Identifier>();
      ExpressionRole role = b.RoleOf(id);
      std::vector<Variable> visible = b.VisibleAt(id);
      Binding* binding = scopes.BindingOf(id);
      JsType type =
          binding != nullptr ? b.GuessType(binding) : JsType::kUnknown;
      std::vector<const Variable*> options;
      for (const Variable& v : visible) {
        if (v.name == id->name) {
          continue;
        }
        if (v.name == "arguments" && !b.ContextAt(id).has_arguments) {
          continue;
        }
        if (role == ExpressionRole::kTarget &&
            (v.is_constant || v.name == "arguments")) {
          continue;
        }
        if (type == JsType::kUnknown || v.type == type ||
            v.type == JsType::kUnknown ||
            r.Chance(kInputTypeMismatchChance())) {
          options.push_back(&v);
        }
      }
      if (role == ExpressionRole::kValue && r.Chance(kInputLiteralChance())) {
        // Replace by a literal of the same type instead.
        GenContext gen = ContextForNode(b, id, ctx);
        CodeGen cg(gen);
        std::string old(id->name);
        if (ReplaceAnchored(b, id, cg.Literal(type)) != nullptr) {
          ctx.detail = "input " + old + " -> literal";
          return true;
        }
        continue;
      }
      if (options.empty()) {
        continue;
      }
      const Variable& choice = *options[r.Index(options.size())];
      // Don't create `delete name` in strict code.
      Node* parent = b.ParentOf(id);
      if (parent != nullptr && parent->Is<UnaryExpression>() &&
          parent->Cast<UnaryExpression>()->op == UnaryOp::kDelete) {
        continue;
      }
      ctx.detail =
          "input " + std::string(id->name) + " -> " + std::string(choice.name);
      id->name = b.tree()->Intern(choice.name);
      b.Invalidate();
      return true;
    }
    return false;
  }
};

// --- Remove mutator
// -----------------------------------------------------------------

REDLINE_TUNABLE(kRemoveElementChance, "mutators.Remove.element_chance", 0.3, 0,
                1,
                "probability of removing an argument, element or property "
                "instead of a statement");
REDLINE_TUNABLE(kRemoveMaxAttempts, "mutators.Remove.max_attempts", 8, 1, 16,
                "statement slots the Remove mutator tries before giving up");

class RemoveMutator final : public Mutator {
 public:
  // No Fuzzilli counterpart: weight 0 by default (enable with
  // --mutator-weights).
  RemoveMutator()
      : Mutator("Remove", "removes a statement, argument, element or property",
                0, false) {}

  bool Mutate(Builder& b, MutationContext& ctx) override {
    Random& r = b.rng();
    if (r.Chance(kRemoveElementChance()) && RemoveElement(b, ctx)) {
      return true;
    }
    std::vector<Builder::Slot> slots = b.StatementSlots();
    for (int attempt = 0;
         attempt < kRemoveMaxAttempts.AsInt() && !slots.empty(); attempt++) {
      const Builder::Slot& slot = slots[r.Index(slots.size())];
      Statement* s = slot.statement();
      if (s == nullptr || IsDirective(s)) {
        continue;
      }
      // Removing a declaration that later code still uses only produces a
      // ReferenceError: never do that.
      if (!b.EscapingBindings({s}, /*block_scoped_only=*/false).empty()) {
        continue;
      }
      ctx.detail =
          std::string("removed ") + s->kind_name() + " `" + Describe(s) + "`";
      b.Remove(slot);
      return true;
    }
    return false;
  }

 private:
  bool RemoveElement(Builder& b, MutationContext& ctx) {
    Random& r = b.rng();
    std::vector<Node*> candidates = b.Collect([&](Node* n) {
      switch (n->kind) {
        case NodeKind::kCallExpression:
          return !n->Cast<CallExpression>()->arguments.empty();
        case NodeKind::kNewExpression:
          return !n->Cast<NewExpression>()->arguments.empty();
        case NodeKind::kArrayExpression:
          return !n->Cast<ArrayExpression>()->elements.empty();
        case NodeKind::kObjectExpression:
          return !n->Cast<ObjectExpression>()->properties.empty();
        default:
          return false;
      }
    });
    if (candidates.empty()) {
      return false;
    }
    Node* node = candidates[r.Index(candidates.size())];
    switch (node->kind) {
      case NodeKind::kCallExpression: {
        auto& args = node->Cast<CallExpression>()->arguments;
        args.erase(args.begin() +
                   static_cast<std::ptrdiff_t>(r.Index(args.size())));
        ctx.detail = "removed call argument";
        break;
      }
      case NodeKind::kNewExpression: {
        auto& args = node->Cast<NewExpression>()->arguments;
        args.erase(args.begin() +
                   static_cast<std::ptrdiff_t>(r.Index(args.size())));
        ctx.detail = "removed constructor argument";
        break;
      }
      case NodeKind::kArrayExpression: {
        auto& elements = node->Cast<ArrayExpression>()->elements;
        elements.erase(elements.begin() +
                       static_cast<std::ptrdiff_t>(r.Index(elements.size())));
        ctx.detail = "removed array element";
        break;
      }
      case NodeKind::kObjectExpression: {
        auto& props = node->Cast<ObjectExpression>()->properties;
        props.erase(props.begin() +
                    static_cast<std::ptrdiff_t>(r.Index(props.size())));
        ctx.detail = "removed object property";
        break;
      }
      default:
        return false;
    }
    b.Invalidate();
    return true;
  }
};

// --- Insert (code generation) mutator
// --------------------------------------------------

REDLINE_TUNABLE(kInsertSlotAttempts, "mutators.Insert.slot_attempts", 6, 1, 16,
                "slots drawn to find one with a function in scope when "
                "preferring functions");
REDLINE_TUNABLE(kInsertExtraContinueChance,
                "mutators.Insert.extra_continue_chance", 0.6, 0, 1,
                "probability of inserting one more generator output beyond the "
                "minimum (geometric)");

class InsertMutator final : public Mutator {
 public:
  // Inserts between min_count and min_count + extra_count generator outputs
  // (geometrically distributed) at a random slot, or at the top of the
  // program with `at_top` (the "fresh values" prefix every mutation chain
  // starts with, after Fuzzilli's buildPrefix()). `max_simultaneous` slots
  // may be filled per application (Fuzzilli's CodeGenMutator: 3).
  InsertMutator(std::string name, std::string_view description, double weight,
                std::vector<std::string> generator_names, bool prefer_functions,
                int min_count = 1, int extra_count = 2, bool at_top = false,
                int max_simultaneous = 1)
      : Mutator(std::move(name), description, weight, false, max_simultaneous),
        generator_names_(std::move(generator_names)),
        prefer_functions_(prefer_functions),
        min_count_(min_count),
        extra_count_(extra_count),
        at_top_(at_top) {}

  // How many generator outputs one application inserts: min_count plus a
  // geometrically distributed 0..extra_count.
  bool SetInsertCounts(int min_count, int extra_count) override {
    min_count_ = min_count;
    extra_count_ = extra_count;
    return true;
  }

  bool Mutate(Builder& b, MutationContext& ctx) override {
    Random& r = b.rng();
    Builder::Slot slot;
    if (at_top_) {
      std::vector<Builder::Slot> slots = b.InsertionSlots();
      for (const Builder::Slot& s : slots) {
        if (s.owner == b.root()) {
          slot = s;
          break;
        }
      }
      if (!slot.valid()) {
        return false;
      }
    }
    for (int attempt = 0; attempt < kInsertSlotAttempts.AsInt() && !at_top_;
         attempt++) {
      slot = b.RandomInsertionSlot();
      if (!slot.valid()) {
        return false;
      }
      if (!prefer_functions_) {
        break;
      }
      std::vector<Variable> visible = b.VisibleAtSlot(slot);
      if (!Builder::OfType(visible, JsType::kFunction).empty()) {
        break;
      }
    }
    GenContext gen = ContextForSlot(b, slot, ctx);
    std::vector<Statement*> out;
    gen.out = &out;
    int count =
        min_count_ + r.SmallCount(extra_count_, kInsertExtraContinueChance());
    std::string names;
    for (int i = 0; i < count; i++) {
      Generator* g = PickGenerator(gen);
      if (g == nullptr) {
        break;
      }
      g->stats().invocations++;
      size_t before = out.size();
      if (g->Generate(gen, &out) && out.size() > before) {
        g->stats().produced++;
        NoteGeneratorUsed(g);
        if (!names.empty()) {
          names += '+';
        }
        names += g->name();
      } else {
        out.resize(before);
      }
    }
    if (out.empty()) {
      return false;
    }
    b.InsertAll(slot, out);
    ctx.detail = "inserted " + names;
    return true;
  }

 private:
  // Asks the generator strategy for one generator that is applicable here
  // and, if this mutator has a generator list, on that list.
  Generator* PickGenerator(const GenContext& gen) {
    const GeneratorSet& set = gen.generators;
    auto eligible = [&](const Generator& g) {
      if (!generator_names_.empty() &&
          std::find(generator_names_.begin(), generator_names_.end(),
                    g.name()) == generator_names_.end()) {
        return false;
      }
      return g.Applicable(gen);
    };
    size_t index =
        set.selector().Pick(set.candidates(), gen.builder.rng(), eligible);
    return index == Selector<Generator>::kNoChoice ? nullptr
                                                   : set.candidates()[index];
  }

  std::vector<std::string> generator_names_;
  bool prefer_functions_;
  int min_count_;
  int extra_count_;
  bool at_top_;
};

// --- Expression mutator
// -------------------------------------------------------------

REDLINE_TUNABLE(
    kExpressionMaxAttempts, "mutators.Expression.max_attempts", 8, 1, 16,
    "value expressions the Expression mutator tries before giving up");
REDLINE_TUNABLE(kExpressionComplexCandidateChance,
                "mutators.Expression.complex_candidate_chance", 0.2, 0, 1,
                "probability that a spread, template, function or class "
                "expression is kept as a candidate");

class ExpressionMutator final : public Mutator {
 public:
  // Closest Fuzzilli counterpart: PropertyAccessorMutator (weight 1, shared
  // with Call), up to 3 simultaneous.
  ExpressionMutator()
      : Mutator("Expression",
                "replaces or wraps an expression in value position", 0.5, false,
                3) {}

  bool Mutate(Builder& b, MutationContext& ctx) override {
    Random& r = b.rng();
    std::vector<Expression*> candidates = b.ValueExpressions();
    for (int attempt = 0;
         attempt < kExpressionMaxAttempts.AsInt() && !candidates.empty();
         attempt++) {
      Expression* e = candidates[r.Index(candidates.size())];
      if ((e->Is<SpreadElement>() || e->Is<TemplateLiteral>() ||
           e->Is<FunctionExpression>() || e->Is<ClassExpression>() ||
           e->Is<ArrowFunctionExpression>()) &&
          !r.Chance(kExpressionComplexCandidateChance())) {
        continue;
      }
      Node* parent = b.ParentOf(e);
      if (parent == nullptr) {
        continue;
      }
      if (parent->Is<ExpressionStatement>() &&
          parent->Cast<ExpressionStatement>()->is_directive) {
        continue;
      }
      if (parent->Is<SpreadElement>() && !r.Coin()) {
        continue;
      }
      bool under_delete =
          parent->Is<UnaryExpression>() &&
          parent->Cast<UnaryExpression>()->op == UnaryOp::kDelete;
      bool exp_left = parent->Is<BinaryExpression>() &&
                      parent->Cast<BinaryExpression>()->op == BinaryOp::kExp &&
                      parent->Cast<BinaryExpression>()->left == e;
      GenContext gen = ContextForNode(b, e, ctx);
      CodeGen cg(gen);
      JsType type = Builder::GuessType(e);
      Expression* replacement = nullptr;
      std::string how;
      switch (r.Index(10)) {
        case 0:
        case 1:
        case 2: {
          Identifier* v = cg.Variable(type);
          if (v == nullptr || under_delete) {
            continue;
          }
          replacement = v;
          how = "variable";
          break;
        }
        case 3:
        case 4:
          replacement = cg.Literal(type);
          how = "literal";
          break;
        case 5:
        case 6:
          replacement = cg.Expression(1);
          how = "expression";
          break;
        default: {
          if (e->Is<SpreadElement>()) {
            continue;  // `...x + 1` is not an expression
          }
          replacement = Wrap(cg, e, gen, exp_left);
          how = "wrap";
          break;
        }
      }
      if (replacement == nullptr) {
        continue;
      }
      if (under_delete && replacement->Is<Identifier>()) {
        continue;
      }
      std::string old = Describe(e);
      if (ReplaceAnchored(b, e, replacement) == nullptr) {
        continue;
      }
      ctx.detail = "expression `";
      ctx.detail += old;
      ctx.detail += "` -> ";
      ctx.detail += how;
      return true;
    }
    return false;
  }

 private:
  // Wraps `e` in a new expression that uses it. `e` is detached from its
  // parent by the caller's Replace; it may be reused inside the wrapper.
  Expression* Wrap(CodeGen& cg, Expression* e, GenContext& gen, bool exp_left) {
    Random& r = cg.rng();
    Tree* t = cg.tree();
    switch (r.Index(14)) {
      case 0:
        return mk::Binary(t, CodeGen::RandomArithmeticOp(r), e,
                          cg.Value(JsType::kNumber, 1));
      case 1:
        return mk::Binary(t, CodeGen::RandomArithmeticOp(r),
                          cg.Value(JsType::kUnknown, 1), e);
      case 2:
        return mk::Call(t,
                        mk::Id(t, r.Pick({"String", "Number", "Boolean",
                                          "Object", "BigInt", "Symbol"})),
                        {e});
      case 3:
        return mk::CallMethod(t, mk::Id(t, "Math"),
                              r.Pick({"abs", "floor", "round", "sqrt", "trunc",
                                      "fround", "sign"}),
                              {e});
      case 4:
        return mk::Array(t, {e});
      case 5:
        return mk::Object(t, {mk::Prop(t, "x", e)});
      case 6:
        if (exp_left) {
          return nullptr;
        }
        return mk::Unary(t, CodeGen::RandomUnaryOp(r), e);
      case 7:
        return mk::Cond(t, cg.Condition(1), e, cg.Value(JsType::kUnknown, 1));
      case 8:
        return mk::Cond(t, cg.Condition(1), cg.Value(JsType::kUnknown, 1), e);
      case 9:
        return mk::Seq(t, {cg.Value(JsType::kUnknown, 1), e});
      case 10:
        if (gen.syntax.in_async && !gen.syntax.in_static_block && !exp_left) {
          return mk::Await(t, e);
        }
        return mk::Call(t, mk::ArrowExpr(t, {}, e), {});
      case 11:
        return mk::Logical(t, CodeGen::RandomLogicalOp(r), e,
                           cg.Value(JsType::kUnknown, 1));
      case 12:
        return mk::CallMethod(
            t, mk::Id(t, "JSON"), "parse",
            {mk::CallMethod(t, mk::Id(t, "JSON"), "stringify", {e})});
      default: {
        std::string_view p = gen.builder.FreshName("x");
        return mk::Call(t, mk::ArrowExpr(t, {p}, mk::Id(t, p)), {e});
      }
    }
  }
};

// --- Wrap mutator
// ---------------------------------------------------------------------

REDLINE_TUNABLE(kWrapMaxAttempts, "mutators.Wrap.max_attempts", 8, 1, 16,
                "statement slots the Wrap mutator tries before giving up");
REDLINE_TUNABLE(kWrapMaxExtraStatements, "mutators.Wrap.max_extra_statements",
                3, 0, 64,
                "most statements after the first that one wrapper encloses");
REDLINE_TUNABLE(
    kWrapExtraStatementChance, "mutators.Wrap.extra_statement_chance", 0.4, 0,
    1, "probability of extending the wrapped run by one more statement");

class WrapMutator final : public Mutator {
 public:
  // No Fuzzilli counterpart: weight 0 by default.
  WrapMutator()
      : Mutator("Wrap", "wraps statements in a loop, if, try, label or IIFE", 0,
                false) {}

  bool Mutate(Builder& b, MutationContext& ctx) override {
    Random& r = b.rng();
    std::vector<Builder::Slot> slots = b.StatementSlots();
    for (int attempt = 0; attempt < kWrapMaxAttempts.AsInt() && !slots.empty();
         attempt++) {
      Builder::Slot slot = slots[r.Index(slots.size())];
      Statement* first = slot.statement();
      if (first == nullptr || IsDirective(first)) {
        continue;
      }
      // A run of 1..4 statements.
      size_t count =
          1 + static_cast<size_t>(r.SmallCount(kWrapMaxExtraStatements.AsInt(),
                                               kWrapExtraStatementChance()));
      count = std::min(count, slot.list->size() - slot.index);
      std::vector<Statement*> run;
      Requirements req;
      for (size_t i = 0; i < count; i++) {
        Statement* s = (*slot.list)[slot.index + i];
        if (IsDirective(s)) {
          break;
        }
        Requirements sr = AnalyzeRequirements(s);
        req.needs_function |= sr.needs_function;
        req.needs_async |= sr.needs_async;
        req.needs_generator |= sr.needs_generator;
        req.needs_break |= sr.needs_break;
        req.needs_continue |= sr.needs_continue;
        req.needs_sloppy |= sr.needs_sloppy;
        req.needs_arguments |= sr.needs_arguments;
        req.needs_super |= sr.needs_super;
        req.needs_new_target |= sr.needs_new_target;
        req.has_external_label |= sr.has_external_label;
        run.push_back(s);
      }
      if (run.empty()) {
        continue;
      }
      // Moving block-scoped declarations into a block hides them from the
      // code that uses them; `var`s survive every wrapper except the IIFE.
      std::vector<Node*> run_nodes(run.begin(), run.end());
      if (!b.EscapingBindings(run_nodes, /*block_scoped_only=*/true).empty()) {
        continue;
      }
      bool vars_escape =
          !b.EscapingBindings(run_nodes, /*block_scoped_only=*/false).empty();
      Tree* t = b.tree();
      GenContext gen = ContextForSlot(b, slot, ctx);
      CodeGen cg(gen);
      Statement* wrapper = nullptr;
      std::string how;
      switch (r.Index(8)) {
        case 0:
        case 1: {
          std::string_view counter = b.FreshName("i");
          static const double kCounts[] = {2, 3, 5, 10, 100, 1000, 10000};
          wrapper = mk::CountedLoop(t, counter, mk::Num(t, kCounts[r.Index(7)]),
                                    mk::Block(t, run));
          how = "loop";
          break;
        }
        case 2:
          wrapper = mk::If(t, cg.Condition(1), mk::Block(t, run));
          how = "if";
          break;
        case 3:
          wrapper = mk::Try(t, run, b.FreshName("e"), {});
          how = "try";
          break;
        case 4: {
          std::vector<Statement*> finalizer;
          wrapper = mk::Try(t, run, {}, {}, &finalizer);
          how = "try-finally";
          break;
        }
        case 5: {
          std::string_view label = b.FreshName("L");
          wrapper = mk::Labeled(t, label, mk::Block(t, run));
          how = "label";
          break;
        }
        case 6: {
          if (req.needs_generator || req.needs_break || req.needs_continue ||
              req.has_external_label || vars_escape) {
            continue;
          }
          Expression* fn = mk::Arrow(t, {}, run, req.needs_async);
          wrapper = mk::ExprStmt(t, mk::Call(t, fn, {}));
          how = "iife";
          break;
        }
        default: {
          std::string_view e = b.FreshName("e");
          wrapper = mk::ForOf(t, VariableKind::kConst, e, cg.ArrayLiteral(1),
                              mk::Block(t, run));
          how = "for-of";
          break;
        }
      }
      // Replace the run by the wrapper.
      slot.list->erase(
          slot.list->begin() + static_cast<std::ptrdiff_t>(slot.index),
          slot.list->begin() +
              static_cast<std::ptrdiff_t>(slot.index + run.size()));
      b.Invalidate();
      b.Insert(slot, wrapper);
      ctx.detail =
          "wrapped " + std::to_string(run.size()) + " statement(s) in " + how;
      return true;
    }
    return false;
  }
};

// --- Duplicate / Reorder
// ------------------------------------------------------------------

REDLINE_TUNABLE(kDuplicateMaxAttempts, "mutators.Duplicate.max_attempts", 8, 1,
                16,
                "statement slots the Duplicate mutator tries before giving up");

class DuplicateMutator final : public Mutator {
 public:
  // No Fuzzilli counterpart: weight 0 by default.
  DuplicateMutator()
      : Mutator("Duplicate", "duplicates a statement", 0, false) {}

  bool Mutate(Builder& b, MutationContext& ctx) override {
    Random& r = b.rng();
    std::vector<Builder::Slot> slots = b.StatementSlots();
    for (int attempt = 0;
         attempt < kDuplicateMaxAttempts.AsInt() && !slots.empty(); attempt++) {
      Builder::Slot slot = slots[r.Index(slots.size())];
      Statement* s = slot.statement();
      if (s == nullptr || IsDirective(s) || IsLexicalDeclaration(s)) {
        continue;
      }
      if (s->Is<FunctionDeclaration>()) {
        // Duplicate function declarations are only fine in sloppy
        // function/script scope.
        Builder::Context c = b.ContextAtSlot(slot);
        bool function_body = slot.owner->Is<BlockStatement>() &&
                             b.ParentOf(slot.owner) != nullptr &&
                             b.ParentOf(slot.owner)->IsFunction();
        if (c.strict || !(slot.owner->Is<fast::Program>() || function_body)) {
          continue;
        }
      }
      Statement* copy = b.tree()->Clone(s);
      Builder::Slot where = slot;
      where.index = r.Coin() ? slot.index + 1
                             : static_cast<size_t>(r.Range(
                                   static_cast<int64_t>(slot.index) + 1,
                                   static_cast<int64_t>(slot.list->size())));
      b.Insert(where, copy);
      ctx.detail = std::string("duplicated ") + s->kind_name() + " `" +
                   Describe(s) + "`";
      return true;
    }
    return false;
  }
};

REDLINE_TUNABLE(kReorderMaxAttempts, "mutators.Reorder.max_attempts", 8, 1, 16,
                "statement slots the Reorder mutator tries before giving up");

class ReorderMutator final : public Mutator {
 public:
  // No Fuzzilli counterpart: weight 0 by default.
  ReorderMutator()
      : Mutator("Reorder", "swaps two adjacent statements", 0, false) {}

  bool Mutate(Builder& b, MutationContext& ctx) override {
    Random& r = b.rng();
    std::vector<Builder::Slot> slots = b.StatementSlots();
    for (int attempt = 0;
         attempt < kReorderMaxAttempts.AsInt() && !slots.empty(); attempt++) {
      Builder::Slot slot = slots[r.Index(slots.size())];
      if (slot.index + 1 >= slot.list->size()) {
        continue;
      }
      Statement* a = (*slot.list)[slot.index];
      Statement* c = (*slot.list)[slot.index + 1];
      if (IsDirective(a) || IsDirective(c)) {
        continue;
      }
      // Do not move a declaration past a use of it (TDZ, undefined values).
      if (Uses(b, c, b.EscapingBindings({a}, false)) ||
          Uses(b, a, b.EscapingBindings({c}, false))) {
        continue;
      }
      std::swap((*slot.list)[slot.index], (*slot.list)[slot.index + 1]);
      b.Invalidate();
      ctx.detail =
          std::string("swapped ") + a->kind_name() + " and " + c->kind_name();
      return true;
    }
    return false;
  }
};

// --- Call mutator
// ------------------------------------------------------------------------

REDLINE_TUNABLE(kCallMaxAttempts, "mutators.Call.max_attempts", 8, 1, 16,
                "call/new expressions the Call mutator tries before giving up");

class CallMutator final : public Mutator {
 public:
  // Closest Fuzzilli counterpart: PropertyAccessorMutator (weight 1, shared
  // with Expression), up to 3 simultaneous.
  CallMutator()
      : Mutator("Call",
                "adds/removes/spreads arguments, swaps callees, call <-> new",
                0.5, false, 3) {}

  bool Mutate(Builder& b, MutationContext& ctx) override {
    Random& r = b.rng();
    std::vector<Node*> candidates = b.Collect([&](Node* n) {
      if (auto* c = n->As<CallExpression>()) {
        return !c->callee->Is<Super>() && !c->optional;
      }
      return n->Is<NewExpression>();
    });
    for (int attempt = 0;
         attempt < kCallMaxAttempts.AsInt() && !candidates.empty(); attempt++) {
      Node* node = candidates[r.Index(candidates.size())];
      Node* parent = b.ParentOf(node);
      if (parent == nullptr) {
        continue;
      }
      if (parent->Is<ChainExpression>()) {
        continue;
      }
      GenContext gen = ContextForNode(b, node, ctx);
      CodeGen cg(gen);
      Tree* t = b.tree();
      std::vector<Expression*>* args =
          node->Is<CallExpression>() ? &node->Cast<CallExpression>()->arguments
                                     : &node->Cast<NewExpression>()->arguments;
      Expression*& callee = node->Is<CallExpression>()
                                ? node->Cast<CallExpression>()->callee
                                : node->Cast<NewExpression>()->callee;
      switch (r.Index(6)) {
        case 0:
        case 1: {
          size_t at = r.Index(args->size() + 1);
          args->insert(args->begin() + static_cast<std::ptrdiff_t>(at),
                       cg.Value(JsType::kUnknown, 1));
          ctx.detail = "added argument";
          break;
        }
        case 2: {
          if (args->empty()) {
            continue;
          }
          size_t at = r.Index(args->size());
          Expression* old = (*args)[at];
          if (old->Is<SpreadElement>()) {
            continue;
          }
          (*args)[at] =
              r.Coin()
                  ? static_cast<Expression*>(mk::Spread(t, mk::Array(t, {old})))
                  : cg.Value(JsType::kUnknown, 1);
          ctx.detail = "replaced argument";
          break;
        }
        case 3: {
          Identifier* f = cg.Variable(JsType::kFunction);
          if (f == nullptr) {
            f = cg.Variable(JsType::kClass);
          }
          if (f == nullptr) {
            continue;
          }
          if (callee->Is<Identifier>() &&
              callee->Cast<Identifier>()->name == f->name) {
            continue;
          }
          ctx.detail = "callee -> " + std::string(f->name);
          callee = f;
          break;
        }
        case 4: {
          // call <-> new
          if (node->Is<CallExpression>()) {
            if (callee->Is<MemberExpression>() &&
                callee->Cast<MemberExpression>()->optional) {
              continue;
            }
            if (callee->Is<ChainExpression>() ||
                callee->Is<ArrowFunctionExpression>()) {
              continue;
            }
            NewExpression* n = mk::New(t, callee, *args);
            if (ReplaceAnchored(b, node, n) == nullptr) {
              continue;
            }
            ctx.detail = "call -> new";
          } else {
            CallExpression* c = mk::Call(t, callee, *args);
            if (ReplaceAnchored(b, node, c) == nullptr) {
              continue;
            }
            ctx.detail = "new -> call";
          }
          return true;
        }
        default: {
          if (!node->Is<CallExpression>()) {
            continue;
          }
          // f(args) -> f.call(thisArg, args) / Reflect.apply
          std::vector<Expression*> new_args = {cg.ObjectLike()};
          new_args.insert(new_args.end(), args->begin(), args->end());
          CallExpression* c = mk::CallMethod(
              t, callee, r.Coin() ? "call" : "bind", std::move(new_args));
          if (ReplaceAnchored(b, node, c) == nullptr) {
            continue;
          }
          ctx.detail = "call -> .call/.bind";
          return true;
        }
      }
      b.Invalidate();
      return true;
    }
    return false;
  }
};

// --- Splice mutator
// ---------------------------------------------------------------------

REDLINE_TUNABLE(kSpliceMinDonorNodes, "mutators.Splice.min_donor_nodes", 2, 1,
                1024,
                "fewest AST nodes a donor statement must have to be spliced");
REDLINE_TUNABLE(kSpliceMaxDonorNodes, "mutators.Splice.max_donor_nodes", 400, 1,
                4096,
                "most AST nodes a donor statement may have to be spliced");
REDLINE_TUNABLE(kSpliceMaxAttempts, "mutators.Splice.max_attempts", 6, 1, 16,
                "donor statements the Splice mutator tries before giving up");
REDLINE_TUNABLE(
    kSpliceSlotAttempts, "mutators.Splice.slot_attempts", 8, 1, 16,
    "insertion slots drawn to find one compatible with the donor statement");
REDLINE_TUNABLE(kSpliceAnyTypeRewireChance,
                "mutators.Splice.any_type_rewire_chance", 0.5, 0, 1,
                "probability of rewiring an external input to a variable of "
                "any type when none of its type is visible");

// Clones a statement from a donor tree into the destination, renaming the
// donor's own bindings to avoid clashes and rewiring references to
// variables declared outside the donor statement to variables visible at
// the destination (or fresh declarations).
class SpliceMutator final : public Mutator {
 public:
  // Fuzzilli: SpliceMutator, weight 2, up to 7 simultaneous.
  SpliceMutator()
      : Mutator(
            "Splice",
            "copies a statement from another program and rewires its inputs", 2,
            true, 7) {}

  bool Mutate(Builder& b, MutationContext& ctx) override {
    if (ctx.partner == nullptr) {
      return false;
    }
    Random& r = b.rng();
    std::unique_ptr<Tree> donor_tree = ctx.partner->CopyTree();
    Random donor_rng = r.Fork();
    Builder donor(donor_tree.get(), &donor_rng);
    std::vector<Node*> candidates = donor.Collect([&](Node* n) {
      if (!n->IsStatement()) {
        return false;
      }
      auto* s = n->Cast<Statement>();
      if (IsDirective(s) || s->Is<EmptyStatement>() ||
          s->Is<BreakStatement>() || s->Is<ContinueStatement>() ||
          s->Is<ReturnStatement>() || s->Is<DebuggerStatement>()) {
        return false;
      }
      // Loop heads (`for (const e of ...)`) are declarations without
      // initializers: not statements on their own.
      if (s->Is<VariableDeclaration>()) {
        Node* parent = donor.ParentOf(n);
        if (parent != nullptr &&
            (parent->Is<ForStatement>() || parent->Is<ForInStatement>() ||
             parent->Is<ForOfStatement>())) {
          return false;
        }
      }
      // Code using a private name of its class cannot leave it.
      if (UsesPrivateNameOfEnclosingClass(n)) {
        return false;
      }
      size_t size = CountNodes(n);
      size_t min_nodes = static_cast<size_t>(kSpliceMinDonorNodes.AsInt());
      size_t max_nodes = static_cast<size_t>(kSpliceMaxDonorNodes.AsInt());
      return size >= min_nodes && size <= max_nodes;
    });
    if (candidates.empty()) {
      return false;
    }
    for (int attempt = 0; attempt < kSpliceMaxAttempts.AsInt(); attempt++) {
      Node* donor_node = candidates[r.Index(candidates.size())];
      Requirements req = AnalyzeRequirements(donor_node);
      // Find a compatible insertion slot.
      Builder::Slot slot;
      bool found = false;
      for (int i = 0; i < kSpliceSlotAttempts.AsInt(); i++) {
        slot = b.RandomInsertionSlot();
        if (!slot.valid()) {
          return false;
        }
        if (Compatible(req, b.ContextAtSlot(slot))) {
          found = true;
          break;
        }
      }
      if (!found) {
        continue;
      }
      if (Splice(b, donor, donor_node->Cast<Statement>(), slot, ctx)) {
        return true;
      }
    }
    return false;
  }

 private:
  bool Splice(Builder& b, Builder& donor, Statement* donor_stmt,
              const Builder::Slot& slot, MutationContext& ctx) {
    Random& r = b.rng();
    Tree* t = b.tree();
    ScopeAnalysis& donor_scopes = donor.scopes();
    // Nodes inside the donor statement.
    absl::flat_hash_set<const Node*> inside;
    Walk(donor_stmt, [&](Node* n) {
      inside.insert(n);
      return true;
    });
    // Clone and map donor nodes to clone nodes.
    Statement* clone = t->Clone(donor_stmt);
    absl::flat_hash_map<const Node*, Node*> to_clone;
    ParallelWalk(donor_stmt, clone, [&](Node* a, Node* c) { to_clone[a] = c; });
    // Classify bindings touched by the donor. They are collected in the
    // order of their first occurrence in the (source-ordered) walk so that
    // the renaming below is deterministic for a given seed.
    std::vector<Binding*> internal, external;
    absl::flat_hash_set<Binding*> seen;
    Walk(donor_stmt, [&](Node* n) {
      if (auto* id = n->As<Identifier>()) {
        Binding* binding = donor_scopes.BindingOf(id);
        if (binding == nullptr || !seen.insert(binding).second) {
          return true;
        }
        bool declared_inside = false;
        for (Identifier* d : binding->declarations) {
          if (inside.count(d)) {
            declared_inside = true;
          }
        }
        (declared_inside ? internal : external).push_back(binding);
      }
      return true;
    });
    // Names in the destination and in the donor must not collide with fresh
    // names.
    b.ReserveNames(donor_scopes.all_names());
    const absl::flat_hash_set<std::string_view>& dest_names =
        b.scopes().all_names();
    std::vector<Variable> visible = b.VisibleAtSlot(slot);
    auto rename = [&](Binding* binding, std::string_view new_name) {
      std::string_view interned = t->Intern(new_name);
      for (Identifier* d : binding->declarations) {
        auto it = to_clone.find(d);
        if (it != to_clone.end()) {
          it->second->Cast<Identifier>()->name = interned;
        }
      }
      for (Identifier* ref : binding->references) {
        auto it = to_clone.find(ref);
        if (it != to_clone.end()) {
          it->second->Cast<Identifier>()->name = interned;
        }
      }
    };
    int renamed = 0, rewired = 0, declared = 0;
    for (Binding* binding : internal) {
      if (dest_names.count(binding->name) || binding->name == "arguments") {
        if (binding->name == "arguments") {
          continue;
        }
        rename(binding, b.FreshName(binding->name));
        renamed++;
      }
    }
    std::vector<Statement*> prologue;
    for (Binding* binding : external) {
      if (binding->name == "arguments") {
        continue;
      }
      JsType type = donor.GuessType(binding);
      std::vector<const Variable*> options;
      for (const Variable& v : visible) {
        if (v.name == "arguments") {
          continue;
        }
        if (binding->assigned && v.is_constant) {
          continue;
        }
        if (type == JsType::kUnknown || v.type == type) {
          options.push_back(&v);
        }
      }
      if (options.empty() && r.Chance(kSpliceAnyTypeRewireChance())) {
        for (const Variable& v : visible) {
          if (v.name == "arguments" || (binding->assigned && v.is_constant)) {
            continue;
          }
          options.push_back(&v);
        }
      }
      if (!options.empty()) {
        const Variable& choice = *options[r.Index(options.size())];
        rename(binding, choice.name);
        rewired++;
      } else {
        // Declare a stand-in before the spliced statement.
        std::string_view name = dest_names.count(binding->name)
                                    ? b.FreshName(binding->name)
                                    : t->Intern(binding->name);
        if (name != binding->name) {
          rename(binding, name);
        }
        GenContext gen = ContextForSlot(b, slot, ctx);
        CodeGen cg(gen);
        prologue.push_back(mk::Let(t, name, cg.Literal(type)));
        declared++;
      }
    }
    prologue.push_back(clone);
    b.InsertAll(slot, prologue);
    ctx.detail = StringPrintf(
        "spliced %s (%zu nodes; %d renamed, %d rewired, %d declared)",
        donor_stmt->kind_name(), CountNodes(donor_stmt), renamed, rewired,
        declared);
    return true;
  }
};

// --- Combine mutator
// -----------------------------------------------------------------------

REDLINE_TUNABLE(kCombineIifeChance, "mutators.Combine.iife_chance", 0.5, 0, 1,
                "probability of merging the partner program as an IIFE rather "
                "than flat at the top level");
REDLINE_TUNABLE(kCombineSlotAttempts, "mutators.Combine.slot_attempts", 8, 1,
                16,
                "insertion slots drawn to find one whose strictness suits the "
                "partner program");
REDLINE_TUNABLE(kCombineIifeAtEndChance, "mutators.Combine.iife_at_end_chance",
                0.6, 0, 1,
                "probability that the IIFE goes at the end of the program "
                "rather than at a random slot");
REDLINE_TUNABLE(kCombineFlatAtEndChance, "mutators.Combine.flat_at_end_chance",
                0.5, 0, 1,
                "probability that the flat merge is appended at the end rather "
                "than at a random top-level index");

class CombineMutator final : public Mutator {
 public:
  // Fuzzilli: CombineMutator, weight 1, one program per application.
  CombineMutator()
      : Mutator("Combine",
                "merges another whole program in (as an IIFE or flat with "
                "renaming)",
                1, true) {}

  bool Mutate(Builder& b, MutationContext& ctx) override {
    if (ctx.partner == nullptr) {
      return false;
    }
    Random& r = b.rng();
    std::unique_ptr<Tree> donor_tree = ctx.partner->CopyTree();
    Random donor_rng = r.Fork();
    Builder donor(donor_tree.get(), &donor_rng);
    fast::Program* donor_root = donor.root();
    if (donor_root->body.empty()) {
      return false;
    }
    Requirements req = AnalyzeRequirements(donor_root);
    Tree* t = b.tree();
    if (r.Chance(kCombineIifeChance())) {
      // IIFE: `(function() { <donor> })();` at a random insertion slot.
      Builder::Slot slot;
      bool found = false;
      for (int i = 0; i < kCombineSlotAttempts.AsInt(); i++) {
        slot = r.Chance(kCombineIifeAtEndChance())
                   ? Builder::Slot{&b.root()->body, b.root()->body.size(),
                                   b.root()}
                   : b.RandomInsertionSlot();
        if (!slot.valid()) {
          return false;
        }
        Builder::Context c = b.ContextAtSlot(slot);
        if (!req.needs_sloppy || !c.strict) {
          found = true;
          break;
        }
      }
      if (!found) {
        return false;
      }
      std::vector<Statement*> body;
      body.reserve(donor_root->body.size());
      for (Statement* s : donor_root->body) {
        body.push_back(t->Clone(s));
      }
      Expression* fn = mk::Function(t, "", {}, std::move(body));
      b.Insert(slot, mk::ExprStmt(t, mk::Call(t, fn, {})));
      ctx.detail = StringPrintf("combined as IIFE (%zu statements)",
                                donor_root->body.size());
      return true;
    }
    // Flat merge into the top level, renaming clashing top-level bindings.
    if (req.needs_sloppy && b.root()->strict) {
      return false;
    }
    ScopeAnalysis& donor_scopes = donor.scopes();
    b.ReserveNames(donor_scopes.all_names());
    const absl::flat_hash_set<std::string_view>& dest_names =
        b.scopes().all_names();
    int renamed = 0;
    std::vector<Binding*> clashing;
    for (Binding* binding : donor_scopes.root()->bindings) {
      if (dest_names.count(binding->name)) {
        clashing.push_back(binding);
      }
    }
    for (Binding* binding : clashing) {
      RenameBinding(donor_tree.get(), binding, b.FreshName(binding->name));
      renamed++;
    }
    std::vector<Statement*> statements;
    for (Statement* s : donor_root->body) {
      if (IsDirective(s)) {
        continue;
      }
      statements.push_back(t->Clone(s));
    }
    if (statements.empty()) {
      return false;
    }
    std::vector<Statement*>& dest = b.root()->body;
    size_t first = 0;
    while (first < dest.size() && IsDirective(dest[first])) {
      first++;
    }
    size_t index =
        r.Chance(kCombineFlatAtEndChance())
            ? dest.size()
            : static_cast<size_t>(r.Range(static_cast<int64_t>(first),
                                          static_cast<int64_t>(dest.size())));
    Builder::Slot slot{&dest, index, b.root()};
    b.InsertAll(slot, statements);
    ctx.detail = StringPrintf("combined flat (%zu statements, %d renamed)",
                              statements.size(), renamed);
    return true;
  }
};

}  // namespace

// --- MutatorSet
// ------------------------------------------------------------------------------

// static
std::unique_ptr<MutatorSet> MutatorSet::CreateDefault() {
  auto set = std::make_unique<MutatorSet>();
  auto add = [&](std::unique_ptr<Mutator> m) {
    set->candidates_.push_back(m.get());
    set->mutators_.push_back(std::move(m));
  };
  // Fuzzilli: CodeGenMutator, weight 2, 1-3 insertion points per
  // application, each with about five instructions' worth of new code
  // (defaultCodeGenerationAmount) = 1-3 generator outputs here.
  add(std::make_unique<InsertMutator>("CodeGen",
                                      "inserts freshly generated statements", 2,
                                      std::vector<std::string>{}, false,
                                      /*min_count=*/1,
                                      /*extra_count=*/2,
                                      /*at_top=*/false,
                                      /*max_simultaneous=*/3));
  add(std::make_unique<ExploreMutator>());
  add(std::make_unique<ProbingMutator>());
  // Weight 0: never picked at random; the worker applies it to the parent of
  // every mutation chain so that the mutators have fresh values to work with
  // (10-15 values, like Fuzzilli's buildPrefix()). Only generators that
  // cannot throw: an exception in the prefix would fail every mutant of the
  // chain.
  add(std::make_unique<InsertMutator>(
      "Prefix",
      "fresh value declarations at the top of a mutation chain's parent", 0,
      std::vector<std::string>{"IntegerDecl", "FloatDecl", "StringDecl",
                               "BooleanDecl", "BigIntDecl", "RegExpDecl",
                               "SymbolDecl", "ArrayDecl", "ObjectDecl",
                               "TypedArrayDecl", "ArrowDecl", "TemplateDecl"},
      false,
      /*min_count=*/10,
      /*extra_count=*/5,
      /*at_top=*/true));
  add(std::make_unique<SpliceMutator>());
  add(std::make_unique<CombineMutator>());
  add(std::make_unique<InputMutator>());
  add(std::make_unique<OperationMutator>());
  add(std::make_unique<ExpressionMutator>());
  add(std::make_unique<CallMutator>());
  add(std::make_unique<RemoveMutator>());
  add(std::make_unique<WrapMutator>());
  add(std::make_unique<DuplicateMutator>());
  add(std::make_unique<ReorderMutator>());
  // No Fuzzilli counterpart as a mutator (Fuzzilli reaches the JIT through
  // its code generators, which CodeGen also has): weight 0 by default.
  add(std::make_unique<InsertMutator>(
      "JitStress",
      "inserts JIT tiering/deopt/GC idioms around existing functions", 0,
      std::vector<std::string>{"JitOptimize", "JitBaseline", "Deopt", "OsrLoop",
                               "NativeCall", "Gc", "ElementsKind",
                               "ShapeChange"},
      true));
  std::string error;
  if (!set->SetSelector(
          std::make_unique<WeightedSelector<Mutator>>(
              "mutator", std::vector<std::pair<std::string, double>>{}),
          &error)) {
    REDLINE_ERROR("mutators", "default mutator strategy: %s", error.c_str());
    return nullptr;
  }
  return set;
}

bool MutatorSet::ConfigurePrefix(int min_values, int extra_values) {
  Mutator* prefix = Find("Prefix");
  return prefix != nullptr && prefix->SetInsertCounts(min_values, extra_values);
}

bool MutatorSet::SetSelector(std::unique_ptr<Selector<Mutator>> selector,
                             std::string* error) {
  if (!selector->Initialize(candidates_, error)) {
    return false;
  }
  selector_ = std::move(selector);
  return true;
}

Mutator* MutatorSet::Find(std::string_view name) const {
  for (const auto& m : mutators_) {
    if (m->name() == name) {
      return m.get();
    }
  }
  return nullptr;
}

std::vector<std::string> MutatorSet::Names() const {
  std::vector<std::string> names;
  names.reserve(mutators_.size());
  for (const auto& m : mutators_) {
    names.push_back(m->name());
  }
  return names;
}

Mutator* MutatorSet::Pick(Random& rng, bool has_partner) const {
  size_t index =
      selector_->Pick(candidates_, rng, [has_partner](const Mutator& m) {
        return has_partner || !m.needs_partner();
      });
  return index == Selector<Mutator>::kNoChoice ? nullptr : candidates_[index];
}

// --- selection strategies for mutators -------------------------------------
//
// To add one: derive from Selector<Mutator>, then add a Register() line
// below. Parameters are declared in the Entry so that the configuration is
// validated before the fuzzer starts.

template <>
void RegisterBuiltinSelectors<Mutator>(SelectorRegistry<Mutator>& registry) {
  registry.Register({std::string(WeightedSelector<Mutator>::kName),
                     "one weighted random draw among the eligible mutators",
                     WeightedSelector<Mutator>::Specs(),
                     [](const JsonValue& params, std::string* error)
                         -> std::unique_ptr<Selector<Mutator>> {
                       std::vector<std::pair<std::string, double>> overrides;
                       if (!ReadWeightOverrides(params, &overrides, error)) {
                         return nullptr;
                       }
                       // Names are checked against the mutators in
                       // Initialize().
                       return std::make_unique<WeightedSelector<Mutator>>(
                           "mutator", std::move(overrides));
                     }});
  registry.SetDefault(WeightedSelector<Mutator>::kName);
  registry.Register({std::string(AdaptiveSelector<Mutator>::kName),
                     "weighted draw whose weights follow each mutator's recent "
                     "yield (new corpus programs per execution)",
                     AdaptiveSelector<Mutator>::Specs(),
                     [](const JsonValue& params, std::string* error)
                         -> std::unique_ptr<Selector<Mutator>> {
                       std::vector<std::pair<std::string, double>> overrides;
                       if (!ReadWeightOverrides(params, &overrides, error)) {
                         return nullptr;
                       }
                       AdaptiveSelector<Mutator>::Settings p =
                           AdaptiveSelector<Mutator>::ReadParams(params);
                       if (!AdaptiveSelector<Mutator>::Validate(p, error)) {
                         return nullptr;
                       }
                       return std::make_unique<AdaptiveSelector<Mutator>>(
                           "mutator", std::move(overrides), p);
                     }});
}

std::vector<const Component*> MutatorSet::Components() const {
  std::vector<const Component*> components;
  components.reserve(mutators_.size());
  for (const auto& m : mutators_) {
    components.push_back(m.get());
  }
  return components;
}

MutationContext::MutationContext() = default;
MutationContext::~MutationContext() = default;

std::unique_ptr<fast::Tree> ApplyMutation(
    Mutator& mutator, const redline::Program& parent, const ProgramPtr& partner,
    Random& rng, const Environment& env, const GeneratorSet& generators,
    int rounds, std::string* detail, std::vector<ExploreProbe>* probes) {
  std::unique_ptr<Tree> tree = parent.CopyTree();
  Builder builder(tree.get(), &rng);
  MutationContext ctx;
  ctx.partner = partner;
  ctx.env = &env;
  ctx.generators = &generators;
  int applied = 0;
  std::string all;
  // Runtime-assisted mutators instrument once; their probes are completed
  // by the worker.
  if (mutator.runtime_assisted()) {
    rounds = 1;
  }
  for (int i = 0; i < rounds; i++) {
    ctx.detail.clear();
    mutator.stats().invocations++;
    if (mutator.Mutate(builder, ctx)) {
      applied++;
      if (!all.empty()) {
        all += "; ";
      }
      all += ctx.detail;
    }
  }
  if (applied == 0) {
    return nullptr;
  }
  mutator.stats().produced++;
  if (detail != nullptr) {
    *detail = all;
  }
  if (probes != nullptr) {
    *probes = std::move(ctx.probes);
  }
  return tree;
}

}  // namespace redline
