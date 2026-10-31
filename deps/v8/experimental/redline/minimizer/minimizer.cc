// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/redline/minimizer/minimizer.h"

#include <algorithm>
#include <limits>
#include <map>
#include <unordered_set>
#include <utility>

#include "experimental/redline/base/log.h"
#include "experimental/redline/base/time_util.h"
#include "experimental/redline/base/v8_runtime.h"
#include "experimental/redline/core/ast_builder.h"
#include "experimental/redline/core/tunable.h"
#include "experimental/redline/fast/ast_visit.h"
#include "experimental/redline/fast/printer.h"
#include "experimental/redline/fast/scope.h"

namespace redline {

using fast::Expression;
using fast::Node;
using fast::NodeKind;
using fast::Statement;
using StatementList = std::vector<Statement*>;

// ---------------------------------------------------------------------------
// MinimizationHelper

MinimizationHelper::MinimizationHelper(fast::Tree* tree,
                                       const MinimizationOracle& oracle,
                                       SyntaxChecker* checker,
                                       const MinimizerOptions& options)
    : tree_(tree),
      oracle_(oracle),
      checker_(checker),
      options_(options),
      start_us_(MonotonicNowUs()) {}

bool MinimizationHelper::BudgetExhausted() const {
  if (result_.tests >= options_.max_tests) {
    return true;
  }
  if (options_.max_time_ms != 0 &&
      (MonotonicNowUs() - start_us_) / 1000 >= options_.max_time_ms) {
    return true;
  }
  return false;
}

bool MinimizationHelper::Test() {
  if (BudgetExhausted()) {
    result_.budget_exhausted = true;
    return false;
  }
  std::string source = fast::Print(tree_->root());
  if (checker_ != nullptr) {
    std::string message;
    if (!checker_->Accepts(source, &message)) {
      result_.syntax_rejected++;
      if (reducer_ != nullptr) {
        reducer_->stats().invalid++;
      }
      return false;
    }
  }
  result_.tests++;
  Stopwatch stopwatch;
  bool ok = oracle_(source);
  if (reducer_ != nullptr) {
    ComponentStats& stats = reducer_->stats();
    stats.executed++;
    stats.time_us += stopwatch.ElapsedUs();
    if (ok) {
      stats.succeeded++;
    } else {
      stats.failed++;
    }
  }
  if (ok) {
    result_.accepted++;
  }
  return ok;
}

// ---------------------------------------------------------------------------
// Tree helpers

namespace {

template <typename F>
void Walk(Node* node, F& f) {
  if (node == nullptr) {
    return;
  }
  f(node);
  fast::ForEachChildSlot(node, [&](Node** slot) {
    if (*slot != nullptr) {
      Walk(*slot, f);
    }
  });
}

StatementList* StatementListOf(Node* node) {
  switch (node->kind) {
    case NodeKind::kProgram:
      return &node->Cast<fast::Program>()->body;
    case NodeKind::kBlockStatement:
      return &node->Cast<fast::BlockStatement>()->body;
    case NodeKind::kSwitchCase:
      return &node->Cast<fast::SwitchCase>()->consequent;
    case NodeKind::kStaticBlock:
      return &node->Cast<fast::StaticBlock>()->body;
    default:
      return nullptr;
  }
}

// Every statement list reachable from `root`, outermost first.
std::vector<StatementList*> CollectStatementLists(Node* root) {
  std::vector<StatementList*> lists;
  auto visit = [&](Node* node) {
    StatementList* list = StatementListOf(node);
    if (list != nullptr) {
      lists.push_back(list);
    }
  };
  Walk(root, visit);
  return lists;
}

// The statements of a body: the block's statements or the statement itself.
void BodyStatements(Statement* body, std::vector<Statement*>* out) {
  if (body == nullptr) {
    return;
  }
  if (auto* block = body->As<fast::BlockStatement>()) {
    out->insert(out->end(), block->body.begin(), block->body.end());
  } else {
    out->push_back(body);
  }
}

bool IsUnlabeledBreak(const Statement* s) {
  const auto* b = s->As<fast::BreakStatement>();
  return b != nullptr && b->label == nullptr;
}

// Removes a contiguous range of a list, tests, and restores on failure.
// Returns whether the removal was kept.
template <typename T>
bool TryRemoveRange(std::vector<T*>& list, size_t begin, size_t end,
                    MinimizationHelper& helper) {
  std::vector<T*> removed(list.begin() + static_cast<ptrdiff_t>(begin),
                          list.begin() + static_cast<ptrdiff_t>(end));
  list.erase(list.begin() + static_cast<ptrdiff_t>(begin),
             list.begin() + static_cast<ptrdiff_t>(end));
  if (helper.Test()) {
    return true;
  }
  list.insert(list.begin() + static_cast<ptrdiff_t>(begin), removed.begin(),
              removed.end());
  return false;
}

// Delta-debugging style trimming of a list: tries to remove chunks of
// halving size, from the end of the list (later elements rarely have
// dependants). Never shrinks the list below `min_size`.
template <typename T>
int TrimList(std::vector<T*>& list, size_t min_size,
             MinimizationHelper& helper) {
  int accepted = 0;
  if (list.size() <= min_size) {
    return 0;
  }
  size_t chunk = std::max<size_t>(1, list.size() / 2);
  while (!helper.BudgetExhausted()) {
    size_t end = list.size();
    while (end > 0 && list.size() > min_size && !helper.BudgetExhausted()) {
      size_t begin = end > chunk ? end - chunk : 0;
      // Respect the minimum size.
      if (list.size() - (end - begin) < min_size) {
        begin = end - (list.size() - min_size);
      }
      if (begin >= end) {
        break;
      }
      if (TryRemoveRange(list, begin, end, helper)) {
        accepted++;
      }
      end = begin;
    }
    if (chunk == 1) {
      break;
    }
    chunk = std::max<size_t>(1, chunk / 2);
  }
  return accepted;
}

// Removes trailing elements one at a time until the oracle objects.
template <typename T>
int TrimTrailing(std::vector<T*>& list, size_t min_size,
                 MinimizationHelper& helper) {
  int accepted = 0;
  while (list.size() > min_size && !helper.BudgetExhausted()) {
    T* last = list.back();
    list.pop_back();
    if (helper.Test()) {
      accepted++;
    } else {
      list.push_back(last);
      break;
    }
  }
  return accepted;
}

void RemoveEmptyStatements(Node* root) {
  for (StatementList* list : CollectStatementLists(root)) {
    list->erase(std::remove_if(
                    list->begin(), list->end(),
                    [](Statement* s) { return s->Is<fast::EmptyStatement>(); }),
                list->end());
  }
}

// ---------------------------------------------------------------------------
// Reducers

// Keeps the shortest prefix of the top-level statements that still passes
// the oracle: what comes after the point where the preserved property is
// reached goes away in about log2(N) tests (binary search on the prefix
// length). Works with any oracle. Runs before `statements`, which then
// works on the shorter program.
class Truncator : public Reducer {
 public:
  Truncator()
      : Reducer("truncate",
                "keep the shortest prefix of the top-level statements") {}

  int Reduce(MinimizationHelper& helper) override {
    StatementList* body = StatementListOf(helper.tree()->root());
    if (body == nullptr || body->size() < 2) {
      return 0;
    }
    const std::vector<Statement*> all = *body;
    // Invariant: the prefix of length `good` passes (the whole program
    // does), prefixes shorter than `lo` were tested and failed.
    size_t lo = 0, good = all.size();
    while (lo < good && !helper.BudgetExhausted()) {
      size_t mid = lo + (good - lo) / 2;
      body->assign(all.begin(), all.begin() + static_cast<ptrdiff_t>(mid));
      if (helper.Test()) {
        good = mid;
      } else {
        lo = mid + 1;
      }
    }
    body->assign(all.begin(), all.begin() + static_cast<ptrdiff_t>(good));
    return good < all.size() ? 1 : 0;
  }
};

// Removes the parts of the program that the rest does not use: the
// top-level statements form a graph in which two statements are connected
// when one references a binding the other declares (scope analysis
// resolves every identifier). Each connected part that does not contain
// every statement is removed with one test. Side effects through globals,
// prototypes or getters can connect statements that share no binding, so
// the graph only proposes the cut; the oracle decides.
class DataflowReducer : public Reducer {
 public:
  DataflowReducer()
      : Reducer("dataflow",
                "remove top-level statements the rest of the program does not "
                "use") {}

  int Reduce(MinimizationHelper& helper) override {
    StatementList* body = StatementListOf(helper.tree()->root());
    if (body == nullptr || body->size() < 2) {
      return 0;
    }
    std::vector<std::vector<size_t>> parts = Parts(helper.tree(), *body);
    if (parts.size() < 2) {
      return 0;
    }
    // Later parts first: later statements rarely have dependants.
    std::sort(parts.begin(), parts.end(),
              [](const auto& a, const auto& b) { return a.back() > b.back(); });
    const std::vector<Statement*> all = *body;
    std::vector<bool> removed(all.size(), false);
    int accepted = 0;
    for (const std::vector<size_t>& part : parts) {
      if (helper.BudgetExhausted()) {
        break;
      }
      for (size_t index : part) {
        removed[index] = true;
      }
      Assign(all, removed, body);
      if (body->empty()) {
        for (size_t index : part) {
          removed[index] = false;
        }
        continue;
      }
      if (helper.Test()) {
        accepted++;
        continue;
      }
      for (size_t index : part) {
        removed[index] = false;
      }
    }
    Assign(all, removed, body);
    return accepted;
  }

 private:
  static void Assign(const std::vector<Statement*>& all,
                     const std::vector<bool>& removed, StatementList* body) {
    body->clear();
    for (size_t i = 0; i < all.size(); i++) {
      if (!removed[i]) {
        body->push_back(all[i]);
      }
    }
  }

  // The connected parts of the def-use graph of the top-level statements,
  // each as a sorted list of statement indices.
  static std::vector<std::vector<size_t>> Parts(fast::Tree* tree,
                                                const StatementList& body) {
    // Which top-level statement each node belongs to.
    constexpr size_t kNone = std::numeric_limits<size_t>::max();
    std::vector<size_t> owner(tree->id_limit(), kNone);
    for (size_t i = 0; i < body.size(); i++) {
      auto mark = [&](Node* node) {
        if (node->id < owner.size()) {
          owner[node->id] = i;
        }
      };
      Walk(body[i], mark);
    }
    std::vector<size_t> parent(body.size());
    for (size_t i = 0; i < parent.size(); i++) {
      parent[i] = i;
    }
    auto find = [&](size_t x) {
      while (parent[x] != x) {
        parent[x] = parent[parent[x]];
        x = parent[x];
      }
      return x;
    };
    std::unique_ptr<fast::ScopeAnalysis> scopes =
        fast::ScopeAnalysis::Analyze(tree);
    auto owner_of = [&](const Node* node) {
      return node != nullptr && node->id < owner.size() ? owner[node->id]
                                                        : kNone;
    };
    for (const fast::Binding* binding : scopes->bindings()) {
      size_t declared = owner_of(binding->identifier);
      if (declared == kNone) {
        continue;
      }
      for (const fast::Identifier* reference : binding->references) {
        size_t used = owner_of(reference);
        if (used != kNone && used != declared) {
          parent[find(used)] = find(declared);
        }
      }
    }
    std::map<size_t, std::vector<size_t>> by_root;
    for (size_t i = 0; i < body.size(); i++) {
      by_root[find(i)].push_back(i);
    }
    std::vector<std::vector<size_t>> parts;
    parts.reserve(by_root.size());
    for (auto& [root, part] : by_root) {
      parts.push_back(std::move(part));
    }
    return parts;
  }
};

// Removes statements: chunks first, then single statements, outermost
// lists first. The workhorse; most of the size reduction comes from here.
class StatementRemover : public Reducer {
 public:
  StatementRemover()
      : Reducer("statements", "remove statements (chunks, then one by one)") {}

  int Reduce(MinimizationHelper& helper) override {
    int accepted = 0;
    std::unordered_set<StatementList*> done;
    while (!helper.BudgetExhausted()) {
      StatementList* list = nullptr;
      for (StatementList* candidate :
           CollectStatementLists(helper.tree()->root())) {
        if (done.count(candidate) == 0) {
          list = candidate;
          break;
        }
      }
      if (list == nullptr) {
        break;
      }
      done.insert(list);
      accepted += TrimList(*list, 0, helper);
    }
    return accepted;
  }
};

// Replaces a compound statement by (parts of) its body: `if (c) { S }` by
// `S`, a loop by its body, `try { S } catch {}` by `S`, an IIFE by its
// statements, and so on. Exposes nested statements to StatementRemover.
class Unwrapper : public Reducer {
 public:
  Unwrapper()
      : Reducer("unwrap",
                "replace blocks, ifs, loops, try, IIFEs by their bodies") {}

  int Reduce(MinimizationHelper& helper) override {
    int accepted = 0;
    std::unordered_set<Statement*> failed;
    bool progress = true;
    while (progress && !helper.BudgetExhausted()) {
      progress = false;
      for (StatementList* list : CollectStatementLists(helper.tree()->root())) {
        for (size_t i = 0; i < list->size() && !helper.BudgetExhausted(); i++) {
          Statement* statement = (*list)[i];
          if (failed.count(statement) != 0) {
            continue;
          }
          std::vector<std::vector<Statement*>> alternatives;
          Alternatives(helper.tree(), statement, &alternatives);
          if (alternatives.empty()) {
            continue;
          }
          bool kept = false;
          for (const std::vector<Statement*>& replacement : alternatives) {
            list->erase(list->begin() + static_cast<ptrdiff_t>(i));
            list->insert(list->begin() + static_cast<ptrdiff_t>(i),
                         replacement.begin(), replacement.end());
            if (helper.Test()) {
              kept = true;
              break;
            }
            list->erase(
                list->begin() + static_cast<ptrdiff_t>(i),
                list->begin() + static_cast<ptrdiff_t>(i + replacement.size()));
            list->insert(list->begin() + static_cast<ptrdiff_t>(i), statement);
          }
          if (kept) {
            accepted++;
            progress = true;
            // The list and the nested lists changed: start over.
            goto restart;
          }
          failed.insert(statement);
        }
      }
    restart: {}
    }
    return accepted;
  }

 private:
  static void Alternatives(fast::Tree* tree, Statement* statement,
                           std::vector<std::vector<Statement*>>* out) {
    switch (statement->kind) {
      case NodeKind::kBlockStatement:
        out->push_back(statement->Cast<fast::BlockStatement>()->body);
        return;
      case NodeKind::kIfStatement: {
        auto* n = statement->Cast<fast::IfStatement>();
        out->emplace_back();
        BodyStatements(n->consequent, &out->back());
        if (n->alternate != nullptr) {
          out->emplace_back();
          BodyStatements(n->alternate, &out->back());
        }
        return;
      }
      case NodeKind::kWhileStatement:
        out->emplace_back();
        BodyStatements(statement->Cast<fast::WhileStatement>()->body,
                       &out->back());
        return;
      case NodeKind::kDoWhileStatement:
        out->emplace_back();
        BodyStatements(statement->Cast<fast::DoWhileStatement>()->body,
                       &out->back());
        return;
      case NodeKind::kForStatement: {
        auto* n = statement->Cast<fast::ForStatement>();
        out->emplace_back();
        if (n->init != nullptr) {
          if (n->init->IsStatement()) {
            out->back().push_back(static_cast<Statement*>(n->init));
          } else if (n->init->IsExpression()) {
            out->back().push_back(
                mk::ExprStmt(tree, static_cast<Expression*>(n->init)));
          }
        }
        BodyStatements(n->body, &out->back());
        return;
      }
      case NodeKind::kForInStatement:
      case NodeKind::kForOfStatement: {
        Node* left;
        Expression* right;
        Statement* body;
        if (auto* fi = statement->As<fast::ForInStatement>()) {
          left = fi->left;
          right = fi->right;
          body = fi->body;
        } else {
          auto* fo = statement->Cast<fast::ForOfStatement>();
          left = fo->left;
          right = fo->right;
          body = fo->body;
        }
        out->emplace_back();
        // Keep the data flow: bind the loop variable to the iterated value.
        if (auto* decl = left->As<fast::VariableDeclaration>()) {
          if (decl->declarations.size() == 1 &&
              decl->declarations[0]->init == nullptr) {
            auto* bound = tree->New<fast::VariableDeclaration>();
            bound->declaration_kind =
                decl->declaration_kind == fast::VariableKind::kConst
                    ? fast::VariableKind::kLet
                    : decl->declaration_kind;
            auto* declarator = tree->New<fast::VariableDeclarator>();
            declarator->target = decl->declarations[0]->target;
            declarator->init = right;
            bound->declarations.push_back(declarator);
            out->back().push_back(bound);
          }
        } else if (left->IsExpression()) {
          out->back().push_back(mk::ExprStmt(
              tree, mk::Assign(tree, static_cast<Expression*>(left), right)));
        }
        BodyStatements(body, &out->back());
        return;
      }
      case NodeKind::kTryStatement: {
        auto* n = statement->Cast<fast::TryStatement>();
        out->emplace_back(n->block->body);
        if (n->finalizer != nullptr) {
          out->back().insert(out->back().end(), n->finalizer->body.begin(),
                             n->finalizer->body.end());
        }
        if (n->handler != nullptr && !n->handler->body->body.empty()) {
          out->emplace_back(n->handler->body->body);
        }
        return;
      }
      case NodeKind::kLabeledStatement:
        out->emplace_back();
        BodyStatements(statement->Cast<fast::LabeledStatement>()->body,
                       &out->back());
        return;
      case NodeKind::kWithStatement:
        out->emplace_back();
        BodyStatements(statement->Cast<fast::WithStatement>()->body,
                       &out->back());
        return;
      case NodeKind::kSwitchStatement: {
        auto* n = statement->Cast<fast::SwitchStatement>();
        out->emplace_back();
        for (fast::SwitchCase* c : n->cases) {
          for (Statement* s : c->consequent) {
            if (!IsUnlabeledBreak(s)) {
              out->back().push_back(s);
            }
          }
        }
        return;
      }
      case NodeKind::kExpressionStatement: {
        // IIFE: (function () { S })() or (() => { S })().
        auto* expression =
            statement->Cast<fast::ExpressionStatement>()->expression;
        auto* call = expression->As<fast::CallExpression>();
        if (call == nullptr || !call->arguments.empty()) {
          return;
        }
        fast::FunctionData* fn = nullptr;
        if (auto* f = call->callee->As<fast::FunctionExpression>()) {
          fn = f;
        }
        if (auto* a = call->callee->As<fast::ArrowFunctionExpression>()) {
          fn = a;
        }
        if (fn == nullptr || fn->body == nullptr || !fn->params.empty()) {
          return;
        }
        out->emplace_back(fn->body->body);
        return;
      }
      default:
        return;
    }
  }
};

// Shortens lists that are not statement lists: call arguments, array
// elements, object properties, class members, declarators, parameters.
class ListTrimmer : public Reducer {
 public:
  ListTrimmer()
      : Reducer("lists",
                "drop call arguments, array elements, properties, parameters") {
  }

  int Reduce(MinimizationHelper& helper) override {
    int accepted = 0;
    auto visit = [&](Node* node) {
      if (helper.BudgetExhausted()) {
        return;
      }
      switch (node->kind) {
        case NodeKind::kCallExpression:
          accepted += TrimList(node->Cast<fast::CallExpression>()->arguments, 0,
                               helper);
          break;
        case NodeKind::kNewExpression:
          accepted +=
              TrimList(node->Cast<fast::NewExpression>()->arguments, 0, helper);
          break;
        case NodeKind::kArrayExpression:
          accepted += TrimList(node->Cast<fast::ArrayExpression>()->elements, 0,
                               helper);
          break;
        case NodeKind::kObjectExpression:
          accepted += TrimList(node->Cast<fast::ObjectExpression>()->properties,
                               0, helper);
          break;
        case NodeKind::kClassBody:
          accepted +=
              TrimList(node->Cast<fast::ClassBody>()->members, 0, helper);
          break;
        case NodeKind::kVariableDeclaration:
          accepted += TrimList(
              node->Cast<fast::VariableDeclaration>()->declarations, 1, helper);
          break;
        case NodeKind::kSequenceExpression:
          accepted += TrimList(
              node->Cast<fast::SequenceExpression>()->expressions, 1, helper);
          break;
        case NodeKind::kSwitchStatement:
          accepted +=
              TrimList(node->Cast<fast::SwitchStatement>()->cases, 0, helper);
          break;
        case NodeKind::kFunctionDeclaration:
          accepted += TrimTrailing(
              node->Cast<fast::FunctionDeclaration>()->params, 0, helper);
          break;
        case NodeKind::kFunctionExpression:
          accepted += TrimTrailing(
              node->Cast<fast::FunctionExpression>()->params, 0, helper);
          break;
        case NodeKind::kArrowFunctionExpression:
          accepted += TrimTrailing(
              node->Cast<fast::ArrowFunctionExpression>()->params, 0, helper);
          break;
        default:
          break;
      }
    };
    Walk(helper.tree()->root(), visit);
    return accepted;
  }
};

REDLINE_TUNABLE(kExpressionsMaxCallArguments,
                "minimizer.expressions.max_call_arguments", 2, 0, 8,
                "leading arguments of a call or `new` that are tried as its "
                "replacement (the callee always is)");

// Replaces expressions by one of their operands or by a constant:
// `a + f(b)` -> `a`, `f(b)` -> `b`, `o.p.q` -> `o.p`, `x ? y : z` -> `y`,
// anything -> `0` / `undefined`.
class ExpressionSimplifier : public Reducer {
 public:
  ExpressionSimplifier()
      : Reducer("expressions",
                "replace expressions by an operand or a constant") {}

  int Reduce(MinimizationHelper& helper) override {
    int accepted = 0;
    // (slot, alternative index) pairs that were already rejected.
    std::unordered_set<uint64_t> failed;
    bool progress = true;
    while (progress && !helper.BudgetExhausted()) {
      progress = false;
      std::vector<Slot> slots = CollectSlots(helper.tree()->root());
      for (const Slot& s : slots) {
        if (helper.BudgetExhausted()) {
          break;
        }
        Node** slot = s.field;
        auto* current = static_cast<Expression*>(*slot);
        if (IsAtomic(current)) {
          continue;
        }
        // A loop condition is only ever replaced by a falsy constant: any
        // other replacement risks turning the loop into an infinite one.
        std::vector<Expression*> alternatives =
            s.loop_test ? std::vector<Expression*>{mk::Num(helper.tree(), 0)}
                        : Alternatives(helper.tree(), current);
        for (size_t alt = 0; alt < alternatives.size(); alt++) {
          // Slots are 8-byte aligned pointers; 4 bits hold the index of
          // any alternative (at most max_call_arguments + 3 of them).
          uint64_t key = reinterpret_cast<uint64_t>(slot) * 16 + alt;
          if (failed.count(key) != 0) {
            continue;
          }
          *slot = alternatives[alt];
          if (helper.Test()) {
            accepted++;
            progress = true;
            break;
          }
          *slot = current;
          failed.insert(key);
        }
        if (progress) {
          break;  // the tree changed: recollect the slots
        }
      }
    }
    return accepted;
  }

 private:
  static bool IsAtomic(const Expression* e) {
    switch (e->kind) {
      case NodeKind::kIdentifier:
      case NodeKind::kNullLiteral:
      case NodeKind::kBooleanLiteral:
      case NodeKind::kNumericLiteral:
      case NodeKind::kBigIntLiteral:
      case NodeKind::kStringLiteral:
      case NodeKind::kRegExpLiteral:
      case NodeKind::kThisExpression:
      case NodeKind::kSuper:
      case NodeKind::kPrivateIdentifier:
      case NodeKind::kMetaProperty:
      case NodeKind::kTemplateLiteral:
        return true;
      default:
        return false;
    }
  }

  static bool IsValueOperand(const Expression* e) {
    return e != nullptr && !e->Is<fast::SpreadElement>() &&
           !e->Is<fast::Super>() && !e->Is<fast::PrivateIdentifier>() &&
           !e->IsPattern();
  }

  // An expression slot in value position. `loop_test` marks the condition of
  // a loop, which must only be replaced by something that terminates it.
  struct Slot {
    Node** field;
    bool loop_test;
  };

  // Slots holding an expression in value position whose replacement by an
  // arbitrary expression keeps the program syntactically valid.
  static std::vector<Slot> CollectSlots(Node* root) {
    std::vector<Slot> slots;
    auto add = [&](auto*& field) {
      if (IsValueOperand(field)) {
        slots.push_back({reinterpret_cast<Node**>(&field), false});
      }
    };
    auto add_loop_test = [&](auto*& field) {
      if (IsValueOperand(field)) {
        slots.push_back({reinterpret_cast<Node**>(&field), true});
      }
    };
    auto add_list = [&](std::vector<Expression*>& list) {
      for (Expression*& e : list) {
        add(e);
      }
    };
    auto visit = [&](Node* node) {
      switch (node->kind) {
        case NodeKind::kExpressionStatement: {
          auto* n = node->Cast<fast::ExpressionStatement>();
          if (!n->is_directive) {
            add(n->expression);
          }
          break;
        }
        case NodeKind::kVariableDeclarator:
          add(node->Cast<fast::VariableDeclarator>()->init);
          break;
        case NodeKind::kReturnStatement:
          add(node->Cast<fast::ReturnStatement>()->argument);
          break;
        case NodeKind::kThrowStatement:
          add(node->Cast<fast::ThrowStatement>()->argument);
          break;
        case NodeKind::kIfStatement:
          add(node->Cast<fast::IfStatement>()->test);
          break;
        case NodeKind::kWhileStatement:
          add_loop_test(node->Cast<fast::WhileStatement>()->test);
          break;
        case NodeKind::kDoWhileStatement:
          add_loop_test(node->Cast<fast::DoWhileStatement>()->test);
          break;
        case NodeKind::kForStatement:
          // The update expression is deliberately left alone: `i++` -> `0`
          // is an infinite loop whenever the test still depends on `i`.
          add_loop_test(node->Cast<fast::ForStatement>()->test);
          break;
        case NodeKind::kForInStatement:
          add(node->Cast<fast::ForInStatement>()->right);
          break;
        case NodeKind::kForOfStatement:
          add(node->Cast<fast::ForOfStatement>()->right);
          break;
        case NodeKind::kSwitchStatement:
          add(node->Cast<fast::SwitchStatement>()->discriminant);
          break;
        case NodeKind::kSwitchCase:
          add(node->Cast<fast::SwitchCase>()->test);
          break;
        case NodeKind::kCallExpression: {
          auto* n = node->Cast<fast::CallExpression>();
          add(n->callee);
          add_list(n->arguments);
          break;
        }
        case NodeKind::kNewExpression:
          add_list(node->Cast<fast::NewExpression>()->arguments);
          break;
        case NodeKind::kNativeCallExpression:
          add_list(node->Cast<fast::NativeCallExpression>()->arguments);
          break;
        case NodeKind::kBinaryExpression: {
          auto* n = node->Cast<fast::BinaryExpression>();
          add(n->left);
          add(n->right);
          break;
        }
        case NodeKind::kLogicalExpression: {
          auto* n = node->Cast<fast::LogicalExpression>();
          add(n->left);
          add(n->right);
          break;
        }
        case NodeKind::kAssignmentExpression:
          add(node->Cast<fast::AssignmentExpression>()->right);
          break;
        case NodeKind::kUnaryExpression: {
          auto* n = node->Cast<fast::UnaryExpression>();
          if (n->op != fast::UnaryOp::kDelete) {
            add(n->argument);
          }
          break;
        }
        case NodeKind::kMemberExpression: {
          auto* n = node->Cast<fast::MemberExpression>();
          add(n->object);
          if (n->computed) {
            add(n->property);
          }
          break;
        }
        case NodeKind::kConditionalExpression: {
          auto* n = node->Cast<fast::ConditionalExpression>();
          add(n->test);
          add(n->consequent);
          add(n->alternate);
          break;
        }
        case NodeKind::kArrayExpression:
          add_list(node->Cast<fast::ArrayExpression>()->elements);
          break;
        case NodeKind::kObjectExpression: {
          for (Node* member :
               node->Cast<fast::ObjectExpression>()->properties) {
            auto* property = member->As<fast::Property>();
            if (property == nullptr ||
                property->property_kind != fast::PropertyKind::kInit ||
                property->shorthand || property->method) {
              continue;
            }
            add(property->value);
            if (property->computed) {
              add(property->key);
            }
          }
          break;
        }
        case NodeKind::kSpreadElement:
          add(node->Cast<fast::SpreadElement>()->argument);
          break;
        case NodeKind::kAwaitExpression:
          add(node->Cast<fast::AwaitExpression>()->argument);
          break;
        case NodeKind::kYieldExpression:
          add(node->Cast<fast::YieldExpression>()->argument);
          break;
        case NodeKind::kSequenceExpression:
          add_list(node->Cast<fast::SequenceExpression>()->expressions);
          break;
        case NodeKind::kTemplateLiteral:
          add_list(node->Cast<fast::TemplateLiteral>()->expressions);
          break;
        case NodeKind::kArrowFunctionExpression:
          add(node->Cast<fast::ArrowFunctionExpression>()->expression_body);
          break;
        case NodeKind::kPropertyDefinition:
          add(node->Cast<fast::PropertyDefinition>()->value);
          break;
        default:
          break;
      }
    };
    Walk(root, visit);
    return slots;
  }

  static std::vector<Expression*> Alternatives(fast::Tree* tree,
                                               Expression* e) {
    std::vector<Expression*> out;
    auto operand = [&](Expression* candidate) {
      if (IsValueOperand(candidate)) {
        out.push_back(candidate);
      }
    };
    switch (e->kind) {
      case NodeKind::kBinaryExpression: {
        auto* n = e->Cast<fast::BinaryExpression>();
        operand(n->left);
        operand(n->right);
        break;
      }
      case NodeKind::kLogicalExpression: {
        auto* n = e->Cast<fast::LogicalExpression>();
        operand(n->left);
        operand(n->right);
        break;
      }
      case NodeKind::kConditionalExpression: {
        auto* n = e->Cast<fast::ConditionalExpression>();
        operand(n->consequent);
        operand(n->alternate);
        break;
      }
      case NodeKind::kCallExpression: {
        auto* n = e->Cast<fast::CallExpression>();
        const size_t max_args =
            static_cast<size_t>(kExpressionsMaxCallArguments.AsInt());
        for (size_t i = 0; i < n->arguments.size() && i < max_args; i++) {
          operand(n->arguments[i]);
        }
        operand(n->callee);
        break;
      }
      case NodeKind::kNewExpression: {
        auto* n = e->Cast<fast::NewExpression>();
        const size_t max_args =
            static_cast<size_t>(kExpressionsMaxCallArguments.AsInt());
        for (size_t i = 0; i < n->arguments.size() && i < max_args; i++) {
          operand(n->arguments[i]);
        }
        break;
      }
      case NodeKind::kMemberExpression:
        operand(e->Cast<fast::MemberExpression>()->object);
        break;
      case NodeKind::kUnaryExpression:
        operand(e->Cast<fast::UnaryExpression>()->argument);
        break;
      case NodeKind::kAwaitExpression:
        operand(e->Cast<fast::AwaitExpression>()->argument);
        break;
      case NodeKind::kYieldExpression:
        operand(e->Cast<fast::YieldExpression>()->argument);
        break;
      case NodeKind::kAssignmentExpression:
        operand(e->Cast<fast::AssignmentExpression>()->right);
        break;
      case NodeKind::kSequenceExpression: {
        auto* n = e->Cast<fast::SequenceExpression>();
        if (!n->expressions.empty()) {
          operand(n->expressions.back());
        }
        break;
      }
      case NodeKind::kChainExpression:
        operand(e->Cast<fast::ChainExpression>()->expression);
        break;
      case NodeKind::kTaggedTemplateExpression:
        operand(e->Cast<fast::TaggedTemplateExpression>()->quasi);
        break;
      default:
        break;
    }
    out.push_back(mk::Num(tree, 0));
    out.push_back(mk::Undefined(tree));
    return out;
  }
};

}  // namespace

// ---------------------------------------------------------------------------
// Minimizer

Minimizer::Minimizer(MinimizerOptions options)
    : options_(options), reducers_(CreateDefaultReducers()) {}

Minimizer::~Minimizer() = default;

// static
std::vector<std::unique_ptr<Reducer>> Minimizer::CreateDefaultReducers() {
  std::vector<std::unique_ptr<Reducer>> reducers;
  reducers.push_back(std::make_unique<StatementRemover>());
  reducers.push_back(std::make_unique<Unwrapper>());
  reducers.push_back(std::make_unique<ListTrimmer>());
  reducers.push_back(std::make_unique<ExpressionSimplifier>());
  // Not in the default order (see DefaultOrder): on crash reproducers they
  // reach the same result as `statements` with 5-12% more tests. Enable
  // them with stages.minimize.reducers or `minimize --reducer=`.
  reducers.push_back(std::make_unique<Truncator>());
  reducers.push_back(std::make_unique<DataflowReducer>());
  return reducers;
}

// static
const std::vector<std::string_view>& Minimizer::DefaultOrder() {
  // Never deleted: no destructor runs when the process exits.
  static const std::vector<std::string_view>* const kOrder =
      new std::vector<std::string_view>{"statements", "unwrap", "lists",
                                        "expressions"};
  return *kOrder;
}

Reducer* Minimizer::Find(std::string_view name) const {
  for (const auto& reducer : reducers_) {
    if (reducer->name() == name) {
      return reducer.get();
    }
  }
  return nullptr;
}

std::vector<std::string> Minimizer::Names() const {
  std::vector<std::string> names;
  names.reserve(reducers_.size());
  for (const auto& reducer : reducers_) {
    names.push_back(reducer->name());
  }
  return names;
}

std::vector<const Component*> Minimizer::Components() const {
  std::vector<const Component*> components;
  components.reserve(reducers_.size());
  for (const auto& reducer : reducers_) {
    components.push_back(reducer.get());
  }
  return components;
}

MinimizationResult Minimizer::Minimize(fast::Tree* tree,
                                       const MinimizationOracle& oracle,
                                       SyntaxChecker* checker) const {
  return Minimize(tree, oracle, checker, options_);
}

MinimizationResult Minimizer::Minimize(fast::Tree* tree,
                                       const MinimizationOracle& oracle,
                                       SyntaxChecker* checker,
                                       const MinimizerOptions& options) const {
  std::vector<Reducer*> reducers;
  for (std::string_view name : DefaultOrder()) {
    reducers.push_back(Find(name));
  }
  return MinimizeWith(reducers, tree, oracle, checker, options);
}

MinimizationResult Minimizer::MinimizeWith(
    const std::vector<Reducer*>& reducers, fast::Tree* tree,
    const MinimizationOracle& oracle, SyntaxChecker* checker) const {
  return MinimizeWith(reducers, tree, oracle, checker, options_);
}

MinimizationResult Minimizer::MinimizeWith(
    const std::vector<Reducer*>& reducers, fast::Tree* tree,
    const MinimizationOracle& oracle, SyntaxChecker* checker,
    const MinimizerOptions& options) const {
  Stopwatch stopwatch;
  MinimizationHelper helper(tree, oracle, checker, options);
  helper.mutable_result()->nodes_before = fast::CountNodes(tree->root());
  for (int round = 0; round < options.max_rounds; round++) {
    int accepted = 0;
    for (Reducer* reducer : reducers) {
      if (helper.BudgetExhausted()) {
        break;
      }
      helper.set_reducer(reducer);
      reducer->stats().invocations++;
      int n = reducer->Reduce(helper);
      reducer->stats().produced += static_cast<uint64_t>(n);
      accepted += n;
    }
    if (accepted == 0 || helper.BudgetExhausted()) {
      break;
    }
  }
  // Semantically neutral clean-up that needs no test.
  RemoveEmptyStatements(tree->root());
  MinimizationResult result = helper.result();
  result.nodes_after = fast::CountNodes(tree->root());
  result.time_us = stopwatch.ElapsedUs();
  result.budget_exhausted = helper.BudgetExhausted();
  return result;
}

int MinimizerOptions::TestsFor(uint64_t exec_time_us) const {
  if (target_time_ms == 0 || exec_time_us == 0) {
    return max_tests;
  }
  uint64_t affordable = target_time_ms * 1000 / exec_time_us;
  int tests = affordable > static_cast<uint64_t>(max_tests)
                  ? max_tests
                  : static_cast<int>(affordable);
  return std::max(tests, std::min(min_tests, max_tests));
}

MinimizerOptions MinimizerOptions::ForProgram(uint64_t exec_time_us) const {
  MinimizerOptions scaled = *this;
  scaled.max_tests = TestsFor(exec_time_us);
  return scaled;
}

}  // namespace redline
