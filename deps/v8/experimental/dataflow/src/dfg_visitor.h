// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DFG_VISITOR_H
#define DFG_VISITOR_H

#include <algorithm>
#include <array>
#include <functional>
#include <mutex>
#include <string>

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendActions.h"
#include "dfg.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "path_translator.h"

namespace dfg {

struct WrapperConfig;

// Sharded thread-safe set for cross-TU AST definition deduplication
class ConcurrentClaimSet {
 public:
  bool TryClaim(uint64_t Key) {
    size_t ShardIdx = (Key ^ (Key >> 32)) & (kNumShards - 1);
    std::lock_guard<std::mutex> Lock(Shards[ShardIdx].Mutex);
    return Shards[ShardIdx].Set.insert(Key).second;
  }

 private:
  static constexpr size_t kNumShards = 64;
  struct Shard {
    std::mutex Mutex;
    llvm::DenseSet<uint64_t> Set;
  };
  std::array<Shard, kNumShards> Shards;
};

class DfgVisitor : public clang::RecursiveASTVisitor<DfgVisitor> {
 public:
  explicit DfgVisitor(clang::ASTContext* Context, const std::string& InFile,
                      DfgGraph& Graph, const PathTranslator& Translator,
                      ConcurrentClaimSet* SharedClaims = nullptr)
      : Context(Context),
        Graph(Graph),
        Translator(Translator),
        InFile(InFile),
        SharedClaims(SharedClaims) {}

  // Visitor methods
  bool VisitVarDecl(clang::VarDecl* VD);
  bool VisitFieldDecl(clang::FieldDecl* FD);
  bool VisitUnaryOperator(clang::UnaryOperator* UO);
  bool VisitBinaryOperator(clang::BinaryOperator* BO);
  bool VisitCallExpr(clang::CallExpr* CE);
  bool VisitCXXConstructExpr(clang::CXXConstructExpr* CE);
  bool VisitInitListExpr(clang::InitListExpr* ILE);
  bool VisitCXXParenListInitExpr(clang::CXXParenListInitExpr* PLIE);
  bool VisitForStmt(clang::ForStmt* FS);
  bool VisitCXXForRangeStmt(clang::CXXForRangeStmt* FRS);
  bool VisitLambdaExpr(clang::LambdaExpr* LE);
  bool VisitReturnStmt(clang::ReturnStmt* RS);
  bool VisitConditionalOperator(clang::ConditionalOperator* CO);
  bool VisitBinaryConditionalOperator(clang::BinaryConditionalOperator* BCO);

  // 1. Enable template instantiation and implicit code traversal (critical for
  // C++ templates like Tagged<T>)
  bool shouldVisitTemplateInstantiations() const { return true; }
  bool shouldVisitImplicitCode() const { return true; }

  // Traverse methods to track context and build parameter
  // definition-to-declaration bridges
  bool TraverseDecl(clang::Decl* D);
  bool TraverseFunctionDecl(clang::FunctionDecl* FD);
  bool TraverseCXXMethodDecl(clang::CXXMethodDecl* MD);
  bool TraverseCXXConstructorDecl(clang::CXXConstructorDecl* CD);
  bool TraverseCXXDestructorDecl(clang::CXXDestructorDecl* DD);
  bool TraverseCXXConversionDecl(clang::CXXConversionDecl* CD);
  bool TraverseCXXForRangeStmt(clang::CXXForRangeStmt* FRS);

  // Structural AST body field resolution utilities (for BitField & container getters/setters)
  const clang::FieldDecl* FindBackingField(clang::CXXMethodDecl* MD);
  const clang::FieldDecl* FindSetterBackingField(clang::CXXMethodDecl* MD);
  const clang::FieldDecl* ResolveBackingField(const clang::CXXMethodDecl* MD);
  const clang::FieldDecl* ResolveSetterBackingField(
      const clang::CXXMethodDecl* MD);
  const clang::FieldDecl* ExtractBackingFieldFromExpr(clang::Expr* E,
                                                      unsigned Depth = 0);
  bool IsReadOnlyRootGetter(const clang::CXXMethodDecl* MD);

  struct OffsetFieldAccess {
    bool IsValid = false;
    bool IsStore = false;
    const clang::CXXRecordDecl* HolderRD = nullptr;
    int64_t Offset = -1;
    clang::QualType ValueType;
    clang::Expr* ValueExpr = nullptr;
    const clang::ValueDecl* OffsetVar = nullptr;
    clang::SourceLocation CallLoc;
    HeapTaint Taint = HeapTaint::None;
  };

  struct BitFieldInfo {
    bool IsValid = false;
    std::string OpName;  // "encode", "update", or "decode"
    int64_t Shift = 0;
    int64_t Size = 0;
    clang::QualType FieldType;
    clang::QualType BaseType;
    const clang::TypedefNameDecl* TypedefDecl = nullptr;
  };

  struct PendingBitFieldWrite {
    BitFieldInfo Info;
    Node::Id CallNodeId = 0;
    SourceLocation CallLoc;
    bool Flushed = false;
  };

 private:
  struct FunctionContext {
    const clang::FunctionDecl* Function = nullptr;
    unsigned CurrentStmtIndex = 0;
    std::unordered_map<const clang::Expr*, unsigned> ExprToId;
    llvm::DenseMap<const clang::VarDecl*, const clang::FieldDecl*>
        LocalVarToBackingField;
    llvm::DenseMap<const clang::Expr*, std::vector<size_t>> ExprBitFieldWrites;
    llvm::DenseMap<const clang::VarDecl*, std::vector<size_t>> VarBitFieldWrites;
    llvm::DenseSet<const clang::CallExpr*> ProcessedBitFieldCalls;
    std::vector<PendingBitFieldWrite> PendingWrites;
  };

  class FunctionContextScope {
   public:
    FunctionContextScope(DfgVisitor* Visitor, const clang::FunctionDecl* FD)
        : Visitor(Visitor) {
      FunctionContext Ctx;
      Ctx.Function = FD->getCanonicalDecl();
      Visitor->ContextStack.push_back(std::move(Ctx));
    }
    ~FunctionContextScope() {
      Visitor->FlushUnboundBitFieldWrites();
      Visitor->ContextStack.pop_back();
    }

   private:
    DfgVisitor* Visitor;
  };

  std::vector<FunctionContext> ContextStack;

  OffsetFieldAccess ExtractOffsetFieldAccess(clang::CallExpr* CE);
  Node::Id GetOrCreateOffsetFieldNode(const OffsetFieldAccess& Access);

  BitFieldInfo ExtractBitFieldInfo(clang::CallExpr* CE);
  Node::Id GetOrCreateBitFieldNode(const clang::FieldDecl* BackingField,
                                   const BitFieldInfo& Info);
  Node::Id GetOrCreateTypedefBitFieldNode(const BitFieldInfo& Info);
  void FlushBitFieldWriteToField(PendingBitFieldWrite& Write,
                                 const clang::FieldDecl* Field);
  void FlushRawWordWriteToRecordBitFields(clang::Expr* RhsExpr, Node::Id RhsId,
                                          const clang::FieldDecl* Field,
                                          EdgeKind Kind,
                                          const SourceLocation& Loc);
  void FlushUnboundBitFieldWrites();
  bool EnsureBitFieldCallProcessed(clang::CallExpr* CE);
  std::vector<size_t> CollectBitFieldWrites(clang::Expr* E, unsigned Depth = 0);

  // Private template helper to perform unified pre/post function AST traversal
  template <typename DeclT, typename BaseTraverseFn>
  bool TraverseAnyFunctionHelper(DeclT* FD, BaseTraverseFn BaseTraverse) {
    SourceLocation Loc = GetSourceLocation(FD->getLocation());
    if (Loc.File.empty()) return true;

    FunctionContextScope Scope(this, FD);

    clang::FunctionDecl* OldCurrent = CurrentFunction;
    CurrentFunction = static_cast<clang::FunctionDecl*>(FD->getCanonicalDecl());
    bool Ret = BaseTraverse(FD);
    CurrentFunction = OldCurrent;
    return Ret;
  }
  // Helper to find a WrapperConfig by walking the class hierarchy of a type
  const WrapperConfig* FindWrapperConfig(clang::QualType T);

  // Cache for resolved wrapper configurations to avoid redundant class
  // hierarchy walks
  llvm::DenseMap<const clang::Type*, const WrapperConfig*> WrapperConfigCache;

  clang::ASTContext* Context;
  DfgGraph& Graph;
  const PathTranslator& Translator;
  std::string InFile;
  ConcurrentClaimSet* SharedClaims = nullptr;
  unsigned UnknownCounter = 0;
  clang::FunctionDecl* CurrentFunction = nullptr;

  llvm::DenseSet<Node::Id> EmittedNodes;
  llvm::DenseSet<uint64_t> EmittedEdges;

  void EmitNode(Node N) {
    if (EmittedNodes.insert(N.ID).second) {
      Graph.Nodes.push_back(std::move(N));
    }
  }

  void EmitEdge(Edge E) {
    uint64_t Key = E.From ^ (E.To * 0x9ddfea08eb382d69ULL) ^
                   (static_cast<uint64_t>(E.Kind) * 0x517cc1b727220a95ULL);
    if (EmittedEdges.insert(Key).second) {
      Graph.Edges.push_back(std::move(E));
    }
  }

  // Telemetry Cache structures
  struct CachedFile {
    std::string RelPath;
    bool ShouldSkip = false;
  };
  llvm::DenseMap<unsigned, CachedFile> FileCache;

  const CachedFile& GetCachedFile(clang::FileID FID, clang::SourceManager& SM);

  // Phase 2 Caches
  llvm::DenseMap<const clang::Decl*, std::string> UsrCache;
  llvm::DenseMap<const void*, std::string> TypeCache;
  llvm::DenseMap<const clang::Decl*, Node::Id> DeclNodeCache;
  llvm::DenseMap<const clang::Expr*, Node::Id> ExprNodeCache;
  llvm::DenseMap<const clang::Expr*, int64_t> ForLoopIncConstantBound;
  llvm::DenseMap<const clang::Expr*, clang::Expr*> ForLoopIncBoundExpr;

  // Dynamic getter-to-backing-field and setter-to-backing-field lookup caches
  llvm::DenseMap<const clang::CXXMethodDecl*, const clang::FieldDecl*>
      GetterToField;
  llvm::DenseMap<const clang::CXXMethodDecl*, const clang::FieldDecl*>
      SetterToField;
  const clang::CXXRecordDecl* CachedReadOnlyRootsRD = nullptr;
  bool SearchedReadOnlyRootsRD = false;

  // Helper to get Unified Symbol Resolution (USR) for a Decl
  llvm::StringRef GetUSRForDecl(const clang::Decl* D);

  // Helper to get SourceLocation from Clang SourceLocation
  SourceLocation GetSourceLocation(clang::SourceLocation Loc);

  // Helper to get type string
  llvm::StringRef GetTypeString(clang::QualType QT);

  // Helper to find or create DFG node for a Decl
  Node::Id GetOrCreateNodeForDecl(clang::ValueDecl* D);

  // Helper to get DFG node ID for an Expression
  Node::Id GetNodeIdForExpr(clang::Expr* E);
  Node::Id GetNodeIdForExprImpl(clang::Expr* E);
  Node::Id GetNodeIdForDefaultArgExpr(
      clang::Expr* E, const clang::ParmVarDecl* FallbackParam = nullptr);

  // Helper to assign stable AST-relative index to an expression node
  Node::Id GetExprASTId(clang::Expr* E);

  // Thread-safe, globally unique ID generator
  Node::Id GenerateId(const std::string& Prefix, const SourceLocation& Loc);
};

class DfgConsumer : public clang::ASTConsumer {
 public:
  using FileCallback = std::function<void(const std::string& File)>;

  explicit DfgConsumer(clang::ASTContext* Context, const std::string& InFile,
                       DfgGraph& Graph, const PathTranslator& Translator,
                       FileCallback Callback = nullptr,
                       ConcurrentClaimSet* SharedClaims = nullptr)
      : Visitor(Context, InFile, Graph, Translator, SharedClaims),
        InFile(InFile),
        Graph(Graph),
        Callback(Callback) {}

  void HandleTranslationUnit(clang::ASTContext& Context) override;

 private:
  DfgVisitor Visitor;
  std::string InFile;
  DfgGraph& Graph;
  FileCallback Callback;
};

class DfgAction : public clang::ASTFrontendAction {
 public:
  using FileCallback = std::function<void(const std::string& File)>;

  explicit DfgAction(DfgGraph& Graph, const PathTranslator& Translator,
                     FileCallback Callback = nullptr,
                     ConcurrentClaimSet* SharedClaims = nullptr)
      : Graph(Graph),
        Translator(Translator),
        Callback(Callback),
        SharedClaims(SharedClaims) {}

  std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
      clang::CompilerInstance& Compiler, llvm::StringRef InFile) override {
    return std::make_unique<DfgConsumer>(
        &Compiler.getASTContext(), InFile.str(), Graph, Translator, Callback,
        SharedClaims);
  }

 private:
  DfgGraph& Graph;
  const PathTranslator& Translator;
  FileCallback Callback;
  ConcurrentClaimSet* SharedClaims = nullptr;
};

}  // namespace dfg

#endif  // DFG_VISITOR_H
