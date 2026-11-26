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
#include <compare>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <stack>
#include <tuple>

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
#include "llvm/ADT/DenseMap.h"
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
static bool IsQualifiedMemberCall(const clang::CXXMemberCallExpr* memcall) {
  if (memcall == nullptr) return false;
  const auto* member_expr =
      llvm::dyn_cast<clang::MemberExpr>(memcall->getCallee()->IgnoreParens());
  return member_expr != nullptr && member_expr->hasQualifier();
}

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
          // Explicitly qualified calls (e.g. `this->Base::Foo()` or
          // `obj->Base::Foo()`) bypass dynamic dispatch in C++ and always
          // invoke `Base::Foo` directly.
          if (IsQualifiedMemberCall(memcall)) return false;
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
  clang::CXXRecordDecl* handle_base = nullptr;
  clang::CXXRecordDecl* direct_handle_base = nullptr;
  clang::CXXRecordDecl* api_indirect_handle_base = nullptr;
  clang::CXXRecordDecl* api_direct_handle_base = nullptr;
  clang::CXXRecordDecl* no_gc_mole = nullptr;
  clang::CXXRecordDecl* conservative_pinning_scope = nullptr;

  static KnownDecls Resolve(clang::ASTContext& ctx) {
    Resolver v8_ns = Resolver(ctx).ResolveNamespace("v8");
    Resolver v8_internal = v8_ns.ResolveNamespace("internal");
    Resolver v8_api_internal = v8_ns.ResolveNamespace("api_internal");
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
    decls.handle_base = v8_internal.ResolveDefinition("HandleBase");
    decls.direct_handle_base =
        v8_internal.ResolveDefinition("DirectHandleBase");
    decls.api_indirect_handle_base =
        v8_api_internal.ResolveDefinition("IndirectHandleBase");
    decls.api_direct_handle_base =
        v8_api_internal.ResolveDefinition("DirectHandleBase");
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
  // (`TraverseVarHelper` skips `getInit()` for `isCXXForRangeDecl()`, so
  // `TraverseDeclStmtInits` visits `*__begin` while `TraverseStmt` visits
  // `VisitVarDecl` for `DisableGCMole` loop variables.)
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

  bool TraverseScopedBranch(clang::Stmt* branch) {
    if (!branch || llvm::isa<clang::CompoundStmt>(branch)) {
      return TraverseStmt(branch);
    }
    gc_scopes_.push_back(GCScope());
    bool result = TraverseStmt(branch);
    gc_scopes_.pop_back();
    return result;
  }

  // Skips discarded branches of non-dependent `if constexpr` statements and
  // isolates `then`/`else` `GCScope`s so Pass 1 (`gcsuspects`) matches Pass 2
  // (`FunctionAnalyzer::VisitIfStmt`).
  bool TraverseIfStmt(clang::IfStmt* stmt) {
    if (!stmt->isConsteval()) {
      if (!TraverseStmt(stmt->getInit())) return false;
      if (!TraverseStmt(stmt->getConditionVariableDeclStmt())) return false;
      if (!TraverseStmt(stmt->getCond())) return false;
    }
    if (std::optional<clang::Stmt*> taken =
            stmt->getNondiscardedCase(ctx_->getASTContext())) {
      return TraverseScopedBranch(*taken);
    }
    return TraverseScopedBranch(stmt->getThen()) &&
           TraverseScopedBranch(stmt->getElse());
  }

  // Isolates the unbraced substatement scope of `do body while (cond);` so a
  // `DisableGCMole` declared as `body` cannot guard `cond`.
  bool TraverseDoStmt(clang::DoStmt* stmt) {
    return TraverseScopedBranch(stmt->getBody()) &&
           TraverseStmt(stmt->getCond());
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
      case clang::Stmt::DoStmtClass:
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
// Value effects (kOnHeapMemberLValue, kRawMemberAddress,
// kIndirectRawMemberAddress) describe the provenance and value category of the
// current expression's result itself:
//   - kOnHeapMemberLValue: glvalue referring to an interior field/element of an
//     on-heap object, e.g.:
//       `obj->field_`, `obj->values()[i]`, `*(&obj->field_)`,
//       or a reference `int& ref = obj->field_;`
//   - kRawMemberAddress: pointer, `Address`, `SlotBase`, or local aggregate/
//     array directly holding an interior address into an on-heap object, e.g.:
//       `&obj->field_`, `std::addressof(obj->field_)`, `obj->values()`,
//       `obj->field_.storage_address()`, `ObjectSlot(&obj->field_)`,
//       `obj->RawField(offset)`, `int* ptrs[2] = {&obj->field_, nullptr}`
//   - kIndirectRawMemberAddress: off-heap pointer to a local array or local
//     aggregate whose elements/fields hold raw member addresses (e.g. decayed
//     `int* ptrs[2]` -> `int**`, or `&const_box` -> `const PtrBox*`), so
//     dereferencing/subscripting (`*ptrs`, `arr[0]`, `boxes->ptr`) yields
//     `kRawMemberAddress` rather than `kOnHeapMemberLValue` (modeling a single
//     level of off-heap container/pointer indirection).
// Value effects are transformed or cleared as the expression is addressed
// (`&` -> kRawMemberAddress / kIndirectRawMemberAddress), dereferenced
// (`*` -> kOnHeapMemberLValue / kRawMemberAddress), decayed
// (`T[N]` -> kRawMemberAddress / kIndirectRawMemberAddress), or loaded by
// value (LValueToRValue -> kRawDef without kOnHeapMemberLValue).
static const int kNoEffect = 0;
static const int kCausesGC = 1 << 0;
static const int kRawDef = 1 << 1;
static const int kRawUse = 1 << 2;
static const int kOnHeapMemberLValue = 1 << 3;
static const int kRawMemberAddress = 1 << 4;
static const int kIndirectRawMemberAddress = 1 << 5;
static const int kRawMemberEffects =
    kRawMemberAddress | kIndirectRawMemberAddress;
static const int kValueEffects = kOnHeapMemberLValue | kRawMemberEffects;
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

  bool hasIndirectRawMemberAddress() const {
    return (effect_ & kIndirectRawMemberAddress) != 0;
  }
  void setIndirectRawMemberAddress() { effect_ |= kIndirectRawMemberAddress; }

  uint8_t rawMemberProvenance() const {
    return static_cast<uint8_t>(effect_ & kRawMemberEffects);
  }

  bool hasMemberProvenance() const { return (effect_ & kValueEffects) != 0; }

  ExprEffect WithProvenance(int provenance) const {
    return ExprEffect((effect_ & kCumulativeEffects) | provenance, env_);
  }

  // Strips value provenance (`kOnHeapMemberLValue`, `kRawMemberAddress`,
  // `kIndirectRawMemberAddress`) when an expression produces a result that is
  // no longer an on-heap member lvalue or address, converting any consumed
  // member provenance into `kRawDef` so enclosing unsequenced expressions still
  // see the heap access.
  ExprEffect WithoutProvenance() const {
    return ExprEffect(
        (effect_ & kCumulativeEffects) | (hasMemberProvenance() ? kRawDef : 0),
        env_);
  }

  // Strips only `kOnHeapMemberLValue` when loading an on-heap member lvalue by
  // value (such as `CK_LValueToRValue` or postfix `++`/`--`), converting it
  // into `kRawDef` while preserving `kRawMemberAddress` /
  // `kIndirectRawMemberAddress` for dual-provenance variables (e.g.
  // `int*& r = cond ? stack_ptr : obj->ptr_field_;`).
  ExprEffect WithoutOnHeapMemberLValue() const {
    if (!hasOnHeapMemberLValue()) return *this;
    return ExprEffect((effect_ & ~kOnHeapMemberLValue) | kRawDef, env_);
  }

  // Drops value provenance and `kRawDef` while preserving `kCausesGC`,
  // `kRawUse`, and `env`. Used when an expression is evaluated only for its
  // side effects and its value is discarded (such as the base expression of a
  // static member function call `p->StaticMethod()`).
  ExprEffect WithoutValueAndRawDef() const {
    return ExprEffect(effect_ & (kCausesGC | kRawUse), env_);
  }

  static ExprEffect None() { return ExprEffect(kNoEffect, nullptr); }
  static ExprEffect NoneWithEnv(Environment* env) {
    return ExprEffect(kNoEffect, env);
  }

  static ExprEffect Merge(ExprEffect a, ExprEffect b,
                          const Environment& base_env);
  static ExprEffect MergeSeq(ExprEffect a, ExprEffect b);
  ExprEffect Define(const clang::ValueDecl* var_decl,
                    uint8_t raw_member_provenance);

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
const std::string ESCAPED_MEMBER_ADDR_MSG(
    "Address of on-heap member passed to a function that might cause GC.");

class Environment {
 public:
  using VarSet = llvm::SmallPtrSet<const clang::ValueDecl*, 4>;
  using RawMemberMap = llvm::SmallDenseMap<const clang::ValueDecl*, uint8_t, 4>;
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
  // object (`kRawMemberAddress`) or a pointer to a local array/aggregate
  // holding raw member addresses (`kIndirectRawMemberAddress`), updating
  // provenance as the variable is assigned or rebound:
  //   int* p = &obj->field_;  // SetRawMemberProvenance(p, kRawMemberAddress)
  //   CauseGC();
  //   *p = 42;                // flagged: `p` is in `raw_member_vars_` and dead
  //   p = &stack_int;         // SetRawMemberProvenanceInDelta(p, 0)
  //   CauseGC();
  //   *p = 42;                // safe: `p` was rebound to a non-heap address
  void SetRawMemberProvenance(const clang::ValueDecl* decl,
                              uint8_t raw_member_provenance) {
    const clang::ValueDecl* canonical = CanonicalVar(decl);
    if (!canonical) return;
    raw_member_provenance &= kRawMemberEffects;
    if (raw_member_provenance != 0) {
      raw_member_vars_[canonical] = raw_member_provenance;
    } else {
      raw_member_vars_.erase(canonical);
    }
  }

  // Updates `decl`'s raw-member provenance inside an expression-level delta
  // environment (`ExprEffect::Define`), recording reassignment in
  // `raw_member_vars_cleared_` so `operator|=` / `ApplyEffect` replaces any
  // pre-existing bits from the enclosing statement-level environment (where
  // `raw_member_vars_cleared_` is always empty) rather than merging stale bits.
  void SetRawMemberProvenanceInDelta(const clang::ValueDecl* decl,
                                     uint8_t raw_member_provenance) {
    const clang::ValueDecl* canonical = CanonicalVar(decl);
    if (!canonical) return;
    raw_member_provenance &= kRawMemberEffects;
    raw_member_vars_cleared_.insert(canonical);
    if (raw_member_provenance != 0) {
      raw_member_vars_[canonical] = raw_member_provenance;
    } else {
      raw_member_vars_.erase(canonical);
    }
  }

  // Clears all liveness bits in `live_` except those set in `keep_live` (used
  // when a sibling argument or callee triggers GC so only variables sequenced
  // after a single sibling GC or passed as direct `&out` parameters to a
  // GC-causing callee remain live).
  void KeepOnlyLive(const std::vector<bool>& keep_live) {
    if (unreachable_) return;
    for (size_t i = 0; i < live_.size(); ++i) {
      if (i >= keep_live.size() || !keep_live[i]) {
        live_[i] = false;
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

  uint8_t GetRawMemberProvenance(const clang::ValueDecl* decl) const {
    const clang::ValueDecl* canonical = CanonicalVar(decl);
    if (!canonical) return 0;
    auto it = raw_member_vars_.find(canonical);
    return it != raw_member_vars_.end() ? it->second : 0;
  }

  bool IsRawMemberVar(const clang::ValueDecl* decl) const {
    return GetRawMemberProvenance(decl) != 0;
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
    for (const auto& entry : raw_member_vars_) {
      if (comma) std::cout << ", ";
      std::cout << entry.first->getNameAsString();
      if (entry.second & kIndirectRawMemberAddress) {
        std::cout << "(indirect)";
      }
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
  // variables explicitly reassigned inside the subexpression so `ApplyEffect`
  // replaces pre-existing bits from the outer environment rather than merging
  // stale provenance.
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
    for (const auto& entry : o.raw_member_vars_) {
      raw_member_vars_[entry.first] |= entry.second;
    }
    member_ref_vars_.insert(o.member_ref_vars_.begin(),
                            o.member_ref_vars_.end());
    return *this;
  }

  // Combines delta environments produced by indeterminately sequenced sibling
  // subexpressions (function arguments in C++17, parenthesized aggregate
  // initializers in C++20). Unlike sequential `operator|=`, one sibling's
  // `raw_member_vars_cleared_` (e.g. `Helper(&p)` taking `&p` before seeing a
  // sibling's `p = &obj->field_`) must not erase another sibling's
  // `raw_member_vars_` entry, because either evaluation order is valid.
  void MergeUnsequenced(const Environment& o) {
    if (this == &o) return;
    if (o.unreachable_) {
      unreachable_ = true;
      live_.clear();
    } else if (!unreachable_) {
      for (size_t i = 0, e = o.live_.size(); i < e; ++i) {
        if (o.live_[i]) set_live(i);
      }
    }
    raw_member_vars_cleared_.insert(o.raw_member_vars_cleared_.begin(),
                                    o.raw_member_vars_cleared_.end());
    for (const auto& entry : o.raw_member_vars_) {
      raw_member_vars_[entry.first] |= entry.second;
    }
    member_ref_vars_.insert(o.member_ref_vars_.begin(),
                            o.member_ref_vars_.end());
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
    for (const auto& entry : raw_member_vars_) {
      if (CanRescueOneSidedLive(entry.first, o, base_env)) {
        one_sided_live.push_back(entry.first);
      }
    }
    for (const auto& entry : o.raw_member_vars_) {
      if (o.CanRescueOneSidedLive(entry.first, *this, base_env)) {
        one_sided_live.push_back(entry.first);
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

    for (const auto& entry : o.raw_member_vars_) {
      raw_member_vars_[entry.first] |= entry.second;
    }
    VarSet cleared;
    for (const clang::ValueDecl* d : raw_member_vars_cleared_) {
      if (o.raw_member_vars_cleared_.contains(d)) {
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
  RawMemberMap raw_member_vars_;
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

  void SetEffect(int arg, ExprEffect in, bool is_unrooted_raw_init = false) {
    if (arg < 0) return;
    assert(arg >= kMaxNumberOfArguments || !seen_args_.test(arg));
#ifndef NDEBUG
    if (arg < kMaxNumberOfArguments) seen_args_.set(arg);
#endif
    bool has_gc = in.hasGC();
    bool has_raw = in.hasRawDef() || in.hasRawUse() ||
                   in.hasMemberProvenance() || is_unrooted_raw_init;
    if (has_gc) {
      ++gc_count_;
      if (gc_count_ == 1 && in.env() != nullptr) {
        gc_arg_live_ = in.env()->live_;
      }
    } else if (in.env() != nullptr) {
      for (const auto& entry : in.env()->raw_member_vars_) {
        non_gc_raw_member_defs_.insert(entry.first);
      }
    }
    if (has_raw) ++raw_count_;
    if (has_gc && has_raw) ++both_gc_and_raw_count_;
    if (in.hasRawDef()) any_raw_def_ = true;
    if (in.hasRawUse()) any_raw_use_ = true;
    if (in.hasOnHeapMemberLValue()) any_on_heap_member_lvalue_ = true;
    if (in.hasRawMemberAddress()) any_raw_member_addr_ = true;
    if (in.hasIndirectRawMemberAddress()) {
      any_indirect_raw_member_addr_ = true;
    }
    if (arg < kMaxNumberOfArguments) {
      if (in.hasOnHeapMemberLValue()) on_heap_member_lvalue_.set(arg);
      if (in.hasRawMemberAddress()) raw_member_addr_.set(arg);
      if (in.hasIndirectRawMemberAddress()) indirect_raw_member_addr_.set(arg);
    }
    if (in.env() != nullptr) {
      if (env_ == nullptr) {
        env_ = in.env();
      } else {
        env_->MergeUnsequenced(*in.env());
      }
    }
  }

  ExprEffect ComputeCumulativeEffect(
      bool result_is_raw, bool propagate_raw_def = false,
      const std::vector<bool>* direct_out_params = nullptr) {
    // Kill all variables (`raw_member_vars_` and `type_raw_vars_`) not
    // guaranteed live after the GC-causing argument(s) so `ApplyEffect` does
    // not resurrect variables defined or out-param-initialized in an
    // unsequenced sibling (`Callee(p = &obj->field_, CauseGC())`,
    // `Callee((InitSlot(&slot), 0), CauseGC())`), while keeping:
    //   1. Variables unconditionally rebound after a single GC-causing
    //      argument's GC (`Helper(&local_flag, (CauseGC(), p = &obj->field_,
    //      0))`), unless a non-GC sibling also gave them raw-member provenance
    //      (`non_gc_raw_member_defs_`); and
    //   2. Direct `&out` parameters of an enclosing `CallExpr` itself
    //      (`AllocatePartialMap(...).To(&map)`), which are written inside the
    //      callee body after all receiver/argument GCs complete.
    if (env_ != nullptr && !env_->unreachable_ && gc_count_ > 0) {
      std::vector<bool> keep_live;
      if (gc_count_ == 1) {
        keep_live = gc_arg_live_;
      }
      if (direct_out_params != nullptr) {
        if (direct_out_params->size() > keep_live.size()) {
          keep_live.resize(direct_out_params->size(), false);
        }
        for (size_t i = 0; i < direct_out_params->size(); ++i) {
          if ((*direct_out_params)[i]) keep_live[i] = true;
        }
      }
      for (const clang::ValueDecl* decl : non_gc_raw_member_defs_) {
        size_t code = static_cast<size_t>(Environment::SymbolToCode(decl));
        if (code < keep_live.size()) keep_live[code] = false;
      }
      env_->KeepOnlyLive(keep_live);
    }
    ExprEffect out = ExprEffect::NoneWithEnv(env_);
    if (gc_count_ > 0) out.setGC();
    if (any_raw_use_) out.setRawUse();
    // Propagate `kRawDef` upward when any child is an on-heap member lvalue or
    // raw member address, or when `propagate_raw_def` is set and any child has
    // `any_raw_def_` (`Parallel` / binary operators / array subscripts /
    // `CXXParenListInitExpr`). `VisitCallExpr` passes `propagate_raw_def =
    // false` to preserve the historical call boundary (by-value arguments are
    // copied before the callee body, and nested `Outer(Inner(*handle),
    // CauseGC())` is not flagged across call boundaries), whereas member
    // addresses/references passed into a call remain conservatively treated as
    // `kRawDef` in case the callee forwards or reads through them:
    //   Outer(p == &obj->field_, CauseGC());   // flagged
    //   arr[obj->int_field_] = CauseGC();      // flagged
    if (result_is_raw || (propagate_raw_def && any_raw_def_) ||
        HasOnHeapMember()) {
      out.setRawDef();
    }
    return out;
  }

  // Checks whether unsequenced sibling subexpressions (function arguments,
  // binary operator operands, array base + index, `CXXParenListInitExpr`
  // elements) are safe to evaluate in any order. Unsafe when one subexpression
  // can cause GC (`kCausesGC`) and another produces or accesses an unrooted
  // on-heap value/member (`kRawDef`, `kOnHeapMemberLValue`,
  // `kRawMemberAddress`, `kIndirectRawMemberAddress`, or
  // `is_unrooted_raw_init`) or reads an already-stale raw variable (`kRawUse`),
  // e.g.:
  //   Callee(*obj_handle, CauseGC());   // arg 1 has `kRawDef`, arg 2 GCs
  //   so->Method(*CauseGC());           // receiver `so->` has `kRawDef`,
  //                                     // arg 1 GCs
  //   obj->field_ = Allocate();         // LHS has `kOnHeapMemberLValue`,
  //                                     // RHS GCs (conservatively flagged
  //                                     // even under C++17 `=` sequencing)
  //   Callee(&obj->field_, CauseGC());  // arg 1 has `kRawMemberAddress`,
  //                                     // arg 2 GCs
  //   arr[obj->int_field_] = CauseGC(); // LHS has `kRawDef` from loading
  //                                     // `obj->int_field_`, RHS GCs
  //   arr[0] = CauseGC();               // `arr` is an already-stale pointer
  //                                     // (`kRawUse`), RHS GCs
  bool IsSafe() const {
    return gc_count_ == 0 || raw_count_ == 0 ||
           (gc_count_ == 1 && raw_count_ == 1 && both_gc_and_raw_count_ == 1);
  }

  bool HasOnHeapMemberAt(int arg) const {
    return arg >= 0 && arg < kMaxNumberOfArguments &&
           (on_heap_member_lvalue_.test(arg) || raw_member_addr_.test(arg) ||
            indirect_raw_member_addr_.test(arg));
  }
  bool HasOnHeapMember() const {
    return any_on_heap_member_lvalue_ || any_raw_member_addr_ ||
           any_indirect_raw_member_addr_;
  }
  bool HasRawMemberAddress() const { return any_raw_member_addr_; }
  bool HasRawMemberAddressAt(int arg) const {
    return arg >= 0 && arg < kMaxNumberOfArguments &&
           raw_member_addr_.test(arg);
  }
  bool HasIndirectRawMemberAddress() const {
    return any_indirect_raw_member_addr_;
  }
  bool HasIndirectRawMemberAddressAt(int arg) const {
    return arg >= 0 && arg < kMaxNumberOfArguments &&
           indirect_raw_member_addr_.test(arg);
  }
  uint8_t RawMemberProvenance() const {
    uint8_t prov = 0;
    if (HasRawMemberAddress()) prov |= kRawMemberAddress;
    if (HasIndirectRawMemberAddress()) prov |= kIndirectRawMemberAddress;
    return prov;
  }
  uint8_t RawMemberProvenanceAt(int arg) const {
    uint8_t prov = 0;
    if (HasRawMemberAddressAt(arg)) prov |= kRawMemberAddress;
    if (HasIndirectRawMemberAddressAt(arg)) prov |= kIndirectRawMemberAddress;
    return prov;
  }
  bool HasOnHeapMemberLValueAt(int arg) const {
    return arg >= 0 && arg < kMaxNumberOfArguments &&
           on_heap_member_lvalue_.test(arg);
  }
  const Environment* env() const { return env_; }

 private:
  static const int kMaxNumberOfArguments = 64;
#ifndef NDEBUG
  std::bitset<kMaxNumberOfArguments> seen_args_;
#endif
  std::bitset<kMaxNumberOfArguments> on_heap_member_lvalue_;
  std::bitset<kMaxNumberOfArguments> raw_member_addr_;
  std::bitset<kMaxNumberOfArguments> indirect_raw_member_addr_;
  unsigned gc_count_ = 0;
  unsigned raw_count_ = 0;
  unsigned both_gc_and_raw_count_ = 0;
  bool any_raw_def_ = false;
  bool any_raw_use_ = false;
  bool any_on_heap_member_lvalue_ = false;
  bool any_raw_member_addr_ = false;
  bool any_indirect_raw_member_addr_ = false;
  std::vector<bool> gc_arg_live_;
  Environment::VarSet non_gc_raw_member_defs_;
  Environment* env_;
};

Environment::SymbolTable Environment::symbol_table_;
Environment::VarSet Environment::type_raw_vars_;
std::vector<Environment*> Environment::envs_;

// Combines two alternative branches of an expression (`cond ? a : b` or
// `a && b` / `a || b`). Expression deltas only record variables modified inside
// each branch (never copying `base_env.live_`, which would be unsafe for
// unsequenced `CallProps`), so a one-sided GC + rebind conservatively leaves
// pre-existing variables cleared across `ApplyEffect` (matching `Tagged<T>`).
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
                              uint8_t raw_member_provenance) {
  Environment* e = env();
  if (e == nullptr) {
    e = Environment::Allocate(Environment());
  }
  e->MDefine(var_decl);
  e->SetRawMemberProvenanceInDelta(var_decl, raw_member_provenance);
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
    if (expr == nullptr || IsCalleeSafetyCheckAborted()) {
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
    VISIT(CallExpr);
    VISIT(CastExpr);
    VISIT(CharacterLiteral);
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
    VISIT(CXXParenListInitExpr);
    VISIT(CXXPseudoDestructorExpr);
    VISIT(CXXRewrittenBinaryOperator);
    VISIT(CXXScalarValueInitExpr);
    VISIT(CXXStdInitializerListExpr);
    VISIT(CXXThisExpr);
    VISIT(CXXThrowExpr);
    VISIT(CXXUnresolvedConstructExpr);
    VISIT(CXXUuidofExpr);
    VISIT(DeclRefExpr);
    VISIT(DependentScopeDeclRefExpr);
    VISIT(DesignatedInitExpr);
    VISIT(ExprWithCleanups);
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
    VISIT(SizeOfPackExpr);
    VISIT(SourceLocExpr);
    VISIT(StringLiteral);
    VISIT(SubstNonTypeTemplateParmExpr);
    VISIT(SubstNonTypeTemplateParmPackExpr);
    VISIT(TypeTraitExpr);
    VISIT(UnaryOperator);
    VISIT(UnaryExprOrTypeTraitExpr);
#undef VISIT

    // Unmodeled executable expression kinds (such as `LambdaExpr`,
    // `AtomicExpr`, `StmtExpr`, `CompoundLiteralExpr`, `ChooseExpr`,
    // `BlockExpr`, `CXXTypeidExpr`, `VAArgExpr`, `ExtVectorElementExpr`,
    // `ShuffleVectorExpr`) are ignored in the main pass, but conservatively
    // mark `callee_unsafe` when summarizing a callee body because they may
    // hide GC calls or member uses.
    if (state_.checking_callee_safety) {
      state_.callee_unsafe = true;
    }
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
  IGNORE_EXPR(CharacterLiteral);
  IGNORE_EXPR(ConceptSpecializationExpr);
  IGNORE_EXPR(CXXBoolLiteralExpr);
  IGNORE_EXPR(CXXDependentScopeMemberExpr);
  IGNORE_EXPR(CXXNullPtrLiteralExpr);
  IGNORE_EXPR(CXXScalarValueInitExpr);
  IGNORE_EXPR(CXXNoexceptExpr);
  IGNORE_EXPR(CXXUnresolvedConstructExpr);
  IGNORE_EXPR(CXXUuidofExpr);
  IGNORE_EXPR(DependentScopeDeclRefExpr);
  IGNORE_EXPR(DesignatedInitExpr);
  IGNORE_EXPR(FloatingLiteral);
  IGNORE_EXPR(ImaginaryLiteral);
  IGNORE_EXPR(IntegerLiteral);
  IGNORE_EXPR(ImplicitValueInitExpr);
  IGNORE_EXPR(PackExpansionExpr);
  IGNORE_EXPR(PredefinedExpr);
  IGNORE_EXPR(RequiresExpr);
  IGNORE_EXPR(SizeOfPackExpr);
  IGNORE_EXPR(SourceLocExpr);
  IGNORE_EXPR(StringLiteral);
  IGNORE_EXPR(SubstNonTypeTemplateParmPackExpr);
  IGNORE_EXPR(TypeTraitExpr);
  IGNORE_EXPR(GNUNullExpr);
  IGNORE_EXPR(OverloadExpr);

  DECL_VISIT_EXPR(ArrayInitLoopExpr) {
    return VisitExpr(expr->getSubExpr(), env);
  }

  DECL_VISIT_EXPR(CXXDefaultInitExpr) {
    return VisitExpr(expr->getExpr(), env);
  }

  DECL_VISIT_EXPR(CXXParenListInitExpr) {
    // C++20 parenthesized aggregate initialization (`Agg a(e1, e2)` or
    // `Arr a(e1)`) evaluates initializers in indeterminately sequenced order,
    // like function arguments.
    CallProps props;
    int idx = 0;
    auto visit_elem = [&](clang::Expr* init) {
      props.SetEffect(idx++, VisitExpr(init, env),
                      IsNonNullTrackedRawInit(init));
    };
    llvm::ArrayRef<clang::Expr*> inits = expr->getInitExprs();
    for (clang::Expr* init : inits) {
      visit_elem(init);
    }
    int filler_visits = TrailingArrayFillerVisitCount(
        expr->getType(), inits.size(), expr->getArrayFiller());
    for (int i = 0; i < filler_visits; ++i) {
      visit_elem(expr->getArrayFiller());
    }
    if (!props.IsSafe()) ReportUnsafe(expr, BAD_EXPR_MSG);
    ExprEffect out =
        props.ComputeCumulativeEffect(IsNonNullTrackedRawInit(expr),
                                      /*propagate_raw_def=*/true);
    if (props.HasOnHeapMember()) {
      out.setRawMemberAddress();
    }
    return out;
  }

  DECL_VISIT_EXPR(OffsetOfExpr) {
    unsigned num_exprs = expr->getNumExpressions();
    if (num_exprs == 0) return ExprEffect::None();
    llvm::SmallVector<clang::Expr*, 4> indices;
    indices.reserve(num_exprs);
    for (unsigned i = 0; i < num_exprs; ++i) {
      indices.push_back(expr->getIndexExpr(i));
    }
    // WithoutProvenance: `offsetof` always produces a `size_t` byte offset,
    // while runtime array-designator index subexpressions may read stale
    // variables or trigger GC.
    return Parallel(expr, static_cast<int>(indices.size()), indices.data(), env)
        .WithoutProvenance();
  }

  DECL_VISIT_EXPR(CXXPseudoDestructorExpr) {
    // WithoutProvenance: a pseudo-destructor call (`p->~T()`) destroys a scalar
    // in place and has `void` type, while its base expression must still be
    // visited for stale uses or GCs.
    return VisitExpr(expr->getBase(), env).WithoutProvenance();
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
    // Unwrap parens and no-op casts to detect whether `expr->getBase()` is an
    // array-to-pointer decay of an array object rather than subscripting a
    // pointer. Visiting the underlying array directly preserves whether it is
    // an on-heap member array (`kOnHeapMemberLValue`), a local stack array of
    // raw member addresses (`kRawMemberAddress`), or a dual-provenance
    // reference to either, without depending on `DeclRefExpr` pattern matching
    // (which misses `ArrayInitLoopExpr`'s `OpaqueValueExpr`, struct array
    // fields like `box.ptrs[0]`, and multidimensional arrays like
    // `grid[0][0]`).
    clang::Expr* peeled_base = expr->getBase()->IgnoreParens();
    while (auto* noop = llvm::dyn_cast<clang::ImplicitCastExpr>(peeled_base)) {
      if (noop->getCastKind() != clang::CK_NoOp) break;
      peeled_base = noop->getSubExpr()->IgnoreParens();
    }
    clang::Expr* decayed_array = nullptr;
    if (auto* decay = llvm::dyn_cast<clang::ImplicitCastExpr>(peeled_base)) {
      if (decay->getCastKind() == clang::CK_ArrayToPointerDecay) {
        decayed_array = decay->getSubExpr();
      }
    }

    CallProps props;
    ExprEffect base_effect = VisitExpr(
        decayed_array != nullptr ? decayed_array : expr->getBase(), env);
    if (decayed_array != nullptr && TrackAmbientRawTypes() &&
        IsPointerToOnHeapMemberType(expr->getBase()->getType())) {
      base_effect.setOnHeapMemberLValue();
    }
    props.SetEffect(0, base_effect);
    props.SetEffect(1, VisitExpr(expr->getIdx(), env));
    if (!props.IsSafe()) ReportUnsafe(expr, BAD_EXPR_MSG);
    ExprEffect out =
        props.ComputeCumulativeEffect(IsTrackedRawPointerType(expr->getType()),
                                      /*propagate_raw_def=*/true);
    if (decayed_array != nullptr) {
      if (props.HasOnHeapMemberLValueAt(0)) {
        out.setOnHeapMemberLValue();
      }
      if (props.HasRawMemberAddressAt(0) &&
          CanHoldRawMemberAddress(expr->getType())) {
        out.setRawMemberAddress();
      }
    } else {
      if (props.HasOnHeapMemberLValueAt(0) || props.HasRawMemberAddressAt(0)) {
        out.setOnHeapMemberLValue();
      }
      if (props.HasIndirectRawMemberAddressAt(0) &&
          CanHoldRawMemberAddress(expr->getType())) {
        out.setRawMemberAddress();
      }
    }
    return out;
  }

  // Environment only tracks liveness for non-constexpr variables, parameters,
  // and structured bindings declared inside the analyzed function (i.e.
  // `isLocalVarDeclOrParm()`, aligned with `VisitDecl`'s `DeclStmt` and
  // parameter registration, including block-scope `static`/`extern`
  // declarations). Namespace- and class-scope variables, constexpr constants
  // (such as `kNullExternalPointerHandle` or `kNullJSDispatchHandle`),
  // functions, and enum constants are never registered in `Environment`.
  static bool IsLocalTrackedVar(const clang::ValueDecl* decl) {
    if (const auto* var_decl = llvm::dyn_cast_or_null<clang::VarDecl>(decl)) {
      return !var_decl->isConstexpr() && var_decl->isLocalVarDeclOrParm();
    }
    if (const auto* binding_decl =
            llvm::dyn_cast_or_null<clang::BindingDecl>(decl)) {
      const auto* decomp_decl = llvm::dyn_cast_or_null<clang::VarDecl>(
          binding_decl->getDecomposedDecl());
      return decomp_decl != nullptr && !decomp_decl->isConstexpr() &&
             decomp_decl->isLocalVarDeclOrParm();
    }
    return false;
  }

  bool IsRawPointerVar(clang::Expr* expr, const clang::ValueDecl** var_decl) {
    if (expr == nullptr) return false;
    if (auto* decl_ref =
            llvm::dyn_cast<clang::DeclRefExpr>(expr->IgnoreParens())) {
      if (IsLocalTrackedVar(decl_ref->getDecl())) {
        *var_decl = decl_ref->getDecl();
        return true;
      }
    }

    return false;
  }

  bool IsMutableOutParamVar(clang::Expr* expr, const Environment& env,
                            const clang::ValueDecl** var_decl) {
    return IsRawPointerVar(expr, var_decl) &&
           !(*var_decl)->getType().getNonReferenceType().isConstant(
               ctx_->getASTContext()) &&
           !env.IsOnHeapMemberRefVar(*var_decl);
  }

  bool IsNonConstPointeePointerType(clang::QualType qtype) const {
    if (qtype.isNull()) return false;
    const auto* ptr = qtype->getAs<clang::PointerType>();
    return ptr != nullptr &&
           !ptr->getPointeeType().isConstant(ctx_->getASTContext());
  }

  bool IsConstPointeePointerType(clang::QualType qtype) const {
    if (qtype.isNull()) return false;
    const auto* ptr = qtype->getAs<clang::PointerType>();
    return ptr != nullptr &&
           ptr->getPointeeType().isConstant(ctx_->getASTContext());
  }

  bool IsAddressOrPointerWidthIntegerType(clang::QualType qtype) const {
    if (qtype.isNull()) return false;
    if (qtype->isDependentType()) return true;
    if (qtype->isPointerType()) return !qtype->isFunctionPointerType();
    if (qtype->isIntegerType() && !qtype->isBooleanType()) {
      return ctx_->getASTContext().getTypeSize(qtype) >=
             ctx_->getASTContext().getTypeSize(ctx_->getASTContext().VoidPtrTy);
    }
    return false;
  }

  static bool IsValuePreservingPointerCast(clang::CastKind kind) {
    switch (kind) {
      case clang::CK_NoOp:
      case clang::CK_BitCast:
      case clang::CK_DerivedToBase:
      case clang::CK_UncheckedDerivedToBase:
      case clang::CK_BaseToDerived:
      case clang::CK_AddressSpaceConversion:
        return true;
      default:
        return false;
    }
  }

  clang::Expr* PeelNonConstPointerCasts(clang::Expr* expr) const {
    if (expr == nullptr) return nullptr;
    expr = expr->IgnoreParens();
    while (auto* cast = llvm::dyn_cast<clang::CastExpr>(expr)) {
      if (!IsValuePreservingPointerCast(cast->getCastKind()) ||
          !IsNonConstPointeePointerType(cast->getSubExpr()->getType())) {
        break;
      }
      expr = cast->getSubExpr()->IgnoreParens();
    }
    return expr;
  }

  // Returns true if `expr` is a load or cv-qualification `CK_NoOp` cast of a
  // tracked local variable (whose `*Member*` provenance was already computed
  // by `Use()` via `raw_member_vars_` and `IsAmbientMemberOrSlotVar`), so
  // `VisitCastExpr` does not overwrite null/empty `const&` local bindings.
  bool IsLocalVarLoadOrNoOp(const clang::Expr* expr) const {
    if (expr == nullptr) return false;
    while (const auto* cast =
               llvm::dyn_cast<clang::CastExpr>(expr->IgnoreParens())) {
      if (cast->getCastKind() != clang::CK_LValueToRValue &&
          cast->getCastKind() != clang::CK_NoOp) {
        return false;
      }
      expr = cast->getSubExpr();
    }
    const auto* dre = llvm::dyn_cast<clang::DeclRefExpr>(expr->IgnoreParens());
    return dre != nullptr && IsLocalTrackedVar(dre->getDecl());
  }

  static ExprEffect ApplyAddrOfProvenance(ExprEffect sub) {
    int provenance = kNoEffect;
    if (sub.hasOnHeapMemberLValue()) provenance |= kRawMemberAddress;
    if (sub.hasRawMemberAddress()) provenance |= kIndirectRawMemberAddress;
    return sub.WithProvenance(provenance);
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

      case clang::BO_Add:
      case clang::BO_Sub:
      case clang::BO_AddAssign:
      case clang::BO_SubAssign: {
        CallProps props;
        props.SetEffect(0, VisitExpr(lhs, env));
        props.SetEffect(1, VisitExpr(rhs, env));
        if (!props.IsSafe()) ReportUnsafe(expr, BAD_EXPR_MSG);
        ExprEffect out = props.ComputeCumulativeEffect(
            IsTrackedRawPointerType(expr->getType()),
            /*propagate_raw_def=*/true);
        // Propagate raw member address provenance across pointer/address offset
        // arithmetic (`&obj->field_ + offset`, `addr - sizeof(Tagged_t)`,
        // `p += 1`, `ptrs + 1`), excluding narrow integers, floating-point
        // results, and pointer/address differences (`end_ptr - &obj->field_`,
        // `addr_a - addr_b`), which yield scalar values:
        bool is_sub = expr->getOpcode() == clang::BO_Sub ||
                      expr->getOpcode() == clang::BO_SubAssign;
        bool rhs_is_ptr = rhs->getType()->isPointerType();
        bool can_hold_addr =
            IsAddressOrPointerWidthIntegerType(expr->getType());
        bool propagates_raw_member_addr =
            can_hold_addr &&
            (is_sub ? (props.HasRawMemberAddressAt(0) &&
                       !props.HasRawMemberAddressAt(1) && !rhs_is_ptr)
                    : props.HasRawMemberAddress());
        if (propagates_raw_member_addr) {
          out.setRawMemberAddress();
        }
        bool propagates_indirect_addr =
            can_hold_addr &&
            (is_sub ? (props.HasIndirectRawMemberAddressAt(0) &&
                       !props.HasIndirectRawMemberAddressAt(1) && !rhs_is_ptr)
                    : props.HasIndirectRawMemberAddress());
        if (propagates_indirect_addr) {
          out.setIndirectRawMemberAddress();
        }
        // If `+=` adds a provenance-carrying address into a previously
        // untracked variable (`Address base = 0; base += addr;`), start
        // tracking `base` in `raw_member_vars_` without resurrecting variables
        // that were already tracked (and possibly stale).
        const clang::ValueDecl* var_decl = nullptr;
        uint8_t new_prov =
            (propagates_raw_member_addr ? kRawMemberAddress : 0) |
            (propagates_indirect_addr ? kIndirectRawMemberAddress : 0);
        if (expr->getOpcode() == clang::BO_AddAssign && new_prov != 0 &&
            IsRawPointerVar(lhs, &var_decl) &&
            !env.IsOnHeapMemberRefVar(var_decl) &&
            env.GetRawMemberProvenance(var_decl) == 0 &&
            !Environment::IsTypeRawVar(var_decl)) {
          out = out.Define(var_decl, new_prov);
        }
        // Built-in compound assignment (`obj->field_ += 1`) returns an lvalue
        // referring to the LHS, matching prefix `++`/`--`.
        if (expr->isCompoundAssignmentOp() &&
            props.HasOnHeapMemberLValueAt(0)) {
          out.setOnHeapMemberLValue();
        }
        return out;
      }

      case clang::BO_PtrMemD:
      case clang::BO_PtrMemI: {
        CallProps props;
        props.SetEffect(0, VisitExpr(lhs, env));
        props.SetEffect(1, VisitExpr(rhs, env));
        if (!props.IsSafe()) ReportUnsafe(expr, BAD_EXPR_MSG);
        ExprEffect out = props.ComputeCumulativeEffect(
            IsTrackedRawPointerType(expr->getType()),
            /*propagate_raw_def=*/true);
        // Pointer-to-data-member (`obj->*pmd`, `(*obj).*pmd`, `box.*ppmd`,
        // `boxes->*ppmd`) yields a glvalue referring to the selected field.
        if (expr->isGLValue()) {
          bool is_arrow = expr->getOpcode() == clang::BO_PtrMemI;
          const clang::CXXRecordDecl* base_record =
              is_arrow ? lhs->getType()->getPointeeCXXRecordDecl()
                       : lhs->getType()->getAsCXXRecordDecl();
          out = out.WithProvenance(ComputeFieldAccessProvenance(
              is_arrow, props.HasOnHeapMemberLValueAt(0),
              props.HasRawMemberAddressAt(0),
              props.HasIndirectRawMemberAddressAt(0), base_record,
              expr->getType()));
        }
        return out;
      }

      case clang::BO_Assign: {
        const clang::ValueDecl* var_decl = nullptr;
        // Builtin assignment to a local variable not bound to an on-heap member
        // (including mutable references to local pointers, e.g. `int*& r = p;`)
        // rebinds the variable and updates whether it holds a raw member
        // address:
        //   p = &obj->field_;   // defines `p` with raw member provenance
        //   p = &stack_var;     // defines `p` and clears raw member provenance
        // By contrast, assigning to a reference bound to an on-heap member
        // (`int& ref = obj->field_; ref = 42;`) writes through to the heap
        // object and must not redefine `ref` or treat the LHS as safe if `ref`
        // is already stale.
        if (IsRawPointerVar(lhs, &var_decl) &&
            !env.IsOnHeapMemberRefVar(var_decl)) {
          ExprEffect rhs_effect = VisitExpr(rhs, env);
          return rhs_effect.Define(var_decl, rhs_effect.rawMemberProvenance());
        }
        [[fallthrough]];
      }

      default: {
        if (expr->isAssignmentOp()) {
          CallProps props;
          props.SetEffect(0, VisitExpr(lhs, env));
          props.SetEffect(1, VisitExpr(rhs, env));
          if (!props.IsSafe()) ReportUnsafe(expr, BAD_EXPR_MSG);
          ExprEffect out = props.ComputeCumulativeEffect(
              IsTrackedRawPointerType(expr->getType()),
              /*propagate_raw_def=*/true);
          // Built-in assignment and compound assignment return an lvalue
          // referring to the LHS (and for `=`, carry any raw member address
          // assigned from the RHS).
          if (props.HasOnHeapMemberLValueAt(0)) {
            out.setOnHeapMemberLValue();
          }
          if (expr->getOpcode() == clang::BO_Assign) {
            if (props.HasRawMemberAddressAt(1)) {
              out.setRawMemberAddress();
            }
            if (props.HasIndirectRawMemberAddressAt(1)) {
              out.setIndirectRawMemberAddress();
            }
          }
          return out;
        }
        return Parallel(expr, 2, exprs, env);
      }
    }
  }

  DECL_VISIT_EXPR(CXXBindTemporaryExpr) {
    return VisitExpr(expr->getSubExpr(), env);
  }

  DECL_VISIT_EXPR(MaterializeTemporaryExpr) {
    return VisitExpr(expr->getSubExpr(), env);
  }

  // Returns true if `expr` statically evaluates to a null pointer, zero
  // handle, constexpr constant, or default/empty aggregate or slot, so it
  // cannot become stale across a subsequent GC in an initializer list.
  bool IsKnownNullOrZeroExpr(const clang::Expr* expr) const {
    while (expr != nullptr) {
      expr = expr->IgnoreParens();
      if (const auto* cleanups =
              llvm::dyn_cast<clang::ExprWithCleanups>(expr)) {
        expr = cleanups->getSubExpr();
      } else if (const auto* mat =
                     llvm::dyn_cast<clang::MaterializeTemporaryExpr>(expr)) {
        expr = mat->getSubExpr();
      } else if (const auto* bind =
                     llvm::dyn_cast<clang::CXXBindTemporaryExpr>(expr)) {
        expr = bind->getSubExpr();
      } else if (const auto* def_init =
                     llvm::dyn_cast<clang::CXXDefaultInitExpr>(expr)) {
        expr = def_init->getExpr();
      } else if (const auto* const_expr =
                     llvm::dyn_cast<clang::ConstantExpr>(expr)) {
        expr = const_expr->getSubExpr();
      } else if (const auto* subst =
                     llvm::dyn_cast<clang::SubstNonTypeTemplateParmExpr>(
                         expr)) {
        expr = subst->getReplacement();
      } else {
        break;
      }
    }
    if (expr == nullptr) return true;
    if (llvm::isa<clang::ImplicitValueInitExpr, clang::CXXScalarValueInitExpr,
                  clang::CXXNullPtrLiteralExpr, clang::GNUNullExpr>(expr)) {
      return true;
    }
    if (const auto* int_lit = llvm::dyn_cast<clang::IntegerLiteral>(expr)) {
      return int_lit->getValue() == 0;
    }
    if (const auto* decl_ref = llvm::dyn_cast<clang::DeclRefExpr>(expr)) {
      const clang::ValueDecl* decl = decl_ref->getDecl();
      if (const auto* var = llvm::dyn_cast<clang::VarDecl>(decl)) {
        return var->isConstexpr();
      }
      return llvm::isa<clang::EnumConstantDecl>(decl);
    }
    if (const auto* cast = llvm::dyn_cast<clang::CastExpr>(expr)) {
      switch (cast->getCastKind()) {
        case clang::CK_NullToPointer:
          return true;
        case clang::CK_LValueToRValue:
        case clang::CK_NoOp:
        case clang::CK_IntegralCast:
        case clang::CK_BitCast:
        case clang::CK_DerivedToBase:
        case clang::CK_UncheckedDerivedToBase:
        case clang::CK_BaseToDerived:
        case clang::CK_ConstructorConversion:
          return IsKnownNullOrZeroExpr(cast->getSubExpr());
        default:
          return false;
      }
    }
    if (const auto* ctor = llvm::dyn_cast<clang::CXXConstructExpr>(expr)) {
      if (ctor->getNumArgs() == 0) return true;
      if (ctor->getConstructor()->isDefaultConstructor()) {
        for (const clang::Expr* arg : ctor->arguments()) {
          const auto* default_arg =
              llvm::dyn_cast<clang::CXXDefaultArgExpr>(arg);
          if (default_arg == nullptr ||
              !IsKnownNullOrZeroExpr(default_arg->getExpr())) {
            return false;
          }
        }
        return true;
      }
      if (ctor->getConstructor()->isCopyOrMoveConstructor()) {
        return IsKnownNullOrZeroExpr(ctor->getArg(0));
      }
      return false;
    }
    if (const auto* init_list = llvm::dyn_cast<clang::InitListExpr>(expr)) {
      for (const clang::Expr* sub : init_list->inits()) {
        if (!IsKnownNullOrZeroExpr(sub)) return false;
      }
      if (TrailingArrayFillerVisitCount(init_list->getType(),
                                        init_list->getNumInits(),
                                        init_list->getArrayFiller()) > 0 &&
          !IsKnownNullOrZeroExpr(init_list->getArrayFiller())) {
        return false;
      }
      return true;
    }
    if (const auto* paren_init =
            llvm::dyn_cast<clang::CXXParenListInitExpr>(expr)) {
      auto sub_inits = paren_init->getInitExprs();
      for (const clang::Expr* sub : sub_inits) {
        if (!IsKnownNullOrZeroExpr(sub)) return false;
      }
      if (TrailingArrayFillerVisitCount(paren_init->getType(), sub_inits.size(),
                                        paren_init->getArrayFiller()) > 0 &&
          !IsKnownNullOrZeroExpr(paren_init->getArrayFiller())) {
        return false;
      }
      return true;
    }
    return false;
  }

  // Returns how many times `array_filler` should be visited (0, 1, or 2) for an
  // array initializer of type `array_type` with `num_inits` explicit elements:
  //   - 0 when `array_filler` is null or when C99 array designators with
  //     interior holes already padded `inits` up to `cat->getSize()` while
  //     leaving `getArrayFiller()` non-null.
  //   - 1 when one trailing element is omitted (or the array bound is not a
  //     constant, e.g. `T arr[]` or dependent `T arr[N]`).
  //   - 2 when two or more trailing elements are omitted, since Clang shares a
  //     single `array_filler` AST node across them and a second visit catches
  //     filler-to-filler GC hazards (element `k` producing an unrooted/member
  //     value that element `k+1`'s GC invalidates).
  int TrailingArrayFillerVisitCount(clang::QualType array_type,
                                    size_t num_inits,
                                    const clang::Expr* array_filler) const {
    if (array_filler == nullptr) return 0;
    const auto* cat = ctx_->getASTContext().getAsConstantArrayType(array_type);
    if (cat == nullptr) return 1;
    if (!cat->getSize().ugt(num_inits)) return 0;
    return cat->getSize().ugt(num_inits + 1) ? 2 : 1;
  }

  // Returns true if `init` initializes an element (or array of elements) of a
  // tracked raw-pointer / table-handle / always-on-heap-slot type with a value
  // that is not statically known null/zero/constexpr/default-constructed.
  bool IsNonNullTrackedRawInit(const clang::Expr* init) {
    if (init == nullptr || init->getType().isNull()) return false;
    if (!IsTrackedRawPointerType(
            ctx_->getASTContext().getBaseElementType(init->getType()))) {
      return false;
    }
    return !IsKnownNullOrZeroExpr(init);
  }

  // Evaluates brace-enclosed initializer elements (`InitListExpr` or
  // list-initialized `CXXConstructExpr`) in C++11+ strict left-to-right order.
  // An earlier element becomes dangling across a later element's GC if the
  // earlier element produced an on-heap member address/lvalue (`{&obj->a_,
  // (CauseGC(), &obj->b_)}`) or a non-null raw on-heap value or array of raw
  // on-heap values (`{*handle, CauseGC()}`, `{{*handle}, CauseGC()}`), whereas
  // null/empty/constexpr initializers (`{nullptr, CauseGC()}`,
  // `{kNullExternalPointerHandle, CauseGC()}`, `{EmbedderDataSlot(),
  // CauseGC()}`) and scalars loaded by value (`Foo{obj->int_f_, CauseGC()}`)
  // cannot become stale across a subsequent GC. (Nested struct/class
  // aggregates holding by-value `Tagged<T>` fields are not recursively
  // inspected, matching GCMole's top-level `IsInternalPointerType` scope.)
  ExprEffect VisitSequencedInitList(clang::Expr* parent,
                                    llvm::ArrayRef<clang::Expr*> inits,
                                    const Environment& env,
                                    clang::Expr* array_filler = nullptr) {
    ExprEffect out = ExprEffect::None();
    Environment out_env = env;
    bool saw_member_provenance = false;
    uint8_t saw_raw_member_prov = 0;
    bool saw_unrooted_value = false;
    bool reported_unsafe = false;
    auto visit_elem = [&](clang::Expr* init) {
      ExprEffect elem = VisitExpr(init, out_env);
      if (elem.hasGC() && (saw_unrooted_value || saw_member_provenance) &&
          !reported_unsafe) {
        ReportUnsafe(parent, BAD_EXPR_MSG);
        reported_unsafe = true;
      }
      if (elem.hasMemberProvenance()) saw_member_provenance = true;
      saw_raw_member_prov |= elem.rawMemberProvenance();
      if (IsNonNullTrackedRawInit(init)) {
        saw_unrooted_value = true;
      }
      out = ExprEffect::MergeSeq(out, elem);
      out_env = out_env.ApplyEffect(elem);
    };
    for (clang::Expr* init : inits) {
      visit_elem(init);
    }
    int filler_visits = TrailingArrayFillerVisitCount(
        parent->getType(), inits.size(), array_filler);
    for (int i = 0; i < filler_visits; ++i) {
      visit_elem(array_filler);
    }
    // WithoutProvenance: clear the last element's individual provenance
    // (`kOnHeapMemberLValue` from an element subexpression does not make the
    // aggregate itself an on-heap member lvalue) before setting the
    // aggregate's own provenance.
    out = out.WithoutProvenance();
    if (IsNonNullTrackedRawInit(parent)) {
      out.setRawDef();
    }
    const auto* ctor_expr = llvm::dyn_cast<clang::CXXConstructExpr>(parent);
    if (llvm::isa<clang::InitListExpr>(parent) && saw_member_provenance) {
      out.setRawMemberAddress();
    } else if (ctor_expr != nullptr) {
      ApplyConstructProvenance(ctor_expr, saw_member_provenance,
                               saw_raw_member_prov, &out);
    }
    return out;
  }

  // Propagates raw member provenance through `CXXConstructExpr` (both
  // parenthesized and brace-initialized):
  //   - Constructing a `SlotBase` subclass from an on-heap member/address (or
  //     non-default constructing an always-on-heap slot such as
  //     `EmbedderDataSlot(js_obj, 0)` or `ExternalPointerSlot(addr, tag)`)
  //     sets `kRawMemberAddress` (whereas `FullObjectSlot(&local_tagged)` does
  //     not).
  //   - Copy/move-constructing an aggregate/wrapper preserves incoming
  //     `kRawMemberAddress` / `kIndirectRawMemberAddress` (e.g. `RefBox b2 =
  //     b1;`).
  // TODO(crbug.com/571823300): Also track non-owning span/view/tuple wrappers
  // constructed from raw interior pointers (such as
  // `base::Vector<T>(obj->chars(), n)` and `std::pair`/`std::tuple`) in both
  // Pass 2 and callee summaries (together with
  // `.data()`/`.begin()`/`operator[]` propagation), and check escaped member
  // arguments once GC-capable constructors are tracked.
  void ApplyConstructProvenance(const clang::CXXConstructExpr* ctor_expr,
                                bool has_on_heap_member,
                                uint8_t raw_member_prov, ExprEffect* out) {
    if (IsSlotType(ctor_expr->getType()) &&
        (has_on_heap_member || (TrackAmbientRawTypes() &&
                                IsAlwaysOnHeapSlotConstruction(ctor_expr)))) {
      out->setRawMemberAddress();
    } else if (ctor_expr->getConstructor()->isCopyOrMoveConstructor()) {
      if (raw_member_prov & kRawMemberAddress) {
        out->setRawMemberAddress();
      }
      if (raw_member_prov & kIndirectRawMemberAddress) {
        out->setIndirectRawMemberAddress();
      }
    }
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
    ExprEffect out =
        props.ComputeCumulativeEffect(IsNonNullTrackedRawInit(expr));
    ApplyConstructProvenance(expr, props.HasOnHeapMember(),
                             props.RawMemberProvenance(), &out);
    return out;
  }

  DECL_VISIT_EXPR(CXXDefaultArgExpr) { return VisitExpr(expr->getExpr(), env); }

  DECL_VISIT_EXPR(CXXDeleteExpr) {
    // WithoutProvenance: `delete` has `void` type, so its result cannot carry
    // member provenance.
    return VisitExpr(expr->getArgument(), env).WithoutProvenance();
  }

  DECL_VISIT_EXPR(CXXNewExpr) {
    CallProps props;
    int idx = 0;
    for (clang::Expr* arg : expr->placement_arguments()) {
      props.SetEffect(idx++, VisitExpr(arg, env));
    }
    if (std::optional<clang::Expr*> array_size = expr->getArraySize()) {
      props.SetEffect(idx++, VisitExpr(*array_size, env));
    }
    if (expr->hasInitializer()) {
      props.SetEffect(idx++, VisitExpr(expr->getInitializer(), env));
    }
    if (!props.IsSafe()) ReportUnsafe(expr, BAD_EXPR_MSG);
    // WithoutProvenance: non-placement `new` returns a newly allocated C++ heap
    // pointer rather than an interior V8 on-heap member address. Standard
    // single-argument placement `new (p) T(...)` returns `p`, so preserve
    // `kRawMemberAddress` when `p` points to an on-heap member (or when
    // placement-constructing an on-heap `*Member` type).
    bool is_ambient_member_new = TrackAmbientRawTypes() &&
                                 expr->getNumPlacementArgs() > 0 &&
                                 IsPointerToOnHeapMemberType(expr->getType());
    ExprEffect out =
        props
            .ComputeCumulativeEffect(/*result_is_raw=*/is_ambient_member_new,
                                     /*propagate_raw_def=*/true)
            .WithoutProvenance();
    if (is_ambient_member_new ||
        (expr->getNumPlacementArgs() == 1 && props.HasRawMemberAddressAt(0) &&
         expr->getOperatorNew() != nullptr &&
         expr->getOperatorNew()->isReservedGlobalPlacementOperator())) {
      out.setRawMemberAddress();
    }
    return out;
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
    // A single-element glvalue `InitListExpr` (e.g. `int& r{obj->field_}`,
    // `PtrBox& b{obj->box_}`, `{obj->field_}` passed to a reference parameter)
    // or non-aggregate scalar brace-init (e.g. `int** pp{ptrs}`,
    // `int* p{&obj->field_}`) is pure syntax sugar around its initializer and
    // must preserve provenance verbatim (including
    // `kIndirectRawMemberAddress`).
    if (expr->getNumInits() == 1 && expr->getArrayFiller() == nullptr &&
        (expr->isGLValue() || (!expr->getType()->isRecordType() &&
                               !expr->getType()->isArrayType()))) {
      return VisitExpr(expr->getInit(0), env);
    }
    // In C++11+, brace-enclosed initializer lists evaluate their elements in
    // strict left-to-right order (including `getArrayFiller()` for omitted
    // trailing array elements), and any raw member address or reference in an
    // element (e.g. `uint8_t* ptrs[2] = {&obj->a_, nullptr}` or
    // `RefBox{obj->a_}`) makes the initialized aggregate hold raw member
    // provenance.
    // TODO(crbug.com/571823300): Teach Pass 1 (`CalleesPrinter`, which runs
    // with `shouldVisitImplicitCode() == false`) to also traverse semantic
    // `InitListExpr` forms (`getArrayFiller()` / `CXXDefaultInitExpr`) so
    // callees whose only GC happens in a default member initializer enter
    // `gcsuspects`.
    return VisitSequencedInitList(
        expr,
        llvm::ArrayRef<clang::Expr*>(expr->getInits(), expr->getNumInits()),
        env, expr->getArrayFiller());
  }

  // Computes result value provenance for a field access (`MemberExpr` or
  // `BO_PtrMemD`/`BO_PtrMemI`), returning `kNoEffect` when neither on-heap nor
  // local-container provenance applies so stray base provenance (such as
  // `kIndirectRawMemberAddress` through a `bit_cast` struct) is never leaked.
  // Note: `IsOnHeapMemberRecord` intentionally excludes `SlotBase` subclasses
  // (e.g. `EmbedderDataSlot`), so a local struct field like `box.slot` is not
  // marked `kOnHeapMemberLValue` (avoiding false `BAD_EXPR_MSG` on
  // `box.slot = MakeSlotThatGCs()`). Post-declaration mutation of an initially
  // empty local aggregate/array (e.g. `box.ptr = &obj->field_;` or
  // `slots[0] = obj->RawField(0);`) does not promote provenance onto `box` or
  // `slots`; only aggregates initialized with member provenance are tracked.
  int ComputeFieldAccessProvenance(bool is_arrow, bool base_on_heap_lvalue,
                                   bool base_raw_member_addr,
                                   bool base_indirect_raw_member_addr,
                                   const clang::CXXRecordDecl* base_record,
                                   clang::QualType field_type) {
    const clang::CXXRecordDecl* field_record = field_type.getNonReferenceType()
                                                   ->getBaseElementTypeUnsafe()
                                                   ->getAsCXXRecordDecl();
    // Mark field access as an on-heap member lvalue when:
    //   1. `base_on_heap_lvalue` or `(is_arrow && base_raw_member_addr)`:
    //      accessing a subfield of an interior struct/union
    //      (`obj->pair_.first`) or dereferencing a member pointer
    //      (`member_ptr->value_`).
    //   2. `IsOnHeapMemberRecord(field_record)`: the field itself is an on-heap
    //      `*Member` wrapper (`TaggedMember<T>`, `UnalignedValueMember<T>`,
    //      etc.).
    //   3. `IsOnHeapClassOrMember(base_record)`: the field belongs to a
    //      `HeapObject` subclass or `*Member` class (`obj->primitive_field_`).
    bool local_container = (!is_arrow && base_raw_member_addr) ||
                           (is_arrow && base_indirect_raw_member_addr);
    if (base_on_heap_lvalue || (is_arrow && base_raw_member_addr) ||
        (TrackAmbientRawTypes() && (IsOnHeapMemberRecord(field_record) ||
                                    IsOnHeapClassOrMember(base_record)))) {
      int provenance = kOnHeapMemberLValue;
      // Preserve `kRawMemberAddress` on a dual-provenance struct reference or
      // pointer (`PtrBox& r = cond ? local_box : obj->box_field_; int* q =
      // r.ptr;`) so loading a pointer-like field retains the local-aggregate
      // branch's raw member address provenance after `CK_LValueToRValue`.
      if (local_container && CanHoldRawMemberAddress(field_type)) {
        provenance |= kRawMemberAddress;
      }
      return provenance;
    }
    if (local_container) {
      // Accessing a field of a local aggregate (directly via `.` or through a
      // pointer to a local array/struct via `->`) that holds raw member
      // provenance (e.g. `RefBox{obj->field_}.ref`,
      // `PtrAndInt{&obj->field_, n}.count`, or `boxes->ptr`).
      if (field_type->isReferenceType()) {
        int provenance = kOnHeapMemberLValue;
        if (CanHoldRawMemberAddress(field_type.getNonReferenceType())) {
          provenance |= kRawMemberAddress;
        }
        return provenance;
      }
      if (CanHoldRawMemberAddress(field_type)) {
        return kRawMemberAddress;
      }
    }
    return kNoEffect;
  }

  DECL_VISIT_EXPR(MemberExpr) {
    ExprEffect out = VisitExpr(expr->getBase(), env);
    const clang::ValueDecl* member = expr->getMemberDecl();
    if (llvm::isa<clang::FieldDecl, clang::IndirectFieldDecl>(member)) {
      const auto* parent_record =
          llvm::dyn_cast<clang::CXXRecordDecl>(member->getDeclContext());
      return out.WithProvenance(ComputeFieldAccessProvenance(
          expr->isArrow(), out.hasOnHeapMemberLValue(),
          out.hasRawMemberAddress(), out.hasIndirectRawMemberAddress(),
          parent_record, member->getType()));
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
    // Excluded for `const`-qualified values, `const` references (e.g.
    // `const ObjectSlot&`, `int* const&`, `TaggedMember<Object>* const&`),
    // `const` arrays (`int* const[N]`), and outer conversions to
    // `const`-pointee pointers (`const T*`, `const void*`, `int* const*`,
    // intercepted in `VisitCastExpr`), whose pointees are `const` and therefore
    // cannot be out-parameters (note mutable pointers to `const` pointees like
    // `const int* p` and mutable references `int*& r` are not themselves
    // `const`), as well as references bound to an on-heap member (`int& ref =
    // obj->field_; Foo(&ref);`), where `&ref` takes the interior address of
    // `obj->field_` rather than initializing `ref` as an out-param. Non-const
    // `&p` preserves the pre-existing out-parameter heuristic in both the main
    // pass and callee summaries (including mutable local aggregates initialized
    // via `Init(&box)`, whereas `&const_box`, `static_cast<const Box*>(&box)`,
    // `&boxes[0]`, and `&outer.box` produce `kIndirectRawMemberAddress`). For
    // non-reference variables, preserve `env.GetRawMemberProvenance(var_decl)`
    // so taking `&p` of an existing `int* p = &obj->field_` keeps `p` in
    // `raw_member_vars_` (and `KeepOnlyLive` kills `p` if the call triggers a
    // GC) while an uninitialized or null-initialized out-param
    // (`TaggedMember<T>* out = nullptr; Init(&out);`) stays out of
    // `raw_member_vars_` during a GC-capable out-param call.
    if (expr->getOpcode() == clang::UO_AddrOf) {
      const clang::ValueDecl* var_decl = nullptr;
      if (IsMutableOutParamVar(expr->getSubExpr(), env, &var_decl)) {
        return ExprEffect::None().Define(var_decl,
                                         env.GetRawMemberProvenance(var_decl));
      }
    }
    ExprEffect out = VisitExpr(expr->getSubExpr(), env);
    switch (expr->getOpcode()) {
      case clang::UO_AddrOf: {
        // `&obj->field_` turns an on-heap member lvalue into a raw member
        // address (`kOnHeapMemberLValue` -> `kRawMemberAddress`), while taking
        // the address of a `const` local container holding raw member addresses
        // (e.g. `&const_box` or `&dual_ref`) yields an indirect container
        // pointer (`kRawMemberAddress` -> `kIndirectRawMemberAddress`).
        return ApplyAddrOfProvenance(out);
      }
      case clang::UO_Deref: {
        // `*(&obj->field_)` or `*member_ptr` turns a raw member address back
        // into an on-heap member lvalue (`kRawMemberAddress` ->
        // `kOnHeapMemberLValue`), while dereferencing an indirect container
        // pointer (`*ptrs`, `*(ptrs + 0)`, `*const_box_ptr`) unwraps to
        // `kRawMemberAddress` when the pointee type can hold an address.
        int provenance = kNoEffect;
        if (out.hasRawMemberAddress()) provenance |= kOnHeapMemberLValue;
        if (out.hasIndirectRawMemberAddress() &&
            CanHoldRawMemberAddress(expr->getType())) {
          provenance |= kRawMemberAddress;
        }
        return out.WithProvenance(provenance);
      }
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
        // while mutating `obj->field_`: strip `kOnHeapMemberLValue` and record
        // `kRawDef` so `Callee(obj->field_++, CauseGC())` is flagged, while
        // preserving `kRawMemberAddress` on dual-provenance pointer references
        // (`int* q = r++;`).
        return out.WithoutOnHeapMemberLValue();
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
    // Intentionally visit the argument expression (even when unevaluated) to
    // preserve historical GCMole diagnostics (e.g. `TestTwoSizeTArguments`).
    // WithoutProvenance: `sizeof` and `alignof` evaluate to a `size_t`
    // constant, not an on-heap member address or lvalue.
    return VisitExpr(expr->getArgumentExpr(), env).WithoutProvenance();
  }

  DECL_VISIT_EXPR(CastExpr) {
    // When a pointer-preserving cast adds pointee `const` (`T* -> const T*`,
    // `T* -> const void*`, `int** -> int* const*`), an addressed mutable local
    // variable (`&var` or `std::addressof(var)`) cannot be written through by
    // the consumer and must be evaluated as a read + indirect address rather
    // than taking the `UO_AddrOf` / `std::addressof` out-parameter shortcut.
    ExprEffect out = ExprEffect::None();
    bool evaluated_const_addr_of = false;
    if (IsValuePreservingPointerCast(expr->getCastKind()) &&
        IsConstPointeePointerType(expr->getType()) &&
        IsNonConstPointeePointerType(expr->getSubExpr()->getType())) {
      clang::Expr* peeled = PeelNonConstPointerCasts(expr->getSubExpr());
      const clang::ValueDecl* var_decl = nullptr;
      if (auto* uo = llvm::dyn_cast<clang::UnaryOperator>(peeled);
          uo != nullptr && uo->getOpcode() == clang::UO_AddrOf &&
          IsMutableOutParamVar(uo->getSubExpr(), env, &var_decl)) {
        out = ApplyAddrOfProvenance(VisitExpr(uo->getSubExpr(), env));
        evaluated_const_addr_of = true;
      } else if (auto* addressof_call = llvm::dyn_cast<clang::CallExpr>(peeled);
                 addressof_call != nullptr &&
                 addressof_call->getNumArgs() == 1) {
        clang::FunctionDecl* callee = addressof_call->getDirectCallee();
        clang::Expr* arg0 = addressof_call->getArg(0);
        if (callee != nullptr && !llvm::isa<clang::CXXMethodDecl>(callee) &&
            callee->getIdentifier() != nullptr &&
            callee->getName() == "addressof" && callee->isInStdNamespace() &&
            IsMutableOutParamVar(arg0, env, &var_decl)) {
          out = ApplyAddrOfProvenance(VisitExpr(arg0, env));
          evaluated_const_addr_of = true;
        }
      }
    }
    if (!evaluated_const_addr_of) {
      out = VisitExpr(expr->getSubExpr(), env);
    }
    switch (expr->getCastKind()) {
      case clang::CK_LValueToRValue:
      case clang::CK_LValueToRValueBitCast:
        // Loading an on-heap member by value (e.g. passing `obj->int_field_` by
        // value to `Callee(obj->int_field_, CauseGC())`) copies the value out
        // of the heap: strip `kOnHeapMemberLValue` and record `kRawDef` so
        // enclosing unsequenced expressions still see the heap access, while
        // preserving `kRawMemberAddress` on dual-provenance pointer references
        // (`int* q = r;`).
        out = out.WithoutOnHeapMemberLValue();
        break;
      case clang::CK_ArrayToPointerDecay: {
        // Decaying an interior on-heap array field or `FLEXIBLE_ARRAY_MEMBER`
        // accessor produces a direct raw interior address:
        //   uint8_t* bytes = obj->inline_bytes_;   // T[N] -> T*
        //   int* values = obj->values();           // T(&)[0] -> T*
        // Decaying a local array whose elements hold raw member addresses
        // (`int* ptrs[2] = {&obj->field_, nullptr}`) produces an indirect
        // container pointer (`kIndirectRawMemberAddress`), so `*ptrs`, `pp[0]`,
        // and callees taking `int* arr[2]` unwrap elements to
        // `kRawMemberAddress` rather than stripping them as on-heap lvalues.
        int provenance = kNoEffect;
        if (out.hasOnHeapMemberLValue()) {
          provenance |= kRawMemberAddress;
        }
        if (out.hasRawMemberAddress()) {
          provenance |= kIndirectRawMemberAddress;
        }
        out = out.WithProvenance(provenance);
        break;
      }
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
    // loads of `*Member*` fields, excluding explicit or const-qualified null
    // casts like `static_cast<TaggedMember<T>*>(nullptr)` and
    // `CK_LValueToRValue` / `CK_NoOp` casts on local variables already handled
    // by `Use()`) points to an on-heap member.
    if (TrackAmbientRawTypes() && !IsKnownNullOrZeroExpr(expr) &&
        !IsLocalVarLoadOrNoOp(expr) &&
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
        IsTrackedRawPointerType(parent->getType()),
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

  bool IsAmbientMemberOrSlotVar(const clang::QualType& var_type,
                                const clang::ValueDecl* var_decl) {
    if (!TrackAmbientRawTypes() || var_type.isNull()) return false;
    if (var_decl != THIS && !Environment::IsTypeRawVar(var_decl)) {
      return false;
    }
    clang::QualType non_ref_type = var_type.getNonReferenceType();
    // Match scalar `*Member*` pointers (including `this` inside `*Member`
    // methods, e.g. `this->ptr_location()`) and always-on-heap slots directly
    // (`IsAlwaysOnHeapSlotRecord` rather than `IsAlwaysOnHeapSlotType` so
    // un-`MDefine`d local slot arrays like `EmbedderDataSlot slots[2]` and
    // null/empty `const&` local bindings are not matched; for `this` inside an
    // always-on-heap slot method, also check the pointee record).
    return IsPointerToOnHeapMemberType(non_ref_type) ||
           IsAlwaysOnHeapSlotRecord(non_ref_type->getAsCXXRecordDecl()) ||
           (var_decl == THIS &&
            IsAlwaysOnHeapSlotRecord(non_ref_type->getPointeeCXXRecordDecl()));
  }

  // Represents a node in the AST {parent} which uses the variable {var_decl}
  // (or `this` when `var_decl == nullptr`).
  // Here we observe the type in {var_type} (or tracked on-heap member
  // address/reference provenance on {var_decl} in {env}) of a previously
  // declared variable. If it represents a raw on-heap value or interior member
  // address/reference, we do the following:
  // 1. Propagate its member value effect (kRawMemberAddress /
  //    kIndirectRawMemberAddress / kOnHeapMemberLValue) in the returned
  //    ExprEffect.
  // 2. If it got stale due to GC since its declaration, report it and mark
  //    kRawUse.
  ExprEffect Use(const clang::Expr* parent, const clang::QualType& var_type,
                 const clang::ValueDecl* var_decl,
                 clang::FullSourceLoc var_location, const Environment& env) {
    ExprEffect out = ExprEffect::None();
    uint8_t raw_member_prov = env.GetRawMemberProvenance(var_decl);
    bool is_raw_member_addr = raw_member_prov != 0;
    bool is_on_heap_member_ref = env.IsOnHeapMemberRefVar(var_decl);
    bool is_ambient_member_or_slot =
        IsAmbientMemberOrSlotVar(var_type, var_decl);
    if ((raw_member_prov & kRawMemberAddress) || is_ambient_member_or_slot) {
      out.setRawMemberAddress();
    }
    if (raw_member_prov & kIndirectRawMemberAddress) {
      out.setIndirectRawMemberAddress();
    }
    if (is_on_heap_member_ref) out.setOnHeapMemberLValue();

    if (!g_dead_vars_analysis) return out;
    if (!is_raw_member_addr && !is_on_heap_member_ref &&
        !IsTrackedRawPointerType(var_type) && !is_ambient_member_or_slot) {
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
    if (!IsLocalTrackedVar(var)) return ExprEffect::None();
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
    if (!method->isVirtual() || IsQualifiedMemberCall(memcall)) return;

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

  // Returns the receiver class of `memcall` before any implicit derived-to-base
  // conversion (such as `CK_UncheckedDerivedToBase` when calling inherited
  // `TaggedImpl::ptr_location()` on `TaggedMember<T>` or
  // `SlotBase::ToVoidPtr()` on a derived slot), falling back to
  // `memcall->getRecordDecl()`.
  static const clang::CXXRecordDecl* GetMemberCallReceiverRecord(
      const clang::CXXMemberCallExpr* memcall) {
    if (memcall == nullptr) return nullptr;
    if (const clang::Expr* obj = memcall->getImplicitObjectArgument()) {
      clang::QualType obj_type = obj->IgnoreParenImpCasts()->getType();
      if (!obj_type.isNull()) {
        if (obj_type->isAnyPointerType()) {
          obj_type = obj_type->getPointeeType();
        }
        if (const clang::CXXRecordDecl* record =
                obj_type->getBaseElementTypeUnsafe()->getAsCXXRecordDecl()) {
          return record;
        }
      }
    }
    return memcall->getRecordDecl();
  }

  // Computes the value provenance (`kRawMemberAddress`,
  // `kIndirectRawMemberAddress`, or `kOnHeapMemberLValue`) of a `CallExpr`'s
  // return value:
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
  //   4. Address-extracting, bit-casting, tuple-access, or forwarding helpers
  //      on an on-heap member/slot:
  //        `obj->field_.storage_address()`, `obj->tagged_.ptr_location()`,
  //        `slot.address()`, `slot.location()`, `slot.handle_location()`,
  //        `slot.ToVoidPtr()`, `std::addressof(obj->field_)`,
  //        `base::bit_cast<Address>(ptr)`, `std::get<0>(arr)`,
  //        `std::forward<T>(arg)`, `std::move(arg)`
  //      (TODO(crbug.com/571823300): raw `HeapObject::address()` /
  //      `Tagged::ptr()` arithmetic and arbitrary `T*`/`Address`-returning
  //      methods or forwarding wrappers are not yet tracked as member
  //      provenance.)
  void ComputeCallProvenance(clang::CallExpr* call,
                             clang::CXXMemberCallExpr* memcall,
                             clang::CXXOperatorCallExpr* opcall,
                             clang::FunctionDecl* callee,
                             const CallProps& props, const Environment& env,
                             ExprEffect* out) {
    const clang::CXXRecordDecl* receiver_record =
        GetMemberCallReceiverRecord(memcall);
    clang::QualType call_type = call->getType();
    if (!call_type.isNull()) {
      if ((props.HasOnHeapMember() || TrackAmbientRawTypes()) &&
          IsPointerToOnHeapMemberType(call_type)) {
        out->setRawMemberAddress();
      } else if (IsSlotType(call_type)) {
        bool is_assign =
            opcall != nullptr && opcall->getOperator() == clang::OO_Equal;
        bool from_member = is_assign
                               ? props.HasOnHeapMemberAt(
                                     IsSlotReferenceType(call_type) ? 1 : 2)
                               : props.HasOnHeapMember();
        if (from_member ||
            (TrackAmbientRawTypes() &&
             ((!is_assign && IsAlwaysOnHeapSlotType(call_type)) ||
              IsOnHeapClassOrMember(receiver_record)))) {
          out->setRawMemberAddress();
        }
      } else if ((props.HasOnHeapMember() || TrackAmbientRawTypes()) &&
                 call->isGLValue() &&
                 (IsOnHeapMemberRecord(call_type->getBaseElementTypeUnsafe()
                                           ->getAsCXXRecordDecl()) ||
                  // In C++, functions cannot return arrays by value, so a
                  // GLValue CallExpr has an ArrayType return type only when
                  // returning a reference to an array (e.g.
                  // FLEXIBLE_ARRAY_MEMBER accessors returning T(&)[0]).
                  (call_type->isArrayType() &&
                   IsOnHeapClassOrMember(receiver_record)))) {
        out->setOnHeapMemberLValue();
      }
    }

    if (callee == nullptr || callee->getIdentifier() == nullptr) return;
    llvm::StringRef callee_name = callee->getName();

    if (memcall != nullptr && props.HasOnHeapMemberAt(0)) {
      // Check `receiver_record` (peeling implicit `CK_UncheckedDerivedToBase`
      // casts, since `TaggedMember<T>::ptr_location()` is inherited from
      // `TaggedImpl`) and require an address-capable return type so same-named
      // methods (e.g. `location()`) on local aggregates or interior structs
      // cannot falsely acquire `kRawMemberAddress`.
      static constexpr llvm::StringLiteral kAddressAccessors[] = {
          "storage_address", "address",         "ptr_location",
          "location",        "handle_location", "ToVoidPtr",
      };
      if ((IsOnHeapMemberRecord(receiver_record) ||
           IsSlotBaseSubclass(receiver_record)) &&
          CanHoldRawMemberAddress(call_type) &&
          llvm::is_contained(kAddressAccessors, callee_name)) {
        out->setRawMemberAddress();
      }
    } else if (!llvm::isa<clang::CXXMethodDecl>(callee) &&
               call->getNumArgs() == 1) {
      if (callee_name == "addressof" && callee->isInStdNamespace()) {
        if (props.HasOnHeapMemberLValueAt(1)) {
          out->setRawMemberAddress();
        }
        // Mirror `UO_AddrOf`: suppress `kIndirectRawMemberAddress` only for a
        // mutable non-member-ref variable `DeclRefExpr` (out-parameter
        // heuristic), keeping `const` vars, dual-provenance member refs, and
        // non-variable lvalues (`boxes[0]`, `outer.box`) tracked.
        const clang::ValueDecl* var_decl = nullptr;
        clang::Expr* arg0 = call->getArg(0);
        if (props.HasRawMemberAddressAt(1) &&
            !IsMutableOutParamVar(arg0, env, &var_decl)) {
          out->setIndirectRawMemberAddress();
        }
      } else if (callee_name == "bit_cast" &&
                 (callee->isInStdNamespace() || InV8Namespace(callee))) {
        if (props.HasRawMemberAddressAt(1) &&
            (CanHoldRawMemberAddress(call_type) ||
             IsAddressOrPointerWidthIntegerType(call_type))) {
          out->setRawMemberAddress();
        }
        if (props.HasIndirectRawMemberAddressAt(1) &&
            IsAddressOrPointerWidthIntegerType(call_type)) {
          out->setIndirectRawMemberAddress();
        }
      } else if (callee->isInStdNamespace() && callee_name == "get") {
        // Propagate raw member provenance out of tuple-like aggregate access
        // (`std::get<0>(arr)` and tuple-protocol structured bindings on local
        // `std::array<int*, N>`), and preserve `kOnHeapMemberLValue` when
        // indexing into an on-heap `std::array`/`std::pair`/`std::tuple`/
        // `std::variant` field (`std::get<0>(obj->arr_)`,
        // `auto& [a, b] = obj->arr_`).
        if (props.HasRawMemberAddressAt(1) &&
            CanHoldRawMemberAddress(call_type)) {
          out->setRawMemberAddress();
        }
        if (props.HasOnHeapMemberLValueAt(1) && call->isGLValue()) {
          out->setOnHeapMemberLValue();
        }
      } else if (callee->isInStdNamespace() &&
                 (callee_name == "forward" || callee_name == "move")) {
        if (props.HasRawMemberAddressAt(1)) {
          out->setRawMemberAddress();
        }
        if (props.HasIndirectRawMemberAddressAt(1)) {
          out->setIndirectRawMemberAddress();
        }
        if (props.HasOnHeapMemberLValueAt(1)) {
          out->setOnHeapMemberLValue();
        }
      }
    }
  }

  void CheckEscapedMemberArgs(clang::CallExpr* call,
                              clang::CXXMemberCallExpr* memcall,
                              clang::CXXOperatorCallExpr* opcall,
                              clang::FunctionDecl* callee,
                              const CallProps& props) {
    if (!g_dead_vars_analysis || IsCalleeSafetyCheckAborted() ||
        !props.HasOnHeapMember()) {
      return;
    }
    // Check both `DisableGCMole` and `ConservativePinningScope` against
    // `call_loc`: the GC event being guarded is specifically `call` (so any
    // enclosing guard declared before `call_loc` protects it even if an earlier
    // GC occurred in an outer scope before the guard, and any variable already
    // killed before a `ConservativePinningScope` is still caught by `Use` at
    // its declaration location).
    clang::FullSourceLoc call_loc(call->getExprLoc(), sm_);
    if (HasActiveGuardAt(call_loc) || HasActiveConservativePinning(call_loc)) {
      return;
    }
    if (memcall != nullptr && props.HasOnHeapMemberAt(0) &&
        DoesCalleeUseMemberArgUnsafely(memcall, opcall, callee, props, 0)) {
      ReportUnsafe(memcall->getImplicitObjectArgument(),
                   ESCAPED_MEMBER_ADDR_MSG);
      if (IsCalleeSafetyCheckAborted()) return;
    }
    for (unsigned arg = 0; arg < call->getNumArgs(); ++arg) {
      unsigned prop_idx = arg + 1;
      if (props.HasOnHeapMemberAt(prop_idx) &&
          DoesCalleeUseMemberArgUnsafely(memcall, opcall, callee, props,
                                         prop_idx)) {
        ReportUnsafe(call->getArg(arg), ESCAPED_MEMBER_ADDR_MSG);
        if (IsCalleeSafetyCheckAborted()) return;
      }
    }
  }

  // After visiting the receiver and the arguments of the {call} node, this
  // function might report a GC-unsafe usage (due to the undefined evaluation
  // order of the receiver and the rest of the arguments, or passing a raw
  // on-heap member address into a callee that can cause GC).
  ExprEffect VisitCallExpr(clang::CallExpr* call, const Environment& env) {
    CallProps props;

    clang::CXXMemberCallExpr* memcall =
        llvm::dyn_cast_or_null<clang::CXXMemberCallExpr>(call);
    clang::CXXOperatorCallExpr* opcall =
        llvm::dyn_cast_or_null<clang::CXXOperatorCallExpr>(call);
    clang::FunctionDecl* callee = call->getDirectCallee();
    const auto* callee_method =
        llvm::dyn_cast_or_null<clang::CXXMethodDecl>(callee);
    if (memcall != nullptr) {
      clang::Expr* receiver = memcall->getImplicitObjectArgument();
      props.SetEffect(0, VisitExpr(receiver, env));
      if (const auto* bo = llvm::dyn_cast<clang::BinaryOperator>(
              memcall->getCallee()->IgnoreParens());
          bo != nullptr && (bo->getOpcode() == clang::BO_PtrMemD ||
                            bo->getOpcode() == clang::BO_PtrMemI)) {
        // Pointer-to-member call (`(obj.*pmf)(args)` / `(ptr->*pmf)(args)`):
        // `getImplicitObjectArgument()` visits `bo->getLHS()`, so also visit
        // `bo->getRHS()` in a slot past the arguments so `props.IsSafe()`
        // checks it without aliasing argument indices in
        // `CheckEscapedMemberArgs`. WithoutProvenance: the member-function
        // pointer RHS is not a data-member address argument.
        props.SetEffect(call->getNumArgs() + 1,
                        VisitExpr(bo->getRHS(), env).WithoutProvenance());
      }
    } else if (callee == nullptr) {
      // Visit indirect callee expressions (e.g. `(*fn_ptr)()` or
      // `obj->callback_()`). WithoutProvenance: the callee function pointer
      // itself is not a data-member address argument.
      props.SetEffect(0, VisitExpr(call->getCallee(), env).WithoutProvenance());
    } else if (callee_method != nullptr && callee_method->isStatic()) {
      if (const auto* member_expr = llvm::dyn_cast<clang::MemberExpr>(
              call->getCallee()->IgnoreParenImpCasts())) {
        // Direct static-method call on an object expression
        // (`p->StaticMethod()`, `(CauseGC(), obj->nested_).StaticMethod()`):
        // the base is evaluated for side effects while its value is discarded,
        // so keep GC/stale-use/env effects without recording a raw value
        // definition or provenance.
        props.SetEffect(
            0, VisitExpr(member_expr->getBase(), env).WithoutValueAndRawDef());
      }
    }

    bool is_trivial_copy_or_move_assign =
        callee_method != nullptr && callee_method->isTrivial() &&
        (callee_method->isCopyAssignmentOperator() ||
         callee_method->isMoveAssignmentOperator());
    // Overloaded `operator=` syntax (`a = b`, not explicit `a.operator=(b)`
    // calls) on a local variable not bound to an on-heap member (e.g.
    // `slot = obj->RawField(0)`, `tagged_var = ...`, mutable local slot
    // references `EmbedderDataSlot& s = slot; s = ...`, or trivial copy/move
    // rebinding of a local struct) redefines the variable and updates its
    // raw-member provenance. Excluded when `var_decl` is in `member_ref_vars_`,
    // is a `SlotBase` proxy reference (`UnalignedSlot<T>::Reference`,
    // `AtomicSlot::Reference`), or uses a non-trivial/custom `operator=` on a
    // local aggregate (which may read/write through a stale interior member or
    // trigger GC).
    if (const clang::ValueDecl* var_decl = nullptr;
        opcall != nullptr && opcall->getOperator() == clang::OO_Equal &&
        IsRawPointerVar(opcall->getArg(0), &var_decl) &&
        !env.IsOnHeapMemberRefVar(var_decl) &&
        !IsSlotReferenceType(var_decl->getType()) &&
        (RepresentsRawPointerType(var_decl->getType()) ||
         IsSlotType(var_decl->getType()) || is_trivial_copy_or_move_assign)) {
      // TODO(gcmole): Narrow non-trivial `operator=` handling on raw-pointer
      // and slot types to known-safe assignment operators rather than assuming
      // all overloads on those types are GC-free.
      // Index 2 is operator-call arg 1 (the RHS); index 1 (the LHS) is
      // intentionally not visited for simple assignment.
      ExprEffect rhs_effect = VisitExpr(call->getArg(1), env);
      props.SetEffect(
          2, rhs_effect.Define(var_decl, rhs_effect.rawMemberProvenance()));
    } else {
      VisitArguments<>(call, &props, env);
    }

    if (!props.IsSafe()) ReportUnsafe(call, BAD_EXPR_MSG);

    std::vector<bool> direct_out_params;
    if (props.env() != nullptr && !props.env()->unreachable_) {
      for (unsigned i = 0; i < call->getNumArgs(); ++i) {
        clang::Expr* arg = call->getArg(i);
        if (!IsNonConstPointeePointerType(arg->getType())) continue;
        arg = PeelNonConstPointerCasts(arg);
        const auto* uo = llvm::dyn_cast<clang::UnaryOperator>(arg);
        const clang::ValueDecl* out_var = nullptr;
        if (uo != nullptr && uo->getOpcode() == clang::UO_AddrOf &&
            IsMutableOutParamVar(uo->getSubExpr(), env, &out_var) &&
            !props.env()->IsRawMemberVar(out_var)) {
          size_t code = static_cast<size_t>(Environment::SymbolToCode(out_var));
          if (code >= direct_out_params.size()) {
            direct_out_params.resize(code + 1, false);
          }
          direct_out_params[code] = true;
        }
      }
    }

    ExprEffect out = props.ComputeCumulativeEffect(
        IsTrackedRawPointerType(call->getType()), /*propagate_raw_def=*/false,
        &direct_out_params);

    ComputeCallProvenance(call, memcall, opcall, callee, props, env, &out);
    // Trivial copy/move assignment (`b2 = b1` or `obj->box_ = b1`) returns
    // `*this`: carry RHS raw member address provenance for chained assignments
    // (`b3 = (b2 = b1)`) and LHS on-heap lvalue provenance when assigning into
    // an on-heap struct field (`PtrBox& r = (obj->box_ = other)`).
    if (opcall != nullptr && opcall->getOperator() == clang::OO_Equal &&
        is_trivial_copy_or_move_assign) {
      if (props.HasRawMemberAddressAt(2)) {
        out.setRawMemberAddress();
      }
      if (props.HasIndirectRawMemberAddressAt(2)) {
        out.setIndirectRawMemberAddress();
      }
      if (props.HasOnHeapMemberLValueAt(1)) {
        out.setOnHeapMemberLValue();
      }
    }
    if (callee == nullptr) return out;

    ExprEffect callee_effect = ExprEffect::None();
    CheckCallForGCCause(call, memcall, callee, &callee_effect);
    if (callee_effect.hasGC()) {
      out.setGC();
      // Kill variables defined in pre-call argument subexpressions (e.g.
      // `CalleeThatGCs(p = &obj->field_)`, `CauseGCRaw(raw_obj = *h, isolate)`,
      // or `CauseGCManaged((InitSlot(&slot), 42), isolate)`), keeping live
      // only direct `&var` out-parameters of `call` itself
      // (`OutParameterFunction(&obj, isolate)`,
      // `InitTaggedMemberPtrThatGCs(&out, isolate)`).
      if (out.env() != nullptr) {
        out.env()->KeepOnlyLive(direct_out_params);
      }
      CheckEscapedMemberArgs(call, memcall, opcall, callee, props);
    }

    return out;
  }

  // Resolves the concrete `FunctionDecl` definition of `callee` when its body
  // is available in the current translation unit, non-variadic, non-dependent,
  // and non-virtual (or statically devirtualizable / explicitly qualified), or
  // returns `nullptr` if `callee` cannot be statically summarized.
  const clang::FunctionDecl* ResolveCalleeBody(
      clang::CXXMemberCallExpr* memcall, clang::FunctionDecl* callee) {
    if (auto* method = llvm::dyn_cast<clang::CXXMethodDecl>(callee)) {
      // Match Pass 1 (`IsVirtualOrVisitorCall`), which redirects
      // `ObjectVisitor::VisitRelocInfo` across all `Visit*` overrides rather
      // than analyzing its base body in isolation.
      if (method->getIdentifier() != nullptr &&
          method->getName() == "VisitRelocInfo" &&
          method->getParent()->getQualifiedNameAsString() ==
              "v8::internal::ObjectVisitor") {
        return nullptr;
      }
      if (method->isVirtual() && !IsQualifiedMemberCall(memcall)) {
        if (memcall == nullptr) return nullptr;
        callee = method->getDevirtualizedMethod(
            memcall->getImplicitObjectArgument(), false);
        if (callee == nullptr) return nullptr;
      }
    }
    const clang::FunctionDecl* body = nullptr;
    if (!callee->hasBody(body) || body->getBody() == nullptr ||
        body->isVariadic() || body->isDependentContext()) {
      return nullptr;
    }
    return body;
  }

  // Returns true if passing the on-heap member address or reference at
  // `prop_idx` in `props` to `callee` is unsafe across a GC inside `callee`.
  // When `callee`'s body is available in the current translation unit and
  // non-virtual (or statically devirtualizable), we analyze the body with only
  // that member parameter tracked (and `checking_callee_safety = true` to
  // suppress unrelated diagnostics) so that callees which only access the
  // member before triggering a GC are not falsely reported:
  //
  //   // Safe callee (not reported at `SafeCallee(obj->RawField(0))`):
  //   void SafeCallee(ObjectSlot slot) {
  //     Tagged<Object> val = *slot;  // read before GC
  //     CauseGC();                   // `slot` is never used after GC
  //   }
  //
  //   // Unsafe callee (reported at `UnsafeCallee(&obj->field_)`):
  //   void UnsafeCallee(int* ptr) {
  //     CauseGC();
  //     *ptr = 42;                   // `ptr` dereferenced after GC
  //   }
  //
  // Note: This intra-procedural summary checks direct uses, local aliases,
  // `DisableGCMole` guards, `ConservativePinningScope` guards (for calls made
  // while the scope is active, and for variables declared after the scope),
  // and further direct calls within `callee` (treating indirect calls with
  // `getDirectCallee() == nullptr` as non-GC just as in the main pass, and
  // conservatively marking unmodeled statements/expressions such as lambdas,
  // `goto`, or `StmtExpr` as unsafe); it does not check `CXXConstructExpr`
  // arguments or track escaping the pointer before a GC into `this->`,
  // another reference/out-parameter, globals, containers (e.g.
  // `vec.push_back(p)`), or returned values (`return p;` after an internal GC
  // is still caught via `Use(p)`).
  // TODO(crbug.com/571823300): Consider summarizing the implicit `this`
  // receiver (`prop_idx == 0` / member operator arg 0) and propagating callee
  // return-value provenance if needed.
  bool DoesCalleeUseMemberArgUnsafely(clang::CXXMemberCallExpr* memcall,
                                      clang::CXXOperatorCallExpr* opcall,
                                      clang::FunctionDecl* callee,
                                      const CallProps& props,
                                      unsigned prop_idx) {
    const auto* callee_method = llvm::dyn_cast<clang::CXXMethodDecl>(callee);
    // Calling a non-static GC-capable method directly on an on-heap member or
    // slot (`obj->field_.MethodThatGCs()`, `slot.MethodThatGCs()`) passes the
    // raw interior address as `this` into the method (low-level `*Member` and
    // `SlotBase` accessors such as `load`/`store` are not GC suspects).
    if (prop_idx == 0) {
      assert(memcall != nullptr && callee_method != nullptr &&
             !callee_method->isStatic() &&
             !callee_method->isExplicitObjectMemberFunction());
      return true;
    }

    // For a member CXXOperatorCallExpr without an explicit object parameter
    // (e.g. `slot[i]` or `obj->field_ = MightGC()`), CallProps index 1 is
    // always the object argument (`opcall->getArg(0)`, even for C++23 static
    // member operators) and index 2 is parameter 0; for free operator calls,
    // C++23 explicit object member operators, and non-operator calls,
    // CallProps index 1 corresponds to parameter 0.
    bool is_implicit_obj_member_opcall =
        opcall != nullptr && callee_method != nullptr &&
        !callee_method->isExplicitObjectMemberFunction();
    if (is_implicit_obj_member_opcall && prop_idx == 1) {
      return !callee_method->isStatic();
    }

    unsigned param_offset = is_implicit_obj_member_opcall ? 2 : 1;
    assert(prop_idx >= param_offset);
    unsigned param_idx = prop_idx - param_offset;

    uint8_t raw_member_prov = props.RawMemberProvenanceAt(prop_idx);
    bool is_member_ref = props.HasOnHeapMemberLValueAt(prop_idx);
    if (is_member_ref && param_idx < callee->getNumParams()) {
      // By-value arguments normally strip `kOnHeapMemberLValue` via
      // `CK_LValueToRValue` or `CXXConstructExpr` before `SetEffect`, so only
      // reference parameters (`T&` or `const T&`) bind to the on-heap member
      // lvalue.
      clang::QualType param_type = callee->getParamDecl(param_idx)->getType();
      if (!param_type->isDependentType() && !param_type->isReferenceType()) {
        is_member_ref = false;
      }
    }
    if (raw_member_prov == 0 && !is_member_ref) {
      return false;
    }

    const clang::FunctionDecl* body = ResolveCalleeBody(memcall, callee);
    if (body == nullptr || param_idx >= body->getNumParams()) return true;
    const clang::ParmVarDecl* param = body->getParamDecl(param_idx);
    if ((raw_member_prov & kRawMemberAddress) &&
        IsSlotReferenceType(param->getType())) {
      is_member_ref = true;
    }

    // Memoize by (canonical FunctionDecl*, param_idx, raw_member_prov,
    // is_member_ref), cap nested summary depth (`kMaxCalleeSummaryDepth`), and
    // break recursion cycles by conservatively returning true on active keys.
    // Depth-truncated results depend on the caller's stack depth and are not
    // cached so deep chains cannot poison later direct calls to inner safe
    // helpers (as a result, a chain deeper than `kMaxCalleeSummaryDepth` can
    // still succeed if a shallower safe sub-chain was already cached earlier
    // in the TU, which is sound because that sub-chain is verified safe).
    CalleeSummaryKey key{body->getCanonicalDecl(), param_idx, raw_member_prov,
                         is_member_ref};
    auto cache_it = callee_safety_cache_.find(key);
    if (cache_it != callee_safety_cache_.end()) {
      return cache_it->second;
    }
    // Check cycle back-edges before `kMaxCalleeSummaryDepth`: a cycle reached
    // at exact depth `kMaxCalleeSummaryDepth` is depth-independent and safe to
    // cache, whereas setting `callee_hit_depth_limit` first would suppress
    // caching for every node in the cycle.
    if (llvm::is_contained(active_callee_checks_, key)) {
      return true;
    }
    if (active_callee_checks_.size() >= kMaxCalleeSummaryDepth) {
      if (state_.checking_callee_safety) {
        state_.callee_hit_depth_limit = true;
      }
      return true;
    }

    bool unsafe = false;
    bool hit_depth_limit = false;
    {
      CalleeSafetyScope scope(this, key);
      Environment callee_env;
      DefineAndVisitParameters(body, callee_env);
      if (raw_member_prov != 0) {
        callee_env.SetRawMemberProvenance(param, raw_member_prov);
      }
      if (is_member_ref) {
        callee_env.SetMemberRefVar(param, true);
      }
      callee_env.MDefine(param);

      VisitStmt(body->getBody(), callee_env);
      unsafe = state_.callee_unsafe;
      hit_depth_limit = state_.callee_hit_depth_limit;
    }
    if (unsafe && hit_depth_limit) {
      if (state_.checking_callee_safety) {
        state_.callee_hit_depth_limit = true;
      }
    } else {
      callee_safety_cache_[key] = unsafe;
    }

    return unsafe;
  }

  // --------------------------------------------------------------------------
  // Statements
  // --------------------------------------------------------------------------

  Environment VisitStmt(clang::Stmt* stmt, const Environment& env) {
    if (stmt == nullptr || IsCalleeSafetyCheckAborted()) return env;
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

    VISIT(AttributedStmt);
    VISIT(BreakStmt);
    VISIT(CompoundStmt);
    VISIT(ContinueStmt);
    VISIT(CXXForRangeStmt);
    VISIT(DeclStmt);
    VISIT(DoStmt);
    VISIT(ForStmt);
    VISIT(IfStmt);
    VISIT(LabelStmt);
    VISIT(NullStmt);
    VISIT(ReturnStmt);
    VISIT(CaseStmt);
    VISIT(DefaultStmt);
    VISIT(SwitchStmt);
    VISIT(WhileStmt);
#undef VISIT

    // Unmodeled statement kinds (including unstructured control flow like
    // `GotoStmt`/`IndirectGotoStmt`, `AsmStmt`, and
    // `CXXTryStmt`/`CXXCatchStmt`) are ignored in the main pass, but
    // conservatively mark `callee_unsafe` when summarizing a callee body
    // because they may hide GC calls, backward loops across GC, or member uses.
    if (state_.checking_callee_safety) {
      state_.callee_unsafe = true;
    }
    return env;
  }

#define DECL_VISIT_STMT(type) \
  Environment Visit##type(clang::type* stmt, const Environment& env)

#define IGNORE_STMT(type)                                              \
  Environment Visit##type(clang::type* stmt, const Environment& env) { \
    return env;                                                        \
  }

  IGNORE_STMT(NullStmt);

  DECL_VISIT_STMT(LabelStmt) { return VisitStmt(stmt->getSubStmt(), env); }

  DECL_VISIT_STMT(AttributedStmt) { return VisitStmt(stmt->getSubStmt(), env); }

  class Block {
   public:
    Block(const Environment& in, FunctionAnalyzer* owner, bool is_loop = true)
        : in_(in),
          out_(Environment::Unreachable()),
          continue_out_(Environment::Unreachable()),
          changed_(false),
          is_loop_(is_loop),
          owner_(owner) {
      parent_ = owner_->EnterBlock(this);
    }

    ~Block() { owner_->LeaveBlock(parent_); }

    void MergeIn(const Environment& env) {
      Environment old_in = in_;
      in_ &= env;
      changed_ |= !old_in.Equal(in_);
    }

    bool changed() {
      if (!changed_ || owner_->IsCalleeSafetyCheckAborted()) return false;
      changed_ = false;
      return true;
    }

    const Environment& in() const { return in_; }

    const Environment& out() const { return out_; }

    void MergeOut(const Environment& env) { out_ &= env; }

    void MergeContinue(const Environment& env) {
      for (Block* b = this; b != nullptr; b = b->parent_) {
        if (b->is_loop_) {
          b->continue_out_ &= env;
          return;
        }
      }
    }

    const Block* FindEnclosingSwitch() const {
      for (const Block* b = this; b != nullptr; b = b->parent_) {
        if (!b->is_loop_) return b;
      }
      return nullptr;
    }

    Environment TakeContinueOut() {
      Environment result = std::move(continue_out_);
      continue_out_ = Environment::Unreachable();
      return result;
    }

   private:
    Environment in_;
    Environment out_;
    Environment continue_out_;
    bool changed_;
    bool is_loop_;
    FunctionAnalyzer* owner_;
    Block* parent_;
  };

  // Returns true for unconditionally true loop conditions (`for (;;)`,
  // `while (true)`, `while (1)`), which can only exit the loop via `break`.
  static bool IsAlwaysTrueLoopCond(const clang::Expr* cond) {
    if (cond == nullptr) return true;
    const clang::Expr* stripped = cond->IgnoreParenImpCasts();
    if (const auto* bool_lit =
            llvm::dyn_cast<clang::CXXBoolLiteralExpr>(stripped)) {
      return bool_lit->getValue();
    }
    if (const auto* int_lit = llvm::dyn_cast<clang::IntegerLiteral>(stripped)) {
      return int_lit->getValue() != 0;
    }
    return false;
  }

  // Returns true for unconditionally false loop conditions (`while (false)`,
  // `while (0)`), used in `do { ... } while (false/0)` macros.
  static bool IsAlwaysFalseLoopCond(const clang::Expr* cond) {
    if (cond == nullptr) return false;
    const clang::Expr* stripped = cond->IgnoreParenImpCasts();
    if (const auto* bool_lit =
            llvm::dyn_cast<clang::CXXBoolLiteralExpr>(stripped)) {
      return !bool_lit->getValue();
    }
    if (const auto* int_lit = llvm::dyn_cast<clang::IntegerLiteral>(stripped)) {
      return int_lit->getValue() == 0;
    }
    return false;
  }

  DECL_VISIT_STMT(BreakStmt) {
    if (state_.block != nullptr) state_.block->MergeOut(env);
    return Environment::Unreachable();
  }

  DECL_VISIT_STMT(ContinueStmt) {
    if (state_.block != nullptr) state_.block->MergeContinue(env);
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

  // Isolates unbraced loop/branch substatements in their own `GCScope` so a
  // single-statement `DisableGCMole` (e.g. `do DisableGCMole g; while (GC());`)
  // does not leak into loop conditions/increments or sibling branches.
  Environment VisitScopedSubStmt(clang::Stmt* sub, const Environment& env) {
    if (sub == nullptr || llvm::isa<clang::CompoundStmt>(sub)) {
      return VisitStmt(sub, env);
    }
    state_.scopes.push_back(GCScope());
    Environment out = VisitStmt(sub, env);
    PopScope();
    return out;
  }

  DECL_VISIT_STMT(WhileStmt) {
    state_.scopes.push_back(GCScope());
    Block block(env, this);
    Environment cond_out = Environment::Unreachable();
    do {
      Environment cond_var_out =
          VisitStmt(stmt->getConditionVariableDeclStmt(), block.in());
      cond_out = VisitStmt(stmt->getCond(), cond_var_out);
      Environment body_out = VisitScopedSubStmt(stmt->getBody(), cond_out);
      Environment after_body =
          Environment::Merge(body_out, block.TakeContinueOut());
      block.MergeIn(after_body);
    } while (block.changed());
    if (!IsAlwaysTrueLoopCond(stmt->getCond())) {
      block.MergeOut(cond_out);
    }
    PopScope();
    return block.out();
  }

  DECL_VISIT_STMT(DoStmt) {
    state_.scopes.push_back(GCScope());
    Block block(env, this);

    // Special case `do { ... } while (false);` / `do { ... } while (0);`,
    // which is known to only run once, and is used in our (D)CHECK macros.
    if (IsAlwaysFalseLoopCond(stmt->getCond())) {
      Environment body_out = VisitScopedSubStmt(stmt->getBody(), block.in());
      Environment after_body =
          Environment::Merge(body_out, block.TakeContinueOut());
      Environment cond_out = VisitStmt(stmt->getCond(), after_body);
      block.MergeOut(cond_out);
      PopScope();
      return block.out();
    }

    Environment cond_out = Environment::Unreachable();
    do {
      Environment body_out = VisitScopedSubStmt(stmt->getBody(), block.in());
      Environment after_body =
          Environment::Merge(body_out, block.TakeContinueOut());
      cond_out = VisitStmt(stmt->getCond(), after_body);
      block.MergeIn(cond_out);
    } while (block.changed());
    if (!IsAlwaysTrueLoopCond(stmt->getCond())) {
      block.MergeOut(cond_out);
    }
    PopScope();
    return block.out();
  }

  DECL_VISIT_STMT(ForStmt) {
    state_.scopes.push_back(GCScope());
    Block block(VisitStmt(stmt->getInit(), env), this);
    Environment cond_out = Environment::Unreachable();
    do {
      Environment cond_var_out =
          VisitStmt(stmt->getConditionVariableDeclStmt(), block.in());
      cond_out = VisitStmt(stmt->getCond(), cond_var_out);
      Environment body_out = VisitScopedSubStmt(stmt->getBody(), cond_out);
      Environment after_body =
          Environment::Merge(body_out, block.TakeContinueOut());
      Environment inc_out = VisitStmt(stmt->getInc(), after_body);
      block.MergeIn(inc_out);
    } while (block.changed());
    if (!IsAlwaysTrueLoopCond(stmt->getCond())) {
      block.MergeOut(cond_out);
    }
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
    Environment cond_out = Environment::Unreachable();
    do {
      cond_out = VisitStmt(stmt->getCond(), block.in());
      Environment var_out = VisitStmt(stmt->getLoopVarStmt(), cond_out);
      Environment body_out = VisitScopedSubStmt(stmt->getBody(), var_out);
      Environment after_body =
          Environment::Merge(body_out, block.TakeContinueOut());
      Environment inc_out = VisitStmt(stmt->getInc(), after_body);
      block.MergeIn(inc_out);
    } while (block.changed());
    block.MergeOut(cond_out);
    PopScope();
    return block.out();
  }

  DECL_VISIT_STMT(IfStmt) {
    state_.scopes.push_back(GCScope());
    Environment init_out = VisitStmt(stmt->getInit(), env);
    Environment cond_var_out =
        VisitStmt(stmt->getConditionVariableDeclStmt(), init_out);
    Environment cond_out = VisitStmt(stmt->getCond(), cond_var_out);

    // For non-dependent `if constexpr`, only analyze the taken branch so
    // discarded branches cannot trigger false positives or kill variables.
    if (std::optional<clang::Stmt*> taken =
            stmt->getNondiscardedCase(ctx_->getASTContext())) {
      Environment branch_out = VisitScopedSubStmt(*taken, cond_out);
      PopScope();
      return branch_out;
    }

    std::vector<GCScope> saved_scopes = state_.scopes;

    Environment then_out = VisitScopedSubStmt(stmt->getThen(), cond_out);
    std::vector<GCScope> then_scopes = state_.scopes;

    state_.scopes = saved_scopes;

    Environment else_out = VisitScopedSubStmt(stmt->getElse(), cond_out);
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
    Environment init_out = VisitStmt(stmt->getInit(), env);
    Environment cond_var_out =
        VisitStmt(stmt->getConditionVariableDeclStmt(), init_out);
    Environment cond_out = VisitStmt(stmt->getCond(), cond_var_out);
    Block block(cond_out, this, /*is_loop=*/false);
    block.MergeOut(VisitStmt(stmt->getBody(), block.in()));
    bool has_default = false;
    for (const clang::SwitchCase* sc = stmt->getSwitchCaseList(); sc != nullptr;
         sc = sc->getNextSwitchCase()) {
      if (llvm::isa<clang::DefaultStmt>(sc)) {
        has_default = true;
        break;
      }
    }
    // Assume `-Wswitch`-clean exhaustive enum switches cover all runtime values
    // and cannot bypass the switch without executing a case.
    if (!has_default && !stmt->isAllEnumCasesCovered()) {
      block.MergeOut(cond_out);
    }
    PopScope();
    return block.out();
  }

  DECL_VISIT_STMT(CaseStmt) {
    const Block* switch_block =
        state_.block != nullptr ? state_.block->FindEnclosingSwitch() : nullptr;
    Environment in = switch_block != nullptr
                         ? Environment::Merge(env, switch_block->in())
                         : env;
    Environment after_lhs = VisitStmt(stmt->getLHS(), in);
    return VisitStmt(stmt->getSubStmt(), after_lhs);
  }

  DECL_VISIT_STMT(DefaultStmt) {
    const Block* switch_block =
        state_.block != nullptr ? state_.block->FindEnclosingSwitch() : nullptr;
    Environment in = switch_block != nullptr
                         ? Environment::Merge(env, switch_block->in())
                         : env;
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

  // Leaf `SlotBase` subclasses that can only ever point into the V8 heap (never
  // wrapping stack/handle addresses) and are unconditionally tracked in
  // `IsRawPointerToOnHeapValue` (`WriteProtectedSlot` only wraps `ObjectSlot`
  // and `ProtectedPointerSlot`, so exact canonical matching suffices):
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

  // Includes `IsAlwaysOnHeapSlotRecord` so both by-value slots
  // (`IsOnHeapValue`) and pointers to slots (`IsRawPointerType`, whose pointee
  // wraps an unrooted interior heap address) are tracked across GC.
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

  // Returns whether the given type is a raw pointer or a wrapper around
  // such. For V8 that means Object and MaybeObject instances.
  bool RepresentsRawPointerType(clang::QualType qtype) {
    // Not yet assigned pointers can't get moved by the GC.
    if (qtype.isNull()) return false;
    // nullptr can't get moved by the GC.
    if (qtype->isNullPtrType()) return false;

    if (const auto* pointer_type = qtype->getAs<clang::PointerType>()) {
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

  // True when analyzing a top-level function rather than summarizing a single
  // escaped member parameter of a callee. While summarizing a callee, ambient
  // type-based and heap-origin provenance is ignored so unrelated variables in
  // the callee do not produce false positives at the caller's call site.
  bool TrackAmbientRawTypes() const { return !state_.checking_callee_safety; }

  bool IsTrackedRawPointerType(clang::QualType qtype) {
    return TrackAmbientRawTypes() && RepresentsRawPointerType(qtype);
  }

  // Returns true if a field or array element of type `qtype` inside a local
  // aggregate can hold a raw member address (object/void pointer excluding
  // function pointers, unsigned pointer-width-or-wider integer such as
  // `Address`/`uintptr_t`, or a record type that is not a GC-rooted V8 handle
  // and contains at least one reference or address-capable base/field). Array
  // types are unwrapped to their base element type so `int counts[2]` is
  // excluded while `int* ptrs[2]` is included. Signed integers are excluded to
  // avoid false positives on ordinary signed counters/sizes; pointer-width
  // unsigned integers (including `size_t`, which canonically aliases
  // `Address`/`uintptr_t` on 64-bit targets) are conservatively included so
  // typedef erasure cannot drop `Address` fields.
  bool CanHoldRawMemberAddress(clang::QualType qtype) {
    if (qtype.isNull()) return false;
    qtype =
        ctx_->getASTContext().getBaseElementType(qtype.getNonReferenceType());
    if (qtype->isDependentType() ||
        (qtype->isPointerType() && !qtype->isFunctionPointerType())) {
      return true;
    }
    if (const clang::CXXRecordDecl* record = qtype->getAsCXXRecordDecl()) {
      const clang::CXXRecordDecl* def = record->getDefinition();
      if (def == nullptr || !def->isCompleteDefinition() ||
          def->isDependentContext()) {
        return true;
      }
      if ((decls_.handle_base != nullptr &&
           IsDerivedFrom(def, decls_.handle_base)) ||
          (decls_.direct_handle_base != nullptr &&
           IsDerivedFrom(def, decls_.direct_handle_base)) ||
          (decls_.api_indirect_handle_base != nullptr &&
           IsDerivedFrom(def, decls_.api_indirect_handle_base)) ||
          (decls_.api_direct_handle_base != nullptr &&
           IsDerivedFrom(def, decls_.api_direct_handle_base))) {
        return false;
      }
      const clang::CXXRecordDecl* canonical = def->getCanonicalDecl();
      auto [it, inserted] =
          can_hold_raw_member_cache_.try_emplace(canonical, false);
      if (!inserted) return it->second;
      bool result = IsOnHeapValue(def) || IsSlotRecord(def);
      if (!result) {
        for (const clang::CXXBaseSpecifier& base : def->bases()) {
          if (CanHoldRawMemberAddress(base.getType())) {
            result = true;
            break;
          }
        }
      }
      if (!result) {
        for (const clang::FieldDecl* field : def->fields()) {
          if (field->getType()->isReferenceType() ||
              CanHoldRawMemberAddress(field->getType())) {
            result = true;
            break;
          }
        }
      }
      can_hold_raw_member_cache_[canonical] = result;
      return result;
    }
    if (qtype->isRecordType()) return true;
    if (qtype->isUnsignedIntegerType() && !qtype->isBooleanType() &&
        !qtype->isEnumeralType() && !qtype->isCharType() &&
        !qtype->isWideCharType() && !qtype->isChar8Type() &&
        !qtype->isChar16Type() && !qtype->isChar32Type()) {
      return ctx_->getASTContext().getTypeSize(qtype) >=
             ctx_->getASTContext().getTypeSize(ctx_->getASTContext().VoidPtrTy);
    }
    return false;
  }

  bool IsCalleeSafetyCheckAborted() const {
    return state_.checking_callee_safety && state_.callee_unsafe;
  }

  // Unwraps `ReferenceType` only for ambient `*Member*` pointer and
  // always-on-heap slot parameters and non-const local lvalue references
  // (`const EmbedderDataSlot&`, `EmbedderDataSlot&`,
  // `TaggedMember<T>* const&`, `TaggedMember<T>*&`), matching
  // `IsAmbientMemberOrSlotVar` in `Use()`. Keeps `ReferenceType` on
  // `const`/rvalue local references (so null/empty temporaries like
  // `const EmbedderDataSlot& r = EmbedderDataSlot();` do not enter
  // `type_raw_vars_`) and on raw-value references like `Tagged<T>&` or
  // `HeapObject*&` (which `Use()` intentionally skips because reference
  // out-parameters like `bool Foo(Tagged<Object>& out)` lack `&out` call-site
  // syntax).
  clang::QualType UnwrapAmbientRefVarType(clang::QualType declared_type,
                                          bool is_param) {
    if (const auto* ref_type = declared_type->getAs<clang::ReferenceType>()) {
      clang::QualType pointee = ref_type->getPointeeType();
      if ((is_param || (declared_type->isLValueReferenceType() &&
                        !pointee.isConstant(ctx_->getASTContext()))) &&
          (IsPointerToOnHeapMemberType(pointee) ||
           IsAlwaysOnHeapSlotType(pointee))) {
        return pointee;
      }
    }
    return declared_type;
  }

  void RegisterDeclaredVar(const clang::ValueDecl* decl,
                           const clang::QualType& declared_type, bool is_param,
                           uint8_t raw_member_provenance,
                           bool is_on_heap_member_ref, Environment& env) {
    env.SetRawMemberProvenance(decl, raw_member_provenance);
    env.SetMemberRefVar(decl, is_on_heap_member_ref);

    clang::QualType var_type = UnwrapAmbientRefVarType(declared_type, is_param);
    bool is_type_raw = IsTrackedRawPointerType(var_type);
    if (is_type_raw) {
      Environment::MarkTypeRawVar(decl);
    }
    if (raw_member_provenance != 0 || is_on_heap_member_ref || is_type_raw) {
      env.MDefine(decl);
    }
    clang::QualType scope_type =
        is_param ? declared_type.getNonReferenceType() : declared_type;
    if (IsGCGuard(scope_type)) {
      state_.scopes.back().guard_location =
          clang::FullSourceLoc(decl->getLocation(), sm_);
    }
    if (IsConservativePinningScope(scope_type)) {
      state_.scopes.back().conservative_pinning_scope_location =
          clang::FullSourceLoc(decl->getLocation(), sm_);
    }
  }

  Environment VisitDecl(clang::Decl* decl, Environment& env) {
    if (IsCalleeSafetyCheckAborted()) return env;
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
      clang::QualType var_type =
          UnwrapAmbientRefVarType(declared_type, is_param);

      // Record raw member address provenance (`raw_member_vars_`) when:
      //   1. The initializer evaluates to an interior heap address, slot, or
      //      indirect container pointer (`kRawMemberAddress` /
      //      `kIndirectRawMemberAddress`):
      //        int* p = &obj->field_;
      //        uint8_t* bytes = obj->inline_bytes_;
      //        Address addr = obj->field_.storage_address();
      //        ObjectSlot slot(&obj->field_);
      //        ObjectSlot slot = obj->RawField(offset);
      //        EmbedderDataSlot slot(js_obj, 0);
      //        int** pp = ptrs;
      //   2. A parameter has a `*Member*` pointer type or an always-on-heap
      //      slot type (`void Foo(TaggedMember<T>* p)`,
      //      `void Bar(const EmbedderDataSlot& slot)`).
      //      (Local `*Member*` pointers and always-on-heap slots without a
      //      provenance-bearing initializer stay out of `raw_member_vars_` so
      //      `Init(&out)` / `Init(&slot)` works with the `UO_AddrOf`
      //      out-parameter heuristic; they are tracked as type-raw variables
      //      via `RepresentsRawPointerType` and re-acquire `kRawMemberAddress`
      //      in `Use()`, as well as via `CastExpr` for non-variable pointer
      //      casts/loads.)
      uint8_t raw_member_prov = init_effect.rawMemberProvenance();
      if (TrackAmbientRawTypes() && is_param &&
          (IsPointerToOnHeapMemberType(var_type) ||
           IsAlwaysOnHeapSlotType(var_type))) {
        raw_member_prov |= kRawMemberAddress;
      }

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
          ((raw_member_prov & kRawMemberAddress) &&
           IsSlotReferenceType(declared_type)) ||
          (TrackAmbientRawTypes() &&
           IsReferenceToOnHeapMemberType(declared_type));
      RegisterDeclaredVar(var, declared_type, is_param, raw_member_prov,
                          is_on_heap_member_ref, out);

      if (auto* decomp = llvm::dyn_cast<clang::DecompositionDecl>(var)) {
        for (clang::BindingDecl* binding : decomp->flat_bindings()) {
          ExprEffect binding_effect = ExprEffect::None();
          clang::VarDecl* holding_var = binding->getHoldingVar();
          if (holding_var != nullptr) {
            if (holding_var->hasInit()) {
              binding_effect = VisitExpr(holding_var->getInit(), out);
              out = out.ApplyEffect(binding_effect);
            }
          } else if (clang::Expr* binding_expr = binding->getBinding()) {
            binding_effect = VisitExpr(binding_expr, out);
            out = out.ApplyEffect(binding_effect);
          }

          clang::QualType binding_type = binding->getType();
          // A binding aliases an external lvalue when the decomposition itself
          // is a reference (`auto& [a, b] = ...`, where struct/array
          // `binding_type` is the non-reference field/element type) or when the
          // bound field/element itself has lvalue-reference type (e.g. `RefBox`
          // or `std::pair<int&, int>`; note tuple-like `holding_var` has `T&&`
          // reference type even for by-value copies per C++17
          // [dcl.struct.bind]).
          bool binding_is_ref =
              declared_type->isReferenceType() ||
              binding_type->isReferenceType() ||
              (holding_var != nullptr &&
               holding_var->getType()->isLValueReferenceType());
          uint8_t binding_raw_member_prov =
              binding_effect.rawMemberProvenance();
          bool binding_is_member_ref =
              (binding_is_ref && binding_effect.hasOnHeapMemberLValue()) ||
              ((binding_raw_member_prov & kRawMemberAddress) &&
               IsSlotReferenceType(binding_type)) ||
              (TrackAmbientRawTypes() &&
               IsReferenceToOnHeapMemberType(binding_type));
          RegisterDeclaredVar(binding, binding_type, /*is_param=*/false,
                              binding_raw_member_prov, binding_is_member_ref,
                              out);
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
    if (f->doesThisDeclarationHaveABody() && f->getBody() != nullptr) {
      Environment env;
      state_.scopes.push_back(GCScope());
      DefineAndVisitParameters(f, env);
      VisitStmt(f->getBody(), env);
      PopScope();
      state_.reported_diagnostics.clear();
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

  bool HasActiveGuardAt(const clang::FullSourceLoc loc) const {
    for (const auto& s : state_.scopes) {
      if (s.IsGuardBefore(loc)) return true;
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
    if (state_.checking_callee_safety) {
      state_.callee_unsafe = true;
      return;
    }
    clang::SourceLocation error_loc = expr->getExprLoc();
    // Deduplicate diagnostics within a function so multi-iteration loop
    // fixpoints do not emit repeated identical warnings on the same expression.
    if (!state_.reported_diagnostics.emplace(error_loc, msg).second) return;
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

    bool IsGuardBefore(const clang::FullSourceLoc loc) const {
      if (!guard_location.isValid() || !loc.isValid()) return false;
      return guard_location.isBeforeInTranslationUnitThan(loc);
    }

    bool IsAfterConservativePinningScope(
        const clang::FullSourceLoc decl_location) const {
      if (!conservative_pinning_scope_location.isValid() ||
          !decl_location.isValid()) {
        return false;
      }
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
    std::set<std::pair<clang::SourceLocation, std::string>>
        reported_diagnostics;
    Block* block = nullptr;
    bool checking_callee_safety = false;
    bool callee_unsafe = false;
    bool callee_hit_depth_limit = false;
  };

  // Maximum nested call-chain depth for intra-procedural callee summaries (8 is
  // well above observed V8 helper chains; deeper chains degrade to conservative
  // `unsafe`).
  static constexpr size_t kMaxCalleeSummaryDepth = 8;

  struct CalleeSummaryKey {
    const clang::FunctionDecl* canonical_decl;
    unsigned param_idx;
    uint8_t raw_member_provenance;
    bool is_member_ref;

    bool operator==(const CalleeSummaryKey&) const = default;
    std::strong_ordering operator<=>(const CalleeSummaryKey& other) const {
      // Built-in pointer `<=>` is unspecified for unrelated objects; use
      // `std::compare_three_way` for a strict total order in ordered maps/sets.
      if (auto cmp =
              std::compare_three_way{}(canonical_decl, other.canonical_decl);
          cmp != 0) {
        return cmp;
      }
      return std::tie(param_idx, raw_member_provenance, is_member_ref) <=>
             std::tie(other.param_idx, other.raw_member_provenance,
                      other.is_member_ref);
    }
  };

  // Isolates per-analysis state during an intra-procedural callee summary.
  // `Environment::type_raw_vars_` must be saved and cleared so an enclosing
  // `AnalyzeFunction(F)`'s type-tracked variables do not leak into
  // `CanRescueOneSidedLive` if `F` is summarized recursively; meanwhile
  // `Environment::symbol_table_` and `Environment::envs_` intentionally stay
  // append-only and are freed together when `AnalyzeFunction` calls
  // `ClearSymbolTable()`.
  class [[nodiscard]] CalleeSafetyScope {
   public:
    CalleeSafetyScope(FunctionAnalyzer* analyzer, const CalleeSummaryKey& key)
        : analyzer_(analyzer),
          key_(key),
          saved_state_(std::move(analyzer->state_)),
          saved_type_raw_vars_(std::move(Environment::type_raw_vars_)) {
      TRACE("Entering callee summary: "
            << key_.canonical_decl->getNameAsString() << " param "
            << key_.param_idx);
      Environment::type_raw_vars_.clear();
      assert(!llvm::is_contained(analyzer_->active_callee_checks_, key_));
      analyzer_->active_callee_checks_.push_back(key_);
      analyzer_->state_ = AnalysisState();
      analyzer_->state_.scopes.push_back(GCScope());
      analyzer_->state_.checking_callee_safety = true;
    }

    ~CalleeSafetyScope() {
      Environment::type_raw_vars_ = std::move(saved_type_raw_vars_);
      analyzer_->state_ = std::move(saved_state_);
      assert(!analyzer_->active_callee_checks_.empty() &&
             analyzer_->active_callee_checks_.back() == key_);
      analyzer_->active_callee_checks_.pop_back();
      TRACE("Leaving callee summary: " << key_.canonical_decl->getNameAsString()
                                       << " param " << key_.param_idx);
    }

    CalleeSafetyScope(const CalleeSafetyScope&) = delete;
    CalleeSafetyScope& operator=(const CalleeSafetyScope&) = delete;

   private:
    FunctionAnalyzer* analyzer_;
    CalleeSummaryKey key_;
    AnalysisState saved_state_;
    Environment::VarSet saved_type_raw_vars_;
  };

  clang::MangleContext* ctx_;
  KnownDecls decls_;
  clang::DiagnosticsEngine& d_;
  clang::SourceManager& sm_;

  AnalysisState state_;
  std::map<CalleeSummaryKey, bool> callee_safety_cache_;
  llvm::SmallVector<CalleeSummaryKey, kMaxCalleeSummaryDepth>
      active_callee_checks_;
  llvm::DenseMap<const clang::CXXRecordDecl*, bool> slot_base_cache_;
  llvm::DenseMap<const clang::CXXRecordDecl*, bool> on_heap_member_cache_;
  llvm::DenseMap<const clang::CXXRecordDecl*, bool>
      on_heap_class_or_member_cache_;
  llvm::DenseMap<const clang::CXXRecordDecl*, bool> can_hold_raw_member_cache_;
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
