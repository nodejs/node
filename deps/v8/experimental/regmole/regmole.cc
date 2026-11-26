// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Clang AST plugin for static register clobber and control-flow analysis in
// V8's Assembler, MacroAssembler, BaselineAssembler, Maglev, Liftoff, and
// codegen callers across x64 and arm64.

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "clang/AST/APValue.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Attr.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/Expr.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/GlobalDecl.h"
#include "clang/AST/Mangle.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/Stmt.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/FileManager.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/raw_ostream.h"

namespace {

bool g_verify_annotations = true;
bool g_check_uses = true;
bool g_check_headers = false;
bool g_check_maglev = true;
std::string g_export_summaries_path;
std::string g_import_summaries_path;

using RegMask = uint32_t;
constexpr int kMaxGprs = 32;

enum class ArchKind {
  kAuto,
  kX64,
  kArm64,
};

ArchKind g_arch_override = ArchKind::kAuto;

struct ArchInfo {
  ArchKind kind = ArchKind::kX64;
  int num_gprs = 16;
  int num_fp_regs = 16;
  const char* gpr_names[kMaxGprs] = {};
  const char* fp_names[kMaxGprs] = {};
  RegMask pinned_mask = 0;
  RegMask c_callee_saved_mask = 0;
  RegMask caller_saved_mask = 0;
  RegMask implicit_macro_scratch_mask = 0;
  RegMask arm64_default_tmp_mask = 0;
  RegMask maglev_allocatable_mask = 0;
  int maglev_gpr_scratch_budget = 1;
  int maglev_fp_scratch_budget = 1;
  int maglev_single_gpr_scratch = -1;
  int ret_reg0 = 0;
  int ret_reg1 = 2;
  int baseline_scratch_pool_size = 0;
  int baseline_scratch_regs[8] = {};
  RegMask baseline_scratch_mask = 0;
};

ArchInfo MakeX64ArchInfo() {
  ArchInfo a;
  a.kind = ArchKind::kX64;
  a.num_gprs = 16;
  a.num_fp_regs = 16;
  static constexpr const char* kNames[16] = {
      "rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi",
      "r8",  "r9",  "r10", "r11", "r12", "r13", "r14", "r15"};
  static constexpr const char* kFpNames[16] = {
      "xmm0", "xmm1", "xmm2",  "xmm3",  "xmm4",  "xmm5",  "xmm6",  "xmm7",
      "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15"};
  for (int i = 0; i < 16; ++i) {
    a.gpr_names[i] = kNames[i];
    a.fp_names[i] = kFpNames[i];
  }
  // rsp (4), rbp (5), r13=root (13), r14=cage_base (14).
  a.pinned_mask = (1u << 4) | (1u << 5) | (1u << 13) | (1u << 14);
  // rbx (3), r12 (12), r15 (15).
  a.c_callee_saved_mask = (1u << 3) | (1u << 12) | (1u << 15);
  a.caller_saved_mask = 0xFFFFu & ~a.pinned_mask;
  // r10 = kScratchRegister on x64.
  a.implicit_macro_scratch_mask = (1u << 10);
  a.arm64_default_tmp_mask = 0;
  // Allocatable GPRs on x64: all except rsp (4), rbp (5), r10 (10), r13 (13),
  // r14 (14).
  a.maglev_allocatable_mask =
      0xFFFFu & ~(a.pinned_mask | a.implicit_macro_scratch_mask);
  a.maglev_gpr_scratch_budget = 1;
  a.maglev_fp_scratch_budget = 1;
  a.maglev_single_gpr_scratch = 10;  // kScratchRegister (r10)
  a.ret_reg0 = 0;                    // rax
  a.ret_reg1 = 2;                    // rdx
  a.baseline_scratch_pool_size = 5;
  int pool[5] = {8, 9, 11, 12, 15};
  for (int i = 0; i < 5; ++i) {
    a.baseline_scratch_regs[i] = pool[i];
    a.baseline_scratch_mask |= (1u << pool[i]);
  }
  return a;
}

ArchInfo MakeArm64ArchInfo() {
  ArchInfo a;
  a.kind = ArchKind::kArm64;
  a.num_gprs = 32;
  a.num_fp_regs = 32;
  static constexpr const char* kNames[32] = {
      "x0",  "x1",  "x2",  "x3",  "x4",  "x5",  "x6",  "x7",
      "x8",  "x9",  "x10", "x11", "x12", "x13", "x14", "x15",
      "x16", "x17", "x18", "x19", "x20", "x21", "x22", "x23",
      "x24", "x25", "x26", "x27", "x28", "fp",  "lr",  "xzr"};
  static constexpr const char* kFpNames[32] = {
      "d0",  "d1",  "d2",  "d3",  "d4",  "d5",  "d6",  "d7",
      "d8",  "d9",  "d10", "d11", "d12", "d13", "d14", "d15",
      "d16", "d17", "d18", "d19", "d20", "d21", "d22", "d23",
      "d24", "d25", "d26", "d27", "d28", "d29", "d30", "d31"};
  for (int i = 0; i < 32; ++i) {
    a.gpr_names[i] = kNames[i];
    a.fp_names[i] = kFpNames[i];
  }
  // x18 (platform), x26 (root), x27 (cp), x28 (cage_base), x29 (fp), x31 (xzr).
  a.pinned_mask = (1u << 18) | (1u << 26) | (1u << 27) | (1u << 28) |
                  (1u << 29) | (1u << 31);
  // AAPCS64 callee-saved x19..x28 (excluding pinned x26..x28).
  a.c_callee_saved_mask = 0x1FF80000u & ~a.pinned_mask;
  a.caller_saved_mask = 0xFFFFFFFFu & ~a.pinned_mask;
  // ip0 = x16, ip1 = x17.
  a.implicit_macro_scratch_mask = (1u << 16) | (1u << 17);
  a.arm64_default_tmp_mask = (1u << 16) | (1u << 17);
  // Allocatable GPRs in Maglev on ARM64: x0..x15, x19..x25, x27 (cp).
  // Excludes x16 (ip0), x17 (ip1), x18 (platform), x26 (root), x28 (cage_base),
  // x29 (fp), x30 (lr), x31 (xzr).
  a.maglev_allocatable_mask = 0x0BF8FFFFu;
  a.maglev_gpr_scratch_budget = 2;
  a.maglev_fp_scratch_budget = 2;
  a.maglev_single_gpr_scratch = -1;
  a.ret_reg0 = 0;  // x0
  a.ret_reg1 = 1;  // x1
  // BaselineAssembler::ScratchRegisterScope includes x14, x15, x19 + ip0, ip1.
  a.baseline_scratch_pool_size = 5;
  int pool[5] = {14, 15, 16, 17, 19};
  for (int i = 0; i < 5; ++i) {
    a.baseline_scratch_regs[i] = pool[i];
    a.baseline_scratch_mask |= (1u << pool[i]);
  }
  return a;
}

inline llvm::StringRef SafeDeclName(const clang::NamedDecl* d) {
  return (d && d->getIdentifier()) ? d->getName() : llvm::StringRef();
}

ArchInfo DetectArchFromAST(clang::ASTContext& ctx, ArchKind override_kind) {
  if (override_kind == ArchKind::kX64) return MakeX64ArchInfo();
  if (override_kind == ArchKind::kArm64) return MakeArm64ArchInfo();

  const clang::TranslationUnitDecl* tu = ctx.getTranslationUnitDecl();
  for (const clang::Decl* d : tu->decls()) {
    const auto* ns_v8 = llvm::dyn_cast<clang::NamespaceDecl>(d);
    if (!ns_v8 || SafeDeclName(ns_v8) != "v8") continue;
    for (const clang::Decl* d2 : ns_v8->decls()) {
      const auto* ns_int = llvm::dyn_cast<clang::NamespaceDecl>(d2);
      if (!ns_int || SafeDeclName(ns_int) != "internal") continue;
      for (const clang::Decl* d3 : ns_int->decls()) {
        if (const auto* rec = llvm::dyn_cast<clang::CXXRecordDecl>(d3)) {
          if (SafeDeclName(rec) == "CPURegister") {
            return MakeArm64ArchInfo();
          }
        }
      }
    }
  }
  return MakeX64ArchInfo();
}

enum class ParamRole {
  kNone,
  kIn,
  kRdOnly,
  kOut,
  kInOut,
  kScratch,
  kInClobber,
  kBind,
  kBranch,
};

bool IsInputLikeRole(ParamRole role) {
  return role == ParamRole::kIn || role == ParamRole::kRdOnly;
}

const char* ParamRoleToString(ParamRole role) {
  switch (role) {
    case ParamRole::kNone:
      return "none";
    case ParamRole::kIn:
      return "V8_REG_IN";
    case ParamRole::kRdOnly:
      return "V8_REG_RDONLY";
    case ParamRole::kOut:
      return "V8_REG_OUT";
    case ParamRole::kInOut:
      return "V8_REG_INOUT";
    case ParamRole::kScratch:
      return "V8_REG_SCRATCH";
    case ParamRole::kInClobber:
      return "V8_REG_IN_CLOBBER";
    case ParamRole::kBind:
      return "V8_ASM_BIND";
    case ParamRole::kBranch:
      return "V8_ASM_BRANCH";
  }
  return "unknown";
}

const clang::ValueDecl* CanonValueDecl(const clang::ValueDecl* d) {
  return d ? llvm::cast<clang::ValueDecl>(d->getCanonicalDecl()) : nullptr;
}

std::string FormatRegMask(RegMask mask, const ArchInfo& arch) {
  std::string out;
  for (int i = 0; i < arch.num_gprs; ++i) {
    if (mask & (1u << i)) {
      if (!out.empty()) out += ", ";
      out += arch.gpr_names[i];
    }
  }
  return out.empty() ? "none" : out;
}

bool IsTypeNamed(clang::QualType qt, llvm::StringRef target_name) {
  if (qt.isNull()) return false;
  qt = qt.getNonReferenceType().getUnqualifiedType();
  if (const auto* td = qt->getAs<clang::TypedefType>()) {
    if (SafeDeclName(td->getDecl()) == target_name) return true;
  }
  if (const auto* tag = qt->getAsTagDecl()) {
    if (SafeDeclName(tag) == target_name) return true;
  }
  return false;
}

bool IsDoubleRegType(clang::QualType qt) {
  return IsTypeNamed(qt, "DoubleRegister") || IsTypeNamed(qt, "XMMRegister") ||
         IsTypeNamed(qt, "VRegister") || IsTypeNamed(qt, "FloatRegister");
}

bool IsNonGprRegType(clang::QualType qt) {
  return IsDoubleRegType(qt) || IsTypeNamed(qt, "Simd128Register") ||
         IsTypeNamed(qt, "ZRegister") || IsTypeNamed(qt, "PRegister") ||
         IsTypeNamed(qt, "CRegister") || IsTypeNamed(qt, "YMMRegister");
}

bool IsCoreGprRegType(clang::QualType qt) {
  if (IsNonGprRegType(qt)) return false;
  return IsTypeNamed(qt, "Register") || IsTypeNamed(qt, "TaggedRegister") ||
         IsTypeNamed(qt, "CPURegister");
}

bool IsTrackableRegType(clang::QualType qt) {
  return IsCoreGprRegType(qt) || IsTypeNamed(qt, "LiftoffRegister");
}

bool IsMaglevTrackableRegType(clang::QualType qt) {
  return IsTrackableRegType(qt) || IsDoubleRegType(qt);
}

bool IsMaglevInputType(clang::QualType qt) {
  return IsTypeNamed(qt, "Input") || IsTypeNamed(qt, "ConstInput") ||
         IsTypeNamed(qt, "ValueLocation") || IsTypeNamed(qt, "InputLocation");
}

bool IsSaveRegisterStateForCallType(clang::QualType qt) {
  return IsTypeNamed(qt, "SaveRegisterStateForCall");
}

bool IsOperandType(clang::QualType qt) {
  return IsTypeNamed(qt, "Operand") || IsTypeNamed(qt, "MemOperand");
}

bool IsLabelPtrType(clang::QualType qt) {
  if (qt.isNull() || !qt->isPointerType()) return false;
  clang::QualType pointee = qt->getPointeeType();
  return IsTypeNamed(pointee, "Label");
}

enum class ScratchScopeKind {
  kNone,
  kBaseline,
  kArm64UseScratch,
  kMaglevTemp,
};

ScratchScopeKind GetScratchScopeKind(clang::QualType qt) {
  if (IsTypeNamed(qt, "ScratchRegisterScope")) {
    return ScratchScopeKind::kBaseline;
  }
  if (IsTypeNamed(qt, "UseScratchRegisterScope")) {
    return ScratchScopeKind::kArm64UseScratch;
  }
  if (IsTypeNamed(qt, "TemporaryRegisterScope")) {
    return ScratchScopeKind::kMaglevTemp;
  }
  return ScratchScopeKind::kNone;
}

// Unwraps implicit casts, parens, MaterializeTemporaryExpr,
// CXXBindTemporaryExpr, __builtin_expect, and GPR size/type wrappers
// (.reg(), .gp(), .W(), .X(), .Reg()).
const clang::Expr* UnwrapExpr(const clang::Expr* expr) {
  while (expr) {
    expr = expr->IgnoreParenImpCasts();
    if (const auto* bind = llvm::dyn_cast<clang::CXXBindTemporaryExpr>(expr)) {
      expr = bind->getSubExpr();
      continue;
    }
    if (const auto* mat =
            llvm::dyn_cast<clang::MaterializeTemporaryExpr>(expr)) {
      expr = mat->getSubExpr();
      continue;
    }
    if (const auto* clean = llvm::dyn_cast<clang::ExprWithCleanups>(expr)) {
      expr = clean->getSubExpr();
      continue;
    }
    if (const auto* ce = llvm::dyn_cast<clang::CallExpr>(expr)) {
      if (const clang::FunctionDecl* fd = ce->getDirectCallee()) {
        if (fd->getIdentifier() && fd->getName() == "__builtin_expect" &&
            ce->getNumArgs() >= 1) {
          expr = ce->getArg(0);
          continue;
        }
      }
    }
    if (const auto* construct = llvm::dyn_cast<clang::CXXConstructExpr>(expr)) {
      if (construct->getConstructor()->isCopyOrMoveConstructor() &&
          construct->getNumArgs() >= 1) {
        expr = construct->getArg(0);
        continue;
      }
      if (construct->getNumArgs() == 1 &&
          IsTrackableRegType(construct->getType())) {
        const clang::Expr* arg0 = construct->getArg(0)->IgnoreParenImpCasts();
        if (arg0 && IsTrackableRegType(arg0->getType())) {
          expr = arg0;
          continue;
        }
      }
    }
    if (const auto* mem_call = llvm::dyn_cast<clang::CXXMemberCallExpr>(expr)) {
      if (const auto* method = mem_call->getMethodDecl()) {
        llvm::StringRef mname = SafeDeclName(method);
        if (mem_call->getNumArgs() == 0 &&
            (mname == "reg" || mname == "gp" || mname == "W" || mname == "X" ||
             mname == "Reg") &&
            IsCoreGprRegType(mem_call->getType())) {
          const clang::Expr* obj = mem_call->getImplicitObjectArgument();
          if (obj && IsTrackableRegType(obj->getType())) {
            expr = obj;
            continue;
          }
        }
      }
    }
    break;
  }
  return expr;
}

enum class RecordKind {
  kOther,
  kAssembler,
  kMacroAssembler,
  kBaselineAssembler,
};

bool HasBaseOrSelfNamed(const clang::CXXRecordDecl* rec,
                        llvm::StringRef target) {
  if (!rec) return false;
  if (SafeDeclName(rec) == target) return true;
  if (!rec->hasDefinition()) return false;
  for (const clang::CXXBaseSpecifier& base : rec->bases()) {
    if (const auto* base_rec = base.getType()->getAsCXXRecordDecl()) {
      if (HasBaseOrSelfNamed(base_rec, target)) return true;
    }
  }
  return false;
}

RecordKind GetRecordKind(const clang::CXXRecordDecl* rec) {
  if (!rec) return RecordKind::kOther;
  if (HasBaseOrSelfNamed(rec, "BaselineAssembler")) {
    return RecordKind::kBaselineAssembler;
  }
  // Check MacroAssembler before Assembler because MacroAssembler inherits from
  // Assembler.
  if (HasBaseOrSelfNamed(rec, "MacroAssembler") ||
      HasBaseOrSelfNamed(rec, "MacroAssemblerBase") ||
      HasBaseOrSelfNamed(rec, "SharedMacroAssemblerBase") ||
      HasBaseOrSelfNamed(rec, "SharedTurboAssembler")) {
    return RecordKind::kMacroAssembler;
  }
  if (HasBaseOrSelfNamed(rec, "Assembler") ||
      HasBaseOrSelfNamed(rec, "AssemblerBase")) {
    return RecordKind::kAssembler;
  }
  return RecordKind::kOther;
}

// Only check unannotated methods directly on MacroAssembler / BaselineAssembler
// (not derived compilers like MaglevAssembler / LiftoffAssembler unless
// explicitly annotated).
bool ShouldVerifyUnannotatedRecord(const clang::CXXRecordDecl* rec) {
  if (!rec) return false;
  llvm::StringRef name = SafeDeclName(rec);
  return name == "MacroAssembler" || name == "MacroAssemblerBase" ||
         name == "SharedMacroAssemblerBase" || name == "SharedMacroAssembler" ||
         name == "SharedTurboAssembler" || name == "BaselineAssembler";
}

bool HasAssemblerOrRegSignature(const clang::FunctionDecl* fn) {
  if (!fn) return false;
  clang::QualType ret = fn->getReturnType();
  if (IsOperandType(ret) || IsCoreGprRegType(ret)) return true;
  for (unsigned i = 0; i < fn->getNumParams(); ++i) {
    clang::QualType pt = fn->getParamDecl(i)->getType();
    if (IsTrackableRegType(pt) || IsOperandType(pt)) return true;
    clang::QualType deref =
        pt->isPointerType() ? pt->getPointeeType() : pt.getNonReferenceType();
    if (const auto* rec = deref->getAsCXXRecordDecl()) {
      if (GetRecordKind(rec) != RecordKind::kOther) return true;
    }
  }
  return false;
}

// Extracts an integer field recursively from an APValue representing
// RegisterBase.
std::optional<int64_t> ExtractIntFromAPValue(const clang::APValue& val) {
  if (val.isInt()) {
    return val.getInt().getSExtValue();
  }
  if (val.isStruct()) {
    for (unsigned i = 0; i < val.getStructNumBases(); ++i) {
      if (auto res = ExtractIntFromAPValue(val.getStructBase(i))) return res;
    }
    for (unsigned i = 0; i < val.getStructNumFields(); ++i) {
      if (auto res = ExtractIntFromAPValue(val.getStructField(i))) return res;
    }
  }
  return std::nullopt;
}

std::optional<int> ExtractGprCodeFromAPValue(const clang::APValue& val,
                                             const ArchInfo& arch) {
  if (arch.kind == ArchKind::kArm64 && val.isStruct()) {
    // On ARM64, CPURegister has 1 base (RegisterBase) and 2 fields:
    // field 0 = reg_size_, field 1 = reg_type_ (0=kRegister, 1=kVRegister,
    // 2=kZRegister, 3=kNoRegister).
    if (val.getStructNumBases() == 1 && val.getStructNumFields() == 2 &&
        val.getStructField(1).isInt()) {
      int64_t reg_type = val.getStructField(1).getInt().getSExtValue();
      if (reg_type == 3) return -1;            // kNoRegister (NoReg / no_reg)
      if (reg_type != 0) return std::nullopt;  // Reject VRegister / ZRegister
      auto code = ExtractIntFromAPValue(val.getStructBase(0));
      if (!code) return std::nullopt;
      if (*code == 63) return std::nullopt;  // kSPRegInternalCode (sp / wsp)
      if (*code >= -1 && *code < arch.num_gprs) {
        return static_cast<int>(*code);
      }
      return std::nullopt;
    }
    if (val.getStructNumBases() == 1 && val.getStructNumFields() == 0) {
      return ExtractGprCodeFromAPValue(val.getStructBase(0), arch);
    }
  }
  if (auto code = ExtractIntFromAPValue(val)) {
    if (*code >= -1 && *code < arch.num_gprs) {
      return static_cast<int>(*code);
    }
  }
  return std::nullopt;
}

// Evaluates a constexpr Register expression (e.g. `rax`, `x16`, `ip0`,
// `kScratchRegister`, `no_reg`) to its register code (-1 for no_reg,
// 0..num_gprs-1 for physical GPRs).
std::optional<int> EvaluateRegisterCode(const clang::Expr* expr,
                                        clang::ASTContext& ast_ctx,
                                        const ArchInfo& arch) {
  if (!expr || expr->isValueDependent()) return std::nullopt;
  const clang::Expr* unwrapped = UnwrapExpr(expr);
  if (!unwrapped || !IsCoreGprRegType(unwrapped->getType())) {
    return std::nullopt;
  }
  // Evaluate the original expression if it is a core GPR (e.g. `x0.W()`), or
  // the unwrapped expression if `expr` was a wrapper.
  const clang::Expr* eval_target =
      IsCoreGprRegType(expr->getType()) ? expr : unwrapped;
  clang::Expr::EvalResult eval_result;
  if (eval_target->EvaluateAsRValue(eval_result, ast_ctx)) {
    return ExtractGprCodeFromAPValue(eval_result.Val, arch);
  }
  return std::nullopt;
}

std::optional<int> ExtractFpCodeFromAPValue(const clang::APValue& val,
                                            const ArchInfo& arch) {
  if (arch.kind == ArchKind::kArm64 && val.isStruct()) {
    if (val.getStructNumBases() == 1 && val.getStructNumFields() == 2 &&
        val.getStructField(1).isInt()) {
      int64_t reg_type = val.getStructField(1).getInt().getSExtValue();
      if (reg_type == 3) return -1;            // kNoRegister
      if (reg_type != 1) return std::nullopt;  // Require kVRegister
      auto code = ExtractIntFromAPValue(val.getStructBase(0));
      if (!code) return std::nullopt;
      if (*code >= -1 && *code < arch.num_fp_regs) {
        return static_cast<int>(*code);
      }
      return std::nullopt;
    }
    if (val.getStructNumBases() == 1 && val.getStructNumFields() == 0) {
      return ExtractFpCodeFromAPValue(val.getStructBase(0), arch);
    }
  }
  if (auto code = ExtractIntFromAPValue(val)) {
    if (*code >= -1 && *code < arch.num_fp_regs) {
      return static_cast<int>(*code);
    }
  }
  return std::nullopt;
}

std::optional<int> EvaluateDoubleRegisterCode(const clang::Expr* expr,
                                              clang::ASTContext& ast_ctx,
                                              const ArchInfo& arch) {
  if (!expr || expr->isValueDependent()) return std::nullopt;
  const clang::Expr* unwrapped = UnwrapExpr(expr);
  if (!unwrapped || !IsDoubleRegType(unwrapped->getType())) {
    return std::nullopt;
  }
  const clang::Expr* eval_target =
      IsDoubleRegType(expr->getType()) ? expr : unwrapped;
  clang::Expr::EvalResult eval_result;
  if (eval_target->EvaluateAsRValue(eval_result, ast_ctx)) {
    return ExtractFpCodeFromAPValue(eval_result.Val, arch);
  }
  return std::nullopt;
}

enum class CondCmpOp { kNe = 0, kEq = 1, kLt = 2, kLe = 3, kGt = 4, kGe = 5 };

struct ConditionalClobber {
  unsigned param_index = 0;
  CondCmpOp op = CondCmpOp::kNe;
  int64_t rhs = 0;
  RegMask mask = 0;

  bool operator==(const ConditionalClobber& o) const {
    return param_index == o.param_index && op == o.op && rhs == o.rhs &&
           mask == o.mask;
  }
};

inline bool EvalCondCmpOp(CondCmpOp op, int64_t lhs, int64_t rhs) {
  switch (op) {
    case CondCmpOp::kNe:
      return lhs != rhs;
    case CondCmpOp::kEq:
      return lhs == rhs;
    case CondCmpOp::kLt:
      return lhs < rhs;
    case CondCmpOp::kLe:
      return lhs <= rhs;
    case CondCmpOp::kGt:
      return lhs > rhs;
    case CondCmpOp::kGe:
      return lhs >= rhs;
  }
  return true;
}

inline bool ParseConditionalClobberString(llvm::StringRef raw,
                                          std::string& out_param_name,
                                          CondCmpOp& out_op, int64_t& out_rhs) {
  llvm::StringRef s = raw.trim();
  while (s.size() >= 2 && s.front() == '(' && s.back() == ')') {
    s = s.drop_front().drop_back().trim();
  }
  if (s.empty()) return false;

  if (s.front() == '!') {
    llvm::StringRef ident = s.drop_front().trim();
    if (ident.empty()) return false;
    for (char c : ident) {
      if (!llvm::isAlnum(c) && c != '_') return false;
    }
    out_param_name = ident.str();
    out_op = CondCmpOp::kEq;
    out_rhs = 0;
    return true;
  }

  struct OpTok {
    const char* tok;
    CondCmpOp op;
  };
  static constexpr OpTok kOps[] = {
      {"!=", CondCmpOp::kNe}, {"==", CondCmpOp::kEq}, {"<=", CondCmpOp::kLe},
      {">=", CondCmpOp::kGe}, {"<", CondCmpOp::kLt},  {">", CondCmpOp::kGt},
  };
  for (const auto& entry : kOps) {
    size_t pos = s.find(entry.tok);
    if (pos == llvm::StringRef::npos) continue;
    llvm::StringRef lhs = s.substr(0, pos).trim();
    llvm::StringRef rhs_s = s.substr(pos + std::strlen(entry.tok)).trim();
    if (lhs.empty() || rhs_s.empty()) return false;
    for (char c : lhs) {
      if (!llvm::isAlnum(c) && c != '_') return false;
    }
    std::string rhs_str = rhs_s.str();
    // Strip integer suffixes like `u`, `U`, `l`, `L`:
    while (!rhs_str.empty() &&
           (rhs_str.back() == 'u' || rhs_str.back() == 'U' ||
            rhs_str.back() == 'l' || rhs_str.back() == 'L')) {
      rhs_str.pop_back();
    }
    if (rhs_str.empty()) return false;
    char* endp = nullptr;
    long long val = std::strtoll(rhs_str.c_str(), &endp, 0);
    if (!endp || *endp != '\0') return false;
    out_param_name = lhs.str();
    out_op = entry.op;
    out_rhs = static_cast<int64_t>(val);
    return true;
  }

  for (char c : s) {
    if (!llvm::isAlnum(c) && c != '_') return false;
  }
  out_param_name = s.str();
  out_op = CondCmpOp::kNe;
  out_rhs = 0;
  return true;
}

struct FuncSummary {
  std::vector<ParamRole> param_roles;
  RegMask implicit_in = 0;
  RegMask implicit_out = 0;
  RegMask clobbers = 0;
  std::vector<ConditionalClobber> conditional_clobbers;
  std::string invalid_cond_clobber_error;
  bool clobbers_caller_saved = false;
  bool no_fallthrough = false;
  bool zeroing_if_same = false;
  bool is_push = false;
  bool is_pop = false;
  bool has_explicit_annotations = false;
  int baseline_scratch_used = 0;
  int must_baseline_scratch_used = 0;
  int arm64_scratch_used = 0;
  int must_arm64_scratch_used = 0;

  // Maglev TemporaryRegisterScope & call property summaries:
  // `*_peak` is the maximum simultaneous acquisitions needed while inside the
  // function (including inside its own nested TemporaryRegisterScopes);
  // `*_leaked` is the net acquisitions still held in the caller's active scope
  // when the function returns (e.g. MapCompare ctor or detail::ToRegister).
  int maglev_gpr_temps_peak = 0;
  int maglev_gpr_temps_leaked = 0;
  int maglev_double_temps_peak = 0;
  int maglev_double_temps_leaked = 0;
  int maglev_gpr_scratches_peak = 0;
  int maglev_gpr_scratches_leaked = 0;
  int maglev_double_scratches_peak = 0;
  int maglev_double_scratches_leaked = 0;
  bool maglev_uses_save_reg_state = false;
  bool maglev_calls_outside_save_scope = false;

  // Scratch-backed and input-backed Operand/MemOperand return summaries:
  RegMask returned_operand_scratch_phys_mask = 0;
  uint32_t returned_operand_scratch_param_mask = 0;
  uint32_t returned_operand_input_param_mask = 0;
  RegMask returned_operand_input_phys_mask = 0;
  uint32_t preserved_across_call_param_mask = 0;

  // Early-clobber pairs (out_param_idx, in_param_idx): writing out_param_idx
  // happens before reading in_param_idx on at least one path without an
  // aliasing-safe guard.
  std::set<std::pair<unsigned, unsigned>> early_clobber_param_pairs;
  // Physical registers overwritten before reading param `in_param_idx`.
  std::vector<RegMask> early_clobber_phys_before_param;
};

// ----------------------------------------------------------------------------
// Maglev Register Allocator Constraint Model
// ----------------------------------------------------------------------------

enum class MaglevInputKind {
  kNone = 0,
  kUseRegister = 1,
  kUseAndClobberRegister = 2,
  kUseAny = 3,
  kUseFixed = 4,
  kUseAndClobberFixed = 5,
};

const char* MaglevInputKindToString(MaglevInputKind kind) {
  switch (kind) {
    case MaglevInputKind::kNone:
      return "none";
    case MaglevInputKind::kUseRegister:
      return "UseRegister";
    case MaglevInputKind::kUseAndClobberRegister:
      return "UseAndClobberRegister";
    case MaglevInputKind::kUseAny:
      return "UseAny";
    case MaglevInputKind::kUseFixed:
      return "UseFixed";
    case MaglevInputKind::kUseAndClobberFixed:
      return "UseAndClobberFixed";
  }
  return "unknown";
}

enum class MaglevResultKind {
  kNone = 0,
  kDefineAsRegister = 1,
  kDefineSameAsInput = 2,
  kDefineAsFixed = 3,
  kDefineAsConstant = 4,
  kCustomUnallocated = 5,
};

struct MaglevCondAtom {
  std::string expr;
  bool truth = true;

  bool operator==(const MaglevCondAtom& o) const {
    return expr == o.expr && truth == o.truth;
  }
  bool operator<(const MaglevCondAtom& o) const {
    if (expr != o.expr) return expr < o.expr;
    return truth < o.truth;
  }
};

struct MaglevInputConstraint {
  MaglevInputKind kind = MaglevInputKind::kNone;
  int fixed_reg = -1;
  bool is_double_fixed = false;
  bool only_constant_or_reg = false;
};

struct MaglevResultConstraint {
  MaglevResultKind kind = MaglevResultKind::kNone;
  int fixed_reg = -1;
  bool is_double_fixed = false;
  int same_as_input_index = -1;
};

struct MaglevConstraintPath {
  std::vector<MaglevCondAtom> conds;
  std::map<std::string, MaglevInputConstraint> inputs;
  MaglevResultConstraint result;
  int gpr_temps = 0;
  int double_temps = 0;
  bool gpr_temps_unknown = false;
  bool double_temps_unknown = false;
  RegMask fixed_gpr_temps = 0;
  uint32_t fixed_double_temps = 0;
  bool has_dynamic_fixed_gpr = false;
};

struct MaglevNodeSpec {
  std::string cls_name;
  bool has_constraints_body = false;
  bool is_value_node = false;
  bool is_terminal_control = false;
  bool is_call = false;
  bool is_deferred_call = false;
  bool can_eager_deopt = false;
  bool can_lazy_deopt = false;
  bool can_allocate = false;
  bool result_is_double = false;
  std::vector<int> input_reprs;
  std::map<std::string, int> input_key_indices;
  std::vector<MaglevConstraintPath> paths;
};

// Parses normalized comparison atoms of the form `<lhs> == <int>`, `<lhs> >
// <int>`, or `<lhs> >= <int>` for interval-based contradiction detection.
struct ParsedIntCmp {
  std::string lhs;
  enum Op { kEq, kGt, kGe } op = kEq;
  int64_t rhs = 0;
};

std::optional<ParsedIntCmp> ParseIntCmpAtom(const std::string& expr) {
  auto try_op = [&](const char* op_str,
                    ParsedIntCmp::Op op) -> std::optional<ParsedIntCmp> {
    size_t pos = expr.find(op_str);
    if (pos == std::string::npos) return std::nullopt;
    std::string lhs = expr.substr(0, pos);
    std::string rhs_str = expr.substr(pos + std::strlen(op_str));
    if (lhs.empty() || rhs_str.empty()) return std::nullopt;
    // Ensure rhs_str is a pure signed integer literal.
    size_t idx = 0;
    if (rhs_str[0] == '-' && rhs_str.size() > 1) idx = 1;
    for (; idx < rhs_str.size(); ++idx) {
      if (rhs_str[idx] < '0' || rhs_str[idx] > '9') return std::nullopt;
    }
    char* endp = nullptr;
    int64_t val = std::strtoll(rhs_str.c_str(), &endp, 10);
    if (!endp || *endp != '\0') return std::nullopt;
    return ParsedIntCmp{std::move(lhs), op, val};
  };
  if (auto r = try_op(" == ", ParsedIntCmp::kEq)) return r;
  if (auto r = try_op(" >= ", ParsedIntCmp::kGe)) return r;
  if (auto r = try_op(" > ", ParsedIntCmp::kGt)) return r;
  return std::nullopt;
}

bool CondAtomListsCompatible(const std::vector<MaglevCondAtom>& a,
                             const std::vector<MaglevCondAtom>& b) {
  // 1. Direct syntactic contradiction check.
  for (const auto& ca : a) {
    for (const auto& cb : b) {
      if (ca.expr == cb.expr && ca.truth != cb.truth) {
        return false;
      }
    }
  }
  // 2. Integer comparison interval contradiction check across both lists.
  struct Interval {
    int64_t lo = std::numeric_limits<int64_t>::min();
    int64_t hi = std::numeric_limits<int64_t>::max();
    std::set<int64_t> excluded;
  };
  std::map<std::string, Interval> intervals;
  auto apply_atom = [&](const MaglevCondAtom& atom) -> bool {
    auto cmp = ParseIntCmpAtom(atom.expr);
    if (!cmp) return true;
    Interval& iv = intervals[cmp->lhs];
    if (cmp->op == ParsedIntCmp::kEq) {
      if (atom.truth) {
        iv.lo = std::max(iv.lo, cmp->rhs);
        iv.hi = std::min(iv.hi, cmp->rhs);
      } else {
        iv.excluded.insert(cmp->rhs);
      }
    } else if (cmp->op == ParsedIntCmp::kGt) {
      if (atom.truth) {
        if (cmp->rhs < std::numeric_limits<int64_t>::max()) {
          iv.lo = std::max(iv.lo, cmp->rhs + 1);
        }
      } else {
        iv.hi = std::min(iv.hi, cmp->rhs);
      }
    } else if (cmp->op == ParsedIntCmp::kGe) {
      if (atom.truth) {
        iv.lo = std::max(iv.lo, cmp->rhs);
      } else {
        if (cmp->rhs > std::numeric_limits<int64_t>::min()) {
          iv.hi = std::min(iv.hi, cmp->rhs - 1);
        }
      }
    }
    if (iv.lo > iv.hi) return false;
    if (iv.lo == iv.hi && iv.excluded.count(iv.lo)) return false;
    return true;
  };
  for (const auto& ca : a) {
    if (!apply_atom(ca)) return false;
  }
  for (const auto& cb : b) {
    if (!apply_atom(cb)) return false;
  }
  return true;
}

int ResolveInputKeyIndex(const MaglevNodeSpec& spec, const std::string& key) {
  if (key.empty()) return -1;
  if (key.rfind("idx:", 0) == 0) {
    char* endp = nullptr;
    long v = std::strtol(key.c_str() + 4, &endp, 10);
    if (endp && *endp == '\0' && v >= 0) return static_cast<int>(v);
    return -1;
  }
  auto it = spec.input_key_indices.find(key);
  if (it != spec.input_key_indices.end()) return it->second;
  return -1;
}

bool MatchesSameAsInput(const MaglevNodeSpec& spec, const std::string& key,
                        const MaglevConstraintPath& path) {
  if (path.result.kind != MaglevResultKind::kDefineSameAsInput ||
      path.result.same_as_input_index < 0) {
    return false;
  }
  return ResolveInputKeyIndex(spec, key) == path.result.same_as_input_index;
}

const MaglevInputConstraint* LookupInputConstraint(
    const MaglevConstraintPath& path, const std::string& key) {
  auto it = path.inputs.find(key);
  if (it != path.inputs.end()) return &it->second;
  // Fall back to variadic loop-constrained inputs (`for (Input in : inputs())`,
  // `for (... i ...) UseAny(arg(i))`, etc.).
  auto var_all = path.inputs.find("var:*");
  if (var_all != path.inputs.end()) return &var_all->second;
  if (key.rfind("idx:", 0) == 0) {
    auto var_in = path.inputs.find("var:input");
    if (var_in != path.inputs.end()) return &var_in->second;
  }
  return nullptr;
}

const MaglevInputConstraint* LookupSameAsInputConstraint(
    const MaglevNodeSpec& spec, const MaglevConstraintPath& path) {
  if (path.result.kind != MaglevResultKind::kDefineSameAsInput ||
      path.result.same_as_input_index < 0) {
    return nullptr;
  }
  int target = path.result.same_as_input_index;
  std::string idx_key = "idx:" + std::to_string(target);
  auto it = path.inputs.find(idx_key);
  if (it != path.inputs.end()) return &it->second;
  for (const auto& [k, ic] : path.inputs) {
    if (ResolveInputKeyIndex(spec, k) == target) return &ic;
  }
  return LookupInputConstraint(path, idx_key);
}

// Collects all declarations to inspect for attributes (redecls + template
// pattern redecls).
std::vector<const clang::FunctionDecl*> GetAllDeclForms(
    const clang::FunctionDecl* fn) {
  std::vector<const clang::FunctionDecl*> forms;
  llvm::SmallPtrSet<const clang::FunctionDecl*, 8> seen;
  auto add_redecls = [&](const clang::FunctionDecl* d) {
    if (!d) return;
    for (const clang::FunctionDecl* r : d->redecls()) {
      if (seen.insert(r).second) {
        forms.push_back(r);
      }
    }
  };
  add_redecls(fn);
  if (const clang::FunctionDecl* pattern =
          fn->getTemplateInstantiationPattern()) {
    add_redecls(pattern);
  }
  if (const clang::FunctionTemplateDecl* ftd = fn->getPrimaryTemplate()) {
    add_redecls(ftd->getTemplatedDecl());
  }
  return forms;
}

FuncSummary ExtractExplicitSummary(const clang::FunctionDecl* fn,
                                   clang::ASTContext& ast_ctx,
                                   const ArchInfo& arch) {
  FuncSummary sum;
  unsigned num_params = fn->getNumParams();
  sum.param_roles.assign(num_params, ParamRole::kNone);
  sum.early_clobber_phys_before_param.assign(num_params, 0);

  for (unsigned i = 0; i < num_params; ++i) {
    clang::QualType pt = fn->getParamDecl(i)->getType();
    if (IsTrackableRegType(pt) || IsOperandType(pt)) {
      sum.param_roles[i] = ParamRole::kIn;
    }
  }

  std::vector<const clang::FunctionDecl*> forms = GetAllDeclForms(fn);
  for (const clang::FunctionDecl* form : forms) {
    for (const auto* attr : form->specific_attrs<clang::AnnotateAttr>()) {
      llvm::StringRef tag = attr->getAnnotation();
      if (tag == "v8_reg_implicit_in" || tag == "v8_reg_implicit_out" ||
          tag == "v8_reg_implicit_inout" || tag == "v8_reg_clobbers") {
        sum.has_explicit_annotations = true;
        RegMask mask = 0;
        for (const clang::Expr* arg : attr->args()) {
          if (auto code = EvaluateRegisterCode(arg, ast_ctx, arch)) {
            if (*code >= 0 && *code < arch.num_gprs) {
              mask |= (1u << *code);
            }
          }
        }
        if (tag == "v8_reg_implicit_in") {
          sum.implicit_in |= mask;
        } else if (tag == "v8_reg_implicit_out") {
          sum.implicit_out |= mask;
        } else if (tag == "v8_reg_implicit_inout") {
          sum.implicit_in |= mask;
          sum.implicit_out |= mask;
        } else if (tag == "v8_reg_clobbers") {
          sum.clobbers |= mask;
        }
      } else if (tag == "v8_reg_clobbers_if") {
        sum.has_explicit_annotations = true;
        RegMask mask = 0;
        llvm::StringRef cond_text;
        bool has_cond = false;
        unsigned arg_idx = 0;
        for (const clang::Expr* arg : attr->args()) {
          if (arg_idx == 0) {
            if (const auto* sl = llvm::dyn_cast_or_null<clang::StringLiteral>(
                    arg->IgnoreParenImpCasts())) {
              cond_text = sl->getString();
              has_cond = true;
            }
          } else if (auto code = EvaluateRegisterCode(arg, ast_ctx, arch)) {
            if (*code >= 0 && *code < arch.num_gprs) {
              mask |= (1u << *code);
            }
          }
          ++arg_idx;
        }
        std::string pname;
        CondCmpOp op = CondCmpOp::kNe;
        int64_t rhs = 0;
        int matched_pidx = -1;
        if (has_cond &&
            ParseConditionalClobberString(cond_text, pname, op, rhs)) {
          auto match_in_decl = [&](const clang::FunctionDecl* d) -> int {
            for (unsigned pi = 0; pi < d->getNumParams(); ++pi) {
              const clang::ParmVarDecl* pd = d->getParamDecl(pi);
              if (pd->getIdentifier() && pd->getName() == pname) {
                return static_cast<int>(pi);
              }
            }
            return -1;
          };
          matched_pidx = match_in_decl(form);
          if (matched_pidx < 0) {
            for (const clang::FunctionDecl* pform : forms) {
              matched_pidx = match_in_decl(pform);
              if (matched_pidx >= 0) break;
            }
          }
        }
        if (matched_pidx >= 0 && mask != 0) {
          ConditionalClobber cc{static_cast<unsigned>(matched_pidx), op, rhs,
                                mask};
          if (std::find(sum.conditional_clobbers.begin(),
                        sum.conditional_clobbers.end(),
                        cc) == sum.conditional_clobbers.end()) {
            sum.conditional_clobbers.push_back(cc);
          }
        } else {
          sum.clobbers |= mask;
          if (sum.invalid_cond_clobber_error.empty()) {
            sum.invalid_cond_clobber_error =
                "Invalid V8_REG_CLOBBERS_IF condition '" + cond_text.str() +
                "' on '" + fn->getQualifiedNameAsString() + "'.";
          }
        }
      } else if (tag == "v8_reg_clobbers_caller_saved") {
        sum.has_explicit_annotations = true;
        sum.clobbers_caller_saved = true;
        sum.clobbers |= arch.caller_saved_mask;
      } else if (tag == "v8_asm_no_fallthrough") {
        sum.has_explicit_annotations = true;
        sum.no_fallthrough = true;
      } else if (tag == "v8_asm_zeroing_if_same") {
        sum.has_explicit_annotations = true;
        sum.zeroing_if_same = true;
      } else if (tag == "v8_asm_push") {
        sum.has_explicit_annotations = true;
        sum.is_push = true;
      } else if (tag == "v8_asm_pop") {
        sum.has_explicit_annotations = true;
        sum.is_pop = true;
      }
    }

    unsigned form_params = std::min(num_params, form->getNumParams());
    for (unsigned i = 0; i < form_params; ++i) {
      const clang::ParmVarDecl* p = form->getParamDecl(i);
      for (const auto* attr : p->specific_attrs<clang::AnnotateAttr>()) {
        llvm::StringRef tag = attr->getAnnotation();
        if (tag == "v8_reg_in") {
          sum.param_roles[i] = ParamRole::kIn;
          sum.has_explicit_annotations = true;
        } else if (tag == "v8_reg_rdonly") {
          sum.param_roles[i] = ParamRole::kRdOnly;
          sum.has_explicit_annotations = true;
        } else if (tag == "v8_reg_out") {
          sum.param_roles[i] = ParamRole::kOut;
          sum.has_explicit_annotations = true;
        } else if (tag == "v8_reg_inout") {
          sum.param_roles[i] = ParamRole::kInOut;
          sum.has_explicit_annotations = true;
        } else if (tag == "v8_reg_scratch") {
          sum.param_roles[i] = ParamRole::kScratch;
          sum.has_explicit_annotations = true;
        } else if (tag == "v8_reg_in_clobber") {
          sum.param_roles[i] = ParamRole::kInClobber;
          sum.has_explicit_annotations = true;
        } else if (tag == "v8_asm_bind") {
          sum.param_roles[i] = ParamRole::kBind;
          sum.has_explicit_annotations = true;
        } else if (tag == "v8_asm_branch") {
          sum.param_roles[i] = ParamRole::kBranch;
          sum.has_explicit_annotations = true;
        }
      }
    }
  }

  // Any function returning an Operand/MemOperand that takes a V8_REG_OUT or
  // V8_REG_SCRATCH register parameter (e.g. ExternalReferenceAsOperand,
  // MoveImmediateForShiftedOp) ties the returned Operand's validity to that
  // scratch/destination register's version.
  if (IsOperandType(fn->getReturnType())) {
    sum.returned_operand_scratch_phys_mask |=
        (sum.clobbers & arch.implicit_macro_scratch_mask);
    for (unsigned i = 0; i < num_params && i < 32; ++i) {
      if (sum.param_roles[i] == ParamRole::kOut ||
          sum.param_roles[i] == ParamRole::kScratch) {
        sum.returned_operand_scratch_param_mask |= (1u << i);
      }
    }
  }

  return sum;
}

// ----------------------------------------------------------------------------
// Dataflow Lattice & Environment
// ----------------------------------------------------------------------------

enum class RegStatus {
  kInitial,    // Holds untouched entry value.
  kAlive,      // Holds valid value written during this function.
  kClobbered,  // Value was destroyed by a clobber or scratch operation.
};

enum class ReadonlyKind {
  kNone,
  kExplicitAttr,  // V8_REG_RDONLY parameter
  kMaglevInput,   // Maglev UseRegister / UseFixed input without UseAndClobber*
};

enum class MaglevRole {
  kNone = 0,
  kInput,    // Bound from ToRegister/ToDoubleRegister/ToRegisterT(Input)
  kResult,   // Bound from ToRegister/ToDoubleRegister(result())
  kTemp,     // Acquired from TemporaryRegisterScope::Acquire/AcquireDouble
  kScratch,  // Acquired from TemporaryRegisterScope::AcquireScratch*
};

struct ClobberNote {
  clang::SourceLocation loc;
  std::string message;
};

enum class Arm64RegWidth : uint8_t {
  kUnknown = 0,
  kX =
      1,  // Guaranteed 64-bit X register (e.g. ToRegister(), AcquireX(), .X()).
  kW = 2,  // Guaranteed 32-bit W register (e.g. .W(), AcquireW()).
};

struct SymRegState {
  RegStatus status = RegStatus::kInitial;
  const clang::ValueDecl* root_decl = nullptr;
  int phys_reg = -1;          // 0..num_gprs-1 if known physical reg, else -1.
  int param_index = -1;       // >= 0 if bound to entry parameter i.
  int scratch_slot = -1;      // >= 0 if acquired from a scratch scope.
  int scratch_scope_id = -1;  // Active scope ID when acquired.
  bool holds_entry_value = false;
  uint64_t def_version = 0;
  ReadonlyKind readonly_kind = ReadonlyKind::kNone;
  std::string readonly_reason;
  MaglevRole maglev_role = MaglevRole::kNone;
  std::string maglev_input_key;
  std::string maglev_input_label;
  bool is_double_reg = false;
  Arm64RegWidth arm64_width = Arm64RegWidth::kUnknown;
  // For kResult registers: maps Maglev input key -> condition atoms under which
  // this register is proven distinct from that input (empty = unconditionally).
  std::map<std::string, std::vector<MaglevCondAtom>> known_distinct_from_inputs;
  ClobberNote clobber;
};

struct PhysRegState {
  RegStatus status = RegStatus::kInitial;
  bool holds_entry_value = true;
  uint64_t def_version = 0;
  ClobberNote clobber;
};

struct OperandDep {
  llvm::SmallVector<const clang::ValueDecl*, 2> sym_decls;
  RegMask phys_mask = 0;
  // Scratch registers (physical or symbolic) populated when the Operand was
  // created, along with their exact def_version at creation time.
  llvm::SmallVector<std::pair<int, uint64_t>, 2> scratch_phys_versions;
  llvm::SmallVector<std::pair<const clang::ValueDecl*, uint64_t>, 2>
      scratch_sym_versions;
  clang::SourceLocation creation_loc;
};

struct SavedStackEntry {
  enum Kind { kSingleReg, kCallerSaved, kAllRegs } kind = kSingleReg;
  const clang::ValueDecl* root_decl = nullptr;
  SymRegState saved_sym;
  int phys_reg = -1;
  PhysRegState saved_phys;
  PhysRegState saved_phys_all[kMaxGprs];
  std::map<const clang::ValueDecl*, SymRegState> saved_syms_all;
  bool saved_clobbered_caller_saved = false;
};

struct ScratchScopeFrame {
  int base_depth = 0;
  ScratchScopeKind kind = ScratchScopeKind::kNone;
  RegMask saved_arm64_avail = 0;
  RegMask saved_arm64_included = 0;
  int saved_arm64_caller_held = 0;
  int saved_maglev_gpr_temps = 0;
  int saved_maglev_double_temps = 0;
  int saved_maglev_gpr_scratches = 0;
  int saved_maglev_double_scratches = 0;
  int saved_maglev_gpr_included = 0;
  int saved_maglev_double_included = 0;
  bool saved_maglev_gpr_unbounded = false;
  bool saved_maglev_double_unbounded = false;
  bool saved_maglev_temps_reset = false;
  bool is_maglev_save_call = false;
};

struct Env {
  bool reachable = true;
  PhysRegState phys[kMaxGprs];
  std::map<const clang::ValueDecl*, SymRegState> syms;
  std::map<const clang::ValueDecl*, OperandDep> operands;
  std::map<const clang::ValueDecl*, std::vector<const clang::ValueDecl*>>
      reg_arrays;
  std::vector<SavedStackEntry> asm_stack;

  // Parameters/regs known to be `no_reg` (or immediate Operand) on this branch.
  llvm::SmallPtrSet<const clang::ValueDecl*, 4> known_no_reg;
  // Pairs of symbolic declarations known to be equal or non-aliasing on branch.
  std::set<std::pair<const clang::ValueDecl*, const clang::ValueDecl*>>
      known_equal;
  std::set<std::pair<const clang::ValueDecl*, const clang::ValueDecl*>>
      known_not_equal;
  // On ARM64, pairs guarded only by size-sensitive `!=` / `!(==)` where
  // `wN != xN` evaluates to true despite aliasing physical register N.
  std::map<std::pair<const clang::ValueDecl*, const clang::ValueDecl*>,
           clang::SourceLocation>
      size_sensitive_not_equal;

  // Maglev path-sensitive & register-allocator tracking:
  std::vector<MaglevCondAtom> active_conds;
  std::map<const clang::ValueDecl*, const clang::Expr*> local_expr_bindings;
  std::map<const clang::ValueDecl*, std::pair<std::string, std::string>>
      local_maglev_inputs;
  std::set<std::string> known_is_gpr_inputs;
  std::set<std::string> known_is_fp_inputs;
  std::set<std::string> known_not_gpr_inputs;
  std::set<std::string> known_not_fp_inputs;
  std::set<std::string> known_not_constant_inputs;
  bool maglev_result_written = false;
  clang::SourceLocation maglev_result_write_loc;
  const clang::ValueDecl* maglev_result_sym = nullptr;
  bool maglev_result_is_double = false;
  std::map<std::string, std::vector<MaglevCondAtom>>
      maglev_result_distinct_inputs;
  llvm::SmallPtrSet<const clang::ValueDecl*, 4> maglev_snapshot_cleared_syms;
  std::map<std::string, clang::SourceLocation> maglev_written_inputs;
  int maglev_save_call_depth = 0;
  int maglev_gpr_temps_used = 0;
  int maglev_double_temps_used = 0;
  int maglev_gpr_scratches_used = 0;
  int maglev_double_scratches_used = 0;
  int maglev_gpr_temps_included = 0;
  int maglev_double_temps_included = 0;
  bool maglev_gpr_temps_unbounded = false;
  bool maglev_double_temps_unbounded = false;
  bool maglev_temps_reset = false;

  // Scratch scope tracking:
  int active_scratch_depth = 0;
  RegMask arm64_avail_scratch_mask = 0;
  RegMask arm64_included_scratch_mask = 0;
  int arm64_caller_scratches_held = 0;
  int path_max_baseline_scratch = 0;
  int path_max_arm64_scratch = 0;
  std::vector<ScratchScopeFrame> scratch_scope_stack;

  // Summary inference accumulators along this path:
  uint32_t params_read_mask = 0;
  uint32_t params_written_mask = 0;
  RegMask phys_written_mask = 0;
  bool clobbered_caller_saved = false;
};

bool EnvEquivalentForFixpoint(const Env& a, const Env& b, int num_gprs) {
  if (a.reachable != b.reachable) return false;
  if (!a.reachable) return true;
  for (int r = 0; r < num_gprs; ++r) {
    if (a.phys[r].status != b.phys[r].status ||
        a.phys[r].holds_entry_value != b.phys[r].holds_entry_value) {
      return false;
    }
  }
  if (a.syms.size() != b.syms.size()) return false;
  for (const auto& [decl, sa] : a.syms) {
    auto it = b.syms.find(decl);
    if (it == b.syms.end()) return false;
    const SymRegState& sb = it->second;
    if (sa.status != sb.status || sa.phys_reg != sb.phys_reg ||
        sa.holds_entry_value != sb.holds_entry_value ||
        sa.scratch_slot != sb.scratch_slot) {
      return false;
    }
  }
  if (a.known_no_reg.size() != b.known_no_reg.size()) return false;
  for (const auto* d : a.known_no_reg) {
    if (!b.known_no_reg.contains(d)) return false;
  }
  if (a.operands.size() != b.operands.size()) return false;
  auto op_scratch_valid = [num_gprs](const OperandDep& dep, const Env& e) {
    for (const auto& [r, ver] : dep.scratch_phys_versions) {
      if (r >= 0 && r < num_gprs && e.phys[r].def_version != ver) return false;
    }
    for (const auto& [sd, ver] : dep.scratch_sym_versions) {
      auto it = e.syms.find(sd);
      if (it == e.syms.end() || it->second.def_version != ver) return false;
    }
    return true;
  };
  for (const auto& [op_d, op_a] : a.operands) {
    auto it_b = b.operands.find(op_d);
    if (it_b == b.operands.end()) return false;
    if (op_scratch_valid(op_a, a) != op_scratch_valid(it_b->second, b)) {
      return false;
    }
  }
  if (a.maglev_result_written != b.maglev_result_written ||
      a.maglev_gpr_temps_used != b.maglev_gpr_temps_used ||
      a.maglev_double_temps_used != b.maglev_double_temps_used ||
      a.maglev_gpr_scratches_used != b.maglev_gpr_scratches_used ||
      a.maglev_double_scratches_used != b.maglev_double_scratches_used) {
    return false;
  }
  return true;
}

RegStatus MergeStatus(RegStatus a, RegStatus b, ClobberNote* out_note,
                      const ClobberNote& note_a, const ClobberNote& note_b) {
  if (a == RegStatus::kClobbered || b == RegStatus::kClobbered) {
    if (out_note) {
      *out_note = (a == RegStatus::kClobbered) ? note_a : note_b;
    }
    return RegStatus::kClobbered;
  }
  if (a == RegStatus::kAlive || b == RegStatus::kAlive) {
    return RegStatus::kAlive;
  }
  return RegStatus::kInitial;
}

Env MergeEnvs(const Env& a, const Env& b, int num_gprs) {
  if (!a.reachable) return b;
  if (!b.reachable) return a;

  Env out = a;
  out.path_max_baseline_scratch =
      std::min(a.path_max_baseline_scratch, b.path_max_baseline_scratch);
  out.path_max_arm64_scratch =
      std::min(a.path_max_arm64_scratch, b.path_max_arm64_scratch);
  for (int r = 0; r < num_gprs; ++r) {
    ClobberNote note;
    out.phys[r].status = MergeStatus(a.phys[r].status, b.phys[r].status, &note,
                                     a.phys[r].clobber, b.phys[r].clobber);
    out.phys[r].clobber = note;
    out.phys[r].holds_entry_value =
        a.phys[r].holds_entry_value && b.phys[r].holds_entry_value;
    out.phys[r].def_version =
        std::max(a.phys[r].def_version, b.phys[r].def_version);
  }

  for (const auto& [decl, b_sym] : b.syms) {
    auto it = out.syms.find(decl);
    if (it == out.syms.end()) {
      out.syms[decl] = b_sym;
    } else {
      SymRegState& a_sym = it->second;
      ClobberNote note;
      bool a_noreg = a.known_no_reg.contains(decl);
      bool b_noreg = b.known_no_reg.contains(decl);
      if (a_noreg && !b_noreg) {
        a_sym = b_sym;
      } else if (!b_noreg) {
        RegStatus b_eff_status = b_sym.status;
        if (b_eff_status == RegStatus::kInitial) {
          for (const auto& eq_pair : b.known_equal) {
            if (eq_pair.first == decl || eq_pair.second == decl) {
              b_eff_status = RegStatus::kAlive;
            }
          }
        }
        RegStatus a_eff_status = a_sym.status;
        if (a_eff_status == RegStatus::kInitial) {
          for (const auto& eq_pair : a.known_equal) {
            if (eq_pair.first == decl || eq_pair.second == decl) {
              a_eff_status = RegStatus::kAlive;
            }
          }
        }
        a_sym.status = MergeStatus(a_eff_status, b_eff_status, &note,
                                   a_sym.clobber, b_sym.clobber);
        a_sym.clobber = note;
        a_sym.holds_entry_value =
            a_sym.holds_entry_value && b_sym.holds_entry_value;
        a_sym.def_version = std::max(a_sym.def_version, b_sym.def_version);
        if (a_sym.phys_reg != b_sym.phys_reg) {
          a_sym.phys_reg = -1;
        }
        if (a_sym.root_decl != b_sym.root_decl) {
          a_sym.root_decl = decl;
        }
        if (a_sym.readonly_kind == ReadonlyKind::kNone &&
            b_sym.readonly_kind != ReadonlyKind::kNone) {
          a_sym.readonly_kind = b_sym.readonly_kind;
          a_sym.readonly_reason = b_sym.readonly_reason;
        }
        if (a_sym.maglev_role == MaglevRole::kResult &&
            b_sym.maglev_role == MaglevRole::kResult) {
          std::map<std::string, std::vector<MaglevCondAtom>> inter;
          for (const auto& [k, conds_a] : a_sym.known_distinct_from_inputs) {
            auto bit = b_sym.known_distinct_from_inputs.find(k);
            if (bit != b_sym.known_distinct_from_inputs.end()) {
              if (conds_a == bit->second) {
                inter[k] = conds_a;
              } else if (conds_a.empty()) {
                inter[k] = bit->second;
              } else if (bit->second.empty()) {
                inter[k] = conds_a;
              }
            }
          }
          a_sym.known_distinct_from_inputs = std::move(inter);
        } else if (b_sym.maglev_role == MaglevRole::kResult &&
                   a_sym.maglev_role != MaglevRole::kResult) {
          // Only paths from `b` write result(); keep `b`'s distinctness
          // provenance.
          a_sym.maglev_role = MaglevRole::kResult;
          a_sym.is_double_reg = b_sym.is_double_reg;
          a_sym.known_distinct_from_inputs = b_sym.known_distinct_from_inputs;
        } else if (a_sym.maglev_role == MaglevRole::kNone &&
                   b_sym.maglev_role != MaglevRole::kNone) {
          a_sym.maglev_role = b_sym.maglev_role;
          a_sym.maglev_input_key = b_sym.maglev_input_key;
          a_sym.maglev_input_label = b_sym.maglev_input_label;
          a_sym.is_double_reg = b_sym.is_double_reg;
        }
        a_sym.arm64_width = (a_sym.arm64_width == b_sym.arm64_width)
                                ? a_sym.arm64_width
                                : Arm64RegWidth::kUnknown;
      }
    }
  }

  for (const auto& [op_decl, b_dep] : b.operands) {
    auto it = out.operands.find(op_decl);
    if (it == out.operands.end()) {
      out.operands[op_decl] = b_dep;
    } else {
      OperandDep& a_dep = it->second;
      a_dep.phys_mask |= b_dep.phys_mask;
      for (const auto* sd : b_dep.sym_decls) {
        if (std::find(a_dep.sym_decls.begin(), a_dep.sym_decls.end(), sd) ==
            a_dep.sym_decls.end()) {
          a_dep.sym_decls.push_back(sd);
        }
      }
      for (const auto& pv : b_dep.scratch_phys_versions) {
        bool found = false;
        for (auto& apv : a_dep.scratch_phys_versions) {
          if (apv.first == pv.first) {
            apv.second = std::min(apv.second, pv.second);
            found = true;
            break;
          }
        }
        if (!found) a_dep.scratch_phys_versions.push_back(pv);
      }
      for (const auto& sv : b_dep.scratch_sym_versions) {
        bool found = false;
        for (auto& asv : a_dep.scratch_sym_versions) {
          if (asv.first == sv.first) {
            asv.second = std::min(asv.second, sv.second);
            found = true;
            break;
          }
        }
        if (!found) a_dep.scratch_sym_versions.push_back(sv);
      }
    }
  }

  out.known_no_reg.clear();
  for (const auto* d : a.known_no_reg) {
    if (b.known_no_reg.contains(d)) out.known_no_reg.insert(d);
  }
  out.known_equal.clear();
  out.known_not_equal.clear();
  out.size_sensitive_not_equal.clear();

  // Merge Maglev path-sensitive state:
  out.active_conds.clear();
  for (const auto& ca : a.active_conds) {
    if (std::find(b.active_conds.begin(), b.active_conds.end(), ca) !=
        b.active_conds.end()) {
      out.active_conds.push_back(ca);
    }
  }
  out.local_expr_bindings.clear();
  for (const auto& [k, v] : a.local_expr_bindings) {
    auto it = b.local_expr_bindings.find(k);
    if (it != b.local_expr_bindings.end() && it->second == v) {
      out.local_expr_bindings[k] = v;
    }
  }
  out.local_maglev_inputs.clear();
  for (const auto& [k, v] : a.local_maglev_inputs) {
    auto it = b.local_maglev_inputs.find(k);
    if (it != b.local_maglev_inputs.end() && it->second == v) {
      out.local_maglev_inputs[k] = v;
    }
  }
  auto intersect_str_set = [](const std::set<std::string>& sa,
                              const std::set<std::string>& sb) {
    std::set<std::string> res;
    for (const auto& k : sa) {
      if (sb.count(k)) res.insert(k);
    }
    return res;
  };
  out.known_is_gpr_inputs =
      intersect_str_set(a.known_is_gpr_inputs, b.known_is_gpr_inputs);
  out.known_is_fp_inputs =
      intersect_str_set(a.known_is_fp_inputs, b.known_is_fp_inputs);
  out.known_not_gpr_inputs =
      intersect_str_set(a.known_not_gpr_inputs, b.known_not_gpr_inputs);
  out.known_not_fp_inputs =
      intersect_str_set(a.known_not_fp_inputs, b.known_not_fp_inputs);
  out.known_not_constant_inputs = intersect_str_set(
      a.known_not_constant_inputs, b.known_not_constant_inputs);
  out.maglev_result_written =
      a.maglev_result_written || b.maglev_result_written;
  out.maglev_result_write_loc = a.maglev_result_written
                                    ? a.maglev_result_write_loc
                                    : b.maglev_result_write_loc;
  out.maglev_result_sym =
      a.maglev_result_sym ? a.maglev_result_sym : b.maglev_result_sym;
  out.maglev_result_is_double =
      a.maglev_result_is_double || b.maglev_result_is_double;
  out.maglev_result_distinct_inputs.clear();
  if (a.maglev_result_written && b.maglev_result_written) {
    for (const auto& [k, conds_a] : a.maglev_result_distinct_inputs) {
      auto bit = b.maglev_result_distinct_inputs.find(k);
      if (bit != b.maglev_result_distinct_inputs.end()) {
        if (conds_a == bit->second) {
          out.maglev_result_distinct_inputs[k] = conds_a;
        } else if (conds_a.empty()) {
          out.maglev_result_distinct_inputs[k] = bit->second;
        } else if (bit->second.empty()) {
          out.maglev_result_distinct_inputs[k] = conds_a;
        }
      }
    }
  } else if (a.maglev_result_written) {
    out.maglev_result_distinct_inputs = a.maglev_result_distinct_inputs;
  } else if (b.maglev_result_written) {
    out.maglev_result_distinct_inputs = b.maglev_result_distinct_inputs;
  }
  out.maglev_snapshot_cleared_syms = a.maglev_snapshot_cleared_syms;
  for (const auto* d : b.maglev_snapshot_cleared_syms) {
    out.maglev_snapshot_cleared_syms.insert(d);
  }
  out.maglev_written_inputs = a.maglev_written_inputs;
  for (const auto& [k, loc] : b.maglev_written_inputs) {
    out.maglev_written_inputs.try_emplace(k, loc);
  }
  out.maglev_save_call_depth =
      std::min(a.maglev_save_call_depth, b.maglev_save_call_depth);
  out.maglev_gpr_temps_used =
      std::max(a.maglev_gpr_temps_used, b.maglev_gpr_temps_used);
  out.maglev_double_temps_used =
      std::max(a.maglev_double_temps_used, b.maglev_double_temps_used);
  out.maglev_gpr_scratches_used =
      std::max(a.maglev_gpr_scratches_used, b.maglev_gpr_scratches_used);
  out.maglev_double_scratches_used =
      std::max(a.maglev_double_scratches_used, b.maglev_double_scratches_used);
  out.maglev_gpr_temps_included =
      std::min(a.maglev_gpr_temps_included, b.maglev_gpr_temps_included);
  out.maglev_double_temps_included =
      std::min(a.maglev_double_temps_included, b.maglev_double_temps_included);
  out.maglev_gpr_temps_unbounded =
      a.maglev_gpr_temps_unbounded && b.maglev_gpr_temps_unbounded;
  out.maglev_double_temps_unbounded =
      a.maglev_double_temps_unbounded && b.maglev_double_temps_unbounded;
  out.maglev_temps_reset = a.maglev_temps_reset && b.maglev_temps_reset;

  out.params_read_mask = a.params_read_mask | b.params_read_mask;
  out.params_written_mask = a.params_written_mask | b.params_written_mask;
  out.phys_written_mask = a.phys_written_mask | b.phys_written_mask;
  out.clobbered_caller_saved =
      a.clobbered_caller_saved || b.clobbered_caller_saved;
  return out;
}

// ----------------------------------------------------------------------------
// Cross-TU Summary Serialization Helpers
// ----------------------------------------------------------------------------

std::string EscapeToken(const std::string& s) {
  static constexpr char kHex[] = "0123456789ABCDEF";
  std::string out;
  out.reserve(s.size() + 8);
  for (unsigned char c : s) {
    if (c <= 0x20 || c >= 0x7F || c == '%' || c == '|' || c == '=' ||
        c == ',' || c == ';' || c == ':') {
      out.push_back('%');
      out.push_back(kHex[(c >> 4) & 0xF]);
      out.push_back(kHex[c & 0xF]);
    } else {
      out.push_back(static_cast<char>(c));
    }
  }
  return out.empty() ? "%00" : out;
}

std::string UnescapeToken(const std::string& s) {
  if (s == "%00") return "";
  auto hex_val = [](char c) -> int {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
  };
  std::string out;
  out.reserve(s.size());
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '%' && i + 2 < s.size()) {
      int hi = hex_val(s[i + 1]);
      int lo = hex_val(s[i + 2]);
      if (hi >= 0 && lo >= 0) {
        out.push_back(static_cast<char>((hi << 4) | lo));
        i += 2;
        continue;
      }
    }
    out.push_back(s[i]);
  }
  return out;
}

// ----------------------------------------------------------------------------
// Function & Statement Analyzer
// ----------------------------------------------------------------------------

class RegmoleAnalyzer {
 public:
  RegmoleAnalyzer(clang::ASTContext& ast_ctx, clang::DiagnosticsEngine& diag,
                  clang::SourceManager& sm, const ArchInfo& arch)
      : ast_ctx_(ast_ctx), diag_(diag), sm_(sm), arch_(arch) {
    diag_warn_ = diag_.getCustomDiagID(clang::DiagnosticsEngine::Warning, "%0");
    diag_note_ = diag_.getCustomDiagID(clang::DiagnosticsEngine::Note, "%0");
  }

  void EmitImmediateWarning(clang::SourceLocation loc, const std::string& msg,
                            const ClobberNote* note = nullptr) {
    if (!loc.isValid()) return;
    diag_.Report(clang::FullSourceLoc(loc, sm_), diag_warn_) << msg;
    if (note && note->loc.isValid()) {
      diag_.Report(clang::FullSourceLoc(note->loc, sm_), diag_note_)
          << note->message;
    }
  }

  std::string GetFuncKey(const clang::FunctionDecl* fn) {
    if (!fn) return "";
    const clang::FunctionDecl* canon = fn->getCanonicalDecl();
    if (!canon || canon->isDependentContext() ||
        llvm::isa<clang::CXXDeductionGuideDecl>(canon)) {
      return "";
    }
    if (!mangle_ctx_) {
      mangle_ctx_.reset(ast_ctx_.createMangleContext());
    }
    if (!mangle_ctx_->shouldMangleDeclName(canon)) {
      return canon->getNameAsString();
    }
    clang::GlobalDecl gd;
    if (const auto* ctor = llvm::dyn_cast<clang::CXXConstructorDecl>(canon)) {
      gd = clang::GlobalDecl(ctor, clang::Ctor_Complete);
    } else if (const auto* dtor =
                   llvm::dyn_cast<clang::CXXDestructorDecl>(canon)) {
      gd = clang::GlobalDecl(dtor, clang::Dtor_Complete);
    } else {
      gd = clang::GlobalDecl(canon);
    }
    std::string out;
    llvm::raw_string_ostream os(out);
    mangle_ctx_->mangleName(gd, os);
    os.flush();
    return out;
  }

  void ImportSummaries(const std::string& path) {
    if (path.empty()) return;
    std::ifstream in(path);
    if (!in.is_open()) return;
    std::string line;
    while (std::getline(in, line)) {
      if (line.empty()) continue;
      std::istringstream iss(line);
      std::string kind;
      iss >> kind;
      if (kind == "FUNC") {
        std::string key_tok;
        if (!(iss >> key_tok)) continue;
        std::string key = UnescapeToken(key_tok);
        FuncSummary sum;
        size_t n_params = 0;
        if (!(iss >> n_params)) continue;
        sum.param_roles.resize(n_params, ParamRole::kNone);
        for (size_t i = 0; i < n_params; ++i) {
          int r = 0;
          iss >> r;
          sum.param_roles[i] = static_cast<ParamRole>(r);
        }
        int ccs = 0, nft = 0, zis = 0, ipu = 0, ipo = 0, hea = 0;
        int mus = 0, mcos = 0;
        iss >> sum.implicit_in >> sum.implicit_out >> sum.clobbers >> ccs >>
            nft >> zis >> ipu >> ipo >> hea >> sum.baseline_scratch_used >>
            sum.must_baseline_scratch_used >> sum.arm64_scratch_used >>
            sum.must_arm64_scratch_used >> sum.maglev_gpr_temps_peak >>
            sum.maglev_gpr_temps_leaked >> sum.maglev_double_temps_peak >>
            sum.maglev_double_temps_leaked >> sum.maglev_gpr_scratches_peak >>
            sum.maglev_gpr_scratches_leaked >>
            sum.maglev_double_scratches_peak >>
            sum.maglev_double_scratches_leaked >> mus >> mcos >>
            sum.returned_operand_scratch_phys_mask >>
            sum.returned_operand_scratch_param_mask >>
            sum.returned_operand_input_param_mask >>
            sum.returned_operand_input_phys_mask >>
            sum.preserved_across_call_param_mask;
        sum.clobbers_caller_saved = (ccs != 0);
        sum.no_fallthrough = (nft != 0);
        sum.zeroing_if_same = (zis != 0);
        sum.is_push = (ipu != 0);
        sum.is_pop = (ipo != 0);
        sum.has_explicit_annotations = (hea != 0);
        sum.maglev_uses_save_reg_state = (mus != 0);
        sum.maglev_calls_outside_save_scope = (mcos != 0);
        size_t n_ec = 0;
        iss >> n_ec;
        for (size_t i = 0; i < n_ec; ++i) {
          unsigned a = 0, b = 0;
          iss >> a >> b;
          sum.early_clobber_param_pairs.insert({a, b});
        }
        size_t n_ep = 0;
        iss >> n_ep;
        sum.early_clobber_phys_before_param.resize(n_ep, 0);
        for (size_t i = 0; i < n_ep; ++i) {
          iss >> sum.early_clobber_phys_before_param[i];
        }
        size_t n_cc = 0;
        if (iss >> n_cc) {
          for (size_t i = 0; i < n_cc; ++i) {
            unsigned pi = 0;
            int op_i = 0;
            int64_t rhs_v = 0;
            RegMask m = 0;
            iss >> pi >> op_i >> rhs_v >> m;
            sum.conditional_clobbers.push_back(
                {pi, static_cast<CondCmpOp>(op_i), rhs_v, m});
          }
        }
        if (iss) {
          imported_summaries_[key] = std::move(sum);
        }
      } else if (kind == "MAGLEV") {
        std::string cls_tok;
        if (!(iss >> cls_tok)) continue;
        MaglevNodeSpec spec;
        spec.cls_name = UnescapeToken(cls_tok);
        spec.has_constraints_body = true;
        int ivn = 0, itc = 0, ic = 0, idc = 0, ced = 0, cld = 0, ca = 0,
            rid = 0;
        size_t n_paths = 0;
        iss >> ivn >> itc >> ic >> idc >> ced >> cld >> ca >> rid >> n_paths;
        spec.is_value_node = (ivn != 0);
        spec.is_terminal_control = (itc != 0);
        spec.is_call = (ic != 0);
        spec.is_deferred_call = (idc != 0);
        spec.can_eager_deopt = (ced != 0);
        spec.can_lazy_deopt = (cld != 0);
        spec.can_allocate = (ca != 0);
        spec.result_is_double = (rid != 0);
        for (size_t pi = 0; pi < n_paths; ++pi) {
          MaglevConstraintPath p;
          size_t n_conds = 0;
          iss >> n_conds;
          for (size_t ci = 0; ci < n_conds; ++ci) {
            std::string etok;
            int truth = 0;
            iss >> etok >> truth;
            p.conds.push_back({UnescapeToken(etok), truth != 0});
          }
          size_t n_ins = 0;
          iss >> n_ins;
          for (size_t ii = 0; ii < n_ins; ++ii) {
            std::string ktok;
            int ik = 0, fr = -1, idf = 0, oco = 0;
            iss >> ktok >> ik >> fr >> idf >> oco;
            p.inputs[UnescapeToken(ktok)] = {static_cast<MaglevInputKind>(ik),
                                             fr, idf != 0, oco != 0};
          }
          int rk = 0, rfr = -1, ridf = 0, rsai = -1;
          int gu = 0, du = 0, hdg = 0;
          iss >> rk >> rfr >> ridf >> rsai >> p.gpr_temps >> p.double_temps >>
              gu >> du >> p.fixed_gpr_temps >> p.fixed_double_temps >> hdg;
          p.result = {static_cast<MaglevResultKind>(rk), rfr, ridf != 0, rsai};
          p.gpr_temps_unknown = (gu != 0);
          p.double_temps_unknown = (du != 0);
          p.has_dynamic_fixed_gpr = (hdg != 0);
          spec.paths.push_back(std::move(p));
        }
        size_t n_reprs = 0;
        if (iss >> n_reprs) {
          spec.input_reprs.resize(n_reprs, -1);
          for (size_t ri = 0; ri < n_reprs; ++ri) {
            iss >> spec.input_reprs[ri];
          }
          size_t n_kidx = 0;
          if (iss >> n_kidx) {
            for (size_t ki = 0; ki < n_kidx; ++ki) {
              std::string ktok;
              int idx = -1;
              iss >> ktok >> idx;
              spec.input_key_indices[UnescapeToken(ktok)] = idx;
            }
          }
        }
        if (iss) {
          imported_maglev_specs_[spec.cls_name] = std::move(spec);
        }
      }
    }
  }

  void ExportSummaries(const std::string& path) {
    if (path.empty()) return;
    std::ofstream out(path, std::ios::out | std::ios::trunc);
    if (!out.is_open()) return;
    for (const auto& [canon_fn, sum] : summary_cache_) {
      if (!canon_fn->hasBody() && !sum.has_explicit_annotations) continue;
      std::string key = GetFuncKey(canon_fn);
      if (key.empty()) continue;
      out << "FUNC " << EscapeToken(key) << " " << sum.param_roles.size();
      for (ParamRole r : sum.param_roles) {
        out << " " << static_cast<int>(r);
      }
      out << " " << sum.implicit_in << " " << sum.implicit_out << " "
          << sum.clobbers << " " << (sum.clobbers_caller_saved ? 1 : 0) << " "
          << (sum.no_fallthrough ? 1 : 0) << " "
          << (sum.zeroing_if_same ? 1 : 0) << " " << (sum.is_push ? 1 : 0)
          << " " << (sum.is_pop ? 1 : 0) << " "
          << (sum.has_explicit_annotations ? 1 : 0) << " "
          << sum.baseline_scratch_used << " " << sum.must_baseline_scratch_used
          << " " << sum.arm64_scratch_used << " " << sum.must_arm64_scratch_used
          << " " << sum.maglev_gpr_temps_peak << " "
          << sum.maglev_gpr_temps_leaked << " " << sum.maglev_double_temps_peak
          << " " << sum.maglev_double_temps_leaked << " "
          << sum.maglev_gpr_scratches_peak << " "
          << sum.maglev_gpr_scratches_leaked << " "
          << sum.maglev_double_scratches_peak << " "
          << sum.maglev_double_scratches_leaked << " "
          << (sum.maglev_uses_save_reg_state ? 1 : 0) << " "
          << (sum.maglev_calls_outside_save_scope ? 1 : 0) << " "
          << sum.returned_operand_scratch_phys_mask << " "
          << sum.returned_operand_scratch_param_mask << " "
          << sum.returned_operand_input_param_mask << " "
          << sum.returned_operand_input_phys_mask << " "
          << sum.preserved_across_call_param_mask << " "
          << sum.early_clobber_param_pairs.size();
      for (const auto& [a, b] : sum.early_clobber_param_pairs) {
        out << " " << a << " " << b;
      }
      out << " " << sum.early_clobber_phys_before_param.size();
      for (RegMask m : sum.early_clobber_phys_before_param) {
        out << " " << m;
      }
      out << " " << sum.conditional_clobbers.size();
      for (const ConditionalClobber& cc : sum.conditional_clobbers) {
        out << " " << cc.param_index << " " << static_cast<int>(cc.op) << " "
            << cc.rhs << " " << cc.mask;
      }
      out << "\n";
    }
    for (const auto& [canon_cls, spec] : maglev_spec_cache_) {
      (void)canon_cls;
      if (!spec.has_constraints_body || spec.cls_name.empty()) continue;
      out << "MAGLEV " << EscapeToken(spec.cls_name) << " "
          << (spec.is_value_node ? 1 : 0) << " "
          << (spec.is_terminal_control ? 1 : 0) << " " << (spec.is_call ? 1 : 0)
          << " " << (spec.is_deferred_call ? 1 : 0) << " "
          << (spec.can_eager_deopt ? 1 : 0) << " "
          << (spec.can_lazy_deopt ? 1 : 0) << " " << (spec.can_allocate ? 1 : 0)
          << " " << (spec.result_is_double ? 1 : 0) << " " << spec.paths.size();
      for (const MaglevConstraintPath& p : spec.paths) {
        out << " " << p.conds.size();
        for (const MaglevCondAtom& c : p.conds) {
          out << " " << EscapeToken(c.expr) << " " << (c.truth ? 1 : 0);
        }
        out << " " << p.inputs.size();
        for (const auto& [k, ic] : p.inputs) {
          out << " " << EscapeToken(k) << " " << static_cast<int>(ic.kind)
              << " " << ic.fixed_reg << " " << (ic.is_double_fixed ? 1 : 0)
              << " " << (ic.only_constant_or_reg ? 1 : 0);
        }
        out << " " << static_cast<int>(p.result.kind) << " "
            << p.result.fixed_reg << " " << (p.result.is_double_fixed ? 1 : 0)
            << " " << p.result.same_as_input_index << " " << p.gpr_temps << " "
            << p.double_temps << " " << (p.gpr_temps_unknown ? 1 : 0) << " "
            << (p.double_temps_unknown ? 1 : 0) << " " << p.fixed_gpr_temps
            << " " << p.fixed_double_temps << " "
            << (p.has_dynamic_fixed_gpr ? 1 : 0);
      }
      out << " " << spec.input_reprs.size();
      for (int r : spec.input_reprs) {
        out << " " << r;
      }
      out << " " << spec.input_key_indices.size();
      for (const auto& [k, idx] : spec.input_key_indices) {
        out << " " << EscapeToken(k) << " " << idx;
      }
      out << "\n";
    }
  }

  void PreCollectMaglevConstraints(const clang::FunctionDecl* fn) {
    if (!fn || !fn->doesThisDeclarationHaveABody() ||
        fn->isDependentContext()) {
      return;
    }
    const auto* md = llvm::dyn_cast<clang::CXXMethodDecl>(fn);
    if (!md || SafeDeclName(md) != "SetValueLocationConstraints") return;
    GetMaglevNodeSpec(md->getParent());
  }

  const FuncSummary& GetOrInferSummary(const clang::FunctionDecl* fn) {
    const clang::FunctionDecl* canon = fn->getCanonicalDecl();
    auto it = summary_cache_.find(canon);
    if (it != summary_cache_.end()) {
      return it->second;
    }

    FuncSummary explicit_sum = ExtractExplicitSummary(fn, ast_ctx_, arch_);
    summary_cache_[canon] = explicit_sum;

    const clang::CXXMethodDecl* method =
        llvm::dyn_cast<clang::CXXMethodDecl>(fn);
    RecordKind rec_kind =
        method ? GetRecordKind(method->getParent()) : RecordKind::kOther;

    const clang::FunctionDecl* body_decl = nullptr;
    if (fn->hasBody(body_decl) && active_inferences_.size() < 8) {
      if (rec_kind == RecordKind::kAssembler &&
          !explicit_sum.has_explicit_annotations) {
        if (active_inferences_.insert(canon).second) {
          FuncSummary inferred =
              InferAssemblerForwarder(body_decl, explicit_sum);
          active_inferences_.erase(canon);
          summary_cache_[canon] = inferred;
        }
      } else if (rec_kind != RecordKind::kAssembler) {
        bool should_infer =
            (rec_kind != RecordKind::kOther) ||
            explicit_sum.has_explicit_annotations ||
            HasAssemblerOrRegSignature(fn) ||
            (sm_.getFileID(body_decl->getLocation()) == sm_.getMainFileID());
        if (should_infer && active_inferences_.insert(canon).second) {
          FuncSummary inferred = AnalyzeFunctionInternal(
              body_decl, explicit_sum, /*emit_diagnostics=*/false);
          active_inferences_.erase(canon);
          MergeInferredIntoSummary(summary_cache_[canon], explicit_sum,
                                   inferred);
        }
      }
    } else if (!fn->hasBody(body_decl) && !imported_summaries_.empty()) {
      std::string key = GetFuncKey(canon);
      auto imp_it = imported_summaries_.find(key);
      if (imp_it != imported_summaries_.end()) {
        FuncSummary merged = explicit_sum;
        MergeInferredIntoSummary(merged, explicit_sum, imp_it->second);
        // Ensure param vectors match this declaration's param count.
        if (merged.param_roles.size() != fn->getNumParams()) {
          merged.param_roles.resize(fn->getNumParams(), ParamRole::kNone);
        }
        if (merged.early_clobber_phys_before_param.size() !=
            fn->getNumParams()) {
          merged.early_clobber_phys_before_param.resize(fn->getNumParams(), 0);
        }
        summary_cache_[canon] = std::move(merged);
      }
    }

    return summary_cache_[canon];
  }

  void AnalyzeTopLevelFunction(const clang::FunctionDecl* fn) {
    if (!fn->doesThisDeclarationHaveABody()) return;
    if (fn->isDependentContext()) return;
    const clang::FunctionDecl* canon = fn->getCanonicalDecl();
    if (analyzed_deferred_lambdas_.count(canon)) return;

    const clang::CXXMethodDecl* method =
        llvm::dyn_cast<clang::CXXMethodDecl>(fn);
    if (method && SafeDeclName(method) == "SetValueLocationConstraints") {
      GetMaglevNodeSpec(method->getParent());
      return;
    }
    RecordKind rec_kind =
        method ? GetRecordKind(method->getParent()) : RecordKind::kOther;

    // Do not analyze low-level Assembler byte-emission bodies.
    if (rec_kind == RecordKind::kAssembler) return;

    FuncSummary explicit_sum = ExtractExplicitSummary(fn, ast_ctx_, arch_);
    active_inferences_.insert(canon);
    FuncSummary inferred = AnalyzeFunctionInternal(
        fn, explicit_sum, /*emit_diagnostics=*/g_check_uses);
    active_inferences_.erase(canon);

    FuncSummary merged = explicit_sum;
    MergeInferredIntoSummary(merged, explicit_sum, inferred);
    summary_cache_[canon] = merged;
  }

 private:
  struct PendingDiag {
    clang::SourceLocation loc;
    std::string msg;
    ClobberNote note;
    bool has_note = false;
  };

  struct CfTarget {
    bool is_loop = false;
    size_t scratch_scope_depth = 0;
    Env break_env;
    Env continue_env;
  };

  struct FunctionAnalysisContext {
    const clang::FunctionDecl* fn = nullptr;
    const clang::CXXRecordDecl* maglev_node_cls = nullptr;
    bool in_maglev_deferred = false;
    bool in_maglev_diverging_deferred = false;
    FuncSummary explicit_sum;
    bool emit_diagnostics = false;
    uint64_t next_version = 1;
    int max_baseline_scratch_used = 0;
    int max_arm64_scratch_used = 0;
    bool used_baseline_scratch_scope = false;
    bool used_arm64_scratch_scope = false;

    // Maglev TemporaryRegisterScope & call summary accumulators:
    int max_maglev_gpr_temps = 0;
    int max_maglev_double_temps = 0;
    int max_maglev_gpr_scratches = 0;
    int max_maglev_double_scratches = 0;
    bool maglev_used_save_reg_state = false;
    bool maglev_called_outside_save_scope = false;
    int inline_lambda_depth = 0;
    size_t base_scratch_scope_depth = 0;

    // Inferred summary tracking across the function:
    std::vector<bool> param_read_entry;
    std::vector<bool> param_written_any;
    std::vector<bool> param_clobbered_any;
    std::vector<bool> param_written_before_read_any;
    bool any_push_param0 = false;
    bool any_pop_param0 = false;
    RegMask phys_read_entry = 0;
    RegMask phys_written_any = 0;
    RegMask phys_clobbered_any = 0;
    std::vector<ConditionalClobber> propagated_conditional_clobbers;
    std::set<std::pair<unsigned, unsigned>> early_clobber_param_pairs;
    std::vector<RegMask> early_clobber_phys_before_param;

    // Returned Operand/MemOperand provenance:
    RegMask returned_operand_scratch_phys_mask = 0;
    uint32_t returned_operand_scratch_param_mask = 0;
    uint32_t returned_operand_input_param_mask = 0;
    RegMask returned_operand_input_phys_mask = 0;
    std::map<const clang::CallExpr*, OperandDep> call_operand_deps;

    // Label states for V8_ASM_BRANCH / V8_ASM_BIND:
    std::map<const clang::ValueDecl*, Env> forward_label_states;
    std::map<const clang::ValueDecl*, Env> backward_label_states;
    const std::map<const clang::ValueDecl*, Env>* carried_backedges = nullptr;
    std::set<const clang::ValueDecl*> bound_labels;

    // C++ switch/loop break & continue targets:
    std::vector<CfTarget> cf_stack;

    // Exit environments (at `return` statements and fallthrough end of body):
    std::vector<Env> exit_envs;

    // Buffered diagnostics for the current fixpoint pass:
    std::vector<PendingDiag> pending_diags;
  };

  void MergeInferredIntoSummary(FuncSummary& dst,
                                const FuncSummary& explicit_sum,
                                const FuncSummary& inferred) {
    if (!explicit_sum.has_explicit_annotations) {
      dst = inferred;
      return;
    }
    dst.early_clobber_param_pairs = inferred.early_clobber_param_pairs;
    dst.early_clobber_phys_before_param =
        inferred.early_clobber_phys_before_param;
    dst.baseline_scratch_used = inferred.baseline_scratch_used;
    dst.must_baseline_scratch_used = inferred.must_baseline_scratch_used;
    dst.arm64_scratch_used = inferred.arm64_scratch_used;
    dst.must_arm64_scratch_used = inferred.must_arm64_scratch_used;
    dst.maglev_gpr_temps_peak =
        std::max(dst.maglev_gpr_temps_peak, inferred.maglev_gpr_temps_peak);
    dst.maglev_gpr_temps_leaked =
        std::max(dst.maglev_gpr_temps_leaked, inferred.maglev_gpr_temps_leaked);
    dst.maglev_double_temps_peak = std::max(dst.maglev_double_temps_peak,
                                            inferred.maglev_double_temps_peak);
    dst.maglev_double_temps_leaked = std::max(
        dst.maglev_double_temps_leaked, inferred.maglev_double_temps_leaked);
    dst.maglev_gpr_scratches_peak = std::max(
        dst.maglev_gpr_scratches_peak, inferred.maglev_gpr_scratches_peak);
    dst.maglev_gpr_scratches_leaked = std::max(
        dst.maglev_gpr_scratches_leaked, inferred.maglev_gpr_scratches_leaked);
    dst.maglev_double_scratches_peak =
        std::max(dst.maglev_double_scratches_peak,
                 inferred.maglev_double_scratches_peak);
    dst.maglev_double_scratches_leaked =
        std::max(dst.maglev_double_scratches_leaked,
                 inferred.maglev_double_scratches_leaked);
    dst.maglev_uses_save_reg_state |= inferred.maglev_uses_save_reg_state;
    dst.maglev_calls_outside_save_scope |=
        inferred.maglev_calls_outside_save_scope;
    dst.preserved_across_call_param_mask |=
        inferred.preserved_across_call_param_mask;
    RegMask cond_mask = 0;
    for (const ConditionalClobber& cc : dst.conditional_clobbers) {
      cond_mask |= cc.mask;
    }
    dst.clobbers |=
        (inferred.clobbers & arch_.implicit_macro_scratch_mask & ~cond_mask);
    for (const ConditionalClobber& cc : inferred.conditional_clobbers) {
      RegMask rem = cc.mask & arch_.implicit_macro_scratch_mask & ~dst.clobbers;
      if (rem == 0) continue;
      ConditionalClobber ncc{cc.param_index, cc.op, cc.rhs, rem};
      if (std::find(dst.conditional_clobbers.begin(),
                    dst.conditional_clobbers.end(),
                    ncc) == dst.conditional_clobbers.end()) {
        dst.conditional_clobbers.push_back(ncc);
      }
    }
    dst.returned_operand_scratch_phys_mask |=
        inferred.returned_operand_scratch_phys_mask;
    dst.returned_operand_scratch_param_mask |=
        inferred.returned_operand_scratch_param_mask;
    dst.returned_operand_input_param_mask |=
        inferred.returned_operand_input_param_mask;
    dst.returned_operand_input_phys_mask |=
        inferred.returned_operand_input_phys_mask;
  }

  void QueueWarning(FunctionAnalysisContext& fctx, clang::SourceLocation loc,
                    const std::string& msg, const ClobberNote* note = nullptr) {
    if (!fctx.emit_diagnostics || !loc.isValid()) return;
    PendingDiag pd;
    pd.loc = loc;
    pd.msg = msg;
    if (note && note->loc.isValid()) {
      pd.note = *note;
      pd.has_note = true;
    }
    fctx.pending_diags.push_back(std::move(pd));
  }

  void FlushDiagnostics(const std::vector<PendingDiag>& diags) {
    std::set<std::pair<unsigned, std::string>> seen;
    for (const PendingDiag& pd : diags) {
      clang::SourceLocation exp_loc = sm_.getExpansionLoc(pd.loc);
      auto key = std::make_pair(exp_loc.getRawEncoding(), pd.msg);
      if (!seen.insert(key).second) continue;
      EmitImmediateWarning(pd.loc, pd.msg, pd.has_note ? &pd.note : nullptr);
    }
  }

  // --------------------------------------------------------------------------
  // Maglev Register Allocator Constraint Extraction & Symbolic Model
  // --------------------------------------------------------------------------

  using ExprSubMap = std::map<const clang::ValueDecl*, const clang::Expr*>;
  using InputBindingMap =
      std::map<const clang::ValueDecl*, std::pair<std::string, std::string>>;

  std::optional<int64_t> TryEvalConstInt(const clang::Expr* expr,
                                         const ExprSubMap& subs,
                                         int depth = 0) {
    if (!expr || depth > 6) return std::nullopt;
    const clang::Expr* cur = UnwrapExpr(expr);
    if (!cur) return std::nullopt;
    if (const auto* dre = llvm::dyn_cast<clang::DeclRefExpr>(cur)) {
      const clang::ValueDecl* d = CanonValueDecl(dre->getDecl());
      auto it = subs.find(d);
      if (it != subs.end() && it->second && it->second != cur) {
        return TryEvalConstInt(it->second, subs, depth + 1);
      }
    }
    if (const auto* me = llvm::dyn_cast<clang::MemberExpr>(cur)) {
      const clang::ValueDecl* d = CanonValueDecl(me->getMemberDecl());
      auto it = subs.find(d);
      if (it != subs.end() && it->second && it->second != cur) {
        return TryEvalConstInt(it->second, subs, depth + 1);
      }
    }
    if (!cur->isValueDependent() &&
        cur->getType()->isIntegralOrEnumerationType()) {
      clang::Expr::EvalResult r;
      if (cur->EvaluateAsInt(r, ast_ctx_)) {
        return r.Val.getInt().getSExtValue();
      }
    }
    return std::nullopt;
  }

  std::optional<int> ResolveAccessorToInputIndex(
      const clang::CXXMethodDecl* md) {
    if (!md || md->getNumParams() != 0) return std::nullopt;
    const clang::FunctionDecl* body_decl = nullptr;
    if (!md->hasBody(body_decl)) return std::nullopt;
    const auto* cs =
        llvm::dyn_cast_or_null<clang::CompoundStmt>(body_decl->getBody());
    if (!cs || cs->size() != 1) return std::nullopt;
    const auto* ret = llvm::dyn_cast<clang::ReturnStmt>(*cs->body_begin());
    if (!ret || !ret->getRetValue()) return std::nullopt;
    const clang::Expr* rv = UnwrapExpr(ret->getRetValue());
    if (const auto* mce =
            llvm::dyn_cast_or_null<clang::CXXMemberCallExpr>(rv)) {
      const clang::CXXMethodDecl* inner = mce->getMethodDecl();
      if (inner && SafeDeclName(inner) == "input" && mce->getNumArgs() == 1) {
        ExprSubMap empty_subs;
        if (auto idx = TryEvalConstInt(mce->getArg(0), empty_subs)) {
          return static_cast<int>(*idx);
        }
      }
    }
    return std::nullopt;
  }

  std::optional<std::string> ResolveTrivialGetterToField(
      const clang::CXXMethodDecl* md) {
    if (!md || md->getNumParams() != 0) return std::nullopt;
    const clang::FunctionDecl* body_decl = nullptr;
    if (!md->hasBody(body_decl)) return std::nullopt;
    const auto* cs =
        llvm::dyn_cast_or_null<clang::CompoundStmt>(body_decl->getBody());
    if (!cs || cs->size() != 1) return std::nullopt;
    const auto* ret = llvm::dyn_cast<clang::ReturnStmt>(*cs->body_begin());
    if (!ret || !ret->getRetValue()) return std::nullopt;
    const clang::Expr* rv = UnwrapExpr(ret->getRetValue());
    if (const auto* me = llvm::dyn_cast_or_null<clang::MemberExpr>(rv)) {
      if (llvm::isa<clang::CXXThisExpr>(me->getBase()->IgnoreParenImpCasts())) {
        if (const auto* fd =
                llvm::dyn_cast<clang::FieldDecl>(me->getMemberDecl())) {
          return fd->getNameAsString();
        }
      }
    }
    return std::nullopt;
  }

  std::string ResolveEnumConstantName(const clang::Expr* expr,
                                      const ExprSubMap& subs) {
    if (!expr) return "";
    const clang::Expr* cur = UnwrapExpr(expr);
    for (int hop = 0; cur && hop < 6; ++hop) {
      if (const auto* dre = llvm::dyn_cast<clang::DeclRefExpr>(cur)) {
        if (const auto* ecd =
                llvm::dyn_cast<clang::EnumConstantDecl>(dre->getDecl())) {
          return SafeDeclName(ecd).str();
        }
        auto it = subs.find(CanonValueDecl(dre->getDecl()));
        if (it != subs.end() && it->second && it->second != cur) {
          cur = UnwrapExpr(it->second);
          continue;
        }
      }
      break;
    }
    if (!cur) return "";
    clang::QualType qt = cur->getType().getNonReferenceType();
    if (!qt.isNull() && qt->isEnumeralType()) {
      if (auto iv = TryEvalConstInt(cur, subs)) {
        if (const auto* et = qt->getAs<clang::EnumType>()) {
          if (const clang::EnumDecl* ed = et->getDecl()) {
            for (const clang::EnumConstantDecl* ecd : ed->enumerators()) {
              if (ecd->getInitVal().getExtValue() == *iv) {
                return SafeDeclName(ecd).str();
              }
            }
          }
        }
      }
    }
    return "";
  }

  bool IsMaglevResultExpr(const clang::Expr* expr,
                          const ExprSubMap& subs = {}) {
    expr = UnwrapExpr(expr);
    for (int hop = 0; expr && hop < 8; ++hop) {
      if (const auto* dre = llvm::dyn_cast<clang::DeclRefExpr>(expr)) {
        auto it = subs.find(CanonValueDecl(dre->getDecl()));
        if (it != subs.end() && it->second && it->second != expr) {
          expr = UnwrapExpr(it->second);
          continue;
        }
      }
      if (const auto* construct =
              llvm::dyn_cast<clang::CXXConstructExpr>(expr)) {
        if (construct->getNumArgs() == 1) {
          expr = UnwrapExpr(construct->getArg(0));
          continue;
        }
      }
      if (const auto* mce = llvm::dyn_cast<clang::CXXMemberCallExpr>(expr)) {
        if (const clang::CXXMethodDecl* md = mce->getMethodDecl()) {
          llvm::StringRef mname = SafeDeclName(md);
          if (mce->getNumArgs() == 0 &&
              (mname == "operand" || mname == "location")) {
            expr = UnwrapExpr(mce->getImplicitObjectArgument());
            continue;
          }
          if (mce->getNumArgs() == 0 && mname == "result") {
            const clang::Expr* obj =
                UnwrapExpr(mce->getImplicitObjectArgument());
            for (int ohop = 0; obj && ohop < 6; ++ohop) {
              if (const auto* odre = llvm::dyn_cast<clang::DeclRefExpr>(obj)) {
                auto oit = subs.find(CanonValueDecl(odre->getDecl()));
                if (oit != subs.end() && oit->second && oit->second != obj) {
                  obj = UnwrapExpr(oit->second);
                  continue;
                }
              }
              break;
            }
            if (const auto* inner_mce =
                    llvm::dyn_cast_or_null<clang::CXXMemberCallExpr>(obj)) {
              if (const auto* inner_md = inner_mce->getMethodDecl()) {
                if (SafeDeclName(inner_md) == "node") return false;
              }
            }
            return true;
          }
        }
      }
      break;
    }
    return false;
  }

  std::pair<std::string, std::string> ExtractMaglevInputKey(
      const clang::Expr* expr, const InputBindingMap& local_inputs,
      const ExprSubMap& subs, int depth = 0) {
    if (!expr || depth > 6) return {"", ""};
    expr = UnwrapExpr(expr);
    if (!expr) return {"", ""};
    while (expr) {
      if (const auto* construct =
              llvm::dyn_cast<clang::CXXConstructExpr>(expr)) {
        if (construct->getNumArgs() == 1) {
          expr = UnwrapExpr(construct->getArg(0));
          continue;
        }
      }
      if (const auto* mce = llvm::dyn_cast<clang::CXXMemberCallExpr>(expr)) {
        if (const clang::CXXMethodDecl* md = mce->getMethodDecl()) {
          llvm::StringRef mname = SafeDeclName(md);
          if (mce->getNumArgs() == 0 &&
              (mname == "operand" || mname == "location")) {
            expr = UnwrapExpr(mce->getImplicitObjectArgument());
            continue;
          }
        }
      }
      break;
    }
    if (!expr) return {"", ""};
    if (IsMaglevResultExpr(expr, subs)) return {"", ""};

    if (const auto* dre = llvm::dyn_cast<clang::DeclRefExpr>(expr)) {
      const clang::ValueDecl* d = CanonValueDecl(dre->getDecl());
      auto lit = local_inputs.find(d);
      if (lit != local_inputs.end()) return lit->second;
      auto sit = subs.find(d);
      if (sit != subs.end() && sit->second && sit->second != expr) {
        return ExtractMaglevInputKey(sit->second, local_inputs, subs,
                                     depth + 1);
      }
    }

    if (const auto* mce = llvm::dyn_cast<clang::CXXMemberCallExpr>(expr)) {
      if (const auto* md = mce->getMethodDecl()) {
        std::string mname = SafeDeclName(md).str();
        if (mname == "result" && mce->getNumArgs() == 0) {
          return {"", ""};
        }
        if (mce->getNumArgs() == 0) {
          if (auto idx = ResolveAccessorToInputIndex(md)) {
            return {"idx:" + std::to_string(*idx), mname};
          }
          return {"m:" + mname, mname};
        }
        if (mname == "input" && mce->getNumArgs() == 1) {
          if (auto idx = TryEvalConstInt(mce->getArg(0), subs)) {
            return {"idx:" + std::to_string(*idx),
                    "input(" + std::to_string(*idx) + ")"};
          }
          return {"var:input", "input(...)"};
        }
        if (mce->getNumArgs() == 1 && IsMaglevInputType(mce->getType())) {
          if (auto idx = TryEvalConstInt(mce->getArg(0), subs)) {
            return {"m:" + mname + ":" + std::to_string(*idx),
                    mname + "(" + std::to_string(*idx) + ")"};
          }
          return {"var:*", mname + "(...)"};
        }
      }
    }

    std::string s;
    llvm::raw_string_ostream os(s);
    expr->printPretty(os, nullptr, ast_ctx_.getPrintingPolicy());
    os.flush();
    return {"expr:" + s, s};
  }

  std::string CanonicalizeExpr(const clang::Expr* expr, const ExprSubMap& subs,
                               int depth = 0) {
    if (!expr || depth > 8) return "";
    // Preserve optional `operator bool()` / `TryCast<T>()` before UnwrapExpr
    // strips implicit conversions if needed.
    const clang::Expr* raw = expr->IgnoreParens();
    if (const auto* clean = llvm::dyn_cast<clang::ExprWithCleanups>(raw)) {
      raw = clean->getSubExpr()->IgnoreParens();
    }
    if (const auto* ice = llvm::dyn_cast<clang::ImplicitCastExpr>(raw)) {
      if (ice->getCastKind() == clang::CK_UserDefinedConversion) {
        if (const auto* mce = llvm::dyn_cast<clang::CXXMemberCallExpr>(
                ice->getSubExpr()->IgnoreParens())) {
          if (const auto* conv =
                  llvm::dyn_cast_or_null<clang::CXXConversionDecl>(
                      mce->getMethodDecl())) {
            if (conv->getConversionType()->isBooleanType()) {
              return "HasVal(" +
                     CanonicalizeExpr(mce->getImplicitObjectArgument(), subs,
                                      depth + 1) +
                     ")";
            }
          }
        }
      }
    }

    const clang::Expr* cur = UnwrapExpr(raw);
    if (!cur) return "";

    if (const auto* dre = llvm::dyn_cast<clang::DeclRefExpr>(cur)) {
      const clang::ValueDecl* d = CanonValueDecl(dre->getDecl());
      auto it = subs.find(d);
      if (it != subs.end() && it->second && it->second != cur) {
        return CanonicalizeExpr(it->second, subs, depth + 1);
      }
      if (auto iv = TryEvalConstInt(cur, subs, depth + 1)) {
        return std::to_string(*iv);
      }
      return SafeDeclName(d).str();
    }

    if (const auto* me = llvm::dyn_cast<clang::MemberExpr>(cur)) {
      const clang::ValueDecl* d = CanonValueDecl(me->getMemberDecl());
      auto it = subs.find(d);
      if (it != subs.end() && it->second && it->second != cur) {
        return CanonicalizeExpr(it->second, subs, depth + 1);
      }
      if (llvm::isa<clang::CXXThisExpr>(me->getBase()->IgnoreParenImpCasts())) {
        return d->getNameAsString();
      }
      return CanonicalizeExpr(me->getBase(), subs, depth + 1) + "." +
             d->getNameAsString();
    }

    if (llvm::isa<clang::IntegerLiteral>(cur) ||
        llvm::isa<clang::CXXBoolLiteralExpr>(cur)) {
      if (auto iv = TryEvalConstInt(cur, subs, depth + 1)) {
        return std::to_string(*iv);
      }
    }

    if (const auto* mce = llvm::dyn_cast<clang::CXXMemberCallExpr>(cur)) {
      if (const clang::CXXMethodDecl* md = mce->getMethodDecl()) {
        if (const auto* conv = llvm::dyn_cast<clang::CXXConversionDecl>(md)) {
          if (conv->getConversionType()->isBooleanType()) {
            return "HasVal(" +
                   CanonicalizeExpr(mce->getImplicitObjectArgument(), subs,
                                    depth + 1) +
                   ")";
          }
        }
        llvm::StringRef mname = SafeDeclName(md);
        const clang::Expr* obj = mce->getImplicitObjectArgument();
        if (mce->getNumArgs() == 0 && mname == "has_value") {
          return "HasVal(" + CanonicalizeExpr(obj, subs, depth + 1) + ")";
        }
        if (mce->getNumArgs() == 0 && mname == "value") {
          return "Deref(" + CanonicalizeExpr(obj, subs, depth + 1) + ")";
        }
        if (mce->getNumArgs() == 0 && (mname == "Is" || mname == "TryCast")) {
          if (const auto* targs = md->getTemplateSpecializationArgs()) {
            if (targs->size() >= 1 &&
                targs->get(0).getKind() == clang::TemplateArgument::Type) {
              std::string tname = targs->get(0).getAsType().getAsString(
                  ast_ctx_.getPrintingPolicy());
              // Strip leading qualifiers like `class v8::internal::maglev::`.
              size_t pos = tname.rfind("::");
              if (pos != std::string::npos) tname = tname.substr(pos + 2);
              return "IsType(" + CanonicalizeExpr(obj, subs, depth + 1) + "," +
                     tname + ")";
            }
          }
        }
        if (mce->getNumArgs() == 0) {
          if (auto field = ResolveTrivialGetterToField(md)) {
            if (!obj ||
                llvm::isa<clang::CXXThisExpr>(obj->IgnoreParenImpCasts())) {
              return *field;
            }
          }
          if (auto idx = ResolveAccessorToInputIndex(md)) {
            if (!obj ||
                llvm::isa<clang::CXXThisExpr>(obj->IgnoreParenImpCasts())) {
              return "input(" + std::to_string(*idx) + ")";
            }
          }
        }
        std::string res;
        if (obj && !llvm::isa<clang::CXXThisExpr>(obj->IgnoreParenImpCasts())) {
          res = CanonicalizeExpr(obj, subs, depth + 1) + ".";
        }
        res += mname.str() + "(";
        for (unsigned i = 0; i < mce->getNumArgs(); ++i) {
          if (llvm::isa<clang::CXXDefaultArgExpr>(mce->getArg(i))) continue;
          if (i > 0) res += ",";
          res += CanonicalizeExpr(mce->getArg(i), subs, depth + 1);
        }
        res += ")";
        return res;
      }
    }

    if (const auto* oce = llvm::dyn_cast<clang::CXXOperatorCallExpr>(cur)) {
      if (oce->getOperator() == clang::OO_Star && oce->getNumArgs() == 1) {
        return "Deref(" + CanonicalizeExpr(oce->getArg(0), subs, depth + 1) +
               ")";
      }
      if (oce->getOperator() == clang::OO_Exclaim && oce->getNumArgs() == 1) {
        return "!(" + CanonicalizeExpr(oce->getArg(0), subs, depth + 1) + ")";
      }
    }

    if (const auto* ce = llvm::dyn_cast<clang::CallExpr>(cur)) {
      if (const clang::FunctionDecl* callee = ce->getDirectCallee()) {
        std::string res = SafeDeclName(callee).str() + "(";
        bool first = true;
        for (unsigned i = 0; i < ce->getNumArgs(); ++i) {
          if (llvm::isa<clang::CXXDefaultArgExpr>(ce->getArg(i))) continue;
          if (!first) res += ",";
          first = false;
          res += CanonicalizeExpr(ce->getArg(i), subs, depth + 1);
        }
        res += ")";
        return res;
      }
    }

    if (const auto* uo = llvm::dyn_cast<clang::UnaryOperator>(cur)) {
      if (uo->getOpcode() == clang::UO_Deref) {
        return "Deref(" + CanonicalizeExpr(uo->getSubExpr(), subs, depth + 1) +
               ")";
      }
      if (uo->getOpcode() == clang::UO_LNot) {
        return "!(" + CanonicalizeExpr(uo->getSubExpr(), subs, depth + 1) + ")";
      }
    }

    if (auto iv = TryEvalConstInt(cur, subs, depth + 1)) {
      return std::to_string(*iv);
    }

    std::string s;
    llvm::raw_string_ostream os(s);
    cur->printPretty(os, nullptr, ast_ctx_.getPrintingPolicy());
    os.flush();
    if (s.rfind("this->", 0) == 0) s = s.substr(6);
    return s;
  }

  // Returns the single return expression of a pure boolean/int helper function
  // if its body is `{ return expr; }` and contains no assembler writes.
  const clang::Expr* GetSingleReturnExpr(const clang::FunctionDecl* fd) {
    if (!fd) return nullptr;
    const clang::FunctionDecl* body_decl = nullptr;
    if (!fd->hasBody(body_decl)) return nullptr;
    const auto* cs =
        llvm::dyn_cast_or_null<clang::CompoundStmt>(body_decl->getBody());
    if (!cs || cs->size() != 1) return nullptr;
    const auto* ret = llvm::dyn_cast<clang::ReturnStmt>(*cs->body_begin());
    return ret ? ret->getRetValue() : nullptr;
  }

  std::vector<std::vector<MaglevCondAtom>> SplitCondBranches(
      const clang::Expr* cond, bool want_true, const ExprSubMap& subs,
      int depth = 0) {
    if (!cond) return {{}};
    if (depth > 4) {
      std::string s = CanonicalizeExpr(cond, subs);
      if (s.empty()) return {{}};
      return {{{s, want_true}}};
    }

    // Check compile-time constant boolean condition before unwrapping.
    if (!cond->isValueDependent()) {
      bool bval = false;
      if (cond->EvaluateAsBooleanCondition(bval, ast_ctx_)) {
        if (bval == want_true) return {{}};
        return {};
      }
    }

    // Detect `std::optional::operator bool()` before UnwrapExpr strips it!
    const clang::Expr* raw = cond->IgnoreParens();
    if (const auto* clean = llvm::dyn_cast<clang::ExprWithCleanups>(raw)) {
      raw = clean->getSubExpr()->IgnoreParens();
    }
    if (const auto* ice = llvm::dyn_cast<clang::ImplicitCastExpr>(raw)) {
      if (ice->getCastKind() == clang::CK_UserDefinedConversion) {
        if (const auto* mce = llvm::dyn_cast<clang::CXXMemberCallExpr>(
                ice->getSubExpr()->IgnoreParens())) {
          if (const auto* conv =
                  llvm::dyn_cast_or_null<clang::CXXConversionDecl>(
                      mce->getMethodDecl())) {
            if (conv->getConversionType()->isBooleanType()) {
              std::string s =
                  "HasVal(" +
                  CanonicalizeExpr(mce->getImplicitObjectArgument(), subs) +
                  ")";
              return {{{std::move(s), want_true}}};
            }
          }
        }
      }
    }

    const clang::Expr* cur = UnwrapExpr(raw);
    if (!cur) return {{}};

    if (const auto* uo = llvm::dyn_cast<clang::UnaryOperator>(cur)) {
      if (uo->getOpcode() == clang::UO_LNot) {
        return SplitCondBranches(uo->getSubExpr(), !want_true, subs, depth);
      }
    }
    if (const auto* oce = llvm::dyn_cast<clang::CXXOperatorCallExpr>(cur)) {
      if (oce->getOperator() == clang::OO_Exclaim && oce->getNumArgs() == 1) {
        return SplitCondBranches(oce->getArg(0), !want_true, subs, depth);
      }
    }

    auto combine = [](const std::vector<std::vector<MaglevCondAtom>>& a,
                      const std::vector<std::vector<MaglevCondAtom>>& b) {
      std::vector<std::vector<MaglevCondAtom>> out;
      for (const auto& ba : a) {
        for (const auto& bb : b) {
          if (!CondAtomListsCompatible(ba, bb)) continue;
          std::vector<MaglevCondAtom> m = ba;
          for (const auto& atom : bb) {
            if (std::find(m.begin(), m.end(), atom) == m.end()) {
              m.push_back(atom);
            }
          }
          out.push_back(std::move(m));
          if (out.size() >= 8) return out;
        }
      }
      return out;
    };

    if (const auto* bo = llvm::dyn_cast<clang::BinaryOperator>(cur)) {
      if (bo->getOpcode() == clang::BO_LAnd) {
        if (want_true) {
          return combine(SplitCondBranches(bo->getLHS(), true, subs, depth),
                         SplitCondBranches(bo->getRHS(), true, subs, depth));
        } else {
          auto out = SplitCondBranches(bo->getLHS(), false, subs, depth);
          auto right_false =
              combine(SplitCondBranches(bo->getLHS(), true, subs, depth),
                      SplitCondBranches(bo->getRHS(), false, subs, depth));
          for (auto& br : right_false) {
            if (out.size() >= 8) break;
            out.push_back(std::move(br));
          }
          return out;
        }
      }
      if (bo->getOpcode() == clang::BO_LOr) {
        if (want_true) {
          auto out = SplitCondBranches(bo->getLHS(), true, subs, depth);
          auto right_true =
              combine(SplitCondBranches(bo->getLHS(), false, subs, depth),
                      SplitCondBranches(bo->getRHS(), true, subs, depth));
          for (auto& br : right_true) {
            if (out.size() >= 8) break;
            out.push_back(std::move(br));
          }
          return out;
        } else {
          return combine(SplitCondBranches(bo->getLHS(), false, subs, depth),
                         SplitCondBranches(bo->getRHS(), false, subs, depth));
        }
      }
    }

    // Normalize comparisons (`==`, `!=`, `<`, `<=`, `>`, `>=`):
    bool is_cmp = false;
    clang::BinaryOperatorKind cmp_op = clang::BO_EQ;
    const clang::Expr* lhs = nullptr;
    const clang::Expr* rhs = nullptr;
    if (const auto* bo = llvm::dyn_cast<clang::BinaryOperator>(cur)) {
      if (bo->isComparisonOp()) {
        is_cmp = true;
        cmp_op = bo->getOpcode();
        lhs = bo->getLHS();
        rhs = bo->getRHS();
      }
    } else if (const auto* oce =
                   llvm::dyn_cast<clang::CXXOperatorCallExpr>(cur)) {
      if (oce->getNumArgs() == 2) {
        switch (oce->getOperator()) {
          case clang::OO_EqualEqual:
            is_cmp = true;
            cmp_op = clang::BO_EQ;
            break;
          case clang::OO_ExclaimEqual:
            is_cmp = true;
            cmp_op = clang::BO_NE;
            break;
          case clang::OO_Less:
            is_cmp = true;
            cmp_op = clang::BO_LT;
            break;
          case clang::OO_LessEqual:
            is_cmp = true;
            cmp_op = clang::BO_LE;
            break;
          case clang::OO_Greater:
            is_cmp = true;
            cmp_op = clang::BO_GT;
            break;
          case clang::OO_GreaterEqual:
            is_cmp = true;
            cmp_op = clang::BO_GE;
            break;
          default:
            break;
        }
        if (is_cmp) {
          lhs = oce->getArg(0);
          rhs = oce->getArg(1);
        }
      }
    }
    if (is_cmp && lhs && rhs) {
      // Handle `ptr != nullptr` / `ptr == nullptr` where `ptr` is `TryCast<T>`.
      bool lhs_null =
          llvm::isa_and_nonnull<clang::CXXNullPtrLiteralExpr>(UnwrapExpr(lhs));
      bool rhs_null =
          llvm::isa_and_nonnull<clang::CXXNullPtrLiteralExpr>(UnwrapExpr(rhs));
      if ((cmp_op == clang::BO_EQ || cmp_op == clang::BO_NE) &&
          (lhs_null != rhs_null)) {
        const clang::Expr* non_null = lhs_null ? rhs : lhs;
        bool target_truth = (cmp_op == clang::BO_NE) ? want_true : !want_true;
        return SplitCondBranches(non_null, target_truth, subs, depth + 1);
      }

      auto li = TryEvalConstInt(lhs, subs);
      auto ri = TryEvalConstInt(rhs, subs);
      if (li && ri) {
        bool res = false;
        switch (cmp_op) {
          case clang::BO_EQ:
            res = (*li == *ri);
            break;
          case clang::BO_NE:
            res = (*li != *ri);
            break;
          case clang::BO_LT:
            res = (*li < *ri);
            break;
          case clang::BO_LE:
            res = (*li <= *ri);
            break;
          case clang::BO_GT:
            res = (*li > *ri);
            break;
          case clang::BO_GE:
            res = (*li >= *ri);
            break;
          default:
            break;
        }
        if (res == want_true) return {{}};
        return {};
      }
      // Put integer constant on RHS for ParseIntCmpAtom compatibility.
      if (li && !ri) {
        std::swap(lhs, rhs);
        std::swap(li, ri);
        switch (cmp_op) {
          case clang::BO_LT:
            cmp_op = clang::BO_GT;
            break;
          case clang::BO_LE:
            cmp_op = clang::BO_GE;
            break;
          case clang::BO_GT:
            cmp_op = clang::BO_LT;
            break;
          case clang::BO_GE:
            cmp_op = clang::BO_LE;
            break;
          default:
            break;
        }
      }
      std::string ls = CanonicalizeExpr(lhs, subs);
      std::string rs = ri ? std::to_string(*ri) : CanonicalizeExpr(rhs, subs);
      std::string op_str = " == ";
      bool truth = want_true;
      switch (cmp_op) {
        case clang::BO_EQ:
          op_str = " == ";
          break;
        case clang::BO_NE:
          op_str = " == ";
          truth = !want_true;
          break;
        case clang::BO_GT:
          op_str = " > ";
          break;
        case clang::BO_LE:
          op_str = " > ";
          truth = !want_true;
          break;
        case clang::BO_GE:
          op_str = " >= ";
          break;
        case clang::BO_LT:
          op_str = " >= ";
          truth = !want_true;
          break;
        default:
          break;
      }
      return {{{ls + op_str + rs, truth}}};
    }

    // Substitute local bool/pointer variable bindings (e.g. `if (auto c =
    // x->TryCast<T>())` or `bool flag = ...; if (flag)`).
    if (const auto* dre = llvm::dyn_cast<clang::DeclRefExpr>(cur)) {
      const clang::ValueDecl* d = CanonValueDecl(dre->getDecl());
      auto it = subs.find(d);
      if (it != subs.end() && it->second && it->second != cur) {
        return SplitCondBranches(it->second, want_true, subs, depth + 1);
      }
    }

    // Inline pure single-return `bool` helper functions (e.g.,
    // `IsClampedFromLength`, `IsLength`) so equivalent predicates unify across
    // `SetValueLocationConstraints` and `GenerateCode`.
    if (depth < 2) {
      if (const auto* ce = llvm::dyn_cast<clang::CallExpr>(cur)) {
        if (!llvm::isa<clang::CXXMemberCallExpr>(ce) &&
            !llvm::isa<clang::CXXOperatorCallExpr>(ce)) {
          if (const clang::FunctionDecl* callee = ce->getDirectCallee()) {
            if (callee->getReturnType()->isBooleanType()) {
              if (const clang::Expr* ret_expr = GetSingleReturnExpr(callee)) {
                ExprSubMap child_subs = subs;
                unsigned np = std::min<unsigned>(callee->getNumParams(),
                                                 ce->getNumArgs());
                for (unsigned i = 0; i < np; ++i) {
                  const clang::Expr* a = UnwrapExpr(ce->getArg(i));
                  if (const auto* adre =
                          llvm::dyn_cast_or_null<clang::DeclRefExpr>(a)) {
                    auto sit = subs.find(CanonValueDecl(adre->getDecl()));
                    if (sit != subs.end() && sit->second) a = sit->second;
                  }
                  child_subs[callee->getParamDecl(i)->getCanonicalDecl()] = a;
                }
                return SplitCondBranches(ret_expr, want_true, child_subs,
                                         depth + 1);
              }
            }
          }
        }
      }
    }

    std::string s = CanonicalizeExpr(cur, subs);
    if (s.empty()) return {{}};
    return {{{std::move(s), want_true}}};
  }

  void ExtractMaglevOpProperties(const clang::CXXRecordDecl* cls,
                                 MaglevNodeSpec& spec) {
    if (!cls) return;
    spec.cls_name = cls->getQualifiedNameAsString();
    spec.is_value_node = HasBaseOrSelfNamed(cls, "ValueNode");
    spec.is_terminal_control = HasBaseOrSelfNamed(cls, "TerminalControlNode");

    const clang::CXXRecordDecl* def = cls->getDefinition();
    if (!def) return;
    // Search `cls` and its bases for `static constexpr OpProperties
    // kProperties`.
    llvm::SmallVector<const clang::CXXRecordDecl*, 8> worklist;
    llvm::SmallPtrSet<const clang::CXXRecordDecl*, 8> visited;
    worklist.push_back(def);
    while (!worklist.empty()) {
      const clang::CXXRecordDecl* cur = worklist.pop_back_val();
      if (!cur || !visited.insert(cur).second) continue;
      for (const clang::Decl* d : cur->decls()) {
        const auto* vd = llvm::dyn_cast<clang::VarDecl>(d);
        if (!vd || SafeDeclName(vd) != "kProperties") continue;
        if (!vd->hasInit() || vd->getInit()->isValueDependent()) continue;
        const clang::APValue* val = vd->evaluateValue();
        if (!val) continue;
        if (auto bits_opt = ExtractIntFromAPValue(*val)) {
          uint32_t bits = static_cast<uint32_t>(*bits_opt);
          spec.is_call = (bits & 1u) != 0;
          uint32_t deopt = (bits >> 1) & 7u;
          spec.can_eager_deopt = (deopt == 1 || deopt == 3);
          spec.can_lazy_deopt = (deopt == 2 || deopt == 3);
          spec.can_allocate = ((bits >> 7) & 1u) != 0;
          uint32_t repr = (bits >> 9) & 15u;
          spec.result_is_double = (repr == 3 || repr == 4);
          spec.is_deferred_call = ((bits >> 14) & 1u) != 0;
        }
        return;
      }
      for (const clang::CXXBaseSpecifier& base : cur->bases()) {
        if (const auto* br = base.getType()->getAsCXXRecordDecl()) {
          if (const auto* bdef = br->getDefinition()) worklist.push_back(bdef);
        }
      }
    }
  }

  void ExtractMaglevInputReprs(const clang::CXXRecordDecl* cls,
                               MaglevNodeSpec& spec) {
    if (!cls) return;
    const clang::CXXRecordDecl* def = cls->getDefinition();
    if (!def) return;
    llvm::SmallVector<const clang::CXXRecordDecl*, 8> worklist;
    llvm::SmallPtrSet<const clang::CXXRecordDecl*, 8> visited;
    worklist.push_back(def);
    bool found_reprs = false;
    while (!worklist.empty()) {
      const clang::CXXRecordDecl* cur = worklist.pop_back_val();
      if (!cur || !visited.insert(cur).second) continue;
      for (const clang::Decl* d : cur->decls()) {
        const auto* vd = llvm::dyn_cast<clang::VarDecl>(d);
        if (!vd || !vd->hasInit() || vd->getInit()->isValueDependent()) {
          continue;
        }
        llvm::StringRef vname = SafeDeclName(vd);
        if (!found_reprs && vname == "kInputRepresentations") {
          if (const clang::APValue* val = vd->evaluateValue()) {
            std::function<void(const clang::APValue&)> collect_ints =
                [&](const clang::APValue& v) {
                  if (v.isInt()) {
                    spec.input_reprs.push_back(
                        static_cast<int>(v.getInt().getSExtValue()));
                  } else if (v.isArray()) {
                    for (unsigned i = 0; i < v.getArraySize(); ++i) {
                      collect_ints(v.getArrayInitializedElt(i));
                    }
                  } else if (v.isStruct()) {
                    for (unsigned i = 0; i < v.getStructNumBases(); ++i) {
                      collect_ints(v.getStructBase(i));
                    }
                    for (unsigned i = 0; i < v.getStructNumFields(); ++i) {
                      collect_ints(v.getStructField(i));
                    }
                  }
                };
            collect_ints(*val);
            if (!spec.input_reprs.empty()) found_reprs = true;
          }
        } else if (vname.size() > 6 && vname.starts_with("k") &&
                   vname.ends_with("Index")) {
          std::string in_key =
              "m:" + vname.slice(1, vname.size() - 5).str() + "Input";
          if (!spec.input_key_indices.count(in_key)) {
            if (const clang::APValue* val = vd->evaluateValue()) {
              if (auto iv = ExtractIntFromAPValue(*val)) {
                spec.input_key_indices[in_key] = static_cast<int>(*iv);
              }
            }
          }
        }
      }
      for (const clang::CXXBaseSpecifier& base : cur->bases()) {
        if (const auto* br = base.getType()->getAsCXXRecordDecl()) {
          if (const auto* bdef = br->getDefinition()) worklist.push_back(bdef);
        }
      }
    }
  }

  static int LookupInputRepr(const MaglevNodeSpec& spec,
                             const std::string& key) {
    if (spec.input_reprs.empty() || key.empty()) return -1;
    int idx = ResolveInputKeyIndex(spec, key);
    if (idx >= 0 && static_cast<size_t>(idx) < spec.input_reprs.size()) {
      return spec.input_reprs[idx];
    }
    return -1;
  }

  // Returns the explicit ARM64 register width forced by an outer `.X()`/`.W()`
  // /`.S()`/`AcquireX()`/`AcquireW()` call, or kUnknown if none.
  static Arm64RegWidth ExprExplicitArm64Width(const clang::Expr* expr) {
    while (expr) {
      expr = expr->IgnoreParenImpCasts();
      if (const auto* clean = llvm::dyn_cast<clang::ExprWithCleanups>(expr)) {
        expr = clean->getSubExpr();
        continue;
      }
      if (const auto* mat =
              llvm::dyn_cast<clang::MaterializeTemporaryExpr>(expr)) {
        expr = mat->getSubExpr();
        continue;
      }
      if (const auto* bind =
              llvm::dyn_cast<clang::CXXBindTemporaryExpr>(expr)) {
        expr = bind->getSubExpr();
        continue;
      }
      if (const auto* cce = llvm::dyn_cast<clang::CXXConstructExpr>(expr)) {
        if (cce->getNumArgs() == 1) {
          expr = cce->getArg(0);
          continue;
        }
      }
      if (const auto* mce = llvm::dyn_cast<clang::CXXMemberCallExpr>(expr)) {
        if (const auto* md = mce->getMethodDecl()) {
          llvm::StringRef mname = SafeDeclName(md);
          if (mname == "W" || mname == "S" || mname == "AcquireW") {
            return Arm64RegWidth::kW;
          }
          if (mname == "X" || mname == "AcquireX") {
            return Arm64RegWidth::kX;
          }
        }
      }
      break;
    }
    return Arm64RegWidth::kUnknown;
  }

  Arm64RegWidth EffectiveArm64Width(const clang::Expr* expr, const Env& env) {
    Arm64RegWidth expl = ExprExplicitArm64Width(expr);
    if (expl != Arm64RegWidth::kUnknown) return expl;
    if (const clang::ValueDecl* sd = ResolveSymDecl(expr, env)) {
      auto it = env.syms.find(sd);
      if (it != env.syms.end()) return it->second.arm64_width;
    }
    if (auto m = MatchMaglevToRegisterCall(expr, env)) {
      if (!m->is_double) return Arm64RegWidth::kX;
    }
    return Arm64RegWidth::kUnknown;
  }

  static bool ExprReferencesDecl(const clang::Expr* expr,
                                 const clang::ValueDecl* target) {
    if (!expr || !target) return false;
    class RefFinder : public clang::RecursiveASTVisitor<RefFinder> {
     public:
      const clang::ValueDecl* target;
      bool found = false;
      explicit RefFinder(const clang::ValueDecl* t) : target(t) {}
      bool VisitDeclRefExpr(const clang::DeclRefExpr* dre) {
        if (CanonValueDecl(dre->getDecl()) == target) {
          found = true;
          return false;
        }
        return true;
      }
    } rf(target);
    rf.TraverseStmt(const_cast<clang::Expr*>(expr));
    return rf.found;
  }

  static bool IsTriviallyFalsyReturnExpr(const clang::Expr* expr,
                                         clang::ASTContext& ctx) {
    if (!expr) return true;
    const clang::Expr* u = expr->IgnoreParenImpCasts();
    while (u) {
      if (const auto* clean = llvm::dyn_cast<clang::ExprWithCleanups>(u)) {
        u = clean->getSubExpr()->IgnoreParenImpCasts();
        continue;
      }
      if (const auto* bind = llvm::dyn_cast<clang::CXXBindTemporaryExpr>(u)) {
        u = bind->getSubExpr()->IgnoreParenImpCasts();
        continue;
      }
      if (const auto* mat =
              llvm::dyn_cast<clang::MaterializeTemporaryExpr>(u)) {
        u = mat->getSubExpr()->IgnoreParenImpCasts();
        continue;
      }
      break;
    }
    if (!u) return true;
    if (llvm::isa<clang::CXXNullPtrLiteralExpr>(u)) return true;
    if (const auto* bl = llvm::dyn_cast<clang::CXXBoolLiteralExpr>(u)) {
      return !bl->getValue();
    }
    if (const auto* ile = llvm::dyn_cast<clang::InitListExpr>(u)) {
      return ile->getNumInits() == 0;
    }
    if (const auto* cce = llvm::dyn_cast<clang::CXXConstructExpr>(u)) {
      if (cce->getNumArgs() == 0) return true;
      if (cce->getNumArgs() == 1) {
        const clang::Expr* a0 = UnwrapExpr(cce->getArg(0));
        if (const auto* dre = llvm::dyn_cast_or_null<clang::DeclRefExpr>(a0)) {
          if (SafeDeclName(dre->getDecl()) == "nullopt") return true;
        }
      }
    }
    if (!u->isValueDependent()) {
      bool bval = true;
      if (u->EvaluateAsBooleanCondition(bval, ctx)) return !bval;
    }
    return false;
  }

  static bool IsConstantOpcodeExpr(const clang::Expr* expr) {
    if (!expr) return false;
    const clang::Expr* u = expr->IgnoreParenImpCasts();
    if (const auto* dre = llvm::dyn_cast<clang::DeclRefExpr>(u)) {
      llvm::StringRef n = SafeDeclName(dre->getDecl());
      return n.ends_with("Constant");
    }
    return false;
  }

  // Checks structurally from a callee's AST whether returning a truthy value
  // proves that parameter `param_idx` (a Maglev ValueNode*) is a constant node.
  bool FuncTruthyImpliesConstantNode(const clang::FunctionDecl* fn,
                                     unsigned param_idx) {
    if (!fn || param_idx >= fn->getNumParams()) return false;
    const clang::FunctionDecl* body_decl = nullptr;
    if (!fn->hasBody(body_decl) || !body_decl->getBody()) return false;
    auto key = std::make_pair(body_decl->getCanonicalDecl(), param_idx);
    auto cit = const_pred_cache_.find(key);
    if (cit != const_pred_cache_.end()) return cit->second;

    const clang::ValueDecl* target_param =
        body_decl->getParamDecl(param_idx)->getCanonicalDecl();
    auto is_param_opcode_call = [&](const clang::Expr* e) -> bool {
      e = UnwrapExpr(e);
      const auto* mce = llvm::dyn_cast_or_null<clang::CXXMemberCallExpr>(e);
      if (!mce || SafeDeclName(mce->getMethodDecl()) != "opcode") return false;
      const clang::Expr* obj = UnwrapExpr(mce->getImplicitObjectArgument());
      const auto* dre = llvm::dyn_cast_or_null<clang::DeclRefExpr>(obj);
      return dre && CanonValueDecl(dre->getDecl()) == target_param;
    };

    // Walk statements tracking whether we are currently inside a switch on
    // `param->opcode()` whose active case label(s) are all constant opcodes.
    bool saw_truthy_return = false;
    bool all_truthy_proven = true;
    std::function<void(const clang::Stmt*, bool)> walk =
        [&](const clang::Stmt* s, bool in_const_case) {
          if (!s || !all_truthy_proven) return;
          if (const auto* ret = llvm::dyn_cast<clang::ReturnStmt>(s)) {
            const clang::Expr* rv = ret->getRetValue();
            if (IsTriviallyFalsyReturnExpr(rv, ast_ctx_)) return;
            saw_truthy_return = true;
            if (in_const_case) return;
            // Also accept direct `return IsConstantNode(param->opcode())` or
            // `return param->Is<*Constant>()`.
            const clang::Expr* urv = UnwrapExpr(rv);
            if (const auto* ce = llvm::dyn_cast_or_null<clang::CallExpr>(urv)) {
              if (const clang::FunctionDecl* cal = ce->getDirectCallee()) {
                if (SafeDeclName(cal) == "IsConstantNode" &&
                    ce->getNumArgs() == 1 &&
                    is_param_opcode_call(ce->getArg(0))) {
                  return;
                }
              }
            }
            all_truthy_proven = false;
            return;
          }
          if (const auto* sw = llvm::dyn_cast<clang::SwitchStmt>(s)) {
            if (is_param_opcode_call(sw->getCond())) {
              const auto* body =
                  llvm::dyn_cast_or_null<clang::CompoundStmt>(sw->getBody());
              if (!body) {
                all_truthy_proven = false;
                return;
              }
              bool cur_const = false;
              for (const clang::Stmt* child : body->body()) {
                const clang::Stmt* cur = child;
                bool saw_label = false;
                bool group_const = true;
                while (llvm::isa<clang::CaseStmt>(cur) ||
                       llvm::isa<clang::DefaultStmt>(cur)) {
                  saw_label = true;
                  if (const auto* cs = llvm::dyn_cast<clang::CaseStmt>(cur)) {
                    if (!IsConstantOpcodeExpr(cs->getLHS())) {
                      group_const = false;
                    }
                    cur = cs->getSubStmt();
                  } else {
                    group_const = false;
                    cur = llvm::cast<clang::DefaultStmt>(cur)->getSubStmt();
                  }
                }
                if (saw_label) cur_const = group_const;
                if (cur) walk(cur, cur_const);
              }
              return;
            }
          }
          for (const clang::Stmt* child : s->children()) {
            if (child) walk(child, in_const_case);
          }
        };
    walk(body_decl->getBody(), false);
    bool res = saw_truthy_return && all_truthy_proven;
    const_pred_cache_[key] = res;
    return res;
  }

  // Collects Maglev input keys that `cond` mathematically proves to be a
  // constant ValueNode when `cond` evaluates to true.
  void CollectGuaranteedConstantInputs(const clang::Expr* cond,
                                       const InputBindingMap& in_binds,
                                       const ExprSubMap& subs,
                                       std::set<std::string>& out_keys) {
    // Unwrap `std::optional::operator bool()` / `.has_value()` so truthiness
    // tests on `std::optional`-returning constant predicates reach the call.
    const clang::Expr* cur = UnwrapExpr(cond);
    while (const auto* mce =
               llvm::dyn_cast_or_null<clang::CXXMemberCallExpr>(cur)) {
      const clang::CXXMethodDecl* md = mce->getMethodDecl();
      if (!md || mce->getNumArgs() != 0) break;
      bool is_bool_conv = false;
      if (const auto* conv = llvm::dyn_cast<clang::CXXConversionDecl>(md)) {
        is_bool_conv = conv->getConversionType()->isBooleanType();
      }
      if (is_bool_conv || SafeDeclName(md) == "has_value") {
        cur = UnwrapExpr(mce->getImplicitObjectArgument());
        continue;
      }
      break;
    }
    if (!cur) return;
    if (const auto* bo = llvm::dyn_cast<clang::BinaryOperator>(cur)) {
      if (bo->getOpcode() == clang::BO_LAnd) {
        CollectGuaranteedConstantInputs(bo->getLHS(), in_binds, subs, out_keys);
        CollectGuaranteedConstantInputs(bo->getRHS(), in_binds, subs, out_keys);
        return;
      }
      if (bo->getOpcode() == clang::BO_EQ) {
        // `X.node()->opcode() == Opcode::k*Constant`
        auto check_eq = [&](const clang::Expr* a, const clang::Expr* b) {
          if (!IsConstantOpcodeExpr(b)) return;
          const auto* mce =
              llvm::dyn_cast_or_null<clang::CXXMemberCallExpr>(UnwrapExpr(a));
          if (!mce || SafeDeclName(mce->getMethodDecl()) != "opcode") return;
          const auto* nmce = llvm::dyn_cast_or_null<clang::CXXMemberCallExpr>(
              UnwrapExpr(mce->getImplicitObjectArgument()));
          if (!nmce || SafeDeclName(nmce->getMethodDecl()) != "node") return;
          auto [k, lbl] = ExtractMaglevInputKey(
              nmce->getImplicitObjectArgument(), in_binds, subs);
          if (!k.empty()) out_keys.insert(k);
        };
        check_eq(bo->getLHS(), bo->getRHS());
        check_eq(bo->getRHS(), bo->getLHS());
        return;
      }
    }
    if (const auto* ce = llvm::dyn_cast<clang::CallExpr>(cur)) {
      const clang::FunctionDecl* callee = ce->getDirectCallee();
      if (!callee) return;
      llvm::StringRef cname = SafeDeclName(callee);
      // Case 1: `Is<Name>InputConstant()` on `this` (defined by DECLARE_INPUTS)
      if (const auto* mce = llvm::dyn_cast<clang::CXXMemberCallExpr>(ce)) {
        if (mce->getNumArgs() == 0 && cname.starts_with("Is") &&
            cname.ends_with("InputConstant")) {
          if (const clang::Expr* ret = GetSingleReturnExpr(callee)) {
            if (const auto* rce =
                    llvm::dyn_cast_or_null<clang::CallExpr>(UnwrapExpr(ret))) {
              if (const clang::FunctionDecl* rcal = rce->getDirectCallee()) {
                if (SafeDeclName(rcal) == "IsConstantNode") {
                  std::string in_key =
                      cname.slice(2, cname.size() - 8).str();  // `<Name>Input`
                  if (!in_key.empty()) out_keys.insert(in_key);
                  return;
                }
              }
            }
          }
        }
        // Case 2: `X.node()->Is<*Constant>()` /
        // `X.node()->TryCast<*Constant>()`
        if ((cname == "Is" || cname == "TryCast") &&
            callee->getTemplateSpecializationArgs() &&
            callee->getTemplateSpecializationArgs()->size() >= 1) {
          const clang::TemplateArgument& targ =
              callee->getTemplateSpecializationArgs()->get(0);
          if (targ.getKind() == clang::TemplateArgument::Type) {
            if (const auto* rec = targ.getAsType()->getAsCXXRecordDecl()) {
              if (SafeDeclName(rec).ends_with("Constant")) {
                const auto* nmce =
                    llvm::dyn_cast_or_null<clang::CXXMemberCallExpr>(
                        UnwrapExpr(mce->getImplicitObjectArgument()));
                if (nmce && SafeDeclName(nmce->getMethodDecl()) == "node") {
                  auto [k, lbl] = ExtractMaglevInputKey(
                      nmce->getImplicitObjectArgument(), in_binds, subs);
                  if (!k.empty()) out_keys.insert(k);
                  return;
                }
              }
            }
          }
        }
      }
      // Case 3: `IsConstantNode(X.node()->opcode())`
      if (cname == "IsConstantNode" && ce->getNumArgs() == 1) {
        const auto* mce = llvm::dyn_cast_or_null<clang::CXXMemberCallExpr>(
            UnwrapExpr(ce->getArg(0)));
        if (mce && SafeDeclName(mce->getMethodDecl()) == "opcode") {
          const auto* nmce = llvm::dyn_cast_or_null<clang::CXXMemberCallExpr>(
              UnwrapExpr(mce->getImplicitObjectArgument()));
          if (nmce && SafeDeclName(nmce->getMethodDecl()) == "node") {
            auto [k, lbl] = ExtractMaglevInputKey(
                nmce->getImplicitObjectArgument(), in_binds, subs);
            if (!k.empty()) out_keys.insert(k);
            return;
          }
        }
      }
      // Case 4: Any predicate helper `F(..., X.node(), ...)` whose AST proves
      // that returning a truthy/non-empty value implies `param` is a constant.
      for (unsigned i = 0; i < ce->getNumArgs(); ++i) {
        const auto* nmce = llvm::dyn_cast_or_null<clang::CXXMemberCallExpr>(
            UnwrapExpr(ce->getArg(i)));
        if (!nmce || SafeDeclName(nmce->getMethodDecl()) != "node") continue;
        auto [k, lbl] = ExtractMaglevInputKey(nmce->getImplicitObjectArgument(),
                                              in_binds, subs);
        if (!k.empty() && FuncTruthyImpliesConstantNode(callee, i)) {
          out_keys.insert(k);
        }
      }
    }
  }

  bool FuncAffectsMaglevConstraints(const clang::FunctionDecl* fn,
                                    int depth = 0) {
    if (!fn || depth > 5) return false;
    const clang::FunctionDecl* body_decl = nullptr;
    if (!fn->hasBody(body_decl) || !body_decl->getBody()) return false;
    const clang::FunctionDecl* canon = body_decl->getCanonicalDecl();
    auto cit = constraint_fn_cache_.find(canon);
    if (cit != constraint_fn_cache_.end()) return cit->second;
    if (!active_constraint_fns_.insert(canon).second) return false;
    bool res = StmtAffectsMaglevConstraints(body_decl->getBody(), depth + 1);
    active_constraint_fns_.erase(canon);
    if (active_constraint_fns_.empty()) {
      constraint_fn_cache_[canon] = res;
    }
    return res;
  }

  bool StmtAffectsMaglevConstraints(const clang::Stmt* stmt, int depth = 0) {
    if (!stmt || depth > 6) return false;
    class Finder : public clang::RecursiveASTVisitor<Finder> {
     public:
      RegmoleAnalyzer& self;
      int cur_depth;
      bool found = false;
      Finder(RegmoleAnalyzer& s, int d) : self(s), cur_depth(d) {}
      bool VisitCallExpr(const clang::CallExpr* ce) {
        if (const auto* oce = llvm::dyn_cast<clang::CXXOperatorCallExpr>(ce)) {
          if (oce->getOperator() == clang::OO_Call) {
            found = true;
            return false;
          }
        }
        if (const clang::FunctionDecl* fd = ce->getDirectCallee()) {
          llvm::StringRef n = SafeDeclName(fd);
          if (n == "SetUnallocated" || n == "set_temporaries_needed" ||
              n == "set_double_temporaries_needed" ||
              n == "RequireSpecificTemporary" ||
              n == "RequireSpecificDoubleTemporary") {
            found = true;
            return false;
          }
          if (const auto* mce = llvm::dyn_cast<clang::CXXMemberCallExpr>(ce)) {
            if (const clang::Expr* obj = mce->getImplicitObjectArgument()) {
              if (const auto* inner_mce =
                      llvm::dyn_cast_or_null<clang::CXXMemberCallExpr>(
                          UnwrapExpr(obj))) {
                if (const auto* inner_md = inner_mce->getMethodDecl()) {
                  if (SafeDeclName(inner_md) == "node") return true;
                }
              }
            }
          }
          if (self.FuncAffectsMaglevConstraints(fd, cur_depth)) {
            found = true;
            return false;
          }
        }
        return true;
      }
    } f(*this, depth);
    f.TraverseStmt(const_cast<clang::Stmt*>(stmt));
    return f.found;
  }

  struct SymbolicTempOutcome {
    std::vector<MaglevCondAtom> extra_conds;
    int value = 0;
    bool unknown = false;
  };

  std::vector<SymbolicTempOutcome> EvalSymbolicTempCount(
      const clang::Expr* expr, const ExprSubMap& subs, int depth = 0) {
    if (!expr || depth > 4) {
      return {{{}, 0, true}};
    }
    const clang::Expr* cur = UnwrapExpr(expr);
    if (!cur) return {{{}, 0, true}};

    if (auto iv = TryEvalConstInt(cur, subs)) {
      return {{{}, static_cast<int>(*iv), false}};
    }
    if (const auto* dre = llvm::dyn_cast<clang::DeclRefExpr>(cur)) {
      auto it = subs.find(CanonValueDecl(dre->getDecl()));
      if (it != subs.end() && it->second && it->second != cur) {
        return EvalSymbolicTempCount(it->second, subs, depth + 1);
      }
    }
    if (const auto* cond_op = llvm::dyn_cast<clang::ConditionalOperator>(cur)) {
      std::vector<SymbolicTempOutcome> out;
      auto t_branches = SplitCondBranches(cond_op->getCond(), true, subs);
      auto t_vals =
          EvalSymbolicTempCount(cond_op->getTrueExpr(), subs, depth + 1);
      for (const auto& cb : t_branches) {
        for (const auto& tv : t_vals) {
          if (!CondAtomListsCompatible(cb, tv.extra_conds)) continue;
          SymbolicTempOutcome o = tv;
          o.extra_conds.insert(o.extra_conds.end(), cb.begin(), cb.end());
          out.push_back(std::move(o));
        }
      }
      auto f_branches = SplitCondBranches(cond_op->getCond(), false, subs);
      auto f_vals =
          EvalSymbolicTempCount(cond_op->getFalseExpr(), subs, depth + 1);
      for (const auto& cb : f_branches) {
        for (const auto& fv : f_vals) {
          if (!CondAtomListsCompatible(cb, fv.extra_conds)) continue;
          SymbolicTempOutcome o = fv;
          o.extra_conds.insert(o.extra_conds.end(), cb.begin(), cb.end());
          out.push_back(std::move(o));
        }
      }
      if (!out.empty() && out.size() <= 8) return out;
      return {{{}, 0, true}};
    }
    if (const auto* bo = llvm::dyn_cast<clang::BinaryOperator>(cur)) {
      if (bo->getOpcode() == clang::BO_Add) {
        auto lv = EvalSymbolicTempCount(bo->getLHS(), subs, depth + 1);
        auto rv = EvalSymbolicTempCount(bo->getRHS(), subs, depth + 1);
        std::vector<SymbolicTempOutcome> out;
        for (const auto& l : lv) {
          for (const auto& r : rv) {
            if (!CondAtomListsCompatible(l.extra_conds, r.extra_conds))
              continue;
            SymbolicTempOutcome o;
            o.extra_conds = l.extra_conds;
            o.extra_conds.insert(o.extra_conds.end(), r.extra_conds.begin(),
                                 r.extra_conds.end());
            o.unknown = l.unknown || r.unknown;
            o.value = l.value + r.value;
            out.push_back(std::move(o));
          }
        }
        if (!out.empty() && out.size() <= 8) return out;
        return {{{}, 0, true}};
      }
    }
    if (const auto* ce = llvm::dyn_cast<clang::CallExpr>(cur)) {
      if (const clang::FunctionDecl* callee = ce->getDirectCallee()) {
        if (SafeDeclName(callee) == "max" && ce->getNumArgs() == 2) {
          auto lv = EvalSymbolicTempCount(ce->getArg(0), subs, depth + 1);
          auto rv = EvalSymbolicTempCount(ce->getArg(1), subs, depth + 1);
          std::vector<SymbolicTempOutcome> out;
          for (const auto& l : lv) {
            for (const auto& r : rv) {
              if (!CondAtomListsCompatible(l.extra_conds, r.extra_conds))
                continue;
              SymbolicTempOutcome o;
              o.extra_conds = l.extra_conds;
              o.extra_conds.insert(o.extra_conds.end(), r.extra_conds.begin(),
                                   r.extra_conds.end());
              o.unknown = l.unknown || r.unknown;
              o.value = std::max(l.value, r.value);
              out.push_back(std::move(o));
            }
          }
          if (!out.empty() && out.size() <= 8) return out;
          return {{{}, 0, true}};
        }
        if (const clang::Expr* ret_expr = GetSingleReturnExpr(callee)) {
          ExprSubMap child_subs = subs;
          unsigned np =
              std::min<unsigned>(callee->getNumParams(), ce->getNumArgs());
          for (unsigned i = 0; i < np; ++i) {
            child_subs[callee->getParamDecl(i)->getCanonicalDecl()] =
                UnwrapExpr(ce->getArg(i));
          }
          return EvalSymbolicTempCount(ret_expr, child_subs, depth + 1);
        }
      }
    }
    return {{{}, 0, true}};
  }

  struct ConstraintExecState {
    MaglevConstraintPath path;
    ExprSubMap expr_subs;
    InputBindingMap input_bindings;
    std::map<const clang::ValueDecl*, const clang::LambdaExpr*> local_lambdas;
    std::set<std::string> known_constant_inputs;
    bool returned = false;
  };

  void ExecConstraintStmt(const clang::Stmt* stmt,
                          std::vector<ConstraintExecState>& states,
                          std::vector<ConstraintExecState>& done_states,
                          int depth) {
    if (!stmt || states.empty() || depth > 8) return;
    constexpr size_t kMaxConstraintPaths = 64;

    if (const auto* cs = llvm::dyn_cast<clang::CompoundStmt>(stmt)) {
      for (const clang::Stmt* child : cs->body()) {
        ExecConstraintStmt(child, states, done_states, depth);
        if (states.empty()) break;
      }
      return;
    }

    if (const auto* ds = llvm::dyn_cast<clang::DeclStmt>(stmt)) {
      for (const clang::Decl* d : ds->decls()) {
        const auto* vd = llvm::dyn_cast<clang::VarDecl>(d);
        if (!vd || !vd->hasInit()) continue;
        const clang::ValueDecl* cvd = vd->getCanonicalDecl();
        const clang::Expr* init = UnwrapExpr(vd->getInit());
        if (const auto* lam = llvm::dyn_cast_or_null<clang::LambdaExpr>(init)) {
          for (auto& st : states) st.local_lambdas[cvd] = lam;
        } else if (IsMaglevInputType(vd->getType())) {
          for (auto& st : states) {
            auto key =
                ExtractMaglevInputKey(init, st.input_bindings, st.expr_subs);
            if (!key.first.empty()) st.input_bindings[cvd] = key;
          }
        } else {
          for (auto& st : states) {
            st.expr_subs[cvd] = vd->getInit();
          }
        }
      }
      return;
    }

    if (llvm::isa<clang::ReturnStmt>(stmt)) {
      for (auto& st : states) {
        st.returned = true;
        done_states.push_back(std::move(st));
      }
      states.clear();
      return;
    }

    if (const auto* ifs = llvm::dyn_cast<clang::IfStmt>(stmt)) {
      if (ifs->getInit()) {
        ExecConstraintStmt(ifs->getInit(), states, done_states, depth);
      }
      if (const clang::VarDecl* cv = ifs->getConditionVariable()) {
        if (cv->hasInit()) {
          for (auto& st : states) {
            st.expr_subs[cv->getCanonicalDecl()] = cv->getInit();
          }
        }
      }
      // Check compile-time constant condition even if we wouldn't otherwise
      // split, so dead arch branches are pruned.
      const clang::Expr* cond = ifs->getCond();
      if (cond && !cond->isValueDependent()) {
        bool bval = false;
        if (cond->EvaluateAsBooleanCondition(bval, ast_ctx_)) {
          if (bval) {
            ExecConstraintStmt(ifs->getThen(), states, done_states, depth);
          } else if (ifs->getElse()) {
            ExecConstraintStmt(ifs->getElse(), states, done_states, depth);
          }
          return;
        }
      }
      if (!StmtAffectsMaglevConstraints(ifs)) return;

      std::vector<ConstraintExecState> then_states;
      std::vector<ConstraintExecState> else_states;
      for (const auto& st : states) {
        auto t_brs = SplitCondBranches(cond, true, st.expr_subs);
        for (const auto& br : t_brs) {
          if (!CondAtomListsCompatible(st.path.conds, br)) continue;
          ConstraintExecState ns = st;
          for (const auto& atom : br) {
            if (std::find(ns.path.conds.begin(), ns.path.conds.end(), atom) ==
                ns.path.conds.end()) {
              ns.path.conds.push_back(atom);
            }
          }
          CollectGuaranteedConstantInputs(cond, ns.input_bindings, ns.expr_subs,
                                          ns.known_constant_inputs);
          then_states.push_back(std::move(ns));
          if (then_states.size() >= kMaxConstraintPaths) break;
        }
        auto f_brs = SplitCondBranches(cond, false, st.expr_subs);
        for (const auto& br : f_brs) {
          if (!CondAtomListsCompatible(st.path.conds, br)) continue;
          ConstraintExecState ns = st;
          for (const auto& atom : br) {
            if (std::find(ns.path.conds.begin(), ns.path.conds.end(), atom) ==
                ns.path.conds.end()) {
              ns.path.conds.push_back(atom);
            }
          }
          else_states.push_back(std::move(ns));
          if (else_states.size() >= kMaxConstraintPaths) break;
        }
      }
      ExecConstraintStmt(ifs->getThen(), then_states, done_states, depth);
      if (ifs->getElse()) {
        ExecConstraintStmt(ifs->getElse(), else_states, done_states, depth);
      }
      states.clear();
      for (auto& s : then_states) {
        if (states.size() < kMaxConstraintPaths) states.push_back(std::move(s));
      }
      for (auto& s : else_states) {
        if (states.size() < kMaxConstraintPaths) states.push_back(std::move(s));
      }
      return;
    }

    if (const auto* sw = llvm::dyn_cast<clang::SwitchStmt>(stmt)) {
      if (!StmtAffectsMaglevConstraints(sw)) return;
      if (sw->getInit()) {
        ExecConstraintStmt(sw->getInit(), states, done_states, depth);
      }
      const auto* body =
          llvm::dyn_cast_or_null<clang::CompoundStmt>(sw->getBody());
      if (!body) return;
      // Partition body statements into switch case groups.
      struct CaseGroup {
        bool is_default = false;
        std::vector<int64_t> case_vals;
        std::vector<const clang::Stmt*> stmts;
      };
      std::vector<CaseGroup> groups;
      std::vector<int64_t> all_case_vals;
      for (const clang::Stmt* child : body->body()) {
        if (llvm::isa<clang::CaseStmt>(child) ||
            llvm::isa<clang::DefaultStmt>(child)) {
          CaseGroup cg;
          const clang::Stmt* cur = child;
          while (llvm::isa<clang::CaseStmt>(cur) ||
                 llvm::isa<clang::DefaultStmt>(cur)) {
            if (const auto* cs_node = llvm::dyn_cast<clang::CaseStmt>(cur)) {
              ExprSubMap empty_subs;
              if (auto iv = TryEvalConstInt(cs_node->getLHS(), empty_subs)) {
                cg.case_vals.push_back(*iv);
                all_case_vals.push_back(*iv);
              }
              cur = cs_node->getSubStmt();
            } else {
              cg.is_default = true;
              cur = llvm::cast<clang::DefaultStmt>(cur)->getSubStmt();
            }
          }
          if (cur) cg.stmts.push_back(cur);
          groups.push_back(std::move(cg));
        } else if (!groups.empty()) {
          groups.back().stmts.push_back(child);
        }
      }
      std::vector<ConstraintExecState> next_states;
      bool has_default = false;
      for (const CaseGroup& cg : groups) {
        if (cg.is_default) has_default = true;
        std::vector<ConstraintExecState> cg_states;
        for (const auto& st : states) {
          std::string cond_str = CanonicalizeExpr(sw->getCond(), st.expr_subs);
          if (cg.is_default) {
            ConstraintExecState ns = st;
            bool ok = true;
            for (int64_t v : all_case_vals) {
              MaglevCondAtom atom{cond_str + " == " + std::to_string(v), false};
              if (!CondAtomListsCompatible(ns.path.conds, {atom})) {
                ok = false;
                break;
              }
              ns.path.conds.push_back(atom);
            }
            if (ok) cg_states.push_back(std::move(ns));
          } else if (cg.case_vals.size() == 1) {
            MaglevCondAtom atom{
                cond_str + " == " + std::to_string(cg.case_vals[0]), true};
            if (CondAtomListsCompatible(st.path.conds, {atom})) {
              ConstraintExecState ns = st;
              ns.path.conds.push_back(atom);
              cg_states.push_back(std::move(ns));
            }
          } else {
            cg_states.push_back(st);
          }
        }
        for (const clang::Stmt* s : cg.stmts) {
          if (llvm::isa<clang::BreakStmt>(s)) break;
          ExecConstraintStmt(s, cg_states, done_states, depth);
        }
        for (auto& s : cg_states) {
          if (next_states.size() < kMaxConstraintPaths) {
            next_states.push_back(std::move(s));
          }
        }
      }
      if (!has_default) {
        for (const auto& st : states) {
          if (next_states.size() < kMaxConstraintPaths) {
            next_states.push_back(st);
          }
        }
      }
      states = std::move(next_states);
      return;
    }

    if (const auto* rfor = llvm::dyn_cast<clang::CXXForRangeStmt>(stmt)) {
      if (const clang::VarDecl* lv = rfor->getLoopVariable()) {
        if (IsMaglevInputType(lv->getType())) {
          for (auto& st : states) {
            st.input_bindings[lv->getCanonicalDecl()] = {"var:*", "input"};
          }
        }
      }
      ExecConstraintStmt(rfor->getBody(), states, done_states, depth);
      return;
    }
    if (const auto* fstmt = llvm::dyn_cast<clang::ForStmt>(stmt)) {
      ExecConstraintStmt(fstmt->getBody(), states, done_states, depth);
      return;
    }
    if (const auto* wstmt = llvm::dyn_cast<clang::WhileStmt>(stmt)) {
      ExecConstraintStmt(wstmt->getBody(), states, done_states, depth);
      return;
    }

    if (const auto* ce = llvm::dyn_cast<clang::CallExpr>(stmt)) {
      // Check local lambda invocation: `use_index(StartIndexInput(), ...)`!
      if (const auto* oce = llvm::dyn_cast<clang::CXXOperatorCallExpr>(ce)) {
        if (oce->getOperator() == clang::OO_Call && oce->getNumArgs() >= 1) {
          const clang::Expr* obj = UnwrapExpr(oce->getArg(0));
          if (const auto* dre =
                  llvm::dyn_cast_or_null<clang::DeclRefExpr>(obj)) {
            const clang::ValueDecl* ld = CanonValueDecl(dre->getDecl());
            std::vector<ConstraintExecState> next_states;
            bool inlined = false;
            for (auto& st : states) {
              auto lit = st.local_lambdas.find(ld);
              if (lit == st.local_lambdas.end() || !lit->second) {
                next_states.push_back(std::move(st));
                continue;
              }
              inlined = true;
              const clang::CXXMethodDecl* call_op =
                  lit->second->getCallOperator();
              if (!call_op || !call_op->getBody()) {
                next_states.push_back(std::move(st));
                continue;
              }
              std::vector<ConstraintExecState> sub_states;
              std::vector<ConstraintExecState> sub_done;
              ConstraintExecState child = st;
              unsigned np = std::min<unsigned>(call_op->getNumParams(),
                                               oce->getNumArgs() - 1);
              for (unsigned i = 0; i < np; ++i) {
                const clang::ParmVarDecl* p = call_op->getParamDecl(i);
                const clang::ValueDecl* cp = p->getCanonicalDecl();
                const clang::Expr* arg = oce->getArg(i + 1);
                if (IsMaglevInputType(p->getType())) {
                  auto k = ExtractMaglevInputKey(arg, st.input_bindings,
                                                 st.expr_subs);
                  if (!k.first.empty()) child.input_bindings[cp] = k;
                } else {
                  child.expr_subs[cp] = arg;
                }
              }
              sub_states.push_back(std::move(child));
              ExecConstraintStmt(call_op->getBody(), sub_states, sub_done,
                                 depth + 1);
              for (auto& ds_st : sub_done) {
                ds_st.returned = false;
                ds_st.expr_subs = st.expr_subs;
                ds_st.input_bindings = st.input_bindings;
                if (next_states.size() < kMaxConstraintPaths) {
                  next_states.push_back(std::move(ds_st));
                }
              }
              for (auto& rem : sub_states) {
                rem.expr_subs = st.expr_subs;
                rem.input_bindings = st.input_bindings;
                if (next_states.size() < kMaxConstraintPaths) {
                  next_states.push_back(std::move(rem));
                }
              }
            }
            if (inlined) {
              states = std::move(next_states);
              return;
            }
          }
        }
      }

      const clang::FunctionDecl* callee = ce->getDirectCallee();
      if (!callee) return;
      llvm::StringRef cname = SafeDeclName(callee);
      if (callee->isNoReturn() || cname == "V8_Fatal" ||
          cname == "UNREACHABLE" || cname == "FatalNoSecurityImpact") {
        states.clear();
        return;
      }

      auto eval_reg_arg = [&](const clang::Expr* arg, const ExprSubMap& subs,
                              bool* out_is_double) -> int {
        for (int hop = 0; arg && hop < 6; ++hop) {
          const clang::Expr* cur = UnwrapExpr(arg);
          const auto* dre = llvm::dyn_cast_or_null<clang::DeclRefExpr>(cur);
          if (!dre) break;
          auto it = subs.find(CanonValueDecl(dre->getDecl()));
          if (it == subs.end() || !it->second || it->second == arg) break;
          arg = it->second;
        }
        if (IsDoubleRegType(arg->getType())) {
          if (out_is_double) *out_is_double = true;
          if (auto c = EvaluateDoubleRegisterCode(arg, ast_ctx_, arch_)) {
            return *c;
          }
          return -1;
        }
        if (out_is_double) *out_is_double = false;
        if (auto c = EvaluateRegisterCode(arg, ast_ctx_, arch_)) {
          return *c;
        }
        return -1;
      };

      auto eval_fixed_reg_index = [&](const clang::Expr* idx_arg, bool is_fp,
                                      const ExprSubMap& subs) -> int {
        const clang::Expr* cur = UnwrapExpr(idx_arg);
        for (int hop = 0; cur && hop < 6; ++hop) {
          const auto* dre = llvm::dyn_cast<clang::DeclRefExpr>(cur);
          if (!dre) break;
          auto it = subs.find(CanonValueDecl(dre->getDecl()));
          if (it == subs.end() || !it->second || it->second == cur) break;
          cur = UnwrapExpr(it->second);
        }
        if (const auto* code_mce =
                llvm::dyn_cast_or_null<clang::CXXMemberCallExpr>(cur)) {
          if (code_mce->getNumArgs() == 0 && code_mce->getMethodDecl() &&
              SafeDeclName(code_mce->getMethodDecl()) == "code") {
            bool dummy_is_dbl = is_fp;
            return eval_reg_arg(code_mce->getImplicitObjectArgument(), subs,
                                &dummy_is_dbl);
          }
        }
        if (auto iv = TryEvalConstInt(idx_arg, subs)) {
          int max_r = is_fp ? arch_.num_fp_regs : arch_.num_gprs;
          if (*iv >= 0 && *iv < max_r) return static_cast<int>(*iv);
        }
        return -1;
      };

      if (cname == "SetUnallocated") {
        if (const auto* mce = llvm::dyn_cast<clang::CXXMemberCallExpr>(ce)) {
          const clang::Expr* recv = mce->getImplicitObjectArgument();
          unsigned nargs = mce->getNumArgs();
          for (auto& st : states) {
            bool is_res = IsMaglevResultExpr(recv, st.expr_subs);
            std::string in_key;
            if (!is_res) {
              in_key =
                  ExtractMaglevInputKey(recv, st.input_bindings, st.expr_subs)
                      .first;
              if (in_key.empty()) continue;
            }

            std::string pol = nargs >= 1 ? ResolveEnumConstantName(
                                               mce->getArg(0), st.expr_subs)
                                         : "";
            bool arg0_is_enum =
                nargs >= 1 &&
                !mce->getArg(0)->getType().getNonReferenceType().isNull() &&
                mce->getArg(0)
                    ->getType()
                    .getNonReferenceType()
                    ->isEnumeralType();

            if (is_res) {
              MaglevResultConstraint rc;
              if (nargs == 2 && !arg0_is_enum) {
                // UnallocatedOperand(int virtual_register, int input_index)
                // -> SAME_AS_INPUT
                rc.kind = MaglevResultKind::kDefineSameAsInput;
                if (auto idx = TryEvalConstInt(mce->getArg(1), st.expr_subs)) {
                  rc.same_as_input_index = static_cast<int>(*idx);
                }
              } else if (pol == "MUST_HAVE_REGISTER") {
                rc.kind = MaglevResultKind::kDefineAsRegister;
              } else if (pol == "NONE") {
                rc.kind = MaglevResultKind::kDefineAsConstant;
              } else if ((pol == "FIXED_REGISTER" ||
                          pol == "FIXED_FP_REGISTER") &&
                         nargs >= 3) {
                bool is_fp = (pol == "FIXED_FP_REGISTER");
                rc.kind = MaglevResultKind::kDefineAsFixed;
                rc.is_double_fixed = is_fp;
                rc.fixed_reg =
                    eval_fixed_reg_index(mce->getArg(1), is_fp, st.expr_subs);
                if (!is_fp && rc.fixed_reg < 0) {
                  st.path.has_dynamic_fixed_gpr = true;
                }
              } else if (nargs == 3 && !arg0_is_enum) {
                // UnallocatedOperand(int reg_id, int slot_id, int vreg)
                rc.kind = MaglevResultKind::kDefineAsFixed;
                rc.is_double_fixed = false;
                rc.fixed_reg =
                    eval_fixed_reg_index(mce->getArg(0), false, st.expr_subs);
                if (rc.fixed_reg < 0) {
                  st.path.has_dynamic_fixed_gpr = true;
                }
              } else {
                rc.kind = MaglevResultKind::kCustomUnallocated;
              }
              st.path.result = rc;
            } else {
              MaglevInputConstraint ic;
              if (pol == "MUST_HAVE_REGISTER") {
                std::string life =
                    nargs == 3
                        ? ResolveEnumConstantName(mce->getArg(1), st.expr_subs)
                        : "";
                ic.kind = (life == "USED_AT_START")
                              ? MaglevInputKind::kUseAndClobberRegister
                              : MaglevInputKind::kUseRegister;
              } else if (pol == "REGISTER_OR_SLOT_OR_CONSTANT" ||
                         pol == "REGISTER_OR_SLOT" || pol == "MUST_HAVE_SLOT" ||
                         pol == "FIXED_SLOT") {
                ic.kind = MaglevInputKind::kUseAny;
                if (pol == "REGISTER_OR_SLOT_OR_CONSTANT" &&
                    st.known_constant_inputs.count(in_key)) {
                  ic.only_constant_or_reg = true;
                }
              } else if ((pol == "FIXED_REGISTER" ||
                          pol == "FIXED_FP_REGISTER") &&
                         nargs >= 3) {
                bool is_fp = (pol == "FIXED_FP_REGISTER");
                std::string life =
                    nargs >= 4
                        ? ResolveEnumConstantName(mce->getArg(2), st.expr_subs)
                        : "";
                ic.kind = (life == "USED_AT_START")
                              ? MaglevInputKind::kUseAndClobberFixed
                              : MaglevInputKind::kUseFixed;
                ic.is_double_fixed = is_fp;
                ic.fixed_reg =
                    eval_fixed_reg_index(mce->getArg(1), is_fp, st.expr_subs);
                if (ic.kind == MaglevInputKind::kUseAndClobberFixed && !is_fp &&
                    ic.fixed_reg < 0) {
                  st.path.has_dynamic_fixed_gpr = true;
                }
              } else if (nargs == 3 && !arg0_is_enum) {
                ic.kind = MaglevInputKind::kUseFixed;
                ic.is_double_fixed = false;
                ic.fixed_reg =
                    eval_fixed_reg_index(mce->getArg(0), false, st.expr_subs);
              }
              if (ic.kind != MaglevInputKind::kNone) {
                st.path.inputs[in_key] = ic;
              }
            }
          }
          return;
        }
      }
      if ((cname == "set_temporaries_needed" ||
           cname == "set_double_temporaries_needed") &&
          ce->getNumArgs() >= 1) {
        bool is_double = (cname == "set_double_temporaries_needed");
        std::vector<ConstraintExecState> next_states;
        for (const auto& st : states) {
          auto outcomes = EvalSymbolicTempCount(ce->getArg(0), st.expr_subs);
          for (const auto& o : outcomes) {
            if (!CondAtomListsCompatible(st.path.conds, o.extra_conds))
              continue;
            ConstraintExecState ns = st;
            for (const auto& atom : o.extra_conds) {
              if (std::find(ns.path.conds.begin(), ns.path.conds.end(), atom) ==
                  ns.path.conds.end()) {
                ns.path.conds.push_back(atom);
              }
            }
            if (is_double) {
              if (o.unknown)
                ns.path.double_temps_unknown = true;
              else
                ns.path.double_temps = o.value;
            } else {
              if (o.unknown)
                ns.path.gpr_temps_unknown = true;
              else
                ns.path.gpr_temps = o.value;
            }
            if (next_states.size() < kMaxConstraintPaths) {
              next_states.push_back(std::move(ns));
            }
          }
        }
        states = std::move(next_states);
        return;
      }
      if (cname == "RequireSpecificTemporary" && ce->getNumArgs() >= 1) {
        for (auto& st : states) {
          bool is_dbl = false;
          int r = eval_reg_arg(ce->getArg(0), st.expr_subs, &is_dbl);
          if (!is_dbl && r >= 0 && r < arch_.num_gprs) {
            st.path.fixed_gpr_temps |= (1u << r);
          } else {
            st.path.gpr_temps += 1;
            st.path.has_dynamic_fixed_gpr = true;
          }
        }
        return;
      }
      if (cname == "RequireSpecificDoubleTemporary" && ce->getNumArgs() >= 1) {
        for (auto& st : states) {
          bool is_dbl = true;
          int r = eval_reg_arg(ce->getArg(0), st.expr_subs, &is_dbl);
          if (r >= 0 && r < 32) {
            st.path.fixed_double_temps |= (1u << r);
          } else {
            st.path.double_temps += 1;
          }
        }
        return;
      }

      // Inline helper functions and member helpers called from
      // SetValueLocationConstraints (including Use*/Define* wrappers and
      // SetValueLocationConstraintsImpl()).
      const clang::FunctionDecl* helper_body = nullptr;
      if (depth < 5 && callee->hasBody(helper_body) &&
          FuncAffectsMaglevConstraints(helper_body, depth)) {
        const auto* mce = llvm::dyn_cast<clang::CXXMemberCallExpr>(ce);
        std::vector<ConstraintExecState> next_states;
        for (auto& st : states) {
          if (mce) {
            bool recv_is_this = false;
            const clang::Expr* obj =
                UnwrapExpr(mce->getImplicitObjectArgument());
            for (int hop = 0; obj && hop < 6; ++hop) {
              if (llvm::isa<clang::CXXThisExpr>(obj)) {
                recv_is_this = true;
                break;
              }
              if (const auto* dre = llvm::dyn_cast<clang::DeclRefExpr>(obj)) {
                auto it = st.expr_subs.find(CanonValueDecl(dre->getDecl()));
                if (it != st.expr_subs.end() && it->second &&
                    it->second != obj) {
                  obj = UnwrapExpr(it->second);
                  continue;
                }
              }
              break;
            }
            if (!recv_is_this) {
              if (next_states.size() < kMaxConstraintPaths) {
                next_states.push_back(std::move(st));
              }
              continue;
            }
          }
          std::vector<ConstraintExecState> sub_states;
          std::vector<ConstraintExecState> sub_done;
          ConstraintExecState child = st;
          unsigned np =
              std::min<unsigned>(callee->getNumParams(), ce->getNumArgs());
          for (unsigned i = 0; i < np; ++i) {
            const clang::ParmVarDecl* p = callee->getParamDecl(i);
            const clang::ValueDecl* cp = p->getCanonicalDecl();
            if (IsMaglevInputType(p->getType())) {
              auto k = ExtractMaglevInputKey(ce->getArg(i), st.input_bindings,
                                             st.expr_subs);
              if (!k.first.empty()) child.input_bindings[cp] = k;
            } else {
              child.expr_subs[cp] = ce->getArg(i);
            }
          }
          sub_states.push_back(std::move(child));
          ExecConstraintStmt(helper_body->getBody(), sub_states, sub_done,
                             depth + 1);
          for (auto& ds_st : sub_done) {
            ds_st.returned = false;
            ds_st.expr_subs = st.expr_subs;
            ds_st.input_bindings = st.input_bindings;
            if (next_states.size() < kMaxConstraintPaths) {
              next_states.push_back(std::move(ds_st));
            }
          }
          for (auto& rem : sub_states) {
            rem.expr_subs = st.expr_subs;
            rem.input_bindings = st.input_bindings;
            if (next_states.size() < kMaxConstraintPaths) {
              next_states.push_back(std::move(rem));
            }
          }
        }
        states = std::move(next_states);
        return;
      }
      return;
    }

    // Descend into expression wrappers (e.g., ExprWithCleanups, etc.)
    for (const clang::Stmt* child : stmt->children()) {
      if (child) ExecConstraintStmt(child, states, done_states, depth);
    }
  }

  const clang::FunctionDecl* FindSetValueLocationConstraintsBody(
      const clang::CXXRecordDecl* cls) {
    if (!cls) return nullptr;
    const clang::CXXRecordDecl* def = cls->getDefinition();
    if (!def) return nullptr;
    for (const clang::CXXMethodDecl* m : def->methods()) {
      if (SafeDeclName(m) != "SetValueLocationConstraints") continue;
      const clang::FunctionDecl* body_decl = nullptr;
      if (m->hasBody(body_decl)) return body_decl;
    }
    for (const clang::CXXBaseSpecifier& base : def->bases()) {
      if (const auto* br = base.getType()->getAsCXXRecordDecl()) {
        if (const clang::FunctionDecl* b =
                FindSetValueLocationConstraintsBody(br)) {
          return b;
        }
      }
    }
    return nullptr;
  }

  const MaglevNodeSpec& GetMaglevNodeSpec(const clang::CXXRecordDecl* cls) {
    const clang::CXXRecordDecl* canon = cls->getCanonicalDecl();
    auto it = maglev_spec_cache_.find(canon);
    if (it != maglev_spec_cache_.end()) return it->second;

    MaglevNodeSpec spec;
    ExtractMaglevOpProperties(cls, spec);
    ExtractMaglevInputReprs(cls, spec);

    if (const clang::FunctionDecl* body_decl =
            FindSetValueLocationConstraintsBody(cls)) {
      spec.has_constraints_body = true;
      std::vector<ConstraintExecState> states(1);
      std::vector<ConstraintExecState> done_states;
      ExecConstraintStmt(body_decl->getBody(), states, done_states, 0);
      for (auto& ds : done_states) {
        spec.paths.push_back(std::move(ds.path));
      }
      for (auto& rem : states) {
        spec.paths.push_back(std::move(rem.path));
      }
    } else if (!imported_maglev_specs_.empty()) {
      auto imp_it = imported_maglev_specs_.find(spec.cls_name);
      if (imp_it != imported_maglev_specs_.end()) {
        spec.has_constraints_body = imp_it->second.has_constraints_body;
        spec.paths = imp_it->second.paths;
        if (spec.input_reprs.empty()) {
          spec.input_reprs = imp_it->second.input_reprs;
        }
        if (spec.input_key_indices.empty()) {
          spec.input_key_indices = imp_it->second.input_key_indices;
        }
      }
    }

    maglev_spec_cache_[canon] = std::move(spec);
    return maglev_spec_cache_[canon];
  }

  // TODO(585533636): Prune incompatible constraint paths backwards from
  // regalloc-state checks in GenerateCode (e.g. !input.operand().IsRegister()
  // or input.operand().IsStackSlot() rules out UseRegister/UseFixed paths,
  // correlating resources like set_temporaries_needed reserved only on a
  // UseAny branch when GenerateCode branches on operand state instead of
  // re-checking the predicate).
  std::vector<const MaglevConstraintPath*> GetCompatibleMaglevPaths(
      const MaglevNodeSpec& spec, const Env& env) {
    std::vector<const MaglevConstraintPath*> out;
    for (const MaglevConstraintPath& p : spec.paths) {
      if (CondAtomListsCompatible(env.active_conds, p.conds)) {
        out.push_back(&p);
      }
    }
    return out;
  }

  // --------------------------------------------------------------------------
  // Maglev Register Allocator Verification Checks (Checks 1-6)
  // --------------------------------------------------------------------------

  struct MaglevToRegMatch {
    bool is_result = false;
    bool is_double = false;
    std::string input_key;
    std::string input_label;
  };

  std::optional<MaglevToRegMatch> MatchMaglevToRegisterCall(
      const clang::Expr* expr, const Env& env) {
    expr = UnwrapExpr(expr);
    const auto* ce = llvm::dyn_cast_or_null<clang::CallExpr>(expr);
    if (!ce) return std::nullopt;
    const clang::FunctionDecl* callee = ce->getDirectCallee();
    if (!callee) return std::nullopt;
    llvm::StringRef cname = SafeDeclName(callee);
    bool is_double = false;
    if (cname == "ToDoubleRegister") {
      is_double = true;
    } else if (cname != "ToRegister" && cname != "ToRegisterT") {
      return std::nullopt;
    }
    if (ce->getNumArgs() != 1) return std::nullopt;
    const clang::Expr* arg0 = ce->getArg(0);
    if (IsMaglevResultExpr(arg0, env.local_expr_bindings)) {
      MaglevToRegMatch m;
      m.is_result = true;
      m.is_double = is_double;
      return m;
    }
    auto [key, label] = ExtractMaglevInputKey(arg0, env.local_maglev_inputs,
                                              env.local_expr_bindings);
    if (!key.empty()) {
      MaglevToRegMatch m;
      m.is_result = false;
      m.is_double = is_double;
      m.input_key = std::move(key);
      m.input_label = std::move(label);
      return m;
    }
    return std::nullopt;
  }

  bool MatchMaglevTempAcquireCall(const clang::Expr* expr, bool* out_is_scratch,
                                  bool* out_is_double) {
    expr = UnwrapExpr(expr);
    const auto* mce = llvm::dyn_cast_or_null<clang::CXXMemberCallExpr>(expr);
    if (!mce) return false;
    if (GetScratchScopeKind(mce->getObjectType()) !=
        ScratchScopeKind::kMaglevTemp) {
      return false;
    }
    const clang::CXXMethodDecl* md = mce->getMethodDecl();
    if (!md) return false;
    llvm::StringRef mname = SafeDeclName(md);
    if (mname == "Acquire") {
      if (out_is_scratch) *out_is_scratch = false;
      if (out_is_double) *out_is_double = false;
      return true;
    }
    if (mname == "AcquireDouble") {
      if (out_is_scratch) *out_is_scratch = false;
      if (out_is_double) *out_is_double = true;
      return true;
    }
    if (mname == "AcquireScratch") {
      if (out_is_scratch) *out_is_scratch = true;
      if (out_is_double) *out_is_double = false;
      return true;
    }
    if (mname == "AcquireScratchDouble") {
      if (out_is_scratch) *out_is_scratch = true;
      if (out_is_double) *out_is_double = true;
      return true;
    }
    return false;
  }

  void ApplyMaglevCondBranch(const clang::Expr* cond, bool want_true,
                             Env& env) {
    if (!cond || !env.reachable) return;
    auto brs = SplitCondBranches(cond, want_true, env.local_expr_bindings);
    if (brs.empty()) {
      env.reachable = false;
      return;
    }
    std::vector<const std::vector<MaglevCondAtom>*> compat;
    for (const auto& br : brs) {
      if (CondAtomListsCompatible(env.active_conds, br)) {
        compat.push_back(&br);
      }
    }
    if (compat.empty()) {
      env.reachable = false;
      return;
    }
    auto add_atom = [&](const MaglevCondAtom& atom) {
      if (std::find(env.active_conds.begin(), env.active_conds.end(), atom) ==
          env.active_conds.end()) {
        env.active_conds.push_back(atom);
      }
    };
    if (compat.size() == 1) {
      for (const MaglevCondAtom& atom : *compat[0]) {
        add_atom(atom);
      }
    } else {
      // Push any atoms shared across all surviving disjuncts.
      for (const MaglevCondAtom& atom : *compat[0]) {
        bool in_all = true;
        for (size_t i = 1; i < compat.size(); ++i) {
          if (std::find(compat[i]->begin(), compat[i]->end(), atom) ==
              compat[i]->end()) {
            in_all = false;
            break;
          }
        }
        if (in_all) add_atom(atom);
      }
    }
    if (!CondAtomListsCompatible(env.active_conds, {})) {
      env.reachable = false;
    }
  }

  // Check 1: ToRegister / ToDoubleRegister location validity.
  void CheckMaglevToRegisterUse(const MaglevToRegMatch& m,
                                clang::SourceLocation loc, const Env& env,
                                FunctionAnalysisContext& fctx) {
    if (!g_check_maglev || !fctx.maglev_node_cls || !fctx.emit_diagnostics) {
      return;
    }
    const MaglevNodeSpec& spec = GetMaglevNodeSpec(fctx.maglev_node_cls);
    if (!spec.has_constraints_body) return;
    auto paths = GetCompatibleMaglevPaths(spec, env);
    if (paths.empty()) return;

    std::string fn_label = m.is_double ? "ToDoubleRegister" : "ToRegister";

    if (m.is_result) {
      bool all_const = true;
      bool all_none = true;
      for (const MaglevConstraintPath* p : paths) {
        if (p->result.kind != MaglevResultKind::kDefineAsConstant) {
          all_const = false;
        }
        if (p->result.kind != MaglevResultKind::kNone) {
          all_none = false;
        }
      }
      if (all_const) {
        QueueWarning(fctx, loc,
                     "Cannot call " + fn_label + "(result()) in '" +
                         spec.cls_name +
                         "::GenerateCode': SetValueLocationConstraints defines "
                         "result as constant (DefineAsConstant).");
      } else if (all_none) {
        QueueWarning(fctx, loc,
                     "Cannot call " + fn_label + "(result()) in '" +
                         spec.cls_name +
                         "::GenerateCode': SetValueLocationConstraints did not "
                         "define a register result.");
      }
      return;
    }

    bool known_not_reg =
        m.is_double ? (env.known_not_fp_inputs.count(m.input_key) > 0)
                    : (env.known_not_gpr_inputs.count(m.input_key) > 0);
    if (known_not_reg) {
      QueueWarning(fctx, loc,
                   "Cannot call " + fn_label + "(" + m.input_label + ") in '" +
                       spec.cls_name +
                       "::GenerateCode': input is known not to be in a "
                       "register on this branch.");
      return;
    }

    bool any_defined = false;
    bool any_use_any = false;
    bool all_use_any_constant_only = true;
    for (const MaglevConstraintPath* p : paths) {
      const MaglevInputConstraint* ic = LookupInputConstraint(*p, m.input_key);
      if (!ic || ic->kind == MaglevInputKind::kNone) continue;
      any_defined = true;
      if (ic->kind == MaglevInputKind::kUseAny) {
        any_use_any = true;
        if (!ic->only_constant_or_reg) {
          all_use_any_constant_only = false;
        }
      }
    }

    bool known_is_reg = m.is_double
                            ? (env.known_is_fp_inputs.count(m.input_key) > 0)
                            : (env.known_is_gpr_inputs.count(m.input_key) > 0);
    if (!known_is_reg && any_use_any && all_use_any_constant_only &&
        env.known_not_constant_inputs.count(m.input_key)) {
      // Constant nodes never receive a stack slot in Maglev regalloc
      // (RegallocValueNodeInfo::allocation returns Register or
      // ConstantOperand), so guarding !IsConstant() guarantees a register.
      known_is_reg = true;
    }

    if (!any_defined) {
      QueueWarning(fctx, loc,
                   "Input '" + m.input_label + "' used with " + fn_label +
                       "() in '" + spec.cls_name +
                       "::GenerateCode' has no location constraint in "
                       "SetValueLocationConstraints.");
    } else if (any_use_any && !known_is_reg) {
      QueueWarning(fctx, loc,
                   "Cannot call " + fn_label + "(" + m.input_label + ") in '" +
                       spec.cls_name +
                       "::GenerateCode' without checking IsRegister(): "
                       "SetValueLocationConstraints uses UseAny.");
    }
  }

  // Check 2: Path-sensitive read-only Maglev input modification / clobber.
  void CheckMaglevReadonlyInputWrite(const clang::ValueDecl* sym,
                                     const std::string& input_key,
                                     const std::string& input_label,
                                     int phys_reg,
                                     const std::string& callee_name,
                                     clang::SourceLocation loc, const Env& env,
                                     FunctionAnalysisContext& fctx) {
    if (!g_check_maglev || !fctx.maglev_node_cls || !fctx.emit_diagnostics ||
        input_key.empty() || fctx.in_maglev_diverging_deferred) {
      return;
    }
    const MaglevNodeSpec& spec = GetMaglevNodeSpec(fctx.maglev_node_cls);
    if (!spec.has_constraints_body) return;
    if (spec.is_call || spec.is_terminal_control) return;
    if (IsSymSavedOnStack(sym, phys_reg, env)) return;
    if (sym && env.maglev_result_sym &&
        (env.known_equal.contains({sym, env.maglev_result_sym}) ||
         env.known_equal.contains({env.maglev_result_sym, sym}))) {
      return;
    }

    auto paths = GetCompatibleMaglevPaths(spec, env);
    if (paths.empty()) return;

    MaglevInputKind forbid_kind = MaglevInputKind::kNone;
    for (const MaglevConstraintPath* p : paths) {
      const MaglevInputConstraint* ic = LookupInputConstraint(*p, input_key);
      if (!ic || ic->kind == MaglevInputKind::kNone) continue;
      if (ic->kind == MaglevInputKind::kUseAndClobberRegister ||
          ic->kind == MaglevInputKind::kUseAndClobberFixed) {
        continue;
      }
      if (MatchesSameAsInput(spec, input_key, *p)) {
        continue;
      }
      if (ic->kind == MaglevInputKind::kUseFixed &&
          p->result.kind == MaglevResultKind::kDefineAsFixed &&
          ic->fixed_reg >= 0 && ic->fixed_reg == p->result.fixed_reg &&
          ic->is_double_fixed == p->result.is_double_fixed) {
        continue;
      }
      forbid_kind = ic->kind;
      break;
    }

    if (forbid_kind != MaglevInputKind::kNone) {
      std::string via =
          callee_name.empty() ? "" : (" in '" + callee_name + "'");
      QueueWarning(
          fctx, loc,
          "Cannot modify/clobber Maglev input register '" + input_label +
              "' in '" + spec.cls_name + "::GenerateCode'" + via +
              " (constrained as " + MaglevInputKindToString(forbid_kind) +
              " in SetValueLocationConstraints; use UseAndClobberRegister / "
              "UseAndClobberFixed).");
    }
  }

  // Check 3: Early-clobber (reading an untouched Maglev input after result()
  // was already written, when result() and the input can alias).
  void CheckMaglevEarlyClobberOnRead(const clang::ValueDecl* sym,
                                     const std::string& input_key,
                                     const std::string& input_label,
                                     bool in_is_double, bool holds_entry_value,
                                     clang::SourceLocation use_loc,
                                     const Env& env,
                                     FunctionAnalysisContext& fctx) {
    if (!g_check_maglev || !fctx.maglev_node_cls || !fctx.emit_diagnostics ||
        input_key.empty() || !holds_entry_value) {
      return;
    }
    if (!env.maglev_result_written) return;
    if (in_is_double != env.maglev_result_is_double) return;
    if (sym && env.maglev_result_sym && sym == env.maglev_result_sym) return;
    if (sym && env.maglev_result_sym &&
        (env.known_not_equal.contains({env.maglev_result_sym, sym}) ||
         env.known_not_equal.contains({sym, env.maglev_result_sym}))) {
      return;
    }
    auto dit = env.maglev_result_distinct_inputs.find(input_key);
    if (dit != env.maglev_result_distinct_inputs.end()) {
      bool all_conds_hold = true;
      for (const MaglevCondAtom& req : dit->second) {
        MaglevCondAtom neg{req.expr, !req.truth};
        if (CondAtomListsCompatible(env.active_conds, {neg})) {
          all_conds_hold = false;
          break;
        }
      }
      if (all_conds_hold) return;
    }

    const MaglevNodeSpec& spec = GetMaglevNodeSpec(fctx.maglev_node_cls);
    if (!spec.has_constraints_body) return;
    auto paths = GetCompatibleMaglevPaths(spec, env);
    if (paths.empty()) return;

    bool can_alias = false;
    for (const MaglevConstraintPath* p : paths) {
      if (p->result.kind == MaglevResultKind::kNone ||
          p->result.kind == MaglevResultKind::kDefineAsConstant) {
        continue;
      }
      const MaglevInputConstraint* ic = LookupInputConstraint(*p, input_key);
      if (p->result.kind == MaglevResultKind::kDefineAsFixed && ic &&
          (ic->kind == MaglevInputKind::kUseFixed ||
           ic->kind == MaglevInputKind::kUseAndClobberFixed) &&
          p->result.is_double_fixed == ic->is_double_fixed &&
          p->result.fixed_reg >= 0 && ic->fixed_reg >= 0 &&
          p->result.fixed_reg != ic->fixed_reg) {
        continue;
      }
      // In Maglev regalloc, AssignFixedTemporaries runs before
      // AssignArbitraryRegisterInput and AssignAnyInput, blocking every
      // RequireSpecificTemporary(r) register from non-fixed inputs.
      if (p->result.kind == MaglevResultKind::kDefineAsFixed &&
          p->result.fixed_reg >= 0 && ic &&
          (ic->kind == MaglevInputKind::kUseRegister ||
           ic->kind == MaglevInputKind::kUseAndClobberRegister ||
           ic->kind == MaglevInputKind::kUseAny)) {
        if (!p->result.is_double_fixed &&
            p->result.fixed_reg < arch_.num_gprs &&
            (p->fixed_gpr_temps & (1u << p->result.fixed_reg))) {
          continue;
        }
        if (p->result.is_double_fixed && p->result.fixed_reg < 32 &&
            (p->fixed_double_temps & (1u << p->result.fixed_reg))) {
          continue;
        }
      }
      if (p->result.kind == MaglevResultKind::kDefineSameAsInput) {
        // Under SAME_AS_INPUT, result() == input(idx). If input(idx) was
        // requested with UseAndClobberRegister or UseAndClobberFixed, Maglev's
        // regalloc chooses an unblocked register distinct from other inputs
        // (aside from an explicit UseFixed on that same register).
        const MaglevInputConstraint* same_ic =
            LookupSameAsInputConstraint(spec, *p);
        if (same_ic &&
            (same_ic->kind == MaglevInputKind::kUseAndClobberRegister ||
             same_ic->kind == MaglevInputKind::kUseAndClobberFixed) &&
            !MatchesSameAsInput(spec, input_key, *p)) {
          continue;
        }
        if (same_ic && ic &&
            (same_ic->kind == MaglevInputKind::kUseFixed ||
             same_ic->kind == MaglevInputKind::kUseAndClobberFixed) &&
            (ic->kind == MaglevInputKind::kUseFixed ||
             ic->kind == MaglevInputKind::kUseAndClobberFixed) &&
            same_ic->is_double_fixed == ic->is_double_fixed &&
            same_ic->fixed_reg >= 0 && ic->fixed_reg >= 0 &&
            same_ic->fixed_reg != ic->fixed_reg) {
          continue;
        }
        // Under SAME_AS_INPUT, input(idx) can only alias input(k) when both
        // refer to the same ValueNode, which is impossible if their declared
        // ValueRepresentations differ.
        if (!MatchesSameAsInput(spec, input_key, *p)) {
          int target_idx = p->result.same_as_input_index;
          int same_repr = (target_idx >= 0 && static_cast<size_t>(target_idx) <
                                                  spec.input_reprs.size())
                              ? spec.input_reprs[target_idx]
                              : -1;
          int in_repr = LookupInputRepr(spec, input_key);
          if (same_repr >= 0 && in_repr >= 0 && same_repr != in_repr) {
            continue;
          }
        }
      }
      // Distinct known physical registers on this path cannot alias.
      if (sym && env.maglev_result_sym) {
        auto it_in = env.syms.find(sym);
        auto it_res = env.syms.find(env.maglev_result_sym);
        if (it_in != env.syms.end() && it_res != env.syms.end() &&
            !it_in->second.is_double_reg && !it_res->second.is_double_reg &&
            it_in->second.phys_reg >= 0 && it_res->second.phys_reg >= 0 &&
            it_in->second.phys_reg != it_res->second.phys_reg) {
          continue;
        }
      }
      can_alias = true;
      break;
    }

    if (!can_alias) return;

    // On ARM64, emit the size-sensitive `!=` diagnostic if guarded only with
    // `!=` instead of `!Aliases()` / `!AreAliased()`.
    if (sym && env.maglev_result_sym) {
      auto ss_it =
          env.size_sensitive_not_equal.find({env.maglev_result_sym, sym});
      if (ss_it != env.size_sensitive_not_equal.end()) {
        ClobberNote guard_note{
            ss_it->second,
            "size-sensitive '!=' / '==' guard here compares register width "
            "(wN != xN is true on ARM64)"};
        QueueWarning(
            fctx, use_loc,
            "Maglev result() is overwritten before reading input '" +
                input_label + "' in '" + spec.cls_name +
                "::GenerateCode' under a size-sensitive '!=' guard on ARM64; "
                "use !Aliases() or !AreAliased() instead.",
            &guard_note);
        return;
      }
    }

    ClobberNote write_note{env.maglev_result_write_loc,
                           "result() was written here"};
    QueueWarning(fctx, use_loc,
                 "Maglev input '" + input_label + "' is read in '" +
                     spec.cls_name +
                     "::GenerateCode' after result() was already written; "
                     "result() can alias this input.",
                 env.maglev_result_write_loc.isValid() ? &write_note : nullptr);
  }

  // Check 4: TemporaryRegisterScope general/double temp & scratch budgets.
  void AccountMaglevTempUse(bool is_scratch, bool is_double, int peak_delta,
                            int leaked_delta, clang::SourceLocation loc,
                            const std::string& action_desc, Env& env,
                            FunctionAnalysisContext& fctx) {
    if (peak_delta <= 0 && leaked_delta <= 0) return;

    int cur_used = 0;
    if (is_scratch) {
      cur_used = is_double ? env.maglev_double_scratches_used
                           : env.maglev_gpr_scratches_used;
    } else {
      cur_used =
          is_double ? env.maglev_double_temps_used : env.maglev_gpr_temps_used;
    }
    int peak_total = cur_used + std::max(peak_delta, leaked_delta);
    int next_used = cur_used + leaked_delta;

    // Caller-borne demand subtracts registers explicitly supplied via
    // Include() inside the current function (so helpers that supply their own
    // temps do not inflate their caller's budget).
    int caller_demand = 0;
    if (is_scratch) {
      caller_demand = std::max(0, peak_total);
    } else if (!(is_double ? env.maglev_double_temps_unbounded
                           : env.maglev_gpr_temps_unbounded) &&
               !env.maglev_temps_reset) {
      int extra_inc = is_double ? env.maglev_double_temps_included
                                : env.maglev_gpr_temps_included;
      caller_demand = std::max(0, peak_total - extra_inc);
    }

    if (is_scratch) {
      if (is_double) {
        env.maglev_double_scratches_used = next_used;
        fctx.max_maglev_double_scratches =
            std::max(fctx.max_maglev_double_scratches, caller_demand);
      } else {
        env.maglev_gpr_scratches_used = next_used;
        fctx.max_maglev_gpr_scratches =
            std::max(fctx.max_maglev_gpr_scratches, caller_demand);
      }
    } else {
      if (is_double) {
        env.maglev_double_temps_used = next_used;
        fctx.max_maglev_double_temps =
            std::max(fctx.max_maglev_double_temps, caller_demand);
      } else {
        env.maglev_gpr_temps_used = next_used;
        fctx.max_maglev_gpr_temps =
            std::max(fctx.max_maglev_gpr_temps, caller_demand);
      }
    }

    if (!g_check_maglev || !fctx.maglev_node_cls || !fctx.emit_diagnostics) {
      return;
    }

    const MaglevNodeSpec& spec = GetMaglevNodeSpec(fctx.maglev_node_cls);
    if (!spec.has_constraints_body) return;

    if (is_scratch) {
      int budget = is_double ? arch_.maglev_fp_scratch_budget
                             : arch_.maglev_gpr_scratch_budget;
      if (peak_total > budget) {
        QueueWarning(fctx, loc,
                     action_desc + " in '" + spec.cls_name +
                         "::GenerateCode' requires " +
                         std::to_string(peak_total) + " simultaneous " +
                         (is_double ? "double " : "general ") +
                         "scratch register(s), exceeding the architecture "
                         "scratch budget (" +
                         std::to_string(budget) + ").");
      }
      return;
    }

    // TODO(585533636): Model direct queries of temporary/regalloc state in
    // GenerateCode (such as temps.Available() and regalloc_info()).
    if (is_double ? env.maglev_double_temps_unbounded
                  : env.maglev_gpr_temps_unbounded) {
      return;
    }
    int extra_included = is_double ? env.maglev_double_temps_included
                                   : env.maglev_gpr_temps_included;
    if (env.maglev_temps_reset) {
      if (peak_total > extra_included) {
        QueueWarning(
            fctx, loc,
            action_desc + " in '" + spec.cls_name +
                "::GenerateCode' acquires " + std::to_string(peak_total) + " " +
                (is_double ? "double " : "general ") +
                "temporary register(s) after ResetToDefault() with only " +
                std::to_string(extra_included) + " explicitly included.");
      }
      return;
    }

    auto paths = GetCompatibleMaglevPaths(spec, env);
    if (paths.empty()) return;

    int min_budget = std::numeric_limits<int>::max();
    for (const MaglevConstraintPath* p : paths) {
      if (is_double ? p->double_temps_unknown : p->gpr_temps_unknown) {
        return;
      }
      int fixed_cnt = is_double ? llvm::popcount(p->fixed_double_temps)
                                : llvm::popcount(p->fixed_gpr_temps);
      int base = (is_double ? p->double_temps : p->gpr_temps) + fixed_cnt;
      min_budget = std::min(min_budget, base + extra_included);
    }
    if (min_budget != std::numeric_limits<int>::max() &&
        peak_total > min_budget) {
      QueueWarning(
          fctx, loc,
          action_desc + " in '" + spec.cls_name + "::GenerateCode' requires " +
              std::to_string(peak_total) + " " +
              (is_double ? "double " : "general ") +
              "temporary register(s), but SetValueLocationConstraints only "
              "reserved " +
              std::to_string(min_budget) + ".");
    }
  }

  // Check 5: Unrequested physical allocatable GPR write / clobber.
  void CheckMaglevPhysWrite(int r, clang::SourceLocation loc,
                            const std::string& reason, const Env& env,
                            FunctionAnalysisContext& fctx) {
    if (!g_check_maglev || !fctx.maglev_node_cls || !fctx.emit_diagnostics ||
        fctx.in_maglev_diverging_deferred) {
      return;
    }
    if (r < 0 || r >= arch_.num_gprs) return;
    if (!(arch_.maglev_allocatable_mask & (1u << r))) return;

    const MaglevNodeSpec& spec = GetMaglevNodeSpec(fctx.maglev_node_cls);
    if (!spec.has_constraints_body) return;
    if (spec.is_call || spec.is_terminal_control) return;
    if (env.maglev_save_call_depth > 0) return;
    if (IsSymSavedOnStack(nullptr, r, env)) return;

    auto paths = GetCompatibleMaglevPaths(spec, env);
    if (paths.empty()) return;

    bool allowed_all = true;
    for (const MaglevConstraintPath* p : paths) {
      if (p->has_dynamic_fixed_gpr) continue;
      if (p->fixed_gpr_temps & (1u << r)) continue;
      if (p->result.kind == MaglevResultKind::kDefineAsFixed &&
          !p->result.is_double_fixed &&
          (p->result.fixed_reg == r || p->result.fixed_reg < 0)) {
        continue;
      }
      bool allowed_by_input = false;
      for (const auto& [k, ic] : p->inputs) {
        if (ic.is_double_fixed) continue;
        if (ic.kind == MaglevInputKind::kUseAndClobberFixed &&
            (ic.fixed_reg == r || ic.fixed_reg < 0)) {
          allowed_by_input = true;
          break;
        }
        if (MatchesSameAsInput(spec, k, *p) &&
            ic.kind == MaglevInputKind::kUseFixed &&
            (ic.fixed_reg == r || ic.fixed_reg < 0)) {
          allowed_by_input = true;
          break;
        }
      }
      if (!allowed_by_input) {
        allowed_all = false;
        break;
      }
    }

    if (!allowed_all) {
      QueueWarning(
          fctx, loc,
          "Physical register '" + std::string(arch_.gpr_names[r]) +
              "' is modified/clobbered (" + reason + ") in '" + spec.cls_name +
              "::GenerateCode', but SetValueLocationConstraints did not "
              "request it as a fixed output, clobbered input, or specific "
              "temporary.");
    }
  }

  // Check 6a: SaveRegisterStateForCall requires OpProperties::DeferredCall().
  void CheckMaglevSaveRegStateProperty(clang::SourceLocation loc,
                                       FunctionAnalysisContext& fctx) {
    if (!g_check_maglev || !fctx.maglev_node_cls || !fctx.emit_diagnostics) {
      return;
    }
    const MaglevNodeSpec& spec = GetMaglevNodeSpec(fctx.maglev_node_cls);
    if (!spec.has_constraints_body) return;
    if (!spec.is_deferred_call) {
      QueueWarning(
          fctx, loc,
          "SaveRegisterStateForCall used in '" + spec.cls_name +
              "::GenerateCode', but kProperties does not include "
              "OpProperties::DeferredCall() (needs_register_snapshot).");
    }
  }

  // Check 6b: Calls clobbering caller-saved registers must match OpProperties.
  void CheckMaglevCallProperty(const std::string& callee_name,
                               const FuncSummary& sum,
                               clang::SourceLocation loc, const Env& env,
                               FunctionAnalysisContext& fctx) {
    if (!g_check_maglev || !fctx.maglev_node_cls || !fctx.emit_diagnostics) {
      return;
    }
    if (sum.no_fallthrough) return;
    const MaglevNodeSpec& spec = GetMaglevNodeSpec(fctx.maglev_node_cls);
    if (!spec.has_constraints_body) return;
    if (spec.is_terminal_control) return;

    if (fctx.in_maglev_deferred) {
      if (!spec.is_deferred_call && !spec.is_call) {
        QueueWarning(
            fctx, loc,
            "Deferred call to '" + callee_name + "' in '" + spec.cls_name +
                "::GenerateCode' requires OpProperties::DeferredCall() in "
                "kProperties.");
      } else if (!fctx.in_maglev_diverging_deferred &&
                 env.maglev_save_call_depth == 0 && !spec.is_call &&
                 !IsSymSavedOnStack(nullptr, -1, env)) {
        QueueWarning(
            fctx, loc,
            "Deferred call to '" + callee_name + "' in '" + spec.cls_name +
                "::GenerateCode' clobbers caller-saved registers without "
                "SaveRegisterStateForCall.");
      }
    } else {
      if (env.maglev_save_call_depth == 0 && !spec.is_call) {
        QueueWarning(
            fctx, loc,
            "Call to '" + callee_name + "' in '" + spec.cls_name +
                "::GenerateCode' clobbers caller-saved registers outside "
                "SaveRegisterStateForCall, but kProperties does not include "
                "OpProperties::Call().");
      }
    }
  }

  // Extracts individual register expressions from `RegList{r1, r2}` or a single
  // Register/DoubleRegister argument. Returns false if the expression is a
  // dynamic/aggregate RegList (like `kAllocatableGeneralRegisters`).
  bool ExtractRegListExprs(const clang::Expr* expr,
                           std::vector<const clang::Expr*>& out_regs) {
    const clang::Expr* cur = UnwrapExpr(expr);
    if (!cur) return false;
    if (IsMaglevTrackableRegType(cur->getType())) {
      out_regs.push_back(expr);
      return true;
    }
    if (const auto* fce = llvm::dyn_cast<clang::CXXFunctionalCastExpr>(cur)) {
      return ExtractRegListExprs(fce->getSubExpr(), out_regs);
    }
    if (const auto* ile = llvm::dyn_cast<clang::InitListExpr>(cur)) {
      for (const clang::Expr* elem : ile->inits()) {
        if (!ExtractRegListExprs(elem, out_regs)) return false;
      }
      return true;
    }
    if (const auto* cce = llvm::dyn_cast<clang::CXXConstructExpr>(cur)) {
      if (cce->getNumArgs() == 0) return true;
      for (const clang::Expr* arg : cce->arguments()) {
        if (llvm::isa<clang::CXXDefaultArgExpr>(arg)) continue;
        if (const auto* std_il =
                llvm::dyn_cast_or_null<clang::CXXStdInitializerListExpr>(
                    UnwrapExpr(arg))) {
          if (!ExtractRegListExprs(std_il->getSubExpr(), out_regs)) {
            return false;
          }
          continue;
        }
        if (!ExtractRegListExprs(arg, out_regs)) return false;
      }
      return true;
    }
    return false;
  }

  bool IsSubFromAvailableExpr(const clang::Expr* expr) {
    const clang::Expr* cur = UnwrapExpr(expr);
    if (!cur) return false;
    const clang::Expr* lhs = nullptr;
    if (const auto* oce = llvm::dyn_cast<clang::CXXOperatorCallExpr>(cur)) {
      if (oce->getOperator() == clang::OO_Minus && oce->getNumArgs() == 2) {
        lhs = oce->getArg(0);
      }
    } else if (const auto* bo = llvm::dyn_cast<clang::BinaryOperator>(cur)) {
      if (bo->getOpcode() == clang::BO_Sub) {
        lhs = bo->getLHS();
      }
    }
    if (!lhs) return false;
    const clang::Expr* ulhs = UnwrapExpr(lhs);
    if (const auto* mce =
            llvm::dyn_cast_or_null<clang::CXXMemberCallExpr>(ulhs)) {
      if (const auto* md = mce->getMethodDecl()) {
        llvm::StringRef mname = SafeDeclName(md);
        if (mname == "Available" || mname == "AvailableDouble") return true;
      }
    }
    return IsSubFromAvailableExpr(ulhs);
  }

  static bool IsJumpTargetType(clang::QualType qt) {
    if (IsLabelPtrType(qt)) return true;
    clang::QualType base = qt.getNonReferenceType();
    if (base->isPointerType()) base = base->getPointeeType();
    return IsTypeNamed(base, "Label") || IsTypeNamed(base, "ZoneLabelRef") ||
           IsTypeNamed(base, "BasicBlock");
  }

  bool StmtEndsWithNoFallthrough(const clang::Stmt* stmt) {
    if (!stmt) return false;
    if (const auto* clean = llvm::dyn_cast<clang::ExprWithCleanups>(stmt)) {
      return StmtEndsWithNoFallthrough(clean->getSubExpr());
    }
    if (const auto* cs = llvm::dyn_cast<clang::CompoundStmt>(stmt)) {
      if (cs->body_empty()) return false;
      return StmtEndsWithNoFallthrough(cs->body_back());
    }
    if (const auto* ifs = llvm::dyn_cast<clang::IfStmt>(stmt)) {
      return ifs->getElse() && StmtEndsWithNoFallthrough(ifs->getThen()) &&
             StmtEndsWithNoFallthrough(ifs->getElse());
    }
    if (const auto* ce = llvm::dyn_cast<clang::CallExpr>(stmt)) {
      if (const clang::FunctionDecl* cal = ResolveCalleeDecl(ce)) {
        llvm::StringRef n = SafeDeclName(cal);
        if (n == "V8_Fatal" || n == "UNREACHABLE" ||
            n == "FatalNoSecurityImpact" || n == "Abort") {
          return true;
        }
        return GetOrInferSummary(cal).no_fallthrough;
      }
    }
    return false;
  }

  bool IsMaglevDeferredCallbackDiverging(const clang::FunctionDecl* cb_body,
                                         const clang::LambdaExpr* lam) {
    if (!cb_body || !cb_body->getBody()) return false;
    for (unsigned i = 0; i < cb_body->getNumParams(); ++i) {
      if (IsJumpTargetType(cb_body->getParamDecl(i)->getType())) return false;
    }
    if (lam) {
      for (const clang::LambdaCapture& cap : lam->captures()) {
        if (cap.capturesVariable()) {
          if (const clang::ValueDecl* vd = cap.getCapturedVar()) {
            if (IsJumpTargetType(vd->getType())) return false;
          }
        }
      }
    }
    return StmtEndsWithNoFallthrough(cb_body->getBody());
  }

  // Analyzes a Maglev MakeDeferredCode / JumpToDeferredIf callback (either a
  // LambdaExpr or a function pointer) in a copy of the caller's Env.
  void AnalyzeMaglevDeferredCallback(const clang::CallExpr* ce,
                                     const std::string& callee_name,
                                     Env& caller_env,
                                     FunctionAnalysisContext& fctx) {
    unsigned fn_arg_idx = (callee_name == "JumpToDeferredIf") ? 1u : 0u;
    if (ce->getNumArgs() <= fn_arg_idx) return;
    const clang::Expr* fn_expr = UnwrapExpr(ce->getArg(fn_arg_idx));
    if (!fn_expr) return;

    const clang::FunctionDecl* cb_fn = nullptr;
    const clang::LambdaExpr* cb_lambda = nullptr;
    if (const auto* lambda = llvm::dyn_cast<clang::LambdaExpr>(fn_expr)) {
      cb_lambda = lambda;
      cb_fn = lambda->getCallOperator();
      if (cb_fn) {
        analyzed_deferred_lambdas_.insert(cb_fn->getCanonicalDecl());
      }
    } else {
      if (const auto* uo = llvm::dyn_cast<clang::UnaryOperator>(fn_expr)) {
        if (uo->getOpcode() == clang::UO_AddrOf) {
          fn_expr = UnwrapExpr(uo->getSubExpr());
        }
      }
      if (const auto* dre = llvm::dyn_cast<clang::DeclRefExpr>(fn_expr)) {
        cb_fn = llvm::dyn_cast<clang::FunctionDecl>(dre->getDecl());
      }
    }
    if (!cb_fn) return;

    const clang::FunctionDecl* cb_body = nullptr;
    if (!cb_fn->hasBody(cb_body) || !cb_body->getBody()) {
      // Cross-TU deferred function pointer: apply imported summary under
      // deferred context!
      const FuncSummary& sum = GetOrInferSummary(cb_fn);
      Env deferred_env = caller_env;
      deferred_env.maglev_save_call_depth = 0;
      deferred_env.asm_stack.clear();
      bool saved_deferred = fctx.in_maglev_deferred;
      fctx.in_maglev_deferred = true;
      std::string cb_name = cb_fn->getNameAsString();
      AccountMaglevTempUse(false, false, sum.maglev_gpr_temps_peak, 0,
                           ce->getExprLoc(), "Deferred call '" + cb_name + "'",
                           deferred_env, fctx);
      AccountMaglevTempUse(false, true, sum.maglev_double_temps_peak, 0,
                           ce->getExprLoc(), "Deferred call '" + cb_name + "'",
                           deferred_env, fctx);
      AccountMaglevTempUse(true, false, sum.maglev_gpr_scratches_peak, 0,
                           ce->getExprLoc(), "Deferred call '" + cb_name + "'",
                           deferred_env, fctx);
      AccountMaglevTempUse(true, true, sum.maglev_double_scratches_peak, 0,
                           ce->getExprLoc(), "Deferred call '" + cb_name + "'",
                           deferred_env, fctx);
      if (sum.maglev_uses_save_reg_state) {
        fctx.maglev_used_save_reg_state = true;
        CheckMaglevSaveRegStateProperty(ce->getExprLoc(), fctx);
      }
      if (sum.clobbers_caller_saved || sum.maglev_calls_outside_save_scope) {
        fctx.maglev_called_outside_save_scope = true;
        CheckMaglevCallProperty(cb_name, sum, ce->getExprLoc(), deferred_env,
                                fctx);
      }
      fctx.in_maglev_deferred = saved_deferred;
      return;
    }

    const clang::FunctionDecl* canon_cb = cb_body->getCanonicalDecl();
    if (!active_inferences_.insert(canon_cb).second) return;

    bool is_diverging = IsMaglevDeferredCallbackDiverging(cb_body, cb_lambda);

    Env deferred_env = caller_env;
    deferred_env.reachable = true;
    deferred_env.scratch_scope_stack.clear();
    deferred_env.asm_stack.clear();
    deferred_env.maglev_save_call_depth = 0;
    deferred_env.maglev_snapshot_cleared_syms.clear();

    // Bind deferred callback parameters (param 0 is MaglevAssembler* masm;
    // params 1.. match args after fn_arg_idx).
    std::vector<std::pair<const clang::ValueDecl*, const clang::ValueDecl*>>
        bound_reg_params;
    unsigned first_def_arg = fn_arg_idx + 1;
    for (unsigned p_idx = 1; p_idx < cb_body->getNumParams(); ++p_idx) {
      unsigned arg_idx = first_def_arg + (p_idx - 1);
      if (arg_idx >= ce->getNumArgs()) break;
      const clang::ParmVarDecl* p = cb_body->getParamDecl(p_idx);
      const clang::ValueDecl* cp = p->getCanonicalDecl();
      const clang::Expr* arg = ce->getArg(arg_idx);
      if (IsMaglevTrackableRegType(p->getType())) {
        if (const clang::ValueDecl* sd = ResolveSymDecl(arg, caller_env)) {
          auto it = caller_env.syms.find(sd);
          if (it != caller_env.syms.end()) {
            deferred_env.syms[cp] = it->second;
            bound_reg_params.push_back({cp, sd});
            continue;
          }
        }
        SymRegState s;
        s.status = RegStatus::kAlive;
        s.root_decl = cp;
        s.is_double_reg = IsDoubleRegType(p->getType());
        s.def_version = fctx.next_version++;
        if (auto m = MatchMaglevToRegisterCall(arg, caller_env)) {
          if (!s.is_double_reg) {
            Arm64RegWidth ew = ExprExplicitArm64Width(arg);
            s.arm64_width =
                (ew != Arm64RegWidth::kUnknown) ? ew : Arm64RegWidth::kX;
          }
          if (m->is_result) {
            s.maglev_role = MaglevRole::kResult;
          } else {
            s.maglev_role = MaglevRole::kInput;
            s.maglev_input_key = m->input_key;
            s.maglev_input_label = m->input_label;
            s.holds_entry_value =
                !caller_env.maglev_written_inputs.count(m->input_key);
          }
        } else if (!s.is_double_reg) {
          if (auto pr = ResolvePhysReg(arg, caller_env)) {
            s.phys_reg = *pr;
          }
        }
        deferred_env.syms[cp] = std::move(s);
      } else if (IsMaglevInputType(p->getType())) {
        auto k = ExtractMaglevInputKey(arg, caller_env.local_maglev_inputs,
                                       caller_env.local_expr_bindings);
        if (!k.first.empty()) {
          deferred_env.local_maglev_inputs[cp] = std::move(k);
        }
      } else if (IsOperandType(p->getType())) {
        Env tmp_env = caller_env;
        deferred_env.operands[cp] = ExtractOperandDep(arg, tmp_env, fctx);
      } else {
        // Chase local_expr_bindings through caller locals so nested deferred
        // callbacks retain concrete expressions (e.g. builtin constants).
        const clang::Expr* resolved = arg;
        for (int hop = 0; hop < 4; ++hop) {
          const auto* rdre =
              llvm::dyn_cast_or_null<clang::DeclRefExpr>(UnwrapExpr(resolved));
          if (!rdre) break;
          auto bit = caller_env.local_expr_bindings.find(
              CanonValueDecl(rdre->getDecl()));
          if (bit == caller_env.local_expr_bindings.end() || !bit->second ||
              bit->second == resolved) {
            break;
          }
          resolved = bit->second;
        }
        deferred_env.local_expr_bindings[cp] = resolved;
      }
    }

    // Remap caller `known_not_equal` and `maglev_result_sym` onto the bound
    // callback parameter decls so pre-deferred `DCHECK_NE(a, b)` guards stay
    // active inside MakeDeferredCode / JumpToDeferredIf callbacks.
    if (!bound_reg_params.empty()) {
      std::map<const clang::ValueDecl*, const clang::ValueDecl*> caller_to_cb;
      for (const auto& [cb_d, caller_d] : bound_reg_params) {
        caller_to_cb.try_emplace(caller_d, cb_d);
      }
      if (deferred_env.maglev_result_sym) {
        auto mit = caller_to_cb.find(deferred_env.maglev_result_sym);
        if (mit != caller_to_cb.end()) {
          deferred_env.maglev_result_sym = mit->second;
        }
      }
      std::set<std::pair<const clang::ValueDecl*, const clang::ValueDecl*>>
          extra_ne;
      for (const auto& [d1, d2] : caller_env.known_not_equal) {
        auto it1 = caller_to_cb.find(d1);
        auto it2 = caller_to_cb.find(d2);
        const clang::ValueDecl* nd1 =
            (it1 != caller_to_cb.end()) ? it1->second : d1;
        const clang::ValueDecl* nd2 =
            (it2 != caller_to_cb.end()) ? it2->second : d2;
        if (nd1 != d1 || nd2 != d2) {
          extra_ne.insert({nd1, nd2});
        }
      }
      for (const auto& pair : extra_ne) {
        deferred_env.known_not_equal.insert(pair);
      }
    }

    // Isolate control-flow stacks and label tables while sharing diagnostics
    // and Maglev peak/property accumulators with fctx.
    bool saved_deferred = fctx.in_maglev_deferred;
    bool saved_diverging = fctx.in_maglev_diverging_deferred;
    size_t saved_base_scope = fctx.base_scratch_scope_depth;
    auto saved_cf = std::move(fctx.cf_stack);
    auto saved_exits = std::move(fctx.exit_envs);
    auto saved_fwd = std::move(fctx.forward_label_states);
    auto saved_bwd = std::move(fctx.backward_label_states);
    auto saved_bound = std::move(fctx.bound_labels);

    fctx.in_maglev_deferred = true;
    fctx.in_maglev_diverging_deferred = is_diverging;
    fctx.base_scratch_scope_depth = deferred_env.scratch_scope_stack.size();
    fctx.cf_stack.clear();
    fctx.exit_envs.clear();
    fctx.forward_label_states.clear();
    fctx.backward_label_states.clear();
    fctx.bound_labels.clear();

    VisitStmt(cb_body->getBody(), deferred_env, fctx);

    // For rejoining deferred callbacks inside helper functions (e.g.
    // MaglevAssembler::EnsureWritableFastElements), propagate non-restored
    // parameter modifications back to caller_env so summary inference sees
    // which registers were overwritten vs restored by SaveRegisterStateForCall.
    if (!is_diverging && !fctx.maglev_node_cls && !bound_reg_params.empty()) {
      Env deferred_exit;
      deferred_exit.reachable = false;
      if (deferred_env.reachable) {
        deferred_exit = deferred_env;
      }
      for (const auto& [lbl, lenv] : fctx.forward_label_states) {
        (void)lbl;
        deferred_exit = MergeEnvs(deferred_exit, lenv, arch_.num_gprs);
      }
      if (deferred_exit.reachable) {
        for (const auto& [cb_d, caller_d] : bound_reg_params) {
          auto dit = deferred_exit.syms.find(cb_d);
          auto cit = caller_env.syms.find(caller_d);
          if (dit != deferred_exit.syms.end() && cit != caller_env.syms.end() &&
              !dit->second.holds_entry_value) {
            cit->second.holds_entry_value = false;
            if (dit->second.status == RegStatus::kClobbered) {
              cit->second.status = RegStatus::kClobbered;
            } else if (cit->second.status == RegStatus::kInitial) {
              cit->second.status = RegStatus::kAlive;
            }
          }
        }
      }
    }

    fctx.in_maglev_deferred = saved_deferred;
    fctx.in_maglev_diverging_deferred = saved_diverging;
    fctx.base_scratch_scope_depth = saved_base_scope;
    fctx.cf_stack = std::move(saved_cf);
    fctx.exit_envs = std::move(saved_exits);
    fctx.forward_label_states = std::move(saved_fwd);
    fctx.backward_label_states = std::move(saved_bwd);
    fctx.bound_labels = std::move(saved_bound);
    active_inferences_.erase(canon_cb);
  }

  // Inlines a direct call to a local lambda (e.g. `clamp(...)` in StringSlice),
  // swapping `fctx.exit_envs` so `return;` inside the lambda merges back into
  // the caller's Env instead of terminating the enclosing GenerateCode.
  bool InlineLocalLambdaCall(const clang::CXXOperatorCallExpr* oce, Env& env,
                             FunctionAnalysisContext& fctx) {
    if (oce->getOperator() != clang::OO_Call || oce->getNumArgs() < 1) {
      return false;
    }
    if (fctx.inline_lambda_depth >= 4) return false;
    const clang::Expr* obj = UnwrapExpr(oce->getArg(0));
    const auto* dre = llvm::dyn_cast_or_null<clang::DeclRefExpr>(obj);
    if (!dre) return false;
    const clang::ValueDecl* ld = CanonValueDecl(dre->getDecl());
    auto it = env.local_expr_bindings.find(ld);
    if (it == env.local_expr_bindings.end() || !it->second) return false;
    const auto* lambda =
        llvm::dyn_cast_or_null<clang::LambdaExpr>(UnwrapExpr(it->second));
    if (!lambda) return false;
    const clang::CXXMethodDecl* call_op = lambda->getCallOperator();
    if (!call_op || !call_op->getBody()) return false;

    analyzed_deferred_lambdas_.insert(call_op->getCanonicalDecl());

    // Visit call arguments first.
    for (unsigned i = 1; i < oce->getNumArgs(); ++i) {
      VisitStmt(oce->getArg(i), env, fctx);
    }

    auto saved_expr_bindings = env.local_expr_bindings;
    auto saved_input_bindings = env.local_maglev_inputs;
    std::vector<const clang::ValueDecl*> lambda_param_decls;
    unsigned np =
        std::min<unsigned>(call_op->getNumParams(), oce->getNumArgs() - 1);
    for (unsigned i = 0; i < np; ++i) {
      const clang::ParmVarDecl* p = call_op->getParamDecl(i);
      const clang::ValueDecl* cp = p->getCanonicalDecl();
      lambda_param_decls.push_back(cp);
      const clang::Expr* arg = oce->getArg(i + 1);
      if (IsMaglevTrackableRegType(p->getType())) {
        if (const clang::ValueDecl* sd = ResolveSymDecl(arg, env)) {
          auto sit = env.syms.find(sd);
          if (sit != env.syms.end()) {
            env.syms[cp] = sit->second;
            if (env.known_no_reg.contains(sd)) {
              env.known_no_reg.insert(cp);
            }
            continue;
          }
        }
        SymRegState s;
        s.status = RegStatus::kAlive;
        s.root_decl = cp;
        s.is_double_reg = IsDoubleRegType(p->getType());
        s.def_version = fctx.next_version++;
        if (!s.is_double_reg) {
          if (auto pr = ResolvePhysReg(arg, env)) {
            s.phys_reg = *pr;
            if (*pr == -1) env.known_no_reg.insert(cp);
          }
        }
        env.syms[cp] = std::move(s);
      } else if (IsMaglevInputType(p->getType())) {
        auto k = ExtractMaglevInputKey(arg, saved_input_bindings,
                                       saved_expr_bindings);
        if (!k.first.empty()) env.local_maglev_inputs[cp] = std::move(k);
      } else {
        env.local_expr_bindings[cp] = arg;
      }
    }

    size_t saved_base_scope = fctx.base_scratch_scope_depth;
    auto saved_exits = std::move(fctx.exit_envs);
    auto saved_cf = std::move(fctx.cf_stack);
    fctx.base_scratch_scope_depth = env.scratch_scope_stack.size();
    fctx.exit_envs.clear();
    fctx.cf_stack.clear();
    fctx.inline_lambda_depth++;

    VisitStmt(call_op->getBody(), env, fctx);

    fctx.inline_lambda_depth--;
    Env merged = env;
    for (const Env& ret_env : fctx.exit_envs) {
      merged = MergeEnvs(merged, ret_env, arch_.num_gprs);
    }
    fctx.base_scratch_scope_depth = saved_base_scope;
    fctx.exit_envs = std::move(saved_exits);
    fctx.cf_stack = std::move(saved_cf);
    env = std::move(merged);
    for (const clang::ValueDecl* cp : lambda_param_decls) {
      env.syms.erase(cp);
      env.known_no_reg.erase(cp);
    }
    env.local_expr_bindings = std::move(saved_expr_bindings);
    env.local_maglev_inputs = std::move(saved_input_bindings);
    return true;
  }

  // Inlines small MaglevAssembler-taking helper constructors (e.g. MapCompare)
  // so path-sensitive conditions like `if (map_count_ != 1)` unify with
  // SetValueLocationConstraints.
  bool InlineMaglevCtorCall(const clang::CXXConstructExpr* cce, Env& env,
                            FunctionAnalysisContext& fctx) {
    if (!cce || fctx.inline_lambda_depth >= 4) return false;
    const clang::CXXConstructorDecl* ctor = cce->getConstructor();
    if (!ctor || ctor->isCopyOrMoveConstructor()) return false;
    if (!TakesAssemblerParam(ctor)) return false;
    const clang::FunctionDecl* body_decl = nullptr;
    if (!ctor->hasBody(body_decl) || !body_decl->getBody()) return false;
    const auto* ctor_body =
        llvm::dyn_cast<clang::CXXConstructorDecl>(body_decl);
    if (!ctor_body) return false;

    for (const clang::Expr* arg : cce->arguments()) {
      VisitStmt(arg, env, fctx);
    }

    auto saved_expr_bindings = env.local_expr_bindings;
    auto saved_input_bindings = env.local_maglev_inputs;
    ExprSubMap param_to_arg;
    unsigned np =
        std::min<unsigned>(ctor_body->getNumParams(), cce->getNumArgs());
    for (unsigned i = 0; i < np; ++i) {
      const clang::ParmVarDecl* p = ctor_body->getParamDecl(i);
      const clang::ValueDecl* cp = p->getCanonicalDecl();
      const clang::Expr* arg = cce->getArg(i);
      param_to_arg[cp] = arg;
      if (IsMaglevTrackableRegType(p->getType())) {
        if (const clang::ValueDecl* sd = ResolveSymDecl(arg, env)) {
          auto sit = env.syms.find(sd);
          if (sit != env.syms.end()) {
            env.syms[cp] = sit->second;
            continue;
          }
        }
      } else if (IsMaglevInputType(p->getType())) {
        auto k = ExtractMaglevInputKey(arg, saved_input_bindings,
                                       saved_expr_bindings);
        if (!k.first.empty()) env.local_maglev_inputs[cp] = std::move(k);
      } else {
        env.local_expr_bindings[cp] = arg;
      }
    }

    // Map member-initializer fields directly to caller argument expressions.
    for (const clang::CXXCtorInitializer* ci : ctor_body->inits()) {
      if (!ci->isMemberInitializer()) continue;
      const clang::FieldDecl* fd = ci->getMember();
      const clang::Expr* init_e = UnwrapExpr(ci->getInit());
      if (!fd || !init_e) continue;
      const clang::Expr* resolved = init_e;
      if (const auto* dre = llvm::dyn_cast<clang::DeclRefExpr>(init_e)) {
        auto pit = param_to_arg.find(CanonValueDecl(dre->getDecl()));
        if (pit != param_to_arg.end() && pit->second) {
          resolved = pit->second;
        }
      }
      env.local_expr_bindings[fd->getCanonicalDecl()] = resolved;
    }

    size_t saved_base_scope = fctx.base_scratch_scope_depth;
    auto saved_exits = std::move(fctx.exit_envs);
    auto saved_cf = std::move(fctx.cf_stack);
    fctx.base_scratch_scope_depth = env.scratch_scope_stack.size();
    fctx.exit_envs.clear();
    fctx.cf_stack.clear();
    fctx.inline_lambda_depth++;

    VisitStmt(ctor_body->getBody(), env, fctx);

    fctx.inline_lambda_depth--;
    Env merged = env;
    for (const Env& ret_env : fctx.exit_envs) {
      merged = MergeEnvs(merged, ret_env, arch_.num_gprs);
    }
    fctx.base_scratch_scope_depth = saved_base_scope;
    fctx.exit_envs = std::move(saved_exits);
    fctx.cf_stack = std::move(saved_cf);
    env = std::move(merged);
    env.local_expr_bindings = std::move(saved_expr_bindings);
    env.local_maglev_inputs = std::move(saved_input_bindings);
    return true;
  }

  bool IsBenignMaglevSameRegExt(llvm::StringRef callee_name,
                                const clang::CallExpr* ce, unsigned out_idx,
                                const Env& env) {
    if (out_idx != 0 || ce->getNumArgs() < 2) return false;
    if (callee_name != "SignExtend32To64Bits" &&
        callee_name != "ZeroExtend32To64Bits" && callee_name != "movsxlq" &&
        callee_name != "movl" && callee_name != "Sxtw" &&
        callee_name != "Uxtw" && callee_name != "sxtw" &&
        callee_name != "uxtw") {
      return false;
    }
    const clang::ValueDecl* s0 = ResolveSymDecl(ce->getArg(0), env);
    const clang::ValueDecl* s1 = ResolveSymDecl(ce->getArg(1), env);
    if (s0 && s0 == s1) return true;
    std::optional<int> p0 = ResolvePhysReg(ce->getArg(0), env);
    std::optional<int> p1 = ResolvePhysReg(ce->getArg(1), env);
    return p0 && p1 && *p0 >= 0 && *p0 == *p1;
  }

  bool IsSymSavedOnStack(const clang::ValueDecl* sym, int phys_reg,
                         const Env& env) {
    for (const SavedStackEntry& e : env.asm_stack) {
      if (e.kind == SavedStackEntry::kCallerSaved ||
          e.kind == SavedStackEntry::kAllRegs) {
        if (sym && env.maglev_snapshot_cleared_syms.contains(sym)) {
          continue;
        }
        return true;
      }
      if (e.kind == SavedStackEntry::kSingleReg) {
        if ((sym && e.root_decl == sym) ||
            (phys_reg >= 0 && e.phys_reg == phys_reg)) {
          return true;
        }
      }
    }
    return false;
  }

  const clang::FunctionDecl* ResolveCalleeDecl(const clang::CallExpr* ce) {
    if (!ce) return nullptr;
    if (const clang::FunctionDecl* direct = ce->getDirectCallee()) {
      return direct;
    }
    const clang::Expr* callee_expr = UnwrapExpr(ce->getCallee());
    if (const auto* bo =
            llvm::dyn_cast_or_null<clang::BinaryOperator>(callee_expr)) {
      if (bo->getOpcode() == clang::BO_PtrMemI ||
          bo->getOpcode() == clang::BO_PtrMemD) {
        const clang::Expr* rhs = UnwrapExpr(bo->getRHS());
        while (const auto* snttp =
                   llvm::dyn_cast_or_null<clang::SubstNonTypeTemplateParmExpr>(
                       rhs)) {
          rhs = UnwrapExpr(snttp->getReplacement());
        }
        if (const auto* uo =
                llvm::dyn_cast_or_null<clang::UnaryOperator>(rhs)) {
          if (uo->getOpcode() == clang::UO_AddrOf) {
            rhs = UnwrapExpr(uo->getSubExpr());
          }
        }
        if (const auto* dre = llvm::dyn_cast_or_null<clang::DeclRefExpr>(rhs)) {
          if (const auto* fd =
                  llvm::dyn_cast<clang::FunctionDecl>(dre->getDecl())) {
            return fd;
          }
        }
      }
    }
    return nullptr;
  }

  FuncSummary InferAssemblerForwarder(const clang::FunctionDecl* fn,
                                      const FuncSummary& base_sum) {
    FuncSummary out = base_sum;
    const clang::Stmt* body = fn->getBody();
    if (!body) return out;

    std::map<const clang::ValueDecl*, unsigned> param_map;
    for (unsigned i = 0; i < fn->getNumParams(); ++i) {
      param_map[fn->getParamDecl(i)->getCanonicalDecl()] = i;
    }

    class CallFinder : public clang::RecursiveASTVisitor<CallFinder> {
     public:
      std::vector<const clang::CallExpr*> calls;
      bool VisitCallExpr(const clang::CallExpr* ce) {
        calls.push_back(ce);
        return true;
      }
    } finder;
    finder.TraverseStmt(const_cast<clang::Stmt*>(body));

    for (const clang::CallExpr* ce : finder.calls) {
      const clang::FunctionDecl* callee = ce->getDirectCallee();
      if (!callee) continue;
      FuncSummary callee_sum = ExtractExplicitSummary(callee, ast_ctx_, arch_);
      if (!callee_sum.has_explicit_annotations) {
        const clang::FunctionDecl* callee_body = nullptr;
        if (callee->hasBody(callee_body) && callee_body != fn) {
          callee_sum = InferAssemblerForwarder(callee_body, callee_sum);
        }
      }
      if (!callee_sum.has_explicit_annotations) continue;

      out.has_explicit_annotations = true;
      out.implicit_in |= callee_sum.implicit_in;
      out.implicit_out |= callee_sum.implicit_out;
      out.clobbers |= callee_sum.clobbers;
      for (const ConditionalClobber& cc : callee_sum.conditional_clobbers) {
        if (cc.param_index < ce->getNumArgs()) {
          const clang::Expr* carg = UnwrapExpr(ce->getArg(cc.param_index));
          ExprSubMap empty_subs;
          if (auto iv = TryEvalConstInt(carg, empty_subs)) {
            if (EvalCondCmpOp(cc.op, *iv, cc.rhs)) {
              out.clobbers |= cc.mask;
            }
            continue;
          }
          if (const auto* dre =
                  llvm::dyn_cast_or_null<clang::DeclRefExpr>(carg)) {
            auto pit = param_map.find(CanonValueDecl(dre->getDecl()));
            if (pit != param_map.end()) {
              ConditionalClobber ncc{pit->second, cc.op, cc.rhs, cc.mask};
              if (std::find(out.conditional_clobbers.begin(),
                            out.conditional_clobbers.end(),
                            ncc) == out.conditional_clobbers.end()) {
                out.conditional_clobbers.push_back(ncc);
              }
              continue;
            }
          }
        }
        out.clobbers |= cc.mask;
      }
      out.clobbers_caller_saved |= callee_sum.clobbers_caller_saved;
      out.no_fallthrough |= callee_sum.no_fallthrough;
      out.zeroing_if_same |= callee_sum.zeroing_if_same;
      out.is_push |= callee_sum.is_push;
      out.is_pop |= callee_sum.is_pop;

      unsigned n =
          std::min<unsigned>(ce->getNumArgs(), callee_sum.param_roles.size());
      for (unsigned i = 0; i < n; ++i) {
        const clang::Expr* arg = UnwrapExpr(ce->getArg(i));
        if (const auto* dre = llvm::dyn_cast_or_null<clang::DeclRefExpr>(arg)) {
          auto it = param_map.find(CanonValueDecl(dre->getDecl()));
          if (it != param_map.end()) {
            out.param_roles[it->second] = callee_sum.param_roles[i];
          }
        }
      }
    }
    return out;
  }

  FuncSummary AnalyzeFunctionInternal(const clang::FunctionDecl* fn,
                                      const FuncSummary& explicit_sum,
                                      bool emit_diagnostics) {
    const clang::CXXRecordDecl* maglev_cls = nullptr;
    if (const auto* md = llvm::dyn_cast<clang::CXXMethodDecl>(fn)) {
      if (md->getIdentifier() && md->getName() == "GenerateCode") {
        maglev_cls = md->getParent();
      }
    }

    std::map<const clang::ValueDecl*, Env> carried_backedges;
    constexpr int kMaxFixpointPasses = 4;

    FuncSummary inferred;
    std::vector<PendingDiag> final_diags;

    for (int pass = 0; pass < kMaxFixpointPasses; ++pass) {
      FunctionAnalysisContext fctx;
      fctx.fn = fn;
      fctx.maglev_node_cls = maglev_cls;
      fctx.explicit_sum = explicit_sum;
      fctx.emit_diagnostics = emit_diagnostics;
      fctx.carried_backedges = &carried_backedges;

      unsigned num_params = fn->getNumParams();
      fctx.param_read_entry.assign(num_params, false);
      fctx.param_written_any.assign(num_params, false);
      fctx.param_clobbered_any.assign(num_params, false);
      fctx.param_written_before_read_any.assign(num_params, false);
      fctx.early_clobber_phys_before_param.assign(num_params, 0);

      Env env;
      env.arm64_avail_scratch_mask = arch_.arm64_default_tmp_mask;
      for (int r = 0; r < arch_.num_gprs; ++r) {
        env.phys[r].status = RegStatus::kInitial;
        env.phys[r].holds_entry_value = true;
        env.phys[r].def_version = fctx.next_version++;
      }

      for (unsigned i = 0; i < num_params; ++i) {
        const clang::ParmVarDecl* p = fn->getParamDecl(i);
        const clang::ValueDecl* canon_p = p->getCanonicalDecl();
        if (IsTrackableRegType(p->getType())) {
          SymRegState s;
          s.status = RegStatus::kInitial;
          s.root_decl = canon_p;
          s.param_index = static_cast<int>(i);
          s.holds_entry_value = true;
          s.def_version = fctx.next_version++;
          if (i < explicit_sum.param_roles.size() &&
              explicit_sum.param_roles[i] == ParamRole::kRdOnly) {
            s.readonly_kind = ReadonlyKind::kExplicitAttr;
            s.readonly_reason = "parameter '" + p->getNameAsString() +
                                "' is annotated V8_REG_RDONLY";
          }
          env.syms[canon_p] = s;
        } else if (IsOperandType(p->getType())) {
          SymRegState s;
          s.status = RegStatus::kInitial;
          s.root_decl = canon_p;
          s.param_index = static_cast<int>(i);
          s.holds_entry_value = true;
          s.def_version = fctx.next_version++;
          env.syms[canon_p] = s;

          OperandDep dep;
          dep.creation_loc = p->getLocation();
          dep.sym_decls.push_back(canon_p);
          env.operands[canon_p] = dep;
        }
      }

      if (const clang::Stmt* body = fn->getBody()) {
        VisitStmt(body, env, fctx);
        if (env.reachable) {
          fctx.exit_envs.push_back(env);
        }
      }

      inferred = BuildInferredSummary(fctx);
      final_diags = std::move(fctx.pending_diags);

      if (fctx.backward_label_states.empty()) {
        break;
      }

      bool changed = false;
      for (const auto& [lbl, back_env] : fctx.backward_label_states) {
        auto it = carried_backedges.find(lbl);
        if (it == carried_backedges.end()) {
          carried_backedges[lbl] = back_env;
          changed = true;
        } else {
          Env merged = MergeEnvs(it->second, back_env, arch_.num_gprs);
          if (!EnvEquivalentForFixpoint(it->second, merged, arch_.num_gprs)) {
            it->second = std::move(merged);
            changed = true;
          }
        }
      }
      if (!changed) break;

      if (pass + 1 == kMaxFixpointPasses) {
        break;
      }
    }

    if (emit_diagnostics) {
      FlushDiagnostics(final_diags);
      if (g_verify_annotations) {
        VerifyAnnotations(fn, explicit_sum, inferred);
      }
    }
    return inferred;
  }

  // Resolves a symbolic Register (or Operand `.reg()` / `.base()`) expression
  // to its canonical ValueDecl* in env.
  const clang::ValueDecl* ResolveSymDecl(const clang::Expr* expr,
                                         const Env& env) {
    expr = UnwrapExpr(expr);
    if (!expr) return nullptr;

    if (const auto* mce = llvm::dyn_cast<clang::CXXMemberCallExpr>(expr)) {
      if (const auto* md = mce->getMethodDecl()) {
        llvm::StringRef mname = SafeDeclName(md);
        if (mce->getNumArgs() == 0 && (mname == "reg" || mname == "base" ||
                                       mname == "regoffset" || mname == "rm")) {
          const clang::Expr* obj = UnwrapExpr(mce->getImplicitObjectArgument());
          if (const auto* dre =
                  llvm::dyn_cast_or_null<clang::DeclRefExpr>(obj)) {
            const clang::ValueDecl* d = CanonValueDecl(dre->getDecl());
            auto sym_it = env.syms.find(d);
            if (sym_it != env.syms.end() && sym_it->second.root_decl) {
              return sym_it->second.root_decl;
            }
            auto op_it = env.operands.find(d);
            if (op_it != env.operands.end() &&
                op_it->second.sym_decls.size() == 1) {
              return op_it->second.sym_decls[0];
            }
          }
        }
      }
    }

    if (const auto* dre = llvm::dyn_cast<clang::DeclRefExpr>(expr)) {
      const clang::ValueDecl* d = CanonValueDecl(dre->getDecl());
      if (const auto* vd = llvm::dyn_cast<clang::VarDecl>(d)) {
        if (vd->isConstexpr() || !vd->isLocalVarDeclOrParm()) {
          return nullptr;
        }
      }
      auto it = env.syms.find(d);
      if (it != env.syms.end() && it->second.root_decl) {
        return it->second.root_decl;
      }
      return d;
    }
    return nullptr;
  }

  std::optional<int> ResolvePhysReg(const clang::Expr* expr, const Env& env) {
    if (!expr) return std::nullopt;
    if (auto c = EvaluateRegisterCode(expr, ast_ctx_, arch_)) {
      return c;
    }
    const clang::Expr* unwrapped = UnwrapExpr(expr);
    if (!unwrapped) return std::nullopt;
    if (auto c = EvaluateRegisterCode(unwrapped, ast_ctx_, arch_)) {
      return c;
    }
    if (const clang::ValueDecl* d = ResolveSymDecl(unwrapped, env)) {
      auto it = env.syms.find(d);
      if (it != env.syms.end() && it->second.phys_reg >= 0) {
        return it->second.phys_reg;
      }
    }
    return std::nullopt;
  }

  const clang::ValueDecl* ResolveLabelDecl(const clang::Expr* expr) {
    expr = UnwrapExpr(expr);
    if (!expr) return nullptr;
    if (const auto* uo = llvm::dyn_cast<clang::UnaryOperator>(expr)) {
      if (uo->getOpcode() == clang::UO_AddrOf ||
          uo->getOpcode() == clang::UO_Deref) {
        expr = UnwrapExpr(uo->getSubExpr());
      }
    } else if (const auto* oce =
                   llvm::dyn_cast<clang::CXXOperatorCallExpr>(expr)) {
      if (oce->getOperator() == clang::OO_Star && oce->getNumArgs() == 1) {
        expr = UnwrapExpr(oce->getArg(0));
      }
    }
    if (const auto* dre = llvm::dyn_cast_or_null<clang::DeclRefExpr>(expr)) {
      return CanonValueDecl(dre->getDecl());
    }
    return nullptr;
  }

  void MergeSubOperandDep(OperandDep& dst, const OperandDep& sub) {
    for (const auto* sd : sub.sym_decls) {
      if (std::find(dst.sym_decls.begin(), dst.sym_decls.end(), sd) ==
          dst.sym_decls.end()) {
        dst.sym_decls.push_back(sd);
      }
    }
    dst.phys_mask |= sub.phys_mask;
    for (const auto& pv : sub.scratch_phys_versions) {
      dst.scratch_phys_versions.push_back(pv);
    }
    for (const auto& sv : sub.scratch_sym_versions) {
      dst.scratch_sym_versions.push_back(sv);
    }
  }

  bool IsMaglevCompilationContext(const FunctionAnalysisContext& fctx) const {
    if (fctx.maglev_node_cls != nullptr) return true;
    if (!fctx.fn) return false;
    if (const auto* md = llvm::dyn_cast<clang::CXXMethodDecl>(fctx.fn)) {
      if (const clang::CXXRecordDecl* parent = md->getParent()) {
        llvm::StringRef rn = SafeDeclName(parent);
        if (rn == "MaglevAssembler" || rn == "FakeMaglevAssembler") {
          return true;
        }
      }
    }
    for (const clang::ParmVarDecl* p : fctx.fn->parameters()) {
      clang::QualType qt = p->getType();
      if (qt->isPointerType() || qt->isReferenceType()) {
        qt = qt->getPointeeType();
      }
      if (const auto* rec = qt->getAsCXXRecordDecl()) {
        llvm::StringRef rn = SafeDeclName(rec);
        if (rn == "MaglevAssembler" || rn == "FakeMaglevAssembler") {
          return true;
        }
      }
    }
    return false;
  }

  // Returns true if `expr` is an `ExternalReference` guaranteed to resolve via
  // `[kRootRegister + offset]` without using `ExternalReferenceAsOperand`'s
  // scratch parameter:
  // 1. Any `ExternalReference` constructed from `IsolateFieldId`
  //    (`ExternalReference::Create(IsolateFieldId::...)` or
  //    `ExternalReference(IsolateFieldId::...)`).
  // 2. When `allow_maglev_isolate_refs` is true (Maglev runtime JIT, where
  //    `enable_root_relative_access` is always true), 1-arg static factories on
  //    `ExternalReference` taking `Isolate*` or `LocalIsolate*` whose target
  //    resides directly inside `Isolate` (excluding factories that dereference
  //    separately heap-allocated pointers off `Isolate` such as
  //    `interpreter()`, `debug()`, `regexp_stack()`, or
  //    `RuntimeFunctionTable`).
  bool IsRootRelativeExternalRefExpr(const clang::Expr* expr, const Env& env,
                                     bool allow_maglev_isolate_refs,
                                     int depth = 0) const {
    if (!expr || depth > 6) return false;
    const clang::Expr* cur = UnwrapExpr(expr);
    if (!cur) return false;
    if (IsTypeNamed(cur->getType(), "IsolateFieldId")) return true;

    if (const auto* dre = llvm::dyn_cast<clang::DeclRefExpr>(cur)) {
      const clang::ValueDecl* d = CanonValueDecl(dre->getDecl());
      auto it = env.local_expr_bindings.find(d);
      if (it != env.local_expr_bindings.end() && it->second &&
          it->second != cur) {
        return IsRootRelativeExternalRefExpr(
            it->second, env, allow_maglev_isolate_refs, depth + 1);
      }
      return false;
    }

    if (const auto* cce = llvm::dyn_cast<clang::CXXConstructExpr>(cur)) {
      if (cce->getNumArgs() == 1) {
        const clang::Expr* a0 = UnwrapExpr(cce->getArg(0));
        if (a0 && IsTypeNamed(a0->getType(), "IsolateFieldId")) {
          return true;
        }
        return IsRootRelativeExternalRefExpr(a0, env, allow_maglev_isolate_refs,
                                             depth + 1);
      }
      return false;
    }

    if (const auto* ce = llvm::dyn_cast<clang::CallExpr>(cur)) {
      const clang::FunctionDecl* fd = ce->getDirectCallee();
      if (!fd) return false;
      const auto* md = llvm::dyn_cast<clang::CXXMethodDecl>(fd);
      if (!md || !md->isStatic()) return false;
      const clang::CXXRecordDecl* parent = md->getParent();
      if (!parent || SafeDeclName(parent) != "ExternalReference") return false;

      llvm::StringRef mname = SafeDeclName(md);
      if (mname == "Create" && ce->getNumArgs() >= 1) {
        const clang::Expr* a0 = UnwrapExpr(ce->getArg(0));
        if (a0 && IsTypeNamed(a0->getType(), "IsolateFieldId")) {
          return true;
        }
        return false;
      }

      if (!allow_maglev_isolate_refs || md->getNumParams() != 1) return false;
      clang::QualType pt = md->getParamDecl(0)->getType();
      if (!pt->isPointerType()) return false;
      const clang::CXXRecordDecl* pt_rec =
          pt->getPointeeType()->getAsCXXRecordDecl();
      if (!pt_rec) return false;
      llvm::StringRef pt_name = SafeDeclName(pt_rec);
      if (pt_name != "Isolate" && pt_name != "LocalIsolate") return false;

      // Exclude `ExternalReference` factories that dereference separately
      // heap-allocated sub-objects (`interpreter()`, `debug()`,
      // `regexp_stack()`, `RuntimeFunctionTable`) rather than fields embedded
      // directly in `class Isolate`.
      if (mname ==
              "address_of_interpreter_entry_trampoline_instruction_start" ||
          mname == "interpreter_dispatch_table_address" ||
          mname == "debug_is_active_address" ||
          mname == "debug_hook_on_function_call_address" ||
          mname == "debug_suspended_generator_address" ||
          mname == "runtime_function_table_address" ||
          mname == "runtime_function_table_address_for_unittests" ||
          mname == "address_of_regexp_stack_thread_local") {
        return false;
      }
      return true;
    }
    return false;
  }

  OperandDep BuildCallReturnedOperandDep(const clang::CallExpr* ce,
                                         const clang::FunctionDecl* callee,
                                         const FuncSummary& sum, const Env& env,
                                         FunctionAnalysisContext& fctx) {
    OperandDep dep;
    dep.creation_loc = ce->getExprLoc();

    if (callee && SafeDeclName(callee) == "ExternalReferenceAsOperand" &&
        ce->getNumArgs() >= 1 &&
        IsRootRelativeExternalRefExpr(ce->getArg(0), env,
                                      IsMaglevCompilationContext(fctx))) {
      return dep;
    }

    bool has_summary_provenance =
        (sum.returned_operand_scratch_phys_mask != 0 ||
         sum.returned_operand_scratch_param_mask != 0 ||
         sum.returned_operand_input_param_mask != 0 ||
         sum.returned_operand_input_phys_mask != 0);

    if (has_summary_provenance) {
      for (int r = 0; r < arch_.num_gprs; ++r) {
        if (sum.returned_operand_scratch_phys_mask & (1u << r)) {
          dep.scratch_phys_versions.push_back({r, env.phys[r].def_version});
        }
        if (sum.returned_operand_input_phys_mask & (1u << r)) {
          dep.phys_mask |= (1u << r);
        }
      }

      unsigned n = std::min<unsigned>(ce->getNumArgs(), 32u);
      for (unsigned i = 0; i < n; ++i) {
        const clang::Expr* arg = ce->getArg(i);
        if (sum.returned_operand_scratch_param_mask & (1u << i)) {
          if (llvm::isa<clang::CXXDefaultArgExpr>(arg)) {
            if (auto def_p = EvaluateRegisterCode(arg, ast_ctx_, arch_)) {
              if (*def_p >= 0 && *def_p < arch_.num_gprs) {
                dep.scratch_phys_versions.push_back(
                    {*def_p, env.phys[*def_p].def_version});
              }
            }
          } else if (auto p = ResolvePhysReg(arg, env)) {
            if (*p >= 0 && *p < arch_.num_gprs) {
              dep.scratch_phys_versions.push_back(
                  {*p, env.phys[*p].def_version});
            }
          } else if (const clang::ValueDecl* sd = ResolveSymDecl(arg, env)) {
            auto it = env.syms.find(sd);
            if (it != env.syms.end()) {
              dep.scratch_sym_versions.push_back({sd, it->second.def_version});
            }
          }
        } else if (sum.returned_operand_input_param_mask & (1u << i)) {
          if (llvm::isa<clang::CXXDefaultArgExpr>(arg)) continue;
          const clang::Expr* uarg = UnwrapExpr(arg);
          if (!uarg) continue;
          if (IsTrackableRegType(uarg->getType())) {
            if (const clang::ValueDecl* sd = ResolveSymDecl(uarg, env)) {
              dep.sym_decls.push_back(sd);
            }
            if (auto pr = ResolvePhysReg(uarg, env)) {
              if (*pr >= 0 && *pr < arch_.num_gprs) {
                dep.phys_mask |= (1u << *pr);
              }
            }
          } else if (IsOperandType(uarg->getType())) {
            Env const_env = env;
            MergeSubOperandDep(dep, ExtractOperandDep(uarg, const_env, fctx));
          }
        }
      }
      return dep;
    }

    // Fallback for unanalyzed constructors/helpers like FieldOperand(base, 8):
    // collect all Register / Operand arguments.
    (void)callee;
    for (const clang::Expr* a : ce->arguments()) {
      if (llvm::isa<clang::CXXDefaultArgExpr>(a)) continue;
      const clang::Expr* ua = UnwrapExpr(a);
      if (!ua) continue;
      if (IsTrackableRegType(ua->getType())) {
        if (const clang::ValueDecl* sd = ResolveSymDecl(ua, env)) {
          dep.sym_decls.push_back(sd);
        }
        if (auto pr = ResolvePhysReg(ua, env)) {
          if (*pr >= 0 && *pr < arch_.num_gprs) {
            dep.phys_mask |= (1u << *pr);
          }
        }
      } else if (IsOperandType(ua->getType())) {
        Env const_env = env;
        MergeSubOperandDep(dep, ExtractOperandDep(ua, const_env, fctx));
      }
    }
    return dep;
  }

  OperandDep ExtractOperandDep(const clang::Expr* expr, Env& env,
                               FunctionAnalysisContext& fctx) {
    OperandDep dep;
    expr = UnwrapExpr(expr);
    if (!expr) return dep;
    dep.creation_loc = expr->getExprLoc();

    if (const auto* dre = llvm::dyn_cast<clang::DeclRefExpr>(expr)) {
      const clang::ValueDecl* d = CanonValueDecl(dre->getDecl());
      auto it = env.operands.find(d);
      if (it != env.operands.end()) {
        return it->second;
      }
      if (env.syms.find(d) != env.syms.end()) {
        dep.sym_decls.push_back(d);
      }
      return dep;
    }

    if (const auto* cond_op =
            llvm::dyn_cast<clang::ConditionalOperator>(expr)) {
      MergeSubOperandDep(dep,
                         ExtractOperandDep(cond_op->getTrueExpr(), env, fctx));
      MergeSubOperandDep(dep,
                         ExtractOperandDep(cond_op->getFalseExpr(), env, fctx));
      return dep;
    }

    if (const auto* ce = llvm::dyn_cast<clang::CallExpr>(expr)) {
      auto cached_it = fctx.call_operand_deps.find(ce);
      if (cached_it != fctx.call_operand_deps.end()) {
        return cached_it->second;
      }
      if (const clang::FunctionDecl* callee = ResolveCalleeDecl(ce)) {
        const FuncSummary& sum = GetOrInferSummary(callee);
        return BuildCallReturnedOperandDep(ce, callee, sum, env, fctx);
      }
    }

    if (const auto* cxx_construct =
            llvm::dyn_cast<clang::CXXConstructExpr>(expr)) {
      for (const clang::Expr* a : cxx_construct->arguments()) {
        if (llvm::isa<clang::CXXDefaultArgExpr>(a)) continue;
        const clang::Expr* ua = UnwrapExpr(a);
        if (!ua) continue;
        if (IsTrackableRegType(ua->getType())) {
          if (const clang::ValueDecl* sd = ResolveSymDecl(ua, env)) {
            dep.sym_decls.push_back(sd);
          }
          if (auto pr = ResolvePhysReg(ua, env)) {
            if (*pr >= 0 && *pr < arch_.num_gprs) {
              dep.phys_mask |= (1u << *pr);
            }
          }
        } else if (IsOperandType(ua->getType())) {
          MergeSubOperandDep(dep, ExtractOperandDep(ua, env, fctx));
        }
      }
    }
    return dep;
  }

  void RecordEntryParamReadAndCheckEarlyClobber(unsigned p_idx,
                                                const clang::ValueDecl* sym,
                                                clang::SourceLocation use_loc,
                                                Env& env,
                                                FunctionAnalysisContext& fctx) {
    if (p_idx >= fctx.param_read_entry.size()) return;
    fctx.param_read_entry[p_idx] = true;
    if (p_idx < 32) {
      env.params_read_mask |= (1u << p_idx);
    }
    unsigned lim = std::min<unsigned>(fctx.fn->getNumParams(), 32u);
    for (unsigned w = 0; w < lim; ++w) {
      if (w == p_idx || !(env.params_written_mask & (1u << w))) continue;
      const clang::ValueDecl* w_decl =
          fctx.fn->getParamDecl(w)->getCanonicalDecl();
      if (env.known_no_reg.contains(w_decl)) continue;
      if (env.known_not_equal.contains({w_decl, sym}) ||
          env.known_not_equal.contains({sym, w_decl})) {
        continue;
      }
      fctx.early_clobber_param_pairs.insert({w, p_idx});

      // On ARM64, if the author guarded this overwrite-before-read using
      // size-sensitive `!=` / `!(==)` on CPURegister/Register, warn directly!
      auto ss_it = env.size_sensitive_not_equal.find({w_decl, sym});
      if (ss_it != env.size_sensitive_not_equal.end()) {
        ClobberNote guard_note{
            ss_it->second,
            "size-sensitive '!=' / '==' guard here compares register width "
            "(wN != xN is true on ARM64)"};
        QueueWarning(fctx, use_loc,
                     "Register '" + w_decl->getNameAsString() +
                         "' is overwritten before reading '" +
                         sym->getNameAsString() +
                         "' under a size-sensitive '!=' guard on ARM64; use "
                         "!Aliases() or !AreAliased() instead.",
                     &guard_note);
      }
    }
    fctx.early_clobber_phys_before_param[p_idx] |= env.phys_written_mask;
  }

  static const clang::Expr* UnwrapImplicitRegToOperand(
      const clang::Expr* expr) {
    const clang::Expr* cur = expr;
    while (cur) {
      cur = cur->IgnoreParens();
      if (const auto* clean = llvm::dyn_cast<clang::ExprWithCleanups>(cur)) {
        cur = clean->getSubExpr();
        continue;
      }
      if (const auto* mat =
              llvm::dyn_cast<clang::MaterializeTemporaryExpr>(cur)) {
        cur = mat->getSubExpr();
        continue;
      }
      if (const auto* bind = llvm::dyn_cast<clang::CXXBindTemporaryExpr>(cur)) {
        cur = bind->getSubExpr();
        continue;
      }
      if (const auto* ice = llvm::dyn_cast<clang::ImplicitCastExpr>(cur)) {
        if (ice->getCastKind() == clang::CK_NoOp) {
          cur = ice->getSubExpr();
          continue;
        }
        if (ice->getCastKind() == clang::CK_ConstructorConversion) {
          const clang::Expr* sub = ice->getSubExpr()->IgnoreParens();
          if (const auto* b2 =
                  llvm::dyn_cast<clang::CXXBindTemporaryExpr>(sub)) {
            sub = b2->getSubExpr()->IgnoreParens();
          }
          if (const auto* cce = llvm::dyn_cast<clang::CXXConstructExpr>(sub)) {
            if (cce->getNumArgs() >= 1 &&
                IsTrackableRegType(cce->getArg(0)->getType())) {
              bool rest_default = true;
              for (unsigned i = 1; i < cce->getNumArgs(); ++i) {
                if (!llvm::isa<clang::CXXDefaultArgExpr>(cce->getArg(i))) {
                  rest_default = false;
                  break;
                }
              }
              if (rest_default) return cce->getArg(0);
            }
          }
        }
      }
      break;
    }
    return nullptr;
  }

  void ReadRegisterExpr(const clang::Expr* expr, clang::SourceLocation use_loc,
                        Env& env, FunctionAnalysisContext& fctx) {
    if (!env.reachable || !expr) return;
    if (llvm::isa<clang::CXXDefaultArgExpr>(expr)) return;
    if (const clang::Expr* imp_reg = UnwrapImplicitRegToOperand(expr)) {
      expr = imp_reg;
    }
    const clang::Expr* uexpr = UnwrapExpr(expr);
    if (!uexpr) return;

    if (IsOperandType(uexpr->getType())) {
      ReadOperandExpr(uexpr, use_loc, env, fctx);
      return;
    }
    if (!IsMaglevTrackableRegType(uexpr->getType())) return;
    bool is_double = IsDoubleRegType(uexpr->getType());
    if (is_double && !fctx.maglev_node_cls) return;

    const clang::ValueDecl* sym = ResolveSymDecl(uexpr, env);
    std::optional<int> phys =
        is_double ? std::nullopt : ResolvePhysReg(expr, env);

    if (sym) {
      if (env.known_no_reg.contains(sym)) return;
      auto it = env.syms.find(sym);
      if (it != env.syms.end()) {
        SymRegState& s = it->second;
        if (s.status == RegStatus::kClobbered) {
          QueueWarning(fctx, use_loc,
                       "Use of register '" + sym->getNameAsString() +
                           "' after it was clobbered.",
                       &s.clobber);
        }
        if (s.status == RegStatus::kInitial && s.param_index >= 0) {
          RecordEntryParamReadAndCheckEarlyClobber(
              static_cast<unsigned>(s.param_index), sym, use_loc, env, fctx);
        }
        if (fctx.maglev_node_cls && s.maglev_role == MaglevRole::kInput) {
          CheckMaglevEarlyClobberOnRead(
              sym, s.maglev_input_key, s.maglev_input_label, s.is_double_reg,
              s.holds_entry_value, use_loc, env, fctx);
        }
      }
    } else if (fctx.maglev_node_cls) {
      if (auto m = MatchMaglevToRegisterCall(expr, env)) {
        if (!m->is_result) {
          bool holds_entry = !env.maglev_written_inputs.count(m->input_key);
          CheckMaglevEarlyClobberOnRead(nullptr, m->input_key, m->input_label,
                                        m->is_double, holds_entry, use_loc, env,
                                        fctx);
        }
      }
    }

    if (phys && *phys >= 0 && *phys < arch_.num_gprs) {
      int r = *phys;
      PhysRegState& ps = env.phys[r];
      if (ps.status == RegStatus::kClobbered && !sym) {
        QueueWarning(fctx, use_loc,
                     std::string("Use of physical register '") +
                         arch_.gpr_names[r] + "' after it was clobbered.",
                     &ps.clobber);
      }
      if (ps.status == RegStatus::kInitial && ps.holds_entry_value && !sym) {
        if (!(arch_.pinned_mask & (1u << r))) {
          fctx.phys_read_entry |= (1u << r);
        }
      }
    }
  }

  void ReadOperandExpr(const clang::Expr* expr, clang::SourceLocation use_loc,
                       Env& env, FunctionAnalysisContext& fctx) {
    OperandDep dep = ExtractOperandDep(expr, env, fctx);
    std::string kind_name =
        IsTypeNamed(expr->getType(), "MemOperand") ? "MemOperand" : "Operand";
    std::string op_name = kind_name;
    if (const auto* dre =
            llvm::dyn_cast_or_null<clang::DeclRefExpr>(UnwrapExpr(expr))) {
      op_name = kind_name + " '" + dre->getDecl()->getNameAsString() + "'";
    }

    for (const auto& [r, ver] : dep.scratch_phys_versions) {
      if (r >= 0 && r < arch_.num_gprs && env.phys[r].def_version != ver) {
        QueueWarning(fctx, use_loc,
                     "Use of " + op_name +
                         " after its underlying scratch register '" +
                         arch_.gpr_names[r] + "' was overwritten.",
                     &env.phys[r].clobber);
      }
    }

    for (const auto& [sd, ver] : dep.scratch_sym_versions) {
      if (!sd || env.known_no_reg.contains(sd)) continue;
      auto it = env.syms.find(sd);
      if (it != env.syms.end() && it->second.def_version != ver) {
        QueueWarning(fctx, use_loc,
                     "Use of " + op_name +
                         " after its underlying scratch register '" +
                         sd->getNameAsString() + "' was overwritten.",
                     &it->second.clobber);
      }
    }

    for (const clang::ValueDecl* sd : dep.sym_decls) {
      if (env.known_no_reg.contains(sd)) continue;
      auto it = env.syms.find(sd);
      if (it != env.syms.end()) {
        SymRegState& s = it->second;
        if (s.status == RegStatus::kClobbered) {
          QueueWarning(fctx, use_loc,
                       "Use of " + op_name + " whose base/index register '" +
                           sd->getNameAsString() + "' was clobbered.",
                       &s.clobber);
        }
        if (s.status == RegStatus::kInitial && s.param_index >= 0) {
          RecordEntryParamReadAndCheckEarlyClobber(
              static_cast<unsigned>(s.param_index), sd, use_loc, env, fctx);
        }
        if (fctx.maglev_node_cls && s.maglev_role == MaglevRole::kInput) {
          CheckMaglevEarlyClobberOnRead(
              sd, s.maglev_input_key, s.maglev_input_label, s.is_double_reg,
              s.holds_entry_value, use_loc, env, fctx);
        }
      }
    }

    for (int r = 0; r < arch_.num_gprs; ++r) {
      if (dep.phys_mask & (1u << r)) {
        PhysRegState& ps = env.phys[r];
        if (ps.status == RegStatus::kClobbered && dep.sym_decls.empty()) {
          QueueWarning(fctx, use_loc,
                       "Use of " + op_name + " whose physical register '" +
                           arch_.gpr_names[r] + "' was clobbered.",
                       &ps.clobber);
        }
        if (ps.status == RegStatus::kInitial && ps.holds_entry_value &&
            dep.sym_decls.empty()) {
          if (!(arch_.pinned_mask & (1u << r))) {
            fctx.phys_read_entry |= (1u << r);
          }
        }
      }
    }
  }

  void RecordMaglevResultWrite(const clang::ValueDecl* sym,
                               const SymRegState* sym_state, bool is_double,
                               clang::SourceLocation loc, Env& env) {
    std::map<std::string, std::vector<MaglevCondAtom>> cur_distinct;
    if (sym_state) {
      cur_distinct = sym_state->known_distinct_from_inputs;
    }
    if (sym) {
      for (const auto& [d, ds] : env.syms) {
        if (ds.maglev_role != MaglevRole::kInput ||
            ds.maglev_input_key.empty()) {
          continue;
        }
        if (env.known_not_equal.contains({sym, d}) ||
            env.known_not_equal.contains({d, sym})) {
          cur_distinct[ds.maglev_input_key] = {};
        }
      }
    }
    if (!env.maglev_result_written) {
      env.maglev_result_distinct_inputs = std::move(cur_distinct);
    } else {
      std::map<std::string, std::vector<MaglevCondAtom>> inter;
      for (const auto& [k, prev_conds] : env.maglev_result_distinct_inputs) {
        auto it = cur_distinct.find(k);
        if (it == cur_distinct.end()) continue;
        if (prev_conds == it->second) {
          inter[k] = prev_conds;
        } else if (prev_conds.empty()) {
          inter[k] = it->second;
        } else if (it->second.empty()) {
          inter[k] = prev_conds;
        }
      }
      env.maglev_result_distinct_inputs = std::move(inter);
    }
    env.maglev_result_written = true;
    env.maglev_result_write_loc = loc;
    if (sym) env.maglev_result_sym = sym;
    env.maglev_result_is_double = is_double;
  }

  void WriteOrClobberSym(const clang::ValueDecl* sym, bool is_clobber,
                         clang::SourceLocation loc, const std::string& reason,
                         Env& env, FunctionAnalysisContext& fctx) {
    if (!sym || env.known_no_reg.contains(sym)) return;
    auto it = env.syms.find(sym);
    if (it == env.syms.end()) return;
    SymRegState& s = it->second;

    if (s.readonly_kind == ReadonlyKind::kExplicitAttr &&
        !IsSymSavedOnStack(sym, s.phys_reg, env)) {
      QueueWarning(fctx, loc,
                   "Cannot modify/clobber read-only register '" +
                       sym->getNameAsString() + "' (" + s.readonly_reason +
                       ").");
    }

    if (s.param_index >= 0) {
      unsigned p_idx = static_cast<unsigned>(s.param_index);
      bool aliases_known_out_param = false;
      if (p_idx < fctx.explicit_sum.param_roles.size() &&
          IsInputLikeRole(fctx.explicit_sum.param_roles[p_idx])) {
        for (unsigned k = 0; k < fctx.fn->getNumParams() &&
                             k < fctx.explicit_sum.param_roles.size();
             ++k) {
          if (k == p_idx) continue;
          ParamRole rk = fctx.explicit_sum.param_roles[k];
          if (rk == ParamRole::kOut || rk == ParamRole::kInOut ||
              rk == ParamRole::kScratch || rk == ParamRole::kInClobber) {
            const clang::ValueDecl* pk =
                fctx.fn->getParamDecl(k)->getCanonicalDecl();
            if (env.known_equal.contains({sym, pk}) ||
                env.known_equal.contains({pk, sym})) {
              aliases_known_out_param = true;
              fctx.param_written_any[k] = true;
              break;
            }
          }
        }
      }
      if (!aliases_known_out_param) {
        if (s.status == RegStatus::kInitial && p_idx < 32 &&
            !(env.params_read_mask & (1u << p_idx))) {
          fctx.param_written_before_read_any[p_idx] = true;
        }
        if (p_idx < 32) {
          env.params_written_mask |= (1u << p_idx);
        }
        if (is_clobber) {
          fctx.param_clobbered_any[p_idx] = true;
        } else {
          fctx.param_written_any[p_idx] = true;
        }
      }
    }
    s.status = is_clobber ? RegStatus::kClobbered : RegStatus::kAlive;
    s.holds_entry_value = false;
    s.def_version = fctx.next_version++;
    if (is_clobber) {
      s.clobber = {loc, reason};
    } else {
      s.clobber = {loc, "overwritten here"};
    }
    if (fctx.maglev_node_cls) {
      if (s.maglev_role == MaglevRole::kResult) {
        RecordMaglevResultWrite(sym, &s, s.is_double_reg, loc, env);
      } else if (s.maglev_role == MaglevRole::kInput &&
                 !s.maglev_input_key.empty()) {
        env.maglev_written_inputs.try_emplace(s.maglev_input_key, loc);
        const MaglevNodeSpec& spec = GetMaglevNodeSpec(fctx.maglev_node_cls);
        if (spec.has_constraints_body) {
          auto paths = GetCompatibleMaglevPaths(spec, env);
          for (const MaglevConstraintPath* p : paths) {
            if (MatchesSameAsInput(spec, s.maglev_input_key, *p)) {
              RecordMaglevResultWrite(sym, &s, s.is_double_reg, loc, env);
              break;
            }
          }
        }
      }
    }
    if (!s.is_double_reg && s.phys_reg >= 0 && s.phys_reg < arch_.num_gprs) {
      WriteOrClobberPhys(s.phys_reg, is_clobber,
                         /*from_sym_or_scratch_scope=*/
                         (s.param_index >= 0 || s.scratch_slot >= 0 ||
                          s.maglev_role != MaglevRole::kNone),
                         loc, reason, env, fctx, /*skip_sym=*/sym);
    }
  }

  void WriteOrClobberPhys(int r, bool is_clobber, bool from_sym_param,
                          clang::SourceLocation loc, const std::string& reason,
                          Env& env, FunctionAnalysisContext& fctx,
                          const clang::ValueDecl* skip_sym = nullptr,
                          bool check_maglev_phys = true,
                          bool record_in_summary = true) {
    if (r < 0 || r >= arch_.num_gprs) return;
    bool is_pinned = (arch_.pinned_mask & (1u << r)) != 0;
    if (is_pinned && !(fctx.maglev_node_cls &&
                       (arch_.maglev_allocatable_mask & (1u << r)))) {
      return;
    }

    bool skip_is_maglev_result =
        skip_sym && env.syms.count(skip_sym) &&
        env.syms[skip_sym].maglev_role == MaglevRole::kResult;
    for (auto& [decl, s] : env.syms) {
      if (decl == skip_sym || s.is_double_reg) continue;
      if (s.phys_reg == r) {
        if (s.readonly_kind == ReadonlyKind::kExplicitAttr &&
            !IsSymSavedOnStack(decl, r, env)) {
          QueueWarning(fctx, loc,
                       "Cannot modify/clobber read-only register '" +
                           decl->getNameAsString() + "' (" + s.readonly_reason +
                           ").");
        }
        if (skip_sym == nullptr && check_maglev_phys && fctx.maglev_node_cls &&
            s.maglev_role == MaglevRole::kInput) {
          CheckMaglevReadonlyInputWrite(decl, s.maglev_input_key,
                                        s.maglev_input_label, r, "", loc, env,
                                        fctx);
        }
        if (!skip_is_maglev_result && fctx.maglev_node_cls &&
            s.maglev_role == MaglevRole::kResult) {
          RecordMaglevResultWrite(decl, &s, false, loc, env);
        }
        if (s.param_index >= 0) {
          from_sym_param = true;
          unsigned p_idx = static_cast<unsigned>(s.param_index);
          if (s.status == RegStatus::kInitial && p_idx < 32 &&
              !(env.params_read_mask & (1u << p_idx))) {
            fctx.param_written_before_read_any[p_idx] = true;
          }
          if (p_idx < 32) {
            env.params_written_mask |= (1u << p_idx);
          }
          if (is_clobber) {
            fctx.param_clobbered_any[p_idx] = true;
          } else {
            fctx.param_written_any[p_idx] = true;
          }
        }
        bool sym_clobbered =
            is_clobber || (skip_sym == nullptr && s.scratch_slot >= 0);
        s.status = sym_clobbered ? RegStatus::kClobbered : RegStatus::kAlive;
        s.holds_entry_value = false;
        s.def_version = fctx.next_version++;
        s.clobber = {loc, reason};
      }
    }

    PhysRegState& ps = env.phys[r];
    ps.status = is_clobber ? RegStatus::kClobbered : RegStatus::kAlive;
    ps.holds_entry_value = false;
    ps.def_version = fctx.next_version++;
    ps.clobber = {loc, reason};

    if (!from_sym_param) {
      if (record_in_summary) {
        env.phys_written_mask |= (1u << r);
        if (is_clobber) {
          fctx.phys_clobbered_any |= (1u << r);
        } else {
          fctx.phys_written_any |= (1u << r);
        }
      }
      if (check_maglev_phys && fctx.maglev_node_cls) {
        CheckMaglevPhysWrite(r, loc, reason, env, fctx);
        const MaglevNodeSpec& spec = GetMaglevNodeSpec(fctx.maglev_node_cls);
        if (spec.has_constraints_body) {
          auto paths = GetCompatibleMaglevPaths(spec, env);
          for (const MaglevConstraintPath* p : paths) {
            if (p->result.kind == MaglevResultKind::kDefineAsFixed &&
                !p->result.is_double_fixed && p->result.fixed_reg == r) {
              RecordMaglevResultWrite(nullptr, nullptr, false, loc, env);
              break;
            }
          }
        }
      }
    }
  }

  ScratchScopeKind MatchScratchAcquireCall(const clang::Expr* expr) {
    expr = UnwrapExpr(expr);
    const auto* mce = llvm::dyn_cast_or_null<clang::CXXMemberCallExpr>(expr);
    if (!mce) return ScratchScopeKind::kNone;
    const auto* md = mce->getMethodDecl();
    if (!md || !IsCoreGprRegType(mce->getType())) {
      return ScratchScopeKind::kNone;
    }
    ScratchScopeKind sk = GetScratchScopeKind(mce->getObjectType());
    if (sk == ScratchScopeKind::kNone) return ScratchScopeKind::kNone;
    llvm::StringRef mname = SafeDeclName(md);
    if (mname == "AcquireScratch" || mname == "AcquireX" ||
        mname == "AcquireW" || mname == "AcquireSameSizeAs" ||
        mname == "Acquire") {
      return sk;
    }
    return ScratchScopeKind::kNone;
  }

  static bool TakesAssemblerParam(const clang::FunctionDecl* fn) {
    if (!fn) return false;
    for (unsigned i = 0; i < fn->getNumParams(); ++i) {
      clang::QualType pt = fn->getParamDecl(i)->getType();
      clang::QualType deref =
          pt->isPointerType() ? pt->getPointeeType() : pt.getNonReferenceType();
      if (const auto* rec = deref->getAsCXXRecordDecl()) {
        if (GetRecordKind(rec) != RecordKind::kOther) return true;
      }
    }
    return false;
  }

  void ApplyCallEffect(const clang::CallExpr* ce, Env& env,
                       FunctionAnalysisContext& fctx) {
    if (!env.reachable) return;
    const clang::FunctionDecl* callee = ResolveCalleeDecl(ce);
    if (!callee) return;

    std::string callee_name = callee->getNameAsString();
    clang::SourceLocation call_loc = ce->getExprLoc();

    // 1. Handle UNREACHABLE / Abort / Fatals / DCHECK failure handlers
    if (callee_name == "V8_Fatal" || callee_name == "UNREACHABLE" ||
        callee_name == "V8_Dcheck" || callee_name == "FatalNoSecurityImpact") {
      env.reachable = false;
      return;
    }

    // 1b. Handle Maglev ToRegister / ToDoubleRegister / ToRegisterT checks
    // and MakeDeferredCode / JumpToDeferredIf callbacks.
    if (auto m = MatchMaglevToRegisterCall(ce, env)) {
      CheckMaglevToRegisterUse(*m, call_loc, env, fctx);
      return;
    }
    if (callee_name == "MakeDeferredCode" ||
        callee_name == "JumpToDeferredIf") {
      AnalyzeMaglevDeferredCallback(ce, callee_name, env, fctx);
      return;
    }

    // 2. Handle UseScratchRegisterScope / TemporaryRegisterScope methods, and
    // ignore host-side C++ metadata queries on Register/Operand/RegList types
    // (such as `.Aliases()`, `AreAliased()`, `DCHECK_NE()`, `RegList::has()`),
    // as well as `const` or pure `static` methods on Assembler/MacroAssembler
    // (such as `Rd(rd)`, `Rt(rt)`, `AppropriateZeroRegFor(dst)`), which cannot
    // emit instructions into an assembler buffer.
    if (llvm::isa<clang::CXXOperatorCallExpr>(ce)) {
      return;
    }
    if (const auto* md = llvm::dyn_cast<clang::CXXMethodDecl>(callee)) {
      if (GetRecordKind(md->getParent()) != RecordKind::kOther) {
        if (md->isConst() || (md->isStatic() && !TakesAssemblerParam(md))) {
          return;
        }
      }
    }
    if (const auto* mce = llvm::dyn_cast<clang::CXXMemberCallExpr>(ce)) {
      clang::QualType obj_qt = mce->getObjectType();
      ScratchScopeKind sk = GetScratchScopeKind(obj_qt);
      if (sk == ScratchScopeKind::kMaglevTemp) {
        bool acq_scratch = false;
        bool acq_double = false;
        if (MatchMaglevTempAcquireCall(ce, &acq_scratch, &acq_double)) {
          AccountMaglevTempUse(acq_scratch, acq_double, 1, 1, call_loc,
                               "TemporaryRegisterScope::" + callee_name + "()",
                               env, fctx);
          return;
        }
        auto check_included_regs =
            [&](const std::vector<const clang::Expr*>& regs) {
              if (!fctx.maglev_node_cls) return;
              for (const clang::Expr* re : regs) {
                if (const clang::ValueDecl* sym = ResolveSymDecl(re, env)) {
                  auto sit = env.syms.find(sym);
                  if (sit != env.syms.end() &&
                      sit->second.maglev_role == MaglevRole::kInput) {
                    CheckMaglevReadonlyInputWrite(
                        sym, sit->second.maglev_input_key,
                        sit->second.maglev_input_label, sit->second.phys_reg,
                        callee_name, call_loc, env, fctx);
                  }
                } else if (auto m = MatchMaglevToRegisterCall(re, env)) {
                  if (!m->is_result) {
                    CheckMaglevReadonlyInputWrite(
                        nullptr, m->input_key, m->input_label, -1, callee_name,
                        call_loc, env, fctx);
                  }
                }
              }
            };
        if (callee_name == "Include" || callee_name == "IncludeDouble") {
          bool is_double = (callee_name == "IncludeDouble");
          for (const clang::Expr* arg : ce->arguments()) {
            if (llvm::isa<clang::CXXDefaultArgExpr>(arg)) continue;
            std::vector<const clang::Expr*> regs;
            if (ExtractRegListExprs(arg, regs)) {
              check_included_regs(regs);
              if (is_double) {
                env.maglev_double_temps_included +=
                    static_cast<int>(regs.size());
              } else {
                env.maglev_gpr_temps_included += static_cast<int>(regs.size());
              }
            } else {
              if (is_double) {
                env.maglev_double_temps_unbounded = true;
              } else {
                env.maglev_gpr_temps_unbounded = true;
              }
            }
          }
          return;
        }
        if (callee_name == "IncludeScratch") {
          for (const clang::Expr* arg : ce->arguments()) {
            if (llvm::isa<clang::CXXDefaultArgExpr>(arg)) continue;
            std::vector<const clang::Expr*> regs;
            if (ExtractRegListExprs(arg, regs)) {
              check_included_regs(regs);
            }
            if (IsDoubleRegType(arg->getType())) {
              env.maglev_double_scratches_used -= 1;
            } else {
              env.maglev_gpr_scratches_used -= 1;
              if (auto r = ResolvePhysReg(arg, env)) {
                if (*r >= 0 && *r < arch_.num_gprs) {
                  env.arm64_avail_scratch_mask |= (1u << *r);
                }
              }
            }
          }
          return;
        }
        if (callee_name == "SetAvailable" ||
            callee_name == "SetAvailableDouble") {
          bool is_double = (callee_name == "SetAvailableDouble");
          if (ce->getNumArgs() >= 1) {
            const clang::Expr* arg0 = ce->getArg(0);
            if (IsSubFromAvailableExpr(arg0)) {
              return;
            }
            std::vector<const clang::Expr*> regs;
            if (ExtractRegListExprs(arg0, regs)) {
              check_included_regs(regs);
              env.maglev_temps_reset = true;
              if (is_double) {
                env.maglev_double_temps_included =
                    static_cast<int>(regs.size());
                env.maglev_double_temps_used = 0;
                env.maglev_double_temps_unbounded = false;
              } else {
                env.maglev_gpr_temps_included = static_cast<int>(regs.size());
                env.maglev_gpr_temps_used = 0;
                env.maglev_gpr_temps_unbounded = false;
              }
            } else {
              if (is_double) {
                env.maglev_double_temps_unbounded = true;
              } else {
                env.maglev_gpr_temps_unbounded = true;
              }
            }
          }
          return;
        }
        if (callee_name == "ResetToDefault") {
          env.maglev_temps_reset = true;
          env.maglev_gpr_temps_used = 0;
          env.maglev_double_temps_used = 0;
          env.maglev_gpr_temps_included = 0;
          env.maglev_double_temps_included = 0;
          env.maglev_gpr_temps_unbounded = false;
          env.maglev_double_temps_unbounded = false;
          env.maglev_gpr_scratches_used = 0;
          env.maglev_double_scratches_used = 0;
          env.arm64_avail_scratch_mask = arch_.arm64_default_tmp_mask;
          return;
        }
        return;
      }
      if (sk != ScratchScopeKind::kNone) {
        if (callee_name == "Exclude" || callee_name == "Include" ||
            callee_name == "ExcludeAll") {
          if (callee_name == "ExcludeAll") {
            env.arm64_avail_scratch_mask = 0;
            env.arm64_included_scratch_mask = 0;
            return;
          }
          for (const clang::Expr* arg : ce->arguments()) {
            if (llvm::isa<clang::CXXDefaultArgExpr>(arg)) continue;
            if (auto r = ResolvePhysReg(arg, env)) {
              if (*r >= 0 && *r < arch_.num_gprs) {
                if (callee_name == "Exclude") {
                  env.arm64_avail_scratch_mask &= ~(1u << *r);
                  env.arm64_included_scratch_mask &= ~(1u << *r);
                } else {
                  env.arm64_avail_scratch_mask |= (1u << *r);
                  env.arm64_included_scratch_mask |= (1u << *r);
                }
              }
            }
          }
          return;
        }
        return;  // Acquire* is handled at VarDecl init / assignment; others are
                 // metadata.
      }
      if (IsTypeNamed(obj_qt, "RegListBase")) {
        if (callee_name == "clear" && ce->getNumArgs() >= 1) {
          if (const clang::ValueDecl* cleared =
                  ResolveSymDecl(ce->getArg(0), env)) {
            env.maglev_snapshot_cleared_syms.insert(cleared);
          }
        }
        return;
      }
      if (IsMaglevTrackableRegType(obj_qt) || IsOperandType(obj_qt) ||
          IsTypeNamed(obj_qt, "CPURegList") ||
          IsTypeNamed(obj_qt, "CacheState") ||
          IsTypeNamed(obj_qt, "VarState")) {
        return;
      }
    }
    if (const auto* md_static = llvm::dyn_cast<clang::CXXMethodDecl>(callee)) {
      if (md_static->isStatic()) {
        clang::QualType parent_qt =
            ast_ctx_.getCanonicalTagType(md_static->getParent());
        if (IsTrackableRegType(parent_qt) || IsOperandType(parent_qt)) {
          return;
        }
      }
    }
    if (callee_name == "AreAliased" || callee_name == "AreSameSizeAndType" ||
        callee_name == "AreSameFormat" || callee_name == "AreConsecutive" ||
        callee_name == "AreEven" || callee_name == "CheckOpImpl" ||
        callee_name == "MakeCheckOpValueString" ||
        callee_name == "MakeCheckOpString" ||
        callee_name == "GetAllocatableRegisterThatIsNotOneOf" ||
        callee_name == "SafeCall" || callee_name == "CallIf" ||
        callee_name == "CallFrameSetupStub") {
      return;
    }

    // 3. Handle PushCallerSaved / PopCallerSaved / PushAll / PopAll /
    // MaybeSaveRegisters / MaybeRestoreRegisters
    if (callee_name == "PushCallerSaved" || callee_name == "PushAll" ||
        callee_name == "MaybeSaveRegisters") {
      SavedStackEntry entry;
      entry.kind = (callee_name == "PushCallerSaved")
                       ? SavedStackEntry::kCallerSaved
                       : SavedStackEntry::kAllRegs;
      for (int r = 0; r < arch_.num_gprs; ++r) {
        entry.saved_phys_all[r] = env.phys[r];
      }
      entry.saved_syms_all = env.syms;
      entry.saved_clobbered_caller_saved = env.clobbered_caller_saved;
      env.asm_stack.push_back(std::move(entry));
      return;
    }
    if (callee_name == "PopCallerSaved" || callee_name == "PopAll" ||
        callee_name == "MaybeRestoreRegisters") {
      const clang::ValueDecl* excl_sym = nullptr;
      std::optional<int> excl_phys;
      if (callee_name == "PopCallerSaved" && ce->getNumArgs() >= 2 &&
          !llvm::isa<clang::CXXDefaultArgExpr>(ce->getArg(1))) {
        excl_sym = ResolveSymDecl(ce->getArg(1), env);
        excl_phys = ResolvePhysReg(ce->getArg(1), env);
      }
      if (!env.asm_stack.empty() &&
          (env.asm_stack.back().kind == SavedStackEntry::kCallerSaved ||
           env.asm_stack.back().kind == SavedStackEntry::kAllRegs)) {
        SavedStackEntry entry = std::move(env.asm_stack.back());
        env.asm_stack.pop_back();
        env.clobbered_caller_saved = entry.saved_clobbered_caller_saved;
        for (int r = 0; r < arch_.num_gprs; ++r) {
          if (excl_phys && *excl_phys == r) continue;
          env.phys[r] = entry.saved_phys_all[r];
        }
        for (const auto& [d, saved_s] : entry.saved_syms_all) {
          if (excl_sym && d == excl_sym) continue;
          env.syms[d] = saved_s;
        }
      }
      return;
    }

    // 4. Special case: CallBuiltin / near_call / Call with register-preserving
    // write-barrier builtins (RecordWrite, EphemeronKeyBarrier,
    // TrustedPointerBarrier).
    if ((callee_name == "CallBuiltin" || callee_name == "near_call" ||
         callee_name == "Call") &&
        ce->getNumArgs() >= 1) {
      std::string caller_name = fctx.fn->getNameAsString();
      if ((callee_name == "near_call" || callee_name == "Call") &&
          caller_name.find("RecordWrite") != std::string::npos) {
        if (arch_.kind == ArchKind::kArm64) {
          WriteOrClobberPhys(30, /*is_clobber=*/true, /*from_sym_param=*/false,
                             call_loc, "clobbered by '" + callee_name + "'",
                             env, fctx);
        }
        return;
      }
      const clang::Expr* b_arg = UnwrapExpr(ce->getArg(0));
      for (int hop = 0; hop < 4 && b_arg; ++hop) {
        const auto* b_dre = llvm::dyn_cast<clang::DeclRefExpr>(b_arg);
        if (!b_dre) break;
        auto bit =
            env.local_expr_bindings.find(CanonValueDecl(b_dre->getDecl()));
        if (bit == env.local_expr_bindings.end() || !bit->second) break;
        const clang::Expr* next = UnwrapExpr(bit->second);
        if (next == b_arg) break;
        b_arg = next;
      }
      std::string b_str;
      llvm::raw_string_ostream os(b_str);
      if (b_arg) {
        b_arg->printPretty(os, nullptr, ast_ctx_.getPrintingPolicy());
      }
      os.flush();
      if (b_str.find("RecordWrite") != std::string::npos ||
          b_str.find("EphemeronKeyBarrier") != std::string::npos ||
          b_str.find("TrustedPointerBarrier") != std::string::npos ||
          b_str.find("kDoubleToI") != std::string::npos ||
          b_str.find("kDumpFrame") != std::string::npos ||
          b_str.find("MaglevFunctionEntryStackCheck") != std::string::npos ||
          b_str == "target" || b_str == "builtin") {
        if ((b_str != "target" && b_str != "builtin") ||
            caller_name.find("Barrier") != std::string::npos ||
            caller_name.find("RecordWrite") != std::string::npos) {
          if (arch_.kind == ArchKind::kArm64 &&
              b_str.find("kDoubleToI") == std::string::npos &&
              b_str.find("kDumpFrame") == std::string::npos) {
            WriteOrClobberPhys(30, /*is_clobber=*/true,
                               /*from_sym_param=*/false, call_loc,
                               "clobbered by '" + callee_name + "'", env, fctx);
          }
          return;
        }
      }
    }

    const FuncSummary& sum = GetOrInferSummary(callee);

    // 5. Check for Zeroing Idiom:
    // - 2-arg x64: `xorq(reg, reg)` / `subq(reg, reg)`
    // - 3-arg arm64: `eor(rd, rn, Operand(rn))` / `sub(rd, rn, Operand(rn))`
    bool is_same_reg_zeroing = false;
    if (sum.zeroing_if_same && ce->getNumArgs() >= 2) {
      unsigned idx_a = (ce->getNumArgs() >= 3) ? 1 : 0;
      unsigned idx_b = (ce->getNumArgs() >= 3) ? 2 : 1;
      const clang::ValueDecl* sa = ResolveSymDecl(ce->getArg(idx_a), env);
      const clang::ValueDecl* sb = ResolveSymDecl(ce->getArg(idx_b), env);
      std::optional<int> pa = ResolvePhysReg(ce->getArg(idx_a), env);
      std::optional<int> pb = ResolvePhysReg(ce->getArg(idx_b), env);
      if (IsOperandType(UnwrapExpr(ce->getArg(idx_b))->getType())) {
        OperandDep b_dep = ExtractOperandDep(ce->getArg(idx_b), env, fctx);
        if (b_dep.sym_decls.size() == 1 && b_dep.phys_mask == 0) {
          sb = b_dep.sym_decls[0];
        }
      }
      if ((sa && sa == sb) || (pa && pb && *pa >= 0 && *pa == *pb)) {
        is_same_reg_zeroing = true;
      }
    }

    // 6. Check for Early-Clobber / Input-Output Aliasing at Call Site!
    unsigned num_args =
        std::min<unsigned>(ce->getNumArgs(), sum.param_roles.size());
    if (fctx.emit_diagnostics) {
      CheckCallSiteAliasing(ce, callee, sum, num_args, env, fctx);
    }

    // 7. Process all READS first (within a single instruction/call, inputs are
    // read before outputs/clobbers are written).
    for (unsigned i = 0; i < num_args; ++i) {
      ParamRole role = sum.param_roles[i];
      if (is_same_reg_zeroing) {
        if (ce->getNumArgs() == 2 && (i == 0 || i == 1)) continue;
        if (ce->getNumArgs() >= 3 && (i == 1 || i == 2)) continue;
      }
      if (IsInputLikeRole(role) || role == ParamRole::kInOut ||
          role == ParamRole::kInClobber) {
        ReadRegisterExpr(ce->getArg(i), ce->getArg(i)->getExprLoc(), env, fctx);
      }
    }

    // Implicit physical register reads:
    for (int r = 0; r < arch_.num_gprs; ++r) {
      if (sum.implicit_in & (1u << r)) {
        PhysRegState& ps = env.phys[r];
        if (ps.status == RegStatus::kClobbered) {
          QueueWarning(fctx, call_loc,
                       "Call to '" + callee_name +
                           "' implicitly reads register '" +
                           arch_.gpr_names[r] + "' after it was clobbered.",
                       &ps.clobber);
        }
        if (ps.status == RegStatus::kInitial && ps.holds_entry_value) {
          if (!(arch_.pinned_mask & (1u << r))) {
            fctx.phys_read_entry |= (1u << r);
          }
        }
      }
    }

    // 8. Handle V8_ASM_PUSH before writes (supports multi-register Push(r0,
    // r1, ...) in order!)
    if (sum.is_push && ce->getNumArgs() >= 1) {
      for (unsigned i = 0; i < ce->getNumArgs(); ++i) {
        const clang::Expr* argi = ce->getArg(i);
        if (llvm::isa<clang::CXXDefaultArgExpr>(argi)) continue;
        const clang::Expr* uarg = UnwrapExpr(argi);
        if (!uarg || !IsTrackableRegType(uarg->getType())) continue;
        std::optional<int> p = ResolvePhysReg(argi, env);
        if (p && *p == -1) break;  // NoReg terminates ARM64 Push list

        SavedStackEntry entry;
        entry.kind = SavedStackEntry::kSingleReg;
        entry.root_decl = ResolveSymDecl(uarg, env);
        if (entry.root_decl && env.syms.count(entry.root_decl)) {
          entry.saved_sym = env.syms[entry.root_decl];
          if (i == 0 && entry.saved_sym.param_index == 0) {
            fctx.any_push_param0 = true;
          }
        }
        if (p && *p >= 0 && *p < arch_.num_gprs) {
          entry.phys_reg = *p;
          entry.saved_phys = env.phys[*p];
        }
        env.asm_stack.push_back(std::move(entry));
      }
    }

    // 9. Process explicit parameter WRITES and CLOBBERS
    for (unsigned i = 0; i < num_args; ++i) {
      ParamRole role = sum.param_roles[i];
      if (is_same_reg_zeroing && i == 0) {
        role = ParamRole::kOut;
      }
      if (role == ParamRole::kOut || role == ParamRole::kInOut ||
          role == ParamRole::kScratch || role == ParamRole::kInClobber) {
        bool is_clobber =
            (role == ParamRole::kScratch || role == ParamRole::kInClobber);
        const clang::Expr* arg = ce->getArg(i);
        if (llvm::isa<clang::CXXDefaultArgExpr>(arg)) {
          if (auto def_phys = EvaluateRegisterCode(arg, ast_ctx_, arch_)) {
            if (*def_phys >= 0 && *def_phys < arch_.num_gprs) {
              WriteOrClobberPhys(*def_phys, is_clobber,
                                 /*from_sym_param=*/false, call_loc,
                                 "clobbered by default scratch argument in '" +
                                     callee_name + "'",
                                 env, fctx);
            }
          }
          continue;
        }
        const clang::Expr* uarg = UnwrapExpr(arg);
        if (!uarg || !IsMaglevTrackableRegType(uarg->getType())) continue;
        bool arg_is_double = IsDoubleRegType(uarg->getType());
        if (arg_is_double && !fctx.maglev_node_cls) continue;

        const clang::ValueDecl* sym = ResolveSymDecl(uarg, env);
        std::optional<int> phys =
            arg_is_double ? std::nullopt : ResolvePhysReg(arg, env);

        // Check Maglev read-only input constraints (both bound variables and
        // inline `ToRegister(Input())` calls) and inline `ToRegister(result())`
        // writes:
        if (fctx.maglev_node_cls) {
          bool benign_ext = IsBenignMaglevSameRegExt(callee_name, ce, i, env);
          if (sym && env.syms.count(sym)) {
            const SymRegState& s = env.syms[sym];
            if (!benign_ext && s.maglev_role == MaglevRole::kInput) {
              CheckMaglevReadonlyInputWrite(sym, s.maglev_input_key,
                                            s.maglev_input_label, s.phys_reg,
                                            callee_name, call_loc, env, fctx);
            }
          } else if (!sym) {
            if (auto m = MatchMaglevToRegisterCall(arg, env)) {
              if (m->is_result) {
                RecordMaglevResultWrite(nullptr, nullptr, m->is_double,
                                        call_loc, env);
              } else {
                if (!benign_ext) {
                  CheckMaglevReadonlyInputWrite(nullptr, m->input_key,
                                                m->input_label, -1, callee_name,
                                                call_loc, env, fctx);
                }
                env.maglev_written_inputs.try_emplace(m->input_key, call_loc);
                const MaglevNodeSpec& spec =
                    GetMaglevNodeSpec(fctx.maglev_node_cls);
                if (spec.has_constraints_body) {
                  auto paths = GetCompatibleMaglevPaths(spec, env);
                  for (const MaglevConstraintPath* p : paths) {
                    if (MatchesSameAsInput(spec, m->input_key, *p)) {
                      RecordMaglevResultWrite(nullptr, nullptr, m->is_double,
                                              call_loc, env);
                      break;
                    }
                  }
                }
              }
            }
          }
        }

        std::string reason = (is_clobber ? "clobbered as scratch argument to '"
                                         : "overwritten by '") +
                             callee_name + "'";
        if (sym) {
          WriteOrClobberSym(sym, is_clobber, call_loc, reason, env, fctx);
        } else if (phys && *phys >= 0 && *phys < arch_.num_gprs) {
          WriteOrClobberPhys(*phys, is_clobber, /*from_sym_param=*/false,
                             call_loc, reason, env, fctx);
        }
      }
    }

    // 10. Handle V8_ASM_POP (restores saved register state if matching top of
    // stack; on ARM64 `Pop(a, b)` pops `a` then `b` where `Push(b, a)` pushed
    // `b` then `a`, so `Pop(a, b)` pops in argument order!)
    if (sum.is_pop && ce->getNumArgs() >= 1) {
      for (unsigned i = 0; i < ce->getNumArgs(); ++i) {
        const clang::Expr* argi = ce->getArg(i);
        if (llvm::isa<clang::CXXDefaultArgExpr>(argi)) continue;
        const clang::Expr* uarg = UnwrapExpr(argi);
        if (!uarg || !IsTrackableRegType(uarg->getType())) continue;
        std::optional<int> phys = ResolvePhysReg(argi, env);
        if (phys && *phys == -1) break;

        const clang::ValueDecl* sym = ResolveSymDecl(uarg, env);
        if (i == 0 && sym && env.syms.count(sym) &&
            env.syms[sym].param_index == 0) {
          fctx.any_pop_param0 = true;
        }
        if (!env.asm_stack.empty() &&
            env.asm_stack.back().kind == SavedStackEntry::kSingleReg) {
          // If an intervening runtime call popped its pushed stack arguments,
          // scan down `env.asm_stack` to match `sym` when present.
          if (env.clobbered_caller_saved && sym &&
              env.asm_stack.back().root_decl != sym) {
            for (int k = static_cast<int>(env.asm_stack.size()) - 1; k >= 0;
                 --k) {
              if (env.asm_stack[k].kind != SavedStackEntry::kSingleReg) break;
              if (env.asm_stack[k].root_decl == sym) {
                env.asm_stack.resize(static_cast<size_t>(k) + 1);
                break;
              }
            }
          }
          SavedStackEntry top = std::move(env.asm_stack.back());
          env.asm_stack.pop_back();
          if (sym && top.root_decl == sym) {
            env.syms[sym] = top.saved_sym;
          }
          if (phys && *phys >= 0 && top.phys_reg == *phys) {
            env.phys[*phys] = top.saved_phys;
          }
        }
      }
    }

    // 11. Process implicit physical register CLOBBERS and OUTS
    if (sum.clobbers_caller_saved) {
      if (env.maglev_save_call_depth == 0 && !sum.no_fallthrough) {
        fctx.maglev_called_outside_save_scope = true;
      }
      CheckMaglevCallProperty(callee_name, sum, call_loc, env, fctx);
      env.clobbered_caller_saved = true;
      std::string reason =
          "clobbered by caller-saved call '" + callee_name + "'";
      bool ret0_was_initial =
          (env.phys[arch_.ret_reg0].status == RegStatus::kInitial);
      bool is_c_abi_call = (callee_name.find("CallC") != std::string::npos);
      PhysRegState saved_c_callee_phys[kMaxGprs];
      if (is_c_abi_call) {
        for (int r = 0; r < arch_.num_gprs; ++r) {
          saved_c_callee_phys[r] = env.phys[r];
        }
      }
      std::map<const clang::ValueDecl*, SymRegState> saved_c_callee_syms;
      for (const auto& [decl, s] : env.syms) {
        if (s.phys_reg >= 0 &&
            (arch_.c_callee_saved_mask & (1u << s.phys_reg)) &&
            s.scratch_slot < 0) {
          saved_c_callee_syms[decl] = s;
        }
      }
      for (unsigned i = 0; i < num_args; ++i) {
        if (sum.param_roles[i] == ParamRole::kOut ||
            sum.param_roles[i] == ParamRole::kInOut ||
            (i < 32 && (sum.preserved_across_call_param_mask & (1u << i)))) {
          if (const clang::ValueDecl* out_d =
                  ResolveSymDecl(ce->getArg(i), env)) {
            if (env.syms.count(out_d)) {
              SymRegState kept = env.syms[out_d];
              kept.status = RegStatus::kAlive;
              saved_c_callee_syms[out_d] = kept;
            }
          }
        }
      }
      for (int r = 0; r < arch_.num_gprs; ++r) {
        if (arch_.caller_saved_mask & (1u << r)) {
          WriteOrClobberPhys(r, /*is_clobber=*/true, /*from_sym_param=*/false,
                             call_loc, reason, env, fctx, /*skip_sym=*/nullptr,
                             /*check_maglev_phys=*/false);
        }
      }
      if (is_c_abi_call) {
        for (int r = 0; r < arch_.num_gprs; ++r) {
          if (arch_.c_callee_saved_mask & (1u << r)) {
            env.phys[r] = saved_c_callee_phys[r];
          }
        }
      }
      for (const auto& [decl, saved_s] : saved_c_callee_syms) {
        env.syms[decl] = saved_s;
      }
      if (ret0_was_initial || callee_name == "CallBuiltin" || is_c_abi_call) {
        env.phys[arch_.ret_reg0].status = RegStatus::kAlive;
      }
      if (callee_name == "CallBuiltin" || is_c_abi_call) {
        env.phys[arch_.ret_reg1].status = RegStatus::kAlive;
      }
      for (auto& [decl, s] : env.syms) {
        if (IsOperandType(decl->getType())) continue;
        if (s.phys_reg >= 0 && (arch_.pinned_mask & (1u << s.phys_reg))) {
          continue;
        }
        if (s.phys_reg >= 0 &&
            (arch_.c_callee_saved_mask & (1u << s.phys_reg)) &&
            s.scratch_slot < 0) {
          continue;
        }
        if (s.phys_reg == arch_.ret_reg0 &&
            (ret0_was_initial || callee_name == "CallBuiltin" ||
             is_c_abi_call) &&
            s.param_index < 0) {
          s.status = RegStatus::kAlive;
          s.holds_entry_value = false;
          continue;
        }
        bool is_out_of_this_call = false;
        for (unsigned i = 0; i < num_args; ++i) {
          if ((sum.param_roles[i] == ParamRole::kOut ||
               sum.param_roles[i] == ParamRole::kInOut ||
               (i < 32 &&
                (sum.preserved_across_call_param_mask & (1u << i)))) &&
              ResolveSymDecl(ce->getArg(i), env) == decl) {
            is_out_of_this_call = true;
            break;
          }
        }
        if (is_out_of_this_call) continue;
        if (s.readonly_kind == ReadonlyKind::kExplicitAttr &&
            !IsSymSavedOnStack(decl, s.phys_reg, env)) {
          QueueWarning(fctx, call_loc,
                       "Cannot clobber read-only register '" +
                           decl->getNameAsString() + "' via '" + callee_name +
                           "' (" + s.readonly_reason + ").");
        }
        s.status = RegStatus::kClobbered;
        s.holds_entry_value = false;
        s.def_version = fctx.next_version++;
        s.clobber = {call_loc, reason};
        if (s.param_index >= 0 && !IsSymSavedOnStack(decl, s.phys_reg, env)) {
          fctx.param_clobbered_any[s.param_index] = true;
        }
      }

      // Mark registers passed via pointer/array out-parameters (`&dst` or
      // `LiftoffRegister result_regs[] = {ret, dst}`) as defined after the
      // call:
      for (const clang::Expr* arg : ce->arguments()) {
        const clang::Expr* ua = UnwrapExpr(arg);
        if (!ua) continue;
        if (const auto* uo = llvm::dyn_cast<clang::UnaryOperator>(ua)) {
          if (uo->getOpcode() == clang::UO_AddrOf) {
            if (const clang::ValueDecl* sd =
                    ResolveSymDecl(uo->getSubExpr(), env)) {
              WriteOrClobberSym(sd, /*is_clobber=*/false, call_loc,
                                "populated by '" + callee_name + "'", env,
                                fctx);
            }
          }
        } else if (const auto* dre = llvm::dyn_cast<clang::DeclRefExpr>(ua)) {
          const clang::ValueDecl* d = CanonValueDecl(dre->getDecl());
          auto arr_it = env.reg_arrays.find(d);
          if (arr_it != env.reg_arrays.end()) {
            for (const clang::ValueDecl* elem_sd : arr_it->second) {
              WriteOrClobberSym(elem_sd, /*is_clobber=*/false, call_loc,
                                "populated by '" + callee_name + "'", env,
                                fctx);
            }
          }
        }
      }
    } else if (sum.clobbers != 0 || !sum.conditional_clobbers.empty()) {
      RegMask uncond_clobbers = sum.clobbers;
      RegMask forwarded_cond_clobbers = 0;
      for (const ConditionalClobber& cc : sum.conditional_clobbers) {
        if (cc.param_index < ce->getNumArgs()) {
          const clang::Expr* carg = ce->getArg(cc.param_index);
          if (auto iv = TryEvalConstInt(carg, env.local_expr_bindings)) {
            if (EvalCondCmpOp(cc.op, *iv, cc.rhs)) {
              uncond_clobbers |= cc.mask;
            }
            continue;
          }
          const clang::Expr* ucarg = UnwrapExpr(carg);
          if (const auto* dre =
                  llvm::dyn_cast_or_null<clang::DeclRefExpr>(ucarg)) {
            const clang::ValueDecl* cd = CanonValueDecl(dre->getDecl());
            int caller_pi = -1;
            if (fctx.fn) {
              for (unsigned pi = 0; pi < fctx.fn->getNumParams(); ++pi) {
                if (fctx.fn->getParamDecl(pi)->getCanonicalDecl() == cd) {
                  caller_pi = static_cast<int>(pi);
                  break;
                }
              }
            }
            if (caller_pi >= 0) {
              forwarded_cond_clobbers |= cc.mask;
              ConditionalClobber ncc{static_cast<unsigned>(caller_pi), cc.op,
                                     cc.rhs, cc.mask};
              if (std::find(fctx.propagated_conditional_clobbers.begin(),
                            fctx.propagated_conditional_clobbers.end(), ncc) ==
                  fctx.propagated_conditional_clobbers.end()) {
                fctx.propagated_conditional_clobbers.push_back(ncc);
              }
              continue;
            }
          }
        }
        uncond_clobbers |= cc.mask;
      }
      std::string reason = "clobbered by '" + callee_name + "'";
      for (int r = 0; r < arch_.num_gprs; ++r) {
        if (uncond_clobbers & (1u << r)) {
          WriteOrClobberPhys(r, /*is_clobber=*/true, /*from_sym_param=*/false,
                             call_loc, reason, env, fctx);
        } else if (forwarded_cond_clobbers & (1u << r)) {
          WriteOrClobberPhys(r, /*is_clobber=*/true, /*from_sym_param=*/false,
                             call_loc, reason, env, fctx, /*skip_sym=*/nullptr,
                             /*check_maglev_phys=*/true,
                             /*record_in_summary=*/false);
        }
      }
    }

    if (sum.implicit_out != 0) {
      std::string reason = "written implicitly by '" + callee_name + "'";
      for (int r = 0; r < arch_.num_gprs; ++r) {
        if (sum.implicit_out & (1u << r)) {
          WriteOrClobberPhys(r, /*is_clobber=*/false, /*from_sym_param=*/false,
                             call_loc, reason, env, fctx);
        }
      }
    }

    // 12. Dynamic scratch scope slot & physical pool clobbering / exhaustion &
    // Maglev temp/call summaries
    if (sum.baseline_scratch_used > 0) {
      int start_slot = env.active_scratch_depth;
      int end_slot = start_slot + sum.baseline_scratch_used;
      int must_end_slot = start_slot + sum.must_baseline_scratch_used;
      fctx.used_baseline_scratch_scope = true;
      fctx.max_baseline_scratch_used =
          std::max(fctx.max_baseline_scratch_used, end_slot);
      if (sum.must_baseline_scratch_used > 0) {
        env.path_max_baseline_scratch =
            std::max(env.path_max_baseline_scratch, must_end_slot);
        if (must_end_slot > arch_.baseline_scratch_pool_size) {
          int avail =
              std::max(0, arch_.baseline_scratch_pool_size - start_slot);
          QueueWarning(fctx, call_loc,
                       "Call to '" + callee_name + "' requires " +
                           std::to_string(sum.must_baseline_scratch_used) +
                           " scratch register(s), exhausting "
                           "BaselineAssembler::ScratchRegisterScope pool (" +
                           std::to_string(avail) + " available).");
        }
      }
      std::string reason = "clobbered by internal scratch register scope in '" +
                           callee_name + "'";
      int limit_slot = std::min(end_slot, arch_.baseline_scratch_pool_size);
      for (int sl = std::max(0, start_slot); sl < limit_slot; ++sl) {
        WriteOrClobberPhys(arch_.baseline_scratch_regs[sl], /*is_clobber=*/true,
                           /*from_sym_param=*/true, call_loc, reason, env,
                           fctx);
      }
      for (auto& [decl, s] : env.syms) {
        if (s.scratch_slot >= start_slot && s.scratch_slot < end_slot) {
          s.status = RegStatus::kClobbered;
          s.def_version = fctx.next_version++;
          s.clobber = {call_loc, reason};
        }
      }
    }
    if (sum.arm64_scratch_used > 0) {
      int avail_cnt = llvm::popcount(env.arm64_avail_scratch_mask);
      fctx.used_arm64_scratch_scope = true;
      int inc_avail = llvm::popcount(env.arm64_avail_scratch_mask &
                                     env.arm64_included_scratch_mask);
      int from_caller = std::max(0, sum.arm64_scratch_used - inc_avail);
      fctx.max_arm64_scratch_used =
          std::max(fctx.max_arm64_scratch_used,
                   env.arm64_caller_scratches_held + from_caller);
      if (sum.must_arm64_scratch_used > 0) {
        int must_from_caller =
            std::max(0, sum.must_arm64_scratch_used - inc_avail);
        env.path_max_arm64_scratch =
            std::max(env.path_max_arm64_scratch,
                     env.arm64_caller_scratches_held + must_from_caller);
        if (avail_cnt < sum.must_arm64_scratch_used) {
          QueueWarning(fctx, call_loc,
                       "Call to '" + callee_name + "' requires " +
                           std::to_string(sum.must_arm64_scratch_used) +
                           " scratch register(s), exhausting "
                           "UseScratchRegisterScope pool (" +
                           std::to_string(avail_cnt) + " available).");
        }
      }
      std::string reason = "clobbered by internal scratch register scope in '" +
                           callee_name + "'";
      RegMask rem = env.arm64_avail_scratch_mask;
      for (int k = 0; k < sum.arm64_scratch_used && rem != 0; ++k) {
        int r = llvm::countr_zero(rem);
        rem &= ~(1u << r);
        WriteOrClobberPhys(r, /*is_clobber=*/true, /*from_sym_param=*/true,
                           call_loc, reason, env, fctx);
      }
    }
    if (sum.maglev_gpr_temps_peak > 0 || sum.maglev_gpr_temps_leaked > 0) {
      AccountMaglevTempUse(false, false, sum.maglev_gpr_temps_peak,
                           sum.maglev_gpr_temps_leaked, call_loc,
                           "Call to '" + callee_name + "'", env, fctx);
    }
    if (sum.maglev_double_temps_peak > 0 ||
        sum.maglev_double_temps_leaked > 0) {
      AccountMaglevTempUse(false, true, sum.maglev_double_temps_peak,
                           sum.maglev_double_temps_leaked, call_loc,
                           "Call to '" + callee_name + "'", env, fctx);
    }
    if (sum.maglev_gpr_scratches_peak > 0 ||
        sum.maglev_gpr_scratches_leaked > 0) {
      AccountMaglevTempUse(true, false, sum.maglev_gpr_scratches_peak,
                           sum.maglev_gpr_scratches_leaked, call_loc,
                           "Call to '" + callee_name + "'", env, fctx);
    }
    if (sum.maglev_double_scratches_peak > 0 ||
        sum.maglev_double_scratches_leaked > 0) {
      AccountMaglevTempUse(true, true, sum.maglev_double_scratches_peak,
                           sum.maglev_double_scratches_leaked, call_loc,
                           "Call to '" + callee_name + "'", env, fctx);
    }
    if (sum.maglev_uses_save_reg_state) {
      fctx.maglev_used_save_reg_state = true;
      CheckMaglevSaveRegStateProperty(call_loc, fctx);
    }
    if (!sum.clobbers_caller_saved && sum.maglev_calls_outside_save_scope) {
      if (env.maglev_save_call_depth == 0 && !sum.no_fallthrough) {
        fctx.maglev_called_outside_save_scope = true;
      }
      CheckMaglevCallProperty(callee_name, sum, call_loc, env, fctx);
    }

    // 13. Control-flow annotations: V8_ASM_BRANCH (forward + backward),
    // V8_ASM_BIND, V8_ASM_NO_FALLTHROUGH
    bool is_unconditional_arm64_b = false;
    if (callee_name == "B" && ce->getNumArgs() >= 2) {
      const clang::Expr* type_arg = UnwrapExpr(ce->getArg(1));
      if (const auto* dre =
              llvm::dyn_cast_or_null<clang::DeclRefExpr>(type_arg)) {
        if (SafeDeclName(dre->getDecl()) == "always") {
          is_unconditional_arm64_b = true;
        }
      }
    }

    for (unsigned i = 0; i < num_args; ++i) {
      ParamRole role = sum.param_roles[i];
      if (role == ParamRole::kBranch) {
        if (llvm::isa<clang::CXXDefaultArgExpr>(ce->getArg(i))) continue;
        if (const clang::ValueDecl* lbl = ResolveLabelDecl(ce->getArg(i))) {
          if (fctx.bound_labels.contains(lbl)) {
            auto it = fctx.backward_label_states.find(lbl);
            if (it == fctx.backward_label_states.end()) {
              fctx.backward_label_states[lbl] = env;
            } else {
              it->second = MergeEnvs(it->second, env, arch_.num_gprs);
            }
          } else {
            auto it = fctx.forward_label_states.find(lbl);
            if (it == fctx.forward_label_states.end()) {
              fctx.forward_label_states[lbl] = env;
            } else {
              it->second = MergeEnvs(it->second, env, arch_.num_gprs);
            }
          }
        }
      } else if (role == ParamRole::kBind) {
        if (llvm::isa<clang::CXXDefaultArgExpr>(ce->getArg(i))) continue;
        if (const clang::ValueDecl* lbl = ResolveLabelDecl(ce->getArg(i))) {
          fctx.bound_labels.insert(lbl);
          auto fwd_it = fctx.forward_label_states.find(lbl);
          if (fwd_it != fctx.forward_label_states.end()) {
            env = MergeEnvs(env, fwd_it->second, arch_.num_gprs);
          }
          if (fctx.carried_backedges) {
            auto back_it = fctx.carried_backedges->find(lbl);
            if (back_it != fctx.carried_backedges->end()) {
              env = MergeEnvs(env, back_it->second, arch_.num_gprs);
            }
          }
        }
      }
    }

    if (sum.no_fallthrough || is_unconditional_arm64_b) {
      env.reachable = false;
    }

    // 14. If this call returns an Operand/MemOperand, snapshot its scratch
    // register versions right now before subsequent expressions execute!
    if (IsOperandType(ce->getType())) {
      fctx.call_operand_deps[ce] =
          BuildCallReturnedOperandDep(ce, callee, sum, env, fctx);
    }
  }

  void CheckCallSiteAliasing(const clang::CallExpr* ce,
                             const clang::FunctionDecl* callee,
                             const FuncSummary& sum, unsigned num_args,
                             Env& env, FunctionAnalysisContext& fctx) {
    std::string callee_name = callee->getNameAsString();

    // 1. Check parameter-pair early clobbers: (out_idx, in_idx)
    for (const auto& [out_idx, in_idx] : sum.early_clobber_param_pairs) {
      if (out_idx >= num_args || in_idx >= num_args) continue;
      const clang::Expr* out_arg = ce->getArg(out_idx);
      const clang::Expr* in_arg = ce->getArg(in_idx);
      if (llvm::isa<clang::CXXDefaultArgExpr>(out_arg) ||
          llvm::isa<clang::CXXDefaultArgExpr>(in_arg)) {
        continue;
      }
      const clang::Expr* u_out = UnwrapExpr(out_arg);
      const clang::Expr* u_in = UnwrapExpr(in_arg);
      if (!u_out || !u_in || !IsTrackableRegType(u_out->getType())) continue;

      const clang::ValueDecl* out_sym = ResolveSymDecl(u_out, env);
      std::optional<int> out_phys = ResolvePhysReg(out_arg, env);
      if (out_sym && env.known_no_reg.contains(out_sym)) continue;
      if (out_phys && *out_phys < 0) continue;

      auto aliases_out = [&](const clang::ValueDecl* in_s,
                             std::optional<int> in_p) {
        if (out_sym && in_s && out_sym == in_s) return true;
        if (out_phys && in_p && *out_phys >= 0 && *out_phys == *in_p) {
          return true;
        }
        return false;
      };

      bool aliased = false;
      if (IsOperandType(u_in->getType())) {
        OperandDep dep = ExtractOperandDep(u_in, env, fctx);
        for (const clang::ValueDecl* sd : dep.sym_decls) {
          if (aliases_out(sd, std::nullopt)) aliased = true;
        }
        if (out_phys && *out_phys >= 0 && (dep.phys_mask & (1u << *out_phys))) {
          aliased = true;
        }
      } else if (IsTrackableRegType(u_in->getType())) {
        aliased =
            aliases_out(ResolveSymDecl(u_in, env), ResolvePhysReg(in_arg, env));
      }

      if (aliased) {
        std::string out_param_name =
            callee->getParamDecl(out_idx)->getNameAsString();
        std::string in_param_name =
            callee->getParamDecl(in_idx)->getNameAsString();
        ClobberNote note{callee->getLocation(),
                         "'" + callee_name + "' overwrites '" + out_param_name +
                             "' before reading '" + in_param_name + "'"};
        QueueWarning(fctx, ce->getExprLoc(),
                     "Register passed as '" + out_param_name + "' to '" +
                         callee_name + "' aliases input '" + in_param_name +
                         "' and is clobbered before the input is read.",
                     &note);
      }
    }

    // 2. Check distinct SCRATCH parameters aliasing each other at call site
    for (unsigned i = 0; i < num_args; ++i) {
      if (sum.param_roles[i] != ParamRole::kScratch) {
        continue;
      }
      const clang::Expr* arg_i = ce->getArg(i);
      if (llvm::isa<clang::CXXDefaultArgExpr>(arg_i)) continue;
      const clang::Expr* u_i = UnwrapExpr(arg_i);
      if (!u_i || !IsTrackableRegType(u_i->getType())) continue;
      const clang::ValueDecl* sym_i = ResolveSymDecl(u_i, env);
      std::optional<int> phys_i = ResolvePhysReg(arg_i, env);
      if (sym_i && env.known_no_reg.contains(sym_i)) continue;
      if (phys_i && *phys_i < 0) continue;

      for (unsigned j = i + 1; j < num_args; ++j) {
        if (sum.param_roles[j] != ParamRole::kScratch) {
          continue;
        }
        const clang::Expr* arg_j = ce->getArg(j);
        if (llvm::isa<clang::CXXDefaultArgExpr>(arg_j)) continue;
        const clang::Expr* u_j = UnwrapExpr(arg_j);
        if (!u_j || !IsTrackableRegType(u_j->getType())) continue;
        const clang::ValueDecl* sym_j = ResolveSymDecl(u_j, env);
        std::optional<int> phys_j = ResolvePhysReg(arg_j, env);
        if ((sym_i && sym_i == sym_j) ||
            (phys_i && phys_j && *phys_i >= 0 && *phys_i == *phys_j)) {
          // Allow xzr/wzr (pinned zero register) to appear multiple times on
          // ARM64 (e.g. Pop(xzr, padreg)).
          if (phys_i && *phys_i >= 0 && (arch_.pinned_mask & (1u << *phys_i))) {
            continue;
          }
          QueueWarning(
              fctx, ce->getExprLoc(),
              "Distinct output/scratch parameters '" +
                  callee->getParamDecl(i)->getNameAsString() + "' and '" +
                  callee->getParamDecl(j)->getNameAsString() + "' of '" +
                  callee_name + "' are passed the same register.");
        }
      }
    }

    // 3. Check physical registers overwritten by callee before reading input
    for (unsigned i = 0;
         i < num_args && i < sum.early_clobber_phys_before_param.size(); ++i) {
      RegMask early_phys = sum.early_clobber_phys_before_param[i];
      if (!early_phys) continue;
      const clang::Expr* arg_i = ce->getArg(i);
      if (llvm::isa<clang::CXXDefaultArgExpr>(arg_i)) continue;
      if (auto p = ResolvePhysReg(arg_i, env)) {
        if (*p >= 0 && *p < arch_.num_gprs && (early_phys & (1u << *p))) {
          ClobberNote note{callee->getLocation(),
                           "'" + callee_name + "' clobbers '" +
                               arch_.gpr_names[*p] + "' before reading '" +
                               callee->getParamDecl(i)->getNameAsString() +
                               "'"};
          QueueWarning(
              fctx, ce->getExprLoc(),
              std::string("Physical register '") + arch_.gpr_names[*p] +
                  "' passed as '" + callee->getParamDecl(i)->getNameAsString() +
                  "' to '" + callee_name +
                  "' is clobbered internally before the parameter is read.",
              &note);
        }
      }
    }
  }

  void PromoteEqualOutInBranch(const clang::ValueDecl* sym_l,
                               const clang::ValueDecl* sym_r, Env& eq_branch,
                               FunctionAnalysisContext& fctx) {
    auto promote_eq_out = [&](const clang::ValueDecl* out_d,
                              const clang::ValueDecl* in_d) {
      if (!eq_branch.syms.count(out_d) || !eq_branch.syms.count(in_d)) return;
      SymRegState& out_s = eq_branch.syms[out_d];
      SymRegState& in_s = eq_branch.syms[in_d];
      if (out_s.param_index >= 0 && in_s.param_index >= 0 &&
          static_cast<unsigned>(out_s.param_index) <
              fctx.explicit_sum.param_roles.size() &&
          fctx.explicit_sum.param_roles[out_s.param_index] == ParamRole::kOut &&
          in_s.status != RegStatus::kClobbered) {
        fctx.param_read_entry[in_s.param_index] = true;
        fctx.param_written_any[out_s.param_index] = true;
        out_s.status = RegStatus::kAlive;
        out_s.holds_entry_value = false;
      }
    };
    promote_eq_out(sym_l, sym_r);
    promote_eq_out(sym_r, sym_l);
  }

  bool ExtractResultEqInput(const clang::Expr* expr, const Env& env,
                            const clang::ValueDecl*& out_res_decl,
                            std::string& out_input_key, bool& out_is_eq) {
    expr = UnwrapExpr(expr);
    if (!expr) return false;

    const clang::Expr* lhs = nullptr;
    const clang::Expr* rhs = nullptr;
    bool is_eq = false;
    bool always_width_safe = false;

    if (const auto* oce = llvm::dyn_cast<clang::CXXOperatorCallExpr>(expr)) {
      if (oce->getNumArgs() == 2) {
        if (oce->getOperator() == clang::OO_EqualEqual) {
          is_eq = true;
          lhs = oce->getArg(0);
          rhs = oce->getArg(1);
        } else if (oce->getOperator() == clang::OO_ExclaimEqual) {
          is_eq = false;
          lhs = oce->getArg(0);
          rhs = oce->getArg(1);
        }
      }
    } else if (const auto* bo = llvm::dyn_cast<clang::BinaryOperator>(expr)) {
      if (bo->getOpcode() == clang::BO_EQ) {
        is_eq = true;
        lhs = bo->getLHS();
        rhs = bo->getRHS();
      } else if (bo->getOpcode() == clang::BO_NE) {
        is_eq = false;
        lhs = bo->getLHS();
        rhs = bo->getRHS();
      }
    } else if (const auto* mce =
                   llvm::dyn_cast<clang::CXXMemberCallExpr>(expr)) {
      if (const auto* md = mce->getMethodDecl()) {
        if (SafeDeclName(md) == "Aliases" && mce->getNumArgs() == 1) {
          is_eq = true;
          always_width_safe = true;
          lhs = mce->getImplicitObjectArgument();
          rhs = mce->getArg(0);
        }
      }
    } else if (const auto* ce = llvm::dyn_cast<clang::CallExpr>(expr)) {
      if (const clang::FunctionDecl* fd = ce->getDirectCallee()) {
        if (SafeDeclName(fd) == "AreAliased" && ce->getNumArgs() == 2) {
          is_eq = true;
          always_width_safe = true;
          lhs = ce->getArg(0);
          rhs = ce->getArg(1);
        }
      }
    }
    if (!lhs || !rhs) return false;

    auto match_pair = [&](const clang::Expr* res_e,
                          const clang::Expr* in_e) -> bool {
      const clang::ValueDecl* res_d = ResolveSymDecl(res_e, env);
      if (!res_d) return false;
      auto rit = env.syms.find(res_d);
      if (rit == env.syms.end() ||
          rit->second.maglev_role != MaglevRole::kResult) {
        return false;
      }
      bool res_is_double = rit->second.is_double_reg;
      Arm64RegWidth res_w = EffectiveArm64Width(res_e, env);

      std::string in_key;
      bool in_is_double = false;
      Arm64RegWidth in_w = EffectiveArm64Width(in_e, env);

      if (const clang::ValueDecl* in_d = ResolveSymDecl(in_e, env)) {
        auto iit = env.syms.find(in_d);
        if (iit != env.syms.end() &&
            iit->second.maglev_role == MaglevRole::kInput &&
            !iit->second.maglev_input_key.empty()) {
          in_key = iit->second.maglev_input_key;
          in_is_double = iit->second.is_double_reg;
        }
      }
      if (in_key.empty()) {
        if (auto m = MatchMaglevToRegisterCall(in_e, env)) {
          if (!m->is_result && !m->input_key.empty()) {
            in_key = m->input_key;
            in_is_double = m->is_double;
          }
        }
      }
      if (in_key.empty() || res_is_double != in_is_double) return false;
      if (!always_width_safe && arch_.kind == ArchKind::kArm64 &&
          !res_is_double &&
          !(res_w != Arm64RegWidth::kUnknown && res_w == in_w)) {
        return false;
      }
      out_res_decl = res_d;
      out_input_key = std::move(in_key);
      out_is_eq = is_eq;
      return true;
    };

    return match_pair(lhs, rhs) || match_pair(rhs, lhs);
  }

  void RecordSymDistinctFromInput(const clang::ValueDecl* res_d,
                                  const std::string& input_key,
                                  const std::vector<MaglevCondAtom>& req_conds,
                                  Env& target_env) {
    if (!res_d || input_key.empty()) return;
    auto rit = target_env.syms.find(res_d);
    if (rit == target_env.syms.end()) return;
    const clang::ValueDecl* root = rit->second.root_decl;
    for (auto& [d, s] : target_env.syms) {
      if (s.maglev_role != MaglevRole::kResult) continue;
      if (d != res_d && (!root || s.root_decl != root)) continue;
      auto existing = s.known_distinct_from_inputs.find(input_key);
      if (req_conds.empty()) {
        s.known_distinct_from_inputs[input_key] = {};
      } else if (existing == s.known_distinct_from_inputs.end()) {
        s.known_distinct_from_inputs[input_key] = req_conds;
      }
    }
  }

  void RefineConditionEnv(const clang::Expr* cond, Env& then_env, Env& else_env,
                          FunctionAnalysisContext& fctx, int depth = 0) {
    cond = UnwrapExpr(cond);
    if (!cond || depth > 4) return;

    if (const auto* dre = llvm::dyn_cast<clang::DeclRefExpr>(cond)) {
      if (cond->getType()->isBooleanType() ||
          cond->getType()->isPointerType()) {
        auto bit =
            then_env.local_expr_bindings.find(CanonValueDecl(dre->getDecl()));
        if (bit != then_env.local_expr_bindings.end() && bit->second &&
            bit->second != cond) {
          RefineConditionEnv(bit->second, then_env, else_env, fctx, depth + 1);
          return;
        }
      }
    }

    if (const auto* uo = llvm::dyn_cast<clang::UnaryOperator>(cond)) {
      if (uo->getOpcode() == clang::UO_LNot) {
        RefineConditionEnv(uo->getSubExpr(), else_env, then_env, fctx,
                           depth + 1);
        return;
      }
    }

    if (const auto* bo = llvm::dyn_cast<clang::BinaryOperator>(cond)) {
      if (bo->getOpcode() == clang::BO_LAnd) {
        Env dummy_else1 = else_env;
        Env dummy_else2 = else_env;
        RefineConditionEnv(bo->getLHS(), then_env, dummy_else1, fctx,
                           depth + 1);
        RefineConditionEnv(bo->getRHS(), then_env, dummy_else2, fctx,
                           depth + 1);
        // Apply De Morgan's Law on `else_env` for `!(C1 && ... && res == in)`:
        // whenever `C1 && ...` hold, `res != in`.
        if (fctx.maglev_node_cls) {
          llvm::SmallVector<const clang::Expr*, 4> conjuncts;
          std::function<void(const clang::Expr*)> collect_and =
              [&](const clang::Expr* e) {
                e = UnwrapExpr(e);
                if (!e) return;
                if (const auto* b = llvm::dyn_cast<clang::BinaryOperator>(e)) {
                  if (b->getOpcode() == clang::BO_LAnd) {
                    collect_and(b->getLHS());
                    collect_and(b->getRHS());
                    return;
                  }
                }
                conjuncts.push_back(e);
              };
          collect_and(cond);
          for (size_t i = 0; i < conjuncts.size(); ++i) {
            const clang::ValueDecl* res_d = nullptr;
            std::string in_key;
            bool eq_val = false;
            if (!ExtractResultEqInput(conjuncts[i], else_env, res_d, in_key,
                                      eq_val) ||
                !eq_val) {
              continue;
            }
            std::vector<MaglevCondAtom> other_atoms;
            bool all_single = true;
            for (size_t j = 0; j < conjuncts.size(); ++j) {
              if (j == i) continue;
              auto branches = SplitCondBranches(conjuncts[j], true,
                                                else_env.local_expr_bindings);
              if (branches.size() != 1 || branches[0].empty()) {
                all_single = false;
                break;
              }
              for (const auto& atom : branches[0]) {
                other_atoms.push_back(atom);
              }
            }
            if (all_single && !other_atoms.empty()) {
              RecordSymDistinctFromInput(res_d, in_key, other_atoms, else_env);
            }
          }
        }
        return;
      } else if (bo->getOpcode() == clang::BO_LOr) {
        Env then1 = then_env;
        Env then2 = then_env;
        RefineConditionEnv(bo->getLHS(), then1, else_env, fctx);
        RefineConditionEnv(bo->getRHS(), then2, else_env, fctx);

        // Intersect non-aliasing and location guarantees across `A || B`:
        auto intersect_into = [](const std::set<std::string>& s1,
                                 const std::set<std::string>& s2,
                                 std::set<std::string>& dst) {
          for (const auto& k : s1) {
            if (s2.count(k)) dst.insert(k);
          }
        };
        intersect_into(then1.known_is_gpr_inputs, then2.known_is_gpr_inputs,
                       then_env.known_is_gpr_inputs);
        intersect_into(then1.known_is_fp_inputs, then2.known_is_fp_inputs,
                       then_env.known_is_fp_inputs);
        intersect_into(then1.known_not_gpr_inputs, then2.known_not_gpr_inputs,
                       then_env.known_not_gpr_inputs);
        intersect_into(then1.known_not_fp_inputs, then2.known_not_fp_inputs,
                       then_env.known_not_fp_inputs);
        intersect_into(then1.known_not_constant_inputs,
                       then2.known_not_constant_inputs,
                       then_env.known_not_constant_inputs);
        auto non_aliasing_in = [](const Env& e, const clang::ValueDecl* x,
                                  const clang::ValueDecl* y) {
          return e.known_no_reg.contains(x) || e.known_no_reg.contains(y) ||
                 e.known_not_equal.contains({x, y}) ||
                 e.known_not_equal.contains({y, x});
        };
        for (const auto& [d1, s1] : then_env.syms) {
          (void)s1;
          if (then1.known_no_reg.contains(d1) &&
              then2.known_no_reg.contains(d1)) {
            then_env.known_no_reg.insert(d1);
          }
          for (const auto& [d2, s2] : then_env.syms) {
            (void)s2;
            if (d1 >= d2) continue;
            if (non_aliasing_in(then1, d1, d2) &&
                non_aliasing_in(then2, d1, d2)) {
              then_env.known_not_equal.insert({d1, d2});
            } else {
              // Propagate ARM64 size-sensitive `!=` if one side is immediate /
              // non-aliasing and the other side used size-sensitive `!=`!
              auto ss1 = then1.size_sensitive_not_equal.find({d1, d2});
              auto ss2 = then2.size_sensitive_not_equal.find({d1, d2});
              if (ss1 != then1.size_sensitive_not_equal.end() &&
                  (non_aliasing_in(then2, d1, d2) ||
                   ss2 != then2.size_sensitive_not_equal.end())) {
                then_env.size_sensitive_not_equal[{d1, d2}] = ss1->second;
                then_env.size_sensitive_not_equal[{d2, d1}] = ss1->second;
              } else if (ss2 != then2.size_sensitive_not_equal.end() &&
                         non_aliasing_in(then1, d1, d2)) {
                then_env.size_sensitive_not_equal[{d1, d2}] = ss2->second;
                then_env.size_sensitive_not_equal[{d2, d1}] = ss2->second;
              }
            }
          }
        }
        return;
      }
    }

    // Handle `AreAliased(r0, r1, ...)` free function calls:
    if (const auto* ce = llvm::dyn_cast<clang::CallExpr>(cond)) {
      if (!llvm::isa<clang::CXXMemberCallExpr>(cond) &&
          !llvm::isa<clang::CXXOperatorCallExpr>(cond)) {
        if (const clang::FunctionDecl* fd = ce->getDirectCallee()) {
          if (fd->getIdentifier() && fd->getName() == "AreAliased") {
            llvm::SmallVector<const clang::ValueDecl*, 4> syms;
            for (const clang::Expr* a : ce->arguments()) {
              if (const clang::ValueDecl* sd = ResolveSymDecl(a, then_env)) {
                syms.push_back(sd);
              }
            }
            for (size_t i = 0; i < syms.size(); ++i) {
              for (size_t j = i + 1; j < syms.size(); ++j) {
                if (syms[i] != syms[j]) {
                  else_env.known_not_equal.insert({syms[i], syms[j]});
                  if (fctx.maglev_node_cls) {
                    auto it_i = else_env.syms.find(syms[i]);
                    auto it_j = else_env.syms.find(syms[j]);
                    if (it_i != else_env.syms.end() &&
                        it_j != else_env.syms.end()) {
                      if (it_i->second.maglev_role == MaglevRole::kResult &&
                          it_j->second.maglev_role == MaglevRole::kInput) {
                        RecordSymDistinctFromInput(
                            syms[i], it_j->second.maglev_input_key, {},
                            else_env);
                      } else if (it_j->second.maglev_role ==
                                     MaglevRole::kResult &&
                                 it_i->second.maglev_role ==
                                     MaglevRole::kInput) {
                        RecordSymDistinctFromInput(
                            syms[j], it_i->second.maglev_input_key, {},
                            else_env);
                      }
                    }
                  }
                }
              }
            }
            if (syms.size() == 2 && syms[0] != syms[1]) {
              then_env.known_equal.insert({syms[0], syms[1]});
              PromoteEqualOutInBranch(syms[0], syms[1], then_env, fctx);
            }
            return;
          }
        }
      }
    }

    // Handle member predicates (`reg.is_valid()`, `reg.Aliases(other)`,
    // `op.IsImmediate()`, Maglev `Input::IsRegister()`/`IsConstant()`):
    if (const auto* mce = llvm::dyn_cast<clang::CXXMemberCallExpr>(cond)) {
      if (const auto* md = mce->getMethodDecl()) {
        llvm::StringRef mname = SafeDeclName(md);
        if (mname == "is_valid" && mce->getNumArgs() == 0) {
          if (const clang::ValueDecl* sym =
                  ResolveSymDecl(mce->getImplicitObjectArgument(), then_env)) {
            else_env.known_no_reg.insert(sym);
            if (then_env.known_no_reg.contains(sym)) {
              then_env.reachable = false;
            }
          }
          return;
        }
        if (mname == "IsImmediate" && mce->getNumArgs() == 0) {
          if (const clang::ValueDecl* sym =
                  ResolveSymDecl(mce->getImplicitObjectArgument(), then_env)) {
            then_env.known_no_reg.insert(sym);
          }
          return;
        }
        if (mce->getNumArgs() == 0 &&
            (mname == "IsRegister" || mname == "IsGeneralRegister")) {
          auto [k, lbl] = ExtractMaglevInputKey(
              mce->getImplicitObjectArgument(), then_env.local_maglev_inputs,
              then_env.local_expr_bindings);
          if (!k.empty()) {
            then_env.known_is_gpr_inputs.insert(k);
            then_env.known_not_fp_inputs.insert(k);
            else_env.known_not_gpr_inputs.insert(k);
          }
          return;
        }
        if (mce->getNumArgs() == 0 &&
            (mname == "IsDoubleRegister" || mname == "IsFPRegister")) {
          auto [k, lbl] = ExtractMaglevInputKey(
              mce->getImplicitObjectArgument(), then_env.local_maglev_inputs,
              then_env.local_expr_bindings);
          if (!k.empty()) {
            then_env.known_is_fp_inputs.insert(k);
            then_env.known_not_gpr_inputs.insert(k);
            else_env.known_not_fp_inputs.insert(k);
          }
          return;
        }
        if (mce->getNumArgs() == 0 && mname == "IsAnyRegister") {
          auto [k, lbl] = ExtractMaglevInputKey(
              mce->getImplicitObjectArgument(), then_env.local_maglev_inputs,
              then_env.local_expr_bindings);
          if (!k.empty()) {
            then_env.known_is_gpr_inputs.insert(k);
            then_env.known_is_fp_inputs.insert(k);
            else_env.known_not_gpr_inputs.insert(k);
            else_env.known_not_fp_inputs.insert(k);
          }
          return;
        }
        if (mce->getNumArgs() == 0 && mname == "IsConstant") {
          auto [k, lbl] = ExtractMaglevInputKey(
              mce->getImplicitObjectArgument(), then_env.local_maglev_inputs,
              then_env.local_expr_bindings);
          if (!k.empty()) {
            then_env.known_not_gpr_inputs.insert(k);
            then_env.known_not_fp_inputs.insert(k);
            else_env.known_not_constant_inputs.insert(k);
          }
          return;
        }
        if (mce->getNumArgs() == 0 &&
            (mname == "IsStackSlot" || mname == "IsAnyStackSlot" ||
             mname == "IsDoubleStackSlot" || mname == "IsTaggedStackSlot" ||
             mname == "IsFPStackSlot")) {
          auto [k, lbl] = ExtractMaglevInputKey(
              mce->getImplicitObjectArgument(), then_env.local_maglev_inputs,
              then_env.local_expr_bindings);
          if (!k.empty()) {
            then_env.known_not_gpr_inputs.insert(k);
            then_env.known_not_fp_inputs.insert(k);
          }
          return;
        }
        if (mname == "Aliases" && mce->getNumArgs() == 1) {
          const clang::ValueDecl* sym_l =
              ResolveSymDecl(mce->getImplicitObjectArgument(), then_env);
          const clang::ValueDecl* sym_r =
              ResolveSymDecl(mce->getArg(0), then_env);
          if (sym_l && sym_r) {
            then_env.known_equal.insert({sym_l, sym_r});
            else_env.known_not_equal.insert({sym_l, sym_r});
            PromoteEqualOutInBranch(sym_l, sym_r, then_env, fctx);
          }
          if (fctx.maglev_node_cls) {
            const clang::ValueDecl* res_d = nullptr;
            std::string in_key;
            bool eq_val = false;
            if (ExtractResultEqInput(cond, else_env, res_d, in_key, eq_val)) {
              RecordSymDistinctFromInput(res_d, in_key, {}, else_env);
            }
          }
          return;
        }
      }
    }

    bool is_eq = false;
    bool is_ne = false;
    const clang::Expr* lhs = nullptr;
    const clang::Expr* rhs = nullptr;

    if (const auto* oce = llvm::dyn_cast<clang::CXXOperatorCallExpr>(cond)) {
      if (oce->getNumArgs() == 2) {
        if (oce->getOperator() == clang::OO_EqualEqual) {
          is_eq = true;
          lhs = oce->getArg(0);
          rhs = oce->getArg(1);
        } else if (oce->getOperator() == clang::OO_ExclaimEqual) {
          is_ne = true;
          lhs = oce->getArg(0);
          rhs = oce->getArg(1);
        }
      }
    } else if (const auto* bo = llvm::dyn_cast<clang::BinaryOperator>(cond)) {
      if (bo->getOpcode() == clang::BO_EQ) {
        is_eq = true;
        lhs = bo->getLHS();
        rhs = bo->getRHS();
      } else if (bo->getOpcode() == clang::BO_NE) {
        is_ne = true;
        lhs = bo->getLHS();
        rhs = bo->getRHS();
      }
    } else if (const auto* call = llvm::dyn_cast<clang::CallExpr>(cond)) {
      if (const clang::FunctionDecl* fd = call->getDirectCallee()) {
        llvm::StringRef fname = SafeDeclName(fd);
        // `DCHECK_EQ`/`DCHECK_NE` branch on `CheckEQImpl`/`CheckNEImpl`
        // (returning nullptr when true, i.e. `else_env`), while release
        // `CHECK_EQ`/`CHECK_NE` branch on `CmpEQImpl`/`CmpNEImpl` (returning
        // true when the assertion holds, i.e. `then_env`).
        if (call->getNumArgs() >= 2 &&
            (fname == "CheckNEImpl" || fname == "CmpEQImpl")) {
          is_eq = true;
          lhs = call->getArg(0);
          rhs = call->getArg(1);
        } else if (call->getNumArgs() >= 2 &&
                   (fname == "CheckEQImpl" || fname == "CmpNEImpl")) {
          is_ne = true;
          lhs = call->getArg(0);
          rhs = call->getArg(1);
        }
      }
    }

    if (!is_eq && !is_ne) return;

    const clang::ValueDecl* sym_l = ResolveSymDecl(lhs, then_env);
    const clang::ValueDecl* sym_r = ResolveSymDecl(rhs, then_env);
    std::optional<int> phys_l = ResolvePhysReg(lhs, then_env);
    std::optional<int> phys_r = ResolvePhysReg(rhs, then_env);

    Env& eq_branch = is_eq ? then_env : else_env;
    Env& ne_branch = is_eq ? else_env : then_env;

    if (sym_l && phys_r && *phys_r == -1) {
      eq_branch.known_no_reg.insert(sym_l);
      if (ne_branch.known_no_reg.contains(sym_l)) {
        ne_branch.reachable = false;
      }
    } else if (sym_r && phys_l && *phys_l == -1) {
      eq_branch.known_no_reg.insert(sym_r);
      if (ne_branch.known_no_reg.contains(sym_r)) {
        ne_branch.reachable = false;
      }
    } else if (sym_l && sym_r) {
      eq_branch.known_equal.insert({sym_l, sym_r});
      bool arm64_width_safe = false;
      if (arch_.kind == ArchKind::kArm64) {
        auto it_l = then_env.syms.find(sym_l);
        auto it_r = then_env.syms.find(sym_r);
        if (it_l != then_env.syms.end() && it_r != then_env.syms.end()) {
          if (it_l->second.is_double_reg && it_r->second.is_double_reg) {
            arm64_width_safe = true;
          } else {
            Arm64RegWidth wl = EffectiveArm64Width(lhs, then_env);
            Arm64RegWidth wr = EffectiveArm64Width(rhs, then_env);
            if (wl != Arm64RegWidth::kUnknown && wl == wr) {
              arm64_width_safe = true;
            }
          }
        }
      }
      if (arch_.kind == ArchKind::kArm64 && !arm64_width_safe) {
        // On ARM64, CPURegister::operator== compares `reg_size_`, so `wN != xN`
        // is true despite aliasing! Record in `size_sensitive_not_equal` rather
        // than `known_not_equal`.
        ne_branch.size_sensitive_not_equal[{sym_l, sym_r}] = cond->getExprLoc();
        ne_branch.size_sensitive_not_equal[{sym_r, sym_l}] = cond->getExprLoc();
      } else {
        ne_branch.known_not_equal.insert({sym_l, sym_r});
        if (fctx.maglev_node_cls) {
          const clang::ValueDecl* res_d = nullptr;
          std::string in_key;
          bool eq_val = false;
          if (ExtractResultEqInput(cond, ne_branch, res_d, in_key, eq_val)) {
            RecordSymDistinctFromInput(res_d, in_key, {}, ne_branch);
          }
        }
      }
      PromoteEqualOutInBranch(sym_l, sym_r, eq_branch, fctx);
    } else if (sym_l && phys_r && *phys_r >= 0) {
      if (eq_branch.syms.count(sym_l)) {
        eq_branch.syms[sym_l].phys_reg = *phys_r;
        if (*phys_r < arch_.num_gprs) {
          if (eq_branch.phys[*phys_r].status == RegStatus::kAlive) {
            eq_branch.syms[sym_l].status = RegStatus::kAlive;
          } else if (eq_branch.syms[sym_l].status == RegStatus::kAlive) {
            eq_branch.phys[*phys_r].status = RegStatus::kAlive;
          }
        }
      }
    } else if (sym_r && phys_l && *phys_l >= 0) {
      if (eq_branch.syms.count(sym_r)) {
        eq_branch.syms[sym_r].phys_reg = *phys_l;
        if (*phys_l < arch_.num_gprs) {
          if (eq_branch.phys[*phys_l].status == RegStatus::kAlive) {
            eq_branch.syms[sym_r].status = RegStatus::kAlive;
          } else if (eq_branch.syms[sym_r].status == RegStatus::kAlive) {
            eq_branch.phys[*phys_l].status = RegStatus::kAlive;
          }
        }
      }
    } else if (fctx.maglev_node_cls) {
      const clang::ValueDecl* res_d = nullptr;
      std::string in_key;
      bool eq_val = false;
      if (ExtractResultEqInput(cond, ne_branch, res_d, in_key, eq_val)) {
        RecordSymDistinctFromInput(res_d, in_key, {}, ne_branch);
      }
    }
  }

  void BindVariableInit(const clang::VarDecl* vd, const clang::Expr* raw_init,
                        Env& env, FunctionAnalysisContext& fctx) {
    const clang::ValueDecl* canon_vd = vd->getCanonicalDecl();
    const clang::Expr* init = UnwrapExpr(raw_init);

    if (IsSaveRegisterStateForCallType(vd->getType())) {
      if (raw_init) VisitStmt(raw_init, env, fctx);
      fctx.maglev_used_save_reg_state = true;
      CheckMaglevSaveRegStateProperty(vd->getLocation(), fctx);

      SavedStackEntry entry;
      entry.kind = SavedStackEntry::kAllRegs;
      for (int r = 0; r < arch_.num_gprs; ++r) {
        entry.saved_phys_all[r] = env.phys[r];
      }
      entry.saved_syms_all = env.syms;
      entry.saved_clobbered_caller_saved = env.clobbered_caller_saved;
      env.asm_stack.push_back(std::move(entry));
      env.maglev_save_call_depth++;

      ScratchScopeFrame frame;
      frame.base_depth = env.active_scratch_depth;
      frame.kind = ScratchScopeKind::kNone;
      frame.is_maglev_save_call = true;
      env.scratch_scope_stack.push_back(frame);
      return;
    }

    ScratchScopeKind scope_kind = GetScratchScopeKind(vd->getType());
    if (scope_kind != ScratchScopeKind::kNone) {
      ScratchScopeFrame frame;
      frame.base_depth = env.active_scratch_depth;
      frame.kind = scope_kind;
      frame.saved_arm64_avail = env.arm64_avail_scratch_mask;
      frame.saved_arm64_included = env.arm64_included_scratch_mask;
      frame.saved_arm64_caller_held = env.arm64_caller_scratches_held;
      frame.saved_maglev_gpr_temps = env.maglev_gpr_temps_used;
      frame.saved_maglev_double_temps = env.maglev_double_temps_used;
      frame.saved_maglev_gpr_scratches = env.maglev_gpr_scratches_used;
      frame.saved_maglev_double_scratches = env.maglev_double_scratches_used;
      frame.saved_maglev_gpr_included = env.maglev_gpr_temps_included;
      frame.saved_maglev_double_included = env.maglev_double_temps_included;
      frame.saved_maglev_gpr_unbounded = env.maglev_gpr_temps_unbounded;
      frame.saved_maglev_double_unbounded = env.maglev_double_temps_unbounded;
      frame.saved_maglev_temps_reset = env.maglev_temps_reset;
      env.scratch_scope_stack.push_back(frame);
      return;
    }

    if (IsMaglevInputType(vd->getType())) {
      if (raw_init) {
        VisitStmt(raw_init, env, fctx);
        auto k = ExtractMaglevInputKey(raw_init, env.local_maglev_inputs,
                                       env.local_expr_bindings);
        if (!k.first.empty()) {
          env.local_maglev_inputs[canon_vd] = std::move(k);
        }
      }
      return;
    }

    if (IsOperandType(vd->getType())) {
      if (raw_init) {
        VisitStmt(raw_init, env, fctx);
        OperandDep dep = ExtractOperandDep(raw_init, env, fctx);
        env.operands[canon_vd] = dep;
      }
      return;
    }

    if (const auto* arr_ty = ast_ctx_.getAsArrayType(vd->getType())) {
      if (IsTrackableRegType(arr_ty->getElementType()) && raw_init) {
        VisitStmt(raw_init, env, fctx);
        if (const auto* ile = llvm::dyn_cast<clang::InitListExpr>(init)) {
          std::vector<const clang::ValueDecl*> elems;
          for (const clang::Expr* e : ile->inits()) {
            if (const clang::ValueDecl* sd = ResolveSymDecl(e, env)) {
              elems.push_back(sd);
            }
          }
          if (!elems.empty()) {
            env.reg_arrays[canon_vd] = std::move(elems);
          }
        }
        return;
      }
    }

    bool is_double_var = IsDoubleRegType(vd->getType());
    bool track_reg = IsTrackableRegType(vd->getType()) ||
                     (is_double_var &&
                      (fctx.maglev_node_cls != nullptr ||
                       MatchMaglevTempAcquireCall(raw_init, nullptr, nullptr) ||
                       MatchMaglevToRegisterCall(raw_init, env).has_value()));
    if (!track_reg) {
      if (raw_init) {
        env.local_expr_bindings[canon_vd] = raw_init;
        VisitStmt(raw_init, env, fctx);
      }
      return;
    }

    env.known_no_reg.erase(canon_vd);
    for (auto it = env.local_expr_bindings.begin();
         it != env.local_expr_bindings.end();) {
      if (ExprReferencesDecl(it->second, canon_vd)) {
        it = env.local_expr_bindings.erase(it);
      } else {
        ++it;
      }
    }

    if (!raw_init) {
      SymRegState s;
      s.status = RegStatus::kInitial;
      s.root_decl = canon_vd;
      s.is_double_reg = is_double_var;
      s.def_version = fctx.next_version++;
      env.syms[canon_vd] = s;
      return;
    }

    // Split path-sensitive ternary initializers (e.g.
    // `Register length = IsLength(end_constant) ? end : temps.Acquire();`).
    if (const auto* cond_op =
            llvm::dyn_cast_or_null<clang::ConditionalOperator>(init)) {
      const clang::Expr* cond = cond_op->getCond();
      bool const_b = false;
      if (cond && !cond->isValueDependent() &&
          cond->EvaluateAsBooleanCondition(const_b, ast_ctx_)) {
        BindVariableInit(
            vd, const_b ? cond_op->getTrueExpr() : cond_op->getFalseExpr(), env,
            fctx);
        return;
      }
      VisitStmt(cond, env, fctx);
      Env true_env = env;
      Env false_env = env;
      RefineConditionEnv(cond, true_env, false_env, fctx);
      if (fctx.maglev_node_cls) {
        ApplyMaglevCondBranch(cond, true, true_env);
        ApplyMaglevCondBranch(cond, false, false_env);
      }
      if (true_env.reachable) {
        BindVariableInit(vd, cond_op->getTrueExpr(), true_env, fctx);
      }
      if (false_env.reachable) {
        BindVariableInit(vd, cond_op->getFalseExpr(), false_env, fctx);
      }
      env = MergeEnvs(true_env, false_env, arch_.num_gprs);
      return;
    }

    // Check if initialized from a non-Maglev scratch scope (`AcquireScratch`,
    // `AcquireX`, `AcquireW`, `AcquireSameSizeAs`, `Acquire`).
    ScratchScopeKind acq_kind = MatchScratchAcquireCall(raw_init);
    if (acq_kind != ScratchScopeKind::kNone &&
        acq_kind != ScratchScopeKind::kMaglevTemp) {
      int slot = env.active_scratch_depth++;
      int phys = -1;
      if (acq_kind == ScratchScopeKind::kBaseline) {
        fctx.used_baseline_scratch_scope = true;
        fctx.max_baseline_scratch_used =
            std::max(fctx.max_baseline_scratch_used, env.active_scratch_depth);
        env.path_max_baseline_scratch =
            std::max(env.path_max_baseline_scratch, env.active_scratch_depth);
        if (slot < arch_.baseline_scratch_pool_size) {
          phys = arch_.baseline_scratch_regs[slot];
        } else {
          QueueWarning(fctx, raw_init->getExprLoc(),
                       "ScratchRegisterScope acquisition exceeds pool size (" +
                           std::to_string(arch_.baseline_scratch_pool_size) +
                           ").");
        }
      } else if (acq_kind == ScratchScopeKind::kArm64UseScratch) {
        fctx.used_arm64_scratch_scope = true;
        if (env.arm64_avail_scratch_mask != 0) {
          phys = llvm::countr_zero(env.arm64_avail_scratch_mask);
          env.arm64_avail_scratch_mask &= ~(1u << phys);
          if (!(env.arm64_included_scratch_mask & (1u << phys))) {
            env.arm64_caller_scratches_held++;
            fctx.max_arm64_scratch_used = std::max(
                fctx.max_arm64_scratch_used, env.arm64_caller_scratches_held);
            env.path_max_arm64_scratch = std::max(
                env.path_max_arm64_scratch, env.arm64_caller_scratches_held);
          }
        } else {
          QueueWarning(
              fctx, raw_init->getExprLoc(),
              "UseScratchRegisterScope acquisition exhausts available scratch "
              "register pool.");
        }
      }

      SymRegState s;
      s.status = RegStatus::kAlive;
      s.root_decl = canon_vd;
      s.phys_reg = phys;
      s.scratch_slot = slot;
      s.scratch_scope_id = static_cast<int>(env.scratch_scope_stack.size());
      s.def_version = fctx.next_version++;
      s.arm64_width = ExprExplicitArm64Width(raw_init);
      env.syms[canon_vd] = s;

      if (phys >= 0 && phys < arch_.num_gprs) {
        WriteOrClobberPhys(phys, /*is_clobber=*/true, /*from_sym_param=*/true,
                           raw_init->getExprLoc(),
                           "acquired by scratch register scope", env, fctx,
                           /*skip_sym=*/canon_vd);
      }

      for (auto& [other_d, other_s] : env.syms) {
        if (other_d != canon_vd && other_s.scratch_slot == slot) {
          other_s.status = RegStatus::kClobbered;
          other_s.def_version = fctx.next_version++;
          std::string reg_desc = (phys >= 0)
                                     ? std::string(arch_.gpr_names[phys])
                                     : "scratch slot " + std::to_string(slot);
          other_s.clobber = {
              raw_init->getExprLoc(),
              "clobbered when scratch scope re-allocated " + reg_desc};
        }
      }
      if (phys >= 0 && phys < arch_.num_gprs) {
        env.phys[phys].status = RegStatus::kAlive;
        env.phys[phys].def_version = fctx.next_version++;
      }
      return;
    }

    VisitStmt(raw_init, env, fctx);

    // Handle Maglev TemporaryRegisterScope::Acquire* binding (budget was
    // already accounted for inside VisitStmt -> ApplyCallEffect).
    bool maglev_acq_scratch = false;
    bool maglev_acq_double = false;
    if (MatchMaglevTempAcquireCall(raw_init, &maglev_acq_scratch,
                                   &maglev_acq_double)) {
      int slot = env.active_scratch_depth++;
      SymRegState s;
      s.status = RegStatus::kAlive;
      s.root_decl = canon_vd;
      s.is_double_reg = is_double_var || maglev_acq_double;
      if (!s.is_double_reg) {
        Arm64RegWidth ew = ExprExplicitArm64Width(raw_init);
        s.arm64_width =
            (ew != Arm64RegWidth::kUnknown) ? ew : Arm64RegWidth::kX;
      }
      s.maglev_role =
          maglev_acq_scratch ? MaglevRole::kScratch : MaglevRole::kTemp;
      s.scratch_slot = slot;
      s.scratch_scope_id = static_cast<int>(env.scratch_scope_stack.size());
      s.def_version = fctx.next_version++;
      if (!s.is_double_reg && maglev_acq_scratch &&
          arch_.maglev_single_gpr_scratch >= 0) {
        s.phys_reg = arch_.maglev_single_gpr_scratch;
        env.phys[s.phys_reg].status = RegStatus::kAlive;
        env.phys[s.phys_reg].def_version = fctx.next_version++;
      }
      env.syms[canon_vd] = std::move(s);
      return;
    }

    if (const clang::ValueDecl* rhs_sym = ResolveSymDecl(init, env)) {
      auto it = env.syms.find(rhs_sym);
      if (it != env.syms.end()) {
        SymRegState copied = it->second;
        Arm64RegWidth ew = ExprExplicitArm64Width(raw_init);
        if (ew != Arm64RegWidth::kUnknown) copied.arm64_width = ew;
        env.syms[canon_vd] = std::move(copied);
        return;
      }
    }

    SymRegState s;
    s.status = RegStatus::kAlive;
    s.root_decl = canon_vd;
    s.is_double_reg = is_double_var;
    s.def_version = fctx.next_version++;

    // Tag Maglev `ToRegister` / `ToDoubleRegister` provenance and propagate
    // known fixed GPR register from SetValueLocationConstraints if unique.
    if (auto m = MatchMaglevToRegisterCall(raw_init, env)) {
      s.is_double_reg = is_double_var || m->is_double;
      if (!s.is_double_reg) {
        Arm64RegWidth ew = ExprExplicitArm64Width(raw_init);
        s.arm64_width =
            (ew != Arm64RegWidth::kUnknown) ? ew : Arm64RegWidth::kX;
      }
      if (m->is_result) {
        s.maglev_role = MaglevRole::kResult;
        s.holds_entry_value = false;
        if (!s.is_double_reg && fctx.maglev_node_cls) {
          const MaglevNodeSpec& spec = GetMaglevNodeSpec(fctx.maglev_node_cls);
          auto paths = GetCompatibleMaglevPaths(spec, env);
          int common_reg = -2;
          for (const MaglevConstraintPath* p : paths) {
            int pr = (p->result.kind == MaglevResultKind::kDefineAsFixed &&
                      !p->result.is_double_fixed)
                         ? p->result.fixed_reg
                         : -1;
            if (common_reg == -2)
              common_reg = pr;
            else if (common_reg != pr)
              common_reg = -1;
          }
          if (common_reg >= 0 && common_reg < arch_.num_gprs) {
            s.phys_reg = common_reg;
          }
        }
      } else {
        s.maglev_role = MaglevRole::kInput;
        s.maglev_input_key = m->input_key;
        s.maglev_input_label = m->input_label;
        s.holds_entry_value = !env.maglev_written_inputs.count(m->input_key);
        if (!s.is_double_reg && fctx.maglev_node_cls) {
          const MaglevNodeSpec& spec = GetMaglevNodeSpec(fctx.maglev_node_cls);
          auto paths = GetCompatibleMaglevPaths(spec, env);
          int common_reg = -2;
          for (const MaglevConstraintPath* p : paths) {
            const MaglevInputConstraint* ic =
                LookupInputConstraint(*p, m->input_key);
            int pr = (ic &&
                      (ic->kind == MaglevInputKind::kUseFixed ||
                       ic->kind == MaglevInputKind::kUseAndClobberFixed) &&
                      !ic->is_double_fixed)
                         ? ic->fixed_reg
                         : -1;
            if (common_reg == -2)
              common_reg = pr;
            else if (common_reg != pr)
              common_reg = -1;
          }
          if (common_reg >= 0 && common_reg < arch_.num_gprs) {
            s.phys_reg = common_reg;
          }
        }
      }
    }

    if (!s.is_double_reg && s.phys_reg < 0) {
      if (auto p = ResolvePhysReg(raw_init, env)) {
        s.phys_reg = *p;
        if (*p == -1) {
          env.known_no_reg.insert(canon_vd);
        } else if (*p >= 0 && *p < arch_.num_gprs) {
          s.status = env.phys[*p].status;
          s.clobber = env.phys[*p].clobber;
        }
      }
    }
    env.syms[canon_vd] = s;
  }

  void PopScratchScopesToDepth(size_t target_depth,
                               clang::SourceLocation end_loc, Env& env,
                               FunctionAnalysisContext& fctx) {
    while (env.scratch_scope_stack.size() > target_depth) {
      ScratchScopeFrame frame = env.scratch_scope_stack.back();
      env.scratch_scope_stack.pop_back();
      if (frame.is_maglev_save_call) {
        env.maglev_save_call_depth =
            std::max(0, env.maglev_save_call_depth - 1);
        if (!env.asm_stack.empty() &&
            env.asm_stack.back().kind == SavedStackEntry::kAllRegs) {
          SavedStackEntry entry = std::move(env.asm_stack.back());
          env.asm_stack.pop_back();
          env.clobbered_caller_saved = entry.saved_clobbered_caller_saved;
          int keep_result_phys = -1;
          if (env.maglev_result_written && env.maglev_result_sym &&
              env.syms.count(env.maglev_result_sym) &&
              !env.syms[env.maglev_result_sym].is_double_reg) {
            keep_result_phys = env.syms[env.maglev_result_sym].phys_reg;
          }
          for (int r = 0; r < arch_.num_gprs; ++r) {
            if (r == keep_result_phys) continue;
            env.phys[r] = entry.saved_phys_all[r];
          }
          for (const auto& [d, saved_s] : entry.saved_syms_all) {
            if (env.maglev_snapshot_cleared_syms.contains(d)) {
              continue;
            }
            if (env.maglev_result_written && d == env.maglev_result_sym) {
              continue;
            }
            if (env.maglev_result_written &&
                saved_s.maglev_role == MaglevRole::kResult) {
              continue;
            }
            env.syms[d] = saved_s;
          }
        }
        continue;
      }
      env.active_scratch_depth = frame.base_depth;
      if (frame.kind == ScratchScopeKind::kArm64UseScratch) {
        env.arm64_avail_scratch_mask = frame.saved_arm64_avail;
        env.arm64_included_scratch_mask = frame.saved_arm64_included;
        env.arm64_caller_scratches_held = frame.saved_arm64_caller_held;
      } else if (frame.kind == ScratchScopeKind::kMaglevTemp) {
        env.maglev_gpr_temps_used = frame.saved_maglev_gpr_temps;
        env.maglev_double_temps_used = frame.saved_maglev_double_temps;
        env.maglev_gpr_scratches_used = frame.saved_maglev_gpr_scratches;
        env.maglev_double_scratches_used = frame.saved_maglev_double_scratches;
        env.maglev_gpr_temps_included = frame.saved_maglev_gpr_included;
        env.maglev_double_temps_included = frame.saved_maglev_double_included;
        env.maglev_gpr_temps_unbounded = frame.saved_maglev_gpr_unbounded;
        env.maglev_double_temps_unbounded = frame.saved_maglev_double_unbounded;
        env.maglev_temps_reset = frame.saved_maglev_temps_reset;
      }
      for (auto& [decl, s] : env.syms) {
        if (s.scratch_slot >= frame.base_depth) {
          s.status = RegStatus::kClobbered;
          s.def_version = fctx.next_version++;
          s.clobber = {
              end_loc,
              "scratch register scope owning this register ended here"};
        }
      }
    }
  }

  void VisitLoop(const clang::Stmt* init_stmt, const clang::VarDecl* cond_var,
                 const clang::Expr* cond_expr, const clang::Stmt* inc_stmt,
                 const clang::Stmt* body_stmt, Env& env,
                 FunctionAnalysisContext& fctx) {
    size_t saved_scope_depth = env.scratch_scope_stack.size();
    if (init_stmt) VisitStmt(init_stmt, env, fctx);
    if (!env.reachable) {
      PopScratchScopesToDepth(saved_scope_depth, body_stmt->getEndLoc(), env,
                              fctx);
      return;
    }

    CfTarget loop_cf;
    loop_cf.is_loop = true;
    loop_cf.scratch_scope_depth = env.scratch_scope_stack.size();
    loop_cf.break_env.reachable = false;
    loop_cf.continue_env.reachable = false;
    fctx.cf_stack.push_back(std::move(loop_cf));

    auto run_one_iter = [&](const Env& header_in, Env* out_latch,
                            Env* out_cond_exit) {
      Env cur = header_in;
      fctx.cf_stack.back().continue_env.reachable = false;
      if (cond_var) {
        BindVariableInit(cond_var, cond_var->getInit(), cur, fctx);
      }
      if (cond_expr) {
        VisitStmt(cond_expr, cur, fctx);
        Env body_env = cur;
        Env exit_env = cur;
        RefineConditionEnv(cond_expr, body_env, exit_env, fctx);
        *out_cond_exit = exit_env;
        cur = body_env;
      } else {
        out_cond_exit->reachable = false;
      }
      VisitStmt(body_stmt, cur, fctx);
      cur = MergeEnvs(cur, fctx.cf_stack.back().continue_env, arch_.num_gprs);
      if (inc_stmt && cur.reachable) {
        VisitStmt(inc_stmt, cur, fctx);
      }
      *out_latch = std::move(cur);
    };

    Env latch1;
    Env cond_exit1;
    run_one_iter(env, &latch1, &cond_exit1);

    Env merged_exit = cond_exit1;
    if (latch1.reachable) {
      Env header2 = MergeEnvs(env, latch1, arch_.num_gprs);
      if (!EnvEquivalentForFixpoint(env, header2, arch_.num_gprs)) {
        Env latch2;
        Env cond_exit2;
        run_one_iter(header2, &latch2, &cond_exit2);
        merged_exit = MergeEnvs(merged_exit, cond_exit2, arch_.num_gprs);
      }
    }

    merged_exit =
        MergeEnvs(merged_exit, fctx.cf_stack.back().break_env, arch_.num_gprs);
    fctx.cf_stack.pop_back();
    env = std::move(merged_exit);
    PopScratchScopesToDepth(saved_scope_depth, body_stmt->getEndLoc(), env,
                            fctx);
  }

  void VisitStmt(const clang::Stmt* stmt, Env& env,
                 FunctionAnalysisContext& fctx) {
    if (!stmt || !env.reachable) return;

    if (const auto* compound = llvm::dyn_cast<clang::CompoundStmt>(stmt)) {
      size_t saved_scope_depth = env.scratch_scope_stack.size();
      for (const clang::Stmt* child : compound->body()) {
        VisitStmt(child, env, fctx);
      }
      PopScratchScopesToDepth(saved_scope_depth, compound->getRBracLoc(), env,
                              fctx);
      return;
    }

    if (const auto* ds = llvm::dyn_cast<clang::DeclStmt>(stmt)) {
      for (const clang::Decl* d : ds->decls()) {
        if (const auto* vd = llvm::dyn_cast<clang::VarDecl>(d)) {
          if (vd->isLocalVarDecl() && !vd->isConstexpr()) {
            BindVariableInit(vd, vd->getInit(), env, fctx);
          }
        }
      }
      return;
    }

    if (const auto* ifs = llvm::dyn_cast<clang::IfStmt>(stmt)) {
      if (const clang::Stmt* init = ifs->getInit()) {
        VisitStmt(init, env, fctx);
      }
      if (const clang::VarDecl* cond_var = ifs->getConditionVariable()) {
        BindVariableInit(cond_var, cond_var->getInit(), env, fctx);
      }
      const clang::Expr* cond = ifs->getCond();
      bool const_known = false;
      bool const_val = false;
      if (cond && !cond->isValueDependent()) {
        const_known = cond->EvaluateAsBooleanCondition(const_val, ast_ctx_);
      }
      VisitStmt(cond, env, fctx);

      Env then_env = env;
      Env else_env = env;
      if (const_known) {
        if (const_val) {
          else_env.reachable = false;
        } else {
          then_env.reachable = false;
        }
      }
      RefineConditionEnv(cond, then_env, else_env, fctx);
      if (fctx.maglev_node_cls) {
        ApplyMaglevCondBranch(cond, true, then_env);
        ApplyMaglevCondBranch(cond, false, else_env);
      }

      VisitStmt(ifs->getThen(), then_env, fctx);
      if (const clang::Stmt* else_stmt = ifs->getElse()) {
        VisitStmt(else_stmt, else_env, fctx);
      }
      env = MergeEnvs(then_env, else_env, arch_.num_gprs);
      return;
    }

    if (const auto* for_stmt = llvm::dyn_cast<clang::ForStmt>(stmt)) {
      VisitLoop(for_stmt->getInit(), for_stmt->getConditionVariable(),
                for_stmt->getCond(), for_stmt->getInc(), for_stmt->getBody(),
                env, fctx);
      return;
    }

    if (const auto* while_stmt = llvm::dyn_cast<clang::WhileStmt>(stmt)) {
      VisitLoop(nullptr, while_stmt->getConditionVariable(),
                while_stmt->getCond(), nullptr, while_stmt->getBody(), env,
                fctx);
      return;
    }

    if (const auto* range_stmt = llvm::dyn_cast<clang::CXXForRangeStmt>(stmt)) {
      size_t saved_scope_depth = env.scratch_scope_stack.size();
      if (range_stmt->getInit()) VisitStmt(range_stmt->getInit(), env, fctx);
      if (range_stmt->getRangeStmt()) {
        VisitStmt(range_stmt->getRangeStmt(), env, fctx);
      }
      VisitLoop(nullptr, nullptr, range_stmt->getCond(), range_stmt->getInc(),
                range_stmt->getBody(), env, fctx);
      PopScratchScopesToDepth(saved_scope_depth, range_stmt->getEndLoc(), env,
                              fctx);
      return;
    }

    if (const auto* do_stmt = llvm::dyn_cast<clang::DoStmt>(stmt)) {
      CfTarget loop_cf;
      loop_cf.is_loop = true;
      loop_cf.scratch_scope_depth = env.scratch_scope_stack.size();
      loop_cf.break_env.reachable = false;
      loop_cf.continue_env.reachable = false;
      fctx.cf_stack.push_back(std::move(loop_cf));

      auto is_const_false = [&](const clang::Expr* e) {
        if (!e || e->isValueDependent()) return false;
        bool bval = false;
        return e->EvaluateAsBooleanCondition(bval, ast_ctx_) && !bval;
      };

      Env entry_env = env;
      Env cur = env;
      VisitStmt(do_stmt->getBody(), cur, fctx);
      cur = MergeEnvs(cur, fctx.cf_stack.back().continue_env, arch_.num_gprs);

      Env merged_exit;
      merged_exit.reachable = false;
      if (cur.reachable) {
        const clang::Expr* cond = do_stmt->getCond();
        VisitStmt(cond, cur, fctx);
        Env back_env = cur;
        Env exit_env = cur;
        RefineConditionEnv(cond, back_env, exit_env, fctx);
        merged_exit = exit_env;

        if (!is_const_false(cond) && back_env.reachable) {
          Env header2 = MergeEnvs(entry_env, back_env, arch_.num_gprs);
          if (!EnvEquivalentForFixpoint(entry_env, header2, arch_.num_gprs)) {
            fctx.cf_stack.back().continue_env.reachable = false;
            Env cur2 = header2;
            VisitStmt(do_stmt->getBody(), cur2, fctx);
            cur2 = MergeEnvs(cur2, fctx.cf_stack.back().continue_env,
                             arch_.num_gprs);
            if (cur2.reachable) {
              VisitStmt(cond, cur2, fctx);
              Env back2 = cur2;
              Env exit2 = cur2;
              RefineConditionEnv(cond, back2, exit2, fctx);
              merged_exit = MergeEnvs(merged_exit, exit2, arch_.num_gprs);
            }
          }
        }
      }

      merged_exit = MergeEnvs(merged_exit, fctx.cf_stack.back().break_env,
                              arch_.num_gprs);
      fctx.cf_stack.pop_back();
      env = std::move(merged_exit);
      return;
    }

    if (llvm::isa<clang::BreakStmt>(stmt)) {
      if (!fctx.cf_stack.empty()) {
        PopScratchScopesToDepth(fctx.cf_stack.back().scratch_scope_depth,
                                stmt->getEndLoc(), env, fctx);
        fctx.cf_stack.back().break_env =
            MergeEnvs(fctx.cf_stack.back().break_env, env, arch_.num_gprs);
      }
      env.reachable = false;
      return;
    }

    if (llvm::isa<clang::ContinueStmt>(stmt)) {
      for (auto it = fctx.cf_stack.rbegin(); it != fctx.cf_stack.rend(); ++it) {
        if (it->is_loop) {
          PopScratchScopesToDepth(it->scratch_scope_depth, stmt->getEndLoc(),
                                  env, fctx);
          it->continue_env = MergeEnvs(it->continue_env, env, arch_.num_gprs);
          break;
        }
      }
      env.reachable = false;
      return;
    }

    if (const auto* sw = llvm::dyn_cast<clang::SwitchStmt>(stmt)) {
      if (const clang::Stmt* init = sw->getInit()) {
        VisitStmt(init, env, fctx);
      }
      if (const clang::VarDecl* cond_var = sw->getConditionVariable()) {
        BindVariableInit(cond_var, cond_var->getInit(), env, fctx);
      }
      VisitStmt(sw->getCond(), env, fctx);
      Env entry_env = env;
      bool has_default = false;

      CfTarget sw_cf;
      sw_cf.is_loop = false;
      sw_cf.scratch_scope_depth = env.scratch_scope_stack.size();
      sw_cf.break_env.reachable = false;
      sw_cf.continue_env.reachable = false;
      fctx.cf_stack.push_back(std::move(sw_cf));

      Env fallthrough_exit;
      fallthrough_exit.reachable = false;

      if (const auto* body =
              llvm::dyn_cast_or_null<clang::CompoundStmt>(sw->getBody())) {
        std::string sw_cond_str;
        std::vector<int64_t> all_case_vals;
        if (fctx.maglev_node_cls && sw->getCond()) {
          sw_cond_str =
              CanonicalizeExpr(sw->getCond(), entry_env.local_expr_bindings);
          for (const clang::Stmt* child : body->body()) {
            const clang::Stmt* cur = child;
            while (llvm::isa<clang::CaseStmt>(cur) ||
                   llvm::isa<clang::DefaultStmt>(cur)) {
              if (const auto* cs_node = llvm::dyn_cast<clang::CaseStmt>(cur)) {
                ExprSubMap empty_subs;
                if (auto iv = TryEvalConstInt(cs_node->getLHS(), empty_subs)) {
                  all_case_vals.push_back(*iv);
                }
                cur = cs_node->getSubStmt();
              } else {
                cur = llvm::cast<clang::DefaultStmt>(cur)->getSubStmt();
              }
            }
          }
        }
        Env cur_env;
        cur_env.reachable = false;
        unsigned num_cases = 0;
        for (const clang::Stmt* child : body->body()) {
          if (llvm::isa<clang::CaseStmt>(child) ||
              llvm::isa<clang::DefaultStmt>(child)) {
            bool group_has_default = false;
            std::vector<int64_t> group_case_vals;
            const clang::Stmt* sub = child;
            while (llvm::isa<clang::CaseStmt>(sub) ||
                   llvm::isa<clang::DefaultStmt>(sub)) {
              if (const auto* cs_node = llvm::dyn_cast<clang::CaseStmt>(sub)) {
                ++num_cases;
                if (fctx.maglev_node_cls) {
                  ExprSubMap empty_subs;
                  if (auto iv =
                          TryEvalConstInt(cs_node->getLHS(), empty_subs)) {
                    group_case_vals.push_back(*iv);
                  }
                }
                sub = cs_node->getSubStmt();
              } else {
                has_default = true;
                group_has_default = true;
                sub = llvm::cast<clang::DefaultStmt>(sub)->getSubStmt();
              }
            }
            Env case_entry = entry_env;
            if (fctx.maglev_node_cls && !sw_cond_str.empty()) {
              if (group_has_default && group_case_vals.empty()) {
                for (int64_t v : all_case_vals) {
                  MaglevCondAtom atom{sw_cond_str + " == " + std::to_string(v),
                                      false};
                  if (!CondAtomListsCompatible(case_entry.active_conds,
                                               {atom})) {
                    case_entry.reachable = false;
                    break;
                  }
                  if (std::find(case_entry.active_conds.begin(),
                                case_entry.active_conds.end(),
                                atom) == case_entry.active_conds.end()) {
                    case_entry.active_conds.push_back(std::move(atom));
                  }
                }
              } else if (!group_has_default && group_case_vals.size() == 1) {
                MaglevCondAtom atom{
                    sw_cond_str + " == " + std::to_string(group_case_vals[0]),
                    true};
                if (!CondAtomListsCompatible(case_entry.active_conds, {atom})) {
                  case_entry.reachable = false;
                } else if (std::find(case_entry.active_conds.begin(),
                                     case_entry.active_conds.end(),
                                     atom) == case_entry.active_conds.end()) {
                  case_entry.active_conds.push_back(std::move(atom));
                }
              }
            }
            cur_env = MergeEnvs(cur_env, case_entry, arch_.num_gprs);
            VisitStmt(sub, cur_env, fctx);
          } else {
            VisitStmt(child, cur_env, fctx);
          }
        }
        fallthrough_exit = cur_env;
        if (!has_default && sw->getCond()) {
          if (const auto* et =
                  sw->getCond()->getType()->getAs<clang::EnumType>()) {
            unsigned enum_count = 0;
            for (const auto* e : et->getDecl()->enumerators()) {
              (void)e;
              ++enum_count;
            }
            if (enum_count > 0 && num_cases >= enum_count) {
              has_default = true;
            }
          }
        }
      } else {
        VisitStmt(sw->getBody(), env, fctx);
        fallthrough_exit = env;
      }

      Env merged_exit = MergeEnvs(
          fallthrough_exit, fctx.cf_stack.back().break_env, arch_.num_gprs);
      fctx.cf_stack.pop_back();
      if (!has_default) {
        merged_exit = MergeEnvs(merged_exit, entry_env, arch_.num_gprs);
      }
      env = std::move(merged_exit);
      return;
    }

    if (const auto* ret = llvm::dyn_cast<clang::ReturnStmt>(stmt)) {
      const clang::Expr* val = ret->getRetValue();
      if (val) {
        VisitStmt(val, env, fctx);
        if (env.reachable && IsOperandType(fctx.fn->getReturnType())) {
          RecordReturnedOperandSummary(val, env, fctx);
        }
      }
      if (env.reachable) {
        PopScratchScopesToDepth(fctx.base_scratch_scope_depth, ret->getEndLoc(),
                                env, fctx);
        fctx.exit_envs.push_back(env);
      }
      env.reachable = false;
      return;
    }

    if (const auto* cond_op =
            llvm::dyn_cast<clang::ConditionalOperator>(stmt)) {
      const clang::Expr* cond = cond_op->getCond();
      bool const_known = false;
      bool const_val = false;
      if (cond && !cond->isValueDependent()) {
        const_known = cond->EvaluateAsBooleanCondition(const_val, ast_ctx_);
      }
      VisitStmt(cond, env, fctx);
      Env true_env = env;
      Env false_env = env;
      if (const_known) {
        if (const_val) {
          false_env.reachable = false;
        } else {
          true_env.reachable = false;
        }
      }
      RefineConditionEnv(cond, true_env, false_env, fctx);
      if (fctx.maglev_node_cls) {
        ApplyMaglevCondBranch(cond, true, true_env);
        ApplyMaglevCondBranch(cond, false, false_env);
      }
      VisitStmt(cond_op->getTrueExpr(), true_env, fctx);
      VisitStmt(cond_op->getFalseExpr(), false_env, fctx);
      env = MergeEnvs(true_env, false_env, arch_.num_gprs);
      return;
    }

    if (const auto* oce = llvm::dyn_cast<clang::CXXOperatorCallExpr>(stmt)) {
      if (oce->getOperator() == clang::OO_Equal && oce->getNumArgs() == 2) {
        const clang::Expr* lhs = UnwrapExpr(oce->getArg(0));
        const clang::Expr* rhs = oce->getArg(1);
        if (const auto* dre = llvm::dyn_cast_or_null<clang::DeclRefExpr>(lhs)) {
          if (const auto* vd = llvm::dyn_cast<clang::VarDecl>(dre->getDecl())) {
            BindVariableInit(vd, rhs, env, fctx);
            return;
          }
        }
      }
      if (oce->getOperator() == clang::OO_Call &&
          InlineLocalLambdaCall(oce, env, fctx)) {
        return;
      }
    }

    if (const auto* bo = llvm::dyn_cast<clang::BinaryOperator>(stmt)) {
      if (bo->getOpcode() == clang::BO_Assign) {
        const clang::Expr* lhs = UnwrapExpr(bo->getLHS());
        const clang::Expr* rhs = bo->getRHS();
        if (const auto* dre = llvm::dyn_cast_or_null<clang::DeclRefExpr>(lhs)) {
          if (const auto* vd = llvm::dyn_cast<clang::VarDecl>(dre->getDecl())) {
            BindVariableInit(vd, rhs, env, fctx);
            return;
          }
        }
      } else if (bo->getOpcode() == clang::BO_LAnd ||
                 bo->getOpcode() == clang::BO_LOr) {
        const clang::Expr* lhs = bo->getLHS();
        bool is_and = (bo->getOpcode() == clang::BO_LAnd);
        bool const_known = false;
        bool const_val = false;
        if (lhs && !lhs->isValueDependent()) {
          const_known = lhs->EvaluateAsBooleanCondition(const_val, ast_ctx_);
        }
        VisitStmt(lhs, env, fctx);
        Env rhs_env = env;
        Env short_env = env;
        if (const_known) {
          if ((is_and && !const_val) || (!is_and && const_val)) {
            rhs_env.reachable = false;
          } else {
            short_env.reachable = false;
          }
        }
        if (is_and) {
          RefineConditionEnv(lhs, rhs_env, short_env, fctx);
          if (fctx.maglev_node_cls) {
            ApplyMaglevCondBranch(lhs, true, rhs_env);
            ApplyMaglevCondBranch(lhs, false, short_env);
          }
        } else {
          RefineConditionEnv(lhs, short_env, rhs_env, fctx);
          if (fctx.maglev_node_cls) {
            ApplyMaglevCondBranch(lhs, true, short_env);
            ApplyMaglevCondBranch(lhs, false, rhs_env);
          }
        }
        VisitStmt(bo->getRHS(), rhs_env, fctx);
        env = MergeEnvs(rhs_env, short_env, arch_.num_gprs);
        return;
      }
    }

    if (const auto* cce = llvm::dyn_cast<clang::CXXConstructExpr>(stmt)) {
      const clang::CXXConstructorDecl* ctor = cce->getConstructor();
      if (ctor && !ctor->isCopyOrMoveConstructor() &&
          GetScratchScopeKind(cce->getType()) == ScratchScopeKind::kNone &&
          !IsTypeNamed(cce->getType(), "SaveRegisterStateForCall") &&
          TakesAssemblerParam(ctor)) {
        if (fctx.maglev_node_cls && InlineMaglevCtorCall(cce, env, fctx)) {
          return;
        }
        for (const clang::Expr* arg : cce->arguments()) {
          VisitStmt(arg, env, fctx);
        }
        const FuncSummary& sum = GetOrInferSummary(ctor);
        clang::SourceLocation cloc = cce->getExprLoc();
        std::string cname = ctor->getQualifiedNameAsString();
        if (sum.maglev_gpr_temps_peak > 0 || sum.maglev_gpr_temps_leaked > 0) {
          AccountMaglevTempUse(false, false, sum.maglev_gpr_temps_peak,
                               sum.maglev_gpr_temps_leaked, cloc,
                               "Constructor '" + cname + "'", env, fctx);
        }
        if (sum.maglev_double_temps_peak > 0 ||
            sum.maglev_double_temps_leaked > 0) {
          AccountMaglevTempUse(false, true, sum.maglev_double_temps_peak,
                               sum.maglev_double_temps_leaked, cloc,
                               "Constructor '" + cname + "'", env, fctx);
        }
        if (sum.maglev_gpr_scratches_peak > 0 ||
            sum.maglev_gpr_scratches_leaked > 0) {
          AccountMaglevTempUse(true, false, sum.maglev_gpr_scratches_peak,
                               sum.maglev_gpr_scratches_leaked, cloc,
                               "Constructor '" + cname + "'", env, fctx);
        }
        if (sum.maglev_double_scratches_peak > 0 ||
            sum.maglev_double_scratches_leaked > 0) {
          AccountMaglevTempUse(true, true, sum.maglev_double_scratches_peak,
                               sum.maglev_double_scratches_leaked, cloc,
                               "Constructor '" + cname + "'", env, fctx);
        }
        if (sum.maglev_uses_save_reg_state) {
          fctx.maglev_used_save_reg_state = true;
          CheckMaglevSaveRegStateProperty(cloc, fctx);
        }
        if (sum.maglev_calls_outside_save_scope) {
          if (env.maglev_save_call_depth == 0 && !sum.no_fallthrough) {
            fctx.maglev_called_outside_save_scope = true;
          }
          CheckMaglevCallProperty(cname, sum, cloc, env, fctx);
        }
        return;
      }
    }

    if (const auto* lam = llvm::dyn_cast<clang::LambdaExpr>(stmt)) {
      // Defining a lambda does not execute its body; only visit capture
      // initializers. Deferred lambdas and local lambda calls are analyzed
      // when registered or invoked.
      for (const clang::Expr* cap_init : lam->capture_inits()) {
        if (cap_init) VisitStmt(cap_init, env, fctx);
      }
      return;
    }

    if (const auto* ce = llvm::dyn_cast<clang::CallExpr>(stmt)) {
      if (const auto* mce = llvm::dyn_cast<clang::CXXMemberCallExpr>(ce)) {
        if (const clang::Expr* obj = mce->getImplicitObjectArgument()) {
          VisitStmt(obj, env, fctx);
        }
      }
      for (const clang::Expr* arg : ce->arguments()) {
        VisitStmt(arg, env, fctx);
      }
      ApplyCallEffect(ce, env, fctx);
      return;
    }

    for (const clang::Stmt* child : stmt->children()) {
      if (child) VisitStmt(child, env, fctx);
    }
  }

  void RecordReturnedOperandSummary(const clang::Expr* ret_expr, Env& env,
                                    FunctionAnalysisContext& fctx) {
    OperandDep dep = ExtractOperandDep(ret_expr, env, fctx);
    for (const auto& [r, ver] : dep.scratch_phys_versions) {
      (void)ver;
      if (r >= 0 && r < arch_.num_gprs) {
        fctx.returned_operand_scratch_phys_mask |= (1u << r);
      }
    }
    for (const auto& [sd, ver] : dep.scratch_sym_versions) {
      (void)ver;
      auto it = env.syms.find(sd);
      if (it != env.syms.end()) {
        if (it->second.param_index >= 0 && it->second.param_index < 32) {
          fctx.returned_operand_scratch_param_mask |=
              (1u << it->second.param_index);
        } else if (it->second.phys_reg >= 0 &&
                   it->second.phys_reg < arch_.num_gprs) {
          fctx.returned_operand_scratch_phys_mask |=
              (1u << it->second.phys_reg);
        }
      }
    }
    for (int r = 0; r < arch_.num_gprs; ++r) {
      if (dep.phys_mask & (1u << r)) {
        if (!env.phys[r].holds_entry_value) {
          fctx.returned_operand_scratch_phys_mask |= (1u << r);
        } else if (!(arch_.pinned_mask & (1u << r))) {
          fctx.returned_operand_input_phys_mask |= (1u << r);
        }
      }
    }
    for (const clang::ValueDecl* sd : dep.sym_decls) {
      auto it = env.syms.find(sd);
      if (it == env.syms.end()) continue;
      const SymRegState& s = it->second;
      if (s.param_index >= 0 && s.param_index < 32) {
        if (!s.holds_entry_value) {
          fctx.returned_operand_scratch_param_mask |= (1u << s.param_index);
        } else {
          fctx.returned_operand_input_param_mask |= (1u << s.param_index);
        }
      } else if (s.phys_reg >= 0 && s.phys_reg < arch_.num_gprs &&
                 !env.phys[s.phys_reg].holds_entry_value) {
        fctx.returned_operand_scratch_phys_mask |= (1u << s.phys_reg);
      }
    }
  }

  FuncSummary BuildInferredSummary(const FunctionAnalysisContext& fctx) {
    FuncSummary inf;
    unsigned num_params = fctx.fn->getNumParams();
    inf.param_roles.assign(num_params, ParamRole::kNone);
    inf.early_clobber_param_pairs = fctx.early_clobber_param_pairs;
    inf.early_clobber_phys_before_param = fctx.early_clobber_phys_before_param;
    inf.baseline_scratch_used = fctx.max_baseline_scratch_used;
    inf.arm64_scratch_used = fctx.max_arm64_scratch_used;
    inf.returned_operand_scratch_phys_mask =
        fctx.returned_operand_scratch_phys_mask;
    inf.returned_operand_scratch_param_mask =
        fctx.returned_operand_scratch_param_mask;
    inf.returned_operand_input_param_mask =
        fctx.returned_operand_input_param_mask;
    inf.returned_operand_input_phys_mask =
        fctx.returned_operand_input_phys_mask;

    Env merged_exit;
    merged_exit.reachable = false;
    for (const Env& e : fctx.exit_envs) {
      merged_exit = MergeEnvs(merged_exit, e, arch_.num_gprs);
    }

    if (merged_exit.reachable) {
      inf.must_baseline_scratch_used = merged_exit.path_max_baseline_scratch;
      inf.must_arm64_scratch_used = merged_exit.path_max_arm64_scratch;
    }

    inf.no_fallthrough = !merged_exit.reachable;
    inf.is_push = fctx.any_push_param0 && !fctx.any_pop_param0;
    inf.is_pop = fctx.any_pop_param0 && !fctx.any_push_param0;
    inf.clobbers_caller_saved =
        merged_exit.reachable && merged_exit.clobbered_caller_saved;

    inf.maglev_gpr_temps_peak = fctx.max_maglev_gpr_temps;
    inf.maglev_double_temps_peak = fctx.max_maglev_double_temps;
    inf.maglev_gpr_scratches_peak = fctx.max_maglev_gpr_scratches;
    inf.maglev_double_scratches_peak = fctx.max_maglev_double_scratches;
    if (merged_exit.reachable) {
      if (!merged_exit.maglev_temps_reset &&
          !merged_exit.maglev_gpr_temps_unbounded) {
        inf.maglev_gpr_temps_leaked =
            std::max(0, merged_exit.maglev_gpr_temps_used -
                            merged_exit.maglev_gpr_temps_included);
      }
      if (!merged_exit.maglev_temps_reset &&
          !merged_exit.maglev_double_temps_unbounded) {
        inf.maglev_double_temps_leaked =
            std::max(0, merged_exit.maglev_double_temps_used -
                            merged_exit.maglev_double_temps_included);
      }
      inf.maglev_gpr_scratches_leaked =
          std::max(0, merged_exit.maglev_gpr_scratches_used);
      inf.maglev_double_scratches_leaked =
          std::max(0, merged_exit.maglev_double_scratches_used);
    }
    inf.maglev_uses_save_reg_state = fctx.maglev_used_save_reg_state;
    inf.maglev_calls_outside_save_scope = fctx.maglev_called_outside_save_scope;

    for (unsigned i = 0; i < num_params; ++i) {
      const clang::ParmVarDecl* p = fctx.fn->getParamDecl(i);
      const clang::ValueDecl* canon_p = p->getCanonicalDecl();
      if (IsLabelPtrType(p->getType())) {
        if (fctx.bound_labels.contains(canon_p)) {
          inf.param_roles[i] = ParamRole::kBind;
        } else if (fctx.forward_label_states.count(canon_p) ||
                   fctx.backward_label_states.count(canon_p)) {
          inf.param_roles[i] = ParamRole::kBranch;
        }
        continue;
      }
      if (!IsTrackableRegType(p->getType()) && !IsOperandType(p->getType())) {
        continue;
      }
      if (IsOperandType(p->getType())) {
        inf.param_roles[i] = ParamRole::kIn;
        continue;
      }

      bool read_in = fctx.param_read_entry[i];
      bool restored_on_exit = false;
      RegStatus exit_status = RegStatus::kInitial;
      if (merged_exit.reachable && merged_exit.syms.count(canon_p)) {
        const SymRegState& es = merged_exit.syms.at(canon_p);
        exit_status = es.status;
        restored_on_exit = es.holds_entry_value;
      }

      if (restored_on_exit ||
          (!fctx.param_written_any[i] && !fctx.param_clobbered_any[i])) {
        inf.param_roles[i] = ParamRole::kIn;
        if (restored_on_exit && inf.clobbers_caller_saved && i < 32) {
          inf.preserved_across_call_param_mask |= (1u << i);
        }
      } else if (read_in) {
        inf.param_roles[i] = (exit_status == RegStatus::kClobbered)
                                 ? ParamRole::kInClobber
                                 : ParamRole::kInOut;
      } else {
        inf.param_roles[i] = (exit_status == RegStatus::kClobbered)
                                 ? ParamRole::kScratch
                                 : ParamRole::kOut;
      }
    }

    if (merged_exit.reachable) {
      for (int r = 0; r < arch_.num_gprs; ++r) {
        if (arch_.pinned_mask & (1u << r)) continue;
        if (fctx.used_baseline_scratch_scope &&
            (arch_.baseline_scratch_mask & (1u << r))) {
          continue;
        }
        if (fctx.used_arm64_scratch_scope &&
            (arch_.arm64_default_tmp_mask & (1u << r))) {
          continue;
        }
        const PhysRegState& ps = merged_exit.phys[r];
        if (!ps.holds_entry_value &&
            ((fctx.phys_written_any | fctx.phys_clobbered_any) & (1u << r))) {
          if (ps.status == RegStatus::kClobbered) {
            inf.clobbers |= (1u << r);
          } else if (ps.status == RegStatus::kAlive) {
            inf.implicit_out |= (1u << r);
          }
        }
      }
    }
    for (const ConditionalClobber& cc : fctx.propagated_conditional_clobbers) {
      RegMask rem = cc.mask & ~(inf.clobbers | inf.implicit_out);
      if (rem == 0) continue;
      ConditionalClobber ncc{cc.param_index, cc.op, cc.rhs, rem};
      if (std::find(inf.conditional_clobbers.begin(),
                    inf.conditional_clobbers.end(),
                    ncc) == inf.conditional_clobbers.end()) {
        inf.conditional_clobbers.push_back(ncc);
      }
    }
    return inf;
  }

  void VerifyAnnotations(const clang::FunctionDecl* fn, const FuncSummary& exp,
                         const FuncSummary& inf) {
    if (!exp.invalid_cond_clobber_error.empty()) {
      EmitImmediateWarning(fn->getLocation(), exp.invalid_cond_clobber_error);
    }
    const clang::CXXMethodDecl* method =
        llvm::dyn_cast<clang::CXXMethodDecl>(fn);
    bool verify_unannotated =
        method && ShouldVerifyUnannotatedRecord(method->getParent());
    if (!verify_unannotated && !exp.has_explicit_annotations) {
      return;
    }

    bool body_is_trivial = (inf.clobbers == 0 && inf.implicit_out == 0 &&
                            !inf.clobbers_caller_saved && !inf.no_fallthrough);
    if (body_is_trivial) {
      bool any_non_in = false;
      for (unsigned i = 0; i < fn->getNumParams(); ++i) {
        if (inf.param_roles[i] != ParamRole::kNone &&
            inf.param_roles[i] != ParamRole::kIn) {
          any_non_in = true;
        }
      }
      // Check if body has zero calls at all (e.g. NOOP_UNLESS_DEBUG_CODE or
      // `#ifdef` stubbed out):
      bool has_any_call = false;
      if (const clang::Stmt* body = fn->getBody()) {
        class AnyCallFinder : public clang::RecursiveASTVisitor<AnyCallFinder> {
         public:
          bool found = false;
          bool VisitCallExpr(const clang::CallExpr*) {
            found = true;
            return false;
          }
        } cf;
        cf.TraverseStmt(const_cast<clang::Stmt*>(body));
        has_any_call = cf.found;
      }
      if (!any_non_in && !has_any_call) return;
    }

    std::string fn_name = fn->getQualifiedNameAsString();

    for (unsigned i = 0; i < fn->getNumParams(); ++i) {
      const clang::ParmVarDecl* p = fn->getParamDecl(i);
      if (!IsCoreGprRegType(p->getType())) continue;

      ParamRole e_role = exp.param_roles[i];
      ParamRole i_role = inf.param_roles[i];
      std::string p_name = p->getNameAsString();

      // Skip `CPURegister` overloads when instantiated/used purely for FP regs
      // if nothing wrote the GPR.
      if (e_role == ParamRole::kIn && i_role != ParamRole::kIn &&
          !exp.clobbers_caller_saved) {
        EmitImmediateWarning(
            p->getLocation(),
            "Parameter '" + p_name + "' of '" + fn_name +
                "' is modified/clobbered in body (inferred " +
                ParamRoleToString(i_role) +
                ") but is annotated as input-only (V8_REG_IN).");
      } else if ((e_role == ParamRole::kOut || e_role == ParamRole::kScratch) &&
                 (i_role == ParamRole::kInOut ||
                  i_role == ParamRole::kInClobber)) {
        EmitImmediateWarning(
            p->getLocation(),
            "Parameter '" + p_name + "' of '" + fn_name + "' is annotated " +
                ParamRoleToString(e_role) +
                " but its entry value is read before being written (inferred " +
                ParamRoleToString(i_role) + ").");
      } else if ((e_role == ParamRole::kOut || e_role == ParamRole::kInOut) &&
                 i_role == ParamRole::kIn &&
                 !(e_role == ParamRole::kInOut && exp.clobbers_caller_saved) &&
                 !IsTypeNamed(p->getType(), "CPURegister")) {
        EmitImmediateWarning(p->getLocation(),
                             "Parameter '" + p_name + "' of '" + fn_name +
                                 "' is annotated " + ParamRoleToString(e_role) +
                                 " but is never written in the method body.");
      }
    }

    if (inf.clobbers_caller_saved && !exp.clobbers_caller_saved &&
        !inf.no_fallthrough && !exp.no_fallthrough) {
      EmitImmediateWarning(
          fn->getLocation(),
          "Method '" + fn_name +
              "' clobbers caller-saved registers in its body but is missing "
              "V8_REG_CLOBBERS_CALLER_SAVED.");
    }

    RegMask exp_cond_mask = 0;
    for (const ConditionalClobber& cc : exp.conditional_clobbers) {
      exp_cond_mask |= cc.mask;
    }
    RegMask inf_cond_mask = 0;
    for (const ConditionalClobber& cc : inf.conditional_clobbers) {
      inf_cond_mask |= cc.mask;
    }
    RegMask unannotated_clobbers =
        (inf.clobbers | inf.implicit_out | inf_cond_mask) &
        ~(exp.clobbers | exp.implicit_out | exp_cond_mask);
    if (exp.clobbers_caller_saved) {
      unannotated_clobbers &= ~arch_.caller_saved_mask;
    }
    if (unannotated_clobbers != 0 && !inf.no_fallthrough &&
        !exp.no_fallthrough) {
      EmitImmediateWarning(
          fn->getLocation(),
          "Method '" + fn_name + "' modifies/clobbers physical register(s) {" +
              FormatRegMask(unannotated_clobbers, arch_) +
              "} without V8_REG_CLOBBERS / V8_REG_IMPLICIT_OUT annotation.");
    }
  }

  clang::ASTContext& ast_ctx_;
  clang::DiagnosticsEngine& diag_;
  clang::SourceManager& sm_;
  ArchInfo arch_;
  unsigned diag_warn_ = 0;
  unsigned diag_note_ = 0;

  std::map<const clang::FunctionDecl*, FuncSummary> summary_cache_;
  std::map<const clang::CXXRecordDecl*, MaglevNodeSpec> maglev_spec_cache_;
  std::map<std::pair<const clang::FunctionDecl*, unsigned>, bool>
      const_pred_cache_;
  std::map<const clang::FunctionDecl*, bool> constraint_fn_cache_;
  llvm::SmallPtrSet<const clang::FunctionDecl*, 8> active_constraint_fns_;
  std::map<std::string, FuncSummary> imported_summaries_;
  std::map<std::string, MaglevNodeSpec> imported_maglev_specs_;
  llvm::SmallPtrSet<const clang::FunctionDecl*, 16> active_inferences_;
  llvm::SmallPtrSet<const clang::FunctionDecl*, 32> analyzed_deferred_lambdas_;
  std::unique_ptr<clang::MangleContext> mangle_ctx_;
};

class RegmoleConsumer : public clang::ASTConsumer,
                        public clang::RecursiveASTVisitor<RegmoleConsumer> {
 public:
  RegmoleConsumer(clang::DiagnosticsEngine& d, clang::SourceManager& sm,
                  const std::vector<std::string>& args)
      : d_(d), sm_(sm) {
    g_export_summaries_path.clear();
    g_import_summaries_path.clear();
    for (const std::string& arg : args) {
      if (arg == "--no-verify-annotations") g_verify_annotations = false;
      if (arg == "--verify-annotations") g_verify_annotations = true;
      if (arg == "--no-check-uses") g_check_uses = false;
      if (arg == "--check-uses") g_check_uses = true;
      if (arg == "--no-check-maglev") g_check_maglev = false;
      if (arg == "--check-maglev") g_check_maglev = true;
      if (arg == "--check-headers") g_check_headers = true;
      if (arg == "--arch=x64") g_arch_override = ArchKind::kX64;
      if (arg == "--arch=arm64") g_arch_override = ArchKind::kArm64;
      if (arg.rfind("--export-summaries=", 0) == 0) {
        g_export_summaries_path = arg.substr(19);
      }
      if (arg.rfind("--import-summaries=", 0) == 0) {
        g_import_summaries_path = arg.substr(19);
      }
    }
  }

  void HandleTranslationUnit(clang::ASTContext& ctx) override {
    ArchInfo arch = DetectArchFromAST(ctx, g_arch_override);
    analyzer_ = std::make_unique<RegmoleAnalyzer>(ctx, d_, sm_, arch);
    if (!g_import_summaries_path.empty()) {
      analyzer_->ImportSummaries(g_import_summaries_path);
    }
    pre_collecting_ = true;
    TraverseDecl(ctx.getTranslationUnitDecl());
    pre_collecting_ = false;
    TraverseDecl(ctx.getTranslationUnitDecl());
    if (!g_export_summaries_path.empty()) {
      analyzer_->ExportSummaries(g_export_summaries_path);
    }
  }

  bool VisitFunctionDecl(clang::FunctionDecl* decl) {
    if (!decl->doesThisDeclarationHaveABody()) return true;
    clang::SourceLocation loc = decl->getLocation();
    if (!loc.isValid()) return true;

    if (!g_check_headers && sm_.getFileID(loc) != sm_.getMainFileID()) {
      return true;
    }
    if (pre_collecting_) {
      analyzer_->PreCollectMaglevConstraints(decl);
      return true;
    }
    analyzer_->AnalyzeTopLevelFunction(decl);
    return true;
  }

 private:
  clang::DiagnosticsEngine& d_;
  clang::SourceManager& sm_;
  std::unique_ptr<RegmoleAnalyzer> analyzer_;
  bool pre_collecting_ = false;
};

class RegmoleAction : public clang::PluginASTAction {
 protected:
  std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
      clang::CompilerInstance& CI, llvm::StringRef InFile) override {
    (void)InFile;
    return std::make_unique<RegmoleConsumer>(CI.getDiagnostics(),
                                             CI.getSourceManager(), args_);
  }

  bool ParseArgs(const clang::CompilerInstance& CI,
                 const std::vector<std::string>& args) override {
    (void)CI;
    args_ = args;
    return true;
  }

 private:
  std::vector<std::string> args_;
};

}  // namespace

static clang::FrontendPluginRegistry::Add<RegmoleAction> RegisterRegmole(
    "regmole", "Static register clobber and control-flow analyzer for V8");
