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

#include <algorithm>
#include <bitset>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <stack>

#include "clang/AST/APValue.h"
#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/Mangle.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/StmtVisitor.h"
#include "clang/AST/TemplateBase.h"
#include "clang/Basic/FileManager.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/LineIterator.h"
#include "llvm/Support/raw_ostream.h"

namespace {

bool g_tracing_enabled = false;
bool g_dead_vars_analysis = false;
bool g_verbose = false;
bool g_print_gc_call_chain = false;
static std::string g_output_dir = ".";

#define TRACE(str)                   \
  do {                               \
    if (g_tracing_enabled) {         \
      std::cout << str << std::endl; \
    }                                \
  } while (false)

#define TRACE_LLVM_TYPE(str, type)                                \
  do {                                                            \
    if (g_tracing_enabled) {                                      \
      std::cout << str << " " << type.getAsString() << std::endl; \
    }                                                             \
  } while (false)

// Node: The following is used when tracing --dead-vars
// to provide extra info for the GC suspect.
#define TRACE_LLVM_DECL(str, decl)                   \
  do {                                               \
    if (g_tracing_enabled && g_dead_vars_analysis) { \
      std::cout << str << std::endl;                 \
      decl->dump();                                  \
    }                                                \
  } while (false)

typedef std::string MangledName;
typedef std::set<MangledName> CalleesSet;
typedef std::map<MangledName, MangledName> CalleesMap;

static bool GetMangledName(clang::MangleContext* ctx,
                           const clang::NamedDecl* decl, MangledName* result) {
  if (llvm::isa<clang::CXXConstructorDecl>(decl)) return false;
  if (llvm::isa<clang::CXXDestructorDecl>(decl)) return false;
  if (llvm::isa<clang::CXXDeductionGuideDecl>(decl)) return false;
  llvm::SmallVector<char, 512> output;
  llvm::raw_svector_ostream out(output);
  ctx->mangleName(decl, out);
  *result = out.str().str();
  return true;
}

static bool InV8Namespace(const clang::NamedDecl* decl) {
  const clang::DeclContext* ctx = decl->getDeclContext();
  const clang::NamespaceDecl* last_ns = nullptr;
  while (ctx) {
    if (const auto* ns = llvm::dyn_cast<clang::NamespaceDecl>(ctx)) {
      last_ns = ns;
    }
    ctx = ctx->getParent();
  }
  if (!last_ns) return false;
  return last_ns->getIdentifier() == &decl->getASTContext().Idents.get("v8");
}
static bool HasAnonymousBase(const clang::CXXRecordDecl* record) {
  if (!record->hasDefinition()) return false;
  for (const auto& base : record->bases()) {
    if (const clang::CXXRecordDecl* base_record =
            base.getType()->getAsCXXRecordDecl()) {
      const clang::CXXRecordDecl* base_def = base_record->getDefinition();
      if (base_def) {
        if (base_def->isInAnonymousNamespace() || HasAnonymousBase(base_def)) {
          return true;
        }
      }
    }
  }
  return false;
}

// Returns the fully qualified name of `record`. If `record` (or any of its base
// classes) is declared inside an anonymous namespace, appends a sanitized
// translation-unit path (`$<sanitized_main_file>`) so that identically named
// file-local classes in different `.cc` files (e.g. two TUs each defining
// `namespace { class MyVisitor : public ObjectVisitor {}; }`) do not collide
// when `gcmole.py` merges `class_hierarchy_*.json` across all TUs.
static std::string GetRecordQualifiedName(const clang::CXXRecordDecl* record) {
  std::string name = record->getQualifiedNameAsString();
  if (record->isInAnonymousNamespace() || HasAnonymousBase(record)) {
    clang::SourceManager& sm = record->getASTContext().getSourceManager();
    clang::FileID main_file_id = sm.getMainFileID();
    auto file_entry = sm.getFileEntryForID(main_file_id);
    std::string filename_str =
        file_entry ? file_entry->tryGetRealPathName().str() : "unknown";

    std::string sanitized_path = filename_str;
    for (char& c : sanitized_path) {
      if (c == '/' || c == '\\' || c == '.' || c == ' ' || c == '-' || c == ':')
        c = '_';
    }
    name += "$" + sanitized_path;
  }
  return name;
}

static std::string GetFunctionQualifiedName(const clang::FunctionDecl* decl) {
  if (const auto* method = llvm::dyn_cast<clang::CXXMethodDecl>(decl)) {
    const clang::CXXRecordDecl* record = method->getParent();
    if (record) {
      std::string class_name = GetRecordQualifiedName(record);
      return class_name + "::" + method->getNameAsString();
    }
  }
  return decl->getQualifiedNameAsString();
}

static bool IsDerivedFrom(const clang::CXXRecordDecl* record,
                          const clang::CXXRecordDecl* base) {
  return record->getCanonicalDecl() == base->getCanonicalDecl() ||
         record->isDerivedFrom(base);
}

static std::string EscapeJSONString(const std::string& s) {
  std::string out;
  for (char c : s) {
    if (c == '"')
      out += "\\\"";
    else if (c == '\\')
      out += "\\\\";
    else
      out += c;
  }
  return out;
}

// ----------------------------------------------------------------------------
// Virtual Call Devirtualization & ObjectVisitor Call-Graph Modeling
// ----------------------------------------------------------------------------
//
// GCMole operates in two passes across all translation units:
//   - Pass 1 (`dump-callees`): `CalleesPrinter` and `FunctionDeclarationFinder`
//     emit a per-TU call graph (`caller -> callee`) to stdout and a per-TU
//     class hierarchy (`class_hierarchy_<tu>.json`) recording base classes,
//     virtual methods, and `base_method -> override_method` mappings.
//     `gcmole.py` merges these across all TUs, resolves virtual dispatch using
//     Class Hierarchy Analysis (CHA), and propagates GC suspects backwards from
//     `CollectGarbage` / `SafepointSlowPath` into `gcsuspects` and `gccauses`.
//   - Pass 2 (`find-problems`): `FunctionAnalyzer` reads `gcsuspects` and
//     checks whether any call site in the analyzed function can transitively
//     trigger a GC.
//
// For a direct non-virtual call `Foo()`, `expr->getDirectCallee()` resolves
// directly to the target `FunctionDecl`. For virtual calls (and
// `ObjectVisitor::VisitRelocInfo`), static resolution at the call site only
// sees the base declaration `Base::Foo`, not the concrete overrides in
// subclasses. To model virtual dispatch accurately without cross-class
// false positives, Pass 1 and `gcmole.py` use three kinds of synthetic
// call-graph nodes:
//
// 1. External virtual calls (`VIRTUAL-<ReceiverClass>-<MangledMethod>`):
//    When calling `b->Foo()` on a receiver `b` with static type `B*`,
//    `CalleesPrinter` emits an edge to `VIRTUAL-B-<Method>` (where `<Method>`
//    is the declaration found by name lookup on `B`: `B::Foo` if `B` overrides
//    `Foo`, or `A::Foo` if `B` inherits it from `A`). `gcmole.py` normalizes
//    `<Method>` to the root declaration `A::Foo` and links the node to the
//    active implementation on `B` and to all overrides in subclasses of `B`
//    (`C::Foo`), without linking to unrelated sibling classes (`D::Foo`):
//
//                 A::Foo (virtual)
//                  +----------+
//                  |          |
//                  v          v
//              B::Foo        D::Foo (causes GC)
//                |
//                v
//              C::Foo
//
//        Caller(B* b) { b->Foo(); }
//             |
//             v
//        VIRTUAL-B-<B::Foo>  --->  B::Foo
//                            --->  C::Foo   (D::Foo is NOT linked)
//
// 2. Self virtual calls (`VIRTUALTHIS-<EnclosingClass>-<MangledMethod>`)
//    and inherited virtual methods (`SYNTHOVER-<Derived>-<MangledBaseMethod>`):
//    Suppose `Base::Helper()` is a virtual method that calls `this->Hook()`,
//    and `Derived1` and `Derived2` both inherit `Base::Helper()` without
//    overriding it, while only `Derived2::Hook()` can trigger GC:
//
//        struct Base {
//          virtual void Helper() { this->Hook(); }
//          virtual void Hook() {}
//        };
//        struct Derived1 : Base { void Hook() override { /* safe */ } };
//        struct Derived2 : Base { void Hook() override { CollectGarbage(); } };
//
//    If `Base::Helper()` emitted a plain `VIRTUAL-Base-<Base::Hook>` edge,
//    `Derived2::Hook()` would mark `Base::Helper()` as a GC suspect and falsely
//    flag `d1->Helper()` on a `Derived1* d1`. Instead:
//      - Inside `Base::Helper()`, `this->Hook()` (`*is_this_call = true`) emits
//        `VIRTUALTHIS-Base-<Base::Hook>`.
//      - For each subclass that inherits `Base::Helper()` without overriding
//        it, `gcmole.py` creates a `SYNTHOVER-Derived-<Base::Helper>` node that
//        clones `Base::Helper`'s outgoing edges and specializes
//        `VIRTUALTHIS-Base-...` to `VIRTUALTHIS-Derived-...`:
//
//        d1->Helper()                            d2->Helper()
//             |                                       |
//             v                                       v
//        VIRTUAL-Derived1-<Base::Helper> VIRTUAL-Derived2-<Base::Helper>
//             |                                       |
//             v                                       v
//        SYNTHOVER-Derived1-<Base::Helper> SYNTHOVER-Derived2-<Base::Helper>
//             |                                       |
//             v                                       v
//        VIRTUALTHIS-Derived1-<Base::Hook> VIRTUALTHIS-Derived2-<Base::Hook>
//             |                                       |
//             v                                       v
//        Derived1::Hook (safe)                   Derived2::Hook --> <GC>
//
// 3. Non-virtual `ObjectVisitor::VisitRelocInfo` bypass:
//    `ObjectVisitor::VisitRelocInfo(InstructionStream host, RelocIterator* it)`
//    is a non-virtual method on `ObjectVisitor` that iterates `RelocIterator`
//    and calls `rinfo->Visit(host, this)`. Inside `VisitRelocInfo`, `this` has
//    static type `ObjectVisitor*`, erasing the caller's concrete visitor
//    subclass `MyVisitor` and fanning out to every `ObjectVisitor` subclass in
//    V8. To preserve `MyVisitor`, `IsVirtualOrVisitorCall` intercepts
//    `v->VisitRelocInfo(...)` at the call site where `v`'s static type is still
//    `MyVisitor*` and emits `VIRTUAL-MyVisitor-<VisitRelocInfo>`. `gcmole.py`
//    then bypasses `VisitRelocInfo`'s AST body and connects the synthetic node
//    directly to `MyVisitor`'s overrides of the six reloc visitor hooks:
//
//        MyVisitor* v;
//        v->VisitRelocInfo(host, &it)
//             |
//             v
//        VIRTUAL-MyVisitor-<VisitRelocInfo>
//             |
//             +---> MyVisitor::VisitCodeTarget (or SYNTHOVER-MyVisitor-...)
//             +---> MyVisitor::VisitEmbeddedPointer
//             +---> MyVisitor::VisitExternalReference
//             +---> MyVisitor::VisitInternalReference
//             +---> MyVisitor::VisitOffHeapTarget
//             +---> MyVisitor::VisitJSDispatchTableEntry
static bool IsVirtualOrVisitorCall(clang::MangleContext* ctx,
                                   const clang::CallExpr* expr,
                                   const clang::CXXRecordDecl** receiver_record,
                                   const clang::CXXMethodDecl** method_decl,
                                   bool* is_this_call) {
  if (const clang::CXXMemberCallExpr* memcall =
          llvm::dyn_cast_or_null<clang::CXXMemberCallExpr>(expr)) {
    const clang::CXXMethodDecl* method = memcall->getMethodDecl();
    if (!method) return false;

    const clang::Expr* receiver =
        memcall->getImplicitObjectArgument()->IgnoreParenImpCasts();
    *is_this_call = llvm::isa<clang::CXXThisExpr>(receiver);

    clang::QualType receiver_type = receiver->getType();
    if (receiver_type->isPointerType()) {
      receiver_type = receiver_type->getPointeeType();
    }
    if (const clang::CXXRecordDecl* record =
            receiver_type->getAsCXXRecordDecl()) {
      if (InV8Namespace(record) && IsDerivedFrom(record, method->getParent())) {
        // Case 1: It is a virtual method.
        if (method->isVirtual()) {
          if (const auto* member_expr =
                  llvm::dyn_cast<clang::MemberExpr>(memcall->getCallee())) {
            // Explicitly qualified calls (e.g. `this->Base::Foo()` or
            // `obj->Base::Foo()`) bypass dynamic dispatch in C++ and always
            // invoke `Base::Foo` directly.
            if (member_expr->hasQualifier()) return false;
          }
          *receiver_record = record;
          *method_decl = method;
          return true;
        }
        // Case 2: It is the non-virtual ObjectVisitor::VisitRelocInfo.
        if (method->getNameAsString() == "VisitRelocInfo" &&
            method->getParent()->getQualifiedNameAsString() ==
                "v8::internal::ObjectVisitor") {
          *receiver_record = record;
          *method_decl = method;
          return true;
        }
      }
    }
  }
  return false;
}
static std::string EXTERNAL("EXTERNAL");
static std::string STATE_TAG("enum v8::internal::StateTag");

static bool IsExternalVMState(const clang::ValueDecl* var) {
  const clang::EnumConstantDecl* enum_constant =
      llvm::dyn_cast<clang::EnumConstantDecl>(var);
  if (enum_constant != nullptr &&
      enum_constant->getNameAsString() == EXTERNAL) {
    clang::QualType type = enum_constant->getType();
    return (type.getAsString() == STATE_TAG);
  }

  return false;
}

struct Resolver {
  explicit Resolver(clang::ASTContext& ctx)
      : ctx_(ctx), decl_ctx_(ctx.getTranslationUnitDecl()) {}

  Resolver(clang::ASTContext& ctx, clang::DeclContext* decl_ctx)
      : ctx_(ctx), decl_ctx_(decl_ctx) {}

  clang::DeclarationName ResolveName(const char* n) {
    clang::IdentifierInfo* ident = &ctx_.Idents.get(n);
    return ctx_.DeclarationNames.getIdentifier(ident);
  }

  Resolver ResolveNamespace(const char* n) {
    return Resolver(ctx_, Resolve<clang::NamespaceDecl>(n));
  }

  template <typename T>
  T* Resolve(const char* n) {
    if (decl_ctx_ == nullptr) return nullptr;

    clang::DeclContext::lookup_result result =
        decl_ctx_->lookup(ResolveName(n));

    clang::DeclContext::lookup_iterator end = result.end();
    for (clang::DeclContext::lookup_iterator i = result.begin(); i != end;
         i++) {
      clang::NamedDecl* decl = *i;

      if (llvm::isa<T>(decl)) {
        return llvm::cast<T>(decl);
      }

      // Try to strip off any type aliases.
      const clang::TypeAliasDecl* type_alias_decl =
          llvm::dyn_cast_or_null<clang::TypeAliasDecl>(decl);
      if (type_alias_decl) {
        clang::QualType underlying_type = type_alias_decl->getUnderlyingType();
        clang::QualType desugared_type = underlying_type.getDesugaredType(ctx_);
        clang::TagDecl* tag_decl = desugared_type->getAsTagDecl();
        if (!tag_decl) {
          llvm::errs() << "Couldn't resolve target decl of type alias "
                       << decl->getNameAsString() << "\n";
          decl->dump();
          return nullptr;
        }
        decl = tag_decl;
      }

      if (llvm::isa<T>(decl)) {
        return llvm::cast<T>(decl);
      }

      llvm::errs() << "Didn't match declaration template for " << n
                   << " against " << decl->getNameAsString() << "\n";
      decl->dump();
    }

    return nullptr;
  }

  template <typename T>
  T* ResolveCanonical(const char* n) {
    T* decl = Resolve<T>(n);
    return decl ? decl->getCanonicalDecl() : nullptr;
  }

  clang::CXXRecordDecl* ResolveDefinition(const char* n) {
    clang::CXXRecordDecl* decl = Resolve<clang::CXXRecordDecl>(n);
    return decl ? decl->getDefinition() : nullptr;
  }

 private:
  clang::ASTContext& ctx_;
  clang::DeclContext* decl_ctx_;
};

struct KnownDecls {
  clang::CXXRecordDecl* heap_object = nullptr;
  clang::CXXRecordDecl* smi = nullptr;
  clang::CXXRecordDecl* tagged_index = nullptr;
  clang::CXXRecordDecl* cleared_weak_value = nullptr;
  clang::ClassTemplateDecl* tagged = nullptr;
  clang::CXXRecordDecl* js_dispatch_handle = nullptr;
  clang::TypedefNameDecl* external_pointer_handle = nullptr;
  clang::TypedefNameDecl* cpp_heap_pointer_handle = nullptr;
  clang::TypedefNameDecl* trusted_pointer_handle = nullptr;
  clang::CXXRecordDecl* js_dispatch_handle_member = nullptr;
  clang::ClassTemplateDecl* external_pointer_member = nullptr;
  clang::ClassTemplateDecl* trusted_pointer_member = nullptr;
  clang::CXXRecordDecl* cpp_heap_pointer_member = nullptr;
  clang::CXXRecordDecl* external_pointer_slot = nullptr;
  clang::CXXRecordDecl* cpp_heap_pointer_slot = nullptr;
  clang::CXXRecordDecl* trusted_pointer_slot = nullptr;
  clang::CXXRecordDecl* embedder_data_slot = nullptr;
  clang::ClassTemplateDecl* tagged_member = nullptr;
  clang::ClassTemplateDecl* unaligned_value_member = nullptr;
  clang::CXXRecordDecl* unaligned_double_member = nullptr;
  clang::ClassTemplateDecl* slot_base = nullptr;
  clang::CXXRecordDecl* no_gc_mole = nullptr;
  clang::CXXRecordDecl* conservative_pinning_scope = nullptr;

  static KnownDecls Resolve(clang::ASTContext& ctx) {
    Resolver v8_internal =
        Resolver(ctx).ResolveNamespace("v8").ResolveNamespace("internal");
    KnownDecls decls;
    // It is a valid situation that no_gc_mole == nullptr when DisableGCMole is
    // not included and can't be resolved. This is gracefully handled in the
    // FunctionAnalyzer later.
    decls.no_gc_mole =
        v8_internal.Resolve<clang::CXXRecordDecl>("DisableGCMole");
    decls.conservative_pinning_scope =
        v8_internal.Resolve<clang::CXXRecordDecl>("ConservativePinningScope");
    decls.heap_object = v8_internal.ResolveDefinition("HeapObject");
    decls.smi = v8_internal.ResolveDefinition("Smi");
    decls.tagged_index = v8_internal.ResolveDefinition("TaggedIndex");
    decls.cleared_weak_value =
        v8_internal.Resolve<clang::CXXRecordDecl>("ClearedWeakValue");
    decls.tagged =
        v8_internal.ResolveCanonical<clang::ClassTemplateDecl>("Tagged");
    decls.js_dispatch_handle =
        v8_internal.ResolveCanonical<clang::CXXRecordDecl>("JSDispatchHandle");
    decls.external_pointer_handle =
        v8_internal.ResolveCanonical<clang::TypedefNameDecl>(
            "ExternalPointerHandle");
    decls.cpp_heap_pointer_handle =
        v8_internal.ResolveCanonical<clang::TypedefNameDecl>(
            "CppHeapPointerHandle");
    decls.trusted_pointer_handle =
        v8_internal.ResolveCanonical<clang::TypedefNameDecl>(
            "TrustedPointerHandle");
    decls.js_dispatch_handle_member =
        v8_internal.ResolveCanonical<clang::CXXRecordDecl>(
            "JSDispatchHandleMember");
    decls.external_pointer_member =
        v8_internal.ResolveCanonical<clang::ClassTemplateDecl>(
            "ExternalPointerMember");
    decls.trusted_pointer_member =
        v8_internal.ResolveCanonical<clang::ClassTemplateDecl>(
            "TrustedPointerMember");
    decls.cpp_heap_pointer_member =
        v8_internal.ResolveCanonical<clang::CXXRecordDecl>(
            "CppHeapPointerMember");
    decls.external_pointer_slot =
        v8_internal.ResolveCanonical<clang::CXXRecordDecl>(
            "ExternalPointerSlot");
    decls.cpp_heap_pointer_slot =
        v8_internal.ResolveCanonical<clang::CXXRecordDecl>(
            "CppHeapPointerSlot");
    decls.trusted_pointer_slot =
        v8_internal.ResolveCanonical<clang::CXXRecordDecl>(
            "TrustedPointerSlot");
    decls.embedder_data_slot =
        v8_internal.ResolveCanonical<clang::CXXRecordDecl>("EmbedderDataSlot");
    decls.tagged_member =
        v8_internal.ResolveCanonical<clang::ClassTemplateDecl>("TaggedMember");
    decls.unaligned_value_member =
        v8_internal.ResolveCanonical<clang::ClassTemplateDecl>(
            "UnalignedValueMember");
    decls.unaligned_double_member =
        v8_internal.ResolveCanonical<clang::CXXRecordDecl>(
            "UnalignedDoubleMember");
    decls.slot_base =
        v8_internal.ResolveCanonical<clang::ClassTemplateDecl>("SlotBase");
    return decls;
  }

  bool HasRequiredDecls() const {
    return heap_object != nullptr && smi != nullptr &&
           tagged_index != nullptr && tagged != nullptr;
  }
};

// Pass 1 (`dump-callees`) visitor: walks every function body in the translation
// unit (ignoring regions guarded by `DisableGCMole`) and prints `caller` ->
// `callee` edges for `gcmole.py`. Virtual calls and `VisitRelocInfo` calls are
// redirected to synthetic `VIRTUAL-` / `VIRTUALTHIS-` nodes (see the comment on
// `IsVirtualOrVisitorCall` above).
class CalleesPrinter : public clang::RecursiveASTVisitor<CalleesPrinter> {
 public:
  explicit CalleesPrinter(clang::MangleContext* ctx,
                          clang::CXXRecordDecl* no_gc_mole_decl)
      : ctx_(ctx), no_gc_mole_decl_(no_gc_mole_decl) {}

  virtual ~CalleesPrinter() {
    for (auto& [_, set_ptr] : callgraph_) {
      delete set_ptr;
    }
  }

  struct GCScope {
    bool is_guarded = false;
  };

  bool IsGuarded() {
    for (const auto& s : gc_scopes_) {
      if (s.is_guarded) return true;
    }
    return false;
  }

  bool TraverseStmt(clang::Stmt* S) {
    if (!S) return true;
    bool introduces_scope = IntroducesScope(S);
    if (introduces_scope) {
      gc_scopes_.push_back(GCScope());
    }
    bool result = clang::RecursiveASTVisitor<CalleesPrinter>::TraverseStmt(S);
    if (introduces_scope) {
      gc_scopes_.pop_back();
    }
    return result;
  }

  bool VisitVarDecl(clang::VarDecl* var) {
    clang::QualType var_type = var->getType();
    if (IsGCGuard(var_type)) {
      if (!gc_scopes_.empty()) {
        gc_scopes_.back().is_guarded = true;
      }
    }
    return true;
  }

  // Visits implicit tuple-like `get<I>()` bindings and `DisableGCMole` guards
  // inside C++17 structured bindings (`auto [a, b] = expr;`), which
  // `RecursiveASTVisitor` skips when `shouldVisitImplicitCode()` is false.
  bool TraverseDecompositionDecl(clang::DecompositionDecl* decomp) {
    if (!clang::RecursiveASTVisitor<CalleesPrinter>::TraverseDecompositionDecl(
            decomp)) {
      return false;
    }
    for (clang::BindingDecl* binding : decomp->flat_bindings()) {
      if (clang::VarDecl* holding_var = binding->getHoldingVar()) {
        if (holding_var->hasInit() && !TraverseStmt(holding_var->getInit())) {
          return false;
        }
      } else if (clang::Expr* binding_expr = binding->getBinding()) {
        if (!TraverseStmt(binding_expr)) return false;
      }
      if (IsGCGuard(binding->getType()) && !gc_scopes_.empty()) {
        gc_scopes_.back().is_guarded = true;
      }
    }
    return true;
  }

  // Visits the implicit `begin()`, `end()`, `operator!=`, `operator++`, and
  // `operator*` expressions of a range-for loop in scope order, which
  // `RecursiveASTVisitor` skips when `shouldVisitImplicitCode()` is false.
  bool TraverseCXXForRangeStmt(clang::CXXForRangeStmt* stmt) {
    if (!TraverseStmt(stmt->getInit())) return false;
    if (!TraverseDeclStmtInits(stmt->getRangeStmt())) return false;
    if (!TraverseDeclStmtInits(stmt->getBeginStmt())) return false;
    if (!TraverseDeclStmtInits(stmt->getEndStmt())) return false;
    if (!TraverseStmt(stmt->getCond())) return false;
    if (!TraverseStmt(stmt->getInc())) return false;
    if (!TraverseDeclStmtInits(stmt->getLoopVarStmt())) return false;
    if (!TraverseStmt(stmt->getLoopVarStmt())) return false;
    if (!TraverseStmt(stmt->getBody())) return false;
    return true;
  }

  // Visits the semantic form of C++20 rewritten comparisons (e.g. `a != b`
  // calling `operator==`, including range-for's `__begin != __end`), since
  // `RecursiveASTVisitor` only visits their syntactic operands when
  // `shouldVisitImplicitCode()` is false and would miss the operator call.
  bool TraverseCXXRewrittenBinaryOperator(
      clang::CXXRewrittenBinaryOperator* expr) {
    return TraverseStmt(expr->getSemanticForm());
  }

  virtual bool VisitCallExpr(clang::CallExpr* expr) {
    if (IsGuarded()) return true;

    const clang::CXXRecordDecl* receiver_record = nullptr;
    const clang::CXXMethodDecl* method_decl = nullptr;
    bool is_this_call = false;
    if (IsVirtualOrVisitorCall(ctx_, expr, &receiver_record, &method_decl,
                               &is_this_call)) {
      MangledName method_mangled;
      if (GetMangledName(ctx_, method_decl, &method_mangled)) {
        MangledName dummy_mangled;
        std::string dummy_qualified;
        std::string receiver_name = GetRecordQualifiedName(receiver_record);
        if (is_this_call) {
          dummy_mangled = "VIRTUALTHIS-" + receiver_name + "-" + method_mangled;
          dummy_qualified =
              receiver_name + "::" + method_decl->getNameAsString();
        } else {
          dummy_mangled = "VIRTUAL-" + receiver_name + "-" + method_mangled;
          dummy_qualified =
              receiver_name + "::" + method_decl->getNameAsString();
        }

        mangled_to_function_[dummy_mangled] = dummy_qualified;
        AddCallee(dummy_mangled, dummy_qualified);
        return true;  // Skip default
      }
    }
    const clang::FunctionDecl* callee = expr->getDirectCallee();
    if (callee != nullptr) AnalyzeFunction(callee);
    return true;
  }

  virtual bool VisitDeclRefExpr(clang::DeclRefExpr* expr) {
    // If function mentions EXTERNAL VMState add artificial garbage collection
    // mark.
    if (IsExternalVMState(expr->getDecl())) {
      AddCallee("CollectGarbage", "CollectGarbage");
    }
    return true;
  }

  void AnalyzeFunction(const clang::FunctionDecl* f) {
    if (f->isNoReturn()) return;
    if (!InV8Namespace(f)) return;
    MangledName name;
    if (!GetMangledName(ctx_, f, &name)) return;
    const std::string function = GetFunctionQualifiedName(f);
    AddCallee(name, function);

    const clang::FunctionDecl* body = nullptr;
    if (f->hasBody(body) && !Analyzed(name)) {
      EnterScope(name);
      TraverseStmt(body->getBody());
      LeaveScope();
    }
  }

  typedef std::map<MangledName, CalleesSet*> Callgraph;

  bool Analyzed(const MangledName& name) {
    return analyzed_.find(name) != analyzed_.end();
  }

  void EnterScope(const MangledName& name) {
    analyzed_.insert(name);

    CalleesSet* callees = callgraph_[name];

    if (callees == nullptr) {
      callgraph_[name] = callees = new CalleesSet();
    }

    scopes_.push(callees);
  }

  void LeaveScope() { scopes_.pop(); }

  void AddCallee(const MangledName& name, const MangledName& function) {
    if (!scopes_.empty()) scopes_.top()->insert(name);
    mangled_to_function_[name] = function;
  }



  void PrintCallGraph() {
    for (Callgraph::const_iterator i = callgraph_.begin(), e = callgraph_.end();
         i != e; ++i) {
      std::cout << i->first << "," << mangled_to_function_[i->first] << "\n";

      CalleesSet* callees = i->second;
      for (CalleesSet::const_iterator j = callees->begin(), e = callees->end();
           j != e; ++j) {
        std::cout << "\t" << *j << "," << mangled_to_function_[*j] << "\n";
      }
    }
  }

 private:
  bool TraverseDeclStmtInits(clang::DeclStmt* decl_stmt) {
    if (!decl_stmt) return true;
    for (clang::Decl* decl : decl_stmt->decls()) {
      if (auto* var = llvm::dyn_cast<clang::VarDecl>(decl)) {
        if (var->hasInit() && !TraverseStmt(var->getInit())) return false;
      }
    }
    return true;
  }

  bool IntroducesScope(clang::Stmt* S) {
    switch (S->getStmtClass()) {
      case clang::Stmt::CompoundStmtClass:
      case clang::Stmt::IfStmtClass:
      case clang::Stmt::ForStmtClass:
      case clang::Stmt::CXXForRangeStmtClass:
      case clang::Stmt::WhileStmtClass:
      case clang::Stmt::SwitchStmtClass:
        return true;
      default:
        return false;
    }
  }

  bool IsSameType(clang::QualType qtype, clang::CXXRecordDecl* decl) {
    if (!decl) return false;
    if (qtype.isNull() || qtype->isNullPtrType()) return false;

    const clang::CXXRecordDecl* record = qtype->getAsCXXRecordDecl();
    if (!record) return false;

    return record->getCanonicalDecl() == decl->getCanonicalDecl();
  }

  bool IsGCGuard(clang::QualType qtype) {
    return IsSameType(qtype, no_gc_mole_decl_);
  }

  clang::MangleContext* ctx_;
  clang::CXXRecordDecl* no_gc_mole_decl_;

  std::stack<CalleesSet*> scopes_;
  Callgraph callgraph_;
  CalleesMap mangled_to_function_;
  CalleesSet analyzed_;

  std::vector<GCScope> gc_scopes_;
};

class FunctionDeclarationFinder
    : public clang::ASTConsumer,
      public clang::RecursiveASTVisitor<FunctionDeclarationFinder> {
 public:
  explicit FunctionDeclarationFinder(
      clang::DiagnosticsEngine& diagnostics_engine,
      clang::SourceManager& source_manager,
      const std::vector<std::string>& args)
      : diagnostics_engine_(diagnostics_engine),
        source_manager_(source_manager),
        mangle_context_(nullptr),
        callees_printer_(nullptr),
        args_(args) {}

  ~FunctionDeclarationFinder() override {
    delete callees_printer_;
    delete mangle_context_;
  }

  struct ClassInfo {
    std::vector<std::string> bases;
    std::map<std::string, std::string> overrides;
    std::vector<std::string> virtual_methods;
  };

  void HandleTranslationUnit(clang::ASTContext& ctx) override {
    if (TranslationUnitIgnored()) return;
    mangle_context_ =
        clang::ItaniumMangleContext::create(ctx, diagnostics_engine_);

    Resolver r(ctx);
    auto v8_internal = r.ResolveNamespace("v8").ResolveNamespace("internal");
    clang::CXXRecordDecl* no_gc_mole_decl =
        v8_internal.Resolve<clang::CXXRecordDecl>("DisableGCMole");

    callees_printer_ = new CalleesPrinter(mangle_context_, no_gc_mole_decl);
    TraverseDecl(ctx.getTranslationUnitDecl());
    callees_printer_->PrintCallGraph();
    DumpClassHierarchy();
  }

  virtual bool VisitFunctionDecl(clang::FunctionDecl* decl) {
    callees_printer_->AnalyzeFunction(decl);
    return true;
  }

  virtual bool VisitCXXRecordDecl(clang::CXXRecordDecl* record) {
    if (record->isCompleteDefinition() && InV8Namespace(record)) {
      RecordClassHierarchy(record);
    }
    return true;
  }

 private:
  // Records direct base classes, virtual methods, and `overridden_mangled ->
  // override_mangled` mappings for `record`, serialized to
  // `class_hierarchy_<tu>.json` so `gcmole.py` can construct the global class
  // hierarchy DAG for `VIRTUAL-`, `VIRTUALTHIS-`, and `SYNTHOVER-` resolution.
  void RecordClassHierarchy(const clang::CXXRecordDecl* record) {
    std::string class_name = GetRecordQualifiedName(record);
    if (class_hierarchy_.find(class_name) != class_hierarchy_.end()) return;

    ClassInfo info;
    for (const auto& base : record->bases()) {
      if (const clang::CXXRecordDecl* base_record =
              base.getType()->getAsCXXRecordDecl()) {
        info.bases.push_back(GetRecordQualifiedName(base_record));
      }
    }

    for (const auto* method : record->methods()) {
      if (method->isVirtual()) {
        MangledName method_mangled;
        if (GetMangledName(mangle_context_, method, &method_mangled)) {
          info.virtual_methods.push_back(method_mangled);
          for (const auto* overridden : method->overridden_methods()) {
            MangledName overridden_mangled;
            if (GetMangledName(mangle_context_, overridden,
                               &overridden_mangled)) {
              info.overrides[overridden_mangled] = method_mangled;
            }
          }
        }
      }
    }
    class_hierarchy_[class_name] = info;
  }

  std::string GetOutputFilePath(const std::string& prefix) {
    std::string output_dir = ".";
    for (const auto& arg : args_) {
      if (arg.compare(0, 11, "output-dir:") == 0) {
        output_dir = arg.substr(11);
      }
    }

    clang::FileID main_file_id = source_manager_.getMainFileID();
    auto file_entry = source_manager_.getFileEntryForID(main_file_id);
    std::string filename_str =
        file_entry ? file_entry->tryGetRealPathName().str() : "unknown";

    std::string sanitized_path = filename_str;
    for (char& c : sanitized_path) {
      if (c == '/' || c == '\\' || c == '.' || c == ' ' || c == '-' || c == ':')
        c = '_';
    }

    return output_dir + "/" + prefix + "_" + sanitized_path + ".json";
  }

  void DumpClassHierarchy() {
    std::string filepath = GetOutputFilePath("class_hierarchy");
    std::ofstream fout(filepath);
    if (!fout.is_open()) return;

    fout << "{\n";
    bool first_class = true;
    for (const auto& [class_name, info] : class_hierarchy_) {
      if (!first_class) fout << ",\n";
      first_class = false;

      fout << "  \"" << EscapeJSONString(class_name) << "\": {\n";
      fout << "    \"bases\": [";
      bool first_base = true;
      for (const auto& base : info.bases) {
        if (!first_base) fout << ", ";
        first_base = false;
        fout << "\"" << EscapeJSONString(base) << "\"";
      }
      fout << "],\n";

      fout << "    \"overrides\": {\n";
      bool first_override = true;
      for (const auto& [base_m, der_m] : info.overrides) {
        if (!first_override) fout << ",\n";
        first_override = false;
        fout << "      \"" << base_m << "\": \"" << der_m << "\"";
      }
      fout << "\n    },\n";

      fout << "    \"virtual_methods\": [";
      bool first_vm = true;
      for (const auto& vm : info.virtual_methods) {
        if (!first_vm) fout << ", ";
        first_vm = false;
        fout << "\"" << vm << "\"";
      }
      fout << "]\n";

      fout << "  }";
    }
    fout << "\n}\n";
  }

  bool TranslationUnitIgnored() {
    if (!ignored_files_loaded_) {
      auto fileOrError =
          llvm::MemoryBuffer::getFile("tools/gcmole/ignored_files");
      if (auto error = fileOrError.getError()) {
        llvm::errs() << "Failed to open ignored_files file\n";
        std::terminate();
      }
      for (llvm::line_iterator it(*fileOrError->get()); !it.is_at_end(); ++it) {
        ignored_files_.insert(*it);
      }
      ignored_files_loaded_ = true;
    }

    clang::FileID main_file_id = source_manager_.getMainFileID();
    auto file_entry = source_manager_.getFileEntryForID(main_file_id);
    if (!file_entry) return false;
    llvm::StringRef filename = file_entry->tryGetRealPathName();

    bool result = false;
    for (const auto& entry : ignored_files_) {
      if (filename.ends_with(entry.getKey())) {
        result = true;
        break;
      }
    }
    if (result) {
      llvm::outs() << "Ignoring file " << filename << "\n";
    }
    return result;
  }

  clang::DiagnosticsEngine& diagnostics_engine_;
  clang::SourceManager& source_manager_;
  clang::MangleContext* mangle_context_;

  CalleesPrinter* callees_printer_;
  std::vector<std::string> args_;
  std::map<std::string, ClassInfo> class_hierarchy_;

  bool ignored_files_loaded_ = false;
  llvm::StringSet<> ignored_files_;
};

static bool gc_suspects_loaded = false;
static CalleesSet gc_suspects;
static CalleesSet gc_functions;

static bool allowlist_loaded = false;
static CalleesSet suspects_allowlist;

static bool gc_causes_loaded = false;
static std::map<MangledName, std::vector<MangledName>> gc_causes;
static std::map<MangledName, std::string> demangled_names;

static void LoadGCCauses() {
  if (gc_causes_loaded) return;
  std::ifstream fin;
  if (g_output_dir != ".") {
    fin.open(g_output_dir + "/gccauses");
  }
  if (!fin.is_open()) {
    fin.open("gccauses");
  }
  std::string mangled, function;

  if (!fin.is_open()) {
    std::cerr << "failed to open gccauses" << std::endl;
    std::abort();
  }

  std::string line;
  auto read_entry = [&fin, &line](std::string* mangled, std::string* function) {
    if (!std::getline(fin, line)) return false;
    size_t comma = line.find(',');
    if (comma == std::string::npos) {
      *mangled = line;
      *function = line;
    } else {
      *mangled = line.substr(0, comma);
      *function = line.substr(comma + 1);
    }
    return true;
  };

  while (read_entry(&mangled, &function)) {
    if (mangled.empty()) break;
    std::string parent = mangled;
    demangled_names[parent] = function;
    // start,nested
    if (!read_entry(&mangled, &function)) {
      std::cerr << "Error: Truncated gccauses database." << std::endl;
      std::abort();
    }
    assert(mangled.compare("start") == 0);
    assert(function.compare("nested") == 0);
    while (true) {
      if (!read_entry(&mangled, &function)) {
        std::cerr << "Error: Truncated nested gccauses database." << std::endl;
        std::abort();
      }
      if (mangled.compare("end") == 0) {
        assert(function.compare("nested") == 0);
        break;
      }
      gc_causes[parent].push_back(mangled);
      demangled_names[mangled] = function;
    }
  }
  gc_causes_loaded = true;
}

static void LoadGCSuspects() {
  if (gc_suspects_loaded) return;

  std::ifstream fin("gcsuspects");
  std::string mangled, function;

  if (!fin.is_open()) {
    std::cerr << "failed to open gcsuspects" << std::endl;
    std::abort();
  }

  while (!fin.eof()) {
    std::getline(fin, mangled, ',');
    gc_suspects.insert(mangled);
    std::getline(fin, function);
    gc_functions.insert(function);
  }

  gc_suspects_loaded = true;
}

static void LoadSuspectsAllowList() {
  if (allowlist_loaded) return;

  // TODO(cbruni): clean up once fully migrated
  std::ifstream fin("tools/gcmole/suspects.allowlist");
  std::string s;

  if (!fin.is_open()) {
    std::cerr << "failed to open suspects.allowlist" << std::endl;
    std::abort();
  }

  while (fin >> s) suspects_allowlist.insert(s);

  allowlist_loaded = true;
}

// Looks for exact match of the mangled name.
static bool IsKnownToCauseGC(clang::MangleContext* ctx,
                             const clang::FunctionDecl* decl) {
  LoadGCSuspects();
  if (!InV8Namespace(decl)) return false;
  if (suspects_allowlist.find(decl->getNameAsString()) !=
      suspects_allowlist.end()) {
    return false;
  }
  MangledName name;
  if (GetMangledName(ctx, decl, &name)) {
    return gc_suspects.find(name) != gc_suspects.end();
  }
  return false;
}

// Looks for partial match of only the function name.
static bool IsSuspectedToCauseGC(clang::MangleContext* ctx,
                                 const clang::FunctionDecl* decl) {
  LoadGCSuspects();
  if (!InV8Namespace(decl)) return false;
  LoadSuspectsAllowList();
  if (suspects_allowlist.find(decl->getNameAsString()) !=
      suspects_allowlist.end()) {
    return false;
  }
  if (gc_functions.find(GetFunctionQualifiedName(decl)) != gc_functions.end()) {
    TRACE_LLVM_DECL("Suspected by ", decl);
    return true;
  }
  return false;
}

// Cumulative effects (kCausesGC, kRawDef, kRawUse) accumulate across sequential
// subexpressions and parent AST nodes to detect unsequenced evaluation-order
// hazards (e.g. `Callee(obj->field_, CauseGC())` or `Foo(*raw_obj_handle,
// CauseGC())`):
//   - kCausesGC: subexpression may trigger a garbage collection.
//   - kRawDef: subexpression produces or loads an unrooted on-heap value (e.g.
//     dereferencing a Handle into a `Tagged<HeapObject>`, calling `so->` on a
//     `Handle<SomeObject>`, copying a `Tagged<T>` argument by value via
//     `CXXConstructExpr`, or loading an on-heap member by value, conservatively
//     flagging unsequenced argument evaluations across C++17 call sites).
//   - kRawUse: subexpression reads a raw variable that is already stale in
//     `env` (note: `Use()` returns without setting `kRawUse` when
//     `env.IsAlive(var_decl)` is true, so `kRawUse` is only set when the
//     variable was already killed by an earlier GC).
//
// Value effects (kOnHeapMemberLValue, kRawMemberAddress) describe the
// provenance and value category of the current expression's result itself:
//   - kOnHeapMemberLValue: glvalue referring to an interior field/element of an
//     on-heap object, e.g.:
//       `obj->field_`, `obj->values()[i]`, `*(&obj->field_)`,
//       or a reference `int& ref = obj->field_;`
//   - kRawMemberAddress: pointer, `Address`, or `SlotBase` pointing inside an
//     on-heap object, e.g.:
//       `&obj->field_`, `std::addressof(obj->field_)`, `obj->values()`,
//       `obj->field_.storage_address()`, `ObjectSlot(&obj->field_)`,
//       `obj->RawField(offset)`
// Value effects are transformed or cleared as the expression is addressed
// (`&` -> kRawMemberAddress), dereferenced (`*` -> kOnHeapMemberLValue),
// decayed (`T[N]` -> kRawMemberAddress), or loaded by value (LValueToRValue ->
// kRawDef without provenance).
static const int kNoEffect = 0;
static const int kCausesGC = 1 << 0;
static const int kRawDef = 1 << 1;
static const int kRawUse = 1 << 2;
static const int kOnHeapMemberLValue = 1 << 3;
static const int kRawMemberAddress = 1 << 4;
static const int kValueEffects = kOnHeapMemberLValue | kRawMemberAddress;
static const int kCumulativeEffects = kCausesGC | kRawDef | kRawUse;
static const int kAllEffects = kCumulativeEffects | kValueEffects;

class Environment;

class ExprEffect {
 public:
  bool hasGC() const { return (effect_ & kCausesGC) != 0; }
  void setGC() { effect_ |= kCausesGC; }

  bool hasRawDef() const { return (effect_ & kRawDef) != 0; }
  void setRawDef() { effect_ |= kRawDef; }

  bool hasRawUse() const { return (effect_ & kRawUse) != 0; }
  void setRawUse() { effect_ |= kRawUse; }

  bool hasOnHeapMemberLValue() const {
    return (effect_ & kOnHeapMemberLValue) != 0;
  }
  void setOnHeapMemberLValue() { effect_ |= kOnHeapMemberLValue; }

  bool hasRawMemberAddress() const {
    return (effect_ & kRawMemberAddress) != 0;
  }
  void setRawMemberAddress() { effect_ |= kRawMemberAddress; }

  bool hasMemberProvenance() const { return (effect_ & kValueEffects) != 0; }

  ExprEffect WithProvenance(int provenance) const {
    return ExprEffect((effect_ & kCumulativeEffects) | provenance, env_);
  }

  // Strips value provenance (`kOnHeapMemberLValue`, `kRawMemberAddress`) when
  // an expression produces a result that is no longer an on-heap member lvalue
  // or address, converting any consumed member provenance into `kRawDef` so
  // enclosing unsequenced expressions still see the heap access.
  ExprEffect WithoutProvenance() const {
    return ExprEffect(
        (effect_ & kCumulativeEffects) | (hasMemberProvenance() ? kRawDef : 0),
        env_);
  }

  static ExprEffect None() { return ExprEffect(kNoEffect, nullptr); }
  static ExprEffect NoneWithEnv(Environment* env) {
    return ExprEffect(kNoEffect, env);
  }

  static ExprEffect Merge(ExprEffect a, ExprEffect b,
                          const Environment& base_env);
  static ExprEffect MergeSeq(ExprEffect a, ExprEffect b);
  ExprEffect Define(const clang::ValueDecl* var_decl, bool has_raw_member_addr);

  Environment* env() const { return env_; }

  static ExprEffect GC() { return ExprEffect(kCausesGC, nullptr); }

 private:
  ExprEffect(int effect, Environment* env)
      : effect_(effect & kAllEffects), env_(env) {}

  int effect_;
  Environment* env_;
};

const std::string BAD_EXPR_MSG(
    "Possible problem with evaluation order with interleaved GCs.");
const std::string DEAD_VAR_MSG("Possibly stale variable due to GCs.");
constexpr const clang::ValueDecl* THIS = nullptr;

class Environment {
 public:
  using VarSet = llvm::SmallPtrSet<const clang::ValueDecl*, 4>;
  using SymbolTable = std::map<const clang::ValueDecl*, int>;

  Environment() = default;

  static Environment Unreachable() {
    Environment env;
    env.unreachable_ = true;
    return env;
  }

  bool IsUnreachable() const { return unreachable_; }

  static Environment Merge(const Environment& l, const Environment& r) {
    Environment out(l);
    out &= r;
    return out;
  }

  Environment ApplyEffect(ExprEffect effect) const {
    Environment out(*this);
    if (effect.hasGC()) out.live_.clear();
    if (effect.env()) {
      out |= *effect.env();
      out.raw_member_vars_cleared_.clear();
    }
    return out;
  }

  // Checks whether `decl` (or `this` when `decl == nullptr`) is currently live.
  bool IsAlive(const clang::ValueDecl* decl) const {
    const clang::ValueDecl* canonical = CanonicalVar(decl);
    if (canonical == nullptr) return is_live(kThisCode);
    SymbolTable::const_iterator code = symbol_table_.find(canonical);
    if (code == symbol_table_.end()) return false;
    return is_live(code->second);
  }

  // Compares statement-level environments for loop fixpoint convergence.
  // `raw_member_vars_cleared_` is omitted because it is only populated in
  // expression-level delta environments and is cleared by `ApplyEffect`.
  bool Equal(const Environment& env) const {
    if (unreachable_ && env.unreachable_) return true;
    if (unreachable_ != env.unreachable_) return false;
    size_t size = std::max(live_.size(), env.live_.size());
    for (size_t i = 0; i < size; ++i) {
      if (is_live(i) != env.is_live(i)) return false;
    }
    return raw_member_vars_ == env.raw_member_vars_ &&
           member_ref_vars_ == env.member_ref_vars_;
  }

  void MDefine(const clang::ValueDecl* decl) { set_live(SymbolToCode(decl)); }

  void MDefineThis() { set_live(kThisCode); }

  // Tracks whether `decl` currently holds a raw address/slot into an on-heap
  // object, updating provenance as the variable is assigned or rebound:
  //   int* p = &obj->field_;  // SetRawMemberVar(p, true)
  //   CauseGC();
  //   *p = 42;                // flagged: `p` is in `raw_member_vars_` and dead
  //   p = &stack_int;         // SetRawMemberVarInDelta(p, false)
  //   CauseGC();
  //   *p = 42;                // safe: `p` was rebound to a non-heap address
  void SetRawMemberVar(const clang::ValueDecl* decl, bool is_raw_member) {
    const clang::ValueDecl* canonical = CanonicalVar(decl);
    if (!canonical) return;
    if (is_raw_member) {
      raw_member_vars_.insert(canonical);
    } else {
      raw_member_vars_.erase(canonical);
    }
  }

  // Updates `decl`'s raw-member provenance inside an expression-level delta
  // environment (`ExprEffect::Define`), recording explicit un-tracking in
  // `raw_member_vars_cleared_` so `operator|=` / `ApplyEffect` can erase `decl`
  // from the enclosing statement-level environment (where
  // `raw_member_vars_cleared_` is always empty).
  void SetRawMemberVarInDelta(const clang::ValueDecl* decl,
                              bool is_raw_member) {
    const clang::ValueDecl* canonical = CanonicalVar(decl);
    if (!canonical) return;
    if (is_raw_member) {
      raw_member_vars_.insert(canonical);
      raw_member_vars_cleared_.erase(canonical);
    } else {
      raw_member_vars_.erase(canonical);
      raw_member_vars_cleared_.insert(canonical);
    }
  }

  // Marks all currently tracked raw-member variables as stale in `live_`.
  void ClearRawMemberVarsLive() {
    for (const clang::ValueDecl* decl : raw_member_vars_) {
      int code = SymbolToCode(decl);
      if (static_cast<size_t>(code) < live_.size()) {
        live_[code] = false;
      }
    }
  }

  // Tracks whether `decl` is a C++ reference or `SlotBase` proxy reference
  // bound to an on-heap member. Unlike pointers (`raw_member_vars_`),
  // references cannot be rebound after initialization; an assignment like
  // `ref = 42` writes through to the underlying heap member rather than
  // redefining `ref`:
  //   int& ref = obj->field_;
  //   CauseGC();
  //   ref = 42;               // flagged: writes to moved `obj->field_`
  void SetMemberRefVar(const clang::ValueDecl* decl, bool is_member_ref) {
    const clang::ValueDecl* canonical = CanonicalVar(decl);
    if (!canonical) return;
    if (is_member_ref) {
      member_ref_vars_.insert(canonical);
    } else {
      member_ref_vars_.erase(canonical);
    }
  }

  bool IsRawMemberVar(const clang::ValueDecl* decl) const {
    const clang::ValueDecl* canonical = CanonicalVar(decl);
    return canonical && raw_member_vars_.contains(canonical);
  }

  bool IsOnHeapMemberRefVar(const clang::ValueDecl* decl) const {
    const clang::ValueDecl* canonical = CanonicalVar(decl);
    return canonical && member_ref_vars_.contains(canonical);
  }

  static int SymbolToCode(const clang::ValueDecl* decl) {
    const clang::ValueDecl* canonical = CanonicalVar(decl);
    if (canonical == nullptr) return kThisCode;
    SymbolTable::iterator code = symbol_table_.find(canonical);

    if (code == symbol_table_.end()) {
      int new_code = static_cast<int>(symbol_table_.size()) + 1;
      symbol_table_.insert(std::make_pair(canonical, new_code));
      return new_code;
    }

    return code->second;
  }

  static void MarkTypeRawVar(const clang::ValueDecl* decl) {
    if (const clang::ValueDecl* canonical = CanonicalVar(decl)) {
      type_raw_vars_.insert(canonical);
    }
  }

  static bool IsTypeRawVar(const clang::ValueDecl* decl) {
    const clang::ValueDecl* canonical = CanonicalVar(decl);
    return canonical && type_raw_vars_.contains(canonical);
  }

  static void ClearSymbolTable() {
    for (Environment* e : envs_) delete e;
    envs_.clear();
    symbol_table_.clear();
    type_raw_vars_.clear();
  }

  void Print() const {
    bool comma = false;
    std::cout << "{";
    if (is_live(kThisCode)) {
      std::cout << "this";
      comma = true;
    }
    for (auto& e : symbol_table_) {
      if (!is_live(e.second)) continue;
      if (comma) std::cout << ", ";
      std::cout << e.first->getNameAsString();
      comma = true;
    }
    std::cout << "} raw_members={";
    comma = false;
    for (const clang::ValueDecl* d : raw_member_vars_) {
      if (comma) std::cout << ", ";
      std::cout << d->getNameAsString();
      comma = true;
    }
    std::cout << "} member_refs={";
    comma = false;
    for (const clang::ValueDecl* d : member_ref_vars_) {
      if (comma) std::cout << ", ";
      std::cout << d->getNameAsString();
      comma = true;
    }
    std::cout << "}" << std::endl;
  }

  static Environment* Allocate(const Environment& env) {
    Environment* allocated_env = new Environment(env);
    envs_.push_back(allocated_env);
    return allocated_env;
  }

 private:
  static constexpr int kThisCode = 0;

  static const clang::ValueDecl* CanonicalVar(const clang::ValueDecl* decl) {
    return decl ? llvm::cast<clang::ValueDecl>(decl->getCanonicalDecl())
                : nullptr;
  }

  void set_live(size_t pos) {
    if (unreachable_) return;
    if (pos >= live_.size()) live_.resize(pos + 1);
    live_[pos] = true;
  }

  bool is_live(size_t pos) const {
    return unreachable_ || (live_.size() > pos && live_[pos]);
  }

  // Applies sequential environment updates produced within an expression (e.g.
  // `p = &obj->field_` or `p = &stack_var`). `raw_member_vars_cleared_` records
  // variables explicitly rebound to non-heap addresses inside the subexpression
  // so that `ApplyEffect` removes them from the outer environment's
  // `raw_member_vars_`.
  Environment& operator|=(const Environment& o) {
    if (o.unreachable_) {
      unreachable_ = true;
      live_.clear();
    } else if (!unreachable_) {
      for (size_t i = 0, e = o.live_.size(); i < e; ++i) {
        if (o.live_[i]) set_live(i);
      }
    }
    for (const clang::ValueDecl* d : o.raw_member_vars_cleared_) {
      raw_member_vars_.erase(d);
      raw_member_vars_cleared_.insert(d);
    }
    for (const clang::ValueDecl* d : o.raw_member_vars_) {
      raw_member_vars_.insert(d);
      raw_member_vars_cleared_.erase(d);
    }
    member_ref_vars_.insert(o.member_ref_vars_.begin(),
                            o.member_ref_vars_.end());
    return *this;
  }

  bool HasRawMemberOnBranch(const clang::ValueDecl* decl,
                            const Environment* base_env) const {
    if (raw_member_vars_.contains(decl)) return true;
    if (base_env != nullptr && base_env->raw_member_vars_.contains(decl) &&
        !raw_member_vars_cleared_.contains(decl)) {
      return true;
    }
    return false;
  }

  bool HasMemberRefOnBranch(const clang::ValueDecl* decl,
                            const Environment* base_env) const {
    return member_ref_vars_.contains(decl) ||
           (base_env != nullptr && base_env->member_ref_vars_.contains(decl));
  }

  // Returns true if `decl` is live on `*this`, holds a raw member address on
  // `*this` but not on `other`, is not unconditionally type-tracked (such as
  // `ExternalPointerSlot`, `EmbedderDataSlot`, or `TaggedMember<T>*`, which
  // are stale across GC regardless of provenance), and is not a member
  // reference on either branch (accounting for `base_env` when joining
  // expression deltas; `SlotBase` proxy references in both `raw_member_vars_`
  // and `member_ref_vars_` are also excluded here), so its liveness from
  // `*this` can be preserved across the join without masking a stale variable
  // on `other`.
  bool CanRescueOneSidedLive(const clang::ValueDecl* decl,
                             const Environment& other,
                             const Environment* base_env) const {
    return !IsTypeRawVar(decl) && !other.HasRawMemberOnBranch(decl, base_env) &&
           !HasMemberRefOnBranch(decl, base_env) &&
           !other.HasMemberRefOnBranch(decl, base_env) && IsAlive(decl);
  }

  // Control-flow join (`IfStmt`, `SwitchStmt`, loop headers/exits, `?:`,
  // `&&`, `||`): conservatively intersects liveness (`live_`) and unions member
  // provenance (`raw_member_vars_`, `member_ref_vars_`). When joining two
  // expression-level delta environments (`base_env != nullptr`), `base_env`
  // supplies the pre-branch provenance so variables that already held a raw
  // member address before the branch are not treated as newly initialized on a
  // single branch.
  void IntersectWith(const Environment& o,
                     const Environment* base_env = nullptr) {
    if (o.unreachable_) return;
    if (unreachable_) {
      *this = o;
      return;
    }

    // If a variable only holds a raw member address on one incoming path, take
    // its liveness from the path where it actually holds a member address so a
    // GC on the other path (where it is still nullptr/unassigned) does not
    // falsely mark it stale:
    //   uint8_t* ptr = nullptr;
    //   if (cond) {
    //     CauseGC();            // kills `ptr` while it is still nullptr
    //   } else {
    //     ptr = obj->chars();   // `ptr` becomes a live raw member address here
    //   }
    //   if (ptr) Use(*ptr);     // safe: `ptr` is live on the only path that
    //                           // gave it a raw member address
    llvm::SmallVector<const clang::ValueDecl*, 4> one_sided_live;
    for (const clang::ValueDecl* decl : raw_member_vars_) {
      if (CanRescueOneSidedLive(decl, o, base_env)) {
        one_sided_live.push_back(decl);
      }
    }
    for (const clang::ValueDecl* decl : o.raw_member_vars_) {
      if (o.CanRescueOneSidedLive(decl, *this, base_env)) {
        one_sided_live.push_back(decl);
      }
    }

    // Carry over false bits from the tail of o.live_, and reset all bits that
    // are not set in o.live_.
    size_t size = std::max(live_.size(), o.live_.size());
    if (size > live_.size()) live_.resize(size);
    for (size_t i = 0; i < size; ++i) {
      if (live_[i] && (i >= o.live_.size() || !o.live_[i])) live_[i] = false;
    }
    for (const clang::ValueDecl* decl : one_sided_live) {
      MDefine(decl);
    }

    raw_member_vars_.insert(o.raw_member_vars_.begin(),
                            o.raw_member_vars_.end());
    VarSet cleared;
    for (const clang::ValueDecl* d : raw_member_vars_cleared_) {
      if (o.raw_member_vars_cleared_.contains(d) &&
          !raw_member_vars_.contains(d)) {
        cleared.insert(d);
      }
    }
    raw_member_vars_cleared_ = std::move(cleared);
    member_ref_vars_.insert(o.member_ref_vars_.begin(),
                            o.member_ref_vars_.end());
  }

  Environment& operator&=(const Environment& o) {
    IntersectWith(o, nullptr);
    return *this;
  }

  static SymbolTable symbol_table_;
  static VarSet type_raw_vars_;
  static std::vector<Environment*> envs_;

  std::vector<bool> live_;
  VarSet raw_member_vars_;
  VarSet raw_member_vars_cleared_;
  VarSet member_ref_vars_;
  // unreachable_ == true implies live_.empty(), but still is_live(i) returns
  // true for all i.
  bool unreachable_ = false;

  friend class ExprEffect;
  friend class CallProps;
  friend class FunctionAnalyzer;
};

class CallProps {
 public:
  CallProps() : env_(nullptr) {}

  void SetEffect(int arg, ExprEffect in) {
    if (arg < 0 || arg >= kMaxNumberOfArguments) return;
    if (in.hasGC()) gc_.set(arg);
    if (in.hasRawDef()) raw_def_.set(arg);
    if (in.hasRawUse()) raw_use_.set(arg);
    if (in.hasOnHeapMemberLValue()) on_heap_member_lvalue_.set(arg);
    if (in.hasRawMemberAddress()) raw_member_addr_.set(arg);
    if (in.env() != nullptr) {
      env_args_.set(arg);
      if (env_ == nullptr) {
        env_ = in.env();
      } else {
        *env_ |= *in.env();
      }
    }
  }

  ExprEffect ComputeCumulativeEffect(bool result_is_raw,
                                     bool propagate_raw_def = false) {
    // Kill `raw_member_vars_` defined in a sibling of a GC-causing argument
    // (e.g. `Callee(p = &obj->field_, CauseGC())`) so `ApplyEffect` does not
    // resurrect `p` after the expression, while leaving a single sequenced
    // argument (`Callee((CauseGC(), p = &obj->field_))`) live.
    if (env_ != nullptr && gc_.any() &&
        (gc_.count() > 1 || (env_args_ & ~gc_).any())) {
      env_->ClearRawMemberVarsLive();
    }
    ExprEffect out = ExprEffect::NoneWithEnv(env_);
    if (gc_.any()) out.setGC();
    if (raw_use_.any()) out.setRawUse();
    // Propagate `kRawDef` upward when any child is an on-heap member lvalue or
    // raw member address, or when `propagate_raw_def` is set and any child has
    // `raw_def_` (`Parallel` / binary operators / array subscripts).
    // `VisitCallExpr` passes `propagate_raw_def = false` to preserve the
    // historical call boundary (by-value arguments are copied before the
    // callee body, and nested `Outer(Inner(*handle), CauseGC())` is not flagged
    // across call boundaries), whereas member addresses/references passed into
    // a call remain conservatively treated as `kRawDef` in case the callee
    // forwards or reads through them:
    //   Outer(p == &obj->field_, CauseGC());   // flagged
    //   arr[obj->int_field_] = CauseGC();      // flagged
    if (result_is_raw || (propagate_raw_def && raw_def_.any()) ||
        on_heap_member_lvalue_.any() || raw_member_addr_.any()) {
      out.setRawDef();
    }
    return out;
  }

  // Checks whether unsequenced sibling subexpressions (function arguments,
  // binary operator operands, array base + index) are safe to evaluate in any
  // order. Unsafe when one subexpression can cause GC and another produces or
  // accesses an unrooted on-heap value/member (`raw_def_`,
  // `on_heap_member_lvalue_`, `raw_member_addr_`) or reads an already-stale raw
  // variable (`raw_use_`), e.g.:
  //   Callee(*obj_handle, CauseGC());  // arg 1 is `raw_def_`, arg 2 is `gc_`
  //   so->Method(*CauseGC());          // receiver `so->` is `raw_def_`,
  //                                    // arg 1 is `gc_`
  //   obj->field_ = Allocate();        // LHS is `on_heap_member_lvalue_`,
  //                                    // RHS is `gc_` (conservatively flagged
  //                                    // even under C++17 `=` sequencing)
  //   Callee(&obj->field_, CauseGC()); // arg 1 is `raw_member_addr_`,
  //                                    // arg 2 is `gc_`
  //   arr[obj->int_field_] = CauseGC(); // LHS has `raw_def_` from loading
  //                                     // `obj->int_field_`, RHS is `gc_`
  //   arr[0] = CauseGC();              // `arr` is an already-stale pointer in
  //                                    // `raw_use_`, RHS is `gc_`
  bool IsSafe() {
    if (!gc_.any()) return true;
    std::bitset<kMaxNumberOfArguments> raw =
        (raw_def_ | raw_use_ | on_heap_member_lvalue_ | raw_member_addr_);
    if (!raw.any()) return true;
    bool result = gc_.count() == 1 && !((raw ^ gc_).any());
    return result;
  }

  bool HasOnHeapMemberAt(int arg) const {
    return arg >= 0 && arg < kMaxNumberOfArguments &&
           (on_heap_member_lvalue_.test(arg) || raw_member_addr_.test(arg));
  }
  bool HasOnHeapMember() const {
    return on_heap_member_lvalue_.any() || raw_member_addr_.any();
  }
  bool HasRawMemberAddress() const { return raw_member_addr_.any(); }
  bool HasRawMemberAddressAt(int arg) const {
    return arg >= 0 && arg < kMaxNumberOfArguments &&
           raw_member_addr_.test(arg);
  }
  bool HasOnHeapMemberLValueAt(int arg) const {
    return arg >= 0 && arg < kMaxNumberOfArguments &&
           on_heap_member_lvalue_.test(arg);
  }

 private:
  static const int kMaxNumberOfArguments = 64;
  std::bitset<kMaxNumberOfArguments> raw_def_;
  std::bitset<kMaxNumberOfArguments> raw_use_;
  std::bitset<kMaxNumberOfArguments> gc_;
  std::bitset<kMaxNumberOfArguments> on_heap_member_lvalue_;
  std::bitset<kMaxNumberOfArguments> raw_member_addr_;
  std::bitset<kMaxNumberOfArguments> env_args_;
  Environment* env_;
};

Environment::SymbolTable Environment::symbol_table_;
Environment::VarSet Environment::type_raw_vars_;
std::vector<Environment*> Environment::envs_;

// Combines two alternative branches of an expression (`cond ? a : b` or
// `a && b` / `a || b`).
ExprEffect ExprEffect::Merge(ExprEffect a, ExprEffect b,
                             const Environment& base_env) {
  Environment* a_env = a.env();
  Environment* b_env = b.env();
  Environment* out = nullptr;
  if (a_env != nullptr || b_env != nullptr) {
    out = Environment::Allocate(a_env ? *a_env : Environment());
    out->IntersectWith(b_env ? *b_env : Environment(), &base_env);
  }
  return ExprEffect(a.effect_ | b.effect_, out);
}

// Combines two sequentially evaluated subexpressions (`a` followed by `b`, e.g.
// in a comma operator `(a, b)` or initializer list `{a, b}`). Only `b`'s value
// provenance is kept as the result of the sequence, while `a`'s variable
// provenance updates (`raw_member_vars_` / `raw_member_vars_cleared_`) survive
// even if `b` triggers a GC and clears `live_`:
//   Foo{(p = &obj->field_), CauseGC(), *p};  // `p` stays in `raw_member_vars_`
//                                            // across `CauseGC()`, so `*p` is
//                                            // flagged as stale.
ExprEffect ExprEffect::MergeSeq(ExprEffect a, ExprEffect b) {
  Environment* a_env = a.env();
  Environment* b_env = b.env();
  Environment* out = nullptr;
  if (a_env != nullptr && b.hasGC()) {
    out = Environment::Allocate(*a_env);
    out->live_.clear();
    if (b_env != nullptr) *out |= *b_env;
  } else if (a_env != nullptr && b_env != nullptr) {
    out = Environment::Allocate(*a_env);
    *out |= *b_env;
  } else {
    out = (b_env == nullptr) ? a_env : b_env;
  }
  // WithoutProvenance: `a`'s value is discarded before `b` runs; only `b`'s
  // value provenance is the result of the sequence, while any member access in
  // `a` becomes `kRawDef`.
  return ExprEffect(a.WithoutProvenance().effect_ | b.effect_, out);
}

ExprEffect ExprEffect::Define(const clang::ValueDecl* var_decl,
                              bool has_raw_member_addr) {
  Environment* e = env();
  if (e == nullptr) {
    e = Environment::Allocate(Environment());
  }
  e->MDefine(var_decl);
  e->SetRawMemberVarInDelta(var_decl, has_raw_member_addr);
  return ExprEffect(effect_, e);
}

class FunctionAnalyzer {
 public:
  FunctionAnalyzer(clang::MangleContext* ctx, const KnownDecls& decls,
                   clang::DiagnosticsEngine& d, clang::SourceManager& sm)
      : ctx_(ctx), decls_(decls), d_(d), sm_(sm) {}

  ~FunctionAnalyzer() { delete ctx_; }

  // --------------------------------------------------------------------------
  // Expressions
  // --------------------------------------------------------------------------

  ExprEffect VisitExpr(clang::Expr* expr, const Environment& env) {
    if (expr == nullptr) {
      return ExprEffect::None();
    }
#define VISIT(type)                                                         \
  do {                                                                      \
    clang::type* concrete_expr = llvm::dyn_cast_or_null<clang::type>(expr); \
    if (concrete_expr != nullptr) {                                         \
      return Visit##type(concrete_expr, env);                               \
    }                                                                       \
  } while (0);

    VISIT(AbstractConditionalOperator);
    VISIT(AddrLabelExpr);
    VISIT(ArrayInitIndexExpr);
    VISIT(ArrayInitLoopExpr);
    VISIT(ArraySubscriptExpr);
    VISIT(BinaryOperator);
    VISIT(BlockExpr);
    VISIT(CallExpr);
    VISIT(CastExpr);
    VISIT(CharacterLiteral);
    VISIT(ChooseExpr);
    VISIT(CompoundLiteralExpr);
    VISIT(ConceptSpecializationExpr);
    VISIT(ConstantExpr);
    VISIT(CXXBindTemporaryExpr);
    VISIT(CXXBoolLiteralExpr);
    VISIT(CXXConstructExpr);
    VISIT(CXXDefaultArgExpr);
    VISIT(CXXDefaultInitExpr);
    VISIT(CXXDeleteExpr);
    VISIT(CXXDependentScopeMemberExpr);
    VISIT(CXXNewExpr);
    VISIT(CXXNoexceptExpr);
    VISIT(CXXNullPtrLiteralExpr);
    VISIT(CXXPseudoDestructorExpr);
    VISIT(CXXRewrittenBinaryOperator);
    VISIT(CXXScalarValueInitExpr);
    VISIT(CXXStdInitializerListExpr);
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
    VISIT(MaterializeTemporaryExpr);
    VISIT(MemberExpr);
    VISIT(OffsetOfExpr);
    VISIT(OpaqueValueExpr);
    VISIT(OverloadExpr);
    VISIT(PackExpansionExpr);
    VISIT(ParenExpr);
    VISIT(ParenListExpr);
    VISIT(PredefinedExpr);
    VISIT(RequiresExpr);
    VISIT(ShuffleVectorExpr);
    VISIT(SizeOfPackExpr);
    VISIT(SourceLocExpr);
    VISIT(StmtExpr);
    VISIT(StringLiteral);
    VISIT(SubstNonTypeTemplateParmExpr);
    VISIT(SubstNonTypeTemplateParmPackExpr);
    VISIT(TypeTraitExpr);
    VISIT(UnaryOperator);
    VISIT(UnaryExprOrTypeTraitExpr);
    VISIT(VAArgExpr);
#undef VISIT

    return ExprEffect::None();
  }

#define DECL_VISIT_EXPR(type) \
  ExprEffect Visit##type(clang::type* expr, const Environment& env)

#define IGNORE_EXPR(type)                                             \
  ExprEffect Visit##type(clang::type* expr, const Environment& env) { \
    return ExprEffect::None();                                        \
  }

  IGNORE_EXPR(AddrLabelExpr);
  IGNORE_EXPR(ArrayInitIndexExpr);
  IGNORE_EXPR(BlockExpr);
  IGNORE_EXPR(CharacterLiteral);
  IGNORE_EXPR(ChooseExpr);
  IGNORE_EXPR(CompoundLiteralExpr);
  IGNORE_EXPR(ConceptSpecializationExpr);
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
  IGNORE_EXPR(RequiresExpr);
  IGNORE_EXPR(ShuffleVectorExpr);
  IGNORE_EXPR(SizeOfPackExpr);
  IGNORE_EXPR(SourceLocExpr);
  IGNORE_EXPR(StmtExpr);
  IGNORE_EXPR(StringLiteral);
  IGNORE_EXPR(SubstNonTypeTemplateParmPackExpr);
  IGNORE_EXPR(TypeTraitExpr);
  IGNORE_EXPR(VAArgExpr);
  IGNORE_EXPR(GNUNullExpr);
  IGNORE_EXPR(OverloadExpr);

  DECL_VISIT_EXPR(ArrayInitLoopExpr) {
    return VisitExpr(expr->getSubExpr(), env);
  }

  DECL_VISIT_EXPR(CXXDefaultInitExpr) {
    return VisitExpr(expr->getExpr(), env);
  }

  DECL_VISIT_EXPR(CXXStdInitializerListExpr) {
    return VisitExpr(expr->getSubExpr(), env);
  }

  DECL_VISIT_EXPR(SubstNonTypeTemplateParmExpr) {
    return VisitExpr(expr->getReplacement(), env);
  }

  DECL_VISIT_EXPR(CXXRewrittenBinaryOperator) {
    return VisitExpr(expr->getSemanticForm(), env);
  }

  DECL_VISIT_EXPR(CXXThisExpr) {
    return Use(expr, expr->getType(), THIS,
               clang::FullSourceLoc(expr->getLocation(), sm_), env);
  }

  DECL_VISIT_EXPR(AbstractConditionalOperator) {
    ExprEffect cond_effect = VisitExpr(expr->getCond(), env);
    Environment after_cond = env.ApplyEffect(cond_effect);

    std::vector<GCScope> saved_scopes = state_.scopes;

    ExprEffect true_effect = VisitExpr(expr->getTrueExpr(), after_cond);
    std::vector<GCScope> true_scopes = state_.scopes;

    state_.scopes = saved_scopes;

    ExprEffect false_effect = VisitExpr(expr->getFalseExpr(), after_cond);
    std::vector<GCScope> false_scopes = state_.scopes;

    assert(true_scopes.size() == false_scopes.size());
    for (size_t i = 0; i < true_scopes.size(); ++i) {
      state_.scopes[i] = MergeScopes(true_scopes[i], false_scopes[i]);
    }

    return ExprEffect::MergeSeq(
        cond_effect, ExprEffect::Merge(true_effect, false_effect, after_cond));
  }

  DECL_VISIT_EXPR(ArraySubscriptExpr) {
    CallProps props;
    props.SetEffect(0, VisitExpr(expr->getBase(), env));
    props.SetEffect(1, VisitExpr(expr->getIdx(), env));
    if (!props.IsSafe()) ReportUnsafe(expr, BAD_EXPR_MSG);
    ExprEffect out =
        props.ComputeCumulativeEffect(RepresentsRawPointerType(expr->getType()),
                                      /*propagate_raw_def=*/true);
    // Subscripting an interior array or pointer yields an on-heap member
    // lvalue (`&obj->inline_buffer_[i]`, `&obj->values()[i]`). When the base is
    // a local stack array of raw member addresses (`int* ptrs[2] = {&obj->a_,
    // nullptr}; int* q = ptrs[0]`), keep `kRawMemberAddress` so
    // `CK_LValueToRValue` preserves the element's raw member address provenance
    // (excluding reference variables bound to an on-heap array, whose elements
    // are on-heap member lvalues).
    if (props.HasOnHeapMemberAt(0)) {
      const clang::Expr* raw_base = expr->getBase()->IgnoreParenImpCasts();
      const auto* base_ref = llvm::dyn_cast<clang::DeclRefExpr>(raw_base);
      if (base_ref != nullptr && raw_base->getType()->isArrayType() &&
          env.IsRawMemberVar(base_ref->getDecl()) &&
          !env.IsOnHeapMemberRefVar(base_ref->getDecl())) {
        out.setRawMemberAddress();
      } else {
        out.setOnHeapMemberLValue();
      }
    }
    return out;
  }

  bool IsRawPointerVar(clang::Expr* expr, const clang::ValueDecl** var_decl) {
    if (auto* decl_ref = llvm::dyn_cast<clang::DeclRefExpr>(expr)) {
      *var_decl = decl_ref->getDecl();
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
        return Sequential(expr, 2, exprs, env);

      case clang::BO_LAnd:
      case clang::BO_LOr: {
        ExprEffect lhs_effect = VisitExpr(lhs, env);
        Environment after_lhs = env.ApplyEffect(lhs_effect);
        ExprEffect rhs_effect = VisitExpr(rhs, after_lhs);
        // WithoutProvenance: logical `&&` and `||` always yield a `bool`
        // rvalue, not a member address or lvalue.
        return ExprEffect::MergeSeq(
                   lhs_effect,
                   ExprEffect::Merge(ExprEffect::None(), rhs_effect, after_lhs))
            .WithoutProvenance();
      }

      case clang::BO_Assign: {
        const clang::ValueDecl* var_decl = nullptr;
        // Builtin assignment to a non-reference local variable rebinds the
        // variable and updates whether it holds a raw member address:
        //   p = &obj->field_;   // defines `p` with raw member provenance
        //   p = &stack_var;     // defines `p` and clears raw member provenance
        // By contrast, assigning to a reference bound to an on-heap member
        // (`int& ref = obj->field_; ref = 42;`) writes through to the heap
        // object and must not redefine `ref` or treat the LHS as safe if `ref`
        // is already stale.
        if (IsRawPointerVar(lhs, &var_decl) &&
            !env.IsOnHeapMemberRefVar(var_decl)) {
          ExprEffect rhs_effect = VisitExpr(rhs, env);
          return rhs_effect.Define(var_decl, rhs_effect.hasRawMemberAddress());
        }
        return Parallel(expr, 2, exprs, env);
      }

      case clang::BO_Add:
      case clang::BO_Sub:
      case clang::BO_AddAssign:
      case clang::BO_SubAssign: {
        CallProps props;
        props.SetEffect(0, VisitExpr(lhs, env));
        props.SetEffect(1, VisitExpr(rhs, env));
        if (!props.IsSafe()) ReportUnsafe(expr, BAD_EXPR_MSG);
        ExprEffect out = props.ComputeCumulativeEffect(
            RepresentsRawPointerType(expr->getType()),
            /*propagate_raw_def=*/true);
        // Propagate raw member address provenance across pointer/address offset
        // arithmetic (`&obj->field_ + offset`, `addr - sizeof(Tagged_t)`,
        // `p += 1`, leaving `raw_member_vars_` unchanged on `+=`/`-=`), but not
        // across pointer or address differences (`end_ptr - &obj->field_`,
        // `addr_a - addr_b`), which yield a scalar distance:
        bool is_sub = expr->getOpcode() == clang::BO_Sub ||
                      expr->getOpcode() == clang::BO_SubAssign;
        bool propagates_raw_member_addr =
            is_sub ? (props.HasRawMemberAddressAt(0) &&
                      !props.HasRawMemberAddressAt(1) &&
                      !rhs->getType()->isPointerType())
                   : props.HasRawMemberAddress();
        if (propagates_raw_member_addr) {
          out.setRawMemberAddress();
        }
        return out;
      }

      default:
        return Parallel(expr, 2, exprs, env);
    }
  }

  DECL_VISIT_EXPR(CXXBindTemporaryExpr) {
    return VisitExpr(expr->getSubExpr(), env);
  }

  DECL_VISIT_EXPR(MaterializeTemporaryExpr) {
    return VisitExpr(expr->getSubExpr(), env);
  }

  // Evaluates brace-enclosed initializer elements (`InitListExpr` or
  // list-initialized `CXXConstructExpr`) in C++11+ strict left-to-right order.
  // An earlier element becomes dangling across a later element's GC if the
  // earlier element produced an on-heap member address/lvalue (`{&obj->a_,
  // (CauseGC(), &obj->b_)}`) or a raw on-heap value (`{*handle, CauseGC()}`),
  // whereas a scalar loaded by value in an earlier element (`Foo{obj->int_f_,
  // CauseGC()}`) is already copied before the GC runs.
  ExprEffect VisitSequencedInitList(clang::Expr* parent,
                                    llvm::ArrayRef<clang::Expr*> inits,
                                    const Environment& env) {
    ExprEffect out = ExprEffect::None();
    Environment out_env = env;
    bool has_raw_member_addr = false;
    bool saw_member_provenance = false;
    bool saw_unrooted_value = false;
    bool reported_unsafe = false;
    for (clang::Expr* init : inits) {
      ExprEffect elem = VisitExpr(init, out_env);
      if (elem.hasGC() && (saw_unrooted_value || saw_member_provenance) &&
          !reported_unsafe) {
        ReportUnsafe(parent, BAD_EXPR_MSG);
        reported_unsafe = true;
      }
      if (elem.hasMemberProvenance()) saw_member_provenance = true;
      if (init != nullptr && RepresentsRawPointerType(init->getType())) {
        saw_unrooted_value = true;
      }
      if (elem.hasRawMemberAddress()) has_raw_member_addr = true;
      out = ExprEffect::MergeSeq(out, elem);
      out_env = out_env.ApplyEffect(elem);
    }
    // WithoutProvenance: clear the last element's individual provenance
    // (`kOnHeapMemberLValue` from an element subexpression does not make the
    // aggregate itself an on-heap member lvalue) before setting the
    // aggregate's own provenance.
    out = out.WithoutProvenance();
    if (RepresentsRawPointerType(parent->getType())) {
      out.setRawDef();
    }
    const auto* ctor_expr = llvm::dyn_cast<clang::CXXConstructExpr>(parent);
    if ((llvm::isa<clang::InitListExpr>(parent) && has_raw_member_addr) ||
        (IsSlotType(parent->getType()) &&
         (saw_member_provenance ||
          (ctor_expr != nullptr &&
           IsAlwaysOnHeapSlotConstruction(ctor_expr))))) {
      out = out.WithProvenance(kRawMemberAddress);
    }
    return out;
  }

  DECL_VISIT_EXPR(CXXConstructExpr) {
    if (expr->isListInitialization()) {
      return VisitSequencedInitList(
          expr,
          llvm::ArrayRef<clang::Expr*>(expr->getArgs(), expr->getNumArgs()),
          env);
    }
    CallProps props;
    VisitArguments<>(expr, &props, env);
    if (!props.IsSafe()) ReportUnsafe(expr, BAD_EXPR_MSG);
    ExprEffect out = props.ComputeCumulativeEffect(
        RepresentsRawPointerType(expr->getType()));
    // Constructing a `SlotBase` subclass from an on-heap member or interior
    // address (or non-default constructing an always-on-heap slot such as
    // `EmbedderDataSlot(js_obj, 0)` or `ExternalPointerSlot(addr, tag)`)
    // produces a slot with raw member address provenance:
    //   ObjectSlot slot(&obj->field_);
    //   OffHeapObjectSlot off_heap_slot(&obj->compressed_field_);
    //   EmbedderDataSlot embedder_slot(js_obj, 0);
    // Constructing from a stack/handle address does not:
    //   FullObjectSlot stack_slot(&local_tagged);
    // TODO(crbug.com/571823300): Also track span/view wrappers constructed from
    // raw interior pointers (such as `base::Vector<T>(obj->chars(), n)`).
    if (IsSlotType(expr->getType()) &&
        (props.HasOnHeapMember() || IsAlwaysOnHeapSlotConstruction(expr))) {
      out.setRawMemberAddress();
    }
    return out;
  }

  DECL_VISIT_EXPR(CXXDefaultArgExpr) { return VisitExpr(expr->getExpr(), env); }

  DECL_VISIT_EXPR(CXXDeleteExpr) {
    // WithoutProvenance: `delete` has `void` type, so its result cannot carry
    // member provenance.
    return VisitExpr(expr->getArgument(), env).WithoutProvenance();
  }

  DECL_VISIT_EXPR(CXXNewExpr) {
    // WithoutProvenance: `new` returns a newly allocated C++ heap pointer
    // rather than an interior V8 on-heap member address.
    return VisitExpr(expr->getInitializer(), env).WithoutProvenance();
  }

  DECL_VISIT_EXPR(ExprWithCleanups) {
    return VisitExpr(expr->getSubExpr(), env);
  }

  DECL_VISIT_EXPR(CXXThrowExpr) {
    // WithoutProvenance: `throw` has `void` type, so its result cannot carry
    // member provenance.
    return VisitExpr(expr->getSubExpr(), env).WithoutProvenance();
  }

  DECL_VISIT_EXPR(ConstantExpr) { return VisitExpr(expr->getSubExpr(), env); }

  DECL_VISIT_EXPR(InitListExpr) {
    // In C++11+, brace-enclosed initializer lists evaluate their elements in
    // strict left-to-right order, and any raw member address in an element
    // (e.g. `uint8_t* ptrs[2] = {&obj->a_, nullptr}`) makes the initialized
    // aggregate hold a raw member address.
    return VisitSequencedInitList(
        expr,
        llvm::ArrayRef<clang::Expr*>(expr->getInits(), expr->getNumInits()),
        env);
  }

  DECL_VISIT_EXPR(MemberExpr) {
    ExprEffect out = VisitExpr(expr->getBase(), env);
    const clang::ValueDecl* member = expr->getMemberDecl();
    if (llvm::isa<clang::FieldDecl, clang::IndirectFieldDecl>(member)) {
      const auto* parent_record =
          llvm::dyn_cast<clang::CXXRecordDecl>(member->getDeclContext());
      const auto* field_record =
          member->getType()->getBaseElementTypeUnsafe()->getAsCXXRecordDecl();
      // Mark field access as an on-heap member lvalue when:
      //   1. `out.hasOnHeapMemberLValue()` or
      //      `(expr->isArrow() && out.hasRawMemberAddress())`: accessing a
      //      subfield of an interior struct/union (`obj->pair_.first`) or
      //      dereferencing a member pointer (`member_ptr->value_`).
      //   2. `IsOnHeapMemberRecord(field_record)`: the field itself is an
      //      on-heap `*Member` wrapper (`TaggedMember<T>`,
      //      `UnalignedValueMember<T>`, etc.).
      //   3. `IsOnHeapClassOrMember(parent_record)`: the field belongs to a
      //      `HeapObject` subclass or `*Member` class
      //      (`obj->primitive_field_`).
      if (out.hasOnHeapMemberLValue() ||
          (expr->isArrow() && out.hasRawMemberAddress()) ||
          IsOnHeapMemberRecord(field_record) ||
          IsOnHeapClassOrMember(parent_record)) {
        return out.WithProvenance(kOnHeapMemberLValue);
      }
      return out;
    }
    // WithoutProvenance: non-field members (methods, static data members, enum
    // constants) do not refer to an interior on-heap field of the base object.
    return out.WithoutProvenance();
  }

  DECL_VISIT_EXPR(OpaqueValueExpr) {
    return VisitExpr(expr->getSourceExpr(), env);
  }

  DECL_VISIT_EXPR(ParenExpr) { return VisitExpr(expr->getSubExpr(), env); }

  DECL_VISIT_EXPR(ParenListExpr) {
    return Parallel(expr, expr->getNumExprs(), expr->getExprs(), env);
  }

  DECL_VISIT_EXPR(UnaryOperator) {
    // TODO(gcmole): We are treating all expressions that look like
    // {&raw_pointer_var} as definitions of {raw_pointer_var}. This should be
    // changed to recognize less generic pattern:
    //
    //   if (maybe_object->ToObject(&obj)) return maybe_object;
    //
    // Excluded when `var_decl` is a reference bound to an on-heap member
    // (`int& ref = obj->field_; Foo(&ref);`), where `&ref` takes the interior
    // address of `obj->field_` rather than initializing `ref` as an out-param.
    // For non-reference variables, preserve `env.IsRawMemberVar(var_decl)` so
    // taking `&p` of an existing `int* p = &obj->field_` keeps `p` in
    // `raw_member_vars_` (and `ClearRawMemberVarsLive` kills `p` if the call
    // triggers a GC) while an uninitialized or null-initialized out-param
    // (`TaggedMember<T>* out = nullptr; Init(&out);`) stays out of
    // `raw_member_vars_` during a GC-capable out-param call.
    if (expr->getOpcode() == clang::UO_AddrOf) {
      const clang::ValueDecl* var_decl = nullptr;
      if (IsRawPointerVar(expr->getSubExpr(), &var_decl) &&
          !env.IsOnHeapMemberRefVar(var_decl)) {
        return ExprEffect::None().Define(var_decl,
                                         env.IsRawMemberVar(var_decl));
      }
    }
    ExprEffect out = VisitExpr(expr->getSubExpr(), env);
    switch (expr->getOpcode()) {
      case clang::UO_AddrOf:
        // `&obj->field_` turns an on-heap member lvalue into a raw member
        // address (`kOnHeapMemberLValue` -> `kRawMemberAddress`).
        return out.WithProvenance(
            out.hasOnHeapMemberLValue() ? kRawMemberAddress : kNoEffect);
      case clang::UO_Deref:
        // `*(&obj->field_)` or `*member_ptr` turns a raw member address back
        // into an on-heap member lvalue (`kRawMemberAddress` ->
        // `kOnHeapMemberLValue`).
        return out.WithProvenance(
            out.hasRawMemberAddress() ? kOnHeapMemberLValue : kNoEffect);
      case clang::UO_PreInc:
      case clang::UO_PreDec:
      case clang::UO_Plus:
      case clang::UO_Extension:
        // In C++, prefix `++obj->field_` / `--obj->field_` returns an lvalue
        // referring to `obj->field_` (or increments a pointer in place), and
        // unary `+p` / `__extension__` preserves the operand's provenance.
        return out;
      case clang::UO_PostInc:
      case clang::UO_PostDec:
        // Postfix `obj->field_++` evaluates to an rvalue copy of the old value
        // while mutating `obj->field_`: clear lvalue provenance and record
        // `kRawDef` so `Callee(obj->field_++, CauseGC())` is flagged.
        if (out.hasOnHeapMemberLValue()) {
          out = out.WithoutProvenance();
        }
        return out;
      default:
        // WithoutProvenance: arithmetic, bitwise, and logical unary operators
        // (`!`, `~`, `-`) compute a scalar or boolean rvalue rather than an
        // on-heap member address or lvalue.
        return out.WithoutProvenance();
    }
  }

  DECL_VISIT_EXPR(UnaryExprOrTypeTraitExpr) {
    if (expr->isArgumentType()) {
      return ExprEffect::None();
    }
    // WithoutProvenance: `sizeof` and `alignof` evaluate to a `size_t`
    // constant, not an on-heap member address or lvalue.
    return VisitExpr(expr->getArgumentExpr(), env).WithoutProvenance();
  }

  DECL_VISIT_EXPR(CastExpr) {
    ExprEffect out = VisitExpr(expr->getSubExpr(), env);
    switch (expr->getCastKind()) {
      case clang::CK_LValueToRValue:
      case clang::CK_LValueToRValueBitCast:
        // Loading an on-heap member by value (e.g. passing `obj->int_field_` by
        // value to `Callee(obj->int_field_, CauseGC())`) copies the value out
        // of the heap: the result is no longer an interior address/lvalue, but
        // the load itself is a raw heap access (`kRawDef`) that must not race
        // with an unsequenced GC in the same expression.
        if (out.hasOnHeapMemberLValue()) {
          out = out.WithoutProvenance();
        }
        break;
      case clang::CK_ArrayToPointerDecay:
        // Decaying an interior array field or `FLEXIBLE_ARRAY_MEMBER` accessor
        // to a pointer produces a raw interior address:
        //   uint8_t* bytes = obj->inline_bytes_;   // T[N] -> T*
        //   int* values = obj->values();           // T(&)[0] -> T*
        if (out.hasOnHeapMemberLValue()) {
          out = out.WithProvenance(kRawMemberAddress);
        }
        break;
      case clang::CK_NoOp:
      case clang::CK_ConstructorConversion:
      case clang::CK_UserDefinedConversion:
      case clang::CK_BitCast:
      case clang::CK_LValueBitCast:
      case clang::CK_UncheckedDerivedToBase:
      case clang::CK_DerivedToBase:
      case clang::CK_BaseToDerived:
      case clang::CK_Dynamic:
      case clang::CK_ToUnion:
      case clang::CK_PointerToIntegral:
      case clang::CK_IntegralToPointer:
      case clang::CK_AddressSpaceConversion:
        // Preserve provenance across casts that keep the same underlying
        // address or reference, e.g.:
        //   reinterpret_cast<uint8_t*>(&obj->field_)
        //   reinterpret_cast<Address>(&obj->field_)
        //   static_cast<const int&>(obj->field_)
        break;
      case clang::CK_IntegralCast:
        // Preserve provenance across pointer-width integer casts (e.g.
        // `static_cast<uintptr_t>(addr)`), but strip it on narrowing casts
        // (e.g. `static_cast<uint32_t>(addr)`).
        if (!expr->getType()->isDependentType() &&
            ctx_->getASTContext().getTypeSize(expr->getType()) <
                ctx_->getASTContext().getTypeSize(
                    ctx_->getASTContext().VoidPtrTy)) {
          out = out.WithoutProvenance();
        }
        break;
      default:
        // WithoutProvenance: casts that do not preserve an address, reference,
        // or pointer-width integer (e.g. `CK_PointerToBoolean`, `CK_ToVoid`,
        // `CK_FloatingCast`) do not yield an on-heap member address or lvalue.
        out = out.WithoutProvenance();
        break;
    }
    // Any non-null pointer expression of `*Member*` type (including
    // `reinterpret_cast<TaggedMember<Smi>*>(addr)` and `CK_LValueToRValue`
    // loads of `*Member*` variables or fields) points to an on-heap member.
    if (expr->getCastKind() != clang::CK_NullToPointer &&
        IsPointerToOnHeapMemberType(expr->getType())) {
      out.setRawMemberAddress();
    }
    return out;
  }

  DECL_VISIT_EXPR(DeclRefExpr) { return Use(expr, expr->getDecl(), env); }

  // Represents a node in the AST {parent} whose children {exprs} have
  // undefined order of evaluation, e.g. array subscript or a binary operator.
  ExprEffect Parallel(clang::Expr* parent, int n, clang::Expr** exprs,
                      const Environment& env) {
    CallProps props;
    for (int i = 0; i < n; ++i) {
      props.SetEffect(i, VisitExpr(exprs[i], env));
    }
    if (!props.IsSafe()) ReportUnsafe(parent, BAD_EXPR_MSG);
    return props.ComputeCumulativeEffect(
        RepresentsRawPointerType(parent->getType()),
        /*propagate_raw_def=*/true);
  }

  // Represents a node in the AST {parent} whose children {exprs} are
  // executed in sequence, e.g. a comma operator.
  ExprEffect Sequential(clang::Stmt* parent, int n, clang::Expr** exprs,
                        const Environment& env) {
    ExprEffect out = ExprEffect::None();
    Environment out_env = env;
    for (int i = 0; i < n; ++i) {
      ExprEffect elem = VisitExpr(exprs[i], out_env);
      out = ExprEffect::MergeSeq(out, elem);
      out_env = out_env.ApplyEffect(elem);
    }
    return out;
  }

  // Represents a node in the AST {parent} which uses the variable {var_decl}
  // (or `this` when `var_decl == nullptr`).
  // Here we observe the type in {var_type} (or tracked on-heap member
  // address/reference provenance on {var_decl} in {env}) of a previously
  // declared variable. If it represents a raw on-heap value or interior member
  // address/reference, we do the following:
  // 1. Propagate its member value effect (kRawMemberAddress /
  //    kOnHeapMemberLValue) in the returned ExprEffect.
  // 2. If it got stale due to GC since its declaration, report it and mark
  //    kRawUse.
  ExprEffect Use(const clang::Expr* parent, const clang::QualType& var_type,
                 const clang::ValueDecl* var_decl,
                 clang::FullSourceLoc var_location, const Environment& env) {
    ExprEffect out = ExprEffect::None();
    bool is_raw_member_addr = env.IsRawMemberVar(var_decl);
    bool is_on_heap_member_ref = env.IsOnHeapMemberRefVar(var_decl);
    if (is_raw_member_addr) out.setRawMemberAddress();
    if (is_on_heap_member_ref) out.setOnHeapMemberLValue();

    if (!g_dead_vars_analysis) return out;
    if (!RepresentsRawPointerType(var_type) && !is_raw_member_addr &&
        !is_on_heap_member_ref) {
      return out;
    }
    // Raw pointer tracking is enabled for HeapObject subclasses, internal
    // pointer/handle/member types, and variables holding interior member
    // addresses/references/slots to catch stale accesses across GC calls.
    if (env.IsAlive(var_decl)) return out;
    if (HasActiveGuard()) return out;
    if (HasActiveConservativePinning(var_location)) return out;
    ReportUnsafe(parent, DEAD_VAR_MSG);
    out.setRawUse();
    return out;
  }

  ExprEffect Use(const clang::Expr* parent, const clang::ValueDecl* var,
                 const Environment& env) {
    if (IsExternalVMState(var)) return ExprEffect::GC();
    if (const auto* var_decl = llvm::dyn_cast<clang::VarDecl>(var)) {
      // Environment only tracks liveness for local variables and parameters
      // declared inside the analyzed function. Ignore constexpr constants and
      // namespace/global-scope variables (such as kNullExternalPointerHandle or
      // kNullJSDispatchHandle), which cannot become stale across GCs and are
      // never registered in Environment.
      if (var_decl->isConstexpr() || !var_decl->isLocalVarDeclOrParm()) {
        return ExprEffect::None();
      }
    } else if (const auto* binding_decl =
                   llvm::dyn_cast<clang::BindingDecl>(var)) {
      const auto* decomp_decl = llvm::dyn_cast_or_null<clang::VarDecl>(
          binding_decl->getDecomposedDecl());
      if (!decomp_decl || decomp_decl->isConstexpr() ||
          !decomp_decl->isLocalVarDeclOrParm()) {
        return ExprEffect::None();
      }
    }
    return Use(parent, var->getType(), var,
               clang::FullSourceLoc(var->getLocation(), sm_), env);
  }

  template <typename ExprType>
  void VisitArguments(ExprType* call, CallProps* props,
                      const Environment& env) {
    for (unsigned arg = 0; arg < call->getNumArgs(); arg++) {
      props->SetEffect(arg + 1, VisitExpr(call->getArg(arg), env));
    }
  }

  // Checks whether `call` can transitively trigger a garbage collection and, if
  // so, marks `out->setGC()` and records the call site in the active `GCScope`.
  //
  // Resolution proceeds in two stages:
  //   1. CHA / Visitor synthetic node lookup (`IsVirtualOrVisitorCall`):
  //      For unqualified virtual calls on `v8::internal` classes and calls to
  //      `ObjectVisitor::VisitRelocInfo`, reconstructs the exact synthetic key
  //      (`VIRTUAL-<ReceiverClass>-<MangledMethod>` or
  //      `VIRTUALTHIS-<ReceiverClass>-<MangledMethod>`) emitted by
  //      `CalleesPrinter` in Pass 1 and checks whether `gcmole.py` propagated a
  //      GC suspect into that node. Always returns early because the synthetic
  //      node already accounts for all reachable overrides in `ReceiverClass`
  //      and its subclasses.
  //   2. Direct and fallback lookup:
  //      For direct non-virtual calls, explicitly qualified virtual calls
  //      (`Base::Foo()`), or calls outside `IsVirtualOrVisitorCall`, checks an
  //      exact mangled-name match (`IsKnownToCauseGC`). If `call` is still a
  //      virtual member call that was not intercepted by step 1, attempts
  //      Clang's static `getDevirtualizedMethod` first and falls back to an
  //      unmangled `Class::Method` name match (`IsSuspectedToCauseGC`).
  void CheckCallForGCCause(clang::CallExpr* call,
                           clang::CXXMemberCallExpr* memcall,
                           clang::FunctionDecl* callee, ExprEffect* out) {
    const clang::CXXRecordDecl* receiver_record = nullptr;
    const clang::CXXMethodDecl* method_decl = nullptr;
    bool is_this_call = false;
    if (IsVirtualOrVisitorCall(ctx_, call, &receiver_record, &method_decl,
                               &is_this_call)) {
      MangledName method_mangled;
      if (GetMangledName(ctx_, method_decl, &method_mangled)) {
        MangledName dummy_mangled;
        std::string receiver_name = GetRecordQualifiedName(receiver_record);
        if (is_this_call) {
          dummy_mangled = "VIRTUALTHIS-" + receiver_name + "-" + method_mangled;
        } else {
          dummy_mangled = "VIRTUAL-" + receiver_name + "-" + method_mangled;
        }

        LoadGCSuspects();
        LoadSuspectsAllowList();
        bool is_allowlisted = false;
        if (suspects_allowlist.find(method_decl->getNameAsString()) !=
            suspects_allowlist.end()) {
          is_allowlisted = true;
        }
        if (!is_allowlisted &&
            gc_suspects.find(dummy_mangled) != gc_suspects.end()) {
          out->setGC();
          state_.scopes.back().SetGCCauseLocation(
              clang::FullSourceLoc(call->getExprLoc(), sm_), callee,
              g_print_gc_call_chain ? std::move(dummy_mangled) : MangledName());
        }
      }
      return;  // Always return early for virtual calls!
    }

    if (IsKnownToCauseGC(ctx_, callee)) {
      out->setGC();
      state_.scopes.back().SetGCCauseLocation(
          clang::FullSourceLoc(call->getExprLoc(), sm_), callee);
    }

    // Support for virtual methods that might be GC suspects.
    if (memcall == nullptr) return;
    clang::CXXMethodDecl* method =
        llvm::dyn_cast_or_null<clang::CXXMethodDecl>(callee);
    if (method == nullptr) return;
    if (!method->isVirtual()) return;

    clang::CXXMethodDecl* target = method->getDevirtualizedMethod(
        memcall->getImplicitObjectArgument(), false);
    if (target != nullptr) {
      if (IsKnownToCauseGC(ctx_, target)) {
        out->setGC();
        state_.scopes.back().SetGCCauseLocation(
            clang::FullSourceLoc(call->getExprLoc(), sm_), target);
      }
    } else {
      // According to the documentation, {getDevirtualizedMethod} might
      // return nullptr, in which case we still want to use the partial
      // match of the {method}'s name against the GC suspects in order
      // to increase coverage.
      if (IsSuspectedToCauseGC(ctx_, method)) {
        out->setGC();
        state_.scopes.back().SetGCCauseLocation(
            clang::FullSourceLoc(call->getExprLoc(), sm_), method);
      }
    }
  }

  // Computes the value provenance (`kRawMemberAddress` or
  // `kOnHeapMemberLValue`) of a `CallExpr`'s return value:
  //   1. Calls returning a `*Member*` pointer (e.g. `TaggedMember<T>*`).
  //   2. Calls returning a `SlotBase` or `SlotBase::Reference` derived from an
  //      on-heap object or member, e.g.:
  //        `obj->RawField(offset)`, `obj->map_slot()`, `unaligned_slot[i]`,
  //        `slot + offset`
  //      (For `slot = rhs`, only the RHS `props.HasOnHeapMemberAt(2)`
  //      determines whether the assigned slot still points into the heap; for a
  //      proxy reference write `slot_ref = value`, the LHS
  //      `props.HasOnHeapMemberAt(1)` determines whether the returned
  //      `Reference&` still refers to the heap.)
  //   3. GLValue calls returning a reference to an on-heap `*Member` or a
  //      `FLEXIBLE_ARRAY_MEMBER` accessor returning `T(&)[0]`, e.g.:
  //        `obj->values()` (decays to `T*` or is subscripted as
  //        `obj->values()[i]`)
  //   4. Address-extracting, bit-casting, or forwarding helpers on an on-heap
  //      member/slot:
  //        `obj->field_.storage_address()`, `slot.address()`,
  //        `slot.location()`, `slot.handle_location()`, `slot.ToVoidPtr()`,
  //        `std::addressof(obj->field_)`, `base::bit_cast<Address>(ptr)`,
  //        `std::forward<T>(arg)`, `std::move(arg)`
  //      (TODO(crbug.com/571823300): raw `HeapObject::address()` /
  //      `Tagged::ptr()` arithmetic, arbitrary `T*`/`Address`-returning methods
  //      or forwarding wrappers, and `BindingDecl` structured bindings are not
  //      yet tracked as member provenance.)
  void ComputeCallProvenance(clang::CallExpr* call,
                             clang::CXXMemberCallExpr* memcall,
                             clang::CXXOperatorCallExpr* opcall,
                             clang::FunctionDecl* callee,
                             const CallProps& props, ExprEffect* out) {
    clang::QualType call_type = call->getType();
    if (!call_type.isNull()) {
      if (IsPointerToOnHeapMemberType(call_type)) {
        out->setRawMemberAddress();
      } else if (IsSlotType(call_type)) {
        bool is_assign =
            opcall != nullptr && opcall->getOperator() == clang::OO_Equal;
        bool from_member = is_assign
                               ? props.HasOnHeapMemberAt(
                                     IsSlotReferenceType(call_type) ? 1 : 2)
                               : props.HasOnHeapMember();
        if (from_member || (!is_assign && IsAlwaysOnHeapSlotType(call_type)) ||
            (memcall != nullptr &&
             IsOnHeapClassOrMember(memcall->getRecordDecl()))) {
          out->setRawMemberAddress();
        }
      } else if (call->isGLValue() &&
                 (IsOnHeapMemberRecord(call_type->getBaseElementTypeUnsafe()
                                           ->getAsCXXRecordDecl()) ||
                  // In C++, functions cannot return arrays by value, so a
                  // GLValue CallExpr has an ArrayType return type only when
                  // returning a reference to an array (e.g.
                  // FLEXIBLE_ARRAY_MEMBER accessors returning T(&)[0]).
                  (memcall != nullptr && call_type->isArrayType() &&
                   IsOnHeapClassOrMember(memcall->getRecordDecl())))) {
        out->setOnHeapMemberLValue();
      }
    }

    if (callee == nullptr || callee->getIdentifier() == nullptr) return;
    llvm::StringRef callee_name = callee->getName();

    if (memcall != nullptr && props.HasOnHeapMemberAt(0)) {
      // Receiver `props.HasOnHeapMemberAt(0)` is already a tracked `*Member` or
      // `SlotBase` instance, where these accessors return its interior address.
      static constexpr llvm::StringLiteral kAddressAccessors[] = {
          "storage_address", "address",         "ptr_location",
          "location",        "handle_location", "ToVoidPtr",
      };
      if (llvm::is_contained(kAddressAccessors, callee_name)) {
        out->setRawMemberAddress();
      }
    } else if (call->getNumArgs() == 1) {
      if ((props.HasOnHeapMemberAt(1) && callee_name == "addressof" &&
           callee->isInStdNamespace()) ||
          (props.HasRawMemberAddressAt(1) && callee_name == "bit_cast" &&
           (callee->isInStdNamespace() || InV8Namespace(callee)))) {
        out->setRawMemberAddress();
      } else if (callee->isInStdNamespace() &&
                 (callee_name == "forward" || callee_name == "move")) {
        if (props.HasRawMemberAddressAt(1)) {
          out->setRawMemberAddress();
        } else if (props.HasOnHeapMemberLValueAt(1)) {
          out->setOnHeapMemberLValue();
        }
      }
    }
  }

  // After visiting the receiver and the arguments of the {call} node, this
  // function might report a GC-unsafe usage (due to the undefined evaluation
  // order of the receiver and the rest of the arguments).
  ExprEffect VisitCallExpr(clang::CallExpr* call, const Environment& env) {
    CallProps props;

    clang::CXXMemberCallExpr* memcall =
        llvm::dyn_cast_or_null<clang::CXXMemberCallExpr>(call);
    if (memcall != nullptr) {
      clang::Expr* receiver = memcall->getImplicitObjectArgument();
      props.SetEffect(0, VisitExpr(receiver, env));
    }

    const clang::ValueDecl* var_decl = nullptr;
    clang::CXXOperatorCallExpr* opcall =
        llvm::dyn_cast_or_null<clang::CXXOperatorCallExpr>(call);
    // Overloaded `operator=` on a non-reference local variable (e.g.
    // `slot = obj->RawField(0)` or `tagged_var = ...`) redefines the variable
    // and updates its raw-member provenance. Excluded when `var_decl` is in
    // `member_ref_vars_` or is a `SlotBase` proxy reference
    // (`UnalignedSlot<T>::Reference`, `AtomicSlot::Reference`), where
    // `operator=` writes through the underlying slot address instead of
    // rebinding the variable.
    if (opcall != nullptr && opcall->getOperator() == clang::OO_Equal &&
        IsRawPointerVar(opcall->getArg(0), &var_decl) &&
        !env.IsOnHeapMemberRefVar(var_decl) &&
        !IsSlotReferenceType(var_decl->getType())) {
      // TODO(gcmole): We are treating all assignment operator calls with
      // the left hand side looking like {raw_pointer_var} as safe independent
      // of the concrete assignment operator implementation. This should be
      // changed to be more narrow only if the assignment operator of the base
      // {Object} or {HeapObject} class was used, which we know to be safe.
      // Index 2 is operator-call arg 1 (the RHS); index 1 (the LHS) is
      // intentionally not visited for simple assignment.
      ExprEffect rhs_effect = VisitExpr(call->getArg(1), env);
      props.SetEffect(
          2, rhs_effect.Define(var_decl, rhs_effect.hasRawMemberAddress()));
    } else {
      VisitArguments<>(call, &props, env);
    }

    if (!props.IsSafe()) ReportUnsafe(call, BAD_EXPR_MSG);

    ExprEffect out = props.ComputeCumulativeEffect(
        RepresentsRawPointerType(call->getType()));

    clang::FunctionDecl* callee = call->getDirectCallee();
    ComputeCallProvenance(call, memcall, opcall, callee, props, &out);
    if (callee == nullptr) return out;

    ExprEffect callee_effect = ExprEffect::None();
    CheckCallForGCCause(call, memcall, callee, &callee_effect);
    if (callee_effect.hasGC()) {
      out.setGC();
      // Kill `raw_member_vars_` defined in arguments of this call (e.g.
      // `CalleeThatGCs(p = &obj->field_)`), while keeping pre-existing
      // out-parameter definitions (`MaybeToObject(&obj)`) live.
      if (out.env() != nullptr) {
        out.env()->ClearRawMemberVarsLive();
      }
    }

    return out;
  }

  // --------------------------------------------------------------------------
  // Statements
  // --------------------------------------------------------------------------

  Environment VisitStmt(clang::Stmt* stmt, const Environment& env) {
    if (stmt == nullptr) return env;
#define VISIT(type)                                                         \
  do {                                                                      \
    clang::type* concrete_stmt = llvm::dyn_cast_or_null<clang::type>(stmt); \
    if (concrete_stmt != nullptr) {                                         \
      return Visit##type(concrete_stmt, env);                               \
    }                                                                       \
  } while (0);

    if (clang::Expr* expr = llvm::dyn_cast_or_null<clang::Expr>(stmt)) {
      return env.ApplyEffect(VisitExpr(expr, env));
    }

    VISIT(AsmStmt);
    VISIT(AttributedStmt);
    VISIT(BreakStmt);
    VISIT(CompoundStmt);
    VISIT(ContinueStmt);
    VISIT(CXXCatchStmt);
    VISIT(CXXForRangeStmt);
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

#define DECL_VISIT_STMT(type) \
  Environment Visit##type(clang::type* stmt, const Environment& env)

#define IGNORE_STMT(type)                                              \
  Environment Visit##type(clang::type* stmt, const Environment& env) { \
    return env;                                                        \
  }

  IGNORE_STMT(IndirectGotoStmt);
  IGNORE_STMT(NullStmt);
  IGNORE_STMT(AsmStmt);

  // We are ignoring unstructured control flow for simplicity.
  IGNORE_STMT(GotoStmt);

  // We are ignoring try/catch because V8 does not use them.
  IGNORE_STMT(CXXCatchStmt);
  IGNORE_STMT(CXXTryStmt);

  DECL_VISIT_STMT(LabelStmt) { return VisitStmt(stmt->getSubStmt(), env); }

  DECL_VISIT_STMT(AttributedStmt) { return VisitStmt(stmt->getSubStmt(), env); }

  class Block {
   public:
    Block(const Environment& in, FunctionAnalyzer* owner)
        : in_(in),
          out_(Environment::Unreachable()),
          changed_(false),
          owner_(owner) {
      parent_ = owner_->EnterBlock(this);
    }

    ~Block() { owner_->LeaveBlock(parent_); }

    void MergeIn(const Environment& env) {
      Environment old_in = in_;
      in_ &= env;
      changed_ = !old_in.Equal(in_);
    }

    bool changed() {
      if (!changed_) return false;
      changed_ = false;
      return true;
    }

    const Environment& in() { return in_; }

    const Environment& out() { return out_; }

    void MergeOut(const Environment& env) { out_ &= env; }

    void Sequential(clang::Stmt* a, clang::Stmt* b, clang::Stmt* c) {
      Environment a_out = owner_->VisitStmt(a, in());
      Environment b_out = owner_->VisitStmt(b, a_out);
      Environment c_out = owner_->VisitStmt(c, b_out);
      MergeOut(c_out);
    }

    void Sequential(clang::Stmt* a, clang::Stmt* b) {
      Environment a_out = owner_->VisitStmt(a, in());
      Environment b_out = owner_->VisitStmt(b, a_out);
      MergeOut(b_out);
    }

    void Loop(clang::Stmt* a, clang::Stmt* b, clang::Stmt* c) {
      Sequential(a, b, c);
      MergeIn(out());
    }

    void Loop(clang::Stmt* a, clang::Stmt* b) {
      Sequential(a, b);
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
    state_.block->MergeOut(env);
    return Environment::Unreachable();
  }

  DECL_VISIT_STMT(ContinueStmt) {
    state_.block->MergeIn(env);
    return Environment::Unreachable();
  }

  DECL_VISIT_STMT(CompoundStmt) {
    state_.scopes.push_back(GCScope());
    Environment out = env;
    clang::CompoundStmt::body_iterator end = stmt->body_end();
    for (clang::CompoundStmt::body_iterator s = stmt->body_begin(); s != end;
         ++s) {
      out = VisitStmt(*s, out);
    }
    PopScope();
    return out;
  }

  DECL_VISIT_STMT(WhileStmt) {
    state_.scopes.push_back(GCScope());
    Block block(env, this);
    do {
      block.Loop(stmt->getCond(), stmt->getBody());
    } while (block.changed());
    PopScope();
    return block.out();
  }

  DECL_VISIT_STMT(DoStmt) {
    Block block(env, this);

    // Special case `do { ... } while (false);`, which is known to only run
    // once, and is used in our (D)CHECK macros.
    if (auto* literal_cond =
            llvm::dyn_cast<clang::CXXBoolLiteralExpr>(stmt->getCond())) {
      if (literal_cond->getValue() == false) {
        block.Loop(stmt->getBody(), stmt->getCond());
        return block.out();
      }
    }

    do {
      block.Loop(stmt->getBody(), stmt->getCond());
    } while (block.changed());
    return block.out();
  }

  DECL_VISIT_STMT(ForStmt) {
    state_.scopes.push_back(GCScope());
    Block block(VisitStmt(stmt->getInit(), env), this);
    do {
      block.Loop(stmt->getCond(), stmt->getBody(), stmt->getInc());
    } while (block.changed());
    PopScope();
    return block.out();
  }

  // Desugared range-for loop (`for (init; decl : range) body`): visits the
  // synthetic `__range`, `__begin`, and `__end` declarations before iterating
  // `cond` (`__begin != __end`), `loop_var` (`decl = *__begin`), `body`, and
  // `inc` (`++__begin`). If `range` is an on-heap array/accessor and `body`
  // triggers a GC, `__begin` is flagged as stale at the `for` header on the
  // next iteration.
  DECL_VISIT_STMT(CXXForRangeStmt) {
    state_.scopes.push_back(GCScope());
    Environment init_env = VisitStmt(stmt->getInit(), env);
    init_env = VisitStmt(stmt->getRangeStmt(), init_env);
    init_env = VisitStmt(stmt->getBeginStmt(), init_env);
    init_env = VisitStmt(stmt->getEndStmt(), init_env);
    Block block(init_env, this);
    do {
      Environment cond_out = VisitStmt(stmt->getCond(), block.in());
      Environment var_out = VisitStmt(stmt->getLoopVarStmt(), cond_out);
      Environment body_out = VisitStmt(stmt->getBody(), var_out);
      Environment inc_out = VisitStmt(stmt->getInc(), body_out);
      block.MergeOut(inc_out);
      block.MergeIn(block.out());
    } while (block.changed());
    PopScope();
    return block.out();
  }

  DECL_VISIT_STMT(IfStmt) {
    state_.scopes.push_back(GCScope());
    Environment init_out = VisitStmt(stmt->getInit(), env);
    Environment cond_out = VisitStmt(stmt->getCond(), init_out);

    std::vector<GCScope> saved_scopes = state_.scopes;

    Environment then_out = VisitStmt(stmt->getThen(), cond_out);
    std::vector<GCScope> then_scopes = state_.scopes;

    state_.scopes = saved_scopes;

    Environment else_out = VisitStmt(stmt->getElse(), cond_out);
    std::vector<GCScope> else_scopes = state_.scopes;

    assert(then_scopes.size() == else_scopes.size());
    for (size_t i = 0; i < then_scopes.size(); ++i) {
      state_.scopes[i] = MergeScopes(then_scopes[i], else_scopes[i]);
    }

    PopScope();
    return Environment::Merge(then_out, else_out);
  }

  DECL_VISIT_STMT(SwitchStmt) {
    state_.scopes.push_back(GCScope());
    Block block(env, this);
    block.Sequential(stmt->getCond(), stmt->getBody());
    PopScope();
    return block.out();
  }

  DECL_VISIT_STMT(CaseStmt) {
    Environment in = Environment::Merge(env, state_.block->in());
    Environment after_lhs = VisitStmt(stmt->getLHS(), in);
    return VisitStmt(stmt->getSubStmt(), after_lhs);
  }

  DECL_VISIT_STMT(DefaultStmt) {
    Environment in = Environment::Merge(env, state_.block->in());
    return VisitStmt(stmt->getSubStmt(), in);
  }

  DECL_VISIT_STMT(ReturnStmt) {
    VisitExpr(stmt->getRetValue(), env);
    return Environment::Unreachable();
  }

  const clang::TagType* ToTagType(const clang::Type* t) {
    if (t == nullptr) {
      return nullptr;
    } else if (llvm::isa<clang::TagType>(t)) {
      return llvm::cast<clang::TagType>(t);
    } else if (llvm::isa<clang::SubstTemplateTypeParmType>(t)) {
      return ToTagType(llvm::cast<clang::SubstTemplateTypeParmType>(t)
                           ->getReplacementType()
                           .getTypePtr());
    } else {
      return nullptr;
    }
  }

  bool IsDerivedFrom(const clang::CXXRecordDecl* record,
                     const clang::CXXRecordDecl* base) {
    return (record == base) || record->isDerivedFrom(base);
  }

  const clang::CXXRecordDecl* GetDefinitionOrNull(
      const clang::CXXRecordDecl* record) {
    assert(record);
    if (!InV8Namespace(record)) return nullptr;
    if (!record->hasDefinition()) return nullptr;
    return record->getDefinition();
  }

  // Matches `Tagged<T>` specializations that point to heap objects (excluding
  // non-moving values `Tagged<Smi>`, `Tagged<TaggedIndex>`, and
  // `Tagged<ClearedWeakValue>`).
  bool IsTaggedPointer(const clang::CXXRecordDecl* record) {
    if (record == nullptr) return false;
    if (!InV8Namespace(record)) return false;
    auto* specialization =
        llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(record);
    if (specialization) {
      auto* template_decl =
          specialization->getSpecializedTemplate()->getCanonicalDecl();
      if (template_decl == decls_.tagged) {
        auto& template_args = specialization->getTemplateArgs();
        if (template_args.size() != 1) {
          llvm::errs() << "v8::internal::Tagged<T> should have exactly one "
                          "template argument\n";
          specialization->dump(llvm::errs());
          return false;
        }
        if (template_args[0].getKind() != clang::TemplateArgument::Type) {
          llvm::errs()
              << "v8::internal::Tagged<T>, T should be a type argument\n";
          specialization->dump(llvm::errs());
          return false;
        }

        auto* tagged_type_record =
            template_args[0].getAsType()->getAsCXXRecordDecl();
        return tagged_type_record != decls_.smi &&
               tagged_type_record != decls_.tagged_index &&
               tagged_type_record != decls_.cleared_weak_value;
      }
    }
    return false;
  }

  // By-value table handle types (stored as indices into pointer/dispatch tables
  // whose entries can be reclaimed on GC):
  //   JSDispatchHandle, ExternalPointerHandle, CppHeapPointerHandle,
  //   TrustedPointerHandle
  bool IsTableHandleType(clang::QualType qtype) {
    if (qtype.isNull()) return false;
    const clang::CXXRecordDecl* record = qtype->getAsCXXRecordDecl();
    if (record && decls_.js_dispatch_handle &&
        record->getCanonicalDecl() == decls_.js_dispatch_handle) {
      return true;
    }
    clang::QualType current = qtype;
    while (const auto* td_type = current->getAs<clang::TypedefType>()) {
      const clang::TypedefNameDecl* decl =
          td_type->getDecl()->getCanonicalDecl();
      if ((decls_.external_pointer_handle &&
           decl == decls_.external_pointer_handle) ||
          (decls_.cpp_heap_pointer_handle &&
           decl == decls_.cpp_heap_pointer_handle) ||
          (decls_.trusted_pointer_handle &&
           decl == decls_.trusted_pointer_handle)) {
        return true;
      }
      current = decl->getUnderlyingType();
    }
    return false;
  }

  // Matches `SlotBase<Subclass, Data, Alignment>` and any derived slot classes
  // (`ObjectSlot`, `FullObjectSlot`, `OffHeapObjectSlot`, `MaybeObjectSlot`,
  // `InstructionStreamSlot`, `UnalignedSlot<T>`, `AtomicSlot`, etc.).
  bool IsSlotBaseSubclass(const clang::CXXRecordDecl* record) {
    if (record == nullptr || decls_.slot_base == nullptr) return false;
    const clang::CXXRecordDecl* canonical = record->getCanonicalDecl();
    auto cache_it = slot_base_cache_.find(canonical);
    if (cache_it != slot_base_cache_.end()) return cache_it->second;

    bool result = false;
    if (const auto* spec =
            llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(record)) {
      if (spec->getSpecializedTemplate()->getCanonicalDecl() ==
          decls_.slot_base) {
        result = true;
      }
    }
    if (!result) {
      if (const clang::CXXRecordDecl* def = GetDefinitionOrNull(record)) {
        for (const auto& base : def->bases()) {
          if (IsSlotBaseSubclass(base.getType()->getAsCXXRecordDecl())) {
            result = true;
            break;
          }
        }
      }
    }
    slot_base_cache_[canonical] = result;
    return result;
  }

  // Nested proxy reference classes inside `SlotBase` subclasses (such as
  // `UnalignedSlot<T>::Reference` and `AtomicSlot::Reference`) store an
  // interior slot address, and their `operator=` writes through that address
  // into the heap rather than rebinding the proxy object:
  //   UnalignedSlot<int> slot(&obj->field_);
  //   auto ref = slot[0];  // UnalignedSlot<int>::Reference
  //   CauseGC();
  //   ref = 42;            // flagged: writes through stale slot address
  bool IsSlotReferenceRecord(const clang::CXXRecordDecl* record) {
    if (record == nullptr || decls_.slot_base == nullptr ||
        record->getIdentifier() == nullptr ||
        record->getName() != "Reference") {
      return false;
    }
    if (const auto* parent_record =
            llvm::dyn_cast<clang::CXXRecordDecl>(record->getDeclContext())) {
      return IsSlotBaseSubclass(parent_record) && !IsSlotBaseSubclass(record);
    }
    return false;
  }

  // `SlotBase` subclasses (`ObjectSlot`, `FullObjectSlot`, `OffHeapObjectSlot`,
  // etc.) are tracked by provenance (when constructed from or returned by an
  // on-heap object/member, e.g. `ObjectSlot(&obj->field_)` or
  // `obj->RawField(offset)`): when pointer compression is disabled,
  // `ObjectSlot` is a type alias for `FullObjectSlot`, which is also used to
  // wrap stack/handle addresses (e.g. `FullObjectSlot(&stack_var)`). Note that
  // `ExternalPointerSlot`, `CppHeapPointerSlot`, `TrustedPointerSlot`, and
  // `EmbedderDataSlot` are never used to wrap stack handles and remain
  // unconditionally tracked in `IsRawPointerToOnHeapValue`.
  bool IsSlotRecord(const clang::CXXRecordDecl* record) {
    return IsSlotBaseSubclass(record) || IsSlotReferenceRecord(record);
  }

  bool IsSlotType(clang::QualType qtype) {
    if (qtype.isNull()) return false;
    return IsSlotRecord(qtype.getNonReferenceType()
                            ->getBaseElementTypeUnsafe()
                            ->getAsCXXRecordDecl());
  }

  bool IsSlotReferenceType(clang::QualType qtype) {
    if (qtype.isNull()) return false;
    return IsSlotReferenceRecord(qtype.getNonReferenceType()
                                     ->getBaseElementTypeUnsafe()
                                     ->getAsCXXRecordDecl());
  }

  // `SlotBase` subclasses that can only ever point into the V8 heap (never
  // wrapping stack/handle addresses) and are unconditionally tracked in
  // `IsRawPointerToOnHeapValue`:
  //   ExternalPointerSlot, CppHeapPointerSlot, TrustedPointerSlot,
  //   EmbedderDataSlot
  bool IsAlwaysOnHeapSlotRecord(const clang::CXXRecordDecl* record) {
    if (record == nullptr) return false;
    const clang::CXXRecordDecl* canonical = record->getCanonicalDecl();
    return (decls_.external_pointer_slot &&
            canonical == decls_.external_pointer_slot) ||
           (decls_.cpp_heap_pointer_slot &&
            canonical == decls_.cpp_heap_pointer_slot) ||
           (decls_.trusted_pointer_slot &&
            canonical == decls_.trusted_pointer_slot) ||
           (decls_.embedder_data_slot &&
            canonical == decls_.embedder_data_slot);
  }

  bool IsAlwaysOnHeapSlotType(clang::QualType qtype) {
    if (qtype.isNull()) return false;
    return IsAlwaysOnHeapSlotRecord(qtype.getNonReferenceType()
                                        ->getBaseElementTypeUnsafe()
                                        ->getAsCXXRecordDecl());
  }

  bool IsAlwaysOnHeapSlotConstruction(const clang::CXXConstructExpr* expr) {
    return expr->getNumArgs() > 0 &&
           !expr->getConstructor()->isCopyOrMoveConstructor() &&
           IsAlwaysOnHeapSlotType(expr->getType());
  }

  // Matches wrapper types used to declare fields inside `HeapObject` classes
  // (including derived wrappers such as `SmiBitFieldMember<Group>`):
  //   TaggedMember<T>, ExternalPointerMember<tag>,
  //   TrustedPointerMember<T, tag>, UnalignedValueMember<T>,
  //   JSDispatchHandleMember, CppHeapPointerMember, UnalignedDoubleMember
  bool IsOnHeapMemberRecord(const clang::CXXRecordDecl* record) {
    if (record == nullptr || !InV8Namespace(record)) return false;

    const clang::CXXRecordDecl* canonical = record->getCanonicalDecl();
    auto cache_it = on_heap_member_cache_.find(canonical);
    if (cache_it != on_heap_member_cache_.end()) return cache_it->second;

    bool result = false;
    if ((decls_.js_dispatch_handle_member &&
         canonical == decls_.js_dispatch_handle_member) ||
        (decls_.cpp_heap_pointer_member &&
         canonical == decls_.cpp_heap_pointer_member) ||
        (decls_.unaligned_double_member &&
         canonical == decls_.unaligned_double_member)) {
      result = true;
    } else if (auto* specialization =
                   llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(
                       record)) {
      auto* template_decl =
          specialization->getSpecializedTemplate()->getCanonicalDecl();
      if ((decls_.tagged_member && template_decl == decls_.tagged_member) ||
          (decls_.external_pointer_member &&
           template_decl == decls_.external_pointer_member) ||
          (decls_.trusted_pointer_member &&
           template_decl == decls_.trusted_pointer_member) ||
          (decls_.unaligned_value_member &&
           template_decl == decls_.unaligned_value_member)) {
        result = true;
      }
    }
    if (!result) {
      if (const clang::CXXRecordDecl* def = GetDefinitionOrNull(record)) {
        for (const auto& base : def->bases()) {
          if (IsOnHeapMemberRecord(base.getType()->getAsCXXRecordDecl())) {
            result = true;
            break;
          }
        }
      }
    }
    on_heap_member_cache_[canonical] = result;
    return result;
  }

  bool IsOnHeapClassOrMember(const clang::CXXRecordDecl* record) {
    if (record == nullptr || !InV8Namespace(record)) return false;
    const clang::CXXRecordDecl* canonical = record->getCanonicalDecl();
    auto cache_it = on_heap_class_or_member_cache_.find(canonical);
    if (cache_it != on_heap_class_or_member_cache_.end()) {
      return cache_it->second;
    }
    bool result = false;
    if (IsOnHeapMemberRecord(record)) {
      result = true;
    } else if (GetDefinitionOrNull(record) != nullptr) {
      result = IsDerivedFrom(record, decls_.heap_object);
    }
    on_heap_class_or_member_cache_[canonical] = result;
    return result;
  }

  bool IsPointerToOnHeapMemberType(clang::QualType qtype) {
    if (qtype.isNull()) return false;
    if (const auto* ptr_type = qtype->getAs<clang::PointerType>()) {
      return IsOnHeapMemberRecord(ptr_type->getPointeeType()
                                      ->getBaseElementTypeUnsafe()
                                      ->getAsCXXRecordDecl());
    }
    return false;
  }

  bool IsReferenceToOnHeapMemberType(clang::QualType qtype) {
    if (qtype.isNull()) return false;
    if (const auto* ref_type = qtype->getAs<clang::ReferenceType>()) {
      return IsOnHeapMemberRecord(ref_type->getPointeeType()
                                      ->getBaseElementTypeUnsafe()
                                      ->getAsCXXRecordDecl());
    }
    return false;
  }

  bool IsRawPointerToOnHeapValue(const clang::CXXRecordDecl* record) {
    if (record == nullptr) return false;
    return IsOnHeapClassOrMember(record) || IsAlwaysOnHeapSlotRecord(record);
  }

  bool IsOnHeapValue(const clang::CXXRecordDecl* record) {
    return IsTaggedPointer(record) || IsRawPointerToOnHeapValue(record);
  }

  bool IsRawPointerType(const clang::PointerType* type) {
    const clang::CXXRecordDecl* record = type->getPointeeCXXRecordDecl();
    bool result = IsRawPointerToOnHeapValue(record);
    TRACE("is raw " << result << " "
                    << (record ? record->getNameAsString() : "nullptr"));
    return result;
  }

  bool IsInternalPointerType(clang::QualType qtype) {
    if (IsTableHandleType(qtype)) {
      return true;
    }
    const clang::CXXRecordDecl* record = qtype->getAsCXXRecordDecl();
    bool result = IsOnHeapValue(record);
    TRACE_LLVM_TYPE("is internal " << result, qtype);
    return result;
  }

  // Returns weather the given type is a raw pointer or a wrapper around
  // such. For V8 that means Object and MaybeObject instances.
  bool RepresentsRawPointerType(clang::QualType qtype) {
    // Not yet assigned pointers can't get moved by the GC.
    if (qtype.isNull()) return false;
    // nullptr can't get moved by the GC.
    if (qtype->isNullPtrType()) return false;

    const clang::PointerType* pointer_type =
        llvm::dyn_cast_or_null<clang::PointerType>(qtype.getTypePtrOrNull());
    if (pointer_type != nullptr) {
      return IsRawPointerType(pointer_type);
    } else {
      return IsInternalPointerType(qtype);
    }
  }

  bool IsGCGuard(clang::QualType qtype) {
    return IsSameType(qtype, decls_.no_gc_mole);
  }

  bool IsConservativePinningScope(clang::QualType qtype) {
    return IsSameType(qtype, decls_.conservative_pinning_scope);
  }

  Environment VisitDecl(clang::Decl* decl, Environment& env) {
    if (clang::VarDecl* var = llvm::dyn_cast<clang::VarDecl>(decl)) {
      bool is_param = llvm::isa<clang::ParmVarDecl>(decl);
      ExprEffect init_effect = ExprEffect::None();
      Environment out = env;
      // For `ParmVarDecl`, `hasInit()` is true when the parameter has a default
      // argument expression (`void Foo(int x = Expr())`), which is evaluated at
      // the call site (`CXXDefaultArgExpr`) rather than on entry to the
      // function body.
      if (!is_param && var->hasInit()) {
        init_effect = VisitExpr(var->getInit(), env);
        out = env.ApplyEffect(init_effect);
      }

      clang::QualType declared_type = var->getType();
      clang::QualType var_type = declared_type;
      if (is_param && var_type->isReferenceType()) {
        var_type = var_type->getAs<clang::ReferenceType>()->getPointeeType();
      }

      // Record raw member address provenance (`raw_member_vars_`) when:
      //   1. The initializer evaluates to an interior heap address or slot:
      //        int* p = &obj->field_;
      //        uint8_t* bytes = obj->inline_bytes_;
      //        Address addr = obj->field_.storage_address();
      //        ObjectSlot slot(&obj->field_);
      //        ObjectSlot slot = obj->RawField(offset);
      //        EmbedderDataSlot slot(js_obj, 0);
      //   2. A parameter has a `*Member*` pointer type or an always-on-heap
      //      slot type (`void Foo(TaggedMember<T>* p)`,
      //      `void Bar(const EmbedderDataSlot& slot)`).
      //      (Local `*Member*` variables without an initializer are already
      //      tracked as type-raw pointers via `RepresentsRawPointerType` and
      //      acquire `kRawMemberAddress` when loaded via `CastExpr`, keeping
      //      uninitialized out-params like `TaggedMember<T>* out; Init(&out);`
      //      compatible with the `UO_AddrOf` out-parameter heuristic.)
      bool has_raw_member_addr =
          init_effect.hasRawMemberAddress() ||
          (is_param && (IsPointerToOnHeapMemberType(declared_type) ||
                        IsAlwaysOnHeapSlotType(var_type)));
      out.SetRawMemberVar(var, has_raw_member_addr);

      // Record on-heap member reference provenance (`member_ref_vars_`) when:
      //   1. A C++ reference is bound to an on-heap member lvalue:
      //        int& ref = obj->field_;
      //        const TaggedMember<Object>& mref = obj->tagged_field_;
      //   2. A `SlotBase` proxy reference (`UnalignedSlot<T>::Reference`,
      //      `AtomicSlot::Reference`) is initialized from an on-heap slot:
      //        auto ref = unaligned_slot[i];
      //   3. A parameter or variable is declared as a reference to an on-heap
      //      `*Member` type (`TaggedMember<T>&`):
      //        void Bar(TaggedMember<Object>& member_ref);
      bool is_on_heap_member_ref =
          (declared_type->isReferenceType() &&
           init_effect.hasOnHeapMemberLValue()) ||
          (has_raw_member_addr && IsSlotReferenceType(declared_type)) ||
          IsReferenceToOnHeapMemberType(declared_type);
      out.SetMemberRefVar(var, is_on_heap_member_ref);

      bool is_type_raw = RepresentsRawPointerType(var_type);
      if (is_type_raw) {
        Environment::MarkTypeRawVar(var);
      }
      if (has_raw_member_addr || is_on_heap_member_ref || is_type_raw) {
        out.MDefine(var);
      }
      if (IsGCGuard(var_type)) {
        state_.scopes.back().guard_location =
            clang::FullSourceLoc(decl->getLocation(), sm_);
      }
      if (IsConservativePinningScope(var_type)) {
        state_.scopes.back().conservative_pinning_scope_location =
            clang::FullSourceLoc(decl->getLocation(), sm_);
      }

      if (auto* decomp = llvm::dyn_cast<clang::DecompositionDecl>(var)) {
        for (clang::BindingDecl* binding : decomp->flat_bindings()) {
          if (clang::VarDecl* holding_var = binding->getHoldingVar()) {
            if (holding_var->hasInit()) {
              out = VisitStmt(holding_var->getInit(), out);
            }
          } else if (clang::Expr* binding_expr = binding->getBinding()) {
            out = VisitStmt(binding_expr, out);
          }

          clang::QualType binding_type = binding->getType();
          if (RepresentsRawPointerType(binding_type)) {
            Environment::MarkTypeRawVar(binding);
            out.MDefine(binding);
          }
          if (IsGCGuard(binding_type)) {
            state_.scopes.back().guard_location =
                clang::FullSourceLoc(binding->getLocation(), sm_);
          }
          if (IsConservativePinningScope(binding_type)) {
            state_.scopes.back().conservative_pinning_scope_location =
                clang::FullSourceLoc(binding->getLocation(), sm_);
          }
        }
      }

      return out;
    }
    // TODO(gcmole): handle other declarations?
    return env;
  }

  DECL_VISIT_STMT(DeclStmt) {
    Environment out = env;
    clang::DeclStmt::decl_iterator end = stmt->decl_end();
    for (clang::DeclStmt::decl_iterator decl = stmt->decl_begin(); decl != end;
         ++decl) {
      out = VisitDecl(*decl, out);
    }
    return out;
  }

  void DefineAndVisitParameters(const clang::FunctionDecl* f,
                                Environment& env) {
    env.MDefineThis();
    clang::FunctionDecl::param_const_iterator end = f->param_end();
    for (clang::FunctionDecl::param_const_iterator p = f->param_begin();
         p != end; ++p) {
      env = VisitDecl(*p, env);
    }
  }

  void AnalyzeFunction(const clang::FunctionDecl* f) {
    const clang::FunctionDecl* body = nullptr;
    if (f->hasBody(body)) {
      Environment env;
      state_.scopes.push_back(GCScope());
      DefineAndVisitParameters(body, env);
      VisitStmt(body->getBody(), env);
      PopScope();
      Environment::ClearSymbolTable();
    }
  }

  Block* EnterBlock(Block* block) {
    Block* parent = state_.block;
    state_.block = block;
    return parent;
  }

  void LeaveBlock(Block* block) { state_.block = block; }

  bool HasActiveGuard() {
    for (const auto& s : state_.scopes) {
      if (s.IsBeforeGCCause()) return true;
    }
    return false;
  }

  bool HasActiveConservativePinning(
      const clang::FullSourceLoc decl_location) const {
    for (const auto& s : state_.scopes) {
      if (s.IsAfterConservativePinningScope(decl_location)) return true;
    }
    return false;
  }

 private:
  bool IsSameType(clang::QualType qtype, clang::CXXRecordDecl* decl) {
    if (!decl) return false;
    if (qtype.isNull() || qtype->isNullPtrType()) return false;

    const clang::CXXRecordDecl* record = qtype->getAsCXXRecordDecl();
    if (!record) return false;

    return record->getCanonicalDecl() == decl->getCanonicalDecl();
  }

  // Prints the hierarchical GC call chain from `gccauses` when
  // `--print-gc-call-chain` is enabled, starting from the GC-causing call site
  // in the analyzed function and annotating synthetic devirtualization nodes
  // with `(virtual)` (`VIRTUAL-` / `VIRTUALTHIS-`) and `(override)`
  // (`SYNTHOVER-` or concrete targets reached from a `(virtual)` dispatch
  // node):
  //
  //   note: └── (virtual) Derived2::Helper
  //   note:     └── (override) Derived2::Helper
  //   note:         └── (virtual) Derived2::Hook
  //   note:             └── (override) Derived2::Hook
  //   note:                 └── Heap::CollectGarbage
  //   note:                     └── <GC> [Heap Allocation / Collect Garbage]
  void PrintGCCauseTree(const std::string& mangled,
                        std::set<std::string>& visited, int depth = 0,
                        bool is_caller_virtual = false) {
    if (depth > 12) return;
    std::string indent = "";
    for (int i = 0; i < depth; ++i) indent += "    ";

    if (mangled == "<GC>") {
      std::cout << "note: " << indent
                << "└── <GC> [Heap Allocation / Collect Garbage]\n";
      return;
    }
    if (mangled == "<Safepoint>") {
      std::cout << "note: " << indent << "└── <Safepoint>\n";
      return;
    }

    auto name_it = demangled_names.find(mangled);
    std::string disp_name =
        (name_it != demangled_names.end()) ? name_it->second : mangled;

    size_t pos = disp_name.find("v8::internal::");
    if (pos != std::string::npos) {
      disp_name.replace(pos, 14, "");
    }

    // Strip ugly anonymous namespace TU suffixes (like $_usr_local_...)
    size_t tu_pos = disp_name.rfind("$_");
    if (tu_pos != std::string::npos) {
      size_t end_pos = disp_name.find("::", tu_pos);
      if (end_pos != std::string::npos) {
        if (end_pos - tu_pos > 10) {
          disp_name.erase(tu_pos, end_pos - tu_pos);
        }
      } else {
        if (disp_name.length() - tu_pos > 10) {
          disp_name.erase(tu_pos);
        }
      }
    }

    // Construct semantic prefixes
    std::string prefix = "";
    bool is_virtual = (mangled.compare(0, 8, "VIRTUAL-") == 0) ||
                      (mangled.compare(0, 12, "VIRTUALTHIS-") == 0);
    if (is_virtual) {
      prefix = "(virtual) ";
    } else if ((mangled.compare(0, 10, "SYNTHOVER-") == 0) ||
               is_caller_virtual) {
      prefix = "(override) ";
    }

    disp_name = prefix + disp_name;

    if (visited.find(mangled) != visited.end()) {
      std::cout << "note: " << indent << "└── " << disp_name
                << " [Recursion Cycle]\n";
      return;
    }

    std::cout << "note: " << indent << "└── " << disp_name << "\n";

    auto it = gc_causes.find(mangled);
    if (it == gc_causes.end() || it->second.empty()) {
      return;
    }

    visited.insert(mangled);
    for (const auto& callee : it->second) {
      PrintGCCauseTree(callee, visited, depth + 1, is_virtual);
    }
    visited.erase(mangled);  // backtrack
  }

  void ReportUnsafe(const clang::Expr* expr, const std::string& msg) {
    clang::SourceLocation error_loc =
        clang::FullSourceLoc(expr->getExprLoc(), sm_);
    d_.Report(error_loc,
              d_.getCustomDiagID(clang::DiagnosticsEngine::Warning, "%0"))
        << msg;
    // Find the relevant GC scope (see HasActiveGuard).
    const GCScope* pscope = nullptr;
    for (auto ri = state_.scopes.rbegin(); ri != state_.scopes.rend(); ++ri) {
      if (!ri->IsBeforeGCCause() && ri->gccause_location.isValid()) {
        pscope = &(*ri);
        break;
      }
    }
    if (!pscope) {
      d_.Report(error_loc,
                d_.getCustomDiagID(clang::DiagnosticsEngine::Note,
                                   "Could not find GC source location."));
      return;
    }
    const GCScope& scope = *pscope;
    d_.Report(scope.gccause_location,
              d_.getCustomDiagID(clang::DiagnosticsEngine::Note,
                                 "Call might cause unexpected GC."));
    clang::FunctionDecl* gccause_decl = scope.gccause_decl;
    d_.Report(
        clang::FullSourceLoc(gccause_decl->getBeginLoc(), sm_),
        d_.getCustomDiagID(clang::DiagnosticsEngine::Note, "GC call here."));

    if (!g_print_gc_call_chain) return;
    LoadGCCauses();
    MangledName name = scope.gccause_mangled;
    if (name.empty() && !GetMangledName(ctx_, gccause_decl, &name)) return;
    std::cout << "Potential GC call chain:\n";
    std::set<std::string> visited;
    PrintGCCauseTree(name, visited);
  }



  struct GCScope {
    clang::FullSourceLoc guard_location;
    clang::FullSourceLoc gccause_location;
    clang::FullSourceLoc conservative_pinning_scope_location;
    clang::FunctionDecl* gccause_decl = nullptr;
    MangledName gccause_mangled;

    // We're only interested in guards that are declared before any further GC
    // causing calls (see TestGuardedDeadVarAnalysisMidFunction for example).
    bool IsBeforeGCCause() const {
      if (!guard_location.isValid()) return false;
      if (!gccause_location.isValid()) return true;
      return guard_location.isBeforeInTranslationUnitThan(gccause_location);
    }

    bool IsAfterConservativePinningScope(
        const clang::FullSourceLoc decl_location) const {
      if (!conservative_pinning_scope_location.isValid()) return false;
      return conservative_pinning_scope_location.isBeforeInTranslationUnitThan(
          decl_location);
    }

    void SetGCCauseLocation(clang::FullSourceLoc gccause_location_,
                            clang::FunctionDecl* decl,
                            MangledName mangled = "") {
      gccause_location = gccause_location_;
      gccause_decl = decl;
      gccause_mangled = std::move(mangled);
    }
  };

  GCScope MergeScopes(const GCScope& a, const GCScope& b) {
    GCScope merged = a;
    if (a.gccause_location.isValid() && b.gccause_location.isValid()) {
      if (a.gccause_location.isBeforeInTranslationUnitThan(
              b.gccause_location)) {
        merged.gccause_location = b.gccause_location;
        merged.gccause_decl = b.gccause_decl;
        merged.gccause_mangled = b.gccause_mangled;
      } else {
        merged.gccause_location = a.gccause_location;
        merged.gccause_decl = a.gccause_decl;
        merged.gccause_mangled = a.gccause_mangled;
      }
    } else if (b.gccause_location.isValid()) {
      merged.gccause_location = b.gccause_location;
      merged.gccause_decl = b.gccause_decl;
      merged.gccause_mangled = b.gccause_mangled;
    }
    return merged;
  }

  void PopScope() {
    if (state_.scopes.size() > 1) {
      GCScope& inner = state_.scopes.back();
      GCScope& outer = state_.scopes[state_.scopes.size() - 2];
      if (inner.gccause_location.isValid()) {
        outer.gccause_location = inner.gccause_location;
        outer.gccause_decl = inner.gccause_decl;
        outer.gccause_mangled = inner.gccause_mangled;
      }
    }
    state_.scopes.pop_back();
  }

  struct AnalysisState {
    std::vector<GCScope> scopes;
    Block* block = nullptr;
  };

  clang::MangleContext* ctx_;
  KnownDecls decls_;
  clang::DiagnosticsEngine& d_;
  clang::SourceManager& sm_;

  AnalysisState state_;
  std::map<const clang::CXXRecordDecl*, bool> slot_base_cache_;
  std::map<const clang::CXXRecordDecl*, bool> on_heap_member_cache_;
  std::map<const clang::CXXRecordDecl*, bool> on_heap_class_or_member_cache_;
};

class ProblemsFinder : public clang::ASTConsumer,
                       public clang::RecursiveASTVisitor<ProblemsFinder> {
 public:
  ProblemsFinder(clang::DiagnosticsEngine& d, clang::SourceManager& sm,
                 const std::vector<std::string>& args)
      : d_(d), sm_(sm), function_analyzer_(nullptr) {
    for (unsigned i = 0; i < args.size(); ++i) {
      if (args[i].compare(0, 11, "output-dir:") == 0) {
        g_output_dir = args[i].substr(11);
      }
      if (args[i] == "--dead-vars") {
        g_dead_vars_analysis = true;
      }
      if (args[i] == "--verbose-trace") g_tracing_enabled = true;
      if (args[i] == "--verbose") g_verbose = true;
      if (args[i] == "--print-gc-call-chain") g_print_gc_call_chain = true;
    }
  }

  ~ProblemsFinder() override { delete function_analyzer_; }

  bool TranslationUnitIgnored() {
    if (!ignored_files_loaded_) {
      auto fileOrError =
          llvm::MemoryBuffer::getFile("tools/gcmole/ignored_files");
      if (auto error = fileOrError.getError()) {
        llvm::errs() << "Failed to open ignored_files file\n";
        std::terminate();
      }
      for (llvm::line_iterator it(*fileOrError->get()); !it.is_at_end(); ++it) {
        ignored_files_.insert(*it);
      }
      ignored_files_loaded_ = true;
    }

    clang::FileID main_file_id = sm_.getMainFileID();
    auto file_entry = sm_.getFileEntryForID(main_file_id);
    if (!file_entry) return false;
    llvm::StringRef filename = file_entry->tryGetRealPathName();

    bool result = false;
    for (const auto& entry : ignored_files_) {
      if (filename.ends_with(entry.getKey())) {
        result = true;
        break;
      }
    }
    if (result) {
      llvm::outs() << "Ignoring file " << filename << "\n";
    }
    return result;
  }

  void HandleTranslationUnit(clang::ASTContext& ctx) override {
    if (TranslationUnitIgnored()) return;

    KnownDecls decls = KnownDecls::Resolve(ctx);
    if (decls.HasRequiredDecls()) {
      function_analyzer_ = new FunctionAnalyzer(
          clang::ItaniumMangleContext::create(ctx, d_), decls, d_, sm_);
      TraverseDecl(ctx.getTranslationUnitDecl());
    } else if (g_verbose) {
      if (decls.heap_object == nullptr) {
        llvm::errs() << "Failed to resolve v8::internal::HeapObject\n";
      }
      if (decls.smi == nullptr) {
        llvm::errs() << "Failed to resolve v8::internal::Smi\n";
      }
      if (decls.tagged_index == nullptr) {
        llvm::errs() << "Failed to resolve v8::internal::TaggedIndex\n";
      }
      if (decls.tagged == nullptr) {
        llvm::errs() << "Failed to resolve v8::internal::Tagged<T>\n";
      }
    }
  }

  virtual bool VisitFunctionDecl(clang::FunctionDecl* decl) {
    // Don't print tracing from includes, otherwise the output is too big.
    bool tracing = g_tracing_enabled;
    const auto& fileID = sm_.getFileID(decl->getLocation());
    if (fileID != sm_.getMainFileID()) {
      g_tracing_enabled = false;
    }

    TRACE("Visiting function " << decl->getNameAsString());
    function_analyzer_->AnalyzeFunction(decl);

    g_tracing_enabled = tracing;
    return true;
  }

 private:
  clang::DiagnosticsEngine& d_;
  clang::SourceManager& sm_;

  bool ignored_files_loaded_ = false;
  llvm::StringSet<> ignored_files_;

  FunctionAnalyzer* function_analyzer_;
};

template <typename ConsumerType>
class Action : public clang::PluginASTAction {
 protected:
  std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
      clang::CompilerInstance& CI, llvm::StringRef InFile) override {
    return std::unique_ptr<clang::ASTConsumer>(
        new ConsumerType(CI.getDiagnostics(), CI.getSourceManager(), args_));
  }

  bool ParseArgs(const clang::CompilerInstance& CI,
                 const std::vector<std::string>& args) override {
    args_ = args;
    return true;
  }

  void PrintHelp(llvm::raw_ostream& ros) {}

 private:
  std::vector<std::string> args_;
};

}  // namespace

static clang::FrontendPluginRegistry::Add<Action<ProblemsFinder>> FindProblems(
    "find-problems", "Find GC-unsafe places.");

static clang::FrontendPluginRegistry::Add<Action<FunctionDeclarationFinder>>
    DumpCallees("dump-callees", "Dump callees for each function.");

#undef TRACE
#undef TRACE_LLVM_TYPE
#undef TRACE_LLVM_DECL
#undef DECL_VISIT_EXPR
#undef IGNORE_EXPR
#undef DECL_VISIT_STMT
#undef IGNORE_STMT
