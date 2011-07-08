// Copyright 2011 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// This is clang plugin used by gcmole tool. See README for more details.

#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/Mangle.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/StmtVisitor.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/Frontend/CompilerInstance.h"
#include "llvm/Support/raw_ostream.h"

#include <bitset>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <stack>

namespace {

typedef std::string MangledName;
typedef std::set<MangledName> CalleesSet;

static bool GetMangledName(clang::MangleContext* ctx,
                           const clang::NamedDecl* decl,
                           MangledName* result) {
  if (!isa<clang::CXXConstructorDecl>(decl) &&
      !isa<clang::CXXDestructorDecl>(decl)) {
    llvm::SmallVector<char, 512> output;
    llvm::raw_svector_ostream out(output);
    ctx->mangleName(decl, out);
    *result = out.str().str();
    return true;
  }

  return false;
}


static bool InV8Namespace(const clang::NamedDecl* decl) {
  return decl->getQualifiedNameAsString().compare(0, 4, "v8::") == 0;
}


struct Resolver {
  explicit Resolver(clang::ASTContext& ctx)
      : ctx_(ctx), decl_ctx_(ctx.getTranslationUnitDecl()) {
  }

  Resolver(clang::ASTContext& ctx, clang::DeclContext* decl_ctx)
      : ctx_(ctx), decl_ctx_(decl_ctx) {
  }

  clang::DeclarationName ResolveName(const char* n) {
    clang::IdentifierInfo* ident = &ctx_.Idents.get(n);
    return ctx_.DeclarationNames.getIdentifier(ident);
  }

  Resolver ResolveNamespace(const char* n) {
    return Resolver(ctx_, Resolve<clang::NamespaceDecl>(n));
  }

  template<typename T>
  T* Resolve(const char* n) {
    if (decl_ctx_ == NULL) return NULL;

    clang::DeclContext::lookup_result result =
        decl_ctx_->lookup(ResolveName(n));

    clang::DeclContext::lookup_iterator end = result.second;
    for (clang::DeclContext::lookup_iterator i = result.first;
         i != end;
         i++) {
      if (isa<T>(*i)) return cast<T>(*i);
    }

    return NULL;
  }

 private:
  clang::ASTContext& ctx_;
  clang::DeclContext* decl_ctx_;
};


class CalleesPrinter : public clang::RecursiveASTVisitor<CalleesPrinter> {
 public:
  explicit CalleesPrinter(clang::MangleContext* ctx) : ctx_(ctx) {
  }

  virtual bool VisitCallExpr(clang::CallExpr* expr) {
    const clang::FunctionDecl* callee = expr->getDirectCallee();
    if (callee != NULL) AnalyzeFunction(callee);
    return true;
  }

  void AnalyzeFunction(const clang::FunctionDecl* f) {
    MangledName name;
    if (InV8Namespace(f) && GetMangledName(ctx_, f, &name)) {
      AddCallee(name);

      const clang::FunctionDecl* body = NULL;
      if (f->hasBody(body) && !Analyzed(name)) {
        EnterScope(name);
        TraverseStmt(body->getBody());
        LeaveScope();
      }
    }
  }

  typedef std::map<MangledName, CalleesSet* > Callgraph;

  bool Analyzed(const MangledName& name) {
    return callgraph_[name] != NULL;
  }

  void EnterScope(const MangledName& name) {
    CalleesSet* callees = callgraph_[name];

    if (callees == NULL) {
      callgraph_[name] = callees = new CalleesSet();
    }

    scopes_.push(callees);
  }

  void LeaveScope() {
    scopes_.pop();
  }

  void AddCallee(const MangledName& name) {
    if (!scopes_.empty()) scopes_.top()->insert(name);
  }

  void PrintCallGraph() {
    for (Callgraph::const_iterator i = callgraph_.begin(), e = callgraph_.end();
         i != e;
         ++i) {
      std::cout << i->first << "\n";

      CalleesSet* callees = i->second;
      for (CalleesSet::const_iterator j = callees->begin(), e = callees->end();
           j != e;
           ++j) {
        std::cout << "\t" << *j << "\n";
      }
    }
  }

 private:
  clang::MangleContext* ctx_;

  std::stack<CalleesSet* > scopes_;
  Callgraph callgraph_;
};


class FunctionDeclarationFinder
    : public clang::ASTConsumer,
      public clang::RecursiveASTVisitor<FunctionDeclarationFinder> {
 public:
  explicit FunctionDeclarationFinder(clang::Diagnostic& d,
                                     clang::SourceManager& sm,
                                     const std::vector<std::string>& args)
      : d_(d), sm_(sm) { }

  virtual void HandleTranslationUnit(clang::ASTContext &ctx) {
    mangle_context_ = clang::createItaniumMangleContext(ctx, d_);
    callees_printer_ = new CalleesPrinter(mangle_context_);

    TraverseDecl(ctx.getTranslationUnitDecl());

    callees_printer_->PrintCallGraph();
  }

  virtual bool VisitFunctionDecl(clang::FunctionDecl* decl) {
    callees_printer_->AnalyzeFunction(decl);
    return true;
  }

 private:
  clang::Diagnostic& d_;
  clang::SourceManager& sm_;
  clang::MangleContext* mangle_context_;

  CalleesPrinter* callees_printer_;
};


static bool loaded = false;
static CalleesSet gc_suspects;


static void LoadGCSuspects() {
  if (loaded) return;

  std::ifstream fin("gcsuspects");
  std::string s;

  while (fin >> s) gc_suspects.insert(s);

  loaded = true;
}


static bool KnownToCauseGC(clang::MangleContext* ctx,
                           const clang::FunctionDecl* decl) {
  LoadGCSuspects();

  if (!InV8Namespace(decl)) return false;

  MangledName name;
  if (GetMangledName(ctx, decl, &name)) {
    return gc_suspects.find(name) != gc_suspects.end();
  }

  return false;
}


static const int kNoEffect = 0;
static const int kCausesGC = 1;
static const int kRawDef = 2;
static const int kRawUse = 4;
static const int kAllEffects = kCausesGC | kRawDef | kRawUse;

class Environment;

class ExprEffect {
 public:
  bool hasGC() { return (effect_ & kCausesGC) != 0; }
  void setGC() { effect_ |= kCausesGC; }

  bool hasRawDef() { return (effect_ & kRawDef) != 0; }
  void setRawDef() { effect_ |= kRawDef; }

  bool hasRawUse() { return (effect_ & kRawUse) != 0; }
  void setRawUse() { effect_ |= kRawUse; }

  static ExprEffect None() { return ExprEffect(kNoEffect, NULL); }
  static ExprEffect NoneWithEnv(Environment* env) {
    return ExprEffect(kNoEffect, env);
  }
  static ExprEffect RawUse() { return ExprEffect(kRawUse, NULL); }

  static ExprEffect Merge(ExprEffect a, ExprEffect b);
  static ExprEffect MergeSeq(ExprEffect a, ExprEffect b);
  ExprEffect Define(const std::string& name);

  Environment* env() {
    return reinterpret_cast<Environment*>(effect_ & ~kAllEffects);
  }

 private:
  ExprEffect(int effect, Environment* env)
      : effect_((effect & kAllEffects) |
                reinterpret_cast<intptr_t>(env)) { }

  intptr_t effect_;
};


const std::string BAD_EXPR_MSG("Possible problem with evaluation order.");
const std::string DEAD_VAR_MSG("Possibly dead variable.");


class Environment {
 public:
  Environment() { }

  static Environment Unreachable() {
    Environment env;
    env.live_.set();
    return env;
  }

  static Environment Merge(const Environment& l,
                           const Environment& r) {
    return Environment(l, r);
  }

  Environment ApplyEffect(ExprEffect effect) const {
    Environment out = effect.hasGC() ? Environment() : Environment(*this);
    if (effect.env() != NULL) out.live_ |= effect.env()->live_;
    return out;
  }

  typedef std::map<std::string, int> SymbolTable;

  bool IsAlive(const std::string& name) const {
    SymbolTable::iterator code = symbol_table_.find(name);
    if (code == symbol_table_.end()) return false;
    return live_[code->second];
  }

  bool Equal(const Environment& env) {
    return live_ == env.live_;
  }

  Environment Define(const std::string& name) const {
    return Environment(*this, SymbolToCode(name));
  }

  void MDefine(const std::string& name) {
    live_.set(SymbolToCode(name));
  }

  static int SymbolToCode(const std::string& name) {
    SymbolTable::iterator code = symbol_table_.find(name);

    if (code == symbol_table_.end()) {
      int new_code = symbol_table_.size();
      symbol_table_.insert(std::make_pair(name, new_code));
      return new_code;
    }

    return code->second;
  }

  static void ClearSymbolTable() {
    std::vector<Environment*>::iterator end = envs_.end();
    for (std::vector<Environment*>::iterator i = envs_.begin();
         i != end;
         ++i) {
      delete *i;
    }
    envs_.clear();
    symbol_table_.clear();
  }

  void Print() const {
    bool comma = false;
    std::cout << "{";
    SymbolTable::iterator end = symbol_table_.end();
    for (SymbolTable::iterator i = symbol_table_.begin();
         i != end;
         ++i) {
      if (live_[i->second]) {
        if (comma) std::cout << ", ";
        std::cout << i->first;
        comma = true;
      }
    }
    std::cout << "}";
  }

  static Environment* Allocate(const Environment& env) {
    Environment* allocated_env = new Environment(env);
    envs_.push_back(allocated_env);
    return allocated_env;
  }

 private:
  Environment(const Environment& l, const Environment& r)
      : live_(l.live_ & r.live_) {
  }

  Environment(const Environment& l, int code)
      : live_(l.live_) {
    live_.set(code);
  }

  static SymbolTable symbol_table_;
  static std::vector<Environment* > envs_;

  static const int kMaxNumberOfLocals = 256;
  std::bitset<kMaxNumberOfLocals> live_;

  friend class ExprEffect;
  friend class CallProps;
};


class CallProps {
 public:
  CallProps() : env_(NULL) { }

  void SetEffect(int arg, ExprEffect in) {
    if (in.hasGC()) gc_.set(arg);
    if (in.hasRawDef()) raw_def_.set(arg);
    if (in.hasRawUse()) raw_use_.set(arg);
    if (in.env() != NULL) {
      if (env_ == NULL) env_ = in.env();
      env_->live_ |= in.env()->live_;
    }
  }

  ExprEffect ComputeCumulativeEffect(bool result_is_raw) {
    ExprEffect out = ExprEffect::NoneWithEnv(env_);
    if (gc_.any()) out.setGC();
    if (raw_use_.any()) out.setRawUse();
    if (result_is_raw) out.setRawDef();
    return out;
  }

  bool IsSafe() {
    if (!gc_.any()) return true;
    std::bitset<kMaxNumberOfArguments> raw = (raw_def_ | raw_use_);
    if (!raw.any()) return true;
    return gc_.count() == 1 && !((raw ^ gc_).any());
  }

 private:
  static const int kMaxNumberOfArguments = 64;
  std::bitset<kMaxNumberOfArguments> raw_def_;
  std::bitset<kMaxNumberOfArguments> raw_use_;
  std::bitset<kMaxNumberOfArguments> gc_;
  Environment* env_;
};


Environment::SymbolTable Environment::symbol_table_;
std::vector<Environment* > Environment::envs_;


ExprEffect ExprEffect::Merge(ExprEffect a, ExprEffect b) {
  Environment* a_env = a.env();
  Environment* b_env = b.env();
  Environment* out = NULL;
  if (a_env != NULL && b_env != NULL) {
    out = Environment::Allocate(*a_env);
    out->live_ &= b_env->live_;
  }
  return ExprEffect(a.effect_ | b.effect_, out);
}


ExprEffect ExprEffect::MergeSeq(ExprEffect a, ExprEffect b) {
  Environment* a_env = b.hasGC() ? NULL : a.env();
  Environment* b_env = b.env();
  Environment* out = (b_env == NULL) ? a_env : b_env;
  if (a_env != NULL && b_env != NULL) {
    out = Environment::Allocate(*b_env);
    out->live_ |= a_env->live_;
  }
  return ExprEffect(a.effect_ | b.effect_, out);
}


ExprEffect ExprEffect::Define(const std::string& name) {
  Environment* e = env();
  if (e == NULL) {
    e = Environment::Allocate(Environment());
  }
  e->MDefine(name);
  return ExprEffect(effect_, e);
}


static std::string THIS ("this");


class FunctionAnalyzer {
 public:
  FunctionAnalyzer(clang::MangleContext* ctx,
                   clang::DeclarationName handle_decl_name,
                   clang::CXXRecordDecl* object_decl,
                   clang::CXXRecordDecl* smi_decl,
                   clang::Diagnostic& d,
                   clang::SourceManager& sm,
                   bool dead_vars_analysis)
      : ctx_(ctx),
        handle_decl_name_(handle_decl_name),
        object_decl_(object_decl),
        smi_decl_(smi_decl),
        d_(d),
        sm_(sm),
        block_(NULL),
        dead_vars_analysis_(dead_vars_analysis) {
  }


  // --------------------------------------------------------------------------
  // Expressions
  // --------------------------------------------------------------------------

  ExprEffect VisitExpr(clang::Expr* expr, const Environment& env) {
#define VISIT(type) do {                                                \
      clang::type* concrete_expr = dyn_cast_or_null<clang::type>(expr); \
      if (concrete_expr != NULL) {                                      \
        return Visit##type (concrete_expr, env);                        \
      }                                                                 \
    } while(0);

    VISIT(AbstractConditionalOperator);
    VISIT(AddrLabelExpr);
    VISIT(ArraySubscriptExpr);
    VISIT(BinaryOperator);
    VISIT(BinaryTypeTraitExpr);
    VISIT(BlockDeclRefExpr);
    VISIT(BlockExpr);
    VISIT(CallExpr);
    VISIT(CastExpr);
    VISIT(CharacterLiteral);
    VISIT(ChooseExpr);
    VISIT(CompoundLiteralExpr);
    VISIT(CXXBindTemporaryExpr);
    VISIT(CXXBoolLiteralExpr);
    VISIT(CXXConstructExpr);
    VISIT(CXXDefaultArgExpr);
    VISIT(CXXDeleteExpr);
    VISIT(CXXDependentScopeMemberExpr);
    VISIT(CXXNewExpr);
    VISIT(CXXNoexceptExpr);
    VISIT(CXXNullPtrLiteralExpr);
    VISIT(CXXPseudoDestructorExpr);
    VISIT(CXXScalarValueInitExpr);
    VISIT(CXXThisExpr);
    VISIT(CXXThrowExpr);
    VISIT(CXXTypeidExpr);
    VISIT(CXXUnresolvedConstructExpr);
    VISIT(CXXUuidofExpr);
    VISIT(DeclRefExpr);
    VISIT(DependentScopeDeclRefExpr);
    VISIT(DesignatedInitExpr);
    VISIT(ExprWithCleanups);
    VISIT(ExtVectorElementExpr);
    VISIT(FloatingLiteral);
    VISIT(GNUNullExpr);
    VISIT(ImaginaryLiteral);
    VISIT(ImplicitValueInitExpr);
    VISIT(InitListExpr);
    VISIT(IntegerLiteral);
    VISIT(MemberExpr);
    VISIT(OffsetOfExpr);
    VISIT(OpaqueValueExpr);
    VISIT(OverloadExpr);
    VISIT(PackExpansionExpr);
    VISIT(ParenExpr);
    VISIT(ParenListExpr);
    VISIT(PredefinedExpr);
    VISIT(ShuffleVectorExpr);
    VISIT(SizeOfPackExpr);
    VISIT(StmtExpr);
    VISIT(StringLiteral);
    VISIT(SubstNonTypeTemplateParmPackExpr);
    VISIT(UnaryExprOrTypeTraitExpr);
    VISIT(UnaryOperator);
    VISIT(UnaryTypeTraitExpr);
    VISIT(VAArgExpr);
#undef VISIT

    return ExprEffect::None();
  }

#define DECL_VISIT_EXPR(type)                                           \
  ExprEffect Visit##type (clang::type* expr, const Environment& env)

#define IGNORE_EXPR(type)                                               \
  ExprEffect Visit##type (clang::type* expr, const Environment& env) {  \
    return ExprEffect::None();                                          \
  }

  IGNORE_EXPR(AddrLabelExpr);
  IGNORE_EXPR(BinaryTypeTraitExpr);
  IGNORE_EXPR(BlockExpr);
  IGNORE_EXPR(CharacterLiteral);
  IGNORE_EXPR(ChooseExpr);
  IGNORE_EXPR(CompoundLiteralExpr);
  IGNORE_EXPR(CXXBoolLiteralExpr);
  IGNORE_EXPR(CXXDependentScopeMemberExpr);
  IGNORE_EXPR(CXXNullPtrLiteralExpr);
  IGNORE_EXPR(CXXPseudoDestructorExpr);
  IGNORE_EXPR(CXXScalarValueInitExpr);
  IGNORE_EXPR(CXXNoexceptExpr);
  IGNORE_EXPR(CXXTypeidExpr);
  IGNORE_EXPR(CXXUnresolvedConstructExpr);
  IGNORE_EXPR(CXXUuidofExpr);
  IGNORE_EXPR(DependentScopeDeclRefExpr);
  IGNORE_EXPR(DesignatedInitExpr);
  IGNORE_EXPR(ExtVectorElementExpr);
  IGNORE_EXPR(FloatingLiteral);
  IGNORE_EXPR(ImaginaryLiteral);
  IGNORE_EXPR(IntegerLiteral);
  IGNORE_EXPR(OffsetOfExpr);
  IGNORE_EXPR(ImplicitValueInitExpr);
  IGNORE_EXPR(PackExpansionExpr);
  IGNORE_EXPR(PredefinedExpr);
  IGNORE_EXPR(ShuffleVectorExpr);
  IGNORE_EXPR(SizeOfPackExpr);
  IGNORE_EXPR(StmtExpr);
  IGNORE_EXPR(StringLiteral);
  IGNORE_EXPR(SubstNonTypeTemplateParmPackExpr);
  IGNORE_EXPR(UnaryExprOrTypeTraitExpr);
  IGNORE_EXPR(UnaryTypeTraitExpr);
  IGNORE_EXPR(VAArgExpr);
  IGNORE_EXPR(GNUNullExpr);
  IGNORE_EXPR(OverloadExpr);

  DECL_VISIT_EXPR(CXXThisExpr) {
    return Use(expr, expr->getType(), THIS, env);
  }

  DECL_VISIT_EXPR(AbstractConditionalOperator) {
    Environment after_cond = env.ApplyEffect(VisitExpr(expr->getCond(), env));
    return ExprEffect::Merge(VisitExpr(expr->getTrueExpr(), after_cond),
                             VisitExpr(expr->getFalseExpr(), after_cond));
  }

  DECL_VISIT_EXPR(ArraySubscriptExpr) {
    clang::Expr* exprs[2] = {expr->getBase(), expr->getIdx()};
    return Par(expr, 2, exprs, env);
  }

  bool IsRawPointerVar(clang::Expr* expr, std::string* var_name) {
    if (isa<clang::BlockDeclRefExpr>(expr)) {
      *var_name = cast<clang::BlockDeclRefExpr>(expr)->getDecl()->
          getNameAsString();
      return true;
    } else if (isa<clang::DeclRefExpr>(expr)) {
      *var_name = cast<clang::DeclRefExpr>(expr)->getDecl()->getNameAsString();
      return true;
    }
    return false;
  }

  DECL_VISIT_EXPR(BinaryOperator) {
    clang::Expr* lhs = expr->getLHS();
    clang::Expr* rhs = expr->getRHS();
    clang::Expr* exprs[2] = {lhs, rhs};

    switch (expr->getOpcode()) {
      case clang::BO_Comma:
        return Seq(expr, 2, exprs, env);

      case clang::BO_LAnd:
      case clang::BO_LOr:
        return ExprEffect::Merge(VisitExpr(lhs, env), VisitExpr(rhs, env));

      case clang::BO_Assign: {
        std::string var_name;
        if (IsRawPointerVar(lhs, &var_name)) {
          return VisitExpr(rhs, env).Define(var_name);
        }
        return Par(expr, 2, exprs, env);
      }

      default:
        return Par(expr, 2, exprs, env);
    }
  }

  DECL_VISIT_EXPR(CXXBindTemporaryExpr) {
    return VisitExpr(expr->getSubExpr(), env);
  }

  DECL_VISIT_EXPR(CXXConstructExpr) {
    return VisitArguments<>(expr, env);
  }

  DECL_VISIT_EXPR(CXXDefaultArgExpr) {
    return VisitExpr(expr->getExpr(), env);
  }

  DECL_VISIT_EXPR(CXXDeleteExpr) {
    return VisitExpr(expr->getArgument(), env);
  }

  DECL_VISIT_EXPR(CXXNewExpr) {
    return Par(expr,
               expr->getNumConstructorArgs(),
               expr->getConstructorArgs(),
               env);
  }

  DECL_VISIT_EXPR(ExprWithCleanups) {
    return VisitExpr(expr->getSubExpr(), env);
  }

  DECL_VISIT_EXPR(CXXThrowExpr) {
    return VisitExpr(expr->getSubExpr(), env);
  }

  DECL_VISIT_EXPR(InitListExpr) {
    return Seq(expr, expr->getNumInits(), expr->getInits(), env);
  }

  DECL_VISIT_EXPR(MemberExpr) {
    return VisitExpr(expr->getBase(), env);
  }

  DECL_VISIT_EXPR(OpaqueValueExpr) {
    return VisitExpr(expr->getSourceExpr(), env);
  }

  DECL_VISIT_EXPR(ParenExpr) {
    return VisitExpr(expr->getSubExpr(), env);
  }

  DECL_VISIT_EXPR(ParenListExpr) {
    return Par(expr, expr->getNumExprs(), expr->getExprs(), env);
  }

  DECL_VISIT_EXPR(UnaryOperator) {
    // TODO We are treating all expressions that look like &raw_pointer_var
    //      as definitions of raw_pointer_var. This should be changed to
    //      recognize less generic pattern:
    //
    //         if (maybe_object->ToObject(&obj)) return maybe_object;
    //
    if (expr->getOpcode() == clang::UO_AddrOf) {
      std::string var_name;
      if (IsRawPointerVar(expr->getSubExpr(), &var_name)) {
        return ExprEffect::None().Define(var_name);
      }
    }
    return VisitExpr(expr->getSubExpr(), env);
  }

  DECL_VISIT_EXPR(CastExpr) {
    return VisitExpr(expr->getSubExpr(), env);
  }

  DECL_VISIT_EXPR(DeclRefExpr) {
    return Use(expr, expr->getDecl(), env);
  }

  DECL_VISIT_EXPR(BlockDeclRefExpr) {
    return Use(expr, expr->getDecl(), env);
  }

  ExprEffect Par(clang::Expr* parent,
                 int n,
                 clang::Expr** exprs,
                 const Environment& env) {
    CallProps props;

    for (int i = 0; i < n; ++i) {
      props.SetEffect(i, VisitExpr(exprs[i], env));
    }

    if (!props.IsSafe()) ReportUnsafe(parent, BAD_EXPR_MSG);

    return props.ComputeCumulativeEffect(IsRawPointerType(parent->getType()));
  }

  ExprEffect Seq(clang::Stmt* parent,
                 int n,
                 clang::Expr** exprs,
                 const Environment& env) {
    ExprEffect out = ExprEffect::None();
    Environment out_env = env;
    for (int i = 0; i < n; ++i) {
      out = ExprEffect::MergeSeq(out, VisitExpr(exprs[i], out_env));
      out_env = out_env.ApplyEffect(out);
    }
    return out;
  }

  ExprEffect Use(const clang::Expr* parent,
                 const clang::QualType& var_type,
                 const std::string& var_name,
                 const Environment& env) {
    if (IsRawPointerType(var_type)) {
      if (!env.IsAlive(var_name) && dead_vars_analysis_) {
        ReportUnsafe(parent, DEAD_VAR_MSG);
      }
      return ExprEffect::RawUse();
    }
    return ExprEffect::None();
  }

  ExprEffect Use(const clang::Expr* parent,
                 const clang::ValueDecl* var,
                 const Environment& env) {
    return Use(parent, var->getType(), var->getNameAsString(), env);
  }


  template<typename ExprType>
  ExprEffect VisitArguments(ExprType* call, const Environment& env) {
    CallProps props;
    VisitArguments<>(call, &props, env);
    if (!props.IsSafe()) ReportUnsafe(call, BAD_EXPR_MSG);
    return props.ComputeCumulativeEffect(IsRawPointerType(call->getType()));
  }

  template<typename ExprType>
  void VisitArguments(ExprType* call,
                      CallProps* props,
                      const Environment& env) {
    for (unsigned arg = 0; arg < call->getNumArgs(); arg++) {
      props->SetEffect(arg + 1, VisitExpr(call->getArg(arg), env));
    }
  }


  ExprEffect VisitCallExpr(clang::CallExpr* call,
                           const Environment& env) {
    CallProps props;

    clang::CXXMemberCallExpr* memcall =
        dyn_cast_or_null<clang::CXXMemberCallExpr>(call);
    if (memcall != NULL) {
      clang::Expr* receiver = memcall->getImplicitObjectArgument();
      props.SetEffect(0, VisitExpr(receiver, env));
    }

    VisitArguments<>(call, &props, env);

    if (!props.IsSafe()) ReportUnsafe(call, BAD_EXPR_MSG);

    ExprEffect out =
        props.ComputeCumulativeEffect(IsRawPointerType(call->getType()));

    clang::FunctionDecl* callee = call->getDirectCallee();
    if ((callee != NULL) && KnownToCauseGC(ctx_, callee)) {
      out.setGC();
    }

    return out;
  }

  // --------------------------------------------------------------------------
  // Statements
  // --------------------------------------------------------------------------

  Environment VisitStmt(clang::Stmt* stmt, const Environment& env) {
#define VISIT(type) do {                                                \
      clang::type* concrete_stmt = dyn_cast_or_null<clang::type>(stmt); \
      if (concrete_stmt != NULL) {                                      \
        return Visit##type (concrete_stmt, env);                        \
      }                                                                 \
    } while(0);

    if (clang::Expr* expr = dyn_cast_or_null<clang::Expr>(stmt)) {
      return env.ApplyEffect(VisitExpr(expr, env));
    }

    VISIT(AsmStmt);
    VISIT(BreakStmt);
    VISIT(CompoundStmt);
    VISIT(ContinueStmt);
    VISIT(CXXCatchStmt);
    VISIT(CXXTryStmt);
    VISIT(DeclStmt);
    VISIT(DoStmt);
    VISIT(ForStmt);
    VISIT(GotoStmt);
    VISIT(IfStmt);
    VISIT(IndirectGotoStmt);
    VISIT(LabelStmt);
    VISIT(NullStmt);
    VISIT(ReturnStmt);
    VISIT(CaseStmt);
    VISIT(DefaultStmt);
    VISIT(SwitchStmt);
    VISIT(WhileStmt);
#undef VISIT

    return env;
  }

#define DECL_VISIT_STMT(type)                                           \
  Environment Visit##type (clang::type* stmt, const Environment& env)

#define IGNORE_STMT(type)                                               \
  Environment Visit##type (clang::type* stmt, const Environment& env) { \
    return env;                                                         \
  }

  IGNORE_STMT(IndirectGotoStmt);
  IGNORE_STMT(NullStmt);
  IGNORE_STMT(AsmStmt);

  // We are ignoring control flow for simplicity.
  IGNORE_STMT(GotoStmt);
  IGNORE_STMT(LabelStmt);

  // We are ignoring try/catch because V8 does not use them.
  IGNORE_STMT(CXXCatchStmt);
  IGNORE_STMT(CXXTryStmt);

  class Block {
   public:
    Block(const Environment& in,
          FunctionAnalyzer* owner)
        : in_(in),
          out_(Environment::Unreachable()),
          changed_(false),
          owner_(owner) {
      parent_ = owner_->EnterBlock(this);
    }

    ~Block() {
      owner_->LeaveBlock(parent_);
    }

    void MergeIn(const Environment& env) {
      Environment old_in = in_;
      in_ = Environment::Merge(in_, env);
      changed_ = !old_in.Equal(in_);
    }

    bool changed() {
      if (changed_) {
        changed_ = false;
        return true;
      }
      return false;
    }

    const Environment& in() {
      return in_;
    }

    const Environment& out() {
      return out_;
    }

    void MergeOut(const Environment& env) {
      out_ = Environment::Merge(out_, env);
    }

    void Seq(clang::Stmt* a, clang::Stmt* b, clang::Stmt* c) {
      Environment a_out = owner_->VisitStmt(a, in());
      Environment b_out = owner_->VisitStmt(b, a_out);
      Environment c_out = owner_->VisitStmt(c, b_out);
      MergeOut(c_out);
    }

    void Seq(clang::Stmt* a, clang::Stmt* b) {
      Environment a_out = owner_->VisitStmt(a, in());
      Environment b_out = owner_->VisitStmt(b, a_out);
      MergeOut(b_out);
    }

    void Loop(clang::Stmt* a, clang::Stmt* b, clang::Stmt* c) {
      Seq(a, b, c);
      MergeIn(out());
    }

    void Loop(clang::Stmt* a, clang::Stmt* b) {
      Seq(a, b);
      MergeIn(out());
    }


   private:
    Environment in_;
    Environment out_;
    bool changed_;
    FunctionAnalyzer* owner_;
    Block* parent_;
  };


  DECL_VISIT_STMT(BreakStmt) {
    block_->MergeOut(env);
    return Environment::Unreachable();
  }

  DECL_VISIT_STMT(ContinueStmt) {
    block_->MergeIn(env);
    return Environment::Unreachable();
  }

  DECL_VISIT_STMT(CompoundStmt) {
    Environment out = env;
    clang::CompoundStmt::body_iterator end = stmt->body_end();
    for (clang::CompoundStmt::body_iterator s = stmt->body_begin();
         s != end;
         ++s) {
      out = VisitStmt(*s, out);
    }
    return out;
  }

  DECL_VISIT_STMT(WhileStmt) {
    Block block (env, this);
    do {
      block.Loop(stmt->getCond(), stmt->getBody());
    } while (block.changed());
    return block.out();
  }

  DECL_VISIT_STMT(DoStmt) {
    Block block (env, this);
    do {
      block.Loop(stmt->getBody(), stmt->getCond());
    } while (block.changed());
    return block.out();
  }

  DECL_VISIT_STMT(ForStmt) {
    Block block (VisitStmt(stmt->getInit(), env), this);
    do {
      block.Loop(stmt->getCond(),
                 stmt->getBody(),
                 stmt->getInc());
    } while (block.changed());
    return block.out();
  }

  DECL_VISIT_STMT(IfStmt) {
    Environment cond_out = VisitStmt(stmt->getCond(), env);
    Environment then_out = VisitStmt(stmt->getThen(), cond_out);
    Environment else_out = VisitStmt(stmt->getElse(), cond_out);
    return Environment::Merge(then_out, else_out);
  }

  DECL_VISIT_STMT(SwitchStmt) {
    Block block (env, this);
    block.Seq(stmt->getCond(), stmt->getBody());
    return block.out();
  }

  DECL_VISIT_STMT(CaseStmt) {
    Environment in = Environment::Merge(env, block_->in());
    Environment after_lhs = VisitStmt(stmt->getLHS(), in);
    return VisitStmt(stmt->getSubStmt(), after_lhs);
  }

  DECL_VISIT_STMT(DefaultStmt) {
    Environment in = Environment::Merge(env, block_->in());
    return VisitStmt(stmt->getSubStmt(), in);
  }

  DECL_VISIT_STMT(ReturnStmt) {
    VisitExpr(stmt->getRetValue(), env);
    return Environment::Unreachable();
  }

  const clang::TagType* ToTagType(const clang::Type* t) {
    if (t == NULL) {
      return NULL;
    } else if (isa<clang::TagType>(t)) {
      return cast<clang::TagType>(t);
    } else if (isa<clang::SubstTemplateTypeParmType>(t)) {
      return ToTagType(cast<clang::SubstTemplateTypeParmType>(t)->
                           getReplacementType().getTypePtr());
    } else {
      return NULL;
    }
  }

  bool IsDerivedFrom(clang::CXXRecordDecl* record,
                     clang::CXXRecordDecl* base) {
    return (record == base) || record->isDerivedFrom(base);
  }

  bool IsRawPointerType(clang::QualType qtype) {
    const clang::PointerType* type =
        dyn_cast_or_null<clang::PointerType>(qtype.getTypePtrOrNull());
    if (type == NULL) return false;

    const clang::TagType* pointee =
        ToTagType(type->getPointeeType().getTypePtr());
    if (pointee == NULL) return false;

    clang::CXXRecordDecl* record =
        dyn_cast_or_null<clang::CXXRecordDecl>(pointee->getDecl());
    if (record == NULL) return false;

    if (!InV8Namespace(record)) return false;

    if (!record->hasDefinition()) return false;

    record = record->getDefinition();

    return IsDerivedFrom(record, object_decl_) &&
        !IsDerivedFrom(record, smi_decl_);
  }

  Environment VisitDecl(clang::Decl* decl, const Environment& env) {
    if (clang::VarDecl* var = dyn_cast<clang::VarDecl>(decl)) {
      Environment out = var->hasInit() ? VisitStmt(var->getInit(), env) : env;

      if (IsRawPointerType(var->getType())) {
        out = out.Define(var->getNameAsString());
      }

      return out;
    }
    // TODO: handle other declarations?
    return env;
  }

  DECL_VISIT_STMT(DeclStmt) {
    Environment out = env;
    clang::DeclStmt::decl_iterator end = stmt->decl_end();
    for (clang::DeclStmt::decl_iterator decl = stmt->decl_begin();
         decl != end;
         ++decl) {
      out = VisitDecl(*decl, out);
    }
    return out;
  }


  void DefineParameters(const clang::FunctionDecl* f,
                        Environment* env) {
    env->MDefine(THIS);
    clang::FunctionDecl::param_const_iterator end = f->param_end();
    for (clang::FunctionDecl::param_const_iterator p = f->param_begin();
         p != end;
         ++p) {
      env->MDefine((*p)->getNameAsString());
    }
  }


  void AnalyzeFunction(const clang::FunctionDecl* f) {
    const clang::FunctionDecl* body = NULL;
    if (f->hasBody(body)) {
      Environment env;
      DefineParameters(body, &env);
      VisitStmt(body->getBody(), env);
      Environment::ClearSymbolTable();
    }
  }

  Block* EnterBlock(Block* block) {
    Block* parent = block_;
    block_ = block;
    return parent;
  }

  void LeaveBlock(Block* block) {
    block_ = block;
  }

 private:
  void ReportUnsafe(const clang::Expr* expr, const std::string& msg) {
    d_.Report(clang::FullSourceLoc(expr->getExprLoc(), sm_),
              d_.getCustomDiagID(clang::Diagnostic::Warning, msg));
  }


  clang::MangleContext* ctx_;
  clang::DeclarationName handle_decl_name_;
  clang::CXXRecordDecl* object_decl_;
  clang::CXXRecordDecl* smi_decl_;

  clang::Diagnostic& d_;
  clang::SourceManager& sm_;

  Block* block_;
  bool dead_vars_analysis_;
};


class ProblemsFinder : public clang::ASTConsumer,
                       public clang::RecursiveASTVisitor<ProblemsFinder> {
 public:
  ProblemsFinder(clang::Diagnostic& d,
                 clang::SourceManager& sm,
                 const std::vector<std::string>& args)
      : d_(d), sm_(sm), dead_vars_analysis_(false) {
    for (unsigned i = 0; i < args.size(); ++i) {
      if (args[i] == "--dead-vars") {
        dead_vars_analysis_ = true;
      }
    }
  }

  virtual void HandleTranslationUnit(clang::ASTContext &ctx) {
    Resolver r(ctx);

    clang::CXXRecordDecl* object_decl =
        r.ResolveNamespace("v8").ResolveNamespace("internal").
            Resolve<clang::CXXRecordDecl>("Object");

    clang::CXXRecordDecl* smi_decl =
        r.ResolveNamespace("v8").ResolveNamespace("internal").
            Resolve<clang::CXXRecordDecl>("Smi");

    if (object_decl != NULL) object_decl = object_decl->getDefinition();

    if (smi_decl != NULL) smi_decl = smi_decl->getDefinition();

    if (object_decl != NULL && smi_decl != NULL) {
      function_analyzer_ =
          new FunctionAnalyzer(clang::createItaniumMangleContext(ctx, d_),
                               r.ResolveName("Handle"),
                               object_decl,
                               smi_decl,
                               d_,
                               sm_,
                               dead_vars_analysis_);
      TraverseDecl(ctx.getTranslationUnitDecl());
    } else {
      if (object_decl == NULL) {
        llvm::errs() << "Failed to resolve v8::internal::Object\n";
      }
      if (smi_decl == NULL) {
        llvm::errs() << "Failed to resolve v8::internal::Smi\n";
      }
    }
  }

  virtual bool VisitFunctionDecl(clang::FunctionDecl* decl) {
    function_analyzer_->AnalyzeFunction(decl);
    return true;
  }

 private:
  clang::Diagnostic& d_;
  clang::SourceManager& sm_;
  bool dead_vars_analysis_;

  FunctionAnalyzer* function_analyzer_;
};


template<typename ConsumerType>
class Action : public clang::PluginASTAction {
 protected:
  clang::ASTConsumer *CreateASTConsumer(clang::CompilerInstance &CI,
                                        llvm::StringRef InFile) {
    return new ConsumerType(CI.getDiagnostics(), CI.getSourceManager(), args_);
  }

  bool ParseArgs(const clang::CompilerInstance &CI,
                 const std::vector<std::string>& args) {
    args_ = args;
    return true;
  }

  void PrintHelp(llvm::raw_ostream& ros) {
  }
 private:
  std::vector<std::string> args_;
};


}

static clang::FrontendPluginRegistry::Add<Action<ProblemsFinder> >
FindProblems("find-problems", "Find GC-unsafe places.");

static clang::FrontendPluginRegistry::Add<
  Action<FunctionDeclarationFinder> >
DumpCallees("dump-callees", "Dump callees for each function.");
