// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/redline/mutators/fixup.h"

#include <map>
#include <set>
#include <utility>

#include "experimental/redline/base/log.h"
#include "experimental/redline/core/ast_builder.h"
#include "experimental/redline/core/tunable.h"

namespace redline {

using namespace fast;  // NOLINT(build/namespaces)

namespace {

// Reports once that the body of a guard starts ("FIXUP_RAN: <id>", the
// call with `ran`) and once that its handler runs ("FIXUP_THREW: <id>").
constexpr char kFixupRuntime[] = R"js(const __redline_fixup = (function() {
  const emit = typeof redline === 'function' ? function(s) { redline('REDLINE_PRINT', s); }
             : typeof fuzzilli === 'function' ? function(s) { fuzzilli('FUZZILLI_PRINT', s); }
                                              : function(s) {};
  const seen = {__proto__: null};
  return function(id, ran) {
    const line = (ran ? 'FIXUP_RAN: ' : 'FIXUP_THREW: ') + id;
    if (seen[line]) return;
    seen[line] = true;
    emit(line);
  };
})();
)js";

REDLINE_TUNABLE(kFixupMaxGuards, "mutators.Fixup.max_guards", 8, 1, 64,
                "most guards tested per application");

// True if `statement` declares a binding whose scope is the enclosing block.
bool DeclaresLexical(const Statement* statement) {
  while (const auto* labeled = statement->As<LabeledStatement>()) {
    statement = labeled->body;
  }
  if (const auto* decl = statement->As<VariableDeclaration>()) {
    return decl->declaration_kind != VariableKind::kVar;
  }
  return statement->Is<ClassDeclaration>() ||
         statement->Is<FunctionDeclaration>();
}

}  // namespace

FixupMutator::FixupMutator()
    : RuntimeAssistedMutator("Fixup",
                             "removes try/catch guards whose handler does not "
                             "run",
                             0.5, false) {}

std::string_view FixupMutator::runtime() const { return kFixupRuntime; }

// static
bool FixupMutator::IsGuard(const Node* node) {
  const auto* guard = node->As<TryStatement>();
  return guard != nullptr && guard->block != nullptr &&
         guard->handler != nullptr && guard->finalizer == nullptr &&
         guard->handler->body != nullptr &&
         guard->handler->body->body.empty() &&
         (guard->handler->param == nullptr ||
          guard->handler->param->Is<Identifier>());
}

// static
void FixupMutator::Unwrap(Builder& b, TryStatement* guard) {
  BlockStatement* block = guard->block;
  // A first statement `"use strict";` can become a directive at the start
  // of a body.
  const auto* first = block->body.empty()
                          ? nullptr
                          : block->body.front()->As<ExpressionStatement>();
  bool keep_block = first != nullptr && first->expression->Is<StringLiteral>();
  for (const Statement* s : block->body) {
    keep_block = keep_block || DeclaresLexical(s);
  }
  Builder::Slot slot = b.SlotOf(guard);
  if (keep_block || !slot.valid()) {
    // `{ let x = f(); }` keeps the scope of x. Where the syntax needs one
    // statement (`if (c) try ...`), a block is also the only replacement.
    b.Replace(guard, block);
    b.Invalidate();
    return;
  }
  std::vector<Statement*> body = block->body;
  b.Remove(slot);
  if (!body.empty()) {
    b.InsertAll(slot, body);
  }
}

bool FixupMutator::Mutate(Builder& b, MutationContext& ctx) {
  Random& r = b.rng();
  ctx.probes.clear();
  std::vector<Node*> guards = b.Collect([](Node* n) { return IsGuard(n); });
  RemoveWorkerNodes(b, &guards);
  if (guards.empty()) {
    return false;
  }
  for (size_t i = guards.size(); i > 1; i--) {
    std::swap(guards[i - 1], guards[r.Index(i)]);
  }
  if (static_cast<int>(guards.size()) > kFixupMaxGuards.AsInt()) {
    guards.resize(static_cast<size_t>(kFixupMaxGuards.AsInt()));
  }
  Tree* t = b.tree();
  const unsigned first_id = static_cast<unsigned>(r.Next() % 100000);
  for (Node* n : guards) {
    ExploreProbe probe;
    probe.id = StringPrintf(
        "f%u", first_id + static_cast<unsigned>(ctx.probes.size()));
    // `try { __redline_fixup("fN", 1); ... } catch { __redline_fixup("fN"); }`
    auto* guard = n->Cast<TryStatement>();
    b.Insert({&guard->block->body, 0, guard->block},
             mk::ExprStmt(t, mk::Call(t, mk::Id(t, kProbeFunction),
                                      {mk::Str(t, probe.id), mk::Num(t, 1)})));
    b.Insert({&guard->handler->body->body, 0, guard->handler->body},
             mk::ExprStmt(t, mk::Call(t, mk::Id(t, kProbeFunction),
                                      {mk::Str(t, probe.id)})));
    ctx.probes.push_back(std::move(probe));
  }
  ctx.detail = StringPrintf("testing %zu guards", ctx.probes.size());
  return true;
}

int FixupMutator::Apply(Builder& b, const std::vector<ExploreProbe>& probes,
                        const std::string& fuzzout, MutationContext& /*ctx*/,
                        std::string* detail) {
  std::set<std::string> ran;
  std::set<std::string> threw;
  size_t start = 0;
  while (start < fuzzout.size()) {
    size_t nl = fuzzout.find('\n', start);
    if (nl == std::string::npos) {
      nl = fuzzout.size();
    }
    std::string_view line(fuzzout.data() + start, nl - start);
    start = nl + 1;
    constexpr std::string_view kRan = "FIXUP_RAN: ";
    constexpr std::string_view kThrew = "FIXUP_THREW: ";
    if (line.starts_with(kRan)) {
      ran.emplace(line.substr(kRan.size()));
    } else if (line.starts_with(kThrew)) {
      threw.emplace(line.substr(kThrew.size()));
    }
  }
  // The guard of each handler call: call -> statement -> handler body ->
  // handler -> try statement.
  std::map<std::string, TryStatement*> guards;
  for (Node* n : b.Collect([](Node* n) {
         auto* call = n->As<CallExpression>();
         auto* callee =
             call != nullptr ? call->callee->As<Identifier>() : nullptr;
         // The handler call has one argument.
         return callee != nullptr && callee->name == kProbeFunction &&
                call->arguments.size() == 1 &&
                call->arguments[0]->Is<StringLiteral>();
       })) {
    Node* up = b.ParentOf(n);
    for (int i = 0; i < 3 && up != nullptr; i++) {
      up = b.ParentOf(up);
    }
    if (up != nullptr && up->Is<TryStatement>()) {
      guards[std::string(n->Cast<CallExpression>()
                             ->arguments[0]
                             ->Cast<StringLiteral>()
                             ->value)] = up->Cast<TryStatement>();
    }
  }
  int applied = 0;
  for (const ExploreProbe& probe : probes) {
    auto it = guards.find(probe.id);
    if (it == guards.end()) {
      continue;
    }
    // Remove the two calls. Unwrap a guard whose body ran and whose handler
    // did not run.
    TryStatement* guard = it->second;
    b.Remove({&guard->handler->body->body, 0, guard->handler->body});
    b.Remove({&guard->block->body, 0, guard->block});
    if (ran.count(probe.id) != 0 && threw.count(probe.id) == 0) {
      Unwrap(b, guard);
      applied++;
    }
  }
  if (detail != nullptr) {
    *detail =
        StringPrintf("unwrapped %d of %zu guards", applied, probes.size());
  }
  return applied;
}

}  // namespace redline
