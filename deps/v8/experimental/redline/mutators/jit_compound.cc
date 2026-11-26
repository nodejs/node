// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/redline/mutators/jit_compound.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "experimental/redline/core/ast_builder.h"
#include "experimental/redline/core/tunable.h"

namespace redline {
namespace {

REDLINE_TUNABLE(kLocalAllocWeight, "mutators.JitCompound.local_alloc_weight",
                1.0, 0.0, 100.0,
                "Relative weight of the local-allocation escape pattern.");
REDLINE_TUNABLE(kPolyWarmupWeight, "mutators.JitCompound.poly_warmup_weight",
                1.0, 0.0, 100.0,
                "Relative weight of the polymorphic warmup and shift pattern.");
REDLINE_TUNABLE(kInlinedDeoptWeight,
                "mutators.JitCompound.inlined_deopt_weight", 1.0, 0.0, 100.0,
                "Relative weight of the inlined-callee side-effect pattern.");
REDLINE_TUNABLE(kOsrPhiWeight, "mutators.JitCompound.osr_phi_weight", 1.0, 0.0,
                100.0,
                "Relative weight of the OSR representation-transition loop.");
REDLINE_TUNABLE(kLocalLoopMinIters, "mutators.JitCompound.local_loop_min_iters",
                2, 1, 8,
                "Minimum iterations in the Pattern A local-object loop.");
REDLINE_TUNABLE(kLocalLoopMaxIters, "mutators.JitCompound.local_loop_max_iters",
                8, 2, 16,
                "Maximum iterations in the Pattern A local-object loop.");
REDLINE_TUNABLE(kMinWarmupCalls, "mutators.JitCompound.min_warmup_calls", 2, 1,
                8, "Minimum warmup calls before optimization.");
REDLINE_TUNABLE(kMaxWarmupCalls, "mutators.JitCompound.max_warmup_calls", 4, 2,
                12, "Maximum warmup calls before optimization.");

constexpr std::array<int, 4> kOsrLoopBounds = {5, 8, 12, 20};

struct CandidateFn {
  fast::Node* fn_node = nullptr;
  fast::FunctionData* data = nullptr;
  std::string_view call_name;
  fast::Statement* enclosing_stmt = nullptr;
  bool is_method = false;
  bool is_constructor = false;
  bool in_derived_ctor = false;
  bool can_call_in_focus = false;
  std::vector<std::string_view> params;
};

// Returns true when `node` lies inside `b.focus_subtree()`.
bool IsInsideFocus(Builder& b, const fast::Node* node) {
  const fast::Node* focus = b.focus_subtree();
  if (focus == nullptr) {
    return true;
  }
  const fast::Node* cur = node;
  while (cur != nullptr) {
    if (cur == focus) {
      return true;
    }
    cur = b.ParentOf(const_cast<fast::Node*>(cur));
  }
  return false;
}

// Returns true when `fn_node` is a direct argument to `Worker`.
bool IsWorkerCallback(Builder& b, fast::Node* fn_node) {
  fast::Node* parent = b.ParentOf(fn_node);
  if (parent == nullptr) {
    return false;
  }
  fast::Expression* callee = nullptr;
  if (auto* call = parent->As<fast::CallExpression>()) {
    callee = call->callee;
  } else if (auto* nw = parent->As<fast::NewExpression>()) {
    callee = nw->callee;
  }
  if (auto* id = callee != nullptr ? callee->As<fast::Identifier>() : nullptr) {
    return id->name == "Worker";
  }
  return false;
}

// Collects simple `Identifier` parameters usable without syntax errors.
std::vector<std::string_view> CollectSimpleParams(
    const fast::FunctionData* data) {
  std::vector<std::string_view> out;
  for (fast::Expression* param : data->params) {
    if (param == nullptr) {
      continue;
    }
    if (auto* id = param->As<fast::Identifier>()) {
      if (id->name != "eval" && id->name != "arguments") {
        out.push_back(id->name);
      }
      continue;
    }
    if (auto* def = param->As<fast::AssignmentPattern>()) {
      if (auto* id = def->left != nullptr ? def->left->As<fast::Identifier>()
                                          : nullptr) {
        if (id->name != "eval" && id->name != "arguments") {
          out.push_back(id->name);
        }
      }
    }
  }
  return out;
}

// Returns true when `stmt` is a top-level `super(...)` call in a constructor.
bool IsSuperCallStmt(const fast::Statement* stmt) {
  auto* expr_stmt = stmt->As<fast::ExpressionStatement>();
  if (expr_stmt == nullptr || expr_stmt->expression == nullptr) {
    return false;
  }
  auto* call = expr_stmt->expression->As<fast::CallExpression>();
  return call != nullptr && call->callee != nullptr &&
         call->callee->Is<fast::Super>();
}

// Returns the earliest safe statement index inside `body`.
size_t BodyInsertIndex(const fast::BlockStatement* body, bool skip_super_call) {
  size_t idx = 0;
  while (idx < body->body.size()) {
    auto* es = body->body[idx]->As<fast::ExpressionStatement>();
    if (es == nullptr || !es->is_directive) {
      break;
    }
    idx++;
  }
  if (skip_super_call) {
    for (size_t i = idx; i < body->body.size(); i++) {
      if (IsSuperCallStmt(body->body[i])) {
        idx = i + 1;
      }
    }
  }
  return idx;
}

// Inserts `stmts` at `index` inside `body`.
void InsertInBlock(Builder& b, fast::BlockStatement* body, size_t index,
                   std::vector<fast::Statement*> stmts) {
  Builder::Slot slot{&body->body, index, body};
  b.InsertAll(slot, stmts);
}

// Converts a concise arrow body to a block body when needed.
fast::BlockStatement* EnsureBlockBody(Builder& b, CandidateFn* target) {
  if (target->data->body != nullptr) {
    return target->data->body;
  }
  auto* arrow = target->fn_node->As<fast::ArrowFunctionExpression>();
  if (arrow == nullptr || arrow->expression_body == nullptr) {
    return nullptr;
  }
  fast::Tree* t = b.tree();
  fast::BlockStatement* block =
      mk::Block(t, {mk::Return(t, arrow->expression_body)});
  arrow->body = block;
  arrow->expression_body = nullptr;
  b.Invalidate();
  return block;
}

// Describes `fn_node` as a mutation candidate. Returns false when ineligible.
bool DescribeCandidate(Builder& b, fast::Node* fn_node, CandidateFn* out) {
  fast::FunctionData* data = fast::AsFunctionData(fn_node);
  if (data == nullptr || data->is_async || data->is_generator) {
    return false;
  }
  if (data->body == nullptr) {
    auto* arrow = fn_node->As<fast::ArrowFunctionExpression>();
    if (arrow == nullptr || arrow->expression_body == nullptr) {
      return false;
    }
  }
  if (IsWorkerCallback(b, fn_node)) {
    return false;
  }
  out->fn_node = fn_node;
  out->data = data;
  out->params = CollectSimpleParams(data);

  fast::Node* parent = b.ParentOf(fn_node);
  if (auto* decl = fn_node->As<fast::FunctionDeclaration>()) {
    out->enclosing_stmt = decl;
    if (decl->name != nullptr) {
      out->call_name = decl->name->name;
    }
  } else if (parent != nullptr) {
    if (auto* method = parent->As<fast::MethodDefinition>()) {
      out->is_method = true;
      out->is_constructor =
          method->method_kind == fast::MethodKind::kConstructor;
      fast::Node* cls_body = b.ParentOf(method);
      fast::Node* cls_node =
          cls_body != nullptr ? b.ParentOf(cls_body) : nullptr;
      fast::ClassData* cls =
          cls_node != nullptr ? fast::AsClassData(cls_node) : nullptr;
      out->in_derived_ctor =
          out->is_constructor && cls != nullptr && cls->super_class != nullptr;
    } else if (parent->Is<fast::Property>()) {
      out->is_method = true;
    } else if (auto* vd = parent->As<fast::VariableDeclarator>();
               vd != nullptr && vd->init == fn_node && vd->target != nullptr) {
      if (auto* id = vd->target->As<fast::Identifier>()) {
        fast::Node* vdecl_node = b.ParentOf(vd);
        if (auto* vdecl = vdecl_node != nullptr
                              ? vdecl_node->As<fast::VariableDeclaration>()
                              : nullptr) {
          out->call_name = id->name;
          out->enclosing_stmt = vdecl;
        }
      }
    }
  }

  if (!out->call_name.empty() && out->enclosing_stmt != nullptr) {
    Builder::Slot slot = b.SlotOf(out->enclosing_stmt);
    if (slot.valid() &&
        (slot.owner->Is<fast::Program>() ||
         slot.owner->Is<fast::BlockStatement>()) &&
        IsInsideFocus(b, slot.owner)) {
      out->can_call_in_focus = true;
    }
  }
  return true;
}

// Picks an eligible existing function under the active focus subtree.
bool PickExistingCandidate(Builder& b, CandidateFn* out) {
  std::vector<CandidateFn> standalone;
  std::vector<CandidateFn> methods;
  std::vector<uint32_t> seen_ids;

  auto try_add = [&](fast::Node* fn_node) {
    for (uint32_t id : seen_ids) {
      if (id == fn_node->id) {
        return;
      }
    }
    seen_ids.push_back(fn_node->id);
    CandidateFn cand;
    if (!DescribeCandidate(b, fn_node, &cand)) {
      return;
    }
    if (cand.is_method) {
      methods.push_back(std::move(cand));
    } else {
      standalone.push_back(std::move(cand));
    }
  };

  if (const fast::Node* focus = b.focus_subtree()) {
    fast::Node* focus_mut = const_cast<fast::Node*>(focus);
    if (fast::AsFunctionData(focus_mut) != nullptr) {
      try_add(focus_mut);
    } else if (focus_mut->Is<fast::BlockStatement>()) {
      fast::Node* parent = b.ParentOf(focus_mut);
      if (parent != nullptr && fast::AsFunctionData(parent) != nullptr &&
          fast::AsFunctionData(parent)->body == focus_mut) {
        try_add(parent);
      }
    }
  }

  std::vector<fast::Node*> collected = b.Collect(
      [](fast::Node* n) { return fast::AsFunctionData(n) != nullptr; });
  for (fast::Node* fn_node : collected) {
    try_add(fn_node);
  }

  if (!standalone.empty()) {
    *out = b.rng().Pick(standalone);
    return true;
  }
  if (!methods.empty()) {
    *out = b.rng().Pick(methods);
    return true;
  }
  return false;
}

// Wraps `stmts` in `try { ... } catch {}`.
fast::Statement* TryCatch(fast::Tree* t, std::vector<fast::Statement*> stmts) {
  return mk::Try(t, std::move(stmts), "", {});
}

// Wraps `expr` in `try { expr; } catch {}`.
fast::Statement* TryCallStmt(fast::Tree* t, fast::Expression* expr) {
  return TryCatch(t, {mk::ExprStmt(t, expr)});
}

// Emits `typeof expr === "type_name"`.
fast::Expression* IsTypeofEq(fast::Tree* t, fast::Expression* expr,
                             std::string_view type_name) {
  return mk::Binary(t, fast::BinaryOp::kStrictEq,
                    mk::Unary(t, fast::UnaryOp::kTypeof, expr),
                    mk::Str(t, type_name));
}

// Emits `var && typeof var === "object"`.
fast::Expression* IsNonNullObject(fast::Tree* t, std::string_view var_name) {
  return mk::Logical(t, fast::LogicalOp::kAnd, mk::Id(t, var_name),
                     IsTypeofEq(t, mk::Id(t, var_name), "object"));
}

// Draws a clamped warmup call count from tunables.
int DrawWarmupCalls(Random& rng) {
  const int lo = kMinWarmupCalls.AsInt();
  const int hi = kMaxWarmupCalls.AsInt() < lo ? lo : kMaxWarmupCalls.AsInt();
  return static_cast<int>(rng.Range(lo, hi));
}

// Synthesizes a compact helper function at a valid insertion slot.
bool SynthesizeHelper(Builder& b, const Builder::Slot& slot,
                      std::vector<std::string_view> param_names,
                      std::vector<fast::Statement*> extra_body,
                      CandidateFn* out) {
  if (!slot.valid()) {
    return false;
  }
  fast::Tree* t = b.tree();
  std::string_view fn_name = b.FreshName("jc");
  std::string_view acc_name = b.FreshName("jc");

  std::vector<fast::Statement*> body;
  if (!param_names.empty()) {
    std::string_view p0 = param_names[0];
    // Initialize `acc_name` and update `p0.a` when `p0` is an object.
    body.push_back(mk::Let(t, acc_name, mk::Num(t, 0)));
    fast::Expression* is_obj = IsNonNullObject(t, p0);
    fast::Expression* read_a =
        mk::Logical(t, fast::LogicalOp::kOr, mk::Member(t, mk::Id(t, p0), "a"),
                    mk::Index(t, mk::Id(t, p0), mk::Num(t, 0)));
    fast::Expression* fallback =
        mk::Logical(t, fast::LogicalOp::kOr, read_a, mk::Num(t, 0));
    fast::Statement* assign_acc =
        mk::ExprStmt(t, mk::Assign(t, mk::Id(t, acc_name), fallback));
    fast::Expression* is_num = IsTypeofEq(t, mk::Id(t, acc_name), "number");
    fast::Statement* bump_prop = TryCatch(
        t, {mk::ExprStmt(
               t, mk::Assign(t, mk::Member(t, mk::Id(t, p0), "a"),
                             mk::Binary(t, fast::BinaryOp::kAdd,
                                        mk::Cond(t, is_num, mk::Id(t, acc_name),
                                                 mk::Num(t, 0)),
                                        mk::Num(t, 1))))});
    body.push_back(mk::If(
        t, is_obj, mk::Block(t, {assign_acc, bump_prop}),
        mk::ExprStmt(t, mk::Assign(t, mk::Id(t, acc_name), mk::Num(t, 1)))));
  } else {
    body.push_back(mk::Let(t, acc_name, mk::Num(t, 1)));
  }
  for (fast::Statement* s : extra_body) {
    body.push_back(s);
  }
  body.push_back(mk::Return(t, mk::Id(t, acc_name)));

  fast::FunctionDeclaration* decl =
      mk::FunctionDecl(t, fn_name, param_names, std::move(body));
  b.Insert(slot, decl);

  *out = CandidateFn{};
  out->fn_node = decl;
  out->data = decl;
  out->call_name = fn_name;
  out->enclosing_stmt = decl;
  out->params = std::move(param_names);
  out->can_call_in_focus = (slot.owner->Is<fast::Program>() ||
                            slot.owner->Is<fast::BlockStatement>()) &&
                           IsInsideFocus(b, slot.owner);
  return true;
}

// Emits a contrasting JS shape or elements-kind literal for call arguments.
fast::Expression* BuildShapeArg(fast::Tree* t, int kind) {
  switch (kind % 7) {
    case 0:
      // {a: 1}
      return mk::Object(t, {mk::Prop(t, "a", mk::Num(t, 1))});
    case 1:
      // {a: 1.5, b: 2}
      return mk::Object(t, {mk::Prop(t, "a", mk::Num(t, 1.5)),
                            mk::Prop(t, "b", mk::Num(t, 2))});
    case 2:
      // [1, 2]
      return mk::Array(t, {mk::Num(t, 1), mk::Num(t, 2)});
    case 3:
      // [1.5, , 3]
      return mk::Array(t, {mk::Num(t, 1.5), nullptr, mk::Num(t, 3)});
    case 4:
      // [1, "s", {a: 2}]
      return mk::Array(t, {mk::Num(t, 1), mk::Str(t, "s"),
                           mk::Object(t, {mk::Prop(t, "a", mk::Num(t, 2))})});
    case 5:
      // Getter accessor object literal.
      return mk::Object(
          t,
          {mk::Accessor(
               t, "a",
               mk::Function(t, "", {}, {mk::Return(t, mk::Num(t, 1.5))}), true),
           mk::Prop(t, "b", mk::Num(t, 2))});
    default:
      // {a: 1.5, b: 2, c: 3n}
      return mk::Object(t, {mk::Prop(t, "a", mk::Num(t, 1.5)),
                            mk::Prop(t, "b", mk::Num(t, 2)),
                            mk::Prop(t, "c", mk::BigInt(t, "3"))});
  }
}

// Builds `argc` distinct arguments centered on `primary_kind`.
std::vector<fast::Expression*> BuildCallArgs(fast::Tree* t, size_t arity,
                                             int primary_kind) {
  size_t count = arity == 0 ? 1 : arity;
  if (count > 3) {
    count = 3;
  }
  std::vector<fast::Expression*> args;
  args.push_back(BuildShapeArg(t, primary_kind));
  for (size_t i = 1; i < count; i++) {
    if (i == 1) {
      args.push_back(BuildShapeArg(t, primary_kind + 2));
    } else {
      args.push_back(mk::Num(t, static_cast<double>(primary_kind + 1)));
    }
  }
  return args;
}

// Builds guarded property write `{ if (p && typeof p === "object") try { ... }
// }`
fast::Statement* GuardedEscWrite(fast::Tree* t, std::string_view slot_var,
                                 std::string_view local_var) {
  fast::Statement* assign = TryCatch(
      t,
      {mk::ExprStmt(t, mk::Assign(t, mk::Member(t, mk::Id(t, slot_var), "esc"),
                                  mk::Id(t, local_var)))});
  return mk::If(t, IsNonNullObject(t, slot_var), assign);
}

// Emits `%Prepare`, warmup calls, `%Optimize`, and post-opt call at `slot`.
void EmitTieringSequence(Builder& b, const Builder::Slot& slot,
                         std::string_view caller_name, size_t arity,
                         std::string_view extra_prep, int warmup_calls,
                         int start_kind, int kind_mod, bool cycle_shape,
                         bool natives, bool emit_opt_and_post, int post_kind) {
  fast::Tree* t = b.tree();
  std::vector<fast::Statement*> seq;
  if (natives) {
    if (!extra_prep.empty()) {
      seq.push_back(
          mk::ExprStmt(t, mk::Native(t, "PrepareFunctionForOptimization",
                                     {mk::Id(t, extra_prep)})));
    }
    seq.push_back(
        mk::ExprStmt(t, mk::Native(t, "PrepareFunctionForOptimization",
                                   {mk::Id(t, caller_name)})));
  }
  for (int i = 0; i < warmup_calls; i++) {
    int kind = cycle_shape ? (start_kind + i) % kind_mod : i;
    seq.push_back(TryCallStmt(
        t, mk::Call(t, mk::Id(t, caller_name), BuildCallArgs(t, arity, kind))));
  }
  if (emit_opt_and_post) {
    if (natives) {
      seq.push_back(mk::ExprStmt(t, mk::Native(t, "OptimizeFunctionOnNextCall",
                                               {mk::Id(t, caller_name)})));
    }
    seq.push_back(TryCallStmt(t, mk::Call(t, mk::Id(t, caller_name),
                                          BuildCallArgs(t, arity, post_kind))));
  }
  b.InsertAll(slot, seq);
}

// Pattern A: local allocation, branch and loop updates, and conditional escape.
bool ApplyLocalAllocation(Builder& b, CandidateFn& target, bool natives,
                          bool synthesized, std::string* detail) {
  fast::BlockStatement* body = EnsureBlockBody(b, &target);
  if (body == nullptr) {
    return false;
  }
  fast::Tree* t = b.tree();
  Random& rng = b.rng();

  const bool use_array = rng.Coin();
  const int lo = kLocalLoopMinIters.AsInt();
  const int hi =
      kLocalLoopMaxIters.AsInt() < lo ? lo : kLocalLoopMaxIters.AsInt();
  const int bound = static_cast<int>(rng.Range(lo, hi));

  std::string_view local_var = b.FreshName("jc");
  std::string_view sum_var = b.FreshName("jc");
  std::string_view idx_var = b.FreshName("jc");

  fast::Expression* seed_expr =
      target.params.empty()
          ? static_cast<fast::Expression*>(mk::Num(t, 1))
          : static_cast<fast::Expression*>(mk::Id(t, target.params[0]));

  auto field_expr = [&](bool primary) -> fast::Expression* {
    if (use_array) {
      return mk::Index(t, mk::Id(t, local_var), mk::Num(t, primary ? 0 : 1));
    }
    return mk::Member(t, mk::Id(t, local_var), primary ? "a" : "b");
  };

  fast::Expression* lit = use_array
                              ? static_cast<fast::Expression*>(
                                    mk::Array(t, {seed_expr, mk::Num(t, 1.5)}))
                              : static_cast<fast::Expression*>(mk::Object(
                                    t, {mk::Prop(t, "a", seed_expr),
                                        mk::Prop(t, "b", mk::Num(t, 1.5))}));

  std::vector<fast::Statement*> stmts;
  // Declare local allocation and accumulator.
  stmts.push_back(mk::Const(t, local_var, lit));
  stmts.push_back(mk::Let(t, sum_var, mk::Num(t, 0)));

  // Branch updating the local allocation.
  fast::Expression* branch_test =
      target.params.empty()
          ? static_cast<fast::Expression*>(mk::Bool(t, rng.Coin()))
          : IsTypeofEq(t, mk::Id(t, target.params[0]), "number");
  fast::Expression* then_rhs =
      target.params.empty() ? static_cast<fast::Expression*>(mk::Num(t, 2))
                            : static_cast<fast::Expression*>(mk::Binary(
                                  t, fast::BinaryOp::kAdd,
                                  mk::Id(t, target.params[0]), mk::Num(t, 1)));
  stmts.push_back(mk::If(
      t, branch_test,
      mk::Block(t,
                {mk::ExprStmt(t, mk::Assign(t, field_expr(true), then_rhs))}),
      mk::Block(t, {mk::ExprStmt(t, mk::Assign(t, field_expr(false),
                                               mk::Num(t, 2.5)))})));

  // Counted loop reading and mutating fields on the virtual object.
  std::vector<fast::Statement*> loop_body;
  loop_body.push_back(
      mk::ExprStmt(t, mk::Assign(t, field_expr(false), mk::Num(t, 0.5),
                                 fast::AssignOp::kAddAssign)));
  loop_body.push_back(
      mk::ExprStmt(t, mk::Assign(t, mk::Id(t, sum_var), field_expr(false),
                                 fast::AssignOp::kAddAssign)));
  stmts.push_back(mk::CountedLoop(t, idx_var,
                                  mk::Num(t, static_cast<double>(bound)),
                                  mk::Block(t, std::move(loop_body))));

  // Read primary slot to keep load elimination engaged.
  fast::Expression* primary_is_num = IsTypeofEq(t, field_expr(true), "number");
  stmts.push_back(
      mk::If(t, primary_is_num,
             mk::ExprStmt(t, mk::Assign(t, mk::Id(t, sum_var), field_expr(true),
                                        fast::AssignOp::kAddAssign))));

  // Conditional escape via parameter slot store or conditional return.
  fast::Expression* escape_cond =
      mk::Binary(t, fast::BinaryOp::kStrictEq,
                 mk::Binary(t, fast::BinaryOp::kBitAnd,
                            mk::Binary(t, fast::BinaryOp::kBitOr,
                                       mk::Id(t, sum_var), mk::Num(t, 0)),
                            mk::Num(t, 1)),
                 mk::Num(t, 0));

  bool store_to_param =
      !target.params.empty() && (target.is_constructor || rng.Coin());
  if (store_to_param) {
    stmts.push_back(mk::If(
        t, escape_cond,
        mk::Block(t, {GuardedEscWrite(t, target.params.back(), local_var)})));
  } else if (!target.is_constructor) {
    stmts.push_back(mk::If(
        t, escape_cond, mk::Block(t, {mk::Return(t, mk::Id(t, local_var))})));
  } else {
    std::string_view obs_var = b.FreshName("jc");
    stmts.push_back(mk::Let(t, obs_var, mk::Id(t, local_var)));
  }

  size_t insert_idx = BodyInsertIndex(body, target.in_derived_ctor);
  InsertInBlock(b, body, insert_idx, std::move(stmts));

  // Add warmup and opt calls when Pattern A synthesized the function.
  if (synthesized && target.can_call_in_focus) {
    Builder::Slot after = b.SlotOf(target.enclosing_stmt);
    if (after.valid()) {
      after.index++;
      int warm = DrawWarmupCalls(rng);
      EmitTieringSequence(b, after, target.call_name, target.params.size(),
                          /*extra_prep=*/"", warm, /*start_kind=*/0,
                          /*kind_mod=*/1, /*cycle_shape=*/false, natives,
                          /*emit_opt_and_post=*/natives, warm + 1);
    }
  }

  *detail = "JitCompound:local_alloc";
  return true;
}

// Pattern B: polymorphic warmup with distinct shapes plus post-opt shift.
bool ApplyPolymorphicWarmup(Builder& b, const CandidateFn* initial_target,
                            bool natives, std::string* detail) {
  Random& rng = b.rng();

  CandidateFn target;
  if (initial_target != nullptr && initial_target->can_call_in_focus) {
    target = *initial_target;
  } else {
    Builder::Slot slot = b.RandomInsertionSlot();
    if (!slot.valid() || !IsInsideFocus(b, slot.owner)) {
      return false;
    }
    std::string_view p0 = b.FreshName("p");
    std::string_view p1 = b.FreshName("p");
    if (!SynthesizeHelper(b, slot, {p0, p1}, {}, &target)) {
      return false;
    }
  }

  Builder::Slot after = b.SlotOf(target.enclosing_stmt);
  if (!after.valid() || !IsInsideFocus(b, after.owner)) {
    return false;
  }
  after.index++;

  const int warmup_count = DrawWarmupCalls(rng);
  const int start_kind = static_cast<int>(rng.Index(4));
  const int post_kind = 4 + static_cast<int>(rng.Index(3));
  EmitTieringSequence(b, after, target.call_name, target.data->params.size(),
                      /*extra_prep=*/"", warmup_count, start_kind,
                      /*kind_mod=*/4, /*cycle_shape=*/true, natives,
                      /*emit_opt_and_post=*/true, post_kind);
  *detail = "JitCompound:poly_warmup";
  return true;
}

// Emits a pure-until-`k` callee `h` and returns its declaration statement.
fast::FunctionDeclaration* BuildSideEffectCallee(
    fast::Tree* t, std::string_view h_name, std::string_view cnt_name,
    std::string_view arg_name, int threshold, int transition_kind) {
  std::vector<fast::Statement*> h_body;
  // Increment call counter `cnt_name`.
  h_body.push_back(mk::ExprStmt(t, mk::Update(t, fast::UpdateOp::kIncrement,
                                              false, mk::Id(t, cnt_name))));

  // Return on warmup fast path while `cnt < threshold`.
  fast::Expression* safe_read = mk::Binary(
      t, fast::BinaryOp::kAdd,
      mk::Logical(t, fast::LogicalOp::kOr,
                  mk::Member(t, mk::Id(t, arg_name), "a"), mk::Num(t, 0)),
      mk::Num(t, 1));
  h_body.push_back(mk::If(
      t,
      mk::Binary(t, fast::BinaryOp::kLessThan, mk::Id(t, cnt_name),
                 mk::Num(t, static_cast<double>(threshold))),
      mk::Block(t, {mk::Return(t, mk::Cond(t, IsNonNullObject(t, arg_name),
                                           safe_read, mk::Num(t, 1)))})));

  // Side effect or deoptimization once `cnt === threshold`.
  std::vector<fast::Statement*> trigger_body;
  switch (transition_kind) {
    case 0: {
      // Property definition via `Object.defineProperty`.
      fast::Expression* desc =
          mk::Object(t, {mk::Prop(t, "value", mk::Num(t, 42)),
                         mk::Prop(t, "configurable", mk::Bool(t, true))});
      fast::Statement* def = TryCallStmt(
          t, mk::CallMethod(t, mk::Id(t, "Object"), "defineProperty",
                            {mk::Id(t, arg_name), mk::Str(t, "d"), desc}));
      trigger_body.push_back(mk::If(t, IsNonNullObject(t, arg_name), def));
      trigger_body.push_back(mk::Return(t, mk::Num(t, 2)));
      break;
    }
    case 1: {
      // Prototype mutation via `Object.setPrototypeOf`.
      fast::Expression* proto =
          mk::Object(t, {mk::Prop(t, "extra", mk::Num(t, 1))});
      fast::Statement* setp = TryCallStmt(
          t, mk::CallMethod(t, mk::Id(t, "Object"), "setPrototypeOf",
                            {mk::Id(t, arg_name), proto}));
      trigger_body.push_back(mk::If(t, IsNonNullObject(t, arg_name), setp));
      trigger_body.push_back(mk::Return(t, mk::Num(t, 3)));
      break;
    }
    case 2: {
      // Array length truncation or fallback property update.
      fast::Expression* is_arr = mk::CallMethod(
          t, mk::Id(t, "Array"), "isArray", {mk::Id(t, arg_name)});
      fast::Statement* trunc = TryCatch(
          t, {mk::ExprStmt(
                 t, mk::Assign(t, mk::Member(t, mk::Id(t, arg_name), "length"),
                               mk::Num(t, 0)))});
      fast::Statement* fallback = TryCatch(
          t, {mk::ExprStmt(
                 t, mk::Assign(t, mk::Member(t, mk::Id(t, arg_name), "len"),
                               mk::Num(t, 0)))});
      trigger_body.push_back(mk::If(
          t, is_arr, trunc, mk::If(t, IsNonNullObject(t, arg_name), fallback)));
      trigger_body.push_back(mk::Return(t, mk::Num(t, 4)));
      break;
    }
    default: {
      // Direct native deoptimization trigger.
      trigger_body.push_back(
          mk::ExprStmt(t, mk::Native(t, "_DeoptimizeNow", {})));
      trigger_body.push_back(mk::Return(t, mk::Num(t, 5)));
      break;
    }
  }
  h_body.push_back(
      mk::If(t,
             mk::Binary(t, fast::BinaryOp::kStrictEq, mk::Id(t, cnt_name),
                        mk::Num(t, static_cast<double>(threshold))),
             mk::Block(t, std::move(trigger_body))));
  h_body.push_back(mk::Return(t, mk::Num(t, 0)));

  return mk::FunctionDecl(t, h_name, {arg_name}, std::move(h_body));
}

// Emits caller-body statements invoking `h_name` and inspecting mutated fields.
std::vector<fast::Statement*> BuildInlinedCallSite(
    fast::Tree* t, std::string_view h_name, std::string_view arg_var,
    std::string_view res_var, std::string_view post_var, bool array_arg) {
  fast::Expression* init =
      array_arg ? static_cast<fast::Expression*>(mk::Array(
                      t, {mk::Num(t, 1), mk::Num(t, 2), mk::Num(t, 3)}))
                : static_cast<fast::Expression*>(
                      mk::Object(t, {mk::Prop(t, "a", mk::Num(t, 1))}));
  fast::Expression* post_read = array_arg
                                    ? static_cast<fast::Expression*>(mk::Index(
                                          t, mk::Id(t, arg_var), mk::Num(t, 0)))
                                    : static_cast<fast::Expression*>(mk::Member(
                                          t, mk::Id(t, arg_var), "a"));
  return {
      mk::Const(t, arg_var, init),
      mk::Let(t, res_var, mk::Call(t, mk::Id(t, h_name), {mk::Id(t, arg_var)})),
      mk::Let(t, post_var, post_read),
      mk::If(t,
             mk::Binary(t, fast::BinaryOp::kStrictEq, mk::Id(t, post_var),
                        mk::Undefined(t)),
             mk::ExprStmt(t, mk::Assign(t, mk::Id(t, res_var), mk::Num(t, 1),
                                        fast::AssignOp::kAddAssign))),
  };
}

// Pattern C: inlined callee with conditional side effect or deoptimization.
bool ApplyInlinedDeopt(Builder& b, const CandidateFn* initial_target,
                       bool natives, std::string* detail) {
  fast::Tree* t = b.tree();
  Random& rng = b.rng();

  const int warmup_calls = DrawWarmupCalls(rng);
  const int threshold = warmup_calls + 1;
  const int transition_kind =
      natives ? static_cast<int>(rng.Index(4)) : static_cast<int>(rng.Index(3));
  const bool array_arg = (transition_kind == 2);

  Builder::Slot before;
  bool use_existing = initial_target != nullptr &&
                      initial_target->can_call_in_focus &&
                      !initial_target->is_constructor;
  if (use_existing) {
    before = b.SlotOf(initial_target->enclosing_stmt);
    if (!before.valid() || !IsInsideFocus(b, before.owner)) {
      use_existing = false;
    }
  }

  Builder::Slot fallback_slot;
  if (!use_existing) {
    fallback_slot = b.RandomInsertionSlot();
    if (!fallback_slot.valid() || !IsInsideFocus(b, fallback_slot.owner)) {
      return false;
    }
  }

  std::string_view cnt_name = b.FreshName("jc");
  std::string_view h_name = b.FreshName("jc");
  std::string_view param_x = b.FreshName("x");
  std::string_view arg_var = b.FreshName("jc");
  std::string_view res_var = b.FreshName("jc");
  std::string_view post_var = b.FreshName("jc");

  // Use `var` so hoisted callers invoked above the declaration avoid TDZ.
  fast::Statement* cnt_decl = mk::Var(t, cnt_name, mk::Num(t, 0));
  fast::FunctionDeclaration* h_decl = BuildSideEffectCallee(
      t, h_name, cnt_name, param_x, threshold, transition_kind);

  auto emit_warmup_seq = [&](const Builder::Slot& after_slot,
                             std::string_view caller_name, size_t arity) {
    EmitTieringSequence(b, after_slot, caller_name, arity, h_name, warmup_calls,
                        /*start_kind=*/0, /*kind_mod=*/2,
                        /*cycle_shape=*/true, natives,
                        /*emit_opt_and_post=*/true, warmup_calls);
  };

  if (use_existing) {
    CandidateFn target = *initial_target;
    fast::BlockStatement* body = EnsureBlockBody(b, &target);
    if (body == nullptr) {
      return false;
    }
    b.InsertAll(before, {cnt_decl, h_decl});

    size_t idx = BodyInsertIndex(body, target.in_derived_ctor);
    InsertInBlock(
        b, body, idx,
        BuildInlinedCallSite(t, h_name, arg_var, res_var, post_var, array_arg));

    Builder::Slot after = b.SlotOf(target.enclosing_stmt);
    after.index++;
    emit_warmup_seq(after, target.call_name, target.data->params.size());
    *detail = "JitCompound:inlined_deopt";
    return true;
  }

  // Insert helper caller inside a focused block or top level.
  std::string_view p0 = b.FreshName("p");
  CandidateFn helper;
  b.InsertAll(fallback_slot, {cnt_decl, h_decl});
  Builder::Slot after_h = b.SlotOf(h_decl);
  after_h.index++;
  if (!SynthesizeHelper(b, after_h, {p0},
                        BuildInlinedCallSite(t, h_name, arg_var, res_var,
                                             post_var, array_arg),
                        &helper)) {
    return false;
  }
  Builder::Slot after_caller = b.SlotOf(helper.enclosing_stmt);
  after_caller.index++;
  emit_warmup_seq(after_caller, helper.call_name, 1);

  *detail = "JitCompound:inlined_deopt";
  return true;
}

// Pattern D: counted loop with Smi -> Float64 -> Tagged/BigInt phi and OSR.
bool ApplyOsrLoopTransition(Builder& b, CandidateFn& target, bool natives,
                            bool synthesized, std::string* detail) {
  fast::BlockStatement* body = EnsureBlockBody(b, &target);
  if (body == nullptr) {
    return false;
  }
  fast::Tree* t = b.tree();
  Random& rng = b.rng();

  const int bound = kOsrLoopBounds[rng.Index(kOsrLoopBounds.size())];
  const int k_float = static_cast<int>(rng.Range(1, (bound - 2) / 2 + 1));
  const int k_big = static_cast<int>(rng.Range(k_float + 1, bound - 1));
  const int k_osr = static_cast<int>(rng.Range(0, bound - 1));
  const bool big_tagged = rng.Coin();

  std::string_view acc_var = b.FreshName("jc");
  std::string_view idx_var = b.FreshName("jc");
  std::string_view obs_var = b.FreshName("jc");

  std::vector<fast::Statement*> loop_body;
  if (natives) {
    loop_body.push_back(mk::If(
        t,
        mk::Binary(t, fast::BinaryOp::kStrictEq, mk::Id(t, idx_var),
                   mk::Num(t, static_cast<double>(k_osr))),
        mk::Block(t, {mk::ExprStmt(t, mk::Native(t, "OptimizeOsr", {}))})));
  }

  // Transition accumulator representation cleanly: Smi -> Float64 -> BigInt or
  // Object.
  fast::Statement* smi_step = mk::Block(
      t, {mk::ExprStmt(t, mk::Assign(t, mk::Id(t, acc_var), mk::Num(t, 1),
                                     fast::AssignOp::kAddAssign))});
  fast::Statement* float_step = mk::Block(
      t, {mk::ExprStmt(t, mk::Assign(t, mk::Id(t, acc_var), mk::Num(t, 0.5),
                                     fast::AssignOp::kAddAssign))});
  fast::Statement* tagged_init =
      big_tagged
          ? mk::Block(t, {mk::ExprStmt(t, mk::Assign(t, mk::Id(t, acc_var),
                                                     mk::BigInt(t, "1")))})
          : mk::Block(
                t, {mk::ExprStmt(
                       t, mk::Assign(
                              t, mk::Id(t, acc_var),
                              mk::Object(t, {mk::Prop(t, "v",
                                                      mk::Id(t, acc_var))})))});
  fast::Statement* tagged_step =
      big_tagged
          ? mk::Block(
                t, {mk::ExprStmt(
                       t, mk::Assign(t, mk::Id(t, acc_var), mk::BigInt(t, "1"),
                                     fast::AssignOp::kAddAssign))})
          : mk::Block(
                t, {mk::ExprStmt(
                       t, mk::Assign(
                              t, mk::Id(t, acc_var),
                              mk::Object(
                                  t, {mk::Prop(t, "v",
                                               mk::Member(t, mk::Id(t, acc_var),
                                                          "v"))})))});

  fast::Statement* tail_if =
      mk::If(t,
             mk::Binary(t, fast::BinaryOp::kStrictEq, mk::Id(t, idx_var),
                        mk::Num(t, static_cast<double>(k_big))),
             tagged_init, tagged_step);
  fast::Statement* mid_if =
      mk::If(t,
             mk::Binary(t, fast::BinaryOp::kLessThan, mk::Id(t, idx_var),
                        mk::Num(t, static_cast<double>(k_big))),
             float_step, tail_if);
  fast::Statement* top_if =
      mk::If(t,
             mk::Binary(t, fast::BinaryOp::kLessThan, mk::Id(t, idx_var),
                        mk::Num(t, static_cast<double>(k_float))),
             smi_step, mid_if);
  loop_body.push_back(top_if);

  std::vector<fast::Statement*> stmts;
  stmts.push_back(mk::Let(t, acc_var, mk::Num(t, 0)));
  stmts.push_back(mk::CountedLoop(t, idx_var,
                                  mk::Num(t, static_cast<double>(bound)),
                                  mk::Block(t, std::move(loop_body))));
  // Observe `acc_var` without throwing on BigInt or Object values.
  stmts.push_back(mk::Let(t, obs_var,
                          mk::Binary(t, fast::BinaryOp::kStrictEq,
                                     mk::Id(t, acc_var), mk::Null(t))));
  if (!target.is_constructor) {
    stmts.push_back(mk::If(t, mk::Id(t, obs_var),
                           mk::Block(t, {mk::Return(t, mk::Id(t, acc_var))})));
  }

  size_t insert_idx = BodyInsertIndex(body, target.in_derived_ctor);
  InsertInBlock(b, body, insert_idx, std::move(stmts));

  if (synthesized && target.can_call_in_focus) {
    Builder::Slot after = b.SlotOf(target.enclosing_stmt);
    if (after.valid()) {
      after.index++;
      std::vector<fast::Statement*> calls;
      if (natives) {
        calls.push_back(
            mk::ExprStmt(t, mk::Native(t, "PrepareFunctionForOptimization",
                                       {mk::Id(t, target.call_name)})));
      }
      calls.push_back(TryCallStmt(
          t, mk::Call(t, mk::Id(t, target.call_name), {mk::Num(t, 1)})));
      b.InsertAll(after, calls);
    }
  }

  *detail = "JitCompound:osr_phi";
  return true;
}

}  // namespace

JitCompoundMutator::JitCompoundMutator(bool natives)
    : Mutator("JitCompound", "applies multi-condition compiler-state patterns",
              0.0,
              /*needs_partner=*/false),
      natives_(natives) {}

bool JitCompoundMutator::Mutate(Builder& builder, MutationContext& ctx) {
  CandidateFn existing;
  const bool has_existing = PickExistingCandidate(builder, &existing);

  JitCompoundPattern pattern;
  if (forced_pattern_.has_value()) {
    pattern = *forced_pattern_;
  } else {
    double w_b = (has_existing && !existing.can_call_in_focus)
                     ? 0.0
                     : kPolyWarmupWeight();
    std::vector<double> weights = {
        kLocalAllocWeight(),
        w_b,
        kInlinedDeoptWeight(),
        kOsrPhiWeight(),
    };
    pattern = static_cast<JitCompoundPattern>(builder.rng().Weighted(weights));
  }

  // Pattern B and Pattern C handle target synthesis on demand when needed.
  if (pattern == JitCompoundPattern::kPolymorphicWarmup) {
    return ApplyPolymorphicWarmup(builder, has_existing ? &existing : nullptr,
                                  natives_, &ctx.detail);
  }
  if (pattern == JitCompoundPattern::kInlinedDeopt) {
    return ApplyInlinedDeopt(builder, has_existing ? &existing : nullptr,
                             natives_, &ctx.detail);
  }

  CandidateFn target = existing;
  bool synthesized = false;
  if (!has_existing) {
    Builder::Slot slot = builder.RandomInsertionSlot();
    std::string_view p0 = builder.FreshName("p");
    std::string_view p1 = builder.FreshName("p");
    if (!SynthesizeHelper(builder, slot, {p0, p1}, {}, &target)) {
      return false;
    }
    synthesized = true;
  }

  if (pattern == JitCompoundPattern::kLocalAllocation) {
    return ApplyLocalAllocation(builder, target, natives_, synthesized,
                                &ctx.detail);
  }
  return ApplyOsrLoopTransition(builder, target, natives_, synthesized,
                                &ctx.detail);
}

}  // namespace redline
