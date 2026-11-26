// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/redline/corpus/material.h"

#include <algorithm>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "experimental/redline/base/file_util.h"
#include "experimental/redline/base/log.h"
#include "experimental/redline/base/time_util.h"
#include "experimental/redline/core/ast_builder.h"
#include "experimental/redline/core/tunable.h"
#include "experimental/redline/engine/d8_flags.h"
#include "experimental/redline/engine/importer.h"
#include "experimental/redline/fast/ast.h"
#include "experimental/redline/fast/requirements.h"
#include "experimental/redline/fast/scope.h"
#include "experimental/redline/generators/codegen.h"

namespace redline {

REDLINE_TUNABLE(kMaterialPartnerChance, "material.partner_chance", 0.35, 0.0,
                1.0,
                "Probability that Splice and Combine pick a partner from "
                "`MaterialBank`.");
REDLINE_TUNABLE(kMaterialFuseChance, "material.fuse_chance", 0.15, 0.0, 1.0,
                "Probability that a mutation chain starts with a `Fuse` root.");
REDLINE_TUNABLE(kMaterialMaxDonorNodes, "material.max_donor_nodes", 300, 50,
                4000, "Maximum AST nodes per donor in `FusePrograms`.");
REDLINE_TUNABLE(kMaterialMaxFuseNodes, "material.max_fuse_nodes", 600, 100,
                8000, "Maximum total AST nodes emitted by `FusePrograms`.");
REDLINE_TUNABLE(kMaterialMaxCombineNodes, "material.max_combine_nodes", 300, 50,
                4000, "Maximum partner AST nodes combined by `Combine`.");
REDLINE_TUNABLE(kMaterialGlueStatements, "material.glue_statements", 2, 0, 8,
                "Maximum cross-donor glue statements in `FusePrograms`.");

namespace {

constexpr char kLog[] = "material";

constexpr std::string_view kSkippedFileNames[] = {
    "d8-os.js",
    "d8-readbuffer.js",
    "d8-arguments.js",
    "d8-finalization-registry-quit",
    "d8-finalization-registry-quit.js",
    "regexp-pcre.js",
    "unicode-test.js",
    "unicodelctest.js",
    "unicodelctest-no-optimization.js",
    "regress-444805.js",
    "regress-crbug-489597.js",
    "regress-crbug-620253.js",
    "regress-8510.js",
    "regress-352044-02-n.js",
    "regress-crbug-400504688.js",
    "regress-392928803.js",
    "random-bit-correlations",
    "random-bit-correlations.js",
};

constexpr std::string_view kSkippedDirSegments[] = {
    "embenchen",
    "poppler",
    "sqlite",
    "v8/test/inspector",
    "test/inspector",
    "v8/test/js-perf-test/Modules",
    "test/js-perf-test/Modules",
    "v8/test/message",
    "test/message",
    "v8/test/mjsunit/tools",
    "test/mjsunit/tools",
    "v8/test/mjsunit/d8/cwd",
    "test/mjsunit/d8/cwd",
    "mozilla/data/e4x",
    "spidermonkey/non262/ReadableStream",
};

std::string NormalizeSlashes(std::string_view path) {
  std::string out(path);
  std::replace(out.begin(), out.end(), '\\', '/');
  return out;
}

bool HasDirSubpath(std::string_view dir, std::string_view subpath) {
  if (dir.empty() || subpath.empty()) {
    return false;
  }
  size_t pos = 0;
  while ((pos = dir.find(subpath, pos)) != std::string_view::npos) {
    bool left_ok = (pos == 0 || dir[pos - 1] == '/');
    size_t end = pos + subpath.size();
    bool right_ok = (end == dir.size() || dir[end] == '/');
    if (left_ok && right_ok) {
      return true;
    }
    pos++;
  }
  return false;
}

std::string_view TrimLeftWs(std::string_view s) {
  while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) {
    s.remove_prefix(1);
  }
  return s;
}

std::string NormalizeFlagName(std::string_view token) {
  size_t eq = token.find('=');
  std::string_view name = token.substr(0, eq);
  std::string out(name);
  for (size_t i = 2; i < out.size(); i++) {
    if (out[i] == '_') {
      out[i] = '-';
    }
  }
  return out;
}

bool IsDiscouragedFlagToken(std::string_view token) {
  if (token.ends_with(".mjs")) {
    return true;
  }
  if (!token.starts_with("--")) {
    return false;
  }
  std::string name = NormalizeFlagName(token);
  if (name == "--module" || name == "--correctness-fuzzer-suppressions" ||
      name == "--expose-trigger-failure" || name == "--sandbox-testing" ||
      name == "--sandbox-fuzzing") {
    return true;
  }
  if (name.starts_with("--icu-data-file")) {
    return true;
  }
  std::string_view sv(name);
  if (sv.starts_with("--harmony-")) {
    return !sv.substr(10).starts_with("shipping");
  }
  if (sv.starts_with("--js-")) {
    std::string_view rest = sv.substr(5);
    return !rest.starts_with("staging") && !rest.starts_with("shipping");
  }
  return false;
}

bool IsModuleHeaderSource(std::string_view source) {
  size_t pos = 0;
  while (pos < source.size()) {
    size_t end = source.find('\n', pos);
    if (end == std::string_view::npos) {
      end = source.size();
    }
    std::string_view line = source.substr(pos, end - pos);
    if (!line.empty() && line.back() == '\r') {
      line.remove_suffix(1);
    }
    line = TrimLeftWs(line);
    if (line.starts_with("// MODULE") &&
        (line.size() == 9 || line[9] == ' ' || line[9] == '\t')) {
      return true;
    }
    pos = end + 1;
  }
  return false;
}

bool IsDirectiveStmt(const fast::Statement* s) {
  auto* e = s->As<fast::ExpressionStatement>();
  return e != nullptr && e->is_directive;
}

bool ExtractZeroParamIIFE(fast::Statement* stmt,
                          const fast::FunctionData** fn_out,
                          fast::Node** fn_node_out) {
  if (IsDirectiveStmt(stmt)) {
    return false;
  }
  auto* expr_stmt = stmt->As<fast::ExpressionStatement>();
  if (expr_stmt == nullptr || expr_stmt->expression == nullptr) {
    return false;
  }
  auto* call = expr_stmt->expression->As<fast::CallExpression>();
  if (call == nullptr || call->optional || !call->arguments.empty() ||
      call->callee == nullptr) {
    return false;
  }
  fast::Node* callee = call->callee;
  if (!callee->Is<fast::FunctionExpression>() &&
      !callee->Is<fast::ArrowFunctionExpression>()) {
    return false;
  }
  const fast::FunctionData* fn = fast::AsFunctionData(callee);
  if (fn == nullptr || fn->is_async || fn->is_generator ||
      !fn->params.empty() || fn->body == nullptr || fn->body->body.empty()) {
    return false;
  }
  *fn_out = fn;
  *fn_node_out = callee;
  return true;
}

bool HasStrictDirective(const fast::BlockStatement* block) {
  for (const fast::Statement* s : block->body) {
    if (!IsDirectiveStmt(s)) {
      break;
    }
    if (s->Cast<fast::ExpressionStatement>()->directive == "use strict") {
      return true;
    }
  }
  return false;
}

bool HasSelfNameReference(const fast::FunctionData* fn, fast::Node* fn_node,
                          const fast::ScopeAnalysis& scopes) {
  if (fn->name == nullptr) {
    return false;
  }
  for (const fast::Binding* b : scopes.bindings()) {
    if (b->declaration == fn_node &&
        b->kind == fast::BindingKind::kFunctionName) {
      return !b->references.empty();
    }
  }
  return true;
}

std::string FreshName(std::string_view prefix, int* counter,
                      const absl::flat_hash_set<std::string>& used) {
  while (true) {
    std::string candidate = StringPrintf(
        "%.*s%d", static_cast<int>(prefix.size()), prefix.data(), (*counter)++);
    if (!used.contains(candidate)) {
      return candidate;
    }
  }
}

void RenameCollidingScopeBindings(fast::Tree* tree, fast::Scope* scope,
                                  absl::flat_hash_set<std::string>* top_names,
                                  absl::flat_hash_set<std::string>* all_names,
                                  int* counter) {
  if (scope == nullptr) {
    return;
  }
  for (fast::Binding* b : scope->bindings) {
    if (b->kind == fast::BindingKind::kImplicit ||
        b->kind == fast::BindingKind::kFunctionName ||
        b->kind == fast::BindingKind::kClassName) {
      continue;
    }
    if (top_names->contains(b->name)) {
      std::string fresh = FreshName("_m", counter, *all_names);
      all_names->insert(fresh);
      top_names->insert(fresh);
      fast::RenameBinding(tree, b, fresh);
    } else {
      top_names->emplace(b->name);
    }
  }
}

void EmitSplitLexicalDecls(fast::Tree* out, const fast::VariableDeclaration* vd,
                           std::vector<fast::Statement*>* hoisted_lets,
                           std::vector<fast::Statement*>* exec_run) {
  for (const fast::VariableDeclarator* d : vd->declarations) {
    if (d == nullptr || d->target == nullptr) {
      continue;
    }
    std::vector<fast::Identifier*> ids;
    fast::CollectBoundIdentifiers(d->target, &ids);
    if (!ids.empty()) {
      auto* decl = out->New<fast::VariableDeclaration>();
      decl->declaration_kind = fast::VariableKind::kLet;
      for (const fast::Identifier* id : ids) {
        auto* declarator = out->New<fast::VariableDeclarator>();
        declarator->target = mk::Id(out, id->name);
        decl->declarations.push_back(declarator);
      }
      hoisted_lets->push_back(decl);
    }
    if (d->init != nullptr) {
      exec_run->push_back(mk::ExprStmt(
          out, mk::Assign(out, out->Clone(d->target), out->Clone(d->init))));
    }
  }
}

bool IsUsingDeclaration(const fast::Statement* stmt) {
  auto* vd = stmt->As<fast::VariableDeclaration>();
  return vd != nullptr &&
         (vd->declaration_kind == fast::VariableKind::kUsing ||
          vd->declaration_kind == fast::VariableKind::kAwaitUsing);
}

// Returns true when the definition of `cls` can run donor code: an `extends`
// clause, a computed key, a static block or a static field initializer.
bool ClassRunsCodeAtDefinition(const fast::ClassData* cls) {
  if (cls->super_class != nullptr) {
    return true;
  }
  if (cls->body == nullptr) {
    return false;
  }
  for (const fast::Node* member : cls->body->members) {
    if (member->Is<fast::StaticBlock>()) {
      return true;
    }
    if (auto* m = member->As<fast::MethodDefinition>()) {
      if (m->computed) {
        return true;
      }
    } else if (auto* p = member->As<fast::PropertyDefinition>()) {
      if (p->computed || (p->is_static && p->value != nullptr)) {
        return true;
      }
    }
  }
  return false;
}

// Splits `class C ...` into `let C;` in `hoisted_lets` and
// `C = class C ...;` in `exec_run`.
void EmitSplitClassDecl(fast::Tree* out, const fast::ClassDeclaration* cls,
                        std::vector<fast::Statement*>* hoisted_lets,
                        std::vector<fast::Statement*>* exec_run) {
  auto* decl = out->New<fast::VariableDeclaration>();
  decl->declaration_kind = fast::VariableKind::kLet;
  auto* declarator = out->New<fast::VariableDeclarator>();
  declarator->target = mk::Id(out, cls->name->name);
  decl->declarations.push_back(declarator);
  hoisted_lets->push_back(decl);

  auto* expr = out->New<fast::ClassExpression>();
  expr->name = mk::Id(out, cls->name->name);
  expr->super_class = out->Clone(cls->super_class);
  expr->body = out->Clone(cls->body);
  exec_run->push_back(
      mk::ExprStmt(out, mk::Assign(out, mk::Id(out, cls->name->name), expr)));
}

void EmitIsolatedDonor(fast::Tree* out, const fast::Program* donor,
                       std::vector<fast::Statement*>* body_out) {
  std::vector<fast::Statement*> hoisted_lets;
  std::vector<fast::Statement*> exec_run;
  auto flush = [&] {
    for (fast::Statement* let_stmt : hoisted_lets) {
      body_out->push_back(let_stmt);
    }
    hoisted_lets.clear();
    if (!exec_run.empty()) {
      body_out->push_back(mk::Try(out, std::move(exec_run), "", {}));
      exec_run.clear();
    }
  };

  for (const fast::Statement* stmt : donor->body) {
    if (stmt == nullptr || stmt->Is<fast::EmptyStatement>() ||
        IsDirectiveStmt(stmt)) {
      continue;
    }
    if (auto* cls = stmt->As<fast::ClassDeclaration>()) {
      if (cls->name != nullptr && ClassRunsCodeAtDefinition(cls)) {
        EmitSplitClassDecl(out, cls, &hoisted_lets, &exec_run);
      } else {
        flush();
        body_out->push_back(out->Clone(stmt));
      }
      continue;
    }
    if (stmt->Is<fast::FunctionDeclaration>()) {
      flush();
      body_out->push_back(out->Clone(stmt));
      continue;
    }
    if (auto* vd = stmt->As<fast::VariableDeclaration>()) {
      // `using` is not valid at the top level of a script, so it stays in the
      // `try` block.
      if (vd->declaration_kind == fast::VariableKind::kVar ||
          IsUsingDeclaration(vd)) {
        exec_run.push_back(out->Clone(vd));
      } else {
        EmitSplitLexicalDecls(out, vd, &hoisted_lets, &exec_run);
      }
      continue;
    }
    exec_run.push_back(out->Clone(stmt));
  }
  flush();
}

size_t EstimateIsolatedNodes(const fast::Program* donor) {
  size_t nodes = 0;
  bool has_exec = false;
  auto flush_exec = [&] {
    if (has_exec) {
      nodes += 4;
      has_exec = false;
    }
  };
  for (const fast::Statement* stmt : donor->body) {
    if (stmt == nullptr || stmt->Is<fast::EmptyStatement>() ||
        IsDirectiveStmt(stmt)) {
      continue;
    }
    if (auto* cls = stmt->As<fast::ClassDeclaration>()) {
      if (cls->name != nullptr && ClassRunsCodeAtDefinition(cls)) {
        nodes += 6 + fast::CountNodes(cls);
        has_exec = true;
      } else {
        flush_exec();
        nodes += fast::CountNodes(stmt);
      }
      continue;
    }
    if (stmt->Is<fast::FunctionDeclaration>()) {
      flush_exec();
      nodes += fast::CountNodes(stmt);
      continue;
    }
    if (auto* vd = stmt->As<fast::VariableDeclaration>()) {
      if (vd->declaration_kind == fast::VariableKind::kVar ||
          IsUsingDeclaration(vd)) {
        nodes += fast::CountNodes(stmt);
        has_exec = true;
      } else {
        for (const fast::VariableDeclarator* d : vd->declarations) {
          if (d == nullptr || d->target == nullptr) {
            continue;
          }
          std::vector<fast::Identifier*> ids;
          fast::CollectBoundIdentifiers(d->target, &ids);
          if (!ids.empty()) {
            nodes += 1 + 2 * ids.size();
          }
          if (d->init != nullptr) {
            nodes +=
                2 + fast::CountNodes(d->target) + fast::CountNodes(d->init);
            has_exec = true;
          }
        }
      }
      continue;
    }
    nodes += fast::CountNodes(stmt);
    has_exec = true;
  }
  flush_exec();
  return nodes;
}

bool ContainsCallOrNew(fast::Statement* stmt) {
  bool found = false;
  fast::Walk(stmt, [&](fast::Node* n) {
    if (n->Is<fast::CallExpression>() || n->Is<fast::NewExpression>() ||
        n->Is<fast::NativeCallExpression>()) {
      found = true;
      return false;
    }
    return true;
  });
  return found;
}

bool IsPreferredSeedStmt(fast::Statement* stmt) {
  if (stmt->Is<fast::FunctionDeclaration>() ||
      stmt->Is<fast::ClassDeclaration>() || stmt->Is<fast::ForStatement>() ||
      stmt->Is<fast::ForInStatement>() || stmt->Is<fast::ForOfStatement>() ||
      stmt->Is<fast::WhileStatement>() || stmt->Is<fast::DoWhileStatement>() ||
      stmt->Is<fast::IfStatement>() || stmt->Is<fast::SwitchStatement>() ||
      stmt->Is<fast::TryStatement>()) {
    return true;
  }
  if (auto* vd = stmt->As<fast::VariableDeclaration>()) {
    return fast::CountNodes(vd) >= 5 || ContainsCallOrNew(vd);
  }
  if (stmt->Is<fast::ExpressionStatement>()) {
    return ContainsCallOrNew(stmt);
  }
  return false;
}

fast::Identifier* RootAssignedIdentifier(fast::Expression* target) {
  fast::Expression* cur = target;
  while (cur != nullptr && cur->Is<fast::MemberExpression>()) {
    cur = cur->Cast<fast::MemberExpression>()->object;
  }
  return cur != nullptr ? cur->As<fast::Identifier>() : nullptr;
}

template <typename T>
void SortUnique(std::vector<T>* vec) {
  std::sort(vec->begin(), vec->end());
  vec->erase(std::unique(vec->begin(), vec->end()), vec->end());
}

void ShuffleIndices(std::vector<size_t>* indices, Random& rng) {
  for (size_t idx = indices->size(); idx > 1; idx--) {
    size_t swap_at = rng.Index(idx);
    std::swap((*indices)[idx - 1], (*indices)[swap_at]);
  }
}

bool CompactStatementList(fast::Tree* tree,
                          std::vector<fast::Statement*>* body_io,
                          size_t max_total_nodes, Random& rng,
                          bool* strict_flag) {
  if (body_io->empty()) {
    return false;
  }
  size_t current_list_nodes = 0;
  for (const fast::Statement* s : *body_io) {
    if (s != nullptr) {
      current_list_nodes += fast::CountNodes(s);
    }
  }
  const size_t total_tree_nodes = fast::CountNodes(tree->root());
  const size_t outer_nodes = total_tree_nodes >= current_list_nodes
                                 ? total_tree_nodes - current_list_nodes
                                 : 0;
  if (max_total_nodes <= outer_nodes) {
    return false;
  }
  size_t budget = max_total_nodes - outer_nodes;
  if (current_list_nodes <= budget) {
    return true;
  }

  std::vector<fast::Statement*> directives;
  size_t dir_nodes = 0;
  size_t idx = 0;
  while (idx < body_io->size()) {
    fast::Statement* s = (*body_io)[idx];
    if (s != nullptr && IsDirectiveStmt(s)) {
      directives.push_back(s);
      dir_nodes += fast::CountNodes(s);
      idx++;
    } else {
      break;
    }
  }
  std::vector<fast::Statement*> stmts;
  stmts.reserve(body_io->size() - idx);
  for (; idx < body_io->size(); idx++) {
    fast::Statement* s = (*body_io)[idx];
    if (s != nullptr && !s->Is<fast::EmptyStatement>() && !IsDirectiveStmt(s)) {
      stmts.push_back(s);
    }
  }
  if (stmts.empty()) {
    return false;
  }
  if (!directives.empty()) {
    if (dir_nodes + 16 <= budget) {
      budget -= dir_nodes;
    } else {
      directives.clear();
      if (strict_flag != nullptr) {
        *strict_flag = false;
      }
    }
  }

  const size_t count = stmts.size();
  std::vector<size_t> stmt_nodes(count, 0);
  size_t stmt_sum = 0;
  for (size_t i = 0; i < count; i++) {
    stmt_nodes[i] = fast::CountNodes(stmts[i]);
    stmt_sum += stmt_nodes[i];
  }
  if (stmt_sum <= budget) {
    std::vector<fast::Statement*> rebuilt = std::move(directives);
    rebuilt.insert(rebuilt.end(), stmts.begin(), stmts.end());
    *body_io = std::move(rebuilt);
    return true;
  }

  std::unique_ptr<fast::ScopeAnalysis> scopes =
      fast::ScopeAnalysis::Analyze(tree);
  constexpr size_t kNoStmt = std::numeric_limits<size_t>::max();
  std::vector<size_t> node_to_stmt(tree->id_limit(), kNoStmt);
  for (size_t i = 0; i < count; i++) {
    fast::Walk(stmts[i], [&](fast::Node* node) {
      if (node->id < node_to_stmt.size()) {
        node_to_stmt[node->id] = i;
      }
      return true;
    });
  }

  std::vector<std::vector<size_t>> hard_deps(count);
  std::vector<std::vector<size_t>> soft_deps(count);
  absl::flat_hash_map<const fast::Binding*, size_t> first_decl;
  absl::flat_hash_map<const fast::Binding*, std::vector<size_t>> decls_by_bind;
  for (const fast::Binding* b : scopes->bindings()) {
    if (b->kind == fast::BindingKind::kImplicit || b->declarations.empty()) {
      continue;
    }
    std::vector<size_t> decl_stmts;
    for (const fast::Identifier* d : b->declarations) {
      if (d != nullptr && d->id < node_to_stmt.size() &&
          node_to_stmt[d->id] != kNoStmt) {
        decl_stmts.push_back(node_to_stmt[d->id]);
      }
    }
    if (decl_stmts.empty()) {
      continue;
    }
    SortUnique(&decl_stmts);
    first_decl[b] = decl_stmts.front();
    for (const fast::Identifier* ref : b->references) {
      if (ref == nullptr || ref->id >= node_to_stmt.size()) {
        continue;
      }
      const size_t use_stmt = node_to_stmt[ref->id];
      if (use_stmt == kNoStmt ||
          std::binary_search(decl_stmts.begin(), decl_stmts.end(), use_stmt)) {
        continue;
      }
      auto it =
          std::lower_bound(decl_stmts.begin(), decl_stmts.end(), use_stmt);
      const size_t dep =
          (it == decl_stmts.begin()) ? decl_stmts.front() : *std::prev(it);
      hard_deps[use_stmt].push_back(dep);
    }
    decls_by_bind[b] = std::move(decl_stmts);
  }

  // Record assignment writes as soft dependencies so uninitialized bindings
  // keep their initialization statements when the budget allows it.
  absl::flat_hash_map<const fast::Binding*, std::vector<size_t>> writes_by_bind;
  for (size_t i = 0; i < count; i++) {
    fast::Walk(stmts[i], [&](fast::Node* node) {
      auto* ae = node->As<fast::AssignmentExpression>();
      if (ae == nullptr) {
        return true;
      }
      fast::Identifier* root_id = RootAssignedIdentifier(ae->left);
      if (root_id == nullptr) {
        return true;
      }
      const fast::Binding* b = scopes->BindingOf(root_id);
      if (b != nullptr && first_decl.contains(b)) {
        writes_by_bind[b].push_back(i);
      }
      return true;
    });
  }
  for (auto& [b, writes] : writes_by_bind) {
    SortUnique(&writes);
    const std::vector<size_t>& decl_stmts = decls_by_bind[b];
    const size_t decl_start = decl_stmts.front();
    for (const fast::Identifier* ref : b->references) {
      if (ref == nullptr || ref->id >= node_to_stmt.size()) {
        continue;
      }
      const size_t use_stmt = node_to_stmt[ref->id];
      if (use_stmt == kNoStmt || use_stmt <= decl_start) {
        continue;
      }
      auto it = std::lower_bound(writes.begin(), writes.end(), use_stmt);
      int taken = 0;
      while (it != writes.begin() && taken < 2) {
        --it;
        if (*it < decl_start) {
          break;
        }
        if (*it != use_stmt &&
            !std::binary_search(decl_stmts.begin(), decl_stmts.end(), *it)) {
          soft_deps[use_stmt].push_back(*it);
          taken++;
        }
      }
    }
  }
  for (size_t i = 0; i < count; i++) {
    SortUnique(&hard_deps[i]);
    SortUnique(&soft_deps[i]);
  }

  std::vector<int> mark(count, 0);
  int epoch = 0;
  std::vector<size_t> queue;
  auto try_closure = [&](size_t start, const std::vector<uint8_t>* kept_mask,
                         size_t base_nodes, std::vector<size_t>* added_out,
                         size_t* add_nodes_out) -> bool {
    added_out->clear();
    *add_nodes_out = 0;
    if (kept_mask != nullptr && (*kept_mask)[start]) {
      return true;
    }
    epoch++;
    queue.clear();
    mark[start] = epoch;
    queue.push_back(start);
    size_t added_nodes = 0;
    for (size_t q = 0; q < queue.size(); q++) {
      const size_t cur = queue[q];
      added_nodes += stmt_nodes[cur];
      if (base_nodes + added_nodes > budget) {
        return false;
      }
      added_out->push_back(cur);
      for (size_t dep : hard_deps[cur]) {
        if ((kept_mask != nullptr && (*kept_mask)[dep]) || mark[dep] == epoch) {
          continue;
        }
        mark[dep] = epoch;
        queue.push_back(dep);
      }
    }
    *add_nodes_out = added_nodes;
    return true;
  };

  std::vector<size_t> preferred;
  std::vector<size_t> fallback;
  for (size_t i = 0; i < count; i++) {
    if (IsPreferredSeedStmt(stmts[i])) {
      preferred.push_back(i);
    } else {
      fallback.push_back(i);
    }
  }
  ShuffleIndices(&preferred, rng);
  ShuffleIndices(&fallback, rng);

  size_t seed_stmt = kNoStmt;
  size_t kept_nodes = 0;
  std::vector<size_t> initial_closure;
  auto pick_from = [&](const std::vector<size_t>& pool) {
    for (size_t cand : pool) {
      if (stmt_nodes[cand] > budget) {
        continue;
      }
      size_t nodes = 0;
      if (try_closure(cand, nullptr, 0, &initial_closure, &nodes)) {
        seed_stmt = cand;
        kept_nodes = nodes;
        return true;
      }
    }
    return false;
  };
  if (!pick_from(preferred) && !pick_from(fallback)) {
    return false;
  }

  std::vector<uint8_t> kept(count, 0);
  for (size_t s_idx : initial_closure) {
    kept[s_idx] = 1;
  }

  std::vector<size_t> scratch;
  auto try_merge = [&](size_t cand) {
    if (kept[cand]) {
      return;
    }
    size_t add = 0;
    if (!try_closure(cand, &kept, kept_nodes, &scratch, &add)) {
      return;
    }
    for (size_t added_idx : scratch) {
      kept[added_idx] = 1;
    }
    kept_nodes += add;
  };

  // Pull soft setup writes referenced by statements already in the window.
  std::vector<size_t> soft_cands;
  for (size_t i = 0; i < count; i++) {
    if (!kept[i]) {
      continue;
    }
    for (size_t w : soft_deps[i]) {
      if (!kept[w]) {
        soft_cands.push_back(w);
      }
    }
  }
  SortUnique(&soft_cands);
  for (size_t s_pos = soft_cands.size(); s_pos > 0; s_pos--) {
    try_merge(soft_cands[s_pos - 1]);
  }

  // Add later statements that consume declarations already in the window.
  for (size_t j = seed_stmt + 1; j < count && kept_nodes < budget; j++) {
    if (kept[j]) {
      continue;
    }
    bool uses_kept = false;
    for (size_t dep : hard_deps[j]) {
      if (kept[dep]) {
        uses_kept = true;
        break;
      }
    }
    if (uses_kept) {
      try_merge(j);
    }
  }

  // Fill remaining budget with nearby statements to preserve control context.
  const size_t target_fill = (budget * 3) / 4;
  for (size_t j = seed_stmt + 1; j < count && kept_nodes < target_fill; j++) {
    try_merge(j);
  }
  for (size_t j = seed_stmt; j > 0 && kept_nodes < target_fill; j--) {
    try_merge(j - 1);
  }

  std::vector<fast::Statement*> rebuilt = std::move(directives);
  for (size_t i = 0; i < count; i++) {
    if (kept[i]) {
      rebuilt.push_back(stmts[i]);
    }
  }
  *body_io = std::move(rebuilt);
  return true;
}

void ShrinkLargeContainers(fast::Node* root, size_t max_elems) {
  fast::Walk(root, [&](fast::Node* node) {
    if (auto* arr = node->As<fast::ArrayExpression>()) {
      if (arr->elements.size() > max_elems) {
        arr->elements.resize(max_elems);
      }
    } else if (auto* obj = node->As<fast::ObjectExpression>()) {
      if (obj->properties.size() > max_elems) {
        obj->properties.resize(max_elems);
      }
    } else if (auto* call = node->As<fast::CallExpression>()) {
      if (call->arguments.size() > max_elems) {
        call->arguments.resize(max_elems);
      }
    } else if (auto* nw = node->As<fast::NewExpression>()) {
      if (nw->arguments.size() > max_elems) {
        nw->arguments.resize(max_elems);
      }
    } else if (auto* sw = node->As<fast::SwitchStatement>()) {
      if (sw->cases.size() > max_elems) {
        sw->cases.resize(max_elems);
      }
    } else if (auto* vd = node->As<fast::VariableDeclaration>()) {
      if (vd->declarations.size() > max_elems) {
        vd->declarations.resize(max_elems);
      }
    }
    return true;
  });
}

bool TryCompactInnerStatement(fast::Tree* tree, fast::Statement* stmt,
                              size_t max_nodes, Random& rng) {
  if (auto* expr_stmt = stmt->As<fast::ExpressionStatement>()) {
    if (auto* call = expr_stmt->expression != nullptr
                         ? expr_stmt->expression->As<fast::CallExpression>()
                         : nullptr) {
      if (call->callee != nullptr &&
          (call->callee->Is<fast::FunctionExpression>() ||
           call->callee->Is<fast::ArrowFunctionExpression>())) {
        fast::FunctionData* fn = fast::AsFunctionData(call->callee);
        if (fn != nullptr && fn->body != nullptr &&
            CompactStatementList(
                tree, &fn->body->body, max_nodes, rng,
                !tree->root()->strict ? &fn->strict : nullptr)) {
          return true;
        }
      }
    }
  } else if (auto* fd = stmt->As<fast::FunctionDeclaration>()) {
    if (fd->body != nullptr &&
        CompactStatementList(tree, &fd->body->body, max_nodes, rng,
                             !tree->root()->strict ? &fd->strict : nullptr)) {
      return true;
    }
  } else if (auto* bs = stmt->As<fast::BlockStatement>()) {
    if (CompactStatementList(tree, &bs->body, max_nodes, rng, nullptr)) {
      return true;
    }
  } else if (auto* ts = stmt->As<fast::TryStatement>()) {
    if (ts->handler != nullptr && ts->finalizer != nullptr &&
        fast::CountNodes(tree->root()) > max_nodes) {
      ts->finalizer = nullptr;
    }
    if (ts->block != nullptr &&
        CompactStatementList(tree, &ts->block->body, max_nodes, rng, nullptr)) {
      return true;
    }
  } else if (auto* ifs = stmt->As<fast::IfStatement>()) {
    if (ifs->alternate != nullptr &&
        fast::CountNodes(tree->root()) > max_nodes) {
      ifs->alternate = nullptr;
    }
    if (ifs->consequent != nullptr &&
        ifs->consequent->Is<fast::BlockStatement>() &&
        CompactStatementList(
            tree, &ifs->consequent->Cast<fast::BlockStatement>()->body,
            max_nodes, rng, nullptr)) {
      return true;
    }
  }
  return fast::CountNodes(tree->root()) <= max_nodes;
}

void TrimNestedBlocksToFit(fast::Tree* tree, size_t max_nodes) {
  std::vector<fast::BlockStatement*> blocks;
  fast::Walk(tree->root(), [&](fast::Node* node) {
    if (auto* bs = node->As<fast::BlockStatement>()) {
      blocks.push_back(bs);
    }
    return true;
  });
  for (size_t i = blocks.size();
       i > 0 && fast::CountNodes(tree->root()) > max_nodes; i--) {
    fast::BlockStatement* bs = blocks[i - 1];
    while (bs->body.size() > 1 && fast::CountNodes(tree->root()) > max_nodes) {
      bs->body.pop_back();
    }
  }
}

struct DonorSymbol {
  std::string name;
  bool is_function = false;
  bool is_class = false;
  size_t arity = 0;
};

struct DonorInterface {
  std::vector<DonorSymbol> all;
  std::vector<DonorSymbol> callables;
  std::vector<std::string> props;
};

DonorInterface CollectDonorInterface(const fast::Tree& tree,
                                     const fast::ScopeAnalysis& scopes) {
  DonorInterface info;
  absl::flat_hash_set<std::string_view> seen_names;
  for (const fast::Binding* b : scopes.root()->bindings) {
    if (b->kind == fast::BindingKind::kImplicit ||
        b->kind == fast::BindingKind::kUsing ||
        b->kind == fast::BindingKind::kAwaitUsing || b->declarations.empty()) {
      continue;
    }
    if (b->name.empty() || b->name == "eval" || b->name == "arguments" ||
        !seen_names.insert(b->name).second) {
      continue;
    }
    DonorSymbol sym;
    sym.name = std::string(b->name);
    if (b->kind == fast::BindingKind::kFunction) {
      sym.is_function = true;
      if (b->declaration != nullptr) {
        if (const auto* fn = fast::AsFunctionData(b->declaration)) {
          sym.arity = std::min<size_t>(fn->params.size(), 3);
        }
      }
    } else if (b->kind == fast::BindingKind::kClass) {
      sym.is_class = true;
      sym.arity = 1;
    } else if (b->declaration != nullptr) {
      if (const auto* d = b->declaration->As<fast::VariableDeclarator>()) {
        if (d->target == b->identifier && d->init != nullptr) {
          if (const auto* fn = fast::AsFunctionData(d->init)) {
            sym.is_function = true;
            sym.arity = std::min<size_t>(fn->params.size(), 3);
          } else if (d->init->Is<fast::ClassExpression>()) {
            sym.is_class = true;
            sym.arity = 1;
          }
        }
      }
    }
    if (sym.is_function || sym.is_class) {
      info.callables.push_back(sym);
    }
    info.all.push_back(std::move(sym));
  }

  absl::flat_hash_set<std::string_view> seen_props;
  fast::Walk(tree.root(), [&](fast::Node* node) {
    std::string_view name;
    if (auto* m = node->As<fast::MemberExpression>()) {
      if (!m->computed && m->property != nullptr &&
          m->property->Is<fast::Identifier>()) {
        name = m->property->Cast<fast::Identifier>()->name;
      }
    } else if (auto* p = node->As<fast::Property>()) {
      if (!p->computed && p->key != nullptr && p->key->Is<fast::Identifier>()) {
        name = p->key->Cast<fast::Identifier>()->name;
      }
    } else if (auto* md = node->As<fast::MethodDefinition>()) {
      if (!md->computed && !md->is_static && md->key != nullptr &&
          md->key->Is<fast::Identifier>()) {
        name = md->key->Cast<fast::Identifier>()->name;
      }
    } else if (auto* pd = node->As<fast::PropertyDefinition>()) {
      if (!pd->computed && !pd->is_static && pd->key != nullptr &&
          pd->key->Is<fast::Identifier>()) {
        name = pd->key->Cast<fast::Identifier>()->name;
      }
    }
    if (!name.empty() && name != "__proto__" && name != "constructor" &&
        IsIdentifierName(name) && seen_props.insert(name).second) {
      info.props.emplace_back(name);
    }
    return true;
  });
  return info;
}

void EmitCrossDonorGlue(
    fast::Tree* out, const std::vector<std::unique_ptr<fast::Tree>>& trees,
    const std::vector<std::unique_ptr<fast::ScopeAnalysis>>& scopes,
    size_t max_fuse_nodes, int max_glue, Random& rng) {
  if (max_glue <= 0 || trees.size() < 2) {
    return;
  }
  std::vector<DonorInterface> active;
  for (size_t i = 0; i < trees.size(); i++) {
    DonorInterface iface = CollectDonorInterface(*trees[i], *scopes[i]);
    if (!iface.all.empty()) {
      active.push_back(std::move(iface));
    }
  }
  if (active.size() < 2) {
    return;
  }

  size_t current_nodes = fast::CountNodes(out->root());
  if (current_nodes >= max_fuse_nodes) {
    return;
  }
  const int target_count = static_cast<int>(rng.Range(1, max_glue));
  for (int g = 0; g < target_count; g++) {
    const size_t idx_a = rng.Index(active.size());
    size_t idx_b = (idx_a + 1 + rng.Index(active.size() - 1)) % active.size();
    const DonorInterface* a = &active[idx_a];
    const DonorInterface* b = &active[idx_b];

    // Prefer a callable on donor `a` when only `b` exports one.
    if (a->callables.empty() && !b->callables.empty()) {
      std::swap(a, b);
    } else if (!a->callables.empty() && !b->callables.empty() && rng.Coin()) {
      std::swap(a, b);
    }

    auto make_arg = [&](const DonorInterface& from) -> fast::Expression* {
      const DonorSymbol& s = rng.Pick(from.all);
      if (s.is_class && rng.Coin()) {
        return mk::New(out, mk::Id(out, s.name), {});
      }
      return mk::Id(out, s.name);
    };
    auto pick_prop = [&]() -> std::string_view {
      if (!a->props.empty() && (b->props.empty() || rng.Coin())) {
        return rng.Pick(a->props);
      }
      if (!b->props.empty()) {
        return rng.Pick(b->props);
      }
      return "valueOf";
    };

    fast::Expression* glue_expr = nullptr;
    if (!a->callables.empty()) {
      const DonorSymbol& callee = rng.Pick(a->callables);
      const size_t arg_count = std::max<size_t>(1, callee.arity);
      std::vector<fast::Expression*> args;
      args.reserve(arg_count);
      for (size_t k = 0; k < arg_count; k++) {
        const DonorInterface* src =
            (active.size() >= 3 && k > 0 && rng.Coin())
                ? &active[(idx_a + 1 + rng.Index(active.size() - 1)) %
                          active.size()]
                : b;
        if (src == a) {
          src = b;
        }
        args.push_back(make_arg(*src));
      }
      if (callee.is_class) {
        fast::Expression* inst =
            mk::New(out, mk::Id(out, callee.name), std::move(args));
        if (!a->props.empty() && rng.Coin()) {
          glue_expr =
              mk::CallMethod(out, inst, rng.Pick(a->props), {make_arg(*b)});
        } else {
          glue_expr = inst;
        }
      } else {
        const std::string_view prop =
            !b->props.empty() ? std::string_view(rng.Pick(b->props)) : "";
        if (!prop.empty() && args.size() == 1 && rng.Chance(0.25)) {
          const DonorSymbol& obj = rng.Pick(b->all);
          glue_expr = mk::Call(out, mk::Id(out, callee.name),
                               {mk::Member(out, mk::Id(out, obj.name), prop)});
        } else {
          glue_expr = mk::Call(out, mk::Id(out, callee.name), std::move(args));
        }
      }
    } else {
      const DonorSymbol& sym_a = rng.Pick(a->all);
      const DonorSymbol& sym_b = rng.Pick(b->all);
      const int kind = static_cast<int>(rng.Index(3));
      if (kind == 0) {
        glue_expr = mk::CallMethod(out, mk::Id(out, sym_a.name), pick_prop(),
                                   {mk::Id(out, sym_b.name)});
      } else if (kind == 1) {
        glue_expr = mk::Assign(
            out, mk::Member(out, mk::Id(out, sym_a.name), pick_prop()),
            mk::Id(out, sym_b.name));
      } else {
        glue_expr =
            mk::CallMethod(out, mk::Id(out, "Object"), "setPrototypeOf",
                           {mk::Id(out, sym_a.name), mk::Id(out, sym_b.name)});
      }
    }

    fast::Statement* stmt =
        mk::Try(out, {mk::ExprStmt(out, glue_expr)}, "", {});
    const size_t stmt_nodes = fast::CountNodes(stmt);
    if (current_nodes + stmt_nodes > max_fuse_nodes) {
      break;
    }
    out->root()->body.push_back(stmt);
    current_nodes += stmt_nodes;
  }
}

}  // namespace

double MaterialPartnerChance() { return kMaterialPartnerChance(); }
double MaterialFuseChance() { return kMaterialFuseChance(); }
size_t MaterialMaxDonorNodes() {
  return static_cast<size_t>(std::max(1, kMaterialMaxDonorNodes.AsInt()));
}
size_t MaterialMaxFuseNodes() {
  return static_cast<size_t>(std::max(1, kMaterialMaxFuseNodes.AsInt()));
}
size_t MaterialMaxCombineNodes() {
  return static_cast<size_t>(std::max(1, kMaterialMaxCombineNodes.AsInt()));
}
int MaterialGlueStatements() {
  return std::max(0, kMaterialGlueStatements.AsInt());
}

bool ExtractCompactWindow(fast::Tree* tree, size_t max_nodes, Random& rng) {
  if (tree == nullptr || tree->root() == nullptr || max_nodes == 0) {
    return false;
  }
  fast::Program* root = tree->root();
  bool has_content = false;
  for (const fast::Statement* s : root->body) {
    if (s != nullptr && !s->Is<fast::EmptyStatement>() && !IsDirectiveStmt(s)) {
      has_content = true;
      break;
    }
  }
  if (!has_content) {
    return false;
  }
  if (fast::CountNodes(root) <= max_nodes) {
    return true;
  }
  if (CompactStatementList(tree, &root->body, max_nodes, rng, &root->strict)) {
    return true;
  }

  // Fallback for a single oversized statement such as an unwrappable IIFE or
  // a large literal expression.
  std::vector<fast::Statement*> candidates;
  for (fast::Statement* s : root->body) {
    if (s != nullptr && !s->Is<fast::EmptyStatement>() && !IsDirectiveStmt(s)) {
      candidates.push_back(s);
    }
  }
  if (candidates.empty()) {
    return false;
  }
  std::sort(candidates.begin(), candidates.end(),
            [](const fast::Statement* a, const fast::Statement* b) {
              return fast::CountNodes(a) < fast::CountNodes(b);
            });
  const size_t pool = std::min<size_t>(candidates.size(), 4);
  fast::Statement* chosen = candidates[rng.Index(pool)];
  root->body = {chosen};
  root->strict = false;
  if (fast::CountNodes(root) <= max_nodes) {
    return true;
  }

  ShrinkLargeContainers(chosen, 16);
  if (TryCompactInnerStatement(tree, chosen, max_nodes, rng)) {
    return true;
  }
  ShrinkLargeContainers(chosen, 4);
  if (fast::CountNodes(root) <= max_nodes) {
    return true;
  }
  TrimNestedBlocksToFit(tree, max_nodes);
  return fast::CountNodes(root) <= max_nodes && !root->body.empty();
}

bool IsMaterialSkippedPath(std::string_view path) {
  std::string norm = NormalizeSlashes(path);
  size_t slash = norm.rfind('/');
  std::string_view base = slash == std::string::npos
                              ? norm
                              : std::string_view(norm).substr(slash + 1);
  if (base.ends_with("_FIXTURE.js")) {
    return true;
  }
  for (std::string_view skipped : kSkippedFileNames) {
    if (base == skipped) {
      return true;
    }
  }
  if (base.ends_with(".js") &&
      (base.find("numops-fuzz-part") != std::string_view::npos ||
       base.starts_with("modules") || base.starts_with("regexp-property-") ||
       base.find("foozzie") != std::string_view::npos)) {
    return true;
  }
  if (slash != std::string::npos) {
    std::string_view dir = std::string_view(norm).substr(0, slash);
    for (std::string_view skipped_dir : kSkippedDirSegments) {
      if (HasDirSubpath(dir, skipped_dir)) {
        return true;
      }
    }
  }
  return false;
}

bool HasDiscouragedFlags(std::string_view source) {
  size_t pos = 0;
  while (pos < source.size()) {
    size_t end = source.find('\n', pos);
    if (end == std::string_view::npos) {
      end = source.size();
    }
    std::string_view line = source.substr(pos, end - pos);
    if (!line.empty() && line.back() == '\r') {
      line.remove_suffix(1);
    }
    line = TrimLeftWs(line);
    if (line.starts_with("// Flags:")) {
      std::string_view rest = line.substr(9);
      size_t i = 0;
      while (i < rest.size()) {
        while (i < rest.size() && (rest[i] == ' ' || rest[i] == '\t')) {
          i++;
        }
        size_t start = i;
        while (i < rest.size() && rest[i] != ' ' && rest[i] != '\t') {
          i++;
        }
        if (i > start &&
            IsDiscouragedFlagToken(rest.substr(start, i - start))) {
          return true;
        }
      }
    }
    pos = end + 1;
  }
  return false;
}

MaterialBank::LoadStats MaterialBank::Load(const Options& options) {
  Stopwatch timer;
  programs_.clear();
  LoadStats stats;
  if (options.paths.empty() || options.max_programs == 0) {
    stats.elapsed_ms = static_cast<double>(timer.ElapsedUs()) / 1000.0;
    return stats;
  }

  std::vector<std::string> files;
  absl::flat_hash_set<std::string> seen_paths;
  for (const std::string& path : options.paths) {
    std::vector<std::string> listed = ListFiles(path, ".js");
    if (listed.empty()) {
      REDLINE_WARN(kLog, "--material=%s: no .js files found", path.c_str());
    }
    for (const std::string& file : listed) {
      if (seen_paths.insert(file).second) {
        files.push_back(file);
      }
    }
  }

  ImportOptions import_opts;
  import_opts.root = options.import_root;
  Random rng(options.seed == 0 ? 1 : options.seed);
  size_t accepted = 0;

  for (const std::string& file : files) {
    stats.scanned++;
    if (IsMaterialSkippedPath(file)) {
      stats.skipped++;
      continue;
    }
    size_t bytes = 0;
    if (!FileSize(file, &bytes) || bytes >= kMaxFileBytes) {
      stats.skipped++;
      continue;
    }
    std::string source;
    std::string err;
    if (!ReadFileToString(file, &source, &err)) {
      stats.errors++;
      continue;
    }
    if (IsModuleHeaderSource(source) || HasDiscouragedFlags(source)) {
      stats.skipped++;
      continue;
    }
    ImportInfo import_info;
    std::unique_ptr<fast::Tree> tree =
        ImportSource(source, file, import_opts, &import_info, &err);
    if (tree == nullptr) {
      if (err.starts_with("test262")) {
        stats.skipped++;
      } else {
        stats.errors++;
      }
      continue;
    }
    if (tree->root()->body.empty()) {
      stats.skipped++;
      continue;
    }

    Lineage lineage;
    lineage.origin = "material";
    lineage.detail = file;
    std::shared_ptr<Program> prog =
        Program::FromTree(std::move(tree), std::move(lineage));
    prog->flags = std::move(import_info.flags);
    accepted++;
    if (programs_.size() < options.max_programs) {
      programs_.push_back(std::move(prog));
    } else {
      size_t slot = rng.Index(accepted);
      if (slot < options.max_programs) {
        programs_[slot] = std::move(prog);
      }
    }
  }

  stats.loaded = programs_.size();
  stats.elapsed_ms = static_cast<double>(timer.ElapsedUs()) / 1000.0;
  return stats;
}

ProgramPtr MaterialBank::Pick(Random& rng) const {
  if (programs_.empty()) {
    return nullptr;
  }
  return programs_[rng.Index(programs_.size())];
}

bool UnwrapTopLevelIIFEs(fast::Tree* tree) {
  if (tree == nullptr || tree->root() == nullptr) {
    return false;
  }
  std::unique_ptr<fast::ScopeAnalysis> scopes =
      fast::ScopeAnalysis::Analyze(tree);
  absl::flat_hash_set<std::string> all_names;
  for (std::string_view name : scopes->all_names()) {
    all_names.emplace(name);
  }
  absl::flat_hash_set<std::string> top_names;
  for (const fast::Binding* b : scopes->root()->bindings) {
    top_names.emplace(b->name);
  }
  for (const fast::Identifier* id : scopes->unresolved()) {
    top_names.emplace(id->name);
  }

  bool changed = false;
  int fresh_counter = 0;
  std::vector<fast::Statement*> new_body;
  new_body.reserve(tree->root()->body.size());

  for (fast::Statement* stmt : tree->root()->body) {
    const fast::FunctionData* fn = nullptr;
    fast::Node* fn_node = nullptr;
    if (!ExtractZeroParamIIFE(stmt, &fn, &fn_node)) {
      new_body.push_back(stmt);
      continue;
    }
    if (!tree->root()->strict && HasStrictDirective(fn->body)) {
      new_body.push_back(stmt);
      continue;
    }
    if (HasSelfNameReference(fn, fn_node, *scopes)) {
      new_body.push_back(stmt);
      continue;
    }
    // `using` is not valid at the top level of a script.
    if (std::any_of(fn->body->body.begin(), fn->body->body.end(),
                    IsUsingDeclaration)) {
      new_body.push_back(stmt);
      continue;
    }
    fast::Scope* fn_scope = scopes->ScopeOf(fn_node);
    fast::Scope* body_scope = scopes->ScopeOf(fn->body);
    if ((fn_scope != nullptr && fn_scope->dynamic_inside) ||
        (body_scope != nullptr && body_scope->dynamic_inside)) {
      new_body.push_back(stmt);
      continue;
    }
    fast::Requirements req = fast::AnalyzeRequirements(fn->body);
    if (req.needs_function || req.needs_async || req.needs_generator ||
        req.needs_break || req.needs_continue || req.needs_this ||
        req.needs_arguments || req.needs_super || req.needs_new_target ||
        req.has_external_label || (req.needs_sloppy && tree->root()->strict)) {
      new_body.push_back(stmt);
      continue;
    }

    RenameCollidingScopeBindings(tree, fn_scope, &top_names, &all_names,
                                 &fresh_counter);
    if (body_scope != fn_scope) {
      RenameCollidingScopeBindings(tree, body_scope, &top_names, &all_names,
                                   &fresh_counter);
    }

    for (fast::Statement* inner : fn->body->body) {
      if (!IsDirectiveStmt(inner)) {
        new_body.push_back(inner);
      }
    }
    changed = true;
  }

  if (changed) {
    tree->root()->body = std::move(new_body);
  }
  return changed;
}

std::shared_ptr<Program> FusePrograms(const std::vector<ProgramPtr>& donors,
                                      Lineage lineage, Random* rng) {
  if (donors.empty()) {
    return nullptr;
  }

  Random fallback_rng(donors.size() + 1);
  Random& effective_rng = rng != nullptr ? *rng : fallback_rng;

  const size_t max_donor = MaterialMaxDonorNodes();
  const size_t max_fuse = MaterialMaxFuseNodes();
  const int max_glue = MaterialGlueStatements();
  size_t non_null_donors = 0;
  for (const ProgramPtr& d : donors) {
    if (d != nullptr) {
      non_null_donors++;
    }
  }
  const size_t glue_reserve =
      (max_glue > 0 && non_null_donors >= 2)
          ? std::min(max_fuse / 4, static_cast<size_t>(max_glue) * 14)
          : 0;

  std::vector<std::unique_ptr<fast::Tree>> trees;
  std::vector<ProgramPtr> included_donors;
  trees.reserve(donors.size());
  included_donors.reserve(donors.size());
  size_t accumulated_nodes = 1;  // Program root node.

  for (const ProgramPtr& donor : donors) {
    if (donor == nullptr) {
      continue;
    }
    std::unique_ptr<fast::Tree> copy = donor->CopyTree();
    UnwrapTopLevelIIFEs(copy.get());
    if (copy->root() == nullptr || copy->root()->body.empty()) {
      continue;
    }

    const size_t usable_fuse = (!trees.empty() && max_fuse > glue_reserve)
                                   ? max_fuse - glue_reserve
                                   : max_fuse;
    const size_t remaining =
        usable_fuse > accumulated_nodes ? usable_fuse - accumulated_nodes : 0;
    if (!trees.empty() && remaining < 24) {
      break;
    }

    size_t donor_budget = max_donor;
    if (trees.empty()) {
      if (non_null_donors >= 2 && max_fuse > glue_reserve + 80) {
        const size_t split_cap =
            std::max<size_t>(40, (max_fuse - glue_reserve) / 2 + 40);
        donor_budget = std::min(max_donor, split_cap);
      } else {
        donor_budget =
            std::min(max_donor, max_fuse > 8 ? max_fuse - 8 : max_fuse);
      }
    } else {
      const size_t target_window = remaining > 12 ? remaining - 12 : remaining;
      donor_budget = std::min(max_donor, target_window);
    }

    if (fast::CountNodes(copy->root()) > donor_budget &&
        !ExtractCompactWindow(copy.get(), donor_budget, effective_rng)) {
      continue;
    }
    const size_t branch_limit = trees.empty() ? max_fuse : usable_fuse;
    size_t iso_nodes = EstimateIsolatedNodes(copy->root());
    bool fits = true;
    for (int retry = 0;
         retry < 4 && accumulated_nodes + iso_nodes > branch_limit; retry++) {
      if (branch_limit <= accumulated_nodes + 16 || iso_nodes == 0) {
        fits = false;
        break;
      }
      const size_t cur_nodes = fast::CountNodes(copy->root());
      const size_t avail_iso = branch_limit - accumulated_nodes;
      const size_t scaled = (cur_nodes * avail_iso) / iso_nodes;
      const size_t slack_target = scaled > 8 ? scaled - 8 : scaled;
      const size_t max_step = cur_nodes > 1 ? cur_nodes - 1 : 0;
      const size_t tighter =
          std::min(donor_budget, std::min(max_step, slack_target));
      if (tighter == 0 ||
          !ExtractCompactWindow(copy.get(), tighter, effective_rng)) {
        fits = false;
        break;
      }
      iso_nodes = EstimateIsolatedNodes(copy->root());
    }
    if (!fits || accumulated_nodes + iso_nodes > max_fuse) {
      continue;
    }

    accumulated_nodes += iso_nodes;
    trees.push_back(std::move(copy));
    included_donors.push_back(donor);
  }
  if (trees.empty()) {
    return nullptr;
  }

  std::vector<std::unique_ptr<fast::ScopeAnalysis>> donor_scopes;
  donor_scopes.reserve(trees.size());
  absl::flat_hash_set<std::string> all_names;
  absl::flat_hash_set<std::string> occupied_names;
  for (const std::unique_ptr<fast::Tree>& t : trees) {
    auto scopes = fast::ScopeAnalysis::Analyze(t.get());
    for (std::string_view name : scopes->all_names()) {
      all_names.emplace(name);
    }
    for (const fast::Identifier* id : scopes->unresolved()) {
      occupied_names.emplace(id->name);
    }
    donor_scopes.push_back(std::move(scopes));
  }

  int fresh_counter = 0;
  for (size_t i = 0; i < trees.size(); i++) {
    for (fast::Binding* b : donor_scopes[i]->root()->bindings) {
      if (b->declarations.empty() || b->kind == fast::BindingKind::kImplicit) {
        continue;
      }
      if (occupied_names.contains(b->name)) {
        std::string fresh = FreshName("_f", &fresh_counter, all_names);
        all_names.insert(fresh);
        occupied_names.insert(fresh);
        fast::RenameBinding(trees[i].get(), b, fresh);
      } else {
        occupied_names.emplace(b->name);
      }
    }
  }

  auto fused = std::make_unique<fast::Tree>();
  auto* fused_root = fused->New<fast::Program>();
  fused->set_root(fused_root);
  for (const std::unique_ptr<fast::Tree>& t : trees) {
    EmitIsolatedDonor(fused.get(), t->root(), &fused_root->body);
  }
  if (fused_root->body.empty()) {
    return nullptr;
  }

  EmitCrossDonorGlue(fused.get(), trees, donor_scopes, max_fuse, max_glue,
                     effective_rng);

  if (lineage.origin.empty()) {
    lineage.origin = "Fuse";
  }
  std::shared_ptr<Program> result =
      Program::FromTree(std::move(fused), std::move(lineage));
  std::vector<std::string> merged_flags;
  for (const ProgramPtr& donor : included_donors) {
    if (donor != nullptr && !donor->flags.empty()) {
      merged_flags = MergeProgramFlags(merged_flags, donor->flags);
    }
  }
  result->flags = std::move(merged_flags);
  return result;
}

std::shared_ptr<Program> FusePrograms(const std::vector<ProgramPtr>& donors,
                                      const std::string& detail, Random* rng) {
  Lineage lineage;
  lineage.origin = "Fuse";
  lineage.detail = detail;
  return FusePrograms(donors, std::move(lineage), rng);
}

}  // namespace redline
