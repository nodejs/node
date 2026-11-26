// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dfg_visitor.h"

#include <filesystem>
#include <iostream>

#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/Expr.h"
#include "clang/AST/Stmt.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Index/USRGeneration.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "wrapper_config.h"

namespace dfg {

// Helper to resolve and cache FileID to relative path
const DfgVisitor::CachedFile& DfgVisitor::GetCachedFile(
    clang::FileID FID, clang::SourceManager& SM) {
  unsigned RawFID = FID.getHashValue();
  auto it = FileCache.find(RawFID);
  if (it != FileCache.end()) {
    return it->second;
  }

  CachedFile CF;
  clang::SourceLocation StartLoc = SM.getLocForStartOfFile(FID);
  if (StartLoc.isInvalid()) {
    CF.ShouldSkip = true;
    return FileCache[RawFID] = CF;
  }

  std::string Filename = SM.getFilename(StartLoc).str();
  if (Filename.empty()) {
    CF.ShouldSkip = true;
    return FileCache[RawFID] = CF;
  }

  std::filesystem::path FilePath(Filename);
  if (FilePath.is_relative()) {
    if (auto Cwd = SM.getFileManager()
                       .getVirtualFileSystem()
                       .getCurrentWorkingDirectory()) {
      FilePath = std::filesystem::path(*Cwd) / FilePath;
    }
  }
  try {
    Filename = std::filesystem::weakly_canonical(FilePath).string();
  } catch (...) {
    Filename = FilePath.lexically_normal().string();
  }

  std::string RelFile = Translator.ToRelative(Filename);
  if (!Translator.IsEmpty() && std::filesystem::path(RelFile).is_absolute()) {
    CF.ShouldSkip = true;
  } else {
    CF.RelPath = RelFile;
    CF.ShouldSkip = false;
  }

  return FileCache[RawFID] = CF;
}

// Optimized helper to deconstruct Clang SourceLocation
SourceLocation DfgVisitor::GetSourceLocation(clang::SourceLocation Loc) {
  if (Loc.isInvalid()) return SourceLocation();

  clang::SourceManager& SM = Context->getSourceManager();
  clang::SourceLocation FileLoc = SM.getFileLoc(Loc);
  if (FileLoc.isInvalid()) return SourceLocation();

  // 1. Fast system header check (Early Exit)
  if (SM.isInSystemHeader(FileLoc)) {
    return SourceLocation();
  }

  clang::FileID FID = SM.getFileID(FileLoc);
  if (FID.isInvalid()) return SourceLocation();

  // 2. Use cached file resolution
  const CachedFile& CF = GetCachedFile(FID, SM);
  if (CF.ShouldSkip) {
    return SourceLocation();
  }

  SourceLocation Result;
  Result.File = CF.RelPath;
  Result.Line = SM.getSpellingLineNumber(FileLoc);
  Result.Column = SM.getSpellingColumnNumber(FileLoc);

  // 3. Optimized Macro spelling resolution using cache
  if (Loc.isMacroID()) {
    clang::SourceLocation SpellingLoc = SM.getSpellingLoc(Loc);
    if (SpellingLoc.isValid()) {
      clang::FileID SpellingFID = SM.getFileID(SpellingLoc);
      if (SpellingFID.isValid()) {
        const CachedFile& SpCF = GetCachedFile(SpellingFID, SM);
        if (!SpCF.ShouldSkip) {
          std::string RelSpellingFile = SpCF.RelPath;
          for (char& c : RelSpellingFile) {
            if (c == '/' || c == '.' || c == '-') {
              c = '_';
            }
          }
          Result.MacroSpelling =
              RelSpellingFile + "_" +
              std::to_string(SM.getSpellingLineNumber(SpellingLoc)) + "_" +
              std::to_string(SM.getSpellingColumnNumber(SpellingLoc));
        }
      }
    }
  }

  return Result;
}

llvm::StringRef DfgVisitor::GetTypeString(clang::QualType QT) {
  if (QT.isNull()) return "";
  const void* Key = QT.getAsOpaquePtr();
  auto it = TypeCache.find(Key);
  if (it != TypeCache.end()) {
    return it->second;
  }
  auto insert_result = TypeCache.insert({Key, QT.getAsString()});
  return insert_result.first->second;
}

static const clang::Decl* UnwrapTemplatePattern(const clang::Decl* D) {
  if (!D) return nullptr;
  if (auto* FD = llvm::dyn_cast<clang::FunctionDecl>(D)) {
    if (const clang::FunctionDecl* Pattern =
            FD->getTemplateInstantiationPattern()) {
      return Pattern->getCanonicalDecl();
    }
    if (const clang::FunctionTemplateDecl* FTD = FD->getPrimaryTemplate()) {
      if (const clang::FunctionDecl* TemplatedFD =
              FTD->getCanonicalDecl()->getTemplatedDecl()) {
        return TemplatedFD->getCanonicalDecl();
      }
    }
    return FD->getCanonicalDecl();
  }
  return D->getCanonicalDecl();
}

llvm::StringRef DfgVisitor::GetUSRForDecl(const clang::Decl* D) {
  if (!D) return "";
  D = D->getCanonicalDecl();
  auto it = UsrCache.find(D);
  if (it != UsrCache.end()) {
    return it->second;
  }

  if (auto* PVD = llvm::dyn_cast<clang::ParmVarDecl>(D)) {
    if (const auto* FD =
            llvm::dyn_cast_or_null<clang::FunctionDecl>(PVD->getDeclContext())) {
      llvm::StringRef FuncUSR = GetUSRForDecl(FD);
      SourceLocation ParamLoc = GetSourceLocation(PVD->getLocation());
      if (!FuncUSR.empty() && !ParamLoc.File.empty()) {
        std::string Usr = FuncUSR.str() + "/param_" +
                          std::to_string(PVD->getFunctionScopeIndex()) + "@" +
                          ParamLoc.File + ":" + std::to_string(ParamLoc.Line) +
                          ":" + std::to_string(ParamLoc.Column);
        auto insert_result = UsrCache.insert({D, std::move(Usr)});
        return insert_result.first->second;
      }
    }
  }

  std::string Usr = "";
  llvm::SmallString<128> Buf;
  if (clang::index::generateUSRForDecl(D, Buf) == 0) {
    Usr = Buf.str().str();
  }

  if (Usr.empty()) {
    if (auto* PVD = llvm::dyn_cast<clang::ParmVarDecl>(D)) {
      if (const auto* FD =
              llvm::dyn_cast_or_null<clang::FunctionDecl>(PVD->getDeclContext())) {
        llvm::StringRef FuncUSR = GetUSRForDecl(FD);
        if (!FuncUSR.empty()) {
          Usr = FuncUSR.str() + "/param_" +
                std::to_string(PVD->getFunctionScopeIndex());
        }
      }
    }
  }

  if (Usr.empty()) {
    // Semantic fallback for named unresolved template declarations where USR fails.
    // Never generate "sem:" for unnamed declarations (where getName() is empty),
    // or all unnamed declarations across the codebase would collide on "sem:".
    if (auto* ND = llvm::dyn_cast<clang::NamedDecl>(D)) {
      if (!ND->getName().empty()) {
        std::string Sig = ND->getQualifiedNameAsString();
        if (!Sig.empty()) {
          if (auto* FD = llvm::dyn_cast<clang::FunctionDecl>(D)) {
            Sig += "#";
            for (unsigned i = 0; i < FD->getNumParams(); ++i) {
              Sig += GetTypeString(FD->getParamDecl(i)->getType()).str() + "#";
            }
          }
          Usr = "sem:" + Sig;
        }
      }
    }
  }
  auto insert_result = UsrCache.insert({D, std::move(Usr)});
  return insert_result.first->second;
}

static HeapTaint ClassifyRecordHeapTaintImpl(const clang::CXXRecordDecl* RD,
                                             unsigned Depth) {
  if (!RD || Depth > 12) return HeapTaint::None;
  llvm::StringRef Name =
      RD->getIdentifier() ? RD->getName() : llvm::StringRef();
  if (Name == "TrustedObject" || Name == "TrustedObjectLayout" ||
      Name == "ExposedTrustedObject" || Name == "ExposedTrustedObjectLayout" ||
      Name == "TrustedFixedArray" || Name == "TrustedByteArray" ||
      Name == "TrustedWeakFixedArray" || Name == "ProtectedFixedArray" ||
      Name == "ProtectedWeakFixedArray") {
    return HeapTaint::TrustedSpace;
  }

  bool FoundInSandbox =
      (Name == "HeapObject" || Name == "HeapObjectLayout" || Name == "Object" ||
       Name == "Struct" || Name == "StructLayout" ||
       Name == "FixedArrayBase" || Name == "FixedArray" ||
       Name == "GarbageCollected" || Name == "GarbageCollectedMixin" ||
       Name == "HeapObjectHeader" || Name.starts_with("TorqueGenerated"));

  if (const auto* CTSD =
          llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(RD)) {
    const auto& Args = CTSD->getTemplateArgs();
    for (unsigned i = 0; i < Args.size(); ++i) {
      if (Args[i].getKind() == clang::TemplateArgument::Type) {
        clang::QualType ArgQT = Args[i].getAsType();
        if (!ArgQT.isNull()) {
          if (const auto* ArgRD = ArgQT->getAsCXXRecordDecl()) {
            HeapTaint ArgTaint = ClassifyRecordHeapTaintImpl(ArgRD, Depth + 1);
            if (ArgTaint == HeapTaint::TrustedSpace) {
              return HeapTaint::TrustedSpace;
            }
            if (ArgTaint == HeapTaint::InSandbox) {
              FoundInSandbox = true;
            }
          }
        }
      }
    }
  }

  if (RD->hasDefinition()) {
    const clang::CXXRecordDecl* Def = RD->getDefinition();
    for (const auto& Base : Def->bases()) {
      if (Base.getType().isNull()) continue;
      if (const auto* BaseRD = Base.getType()->getAsCXXRecordDecl()) {
        HeapTaint BaseTaint = ClassifyRecordHeapTaintImpl(BaseRD, Depth + 1);
        if (BaseTaint == HeapTaint::TrustedSpace) {
          return HeapTaint::TrustedSpace;
        }
        if (BaseTaint == HeapTaint::InSandbox) {
          FoundInSandbox = true;
        }
      }
    }
  }

  if (const clang::DeclContext* DC = RD->getDeclContext()) {
    if (const auto* ParentRD = llvm::dyn_cast<clang::CXXRecordDecl>(DC)) {
      HeapTaint ParentTaint = ClassifyRecordHeapTaintImpl(ParentRD, Depth + 1);
      if (ParentTaint == HeapTaint::TrustedSpace) {
        return HeapTaint::TrustedSpace;
      }
      if (ParentTaint == HeapTaint::InSandbox) {
        FoundInSandbox = true;
      }
    }
  }

  return FoundInSandbox ? HeapTaint::InSandbox : HeapTaint::None;
}

static HeapTaint ClassifyRecordHeapTaint(const clang::CXXRecordDecl* RD) {
  return ClassifyRecordHeapTaintImpl(RD, 0);
}

static HeapTaint ClassifyFieldHeapTaint(const clang::FieldDecl* FD) {
  if (!FD) return HeapTaint::None;
  HeapTaint RecordTaint = ClassifyRecordHeapTaint(
      llvm::dyn_cast_or_null<clang::CXXRecordDecl>(FD->getParent()));
  if (RecordTaint == HeapTaint::TrustedSpace) {
    return HeapTaint::TrustedSpace;
  }
  std::string TypeStr = FD->getType().getAsString();
  if (TypeStr.find("ProtectedTaggedMember") != std::string::npos ||
      TypeStr.find("TrustedPointerMember") != std::string::npos) {
    return HeapTaint::TrustedSpace;
  }
  if (RecordTaint != HeapTaint::None) {
    return RecordTaint;
  }
  if (TypeStr.find("TaggedMember") != std::string::npos ||
      TypeStr.find("ExternalPointerMember") != std::string::npos ||
      TypeStr.find("JSDispatchHandleMember") != std::string::npos) {
    return HeapTaint::InSandbox;
  }
  return HeapTaint::None;
}

Node::Id DfgVisitor::GetOrCreateNodeForDecl(clang::ValueDecl* D) {
  if (!D) return 0;
  auto it = DeclNodeCache.find(D);
  if (it != DeclNodeCache.end()) {
    return it->second;
  }

  SourceLocation Loc = GetSourceLocation(D->getLocation());
  if (Loc.File.empty()) {
    DeclNodeCache[D] = 0;
    return 0;
  }
  NodeKind Kind = NodeKind::Unknown;

  if (llvm::isa<clang::ParmVarDecl>(D)) {
    Kind = NodeKind::Parameter;
  } else if (auto* VD = llvm::dyn_cast<clang::VarDecl>(D)) {
    if (VD->isImplicit() && !llvm::isa<clang::DecompositionDecl>(VD)) {
      DeclNodeCache[D] = 0;
      return 0;
    }
    Kind = NodeKind::Variable;
  } else if (llvm::isa<clang::BindingDecl>(D)) {
    Kind = NodeKind::Variable;
  } else if (auto* FD = llvm::dyn_cast<clang::FieldDecl>(D)) {
    if (FD->getName().empty()) {
      DeclNodeCache[D] = 0;
      return 0;
    }
    Kind = NodeKind::Field;
    clang::CXXRecordDecl* Parent =
        llvm::dyn_cast<clang::CXXRecordDecl>(FD->getParent());
    if (Parent) {
      std::string ClassName = Parent->getNameAsString();
      if (WrapperConfigManager::Get().IsWrapperClass(ClassName)) {
        DeclNodeCache[D] = 0;
        return 0;
      }
    }
  } else if (llvm::isa<clang::EnumConstantDecl>(D)) {
    Kind = NodeKind::Literal;
  }

  llvm::StringRef UsrID = GetUSRForDecl(D);
  Node::Id ID;
  if (UsrID.empty()) {
    ID = GenerateId(D->getNameAsString(), Loc);
  } else {
    ID = HashFnv1a64(UsrID);
  }

  Node N;
  N.ID = ID;
  N.Kind = Kind;
  N.Name = D->getNameAsString();
  N.Type = GetTypeString(D->getType()).str();
  if (auto* ECD = llvm::dyn_cast<clang::EnumConstantDecl>(D)) {
    llvm::SmallString<32> ValBuf;
    ECD->getInitVal().toString(ValBuf, 10);
    N.Type += " = " + ValBuf.str().str();
  } else if (auto* VD = llvm::dyn_cast<clang::VarDecl>(D)) {
    if (!llvm::isa<clang::ParmVarDecl>(VD) && !VD->getType().isNull() &&
        VD->getType()->isIntegralOrEnumerationType() &&
        (VD->isConstexpr() || VD->getType().isConstQualified())) {
      if (const clang::Expr* Init = VD->getAnyInitializer()) {
        if (!Init->isValueDependent()) {
          clang::Expr::EvalResult Eval;
          if (Init->EvaluateAsInt(Eval, *Context)) {
            llvm::SmallString<32> ValBuf;
            Eval.Val.getInt().toString(ValBuf, 10);
            N.Type += " = " + ValBuf.str().str();
          }
        }
      }
    }
  } else if (auto* FD = llvm::dyn_cast<clang::FieldDecl>(D)) {
    if (FD->isBitField() && FD->getBitWidth() &&
        !FD->getBitWidth()->isValueDependent()) {
      unsigned BitWidth = FD->getBitWidthValue();
      if (BitWidth > 0) {
        N.Type += " : " + std::to_string(BitWidth);
      }
    }
    N.Type += HeapTaintTypeSuffix(ClassifyFieldHeapTaint(FD));
  }
  N.Loc = Loc;
  EmitNode(std::move(N));

  DeclNodeCache[D] = ID;
  return ID;
}

// Implement the thread-safe, globally unique GenerateId member function
Node::Id DfgVisitor::GenerateId(const std::string& Prefix,
                                const SourceLocation& Loc) {
  std::string raw_id;
  if (Loc.IsValid()) {
    raw_id = Prefix + "_" + Loc.File + "_" + std::to_string(Loc.Line) + "_" +
             std::to_string(Loc.Column);
    if (!Loc.MacroSpelling.empty()) {
      raw_id += "_" + Loc.MacroSpelling;
    }
  } else {
    // Thread-safe and globally unique across different files!
    raw_id = Prefix + "_" + Translator.ToRelative(InFile) + "_unknown_" +
             std::to_string(UnknownCounter++);
  }
  return HashFnv1a64(raw_id);
}

// (IsTransparentCastOrMove has been removed in favor of
// WrapperConfigManager::IsIdentityFunction)

// Helper to peel off implicit/explicit casts, parentheses, and other wrappers
// recursively
static clang::Expr* PeelExpr(clang::Expr* E) {
  while (E) {
    if (auto* CE = llvm::dyn_cast<clang::CastExpr>(E)) {
      if (CE->getCastKind() == clang::CK_IntegralToPointer) {
        break;
      }
      E = CE->getSubExpr();
    } else if (auto* PE = llvm::dyn_cast<clang::ParenExpr>(E)) {
      E = PE->getSubExpr();
    } else if (auto* BTE = llvm::dyn_cast<clang::CXXBindTemporaryExpr>(E)) {
      E = BTE->getSubExpr();
    } else if (auto* EWC = llvm::dyn_cast<clang::ExprWithCleanups>(E)) {
      E = EWC->getSubExpr();
    } else if (auto* MTE = llvm::dyn_cast<clang::MaterializeTemporaryExpr>(E)) {
      E = MTE->getSubExpr();
    } else if (auto* DAE = llvm::dyn_cast<clang::CXXDefaultArgExpr>(E)) {
      E = DAE->getExpr();
    } else if (auto* DIE = llvm::dyn_cast<clang::CXXDefaultInitExpr>(E)) {
      E = DIE->getExpr();
    } else if (auto* SILE =
                   llvm::dyn_cast<clang::CXXStdInitializerListExpr>(E)) {
      E = SILE->getSubExpr();
    } else if (auto* SNTTPE =
                   llvm::dyn_cast<clang::SubstNonTypeTemplateParmExpr>(E)) {
      E = SNTTPE->getReplacement();
    } else if (auto* ASE = llvm::dyn_cast<clang::ArraySubscriptExpr>(E)) {
      E = ASE->getBase();
    } else if (auto* CNE = llvm::dyn_cast<clang::CXXNewExpr>(E)) {
      const clang::CXXConstructExpr* CCE = CNE->getConstructExpr();
      if (CCE && CCE->getNumArgs() > 0 &&
          !CCE->getArg(0)->isDefaultArgument()) {
        E = const_cast<clang::CXXConstructExpr*>(CCE);
      } else if (clang::Expr* Init = CNE->getInitializer()) {
        if (!llvm::isa<clang::CXXConstructExpr>(Init)) {
          E = Init;
        } else {
          break;
        }
      } else {
        break;
      }
    } else if (auto* ILE = llvm::dyn_cast<clang::InitListExpr>(E)) {
      if (ILE->getNumInits() == 1) {
        E = ILE->getInit(0);
      } else {
        break;
      }
    } else if (auto* UO = llvm::dyn_cast<clang::UnaryOperator>(E)) {
      if (UO->getOpcode() == clang::UO_AddrOf ||
          UO->getOpcode() == clang::UO_Deref) {
        E = UO->getSubExpr();
      } else {
        break;
      }
    } else {
      break;
    }
  }
  return E;
}

const WrapperConfig* DfgVisitor::FindWrapperConfig(clang::QualType T) {
  if (T.isNull()) return nullptr;

  // Peel pointers and references to get the underlying class type
  T = T.getNonReferenceType();
  if (T->isPointerType()) {
    T = T->getPointeeType();
  }

  // Get the canonical type pointer to use as a stable O(1) cache key
  const clang::Type* CanonicalTypePtr = T.getCanonicalType().getTypePtr();

  // Check the cache first (including cached negative lookups)
  auto it = WrapperConfigCache.find(CanonicalTypePtr);
  if (it != WrapperConfigCache.end()) {
    return it->second;
  }

  clang::CXXRecordDecl* RD = T->getAsCXXRecordDecl();
  if (!RD) {
    WrapperConfigCache[CanonicalTypePtr] = nullptr;
    return nullptr;
  }

  // Helper lambda to perform the actual class hierarchy lookup
  auto PerformLookup = [this, RD, T]() -> const WrapperConfig* {
    // 1. Check the class itself (using its type name)
    std::string TypeStr = GetTypeString(T).str();
    const WrapperConfig* Config =
        WrapperConfigManager::Get().GetConfigForType(TypeStr);
    if (Config) return Config;

    if (RD) {
      std::string RecordName = RD->getNameAsString();
      if (!RecordName.empty()) {
        const WrapperConfig* RecordConfig =
            WrapperConfigManager::Get().GetConfigForType(RecordName);
        if (RecordConfig) return RecordConfig;
      }
    }

    // 2. Walk the base classes recursively
    if (RD->hasDefinition()) {
      for (const auto& Base : RD->bases()) {
        const WrapperConfig* BaseConfig = FindWrapperConfig(Base.getType());
        if (BaseConfig) return BaseConfig;
      }
    }
    return nullptr;
  };

  const WrapperConfig* Result = PerformLookup();
  WrapperConfigCache[CanonicalTypePtr] = Result;
  return Result;
}

Node::Id DfgVisitor::GetExprASTId(clang::Expr* E) {
  if (ContextStack.empty()) {
    return 0;
  }
  auto& ActiveCtx = ContextStack.back();
  unsigned index = 0;
  auto it = ActiveCtx.ExprToId.find(E);
  if (it != ActiveCtx.ExprToId.end()) {
    index = it->second;
  } else {
    index = ActiveCtx.CurrentStmtIndex++;
    ActiveCtx.ExprToId[E] = index;
  }
  std::string ParentUSR = GetUSRForDecl(ActiveCtx.Function).str();
  if (ParentUSR.empty()) {
    ParentUSR = "unknown_function";
  }
  std::string CalleeSuffix;
  if (auto* CE = llvm::dyn_cast<clang::CallExpr>(E)) {
    if (auto* FD = CE->getDirectCallee()) {
      CalleeSuffix = "_" + FD->getNameAsString();
    }
  } else if (auto* CCE = llvm::dyn_cast<clang::CXXConstructExpr>(E)) {
    if (auto* CD = CCE->getConstructor()) {
      CalleeSuffix = "_" + CD->getNameAsString();
    }
  } else if (auto* BO = llvm::dyn_cast<clang::BinaryOperator>(E)) {
    CalleeSuffix = "_" + BO->getOpcodeStr().str();
  } else if (auto* UO = llvm::dyn_cast<clang::UnaryOperator>(E)) {
    CalleeSuffix = "_" + UO->getOpcodeStr(UO->getOpcode()).str();
  }
  std::string raw_id = ParentUSR + "/expr_" +
                       std::string(E->getStmtClassName()) + CalleeSuffix + "_" +
                       std::to_string(index);
  return HashFnv1a64(raw_id);
}

Node::Id DfgVisitor::GetNodeIdForExpr(clang::Expr* E) {
  E = PeelExpr(E);
  if (!E) return 0;

  auto it = ExprNodeCache.find(E);
  if (it != ExprNodeCache.end()) {
    return it->second;
  }

  Node::Id ID = GetNodeIdForExprImpl(E);
  ExprNodeCache[E] = ID;
  return ID;
}

Node::Id DfgVisitor::GetNodeIdForDefaultArgExpr(
    clang::Expr* E, const clang::ParmVarDecl* FallbackParam) {
  if (auto* DAE = llvm::dyn_cast_or_null<clang::CXXDefaultArgExpr>(E)) {
    if (!FallbackParam) {
      FallbackParam = DAE->getParam();
    }
    E = DAE->getExpr();
  }
  auto ExtractParamDefaultExpr =
      [](const clang::ParmVarDecl* PVD) -> clang::Expr* {
    if (!PVD || !PVD->hasDefaultArg() || PVD->hasUnparsedDefaultArg()) {
      return nullptr;
    }
    if (PVD->hasUninstantiatedDefaultArg()) {
      return const_cast<clang::Expr*>(PVD->getUninstantiatedDefaultArg());
    }
    return const_cast<clang::Expr*>(PVD->getDefaultArg());
  };
  if (!E && FallbackParam) {
    E = ExtractParamDefaultExpr(FallbackParam);
    if (!E) {
      if (const auto* FD =
              llvm::dyn_cast_or_null<clang::FunctionDecl>(
                  FallbackParam->getDeclContext())) {
        unsigned ParamIdx = FallbackParam->getFunctionScopeIndex();
        if (const clang::FunctionDecl* PatternFD =
                llvm::dyn_cast_or_null<clang::FunctionDecl>(
                    UnwrapTemplatePattern(FD))) {
          for (const clang::FunctionDecl* RedeFD : PatternFD->redecls()) {
            if (ParamIdx < RedeFD->getNumParams()) {
              E = ExtractParamDefaultExpr(RedeFD->getParamDecl(ParamIdx));
              if (E) break;
            }
          }
        }
      }
    }
  }
  if (!E) return 0;

  Node::Id Id = GetNodeIdForExpr(E);
  if (Id != 0) return Id;

  clang::Expr* Peeled = PeelExpr(E);
  if (!Peeled) return 0;

  SourceLocation Loc = GetSourceLocation(Peeled->getBeginLoc());
  if (Loc.File.empty() && FallbackParam) {
    Loc = GetSourceLocation(FallbackParam->getLocation());
  }
  if (Loc.File.empty()) return 0;

  std::string Name;
  NodeKind Kind = NodeKind::Literal;
  if (llvm::isa<clang::CXXNullPtrLiteralExpr>(Peeled) ||
      llvm::isa<clang::GNUNullExpr>(Peeled)) {
    Name = "nullptr";
  } else if (llvm::isa<clang::CXXScalarValueInitExpr>(Peeled) ||
             llvm::isa<clang::ImplicitValueInitExpr>(Peeled) ||
             llvm::isa<clang::InitListExpr>(Peeled)) {
    Name = "{}";
  } else if (auto* CCE = llvm::dyn_cast<clang::CXXConstructExpr>(Peeled)) {
    if (auto* CD = CCE->getConstructor()) {
      Name = CD->getNameAsString() + "()";
      Kind = NodeKind::Expression;
    }
  }
  if (Name.empty()) return 0;

  Node::Id FallbackId =
      HashFnv1a64("default_arg@" + Loc.File + ":" + std::to_string(Loc.Line) +
                  ":" + std::to_string(Loc.Column) + ":" + Name);
  Node N;
  N.ID = FallbackId;
  N.Kind = Kind;
  N.Name = Name;
  N.Type = GetTypeString(Peeled->getType()).str();
  N.Loc = Loc;
  EmitNode(std::move(N));
  return FallbackId;
}

static HeapTaint ClassifyDirectHeapReadCall(
    clang::CallExpr* CE, clang::FunctionDecl* FD,
    clang::FunctionDecl* CurrentFunction) {
  if (!CE || !FD || !FD->getIdentifier()) return HeapTaint::None;
  if (auto* MD = llvm::dyn_cast<clang::CXXMethodDecl>(FD)) {
    if (MD->isStatic()) {
      if (auto* CTSD =
              llvm::dyn_cast_or_null<clang::ClassTemplateSpecializationDecl>(
                  MD->getParent())) {
        if (CTSD->getIdentifier() && CTSD->getName() == "TaggedField") {
          llvm::StringRef Op = MD->getName();
          if (Op == "load" || Op == "Relaxed_Load" || Op == "Acquire_Load" ||
              Op == "SeqCst_Load" || Op == "Acquire_Load_No_Unpack" ||
              Op == "Relaxed_Load_Map_Word" || Op == "Relaxed_Load_Raw") {
            return HeapTaint::InSandbox;
          }
        }
      }
    }
  }
  llvm::StringRef Name = FD->getName();
  if (Name == "ReadTrustedPointerField" ||
      Name == "ReadProtectedPointerField") {
    return HeapTaint::TrustedSpace;
  }
  if (Name == "ReadField" || Name == "Relaxed_ReadField" ||
      Name == "Acquire_ReadField" || Name == "SeqCst_ReadField" ||
      Name == "ReadSandboxedPointerField" || Name == "ReadBoundedSizeField" ||
      Name == "ReadExternalPointerField" || Name == "ReadCppHeapPointerField") {
    if (auto* MCE = llvm::dyn_cast<clang::CXXMemberCallExpr>(CE)) {
      if (clang::Expr* Recv = MCE->getImplicitObjectArgument()) {
        clang::QualType T = Recv->IgnoreParenImpCasts()->getType();
        if (!T.isNull()) {
          T = T.getNonReferenceType();
          if (T->isPointerType()) T = T->getPointeeType();
          if (!T.isNull()) {
            HeapTaint RecvTaint =
                ClassifyRecordHeapTaint(T->getAsCXXRecordDecl());
            if (RecvTaint == HeapTaint::TrustedSpace) {
              return HeapTaint::TrustedSpace;
            }
          }
        }
      }
    } else if (auto* CurMD =
                   llvm::dyn_cast_or_null<clang::CXXMethodDecl>(CurrentFunction)) {
      HeapTaint ParentTaint = ClassifyRecordHeapTaint(CurMD->getParent());
      if (ParentTaint == HeapTaint::TrustedSpace) {
        return HeapTaint::TrustedSpace;
      }
    }
    return HeapTaint::InSandbox;
  }
  return HeapTaint::None;
}

Node::Id DfgVisitor::GetNodeIdForExprImpl(clang::Expr* E) {
  E = PeelExpr(E);
  if (!E) return 0;

  SourceLocation Loc = GetSourceLocation(E->getBeginLoc());
  if (Loc.File.empty()) return 0;

  // 1. Handle CXXConstructExpr for configured wrapper load/pass-through
  // constructors (e.g. Tagged<T>, Handle<T>, std::shared_ptr<T>), copy/move
  // constructors, and non-wrapper constructors (e.g. Descriptor, PropertyDetails)
  if (auto* CE = llvm::dyn_cast<clang::CXXConstructExpr>(E)) {
    const WrapperConfig* Config = FindWrapperConfig(CE->getType());
    if (Config) {
      const WrapperOp* Op = Config->FindLoadOp("constructor");
      if (Op && Op->ValueArgIndex >= 0 &&
          CE->getNumArgs() > (unsigned)Op->ValueArgIndex &&
          !CE->getArg(Op->ValueArgIndex)->isDefaultArgument()) {
        return GetNodeIdForExpr(CE->getArg(Op->ValueArgIndex));
      }
    }
    if (CE->getConstructor() &&
        CE->getConstructor()->isCopyOrMoveConstructor() &&
        CE->getNumArgs() >= 1 && !CE->getArg(0)->isDefaultArgument()) {
      return GetNodeIdForExpr(CE->getArg(0));
    }
    // Default (0-argument) constructor calls like `DirectHandle<T> x;` do not
    // carry an input dataflow value; return 0 to avoid bogus CXXConstructExpr leaves.
    if (CE->getNumArgs() == 0 || CE->getArg(0)->isDefaultArgument()) {
      return 0;
    }
    if (auto* CD = CE->getConstructor()) {
      Node::Id ID = GetExprASTId(E);
      if (ID == 0) {
        ID = GenerateId("ctor_" + CD->getNameAsString(), Loc);
      }
      Node N;
      N.ID = ID;
      N.Kind = NodeKind::Expression;
      N.Name = CD->getNameAsString() + "()";
      N.Type = GetTypeString(CE->getType()).str();
      N.Loc = Loc;
      EmitNode(std::move(N));
      return ID;
    }
  }

  // 2. Handle CXXOperatorCallExpr for configured wrapper overloaded operators
  // (e.g. operator*, operator->)
  if (auto* OCE = llvm::dyn_cast<clang::CXXOperatorCallExpr>(E)) {
    if (auto* MD = OCE->getCalleeDecl()
                       ? llvm::dyn_cast<clang::CXXMethodDecl>(
                             OCE->getCalleeDecl()->getCanonicalDecl())
                       : nullptr) {
      if (OCE->getNumArgs() > 0) {
        clang::Expr* Receiver = PeelExpr(OCE->getArg(0));
        const WrapperConfig* Config = FindWrapperConfig(Receiver->getType());
        if (Config) {
          std::string opName = MD->getNameAsString();
          const WrapperOp* Op = Config->FindLoadOp(opName);
          if (Op) {
            return GetNodeIdForExpr(Receiver);
          }
        }
      }
    }
  }

  // 2.5 Handle CXXMemberCallExpr (for wrapper .load(), .Acquire_Load(), .get()
  // and implicit conversion casts)
  if (auto* MCE = llvm::dyn_cast<clang::CXXMemberCallExpr>(E)) {
    if (auto* MD = MCE->getMethodDecl()) {
      if (!MD->isStatic()) {
        clang::Expr* Receiver = MCE->getImplicitObjectArgument();
        if (Receiver) {
          Receiver = PeelExpr(Receiver);
          std::string methodName = MD->getNameAsString();
          if (MCE->getNumArgs() <= 1 &&
              (methodName == "load" || methodName == "Acquire_Load" ||
               methodName == "Relaxed_Load" || methodName == "SeqCst_Load" ||
               methodName == "Acquire_Load_No_Unpack" ||
               methodName == "Relaxed_Load_Map_Word" ||
               methodName == "Relaxed_Load_Raw" || methodName == "load_map" ||
               methodName == "ptr_location" || methodName == "location" ||
               methodName == "ptr")) {
            return GetNodeIdForExpr(Receiver);
          }
          const WrapperConfig* Config = FindWrapperConfig(Receiver->getType());
          if (Config) {
            if (llvm::isa<clang::CXXConversionDecl>(MD)) {
              return GetNodeIdForExpr(Receiver);
            }
            const WrapperOp* Op = Config->FindLoadOp(methodName);
            if (Op) {
              return GetNodeIdForExpr(Receiver);
            }
          }
        }
      }
    }
  }

  // 3. Handle V8 offset-based field loads (TaggedField<T, kOffset>::load,
  // ReadField<T>(offset), etc.) and transparent static casts/moves
  if (auto* CE = llvm::dyn_cast<clang::CallExpr>(E)) {
    OffsetFieldAccess OffsetAccess = ExtractOffsetFieldAccess(CE);
    if (OffsetAccess.IsValid && !OffsetAccess.IsStore) {
      Node::Id FieldId = GetOrCreateOffsetFieldNode(OffsetAccess);
      if (FieldId != 0) {
        return FieldId;
      }
    }

    std::string CalleeName;
    bool IsInstanceMember = false;
    bool IsAllocationResultFromObject = false;
    if (auto* FD = CE->getDirectCallee()) {
      CalleeName = FD->getNameAsString();
      if (auto* Method = llvm::dyn_cast<clang::CXXMethodDecl>(FD)) {
        if (!Method->isStatic()) {
          IsInstanceMember = true;
        } else if (CalleeName == "FromObject" && Method->getParent() &&
                   Method->getParent()->getName() == "AllocationResult") {
          IsAllocationResultFromObject = true;
        }
      }
    } else if (CE->getCallee()) {
      if (auto* ULE = llvm::dyn_cast<clang::UnresolvedLookupExpr>(
              PeelExpr(CE->getCallee()))) {
        CalleeName = ULE->getName().getAsString();
      }
    }

    if (!CalleeName.empty()) {
      // Check if it is a transparent identity/cast/atomic-load helper
      // (DescriptorArray::cast, std::move, Acquire_Load, to_storage_type, etc.)
      if (!IsInstanceMember && CE->getNumArgs() >= 1 &&
          (WrapperConfigManager::Get().IsIdentityFunction(CalleeName) ||
           IsAllocationResultFromObject)) {
        unsigned ArgIdx =
            (CalleeName == "DecompressTagged" ||
             CalleeName == "DecompressTaggedSigned" ||
             CalleeName == "DecompressTaggedAny" ||
             CalleeName == "CompressObject" || CalleeName == "CompressAny" ||
             CalleeName == "RawToTagged")
                ? (CE->getNumArgs() - 1)
                : 0;
        if (ArgIdx == 0 && CE->getNumArgs() >= 2) {
          clang::QualType Arg0Type = CE->getArg(0)->getType();
          if (!Arg0Type.isNull()) {
            std::string Arg0TypeStr = Arg0Type.getAsString();
            if (Arg0TypeStr.find("Isolate") != std::string::npos ||
                Arg0TypeStr.find("CageBase") != std::string::npos) {
              ArgIdx = 1;
            }
          }
        }
        return GetNodeIdForExpr(CE->getArg(ArgIdx));
      }
      if (!IsInstanceMember && CE->getNumArgs() >= 2 &&
          (CalleeName == "Release_CompareAndSwap" ||
           CalleeName == "Relaxed_CompareAndSwap" ||
           CalleeName == "AcquireRelease_CompareAndSwap" ||
           CalleeName == "SeqCst_CompareAndSwap" ||
           CalleeName == "SeqCst_Swap" || CalleeName == "Relaxed_Swap" ||
           CalleeName == "Relaxed_AtomicExchange" ||
           CalleeName == "SeqCst_AtomicExchange" ||
           CalleeName == "Relaxed_FetchOr" ||
           CalleeName == "Relaxed_AtomicIncrement")) {
        return GetNodeIdForExpr(CE->getArg(0));
      }
    }
  }

  if (auto* DRE = llvm::dyn_cast<clang::DeclRefExpr>(E)) {
    if (auto* VD = llvm::dyn_cast<clang::ValueDecl>(DRE->getDecl())) {
      llvm::StringRef Name = VD->getName();
      if (Name == "kNullMaybe" || Name == "kNullMaybeHandle") {
        return 0;
      }
      return GetOrCreateNodeForDecl(VD);
    }
  } else if (auto* ME = llvm::dyn_cast<clang::MemberExpr>(E)) {
    if (auto* FD = llvm::dyn_cast<clang::FieldDecl>(ME->getMemberDecl())) {
      return GetOrCreateNodeForDecl(FD);
    }
  } else if (auto* IL = llvm::dyn_cast<clang::IntegerLiteral>(E)) {
    llvm::SmallString<16> Str;
    IL->getValue().toString(Str, 10, true);
    std::string valStr = Str.str().str();
    Node::Id ID = GetExprASTId(E);
    if (ID == 0) {
      ID = GenerateId("literal_int_" + valStr, Loc);
    }
    Node N;
    N.ID = ID;
    N.Kind = NodeKind::Literal;
    N.Name = valStr;
    N.Type = GetTypeString(IL->getType()).str();
    N.Loc = Loc;
    EmitNode(std::move(N));
    return ID;
  } else if (auto* BL = llvm::dyn_cast<clang::CXXBoolLiteralExpr>(E)) {
    std::string valStr = BL->getValue() ? "1" : "0";
    Node::Id ID = GetExprASTId(E);
    if (ID == 0) {
      ID = GenerateId("literal_bool_" + valStr, Loc);
    }
    Node N;
    N.ID = ID;
    N.Kind = NodeKind::Literal;
    N.Name = BL->getValue() ? "true" : "false";
    N.Type = GetTypeString(BL->getType()).str() + " = " + valStr;
    N.Loc = Loc;
    EmitNode(std::move(N));
    return ID;
  } else if (auto* CL = llvm::dyn_cast<clang::CharacterLiteral>(E)) {
    std::string valStr = std::to_string(CL->getValue());
    Node::Id ID = GetExprASTId(E);
    if (ID == 0) {
      ID = GenerateId("literal_char_" + valStr, Loc);
    }
    Node N;
    N.ID = ID;
    N.Kind = NodeKind::Literal;
    N.Name = valStr;
    N.Type = GetTypeString(CL->getType()).str() + " = " + valStr;
    N.Loc = Loc;
    EmitNode(std::move(N));
    return ID;
  } else if (auto* UETT = llvm::dyn_cast<clang::UnaryExprOrTypeTraitExpr>(E)) {
    if (!UETT->isValueDependent()) {
      clang::Expr::EvalResult Eval;
      if (UETT->EvaluateAsInt(Eval, *Context)) {
        llvm::SmallString<32> ValBuf;
        Eval.Val.getInt().toString(ValBuf, 10);
        std::string valStr = ValBuf.str().str();
        Node::Id ID = GetExprASTId(E);
        if (ID == 0) {
          ID = GenerateId("uett_" + valStr, Loc);
        }
        Node N;
        N.ID = ID;
        N.Kind = NodeKind::Literal;
        N.Name = (UETT->getKind() == clang::UETT_SizeOf) ? "sizeof" : "alignof";
        N.Type = GetTypeString(UETT->getType()).str() + " = " + valStr;
        N.Loc = Loc;
        EmitNode(std::move(N));
        return ID;
      }
    }
  } else if (auto* OOE = llvm::dyn_cast<clang::OffsetOfExpr>(E)) {
    if (!OOE->isValueDependent()) {
      clang::Expr::EvalResult Eval;
      if (OOE->EvaluateAsInt(Eval, *Context)) {
        llvm::SmallString<32> ValBuf;
        Eval.Val.getInt().toString(ValBuf, 10);
        std::string valStr = ValBuf.str().str();
        Node::Id ID = GetExprASTId(E);
        if (ID == 0) {
          ID = GenerateId("offsetof_" + valStr, Loc);
        }
        Node N;
        N.ID = ID;
        N.Kind = NodeKind::Literal;
        N.Name = "offsetof";
        N.Type = GetTypeString(OOE->getType()).str() + " = " + valStr;
        N.Loc = Loc;
        EmitNode(std::move(N));
        return ID;
      }
    }
  } else if (auto* SL = llvm::dyn_cast<clang::StringLiteral>(E)) {
    std::string valStr = "<wide_string_literal>";
    if (SL->getCharByteWidth() == 1) {
      valStr = SL->getString().str();
    }
    Node::Id ID = GetExprASTId(E);
    if (ID == 0) {
      ID = GenerateId("literal_str_" + valStr, Loc);
    }
    Node N;
    N.ID = ID;
    N.Kind = NodeKind::Literal;
    N.Name = valStr;
    N.Type = GetTypeString(SL->getType()).str();
    N.Loc = Loc;
    EmitNode(std::move(N));
    return ID;
  } else if (auto* CE = llvm::dyn_cast<clang::CallExpr>(E)) {
    BitFieldInfo BfInfo = ExtractBitFieldInfo(CE);
    auto* FD = CE->getDirectCallee();
    if (!BfInfo.IsValid && !FD) {
      return 0;
    }
    SourceLocation CallNameLoc = Loc;
    bool IsOverloadedIncDec = false;
    if (auto* OCE = llvm::dyn_cast<clang::CXXOperatorCallExpr>(CE)) {
      if ((OCE->getOperator() == clang::OO_PlusPlus ||
           OCE->getOperator() == clang::OO_MinusMinus) &&
          OCE->getNumArgs() >= 1) {
        IsOverloadedIncDec = true;
        SourceLocation OpLoc = GetSourceLocation(OCE->getOperatorLoc());
        if (!OpLoc.File.empty()) CallNameLoc = OpLoc;
      }
    }
    if (!IsOverloadedIncDec) {
      if (auto* MCE = llvm::dyn_cast<clang::CXXMemberCallExpr>(CE)) {
        SourceLocation MemberLoc = GetSourceLocation(MCE->getExprLoc());
        if (!MemberLoc.File.empty()) CallNameLoc = MemberLoc;
      } else if (CE->getCallee()) {
        SourceLocation CalleeLoc =
            GetSourceLocation(CE->getCallee()->getExprLoc());
        if (!CalleeLoc.File.empty()) CallNameLoc = CalleeLoc;
      }
    }
    Node::Id ID = GetExprASTId(E);
    if (ID == 0) {
      std::string prefix = "call_expr";
      if (IsOverloadedIncDec) {
        auto* OCE = llvm::cast<clang::CXXOperatorCallExpr>(CE);
        prefix = (OCE->getOperator() == clang::OO_PlusPlus) ? "unary_++"
                                                            : "unary_--";
      } else if (FD) {
        prefix = "call_" + FD->getNameAsString();
      }
      ID = GenerateId(prefix, CallNameLoc);
    }
    Node N;
    N.ID = ID;
    N.Kind = NodeKind::Expression;
    if (BfInfo.IsValid) {
      std::string Prefix =
          BfInfo.TypedefDecl ? (BfInfo.TypedefDecl->getNameAsString() + "::")
                             : "BitField::";
      N.Name = Prefix + BfInfo.OpName + "()";
    } else if (IsOverloadedIncDec) {
      auto* OCE = llvm::cast<clang::CXXOperatorCallExpr>(CE);
      N.Name = (OCE->getOperator() == clang::OO_PlusPlus) ? "++" : "--";
    } else {
      N.Name = FD->getNameAsString() + "()";
    }
    if (BfInfo.IsValid && BfInfo.Size > 0) {
      N.Type = GetTypeString(BfInfo.FieldType).str() + " : " +
               std::to_string(BfInfo.Size);
    } else {
      N.Type = GetTypeString(CE->getType()).str();
      if (!BfInfo.IsValid && !CE->isValueDependent() &&
          !CE->getType().isNull() &&
          CE->getType()->isIntegralOrEnumerationType()) {
        clang::Expr::EvalResult Eval;
        if (CE->EvaluateAsInt(Eval, *Context)) {
          llvm::SmallString<32> ValBuf;
          Eval.Val.getInt().toString(ValBuf, 10);
          N.Type += " = " + ValBuf.str().str();
        }
      }
      if (!BfInfo.IsValid && FD) {
        N.Type += HeapTaintTypeSuffix(
            ClassifyDirectHeapReadCall(CE, FD, CurrentFunction));
      }
      if (IsOverloadedIncDec) {
        auto bound_it = ForLoopIncConstantBound.find(CE);
        if (bound_it != ForLoopIncConstantBound.end()) {
          N.Type += " [loop_bound:" + std::to_string(bound_it->second) + "]";
        }
      }
    }
    N.Loc = CallNameLoc;
    EmitNode(std::move(N));
    if (IsOverloadedIncDec) {
      ExprNodeCache[E] = ID;
      auto* OCE = llvm::cast<clang::CXXOperatorCallExpr>(CE);
      Node::Id SubId = GetNodeIdForExpr(OCE->getArg(0));
      if (SubId != 0 && SubId != ID) {
        Edge SubEdge;
        SubEdge.From = SubId;
        SubEdge.To = ID;
        SubEdge.Kind = EdgeKind::ArgPass;
        SourceLocation SubLoc =
            GetSourceLocation(OCE->getArg(0)->getBeginLoc());
        SubEdge.Loc = !SubLoc.File.empty() ? SubLoc : CallNameLoc;
        EmitEdge(std::move(SubEdge));
      }
      auto bound_expr_it = ForLoopIncBoundExpr.find(CE);
      if (bound_expr_it != ForLoopIncBoundExpr.end() &&
          bound_expr_it->second) {
        clang::Expr* BoundExpr = bound_expr_it->second;
        Node::Id BoundId = GetNodeIdForExpr(BoundExpr);
        if (BoundId != 0 && BoundId != ID && BoundId != SubId) {
          Edge BoundEdge;
          BoundEdge.From = BoundId;
          BoundEdge.To = ID;
          BoundEdge.Kind = EdgeKind::ArgPass;
          SourceLocation BoundLoc =
              GetSourceLocation(BoundExpr->getBeginLoc());
          BoundEdge.Loc = !BoundLoc.File.empty() ? BoundLoc : CallNameLoc;
          EmitEdge(std::move(BoundEdge));
        }
      }
    } else if (BfInfo.IsValid) {
      ExprNodeCache[E] = ID;
      EnsureBitFieldCallProcessed(CE);
    } else if (FD && FD->isInStdNamespace() && FD->getIdentifier() &&
               (FD->getName() == "min" || FD->getName() == "max") &&
               CE->getNumArgs() == 2) {
      ExprNodeCache[E] = ID;
      for (unsigned i = 0; i < 2; ++i) {
        clang::Expr* Arg = CE->getArg(i);
        Node::Id ArgId = GetNodeIdForExpr(Arg);
        if (ArgId != 0 && ArgId != ID) {
          Edge ArgEdge;
          ArgEdge.From = ArgId;
          ArgEdge.To = ID;
          ArgEdge.Kind = EdgeKind::ArgPass;
          SourceLocation ArgLoc = GetSourceLocation(Arg->getBeginLoc());
          ArgEdge.Loc = !ArgLoc.File.empty() ? ArgLoc : CallNameLoc;
          EmitEdge(std::move(ArgEdge));
        }
      }
    }
    return ID;
  } else if (auto* UO = llvm::dyn_cast<clang::UnaryOperator>(E)) {
    if (UO->getOpcode() == clang::UO_Plus) {
      return GetNodeIdForExpr(UO->getSubExpr());
    }
    if ((UO->getOpcode() == clang::UO_Minus ||
         UO->getOpcode() == clang::UO_Not ||
         UO->getOpcode() == clang::UO_LNot ||
         UO->isIncrementDecrementOp()) &&
        !UO->getType().isNull() &&
        (UO->getType()->isIntegralOrEnumerationType() ||
         UO->isIncrementDecrementOp())) {
      if (!UO->isIncrementDecrementOp() && !UO->isValueDependent() &&
          llvm::isa<clang::IntegerLiteral>(
              UO->getSubExpr()->IgnoreParenImpCasts())) {
        clang::Expr::EvalResult Eval;
        if (UO->EvaluateAsInt(Eval, *Context)) {
          llvm::SmallString<32> ValBuf;
          Eval.Val.getInt().toString(ValBuf, 10);
          std::string valStr = ValBuf.str().str();
          Node::Id ID = GetExprASTId(E);
          if (ID == 0) {
            ID = GenerateId("literal_int_" + valStr, Loc);
          }
          Node N;
          N.ID = ID;
          N.Kind = NodeKind::Literal;
          N.Name = valStr;
          N.Type = GetTypeString(UO->getType()).str() + " = " + valStr;
          N.Loc = Loc;
          EmitNode(std::move(N));
          return ID;
        }
      }
      std::string opStr = UO->getOpcodeStr(UO->getOpcode()).str();
      SourceLocation OpLoc = GetSourceLocation(UO->getOperatorLoc());
      if (OpLoc.File.empty()) OpLoc = Loc;
      Node::Id ID = GetExprASTId(E);
      if (ID == 0) {
        ID = GenerateId("unary_" + opStr, OpLoc);
      }
      Node N;
      N.ID = ID;
      N.Kind = NodeKind::Expression;
      N.Name = opStr;
      N.Type = GetTypeString(UO->getType()).str();
      if (!UO->isValueDependent()) {
        clang::Expr::EvalResult Eval;
        if (UO->EvaluateAsInt(Eval, *Context)) {
          llvm::SmallString<32> ValBuf;
          Eval.Val.getInt().toString(ValBuf, 10);
          N.Type += " = " + ValBuf.str().str();
        }
      }
      if (UO->isIncrementDecrementOp()) {
        auto bound_it = ForLoopIncConstantBound.find(UO);
        if (bound_it != ForLoopIncConstantBound.end()) {
          N.Type += " [loop_bound:" + std::to_string(bound_it->second) + "]";
        }
      }
      N.Loc = OpLoc;
      EmitNode(std::move(N));
      ExprNodeCache[E] = ID;
      Node::Id SubId = GetNodeIdForExpr(UO->getSubExpr());
      if (SubId != 0 && SubId != ID) {
        Edge SubEdge;
        SubEdge.From = SubId;
        SubEdge.To = ID;
        SubEdge.Kind = EdgeKind::ArgPass;
        SourceLocation SubLoc =
            GetSourceLocation(UO->getSubExpr()->getBeginLoc());
        SubEdge.Loc = !SubLoc.File.empty() ? SubLoc : OpLoc;
        EmitEdge(std::move(SubEdge));
      }
      if (UO->isIncrementDecrementOp()) {
        auto bound_expr_it = ForLoopIncBoundExpr.find(UO);
        if (bound_expr_it != ForLoopIncBoundExpr.end() &&
            bound_expr_it->second) {
          clang::Expr* BoundExpr = bound_expr_it->second;
          Node::Id BoundId = GetNodeIdForExpr(BoundExpr);
          if (BoundId != 0 && BoundId != ID && BoundId != SubId) {
            Edge BoundEdge;
            BoundEdge.From = BoundId;
            BoundEdge.To = ID;
            BoundEdge.Kind = EdgeKind::ArgPass;
            SourceLocation BoundLoc =
                GetSourceLocation(BoundExpr->getBeginLoc());
            BoundEdge.Loc = !BoundLoc.File.empty() ? BoundLoc : OpLoc;
            EmitEdge(std::move(BoundEdge));
          }
        }
      }
      return ID;
    }
  } else if (auto* BO = llvm::dyn_cast<clang::BinaryOperator>(E)) {
    if (!BO->isAssignmentOp() &&
        (BO->isAdditiveOp() || BO->isMultiplicativeOp() || BO->isShiftOp() ||
         BO->isBitwiseOp()) &&
        !BO->getType().isNull() &&
        BO->getType()->isIntegralOrEnumerationType()) {
      std::string opStr = BO->getOpcodeStr().str();
      SourceLocation OpLoc = GetSourceLocation(BO->getOperatorLoc());
      if (OpLoc.File.empty()) OpLoc = Loc;
      Node::Id ID = GetExprASTId(E);
      if (ID == 0) {
        ID = GenerateId("binop_" + opStr, OpLoc);
      }
      Node N;
      N.ID = ID;
      N.Kind = NodeKind::Expression;
      N.Name = opStr;
      N.Type = GetTypeString(BO->getType()).str();
      if (!BO->isValueDependent()) {
        clang::Expr::EvalResult Eval;
        if (BO->EvaluateAsInt(Eval, *Context)) {
          llvm::SmallString<32> ValBuf;
          Eval.Val.getInt().toString(ValBuf, 10);
          N.Type += " = " + ValBuf.str().str();
        }
      }
      N.Loc = OpLoc;
      EmitNode(std::move(N));
      ExprNodeCache[E] = ID;

      Node::Id LHSId = GetNodeIdForExpr(BO->getLHS());
      if (LHSId != 0 && LHSId != ID) {
        Edge LEdge;
        LEdge.From = LHSId;
        LEdge.To = ID;
        LEdge.Kind = EdgeKind::ArgPass;
        SourceLocation LLoc = GetSourceLocation(BO->getLHS()->getBeginLoc());
        LEdge.Loc = !LLoc.File.empty() ? LLoc : OpLoc;
        EmitEdge(std::move(LEdge));
      }
      Node::Id RHSId = GetNodeIdForExpr(BO->getRHS());
      if (RHSId != 0 && RHSId != ID) {
        Edge REdge;
        REdge.From = RHSId;
        REdge.To = ID;
        REdge.Kind = EdgeKind::ArgPass;
        SourceLocation RLoc = GetSourceLocation(BO->getRHS()->getBeginLoc());
        REdge.Loc = !RLoc.File.empty() ? RLoc : OpLoc;
        EmitEdge(std::move(REdge));
      }
      return ID;
    }
  } else if (auto* ILE = llvm::dyn_cast<clang::InitListExpr>(E)) {
    if (ILE->getNumInits() == 0) {
      return 0;
    }
    if (ILE->getNumInits() == 1) {
      return GetNodeIdForExpr(ILE->getInit(0));
    }
  } else if (llvm::isa<clang::CXXNullPtrLiteralExpr>(E) ||
             llvm::isa<clang::GNUNullExpr>(E) ||
             llvm::isa<clang::CXXScalarValueInitExpr>(E) ||
             llvm::isa<clang::ImplicitValueInitExpr>(E)) {
    return 0;
  } else if (auto* TE = llvm::dyn_cast<clang::CXXThisExpr>(E)) {
    SourceLocation StableLoc = Loc;
    Node::Id ID = 0;
    if (CurrentFunction) {
      SourceLocation CanonLoc =
          GetSourceLocation(CurrentFunction->getCanonicalDecl()->getLocation());
      if (!CanonLoc.File.empty()) {
        StableLoc = CanonLoc;
      }
      std::string FuncUSR = GetUSRForDecl(CurrentFunction).str();
      if (!FuncUSR.empty()) {
        ID = HashFnv1a64(FuncUSR + "/this");
      }
    }
    if (ID == 0) {
      ID = GenerateId("this", StableLoc);
    }
    Node N;
    N.ID = ID;
    N.Kind = NodeKind::Parameter;
    N.Name = "this";
    N.Type = GetTypeString(TE->getType()).str();
    N.Loc = StableLoc;
    EmitNode(std::move(N));
    return ID;
  } else if (auto* ACO =
                 llvm::dyn_cast<clang::AbstractConditionalOperator>(E)) {
    Node::Id ID = GetExprASTId(E);
    if (ID == 0) {
      ID = GenerateId("conditional_expr", Loc);
    }
    Node N;
    N.ID = ID;
    N.Kind = NodeKind::Expression;
    N.Name = "?:";
    N.Type = GetTypeString(ACO->getType()).str();
    N.Loc = Loc;
    EmitNode(std::move(N));
    return ID;
  }

  // Fallback: create a generic expression node
  Node::Id ID = GetExprASTId(E);
  if (ID == 0) {
    std::string prefix = std::string(E->getStmtClassName()) + "_expr";
    ID = GenerateId(prefix, Loc);
  }
  Node N;
  N.ID = ID;
  N.Kind = NodeKind::Expression;
  N.Name = "expr";
  N.Type = GetTypeString(E->getType()).str();
  N.Loc = Loc;
  EmitNode(std::move(N));
  return ID;
}

bool DfgVisitor::VisitFieldDecl(clang::FieldDecl* FD) {
  if (!FD) return true;
  SourceLocation DeclLoc = GetSourceLocation(FD->getLocation());
  if (DeclLoc.File.empty()) return true;

  if (FD->hasInClassInitializer()) {
    if (clang::Expr* Init = FD->getInClassInitializer()) {
      Node::Id LHS = GetOrCreateNodeForDecl(FD);
      Node::Id RHS = GetNodeIdForExpr(Init);
      if (LHS != 0 && RHS != 0 && LHS != RHS) {
        Edge E;
        E.From = RHS;
        E.To = LHS;
        E.Kind = EdgeKind::Initialization;
        E.Loc = DeclLoc;
        EmitEdge(std::move(E));
      }
    }
  } else {
    GetOrCreateNodeForDecl(FD);
  }
  return true;
}

bool DfgVisitor::VisitVarDecl(clang::VarDecl* VD) {
  if (llvm::isa<clang::ParmVarDecl>(VD)) {
    return true;
  }
  if (VD->isImplicit() && !llvm::isa<clang::DecompositionDecl>(VD)) {
    return true;
  }

  SourceLocation DeclLoc = GetSourceLocation(VD->getLocation());
  if (DeclLoc.File.empty()) return true;

  if (VD->hasInit()) {
    if (auto* DD = llvm::dyn_cast<clang::DecompositionDecl>(VD)) {
      Node::Id RHS = GetNodeIdForExpr(DD->getInit());
      for (clang::BindingDecl* BD : DD->bindings()) {
        Node::Id BDId = GetOrCreateNodeForDecl(BD);
        if (BDId != 0 && RHS != 0 && BDId != RHS) {
          Edge E;
          E.From = RHS;
          E.To = BDId;
          E.Kind = EdgeKind::Initialization;
          E.Loc = GetSourceLocation(BD->getLocation());
          EmitEdge(std::move(E));
        }
      }
      return true;
    }
    Node::Id LHS = GetOrCreateNodeForDecl(VD);
    if (!ContextStack.empty()) {
      auto& Ctx = ContextStack.back();
      if (const clang::FieldDecl* F =
              ExtractBackingFieldFromExpr(VD->getInit())) {
        Ctx.LocalVarToBackingField[VD] = F;
      }
      std::vector<size_t> Writes = CollectBitFieldWrites(VD->getInit());
      if (!Writes.empty()) {
        auto& DstVec = Ctx.VarBitFieldWrites[VD];
        DstVec.insert(DstVec.end(), Writes.begin(), Writes.end());
        if (const clang::FieldDecl* TargetF =
                Ctx.LocalVarToBackingField.lookup(VD)) {
          for (size_t idx : Writes) {
            FlushBitFieldWriteToField(Ctx.PendingWrites[idx], TargetF);
          }
        }
      }
    }
    Node::Id RHS = GetNodeIdForExpr(VD->getInit());

    if (LHS != 0 && RHS != 0 && LHS != RHS) {
      Edge E;
      E.From = RHS;
      E.To = LHS;
      E.Kind = EdgeKind::Initialization;
      E.Loc = GetSourceLocation(VD->getLocation());
      EmitEdge(std::move(E));
    }
  } else {
    GetOrCreateNodeForDecl(VD);
  }
  return true;
}

bool DfgVisitor::VisitForStmt(clang::ForStmt* FS) {
  if (!FS || !FS->getInc() || !FS->getCond()) return true;
  clang::Expr* IncE = PeelExpr(FS->getInc());
  if (!IncE) return true;
  bool IsInc = false;
  bool IsDec = false;
  clang::Expr* VarE = nullptr;
  if (auto* UO = llvm::dyn_cast<clang::UnaryOperator>(IncE)) {
    if (UO->isIncrementOp()) {
      IsInc = true;
      VarE = UO->getSubExpr();
    } else if (UO->isDecrementOp()) {
      IsDec = true;
      VarE = UO->getSubExpr();
    }
  } else if (auto* OCE = llvm::dyn_cast<clang::CXXOperatorCallExpr>(IncE)) {
    if (OCE->getOperator() == clang::OO_PlusPlus && OCE->getNumArgs() >= 1) {
      IsInc = true;
      VarE = OCE->getArg(0);
    } else if (OCE->getOperator() == clang::OO_MinusMinus &&
               OCE->getNumArgs() >= 1) {
      IsDec = true;
      VarE = OCE->getArg(0);
    }
  }
  if ((!IsInc && !IsDec) || !VarE) return true;
  auto* VarDRE = llvm::dyn_cast_or_null<clang::DeclRefExpr>(PeelExpr(VarE));
  if (!VarDRE || !VarDRE->getDecl()) return true;
  const clang::Decl* CanonLoopVar = VarDRE->getDecl()->getCanonicalDecl();

  auto FindLoopBoundInCond =
      [&](auto& self, clang::Expr* CondExpr, clang::BinaryOperatorKind& OutOp)
      -> clang::Expr* {
    if (!CondExpr) return nullptr;
    auto* BO =
        llvm::dyn_cast_or_null<clang::BinaryOperator>(PeelExpr(CondExpr));
    if (!BO) return nullptr;
    if (BO->getOpcode() == clang::BO_LAnd) {
      if (clang::Expr* L = self(self, BO->getLHS(), OutOp)) return L;
      return self(self, BO->getRHS(), OutOp);
    }
    if (!BO->isComparisonOp()) return nullptr;
    auto* LhsDRE =
        llvm::dyn_cast_or_null<clang::DeclRefExpr>(PeelExpr(BO->getLHS()));
    if (LhsDRE && LhsDRE->getDecl() &&
        LhsDRE->getDecl()->getCanonicalDecl() == CanonLoopVar) {
      OutOp = BO->getOpcode();
      return BO->getRHS();
    }
    auto* RhsDRE =
        llvm::dyn_cast_or_null<clang::DeclRefExpr>(PeelExpr(BO->getRHS()));
    if (RhsDRE && RhsDRE->getDecl() &&
        RhsDRE->getDecl()->getCanonicalDecl() == CanonLoopVar) {
      clang::BinaryOperatorKind RawOp = BO->getOpcode();
      if (RawOp == clang::BO_LT)
        OutOp = clang::BO_GT;
      else if (RawOp == clang::BO_LE)
        OutOp = clang::BO_GE;
      else if (RawOp == clang::BO_GT)
        OutOp = clang::BO_LT;
      else if (RawOp == clang::BO_GE)
        OutOp = clang::BO_LE;
      else
        OutOp = RawOp;
      return BO->getLHS();
    }
    return nullptr;
  };

  clang::BinaryOperatorKind Op = clang::BO_Assign;
  clang::Expr* BoundE =
      FindLoopBoundInCond(FindLoopBoundInCond, FS->getCond(), Op);
  if (!BoundE) return true;

  bool ValidDir =
      (IsInc &&
       (Op == clang::BO_LT || Op == clang::BO_LE || Op == clang::BO_NE)) ||
      (IsDec &&
       (Op == clang::BO_GT || Op == clang::BO_GE || Op == clang::BO_NE));
  if (!ValidDir) return true;

  ForLoopIncBoundExpr[IncE] = BoundE;

  if (!BoundE->isValueDependent() && !BoundE->getType().isNull() &&
      BoundE->getType()->isIntegralOrEnumerationType()) {
    clang::Expr::EvalResult Eval;
    if (BoundE->EvaluateAsInt(Eval, *Context)) {
      int64_t K = Eval.Val.getInt().getExtValue();
      if (IsInc) {
        if (Op == clang::BO_LE) {
          ForLoopIncConstantBound[IncE] = K;
        } else if (Op == clang::BO_LT || Op == clang::BO_NE) {
          ForLoopIncConstantBound[IncE] =
              (K > std::numeric_limits<int64_t>::min()) ? (K - 1) : K;
        }
      } else if (IsDec) {
        if (Op == clang::BO_GE) {
          ForLoopIncConstantBound[IncE] = K;
        } else if (Op == clang::BO_GT || Op == clang::BO_NE) {
          ForLoopIncConstantBound[IncE] =
              (K < std::numeric_limits<int64_t>::max()) ? (K + 1) : K;
        }
      }
    }
  }
  return true;
}

bool DfgVisitor::VisitUnaryOperator(clang::UnaryOperator* UO) {
  if (UO && UO->isIncrementDecrementOp()) {
    SourceLocation OpLoc = GetSourceLocation(UO->getOperatorLoc());
    if (OpLoc.File.empty()) return true;
    Node::Id LHS = GetNodeIdForExpr(UO->getSubExpr());
    Node::Id RHS = GetNodeIdForExpr(UO);
    if (LHS != 0 && RHS != 0 && LHS != RHS) {
      Edge E;
      E.From = RHS;
      E.To = LHS;
      E.Kind = EdgeKind::Assignment;
      E.Loc = OpLoc;
      EmitEdge(std::move(E));
    }
  }
  return true;
}

bool DfgVisitor::VisitBinaryOperator(clang::BinaryOperator* BO) {
  if (BO->isAssignmentOp()) {
    SourceLocation OpLoc = GetSourceLocation(BO->getOperatorLoc());
    if (OpLoc.File.empty()) return true;

    if (!ContextStack.empty()) {
      auto& Ctx = ContextStack.back();
      std::vector<size_t> Writes = CollectBitFieldWrites(BO->getRHS());
      if (const clang::FieldDecl* LHSField =
              ExtractBackingFieldFromExpr(BO->getLHS())) {
        if (!Writes.empty()) {
          for (size_t idx : Writes) {
            FlushBitFieldWriteToField(Ctx.PendingWrites[idx], LHSField);
          }
        }
      }
      if (auto* DRE =
              llvm::dyn_cast<clang::DeclRefExpr>(PeelExpr(BO->getLHS()))) {
        if (auto* LHS_VD = llvm::dyn_cast<clang::VarDecl>(DRE->getDecl())) {
          if (!Ctx.LocalVarToBackingField.lookup(LHS_VD)) {
            if (const clang::FieldDecl* F =
                    ExtractBackingFieldFromExpr(BO->getRHS())) {
              Ctx.LocalVarToBackingField[LHS_VD] = F;
            }
          }
          if (!Writes.empty()) {
            auto& DstVec = Ctx.VarBitFieldWrites[LHS_VD];
            DstVec.insert(DstVec.end(), Writes.begin(), Writes.end());
            if (const clang::FieldDecl* TargetF =
                    Ctx.LocalVarToBackingField.lookup(LHS_VD)) {
              for (size_t idx : Writes) {
                FlushBitFieldWriteToField(Ctx.PendingWrites[idx], TargetF);
              }
            }
          }
        }
      }
    }

    Node::Id LHS = GetNodeIdForExpr(BO->getLHS());
    Node::Id RHS = GetNodeIdForExpr(BO->getRHS());

    if (!ContextStack.empty() && RHS != 0 && BO->getRHS()) {
      if (const clang::FieldDecl* LHSField =
              ExtractBackingFieldFromExpr(BO->getLHS())) {
        if (CollectBitFieldWrites(BO->getRHS()).empty()) {
          FlushRawWordWriteToRecordBitFields(
              BO->getRHS(), RHS, LHSField, EdgeKind::Assignment,
              GetSourceLocation(BO->getOperatorLoc()));
        }
      }
    }

    if (LHS != 0 && RHS != 0 && LHS != RHS) {
      Edge E;
      E.From = RHS;
      E.To = LHS;
      E.Kind = EdgeKind::Assignment;
      E.Loc = GetSourceLocation(BO->getOperatorLoc());
      EmitEdge(std::move(E));
    }
  }
  return true;
}

struct ForwardedConstructorResolution {
  const clang::CXXConstructorDecl* Constructor = nullptr;
  llvm::SmallVector<std::pair<unsigned, unsigned>, 8> ArgToParamMap;
};

static clang::InitListExpr* FindInitListExpr(clang::Expr* E) {
  while (E) {
    if (auto* ILE = llvm::dyn_cast<clang::InitListExpr>(E)) return ILE;
    if (auto* SILE = llvm::dyn_cast<clang::CXXStdInitializerListExpr>(E)) {
      E = SILE->getSubExpr();
    } else if (auto* CE = llvm::dyn_cast<clang::CastExpr>(E)) {
      E = CE->getSubExpr();
    } else if (auto* PE = llvm::dyn_cast<clang::ParenExpr>(E)) {
      E = PE->getSubExpr();
    } else if (auto* BTE = llvm::dyn_cast<clang::CXXBindTemporaryExpr>(E)) {
      E = BTE->getSubExpr();
    } else if (auto* EWC = llvm::dyn_cast<clang::ExprWithCleanups>(E)) {
      E = EWC->getSubExpr();
    } else if (auto* MTE = llvm::dyn_cast<clang::MaterializeTemporaryExpr>(E)) {
      E = MTE->getSubExpr();
    } else if (auto* CCE = llvm::dyn_cast<clang::CXXConstructExpr>(E)) {
      if (CCE->getNumArgs() == 1) {
        E = CCE->getArg(0);
      } else {
        break;
      }
    } else {
      break;
    }
  }
  return nullptr;
}

static int GetForwardedParamIndex(clang::Expr* E,
                                  const clang::FunctionDecl* OwnerFD) {
  while (E) {
    E = PeelExpr(E);
    if (!E) break;
    if (auto* CCE = llvm::dyn_cast<clang::CXXConstructExpr>(E)) {
      if (CCE->getNumArgs() >= 1 &&
          (CCE->getNumArgs() == 1 || CCE->getArg(1)->isDefaultArgument())) {
        E = CCE->getArg(0);
        continue;
      }
    }
    if (auto* InnerCE = llvm::dyn_cast<clang::CallExpr>(E)) {
      if (const auto* InnerFD = InnerCE->getDirectCallee()) {
        llvm::StringRef InnerName = InnerFD->getName();
        if ((InnerName == "forward" || InnerName == "move" ||
             WrapperConfigManager::Get().IsIdentityFunction(InnerName.str())) &&
            InnerCE->getNumArgs() == 1) {
          E = InnerCE->getArg(0);
          continue;
        }
      }
    }
    break;
  }
  if (auto* DRE = llvm::dyn_cast_or_null<clang::DeclRefExpr>(E)) {
    if (auto* PVD = llvm::dyn_cast<clang::ParmVarDecl>(DRE->getDecl())) {
      for (unsigned p = 0; p < OwnerFD->getNumParams(); ++p) {
        if (OwnerFD->getParamDecl(p) == PVD ||
            OwnerFD->getParamDecl(p)->getCanonicalDecl() ==
                PVD->getCanonicalDecl()) {
          return static_cast<int>(p);
        }
      }
    }
  }
  return -1;
}

static bool ReturnsTargetOrWrapper(clang::QualType RetType,
                                   const clang::CXXRecordDecl* TargetRD) {
  if (RetType.isNull() || !TargetRD) return false;
  clang::QualType Unref = RetType.getNonReferenceType();
  if (Unref->isPointerType()) {
    Unref = Unref->getPointeeType();
  }
  if (const auto* RetRD = Unref->getAsCXXRecordDecl()) {
    if (RetRD->getCanonicalDecl() == TargetRD->getCanonicalDecl()) {
      return true;
    }
    if (TargetRD->hasDefinition() && RetRD->hasDefinition() &&
        TargetRD->isDerivedFrom(RetRD)) {
      return true;
    }
    if (const auto* CTSD =
            llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(RetRD)) {
      const auto& Args = CTSD->getTemplateArgs();
      for (unsigned i = 0; i < Args.size(); ++i) {
        if (Args[i].getKind() == clang::TemplateArgument::Type) {
          if (const auto* ArgRD = Args[i].getAsType()->getAsCXXRecordDecl()) {
            if (ArgRD->getCanonicalDecl() == TargetRD->getCanonicalDecl()) {
              return true;
            }
          }
        }
      }
    }
  }
  return false;
}

static ForwardedConstructorResolution FindConstructInForwardingBody(
    const clang::FunctionDecl* CurFD, const clang::CXXRecordDecl* TargetRD,
    llvm::ArrayRef<int> ParamToCallerArg, int Depth) {
  ForwardedConstructorResolution Res;
  if (!CurFD || !TargetRD || Depth > 3) return Res;

  const clang::FunctionDecl* DefFD = CurFD;
  if (!DefFD->getBody()) {
    CurFD->hasBody(DefFD);
  }
  if (!DefFD || !DefFD->getBody()) return Res;

  struct BodyScanner : public clang::RecursiveASTVisitor<BodyScanner> {
    const clang::CXXRecordDecl* TargetCanon;
    llvm::SmallVector<clang::CXXConstructExpr*, 4> ConstructExprs;
    llvm::SmallVector<clang::CallExpr*, 8> CallExprs;

    explicit BodyScanner(const clang::CXXRecordDecl* Target)
        : TargetCanon(Target->getCanonicalDecl()) {}

    bool VisitCXXConstructExpr(clang::CXXConstructExpr* CCE) {
      if (const auto* CD = CCE->getConstructor()) {
        if (const auto* Parent = CD->getParent()) {
          if (Parent->getCanonicalDecl() == TargetCanon) {
            ConstructExprs.push_back(CCE);
          }
        }
      }
      return true;
    }

    bool VisitCallExpr(clang::CallExpr* CE) {
      if (CE->getDirectCallee()) {
        CallExprs.push_back(CE);
      }
      return true;
    }
  };

  BodyScanner Scanner(TargetRD);
  Scanner.TraverseStmt(DefFD->getBody());

  // 1. Prefer a direct CXXConstructExpr of TargetRD in DefFD's body.
  for (clang::CXXConstructExpr* CCE : Scanner.ConstructExprs) {
    const clang::CXXConstructorDecl* CD = CCE->getConstructor();
    if (!CD || CD->isCopyOrMoveConstructor()) continue;
    Res.Constructor = CD;
    for (unsigned k = 0; k < CCE->getNumArgs() && k < CD->getNumParams(); ++k) {
      int p = GetForwardedParamIndex(CCE->getArg(k), DefFD);
      if (p >= 0 && static_cast<unsigned>(p) < ParamToCallerArg.size() &&
          ParamToCallerArg[p] >= 0) {
        Res.ArgToParamMap.push_back(
            {static_cast<unsigned>(ParamToCallerArg[p]), k});
      }
    }
    return Res;
  }

  // 2. Otherwise, follow inner calls that also specialize on TargetRD (e.g.
  // AddNewNode<T> -> NodeBase::New<T> -> NodeBase::Allocate<T>).
  for (clang::CallExpr* InnerCE : Scanner.CallExprs) {
    const clang::FunctionDecl* InnerFD = InnerCE->getDirectCallee();
    if (!InnerFD) continue;
    bool SpecializesTarget = false;
    if (const auto* InnerTSA = InnerFD->getTemplateSpecializationArgs()) {
      for (unsigned i = 0; i < InnerTSA->size(); ++i) {
        if (InnerTSA->get(i).getKind() == clang::TemplateArgument::Type) {
          if (const auto* InnerRD =
                  InnerTSA->get(i).getAsType()->getAsCXXRecordDecl()) {
            if (InnerRD->getCanonicalDecl() == TargetRD->getCanonicalDecl()) {
              SpecializesTarget = true;
              break;
            }
          }
        }
      }
    }
    if (!SpecializesTarget) continue;

    unsigned InnerArgOffset = 0;
    if (const auto* InnerMD = llvm::dyn_cast<clang::CXXMethodDecl>(InnerFD)) {
      if (!InnerMD->isStatic() &&
          llvm::isa<clang::CXXOperatorCallExpr>(InnerCE)) {
        InnerArgOffset = 1;
      }
    }
    llvm::SmallVector<int, 8> NextParamToCallerArg(InnerFD->getNumParams(), -1);
    unsigned InnerNumArgs = (InnerCE->getNumArgs() > InnerArgOffset)
                                ? (InnerCE->getNumArgs() - InnerArgOffset)
                                : 0;
    unsigned Limit = std::min(InnerNumArgs, InnerFD->getNumParams());
    for (unsigned i = 0; i < Limit; ++i) {
      int p = GetForwardedParamIndex(InnerCE->getArg(i + InnerArgOffset), DefFD);
      if (p >= 0 && static_cast<unsigned>(p) < ParamToCallerArg.size()) {
        NextParamToCallerArg[i] = ParamToCallerArg[p];
      }
    }

    Res = FindConstructInForwardingBody(InnerFD, TargetRD, NextParamToCallerArg,
                                        Depth + 1);
    if (Res.Constructor) {
      return Res;
    }
  }

  return Res;
}

static bool IsForwardingAllocatorDecl(const clang::FunctionDecl* FD) {
  if (!FD) return false;
  llvm::StringRef Name = FD->getName();
  if (Name == "make_unique" || Name == "make_shared" ||
      Name == "allocate_shared" || Name == "MakeGarbageCollected") {
    return true;
  }
  if (auto* Method = llvm::dyn_cast<clang::CXXMethodDecl>(FD)) {
    if (const auto* Parent = Method->getParent()) {
      llvm::StringRef ParentName = Parent->getName();
      if (Name == "New" &&
          (ParentName == "Zone" || ParentName == "AccountingAllocator" ||
           ParentName == "NodeBase")) {
        return true;
      }
      if (Name == "Allocate" && ParentName == "NodeBase") {
        return true;
      }
      if ((Name == "AddNewNode" || Name == "AddNewNodeNoInputConversion") &&
          (ParentName == "MaglevReducer" ||
           ParentName == "MaglevGraphBuilder")) {
        return true;
      }
    }
  }
  return false;
}

bool DfgVisitor::VisitCallExpr(clang::CallExpr* CE) {
  clang::FunctionDecl* FD = CE->getDirectCallee();
  if (!FD) return true;
  FD = FD->getCanonicalDecl();  // Resolve to canonical declaration!

  SourceLocation CallLoc = GetSourceLocation(CE->getBeginLoc());
  if (auto* MCE = llvm::dyn_cast<clang::CXXMemberCallExpr>(CE)) {
    SourceLocation MemberLoc = GetSourceLocation(MCE->getExprLoc());
    if (!MemberLoc.File.empty()) CallLoc = MemberLoc;
  } else if (CE->getCallee()) {
    SourceLocation CalleeLoc = GetSourceLocation(CE->getCallee()->getExprLoc());
    if (!CalleeLoc.File.empty()) CallLoc = CalleeLoc;
  }
  if (CallLoc.File.empty()) return true;

  // Intercept BitField<T, shift, size, U>::encode / update / decode calls
  if (ExtractBitFieldInfo(CE).IsValid) {
    GetNodeIdForExpr(CE);
    EnsureBitFieldCallProcessed(CE);
    return true;
  }

  // 0.5 Intercept V8 offset-based field loads/stores (TaggedField<T, kOffset>,
  // ReadField<T>(offset), WriteField<T>(offset, value), etc.)
  {
    OffsetFieldAccess OffsetAccess = ExtractOffsetFieldAccess(CE);
    if (OffsetAccess.IsValid) {
      if (OffsetAccess.IsStore && OffsetAccess.ValueExpr) {
        Node::Id FieldId = GetOrCreateOffsetFieldNode(OffsetAccess);
        Node::Id ValId = GetNodeIdForExpr(OffsetAccess.ValueExpr);
        if (FieldId != 0 && ValId != 0 && FieldId != ValId) {
          Edge E;
          E.From = ValId;
          E.To = FieldId;
          E.Kind = EdgeKind::Assignment;
          E.Loc = GetSourceLocation(CE->getExprLoc());
          EmitEdge(std::move(E));
        }
      } else {
        GetNodeIdForExpr(CE);
      }
      return true;
    }
  }

  // Skip transparent C++ smart pointer operator dereferences (-> and unary *)
  // in the DFG
  if (auto* OCE = llvm::dyn_cast<clang::CXXOperatorCallExpr>(CE)) {
    clang::OverloadedOperatorKind OOK = OCE->getOperator();
    if (OOK == clang::OO_Arrow ||
        (OOK == clang::OO_Star && OCE->getNumArgs() == 1)) {
      return true;
    }
  }

  // Skip transparent identity and cast helper calls completely in the DFG
  bool IsInstanceMember = false;
  bool IsAllocationResultFromObject = false;
  std::string name = FD->getNameAsString();
  if (name == "use") {
    return true;
  }
  if (FD->isInStdNamespace() && (name == "min" || name == "max") &&
      CE->getNumArgs() == 2) {
    GetNodeIdForExpr(CE);
    return true;
  }
  if (auto* Method = llvm::dyn_cast<clang::CXXMethodDecl>(FD)) {
    if (IsReadOnlyRootGetter(Method)) {
      return true;
    }
    if (!Method->isStatic()) {
      IsInstanceMember = true;
    } else if (name == "FromObject" && Method->getParent() &&
               Method->getParent()->getName() == "AllocationResult") {
      IsAllocationResultFromObject = true;
    }
  }
  if (!IsInstanceMember && name == "TryCast" && CE->getNumArgs() == 2) {
    Node::Id SrcId = GetNodeIdForExpr(CE->getArg(0));
    Node::Id DstId = GetNodeIdForExpr(CE->getArg(1));
    if (SrcId != 0 && DstId != 0 && SrcId != DstId) {
      Edge E;
      E.From = SrcId;
      E.To = DstId;
      E.Kind = EdgeKind::Assignment;
      SourceLocation OutLoc = GetSourceLocation(CE->getArg(1)->getBeginLoc());
      E.Loc = !OutLoc.File.empty() ? OutLoc : GetSourceLocation(CE->getBeginLoc());
      EmitEdge(std::move(E));
    }
    return true;
  }
  if (!IsInstanceMember && CE->getNumArgs() >= 1 &&
      (WrapperConfigManager::Get().IsIdentityFunction(name) ||
       IsAllocationResultFromObject)) {
    return true;
  }
  if (!IsInstanceMember && CE->getNumArgs() >= 2 &&
      (name == "Release_Store" || name == "Relaxed_Store" ||
       name == "SeqCst_Store" || name == "Release_Store_Map_Word" ||
       name == "Relaxed_Store_Map_Word" || name == "Release_CompareAndSwap" ||
       name == "Relaxed_CompareAndSwap" ||
       name == "AcquireRelease_CompareAndSwap" ||
       name == "SeqCst_CompareAndSwap" || name == "SeqCst_Swap" ||
       name == "Relaxed_Swap" || name == "Relaxed_AtomicExchange" ||
       name == "SeqCst_AtomicExchange" || name == "Relaxed_FetchOr" ||
       name == "Relaxed_AtomicIncrement")) {
    clang::Expr* TargetExpr = CE->getArg(0);
    clang::Expr* ValExpr = CE->getArg(CE->getNumArgs() - 1);
    if (!ContextStack.empty()) {
      if (const clang::FieldDecl* RecvField =
              ExtractBackingFieldFromExpr(TargetExpr)) {
        std::vector<size_t> Writes = CollectBitFieldWrites(ValExpr);
        if (!Writes.empty()) {
          auto& Ctx = ContextStack.back();
          for (size_t idx : Writes) {
            FlushBitFieldWriteToField(Ctx.PendingWrites[idx], RecvField);
          }
        }
      }
    }
    Node::Id DstId = GetNodeIdForExpr(TargetExpr);
    Node::Id SrcId = GetNodeIdForExpr(ValExpr);
    if (DstId != 0 && SrcId != 0 && DstId != SrcId) {
      Edge E;
      E.From = SrcId;
      E.To = DstId;
      E.Kind = EdgeKind::Assignment;
      E.Loc = GetSourceLocation(CE->getExprLoc());
      EmitEdge(std::move(E));
    }
    return true;
  }

  // 1. Intercept wrapper member or static store operations (writes) and forward
  // dataflow directly to the backing field node
  const WrapperConfig* Config = nullptr;
  clang::Expr* Receiver = nullptr;
  if (auto* MCE = llvm::dyn_cast<clang::CXXMemberCallExpr>(CE)) {
    Receiver = MCE->getImplicitObjectArgument();
  } else if (auto* OCE = llvm::dyn_cast<clang::CXXOperatorCallExpr>(CE)) {
    if (OCE->getNumArgs() > 0) {
      Receiver = OCE->getArg(0);
    }
  }

  if (Receiver) {
    Receiver = PeelExpr(Receiver);
    if (IsInstanceMember && CE->getNumArgs() <= 1 &&
        (name == "load" || name == "Acquire_Load" || name == "Relaxed_Load" ||
         name == "SeqCst_Load" || name == "Acquire_Load_No_Unpack" ||
         name == "Relaxed_Load_Map_Word" || name == "Relaxed_Load_Raw" ||
         name == "load_map" || name == "ptr_location" || name == "location" ||
         name == "ptr")) {
      return true;
    }
    Config = FindWrapperConfig(Receiver->getType());
  } else if (auto* Method = llvm::dyn_cast<clang::CXXMethodDecl>(FD)) {
    // Static method fallback (using parent class)
    Config =
        FindWrapperConfig(Context->getCanonicalTagType(Method->getParent()));
  }

  if (Config) {
    std::string methodName = FD->getNameAsString();
    if ((methodName == "To" || methodName == "ToHandle" ||
         methodName == "ToLocal" || methodName == "GetHeapObject" ||
         methodName == "GetHeapObjectIfWeak" ||
         methodName == "GetHeapObjectIfStrong") &&
        CE->getNumArgs() == 1 && Receiver) {
      Node::Id SrcId = GetNodeIdForExpr(Receiver);
      Node::Id DstId = GetNodeIdForExpr(CE->getArg(0));
      if (SrcId != 0 && DstId != 0 && SrcId != DstId) {
        Edge E;
        E.From = SrcId;
        E.To = DstId;
        E.Kind = EdgeKind::Assignment;
        SourceLocation OutLoc = GetSourceLocation(CE->getArg(0)->getBeginLoc());
        E.Loc = !OutLoc.File.empty() ? OutLoc : GetSourceLocation(CE->getBeginLoc());
        EmitEdge(std::move(E));
      }
      return true;
    }
    const WrapperOp* StoreOp = Config->FindStoreOp(methodName);
    if (StoreOp) {
      // Member store (e.g., obj->field_.store(value) or obj->field_ = value)
      clang::Expr* Receiver = nullptr;
      clang::Expr* ValExpr = nullptr;

      if (auto* MCE = llvm::dyn_cast<clang::CXXMemberCallExpr>(CE)) {
        Receiver = MCE->getImplicitObjectArgument();
        if (StoreOp->ValueArgIndex >= 0) {
          int ResolveIdx = StoreOp->ValueArgIndex;
          if (methodName == "store" || methodName == "Relaxed_Store" ||
              methodName == "Release_Store" || methodName == "SeqCst_Store") {
            if (CE->getNumArgs() == 1) {
              ResolveIdx = 0;
            } else if (CE->getNumArgs() >= 2 && ResolveIdx == 0 &&
                       CE->getArg(0)->getType()->isPointerType()) {
              ResolveIdx = 1;
            }
          }
          if (CE->getNumArgs() > (unsigned)ResolveIdx) {
            ValExpr = CE->getArg(ResolveIdx);
          }
        }
      } else if (auto* OCE = llvm::dyn_cast<clang::CXXOperatorCallExpr>(CE)) {
        // In CXXOperatorCallExpr, arg 0 is the receiver object (implicit
        // this), and arg 1 is the value being assigned
        if (StoreOp->ValueArgIndex >= 0 &&
            OCE->getNumArgs() > (unsigned)(StoreOp->ValueArgIndex + 1)) {
          Receiver = OCE->getArg(0);
          ValExpr = OCE->getArg(
              StoreOp->ValueArgIndex +
              1);  // Arg 0 is receiver, so ValueArgIndex is offset by 1
        }
      }

      if (Receiver && ValExpr) {
        if (!ContextStack.empty()) {
          if (const clang::FieldDecl* RecvField =
                  ExtractBackingFieldFromExpr(Receiver)) {
            std::vector<size_t> Writes = CollectBitFieldWrites(ValExpr);
            if (!Writes.empty()) {
              auto& Ctx = ContextStack.back();
              for (size_t idx : Writes) {
                FlushBitFieldWriteToField(Ctx.PendingWrites[idx], RecvField);
              }
            }
          }
        }
        Node::Id fieldId = GetNodeIdForExpr(Receiver);
        Node::Id valId = GetNodeIdForExpr(ValExpr);
        if (fieldId != 0 && valId != 0) {
          Edge E;
          E.From = valId;
          E.To = fieldId;
          E.Kind = EdgeKind::Assignment;
          E.Loc = GetSourceLocation(CE->getExprLoc());
          EmitEdge(std::move(E));
          return true;  // Skip standard parameter mapping!
        }
      }
    }

    // 2. Skip wrapper Load operations in VisitCallExpr (matching
    // GetNodeIdForExpr logic exactly)
    const WrapperOp* LoadOp = Config->FindLoadOp(methodName);
    if (LoadOp) {
      return true;
    }

    // 2.5 If Receiver is a configured wrapper class and the called method is
    // defined on a wrapper/base class (e.g. TaggedImpl::IsSmi(),
    // HandleBase::is_null()) rather than on the wrapped pointee via operator->,
    // do not bridge Receiver into the shared wrapper class's `this` parameter.
    if (auto* MCE = llvm::dyn_cast<clang::CXXMemberCallExpr>(CE)) {
      if (auto* MD = MCE->getMethodDecl()) {
        if (auto* ParentRD = MD->getParent()) {
          std::string ParentName = ParentRD->getNameAsString();
          if (WrapperConfigManager::Get().IsWrapperClass(ParentName) ||
              ParentName == "TaggedImpl" || ParentName == "HandleBase" ||
              ParentName == "DirectHandleBase") {
            return true;
          }
        }
      }
    }
  }

  // 2.6 Handle overloaded assignment and increment/decrement operators
  // (operator=, operator|=, operator+=, operator++, operator--, etc.) on
  // non-wrapper classes/structs/enums
  if (auto* OCE = llvm::dyn_cast<clang::CXXOperatorCallExpr>(CE)) {
    if ((OCE->getOperator() == clang::OO_PlusPlus ||
         OCE->getOperator() == clang::OO_MinusMinus) &&
        OCE->getNumArgs() >= 1) {
      Node::Id LHSId = GetNodeIdForExpr(OCE->getArg(0));
      Node::Id RHSId = GetNodeIdForExpr(OCE);
      if (LHSId != 0 && RHSId != 0 && LHSId != RHSId) {
        Edge E;
        E.From = RHSId;
        E.To = LHSId;
        E.Kind = EdgeKind::Assignment;
        E.Loc = GetSourceLocation(OCE->getOperatorLoc());
        EmitEdge(std::move(E));
      }
      return true;
    }
    if (OCE->isAssignmentOp() && OCE->getNumArgs() == 2) {
      Node::Id LHSId = GetNodeIdForExpr(OCE->getArg(0));
      Node::Id RHSId = GetNodeIdForExpr(OCE->getArg(1));
      if (LHSId != 0 && RHSId != 0 && LHSId != RHSId) {
        Edge E;
        E.From = RHSId;
        E.To = LHSId;
        E.Kind = EdgeKind::Assignment;
        E.Loc = GetSourceLocation(OCE->getOperatorLoc());
        EmitEdge(std::move(E));
      }
      if (FD->isDefaulted() || FD->isTrivial()) {
        return true;
      }
    }
  }

  // 2.7 Handle forwarding constructor helpers (std::make_unique<T>,
  // std::make_shared<T>, std::allocate_shared<T>, Zone::New<T>,
  // NodeBase::New<T>, MaglevReducer::AddNewNode<T>, MakeGarbageCollected<T>,
  // and custom forwarding wrappers) by structurally resolving T's constructor
  // and mapping forwarded caller arguments to constructor parameters.
  if (const auto* TSA = FD->getTemplateSpecializationArgs()) {
    if (TSA->size() >= 1 &&
        TSA->get(0).getKind() == clang::TemplateArgument::Type) {
      clang::QualType FirstArgType = TSA->get(0).getAsType();
      if (!FindWrapperConfig(FirstArgType)) {
        if (auto* TargetRD = FirstArgType->getAsCXXRecordDecl()) {
          if (TargetRD->hasDefinition()) {
            TargetRD = TargetRD->getDefinition();
            if (ReturnsTargetOrWrapper(FD->getReturnType(), TargetRD)) {
              unsigned CallArgOffset =
                  (IsInstanceMember && llvm::isa<clang::CXXOperatorCallExpr>(CE))
                      ? 1
                      : 0;
              llvm::SmallVector<int, 8> InitialParamToCallerArg(
                  FD->getNumParams(), -1);
              for (unsigned p = 0; p < FD->getNumParams(); ++p) {
                if (p + CallArgOffset < CE->getNumArgs()) {
                  InitialParamToCallerArg[p] =
                      static_cast<int>(p + CallArgOffset);
                }
              }

              ForwardedConstructorResolution Res =
                  FindConstructInForwardingBody(FD, TargetRD,
                                                InitialParamToCallerArg, 0);
              bool IsKnownFactory =
                  (name == "make_unique" || name == "make_shared" ||
                   name == "allocate_shared" ||
                   name == "MakeGarbageCollected" || name == "New" ||
                   name == "Allocate" || name == "AddNewNode" ||
                   name == "AddNewNodeNoInputConversion");
              if (Res.Constructor && Res.ArgToParamMap.empty() &&
                  !IsKnownFactory &&
                  Res.Constructor->getMinRequiredArguments() > 0) {
                // Avoid treating an unrelated internal default construction of
                // TargetRD as a forwarding constructor call unless arguments
                // were forwarded or FD is a factory/allocator.
                Res.Constructor = nullptr;
              }

              if (!Res.Constructor && IsKnownFactory) {
                unsigned NumCallArgs =
                    (CE->getNumArgs() > CallArgOffset)
                        ? (CE->getNumArgs() - CallArgOffset)
                        : 0;
                unsigned LeadingArgs =
                    ((name == "allocate_shared" ||
                      name == "MakeGarbageCollected") &&
                     NumCallArgs >= 1)
                        ? 1
                        : 0;
                unsigned NumForwarded = NumCallArgs - LeadingArgs;

                llvm::SmallVector<const clang::CXXConstructorDecl*, 8>
                    CandidateCtors;
                for (const auto* CD : TargetRD->ctors()) {
                  if (!CD->isCopyOrMoveConstructor()) {
                    CandidateCtors.push_back(CD);
                  }
                }
                for (const auto* D : TargetRD->decls()) {
                  if (const auto* FTD =
                          llvm::dyn_cast<clang::FunctionTemplateDecl>(D)) {
                    for (const auto* SpecFD : FTD->specializations()) {
                      if (const auto* SpecCD =
                              llvm::dyn_cast<clang::CXXConstructorDecl>(
                                  SpecFD)) {
                        if (!SpecCD->isCopyOrMoveConstructor()) {
                          CandidateCtors.push_back(SpecCD);
                        }
                      }
                    }
                    if (const auto* PatternCD =
                            llvm::dyn_cast<clang::CXXConstructorDecl>(
                                FTD->getTemplatedDecl())) {
                      if (!PatternCD->isCopyOrMoveConstructor()) {
                        CandidateCtors.push_back(PatternCD);
                      }
                    }
                  }
                }

                const clang::CXXConstructorDecl* MatchedCD = nullptr;
                const clang::CXXConstructorDecl* FallbackCD = nullptr;
                for (const auto* CD : CandidateCtors) {
                  if (NumForwarded >= CD->getMinRequiredArguments() &&
                      NumForwarded <= CD->getNumParams()) {
                    if (!FallbackCD) {
                      FallbackCD = CD;
                    }
                    bool TypesMatch = true;
                    for (unsigned i = 0; i < NumForwarded; ++i) {
                      clang::QualType ArgT =
                          CE->getArg(CallArgOffset + LeadingArgs + i)
                              ->getType()
                              .getNonReferenceType()
                              .getUnqualifiedType();
                      clang::QualType ParamT =
                          CD->getParamDecl(i)
                              ->getType()
                              .getNonReferenceType()
                              .getUnqualifiedType();
                      if (!ParamT->isDependentType() &&
                          !Context->hasSameUnqualifiedType(ArgT, ParamT)) {
                        TypesMatch = false;
                        break;
                      }
                    }
                    if (TypesMatch) {
                      MatchedCD = CD;
                      break;
                    }
                  }
                }
                if (!MatchedCD) MatchedCD = FallbackCD;
                if (MatchedCD) {
                  Res.Constructor = MatchedCD;
                  unsigned Min =
                      std::min(NumForwarded, MatchedCD->getNumParams());
                  for (unsigned i = 0; i < Min; ++i) {
                    Res.ArgToParamMap.push_back(
                        {CallArgOffset + LeadingArgs + i, i});
                  }
                }
              }

              if (const clang::CXXConstructorDecl* MatchedCD =
                      Res.Constructor) {
                Node::Id CallExprId = GetNodeIdForExpr(CE);
                llvm::SmallVector<bool, 8> MappedCtorParams(
                    MatchedCD->getNumParams(), false);
                for (const auto& [CallerArgIdx, CtorParamIdx] :
                     Res.ArgToParamMap) {
                  if (CallerArgIdx >= CE->getNumArgs() ||
                      CtorParamIdx >= MatchedCD->getNumParams()) {
                    continue;
                  }
                  MappedCtorParams[CtorParamIdx] = true;
                  clang::Expr* CallerArg = CE->getArg(CallerArgIdx);
                  const clang::ParmVarDecl* CtorParam =
                      MatchedCD->getParamDecl(CtorParamIdx);
                  Node::Id ArgId =
                      llvm::isa<clang::CXXDefaultArgExpr>(CallerArg)
                          ? GetNodeIdForDefaultArgExpr(CallerArg, CtorParam)
                          : GetNodeIdForExpr(CallerArg);
                  Node::Id ParamId = GetOrCreateNodeForDecl(
                      const_cast<clang::ParmVarDecl*>(CtorParam));
                  if (ArgId != 0 && ParamId != 0) {
                    Edge E;
                    E.From = ArgId;
                    E.To = ParamId;
                    E.Kind = EdgeKind::ArgPass;
                    SourceLocation ArgLoc =
                        GetSourceLocation(CallerArg->getBeginLoc());
                    if (auto* DAE =
                            llvm::dyn_cast<clang::CXXDefaultArgExpr>(CallerArg)) {
                      ArgLoc = GetSourceLocation(DAE->getUsedLocation());
                    }
                    if (ArgLoc.File.empty()) {
                      ArgLoc = CallLoc;
                    }
                    E.Loc = ArgLoc;
                    EmitEdge(std::move(E));
                  }
                }

                // Wire any unmapped constructor parameters that have default
                // arguments (e.g. Zone::New<T>(x) where T(int x, int mode = kDef)).
                for (unsigned p = 0; p < MatchedCD->getNumParams(); ++p) {
                  if (MappedCtorParams[p]) continue;
                  const clang::ParmVarDecl* DefaultPVD = nullptr;
                  for (const clang::FunctionDecl* RedeFD :
                       MatchedCD->redecls()) {
                    if (p < RedeFD->getNumParams()) {
                      const clang::ParmVarDecl* Cand = RedeFD->getParamDecl(p);
                      if (Cand->hasDefaultArg() &&
                          !Cand->hasUninstantiatedDefaultArg() &&
                          !Cand->hasUnparsedDefaultArg()) {
                        DefaultPVD = Cand;
                        break;
                      }
                    }
                  }
                  if (DefaultPVD) {
                    Node::Id DefArgId = GetNodeIdForDefaultArgExpr(
                        const_cast<clang::Expr*>(DefaultPVD->getDefaultArg()),
                        DefaultPVD);
                    Node::Id ParamId =
                        GetOrCreateNodeForDecl(const_cast<clang::ParmVarDecl*>(
                            MatchedCD->getParamDecl(p)));
                    if (DefArgId != 0 && ParamId != 0) {
                      Edge E;
                      E.From = DefArgId;
                      E.To = ParamId;
                      E.Kind = EdgeKind::ArgPass;
                      E.Loc = CallLoc;
                      EmitEdge(std::move(E));
                    }
                  }
                }

                // Also connect any std::initializer_list / InitListExpr caller
                // arguments (such as `{object}` in `AddNewNode<T>({object}, ...)`)
                // to CallExprId so node input operands flow into the created node.
                if (CallExprId != 0) {
                  for (unsigned i = CallArgOffset; i < CE->getNumArgs(); ++i) {
                    if (auto* ILE = FindInitListExpr(CE->getArg(i))) {
                      for (unsigned k = 0; k < ILE->getNumInits(); ++k) {
                        clang::Expr* Elem = ILE->getInit(k);
                        if (!Elem) continue;
                        Node::Id ElemId = GetNodeIdForExpr(Elem);
                        if (ElemId != 0 && ElemId != CallExprId) {
                          Edge E2;
                          E2.From = ElemId;
                          E2.To = CallExprId;
                          E2.Kind = EdgeKind::ArgPass;
                          E2.Loc = GetSourceLocation(Elem->getBeginLoc());
                          EmitEdge(std::move(E2));
                        }
                      }
                    }
                  }
                }

                SourceLocation CtorLoc =
                    GetSourceLocation(MatchedCD->getLocation());
                if (!CtorLoc.File.empty()) {
                  llvm::StringRef CtorUSR = GetUSRForDecl(MatchedCD);
                  Node::Id CtorRetId =
                      !CtorUSR.empty()
                          ? HashFnv1a64(CtorUSR.str() + "/return")
                          : GenerateId(MatchedCD->getNameAsString() + "_return",
                                       CtorLoc);
                  Node N;
                  N.ID = CtorRetId;
                  N.Kind = NodeKind::Expression;
                  N.Name = MatchedCD->getNameAsString() + "_return";
                  N.Type = MatchedCD->getNameAsString();
                  N.Loc = CtorLoc;
                  EmitNode(std::move(N));

                  if (CtorRetId != 0 && CallExprId != 0) {
                    Edge E;
                    E.From = CtorRetId;
                    E.To = CallExprId;
                    E.Kind = EdgeKind::Return;
                    E.Loc = GetSourceLocation(CE->getBeginLoc());
                    EmitEdge(std::move(E));
                  }
                }
                return true;
              }
            }
          }
        }
      }
    }
  }

  // 2.8 Also flush pending BitField::encode / BitField::update writes if a
  // method's AST body stores its parameter into a member field
  if (!ContextStack.empty()) {
    if (auto* Method = llvm::dyn_cast<clang::CXXMethodDecl>(FD)) {
      if (const clang::FieldDecl* SetterField =
              ResolveSetterBackingField(Method)) {
        auto& Ctx = ContextStack.back();
        for (unsigned i = 0; i < CE->getNumArgs(); ++i) {
          std::vector<size_t> Writes = CollectBitFieldWrites(CE->getArg(i));
          for (size_t idx : Writes) {
            FlushBitFieldWriteToField(Ctx.PendingWrites[idx], SetterField);
          }
        }
      }
    }
  }

  // 4. Skip wrapper class member calls in VisitCallExpr, matching
  // GetNodeIdForExpr which resolves them via WrapperConfig rules.
  IsInstanceMember = false;
  if (auto* Method = llvm::dyn_cast<clang::CXXMethodDecl>(FD)) {
    if (WrapperConfigManager::Get().IsWrapperClass(
            Method->getParent()->getNameAsString())) {
      return true;
    }
    if (!Method->isStatic()) {
      IsInstanceMember = true;
    }
  }

  // 1. Map arguments to parameters (for non-static member CXXOperatorCallExpr,
  // arg 0 is the implicit object receiver and explicit parameters start at 1)
  unsigned ArgOffset =
      (IsInstanceMember && llvm::isa<clang::CXXOperatorCallExpr>(CE)) ? 1 : 0;
  unsigned NumArgs =
      (CE->getNumArgs() > ArgOffset) ? (CE->getNumArgs() - ArgOffset) : 0;
  unsigned NumParams = FD->getNumParams();
  unsigned Min = std::min(NumArgs, NumParams);

  auto FindDefaultArgExpr =
      [](clang::Expr* E) -> clang::CXXDefaultArgExpr* {
    while (E) {
      if (auto* DAE = llvm::dyn_cast<clang::CXXDefaultArgExpr>(E)) return DAE;
      if (auto* CE = llvm::dyn_cast<clang::CastExpr>(E))
        E = CE->getSubExpr();
      else if (auto* PE = llvm::dyn_cast<clang::ParenExpr>(E))
        E = PE->getSubExpr();
      else if (auto* BTE = llvm::dyn_cast<clang::CXXBindTemporaryExpr>(E))
        E = BTE->getSubExpr();
      else if (auto* EWC = llvm::dyn_cast<clang::ExprWithCleanups>(E))
        E = EWC->getSubExpr();
      else if (auto* MTE = llvm::dyn_cast<clang::MaterializeTemporaryExpr>(E))
        E = MTE->getSubExpr();
      else
        break;
    }
    return nullptr;
  };

  for (unsigned i = 0; i < NumParams; ++i) {
    clang::Expr* Arg = (i < NumArgs) ? CE->getArg(i + ArgOffset) : nullptr;
    clang::ParmVarDecl* Param = FD->getParamDecl(i);
    Node::Id ParamId = GetOrCreateNodeForDecl(Param);

    if (Arg) {
      if (auto* ILE = FindInitListExpr(Arg)) {
        Node::Id CallExprId = GetNodeIdForExpr(CE);
        for (unsigned k = 0; k < ILE->getNumInits(); ++k) {
          clang::Expr* Elem = ILE->getInit(k);
          if (!Elem) continue;
          Node::Id ElemId = GetNodeIdForExpr(Elem);
          SourceLocation ElemLoc = GetSourceLocation(Elem->getBeginLoc());
          if (ElemId != 0 && ParamId != 0) {
            Edge E;
            E.From = ElemId;
            E.To = ParamId;
            E.Kind = EdgeKind::ArgPass;
            E.Loc = ElemLoc;
            EmitEdge(std::move(E));
          }
          if (ElemId != 0 && CallExprId != 0 && ElemId != CallExprId) {
            Edge E2;
            E2.From = ElemId;
            E2.To = CallExprId;
            E2.Kind = EdgeKind::ArgPass;
            E2.Loc = ElemLoc;
            EmitEdge(std::move(E2));
          }
        }
        continue;
      }
    }

    clang::CXXDefaultArgExpr* DAE = FindDefaultArgExpr(Arg);
    Node::Id ArgId = (DAE || !Arg)
                         ? GetNodeIdForDefaultArgExpr(DAE ? DAE : Arg, Param)
                         : GetNodeIdForExpr(Arg);

    if (ArgId != 0 && ParamId != 0) {
      Edge E;
      E.From = ArgId;
      E.To = ParamId;
      E.Kind = EdgeKind::ArgPass;
      SourceLocation ArgLoc =
          Arg ? GetSourceLocation(Arg->getBeginLoc()) : SourceLocation{};
      if (DAE) {
        ArgLoc = GetSourceLocation(DAE->getUsedLocation());
      }
      if (ArgLoc.File.empty()) {
        ArgLoc = CallLoc;
      }
      E.Loc = ArgLoc;
      EmitEdge(std::move(E));
    }
  }

  // 1.5 Bridge receiver to callee's 'this' parameter for non-static member
  // methods using the exact same USR-based ID as CXXThisExpr
  if (auto* Method = llvm::dyn_cast<clang::CXXMethodDecl>(FD)) {
    if (!Method->isStatic() &&
        !WrapperConfigManager::Get().IsWrapperClass(
            Method->getParent()->getNameAsString())) {
      clang::Expr* Receiver = nullptr;
      if (auto* MCE = llvm::dyn_cast<clang::CXXMemberCallExpr>(CE)) {
        Receiver = MCE->getImplicitObjectArgument();
      } else if (auto* OCE = llvm::dyn_cast<clang::CXXOperatorCallExpr>(CE)) {
        if (OCE->getNumArgs() > 0) {
          Receiver = OCE->getArg(0);
        }
      }

      if (Receiver) {
        clang::CXXMethodDecl* CanonicalMethod = Method->getCanonicalDecl();
        SourceLocation CalleeLoc = GetSourceLocation(CanonicalMethod->getLocation());
        if (!CalleeLoc.File.empty()) {
          llvm::StringRef MethodUSR = GetUSRForDecl(CanonicalMethod);
          Node::Id CalleeThisParamId =
              !MethodUSR.empty() ? HashFnv1a64(MethodUSR.str() + "/this")
                                 : GenerateId("this", CalleeLoc);
          Node N;
          N.ID = CalleeThisParamId;
          N.Kind = NodeKind::Parameter;
          N.Name = "this";
          N.Type = GetTypeString(CanonicalMethod->getThisType()).str();
          N.Loc = CalleeLoc;
          EmitNode(std::move(N));

          Node::Id ReceiverId = GetNodeIdForExpr(Receiver);
          if (ReceiverId != 0 && CalleeThisParamId != 0) {
            Edge E;
            E.From = ReceiverId;
            E.To = CalleeThisParamId;
            E.Kind = EdgeKind::ArgPass;
            E.Loc = GetSourceLocation(Receiver->getBeginLoc());
            EmitEdge(std::move(E));
          }
        }
      }
    }
  }

  // 2. Flow from Function Return to CallExpr node (only for non-void functions!)
  if (!FD->getReturnType()->isVoidType()) {
    SourceLocation FuncLoc = GetSourceLocation(FD->getLocation());
    if (!FuncLoc.File.empty()) {
      llvm::StringRef UsrReturnId = GetUSRForDecl(FD);
      Node::Id ReturnNodeId;
      if (!UsrReturnId.empty()) {
        ReturnNodeId = HashFnv1a64(UsrReturnId.str() + "/return");
      } else {
        ReturnNodeId = GenerateId(FD->getNameAsString() + "_return", FuncLoc);
      }

      Node N;
      N.ID = ReturnNodeId;
      N.Kind = NodeKind::Expression;
      N.Name = FD->getNameAsString() + "_return";
      N.Type = GetTypeString(FD->getReturnType()).str();
      N.Loc = FuncLoc;
      EmitNode(std::move(N));

      Node::Id CallExprId = GetNodeIdForExpr(CE);
      if (ReturnNodeId != 0 && CallExprId != 0) {
        Edge E;
        E.From = ReturnNodeId;
        E.To = CallExprId;
        E.Kind = EdgeKind::Return;
        E.Loc = GetSourceLocation(CE->getBeginLoc());
        EmitEdge(std::move(E));
      }
    }
  }

  return true;
}

bool DfgVisitor::VisitCXXConstructExpr(clang::CXXConstructExpr* CE) {
  clang::CXXConstructorDecl* CD = CE->getConstructor();
  if (!CD) return true;
  if (CD->isDefaultConstructor() || CD->isCopyOrMoveConstructor()) {
    return true;
  }
  if (FindWrapperConfig(CE->getType()) != nullptr) {
    return true;
  }
  if (auto* ParentRD = CD->getParent()) {
    if (WrapperConfigManager::Get().IsWrapperClass(
            ParentRD->getNameAsString())) {
      return true;
    }
  }

  CD = CD->getCanonicalDecl();
  SourceLocation CallLoc = GetSourceLocation(CE->getBeginLoc());
  if (CallLoc.File.empty()) return true;

  // 1. Map arguments to constructor parameters
  unsigned NumArgs = CE->getNumArgs();
  unsigned NumParams = CD->getNumParams();
  unsigned Min = std::min(NumArgs, NumParams);

  auto FindDefaultArgExpr =
      [](clang::Expr* E) -> clang::CXXDefaultArgExpr* {
    while (E) {
      if (auto* DAE = llvm::dyn_cast<clang::CXXDefaultArgExpr>(E)) return DAE;
      if (auto* CE = llvm::dyn_cast<clang::CastExpr>(E))
        E = CE->getSubExpr();
      else if (auto* PE = llvm::dyn_cast<clang::ParenExpr>(E))
        E = PE->getSubExpr();
      else if (auto* BTE = llvm::dyn_cast<clang::CXXBindTemporaryExpr>(E))
        E = BTE->getSubExpr();
      else if (auto* EWC = llvm::dyn_cast<clang::ExprWithCleanups>(E))
        E = EWC->getSubExpr();
      else if (auto* MTE = llvm::dyn_cast<clang::MaterializeTemporaryExpr>(E))
        E = MTE->getSubExpr();
      else
        break;
    }
    return nullptr;
  };

  for (unsigned i = 0; i < NumParams; ++i) {
    clang::Expr* Arg = (i < NumArgs) ? CE->getArg(i) : nullptr;
    clang::ParmVarDecl* Param = CD->getParamDecl(i);

    clang::CXXDefaultArgExpr* DAE = FindDefaultArgExpr(Arg);
    Node::Id ArgId = (DAE || !Arg)
                         ? GetNodeIdForDefaultArgExpr(DAE ? DAE : Arg, Param)
                         : GetNodeIdForExpr(Arg);
    Node::Id ParamId = GetOrCreateNodeForDecl(Param);

    if (ArgId != 0 && ParamId != 0) {
      Edge E;
      E.From = ArgId;
      E.To = ParamId;
      E.Kind = EdgeKind::ArgPass;
      SourceLocation ArgLoc =
          Arg ? GetSourceLocation(Arg->getBeginLoc()) : SourceLocation{};
      if (DAE) {
        ArgLoc = GetSourceLocation(DAE->getUsedLocation());
      }
      if (ArgLoc.File.empty()) {
        ArgLoc = CallLoc;
      }
      E.Loc = ArgLoc;
      EmitEdge(std::move(E));
    }
  }

  // 2. Flow from Constructor Return node to CXXConstructExpr node
  SourceLocation FuncLoc = GetSourceLocation(CD->getLocation());
  if (!FuncLoc.File.empty()) {
    llvm::StringRef UsrReturnId = GetUSRForDecl(CD);
    Node::Id ReturnNodeId;
    if (!UsrReturnId.empty()) {
      ReturnNodeId = HashFnv1a64(UsrReturnId.str() + "/return");
    } else {
      ReturnNodeId = GenerateId(CD->getNameAsString() + "_return", FuncLoc);
    }

    Node N;
    N.ID = ReturnNodeId;
    N.Kind = NodeKind::Expression;
    N.Name = CD->getNameAsString() + "_return";
    N.Type = GetTypeString(CE->getType()).str();
    N.Loc = FuncLoc;
    EmitNode(std::move(N));

    Node::Id ConstructExprId = GetNodeIdForExpr(CE);
    if (ReturnNodeId != 0 && ConstructExprId != 0 &&
        ReturnNodeId != ConstructExprId) {
      Edge E;
      E.From = ReturnNodeId;
      E.To = ConstructExprId;
      E.Kind = EdgeKind::Return;
      E.Loc = GetSourceLocation(CE->getBeginLoc());
      EmitEdge(std::move(E));
    }
  }

  return true;
}

static clang::FunctionDecl* ResolveHeaderCanonicalDecl(
    clang::FunctionDecl* FD) {
  if (!FD) return nullptr;
  clang::FunctionDecl* CanonicalFD = FD->getCanonicalDecl();
  if (CanonicalFD == FD) {
    if (clang::FunctionDecl* Pattern = FD->getTemplateInstantiationPattern()) {
      clang::FunctionDecl* PatCanon = Pattern->getCanonicalDecl();
      const clang::FunctionDecl* PatDef = Pattern->getDefinition();
      if (PatCanon && (PatCanon != Pattern || (PatDef && PatCanon != PatDef))) {
        CanonicalFD = PatCanon;
      }
    }
    if (CanonicalFD == FD) {
      if (clang::FunctionTemplateDecl* FTD = FD->getPrimaryTemplate()) {
        if (clang::FunctionDecl* HeaderTD =
                FTD->getCanonicalDecl()->getTemplatedDecl()) {
          if (HeaderTD != FD) {
            CanonicalFD = HeaderTD;
          }
        }
      }
    }
  }
  return CanonicalFD;
}

bool DfgVisitor::TraverseDecl(clang::Decl* D) {
  if (!D) return true;

  // 1. Never skip the TranslationUnitDecl (root of the AST)
  if (llvm::isa<clang::TranslationUnitDecl>(D)) {
    return clang::RecursiveASTVisitor<DfgVisitor>::TraverseDecl(D);
  }

  // 2. Fast O(1) system header check using Clang's native metadata
  clang::SourceLocation DeclLoc = D->getLocation();
  if (DeclLoc.isValid()) {
    if (Context->getSourceManager().isInSystemHeader(DeclLoc)) {
      return true;  // Skip immediately with zero overhead!
    }

    // 3. Fallback check for external files outside the workspace
    SourceLocation Loc = GetSourceLocation(DeclLoc);
    if (Loc.File.empty()) {
      return true;  // Skip!
    }
  }

  // 4. Unify Parameter & Return Declarations and Definitions
  // (connects call sites to function body, and enables querying from definition site)
  clang::FunctionDecl* FD = llvm::dyn_cast_or_null<clang::FunctionDecl>(D);
  if (FD) {
    if (auto* CTSD =
            llvm::dyn_cast_or_null<clang::ClassTemplateSpecializationDecl>(
                FD->getDeclContext())) {
      if (CTSD->getName() == "BitField" ||
          CTSD->getName() == "BitSetComputer") {
        return true;
      }
    }
    if (IsForwardingAllocatorDecl(FD)) {
      return true;
    }
    bool IsInstanceMember = false;
    bool IsAllocationResultFromObject = false;
    if (auto* Method = llvm::dyn_cast<clang::CXXMethodDecl>(FD)) {
      if (IsReadOnlyRootGetter(Method)) {
        return true;
      }
      if (!Method->isStatic()) {
        IsInstanceMember = true;
      } else if (FD->getName() == "FromObject" && Method->getParent() &&
                 Method->getParent()->getName() == "AllocationResult") {
        IsAllocationResultFromObject = true;
      }
    }
    if (!IsInstanceMember && FD->getNumParams() >= 1 &&
        (WrapperConfigManager::Get().IsIdentityFunction(
             FD->getNameAsString()) ||
         IsAllocationResultFromObject)) {
      return true;
    }
    // Forward declarations without a body never produce DFG nodes or edges on
    // their own (parameter/return nodes are emitted on-demand at call sites).
    if (!FD->isThisDeclarationADefinition() || !FD->hasBody()) {
      return true;
    }
    // Deduplicate identical function/method/template-instantiation definitions
    // across translation units during parallel indexing.
    if (SharedClaims) {
      SourceLocation FuncLoc = GetSourceLocation(FD->getLocation());
      llvm::StringRef Usr = GetUSRForDecl(UnwrapTemplatePattern(FD));
      std::string SpecSuffix = FD->isDependentContext() ? "@pat" : "@inst";
      auto AppendShallowArgSlice =
          [this](auto& Self, llvm::ArrayRef<clang::TemplateArgument> Args,
                 std::string& Out) -> void {
        Out += "<";
        for (unsigned i = 0; i < Args.size(); ++i) {
          if (i > 0) Out += ",";
          const clang::TemplateArgument& Arg = Args[i];
          if (Arg.getKind() == clang::TemplateArgument::Pack) {
            Self(Self, Arg.getPackAsArray(), Out);
            continue;
          }
          if (Arg.getKind() == clang::TemplateArgument::Type) {
            clang::QualType T = Arg.getAsType();
            if (!T.isNull()) {
              T = T.getCanonicalType().getNonReferenceType();
              if (T->isPointerType()) T = T->getPointeeType();
              if (auto* RD = T->getAsCXXRecordDecl()) {
                std::string Name = RD->getNameAsString();
                if (Name.empty()) {
                  Name = GetTypeString(T).str();
                }
                if (WrapperConfigManager::Get().IsWrapperClass(Name)) {
                  if (auto* CTSD =
                          llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(
                              RD)) {
                    const auto& InnerArgs = CTSD->getTemplateArgs();
                    if (InnerArgs.size() > 0 &&
                        InnerArgs[0].getKind() ==
                            clang::TemplateArgument::Type) {
                      if (auto* InnerRD =
                              InnerArgs[0].getAsType()->getAsCXXRecordDecl()) {
                        Out += InnerRD->getNameAsString();
                        continue;
                      }
                    }
                  }
                }
                Out += Name;
                continue;
              }
            }
          }
          if (Arg.getKind() == clang::TemplateArgument::Template ||
              Arg.getKind() == clang::TemplateArgument::TemplateExpansion) {
            if (const auto* TD =
                    Arg.getAsTemplateOrTemplatePattern().getAsTemplateDecl()) {
              Out += TD->getNameAsString();
              continue;
            }
          }
          Out += std::to_string(static_cast<int>(Arg.getKind()));
        }
        Out += ">";
      };
      if (!FD->isDependentContext()) {
        if (auto* CTSD =
                llvm::dyn_cast_or_null<clang::ClassTemplateSpecializationDecl>(
                    FD->getDeclContext())) {
          AppendShallowArgSlice(AppendShallowArgSlice,
                                CTSD->getTemplateArgs().asArray(), SpecSuffix);
        }
        if (const auto* TSA = FD->getTemplateSpecializationArgs()) {
          AppendShallowArgSlice(AppendShallowArgSlice, TSA->asArray(),
                                SpecSuffix);
        }
      }
      std::string ClaimKey = Usr.str() + SpecSuffix + "@" + FuncLoc.File + ":" +
                             std::to_string(FuncLoc.Line) + ":" +
                             std::to_string(FuncLoc.Column);
      if (!SharedClaims->TryClaim(HashFnv1a64(ClaimKey))) {
        return true;
      }
    }
  } else if (auto* VD = llvm::dyn_cast<clang::VarDecl>(D)) {
    if (!llvm::isa<clang::ParmVarDecl>(VD) && VD->isFileVarDecl() &&
        SharedClaims) {
      SourceLocation VarLoc = GetSourceLocation(VD->getLocation());
      llvm::StringRef Usr = GetUSRForDecl(VD);
      std::string ClaimKey = Usr.str() + "@var@" + VarLoc.File + ":" +
                             std::to_string(VarLoc.Line) + ":" +
                             std::to_string(VarLoc.Column);
      if (!SharedClaims->TryClaim(HashFnv1a64(ClaimKey))) {
        return true;
      }
    }
  }
  llvm::SmallVector<std::pair<clang::ValueDecl*, Node::Id>, 4>
      RestoredDeclParamNodes;
  if (FD && FD->isThisDeclarationADefinition() && FD->hasBody()) {
    clang::FunctionDecl* CanonicalFD = ResolveHeaderCanonicalDecl(FD);
    if (CanonicalFD && CanonicalFD != FD) {
      // A. Unify Parameters (strictly forward: DeclParam -> DefParam)
      unsigned NumParams = FD->getNumParams();
      unsigned NumCanonicalParams = CanonicalFD->getNumParams();
      unsigned Min = std::min(NumParams, NumCanonicalParams);
      llvm::StringRef FuncUSR = GetUSRForDecl(FD);

      for (unsigned i = 0; i < Min; ++i) {
        clang::ParmVarDecl* DefParam = FD->getParamDecl(i);
        clang::ParmVarDecl* DeclParam = CanonicalFD->getParamDecl(i);

        SourceLocation DefParamLoc = GetSourceLocation(DefParam->getLocation());
        SourceLocation DeclParamLoc =
            GetSourceLocation(DeclParam->getLocation());
        auto SameLoc = [](const SourceLocation& A, const SourceLocation& B) {
          return A.File == B.File && A.Line == B.Line && A.Column == B.Column;
        };
        bool HadSameDeclAndDefLoc = SameLoc(DefParamLoc, DeclParamLoc);
        if (HadSameDeclAndDefLoc) {
          if (const clang::FunctionDecl* PatternFD =
                  FD->getTemplateInstantiationPattern()) {
            const clang::FunctionDecl* PatternDef =
                PatternFD->getDefinition() ? PatternFD->getDefinition()
                                           : PatternFD;
            if (PatternDef && i < PatternDef->getNumParams()) {
              SourceLocation PatDefLoc =
                  GetSourceLocation(PatternDef->getParamDecl(i)->getLocation());
              if (!PatDefLoc.File.empty()) {
                DefParamLoc = PatDefLoc;
              }
            }
          }
        }

        Node::Id DeclParamId = 0;
        if (!FuncUSR.empty() && !DeclParamLoc.File.empty()) {
          std::string DeclParamUSR =
              FuncUSR.str() + "/param_" + std::to_string(i) + "@" +
              DeclParamLoc.File + ":" + std::to_string(DeclParamLoc.Line) +
              ":" + std::to_string(DeclParamLoc.Column);
          DeclParamId = HashFnv1a64(DeclParamUSR);
          Node N;
          N.ID = DeclParamId;
          N.Kind = NodeKind::Parameter;
          N.Name = !DeclParam->getName().empty()
                       ? DeclParam->getNameAsString()
                       : DefParam->getNameAsString();
          N.Type = GetTypeString(DefParam->getType()).str();
          N.Loc = DeclParamLoc;
          EmitNode(std::move(N));
        } else {
          DeclParamId = GetOrCreateNodeForDecl(DeclParam);
        }

        Node::Id DefParamId = 0;
        if (HadSameDeclAndDefLoc && !SameLoc(DefParamLoc, DeclParamLoc) &&
            !FuncUSR.empty() && !DefParamLoc.File.empty()) {
          std::string DefParamUSR =
              FuncUSR.str() + "/param_" + std::to_string(i) + "@" +
              DefParamLoc.File + ":" + std::to_string(DefParamLoc.Line) + ":" +
              std::to_string(DefParamLoc.Column);
          DefParamId = HashFnv1a64(DefParamUSR);
          Node N;
          N.ID = DefParamId;
          N.Kind = NodeKind::Parameter;
          N.Name = !DefParam->getName().empty()
                       ? DefParam->getNameAsString()
                       : DeclParam->getNameAsString();
          N.Type = GetTypeString(DefParam->getType()).str();
          N.Loc = DefParamLoc;
          EmitNode(std::move(N));
          DeclNodeCache[DefParam] = DefParamId;
          if (DeclParamId != 0) {
            RestoredDeclParamNodes.push_back({DefParam, DeclParamId});
          }
        } else {
          DefParamId = GetOrCreateNodeForDecl(DefParam);
        }

        if (DeclParamId != 0 && DefParamId != 0 && DeclParamId != DefParamId) {
          Edge E1;
          E1.From = DeclParamId;
          E1.To = DefParamId;
          E1.Kind = EdgeKind::Implicit;
          E1.Loc = DefParamLoc;
          EmitEdge(std::move(E1));
        }
      }

      // B. Unify Return Nodes (for non-void functions and constructors, strictly Canonical -> Def)
      if (!FD->getReturnType()->isVoidType() ||
          llvm::isa<clang::CXXConstructorDecl>(FD)) {
        SourceLocation FuncLoc = GetSourceLocation(FD->getLocation());
        SourceLocation CanonicalFuncLoc =
            GetSourceLocation(CanonicalFD->getLocation());

        if (!FuncLoc.File.empty() && !CanonicalFuncLoc.File.empty()) {
          llvm::StringRef UsrReturnId = GetUSRForDecl(FD);
          Node::Id CanonicalReturnNodeId = 0;
          if (!UsrReturnId.empty()) {
            CanonicalReturnNodeId = HashFnv1a64(UsrReturnId.str() + "/return");
          } else {
            CanonicalReturnNodeId = GenerateId(
                CanonicalFD->getNameAsString() + "_return", CanonicalFuncLoc);
          }

          Node::Id DefReturnNodeId =
              !UsrReturnId.empty()
                  ? HashFnv1a64(UsrReturnId.str() + "/def_return@" +
                                FuncLoc.File + ":" +
                                std::to_string(FuncLoc.Line) + ":" +
                                std::to_string(FuncLoc.Column))
                  : GenerateId(FD->getNameAsString() + "_return", FuncLoc);

          if (DefReturnNodeId != 0 && CanonicalReturnNodeId != 0 &&
              DefReturnNodeId != CanonicalReturnNodeId) {
            Node N;
            N.ID = DefReturnNodeId;
            N.Kind = NodeKind::Expression;
            N.Name = FD->getNameAsString() + "_return";
            N.Type = llvm::isa<clang::CXXConstructorDecl>(FD)
                         ? FD->getNameAsString()
                         : GetTypeString(FD->getReturnType()).str();
            N.Loc = FuncLoc;
            EmitNode(std::move(N));

            // Connect Canonical Return -> Definition Return (enables querying
            // on definition signature without cycle!)
            Edge E1;
            E1.From = CanonicalReturnNodeId;
            E1.To = DefReturnNodeId;
            E1.Kind = EdgeKind::Implicit;
            E1.Loc = FuncLoc;
            EmitEdge(std::move(E1));
          }
        }
      }
    }

    // C. Connect default argument expressions declared on FD, CanonicalFD,
    // template pattern, or any redeclaration to each parameter node so
    // parameter queries include default argument values even when inspected
    // directly inside the callee or across translation units.
    for (unsigned i = 0; i < FD->getNumParams(); ++i) {
      clang::ParmVarDecl* DefParam = FD->getParamDecl(i);
      clang::ParmVarDecl* DeclParam =
          (CanonicalFD && i < CanonicalFD->getNumParams())
              ? CanonicalFD->getParamDecl(i)
              : DefParam;
      const clang::ParmVarDecl* DefaultPVD = nullptr;
      auto CheckParamDefault = [&](const clang::ParmVarDecl* Cand) {
        if (!DefaultPVD && Cand && Cand->hasDefaultArg() &&
            !Cand->hasUnparsedDefaultArg()) {
          DefaultPVD = Cand;
        }
      };
      CheckParamDefault(DeclParam);
      CheckParamDefault(DefParam);
      for (const clang::FunctionDecl* RedeFD : FD->redecls()) {
        if (i < RedeFD->getNumParams()) {
          CheckParamDefault(RedeFD->getParamDecl(i));
        }
      }
      if (!DefaultPVD && CanonicalFD && CanonicalFD != FD) {
        for (const clang::FunctionDecl* RedeFD : CanonicalFD->redecls()) {
          if (i < RedeFD->getNumParams()) {
            CheckParamDefault(RedeFD->getParamDecl(i));
          }
        }
      }
      if (!DefaultPVD) {
        if (const clang::FunctionDecl* PatFD =
                llvm::dyn_cast_or_null<clang::FunctionDecl>(
                    UnwrapTemplatePattern(FD))) {
          for (const clang::FunctionDecl* RedeFD : PatFD->redecls()) {
            if (i < RedeFD->getNumParams()) {
              CheckParamDefault(RedeFD->getParamDecl(i));
            }
          }
        }
      }
      if (DefaultPVD) {
        clang::Expr* DefExpr =
            DefaultPVD->hasUninstantiatedDefaultArg()
                ? const_cast<clang::Expr*>(
                      DefaultPVD->getUninstantiatedDefaultArg())
                : const_cast<clang::Expr*>(DefaultPVD->getDefaultArg());
        Node::Id DefaultNodeId =
            GetNodeIdForDefaultArgExpr(DefExpr, DefaultPVD);
        Node::Id TargetParamId = 0;
        if (CanonicalFD && CanonicalFD != FD && !GetUSRForDecl(FD).empty()) {
          SourceLocation DeclParamLoc =
              GetSourceLocation(DeclParam->getLocation());
          if (!DeclParamLoc.File.empty()) {
            TargetParamId = HashFnv1a64(
                GetUSRForDecl(FD).str() + "/param_" + std::to_string(i) + "@" +
                DeclParamLoc.File + ":" + std::to_string(DeclParamLoc.Line) +
                ":" + std::to_string(DeclParamLoc.Column));
          }
        }
        if (TargetParamId == 0) {
          TargetParamId = GetOrCreateNodeForDecl(DefParam);
        }
        if (DefaultNodeId != 0 && TargetParamId != 0 &&
            DefaultNodeId != TargetParamId) {
          Edge EDef;
          EDef.From = DefaultNodeId;
          EDef.To = TargetParamId;
          EDef.Kind = EdgeKind::Initialization;
          SourceLocation DefLoc =
              DefExpr ? GetSourceLocation(DefExpr->getBeginLoc())
                      : SourceLocation{};
          if (DefLoc.File.empty()) {
            DefLoc = GetSourceLocation(DefaultPVD->getLocation());
          }
          EDef.Loc = DefLoc;
          EmitEdge(std::move(EDef));
        }
      }
    }
  }

  bool Result = clang::RecursiveASTVisitor<DfgVisitor>::TraverseDecl(D);
  for (const auto& [ParamDecl, RestoredId] : RestoredDeclParamNodes) {
    DeclNodeCache[ParamDecl] = RestoredId;
  }
  return Result;
}

bool DfgVisitor::TraverseFunctionDecl(clang::FunctionDecl* FD) {
  if (FD) {
    llvm::StringRef Name = FD->getName();
    if (Name == "Acquire_Load" || Name == "Relaxed_Load" ||
        Name == "SeqCst_Load" || Name == "Release_Store" ||
        Name == "Relaxed_Store" || Name == "SeqCst_Store" ||
        Name == "Relaxed_CompareAndSwap" ||
        Name == "AcquireRelease_CompareAndSwap" ||
        Name == "Release_CompareAndSwap" || Name == "SeqCst_CompareAndSwap" ||
        Name == "Relaxed_AtomicExchange" || Name == "SeqCst_AtomicExchange" ||
        Name == "Relaxed_FetchOr" || Name == "Relaxed_AtomicIncrement" ||
        Name == "AsAtomicPtr" || Name == "Relaxed_Memcpy" ||
        Name == "Relaxed_Memmove") {
      return true;
    }
  }
  return TraverseAnyFunctionHelper(FD, [this](clang::FunctionDecl* D) {
    return clang::RecursiveASTVisitor<DfgVisitor>::TraverseFunctionDecl(D);
  });
}

bool DfgVisitor::TraverseCXXMethodDecl(clang::CXXMethodDecl* MD) {
  if (IsReadOnlyRootGetter(MD)) {
    return true;
  }
  if (auto* ParentRD = MD->getParent()) {
    llvm::StringRef ParentName = ParentRD->getName();
    if (ParentName == "BitField" || ParentName == "BitSetComputer" ||
        ParentName == "AsAtomicImpl" || ParentName == "AsAtomicPointerImpl" ||
        ParentName == "cast_helper") {
      return true;
    }
  }
  if (MD->isThisDeclarationADefinition()) {
    // 1. Getter backing field discovery
    if (const clang::FieldDecl* FD = FindBackingField(MD)) {
      GetterToField[MD->getCanonicalDecl()] = FD;
    }
  }

  return TraverseAnyFunctionHelper(MD, [this](clang::CXXMethodDecl* D) {
    return clang::RecursiveASTVisitor<DfgVisitor>::TraverseCXXMethodDecl(D);
  });
}

bool DfgVisitor::TraverseCXXConstructorDecl(clang::CXXConstructorDecl* CD) {
  return TraverseAnyFunctionHelper(CD, [this](clang::CXXConstructorDecl* D) {
    if (D->isThisDeclarationADefinition()) {
      for (auto* Init : D->inits()) {
        if (Init->isDelegatingInitializer() && Init->getInit()) {
          clang::Expr* DelegatedExpr = Init->getInit();
          Node::Id DelegatedId = GetNodeIdForExpr(DelegatedExpr);
          clang::CXXConstructorDecl* CanonicalCD = D->getCanonicalDecl();
          SourceLocation FuncLoc = GetSourceLocation(CanonicalCD->getLocation());
          if (DelegatedId != 0 && !FuncLoc.File.empty()) {
            llvm::StringRef UsrReturnId = GetUSRForDecl(CanonicalCD);
            Node::Id ReturnNodeId =
                !UsrReturnId.empty()
                    ? HashFnv1a64(UsrReturnId.str() + "/return")
                    : GenerateId(CanonicalCD->getNameAsString() + "_return",
                                 FuncLoc);
            Node N;
            N.ID = ReturnNodeId;
            N.Kind = NodeKind::Expression;
            N.Name = CanonicalCD->getNameAsString() + "_return";
            N.Type = CanonicalCD->getNameAsString();
            N.Loc = FuncLoc;
            EmitNode(std::move(N));

            if (ReturnNodeId != 0 && DelegatedId != ReturnNodeId) {
              Edge E;
              E.From = DelegatedId;
              E.To = ReturnNodeId;
              E.Kind = EdgeKind::Return;
              E.Loc = GetSourceLocation(Init->getSourceLocation());
              EmitEdge(std::move(E));
            }
          }
          continue;
        }
        if (Init->isAnyMemberInitializer() && Init->getInit()) {
          clang::FieldDecl* Member = Init->getAnyMember();
          clang::Expr* InitExpr = Init->getInit();
          if (Member && InitExpr) {
            if (!ContextStack.empty()) {
              std::vector<size_t> Writes = CollectBitFieldWrites(InitExpr);
              if (!Writes.empty()) {
                auto& Ctx = ContextStack.back();
                for (size_t idx : Writes) {
                  FlushBitFieldWriteToField(Ctx.PendingWrites[idx], Member);
                }
                continue;
              }
            }
            Node::Id LHS = GetOrCreateNodeForDecl(Member);
            Node::Id RHS = GetNodeIdForExpr(InitExpr);
            if (RHS != 0) {
              FlushRawWordWriteToRecordBitFields(
                  InitExpr, RHS, Member, EdgeKind::Initialization,
                  GetSourceLocation(Init->getSourceLocation()));
            }
            if (LHS != 0 && RHS != 0) {
              Edge E;
              E.From = RHS;
              E.To = LHS;
              E.Kind = EdgeKind::Initialization;
              E.Loc = GetSourceLocation(Init->getSourceLocation());
              EmitEdge(std::move(E));
            }
          }
        }
      }
    }
    return clang::RecursiveASTVisitor<DfgVisitor>::TraverseCXXConstructorDecl(
        D);
  });
}

bool DfgVisitor::TraverseCXXDestructorDecl(clang::CXXDestructorDecl* DD) {
  return TraverseAnyFunctionHelper(DD, [this](clang::CXXDestructorDecl* D) {
    return clang::RecursiveASTVisitor<DfgVisitor>::TraverseCXXDestructorDecl(D);
  });
}

bool DfgVisitor::TraverseCXXConversionDecl(clang::CXXConversionDecl* CD) {
  return TraverseAnyFunctionHelper(CD, [this](clang::CXXConversionDecl* D) {
    return clang::RecursiveASTVisitor<DfgVisitor>::TraverseCXXConversionDecl(D);
  });
}

bool DfgVisitor::VisitInitListExpr(clang::InitListExpr* ILE) {
  if (!ILE) return true;
  if (ILE->isSyntacticForm() && ILE->getSemanticForm()) {
    return true;
  }
  SourceLocation Loc = GetSourceLocation(ILE->getBeginLoc());
  if (Loc.File.empty()) return true;

  if (auto* RD = ILE->getType()->getAsCXXRecordDecl()) {
    if (RD->hasDefinition()) {
      RD = RD->getDefinition();
      if (RD->isUnion()) {
        if (clang::FieldDecl* UnionField = ILE->getInitializedFieldInUnion()) {
          if (ILE->getNumInits() > 0 && ILE->getInit(0) &&
              !llvm::isa<clang::ImplicitValueInitExpr>(ILE->getInit(0))) {
            Node::Id FieldId = GetOrCreateNodeForDecl(UnionField);
            Node::Id InitId = GetNodeIdForExpr(ILE->getInit(0));
            if (FieldId != 0 && InitId != 0 && FieldId != InitId) {
              Edge E;
              E.From = InitId;
              E.To = FieldId;
              E.Kind = EdgeKind::Initialization;
              E.Loc = GetSourceLocation(ILE->getInit(0)->getBeginLoc());
              EmitEdge(std::move(E));
            }
          }
        }
        return true;
      }
      unsigned idx = RD->getNumBases();
      for (clang::FieldDecl* Field : RD->fields()) {
        if (idx >= ILE->getNumInits()) break;
        clang::Expr* Init = ILE->getInit(idx++);
        if (!Init || llvm::isa<clang::ImplicitValueInitExpr>(Init)) continue;
        Node::Id FieldId = GetOrCreateNodeForDecl(Field);
        Node::Id InitId = GetNodeIdForExpr(Init);
        if (FieldId != 0 && InitId != 0 && FieldId != InitId) {
          Edge E;
          E.From = InitId;
          E.To = FieldId;
          E.Kind = EdgeKind::Initialization;
          E.Loc = GetSourceLocation(Init->getBeginLoc());
          EmitEdge(std::move(E));
        }
      }
    }
  }
  return true;
}

bool DfgVisitor::VisitCXXParenListInitExpr(
    clang::CXXParenListInitExpr* PLIE) {
  if (!PLIE) return true;
  SourceLocation Loc = GetSourceLocation(PLIE->getBeginLoc());
  if (Loc.File.empty()) return true;

  if (auto* RD = PLIE->getType()->getAsCXXRecordDecl()) {
    if (RD->hasDefinition() && !RD->isUnion()) {
      RD = RD->getDefinition();
      auto Inits = PLIE->getInitExprs();
      unsigned idx = RD->getNumBases();
      for (clang::FieldDecl* Field : RD->fields()) {
        if (idx >= Inits.size()) break;
        clang::Expr* Init = Inits[idx++];
        if (!Init || llvm::isa<clang::ImplicitValueInitExpr>(Init)) continue;
        Node::Id FieldId = GetOrCreateNodeForDecl(Field);
        Node::Id InitId = GetNodeIdForExpr(Init);
        if (FieldId != 0 && InitId != 0 && FieldId != InitId) {
          Edge E;
          E.From = InitId;
          E.To = FieldId;
          E.Kind = EdgeKind::Initialization;
          E.Loc = GetSourceLocation(Init->getBeginLoc());
          EmitEdge(std::move(E));
        }
      }
    }
  }
  return true;
}

bool DfgVisitor::TraverseCXXForRangeStmt(clang::CXXForRangeStmt* FRS) {
  if (!FRS) return true;
  if (FRS->getInit() && !TraverseStmt(FRS->getInit())) return false;
  if (FRS->getRangeInit() && !TraverseStmt(FRS->getRangeInit())) return false;
  if (!VisitCXXForRangeStmt(FRS)) return false;
  if (FRS->getBody() && !TraverseStmt(FRS->getBody())) return false;
  return true;
}

bool DfgVisitor::VisitCXXForRangeStmt(clang::CXXForRangeStmt* FRS) {
  if (!FRS) return true;
  clang::VarDecl* LoopVar = FRS->getLoopVariable();
  clang::Expr* RangeInit = FRS->getRangeInit();
  if (!LoopVar || !RangeInit) return true;

  SourceLocation Loc = GetSourceLocation(LoopVar->getLocation());
  if (Loc.File.empty()) return true;

  Node::Id RHS = GetNodeIdForExpr(RangeInit);
  if (auto* DD = llvm::dyn_cast<clang::DecompositionDecl>(LoopVar)) {
    for (clang::BindingDecl* BD : DD->bindings()) {
      Node::Id BDId = GetOrCreateNodeForDecl(BD);
      if (BDId != 0 && RHS != 0 && BDId != RHS) {
        Edge E;
        E.From = RHS;
        E.To = BDId;
        E.Kind = EdgeKind::Initialization;
        E.Loc = GetSourceLocation(BD->getLocation());
        EmitEdge(std::move(E));
      }
    }
    return true;
  }

  Node::Id LHS = GetOrCreateNodeForDecl(LoopVar);
  if (LHS != 0 && RHS != 0 && LHS != RHS) {
    Edge E;
    E.From = RHS;
    E.To = LHS;
    E.Kind = EdgeKind::Initialization;
    E.Loc = Loc;
    EmitEdge(std::move(E));
  }
  return true;
}

bool DfgVisitor::VisitLambdaExpr(clang::LambdaExpr* LE) {
  if (!LE) return true;
  SourceLocation Loc = GetSourceLocation(LE->getBeginLoc());
  if (Loc.File.empty()) return true;

  clang::CXXRecordDecl* LambdaClass = LE->getLambdaClass();
  if (!LambdaClass) return true;

  llvm::DenseMap<const clang::ValueDecl*, clang::FieldDecl*> CaptureFields;
  clang::FieldDecl* ThisCaptureField = nullptr;
  LambdaClass->getCaptureFields(CaptureFields, ThisCaptureField);

  auto Captures = LE->captures();
  auto CaptureInits = LE->capture_inits();
  auto CapIt = Captures.begin();
  auto InitIt = CaptureInits.begin();
  for (; CapIt != Captures.end() && InitIt != CaptureInits.end();
       ++CapIt, ++InitIt) {
    clang::Expr* InitExpr = *InitIt;
    if (!InitExpr || !CapIt->capturesVariable()) continue;
    clang::ValueDecl* CapturedVar = CapIt->getCapturedVar();
    if (!CapturedVar) continue;

    Node::Id InitId = GetNodeIdForExpr(InitExpr);
    Node::Id VarId = GetOrCreateNodeForDecl(CapturedVar);
    if (InitId != 0 && VarId != 0 && InitId != VarId) {
      Edge E;
      E.From = InitId;
      E.To = VarId;
      E.Kind = EdgeKind::Initialization;
      E.Loc = GetSourceLocation(CapIt->getLocation());
      EmitEdge(std::move(E));
    }
    if (clang::FieldDecl* CapField = CaptureFields.lookup(CapturedVar);
        CapField && !CapField->getName().empty()) {
      Node::Id FieldId = GetOrCreateNodeForDecl(CapField);
      if (InitId != 0 && FieldId != 0 && InitId != FieldId) {
        Edge E;
        E.From = InitId;
        E.To = FieldId;
        E.Kind = EdgeKind::Initialization;
        E.Loc = GetSourceLocation(CapIt->getLocation());
        EmitEdge(std::move(E));
      }
    }
  }
  return true;
}

bool DfgVisitor::VisitReturnStmt(clang::ReturnStmt* RS) {
  if (!RS->getRetValue() || !CurrentFunction) return true;

  SourceLocation RetLoc = GetSourceLocation(RS->getReturnLoc());
  if (RetLoc.File.empty()) return true;

  clang::Expr* PeeledRet = PeelExpr(RS->getRetValue());
  if (!PeeledRet) return true;
  if (auto* DRE = llvm::dyn_cast<clang::DeclRefExpr>(PeeledRet)) {
    if (auto* VD = DRE->getDecl()) {
      llvm::StringRef Name = VD->getName();
      if (Name == "kNullMaybe" || Name == "kNullMaybeHandle") {
        return true;
      }
    }
  } else if (auto* CE = llvm::dyn_cast<clang::CallExpr>(PeeledRet)) {
    if (auto* CalleeFD = CE->getDirectCallee()) {
      llvm::StringRef Name = CalleeFD->getName();
      if (Name == "Nothing" || Name == "Failure" || Name == "exception" ||
          Name == "Throw" || Name == "ReThrow" || Name == "ThrowAt" ||
          Name == "ThrowIllegalOperation" || Name == "TerminateExecution") {
        return true;
      }
    }
  }

  Node::Id RHS = GetNodeIdForExpr(RS->getRetValue());

  clang::FunctionDecl* CanonicalFD =
      ResolveHeaderCanonicalDecl(CurrentFunction);
  SourceLocation FuncLoc = GetSourceLocation(CanonicalFD->getLocation());
  if (FuncLoc.File.empty()) return true;
  llvm::StringRef UsrReturnId = GetUSRForDecl(CurrentFunction);
  Node::Id ReturnNodeId;
  if (!UsrReturnId.empty()) {
    ReturnNodeId = HashFnv1a64(UsrReturnId.str() + "/return");
  } else {
    ReturnNodeId =
        GenerateId(CanonicalFD->getNameAsString() + "_return", FuncLoc);
  }

  Node N;
  N.ID = ReturnNodeId;
  N.Kind = NodeKind::Expression;
  N.Name = CanonicalFD->getNameAsString() + "_return";
  N.Type = GetTypeString(CanonicalFD->getReturnType()).str();
  N.Loc = FuncLoc;
  EmitNode(std::move(N));

  if (RHS != 0 && ReturnNodeId != 0) {
    Edge E;
    E.From = RHS;
    E.To = ReturnNodeId;
    E.Kind = EdgeKind::Return;
    E.Loc = GetSourceLocation(RS->getReturnLoc());
    EmitEdge(std::move(E));
  }

  return true;
}

bool DfgVisitor::VisitConditionalOperator(clang::ConditionalOperator* CO) {
  SourceLocation Loc = GetSourceLocation(CO->getExprLoc());
  if (Loc.File.empty()) return true;

  Node::Id CondId = GetNodeIdForExpr(CO);
  clang::Expr* TrueExpr = CO->getLHS() ? CO->getLHS() : CO->getCond();
  Node::Id TrueId = GetNodeIdForExpr(TrueExpr);
  Node::Id FalseId = GetNodeIdForExpr(CO->getRHS());

  if (CondId != 0) {
    if (TrueId != 0) {
      Edge E;
      E.From = TrueId;
      E.To = CondId;
      E.Kind = EdgeKind::Assignment;
      E.Loc = GetSourceLocation(TrueExpr->getBeginLoc());
      EmitEdge(std::move(E));
    }
    if (FalseId != 0) {
      Edge E;
      E.From = FalseId;
      E.To = CondId;
      E.Kind = EdgeKind::Assignment;
      E.Loc = GetSourceLocation(CO->getRHS()->getBeginLoc());
      EmitEdge(std::move(E));
    }
  }
  return true;
}

bool DfgVisitor::VisitBinaryConditionalOperator(
    clang::BinaryConditionalOperator* BCO) {
  SourceLocation Loc = GetSourceLocation(BCO->getExprLoc());
  if (Loc.File.empty()) return true;

  Node::Id CondId = GetNodeIdForExpr(BCO);
  clang::Expr* CommonExpr = BCO->getCommon();
  Node::Id TrueId = GetNodeIdForExpr(CommonExpr);
  Node::Id FalseId = GetNodeIdForExpr(BCO->getFalseExpr());

  if (CondId != 0) {
    if (TrueId != 0) {
      Edge E;
      E.From = TrueId;
      E.To = CondId;
      E.Kind = EdgeKind::Assignment;
      E.Loc = GetSourceLocation(CommonExpr->getBeginLoc());
      EmitEdge(std::move(E));
    }
    if (FalseId != 0) {
      Edge E;
      E.From = FalseId;
      E.To = CondId;
      E.Kind = EdgeKind::Assignment;
      E.Loc = GetSourceLocation(BCO->getFalseExpr()->getBeginLoc());
      EmitEdge(std::move(E));
    }
  }
  return true;
}

static bool IsAllowedGetterTagParam(const clang::ParmVarDecl* PVD) {
  if (!PVD) return false;
  clang::QualType T = PVD->getType().getNonReferenceType();
  std::string TypeStr = T.getAsString();
  if (TypeStr.find("Tag") != std::string::npos ||
      TypeStr.find("IsolateForSandbox") != std::string::npos ||
      TypeStr.find("PtrComprCageBase") != std::string::npos) {
    return true;
  }
  if (auto* RD = T->getAsCXXRecordDecl()) {
    if (RD->hasDefinition() && RD->isEmpty()) {
      return true;
    }
  }
  return false;
}

static bool HasOnlyAllowedGetterParams(const clang::CXXMethodDecl* MD) {
  if (!MD) return false;
  if (MD->param_size() == 0) return true;
  if (MD->param_size() == 1) return IsAllowedGetterTagParam(MD->getParamDecl(0));
  return false;
}

// Recursive helper to dynamically discover backing field by analyzing getter
// return expressions
class BackingFieldVisitor
    : public clang::RecursiveASTVisitor<BackingFieldVisitor> {
 public:
  clang::ASTContext* Context;
  clang::CXXRecordDecl* ParentClass;
  const clang::FieldDecl* FoundField = nullptr;
  llvm::SmallPtrSet<const clang::CXXMethodDecl*, 4> VisitedMethods;
  bool InReturnExpr = false;

  BackingFieldVisitor(
      clang::ASTContext* Context, clang::CXXRecordDecl* ParentClass,
      llvm::SmallPtrSet<const clang::CXXMethodDecl*, 4> Visited = {})
      : Context(Context), ParentClass(ParentClass), VisitedMethods(Visited) {}

  bool TraverseReturnStmt(clang::ReturnStmt* RS) {
    if (!RS || !RS->getRetValue()) return true;
    bool OldInReturn = InReturnExpr;
    InReturnExpr = true;
    bool Res =
        clang::RecursiveASTVisitor<BackingFieldVisitor>::TraverseStmt(
            RS->getRetValue());
    InReturnExpr = OldInReturn;
    return Res;
  }

  bool VisitDeclRefExpr(clang::DeclRefExpr* DRE) {
    if (!InReturnExpr) return true;
    if (auto* VD = clang::dyn_cast<clang::VarDecl>(DRE->getDecl())) {
      if (VD->isLocalVarDecl() && VD->hasInit()) {
        return TraverseStmt(VD->getInit());
      }
    }
    return true;
  }

  bool VisitMemberExpr(clang::MemberExpr* ME) {
    if (!InReturnExpr) return true;
    if (auto* FD = clang::dyn_cast<clang::FieldDecl>(ME->getMemberDecl())) {
      if (ME->getBase()) {
        clang::Expr* Base = ME->getBase()->IgnoreParenImpCasts();
        if (llvm::isa<clang::CXXThisExpr>(Base) ||
            (llvm::isa<clang::ImplicitCastExpr>(Base) &&
             llvm::isa<clang::CXXThisExpr>(Base->IgnoreParenImpCasts()))) {
          FoundField = FD;
          return false;  // Stop traversal
        }
      }
    }
    return true;
  }

  bool TraverseCallExpr(clang::CallExpr* CE) {
    if (!InReturnExpr || !CE) return true;
    if (auto* FD = CE->getDirectCallee()) {
      // Never treat BitField::decode / encode / update as raw field getters!
      if (auto* CTSD =
              llvm::dyn_cast_or_null<clang::ClassTemplateSpecializationDecl>(
                  FD->getDeclContext())) {
        if (CTSD->getName() == "BitField" ||
            CTSD->getName() == "BitSetComputer") {
          return true;
        }
      }
      std::string Name = FD->getNameAsString();
      // Allow transparent identity/cast calls (e.g. Cast(this->field_), handle(this->field_, isolate()))
      if (WrapperConfigManager::Get().IsIdentityFunction(Name) &&
          CE->getNumArgs() >= 1) {
        return TraverseStmt(CE->getArg(0));
      }
      // Allow 0-arg (or load-tag-arg) helper getters on `this` (e.g. return flags(kRelaxedLoad); or field_.load())
      if (auto* CXXMD = clang::dyn_cast<clang::CXXMethodDecl>(FD->getCanonicalDecl())) {
        if (!CXXMD->isStatic()) {
          if (auto* MCE = llvm::dyn_cast<clang::CXXMemberCallExpr>(CE)) {
            if (clang::Expr* Receiver = MCE->getImplicitObjectArgument()) {
              clang::Expr* Base = Receiver->IgnoreParenImpCasts();
              if (llvm::isa<clang::CXXThisExpr>(Base)) {
                if (HasOnlyAllowedGetterParams(CXXMD) &&
                    CXXMD->getParent() == ParentClass && CXXMD->hasBody() &&
                    VisitedMethods.count(CXXMD) == 0) {
                  VisitedMethods.insert(CXXMD);
                  BackingFieldVisitor SubVisitor(Context, ParentClass, VisitedMethods);
                  SubVisitor.TraverseStmt(CXXMD->getBody());
                  if (SubVisitor.FoundField) {
                    FoundField = SubVisitor.FoundField;
                    return false;
                  }
                }
                return true;
              }
              // Allow chained getter / wrapper load on a member field (e.g. isolate_data_.roots() or flags_.load(std::memory_order_relaxed))
              return TraverseStmt(Base);
            }
          }
        }
      }
    }
    // Do NOT traverse arguments of general function calls (e.g. DescriptorArray::Allocate(isolate_, ...))
    return true;
  }

  bool TraverseCXXMemberCallExpr(clang::CXXMemberCallExpr* CE) {
    return TraverseCallExpr(CE);
  }

  bool TraverseCXXOperatorCallExpr(clang::CXXOperatorCallExpr* CE) {
    return TraverseCallExpr(CE);
  }
};

bool DfgVisitor::IsReadOnlyRootGetter(const clang::CXXMethodDecl* MD) {
  if (!MD) return false;
  const clang::CXXRecordDecl* ParentRD = MD->getParent();
  if (!ParentRD) return false;

  llvm::StringRef ParentName = ParentRD->getName();
  if (ParentName == "ReadOnlyRoots" || ParentName == "EarlyReadOnlyRoots" ||
      ParentName == "RootsTable") {
    return !MD->getReturnType()->isVoidType();
  }

  if (ParentName == "FactoryBase" || ParentName == "Factory" ||
      ParentName == "LocalFactory") {
    if (MD->getReturnType()->isVoidType()) return false;
    llvm::StringRef MethodName = MD->getName();
    if (MethodName == "ToBoolean" || MethodName == "boolean_value" ||
        MethodName == "single_character_string" ||
        MethodName == "empty_string" || MethodName == "read_only_roots") {
      return true;
    }
    if (MD->getNumParams() != 0) return false;

    if (!SearchedReadOnlyRootsRD) {
      SearchedReadOnlyRootsRD = true;
      if (const clang::DeclContext* DC = MD->getEnclosingNamespaceContext()) {
        clang::IdentifierInfo& II = Context->Idents.get("ReadOnlyRoots");
        for (const clang::NamedDecl* ND : DC->lookup(&II)) {
          if (const auto* RD = llvm::dyn_cast<clang::CXXRecordDecl>(ND)) {
            if (RD->hasDefinition()) {
              CachedReadOnlyRootsRD = RD->getDefinition();
              break;
            }
          }
        }
      }
    }
    if (CachedReadOnlyRootsRD) {
      if (!CachedReadOnlyRootsRD->lookup(MD->getDeclName()).empty()) {
        return true;
      }
    }
  }
  return false;
}

const clang::FieldDecl* DfgVisitor::FindBackingField(clang::CXXMethodDecl* MD) {
  if (!MD || !MD->hasBody()) return nullptr;
  if (IsReadOnlyRootGetter(MD)) return nullptr;

  // Never treat internal fields of configured wrapper classes (e.g. TaggedMember::ptr)
  // as backing fields; wrapper classes are handled by WrapperConfig load/store rules.
  if (MD->getParent() &&
      WrapperConfigManager::Get().IsWrapperClass(
          MD->getParent()->getNameAsString())) {
    return nullptr;
  }

  // Candidate check: Non-static instance method, 0 or 1 load-tag parameters, non-void return
  if (MD->isStatic() || !HasOnlyAllowedGetterParams(MD) ||
      MD->getReturnType()->isVoidType()) {
    return nullptr;
  }

  // A backing-field getter body must be short (at most 2 top-level statements,
  // e.g. an optional DCHECK followed by a ReturnStmt).
  if (auto* CS = llvm::dyn_cast<clang::CompoundStmt>(MD->getBody())) {
    if (CS->size() == 0 || CS->size() > 2) {
      return nullptr;
    }
  }

  BackingFieldVisitor Visitor(Context, MD->getParent());
  Visitor.TraverseStmt(MD->getBody());
  return Visitor.FoundField;
}

namespace {
class ParamReferenceChecker
    : public clang::RecursiveASTVisitor<ParamReferenceChecker> {
 public:
  const clang::CXXMethodDecl* SetterMD;
  bool Found = false;

  explicit ParamReferenceChecker(const clang::CXXMethodDecl* SetterMD)
      : SetterMD(SetterMD) {}

  bool VisitDeclRefExpr(clang::DeclRefExpr* DRE) {
    if (!DRE || !SetterMD) return true;
    if (auto* PVD = llvm::dyn_cast<clang::ParmVarDecl>(DRE->getDecl())) {
      for (const clang::ParmVarDecl* Param : SetterMD->parameters()) {
        if (Param->getCanonicalDecl() == PVD->getCanonicalDecl()) {
          Found = true;
          return false;
        }
      }
    }
    return true;
  }
};

bool ExprReferencesSetterParam(clang::Expr* E,
                               const clang::CXXMethodDecl* SetterMD) {
  if (!E || !SetterMD) return false;
  ParamReferenceChecker Checker(SetterMD);
  Checker.TraverseStmt(E);
  return Checker.Found;
}
}  // namespace

// Recursive helper to discover the backing field modified by a setter method's
// AST body (e.g., `this->field_ = val`, `this->field_.store(this, val)`, or
// delegating `this->set_raw_flags(BitField::update(..., val))`), without
// relying on method or field names and ignoring leading DCHECK reads.
class SetterFieldVisitor
    : public clang::RecursiveASTVisitor<SetterFieldVisitor> {
 public:
  DfgVisitor& Owner;
  const clang::CXXMethodDecl* SetterMD;
  const clang::FieldDecl* FoundField = nullptr;

  SetterFieldVisitor(DfgVisitor& Owner, const clang::CXXMethodDecl* SetterMD)
      : Owner(Owner), SetterMD(SetterMD) {}

  bool VisitBinaryOperator(clang::BinaryOperator* BO) {
    if (!BO || !BO->isAssignmentOp()) return true;
    if (!ExprReferencesSetterParam(BO->getRHS(), SetterMD)) return true;
    if (clang::Expr* LHS = BO->getLHS()) {
      if (auto* ME =
              llvm::dyn_cast<clang::MemberExpr>(LHS->IgnoreParenImpCasts())) {
        if (auto* FD = llvm::dyn_cast<clang::FieldDecl>(ME->getMemberDecl())) {
          if (ME->getBase() &&
              llvm::isa<clang::CXXThisExpr>(
                  ME->getBase()->IgnoreParenImpCasts())) {
            FoundField = FD->getCanonicalDecl();
            return false;
          }
        }
      }
    }
    return true;
  }

  bool VisitCallExpr(clang::CallExpr* CE) {
    if (!CE) return true;
    clang::Expr* Receiver = nullptr;
    unsigned ExplicitArgStart = 0;
    if (auto* MCE = llvm::dyn_cast<clang::CXXMemberCallExpr>(CE)) {
      Receiver = MCE->getImplicitObjectArgument();
    } else if (auto* OCE = llvm::dyn_cast<clang::CXXOperatorCallExpr>(CE)) {
      if (OCE->getNumArgs() > 0) {
        Receiver = OCE->getArg(0);
        ExplicitArgStart = 1;
      }
    }
    if (!Receiver) {
      if (auto* CalleeFD = CE->getDirectCallee()) {
        std::string FnName = CalleeFD->getNameAsString();
        if (CE->getNumArgs() >= 2 &&
            (FnName == "Release_Store" || FnName == "Relaxed_Store" ||
             FnName == "SeqCst_Store" || FnName == "Release_CompareAndSwap" ||
             FnName == "Relaxed_CompareAndSwap" ||
             FnName == "AcquireRelease_CompareAndSwap" ||
             FnName == "SeqCst_CompareAndSwap" || FnName == "SeqCst_Swap" ||
             FnName == "Relaxed_Swap")) {
          if (ExprReferencesSetterParam(CE->getArg(CE->getNumArgs() - 1),
                                        SetterMD)) {
            if (const clang::FieldDecl* FD =
                    Owner.ExtractBackingFieldFromExpr(CE->getArg(0))) {
              FoundField = FD;
              return false;
            }
          }
        }
      }
      return true;
    }

    bool HasParamArg = false;
    for (unsigned i = ExplicitArgStart; i < CE->getNumArgs(); ++i) {
      if (ExprReferencesSetterParam(CE->getArg(i), SetterMD)) {
        HasParamArg = true;
        break;
      }
    }
    if (!HasParamArg) return true;

    clang::Expr* Base = Receiver->IgnoreParenImpCasts();
    if (auto* ME = llvm::dyn_cast<clang::MemberExpr>(Base)) {
      if (auto* FD = llvm::dyn_cast<clang::FieldDecl>(ME->getMemberDecl())) {
        if (ME->getBase() &&
            llvm::isa<clang::CXXThisExpr>(
                ME->getBase()->IgnoreParenImpCasts())) {
          bool IsStore = false;
          if (auto* OCE = llvm::dyn_cast<clang::CXXOperatorCallExpr>(CE)) {
            if (OCE->isAssignmentOp()) IsStore = true;
          }
          if (!IsStore) {
            if (auto* CalleeFD = CE->getDirectCallee()) {
              std::string MethodName = CalleeFD->getNameAsString();
              if (MethodName == "store" || MethodName == "Relaxed_Store" ||
                  MethodName == "Release_Store" ||
                  MethodName == "SeqCst_Store" || MethodName == "set") {
                IsStore = true;
              }
            }
          }
          if (IsStore) {
            FoundField = FD->getCanonicalDecl();
            return false;
          }
        }
      }
    } else if (llvm::isa<clang::CXXThisExpr>(Base)) {
      if (auto* CalleeMD =
              llvm::dyn_cast_or_null<clang::CXXMethodDecl>(CE->getDirectCallee())) {
        if (const clang::FieldDecl* Delegated =
                Owner.ResolveSetterBackingField(CalleeMD)) {
          FoundField = Delegated;
          return false;
        }
      }
    }
    return true;
  }
};

const clang::FieldDecl* DfgVisitor::FindSetterBackingField(
    clang::CXXMethodDecl* MD) {
  if (!MD || !MD->hasBody() || MD->isStatic() || MD->param_size() == 0) {
    return nullptr;
  }
  if (MD->getParent() &&
      WrapperConfigManager::Get().IsWrapperClass(
          MD->getParent()->getNameAsString())) {
    return nullptr;
  }
  if (auto* CS = llvm::dyn_cast<clang::CompoundStmt>(MD->getBody())) {
    if (CS->size() == 0 || CS->size() > 6) {
      return nullptr;
    }
  }
  SetterFieldVisitor Visitor(*this, MD);
  Visitor.TraverseStmt(MD->getBody());
  return Visitor.FoundField;
}

const clang::FieldDecl* DfgVisitor::ResolveBackingField(
    const clang::CXXMethodDecl* MD) {
  if (!MD || MD->isStatic()) return nullptr;
  const clang::CXXMethodDecl* CanonicalMD = MD->getCanonicalDecl();
  auto It = GetterToField.find(CanonicalMD);
  if (It != GetterToField.end()) {
    return It->second;
  }
  const clang::FieldDecl* Field = nullptr;
  if (const clang::FunctionDecl* DefFD = CanonicalMD->getDefinition()) {
    if (auto* DefMD = llvm::dyn_cast<clang::CXXMethodDecl>(
            const_cast<clang::FunctionDecl*>(DefFD))) {
      Field = FindBackingField(DefMD);
    }
  }
  GetterToField[CanonicalMD] = Field;
  return Field;
}

const clang::FieldDecl* DfgVisitor::ResolveSetterBackingField(
    const clang::CXXMethodDecl* MD) {
  if (!MD || MD->isStatic()) return nullptr;
  const clang::CXXMethodDecl* CanonicalMD = MD->getCanonicalDecl();
  auto It = SetterToField.find(CanonicalMD);
  if (It != SetterToField.end()) {
    return It->second;
  }
  // Insert nullptr sentinel before recursion to prevent cycles in delegating setters
  SetterToField[CanonicalMD] = nullptr;
  const clang::FieldDecl* Field = nullptr;
  if (const clang::FunctionDecl* DefFD = CanonicalMD->getDefinition()) {
    if (auto* DefMD = llvm::dyn_cast<clang::CXXMethodDecl>(
            const_cast<clang::FunctionDecl*>(DefFD))) {
      Field = FindSetterBackingField(DefMD);
    }
  }
  SetterToField[CanonicalMD] = Field;
  return Field;
}

static const clang::CXXRecordDecl* ExtractRecordFromHostExpr(
    clang::Expr* HostExpr, clang::FunctionDecl* CurrentFunction) {
  if (HostExpr) {
    clang::QualType T = HostExpr->IgnoreParenImpCasts()->getType();
    T = T.getNonReferenceType();
    if (T->isPointerType()) {
      T = T->getPointeeType();
    }
    if (auto* CTSD = llvm::dyn_cast_or_null<clang::ClassTemplateSpecializationDecl>(
            T->getAsCXXRecordDecl())) {
      if (CTSD->getName() == "Tagged" || CTSD->getName() == "DirectHandle" ||
          CTSD->getName() == "Handle" || CTSD->getName() == "IndirectHandle") {
        const auto& Args = CTSD->getTemplateArgs();
        if (Args.size() >= 1 &&
            Args[0].getKind() == clang::TemplateArgument::Type) {
          T = Args[0].getAsType();
        }
      }
    }
    if (const clang::CXXRecordDecl* RD = T->getAsCXXRecordDecl()) {
      if (RD->getName() != "HeapObject" && RD->getName() != "Object" &&
          RD->getName() != "HeapObjectLayout") {
        return RD->getCanonicalDecl();
      }
    }
  }
  if (auto* MD = llvm::dyn_cast_or_null<clang::CXXMethodDecl>(CurrentFunction)) {
    if (MD->getParent()) {
      return MD->getParent()->getCanonicalDecl();
    }
  }
  return nullptr;
}

static const clang::ValueDecl* FindOffsetVarInHierarchy(
    const clang::CXXRecordDecl* RD, int64_t Offset, clang::ASTContext& Ctx,
    const clang::CXXRecordDecl** OutOwnerRD) {
  if (!RD) return nullptr;
  RD = RD->getDefinition();
  if (!RD) return nullptr;

  for (const auto* D : RD->decls()) {
    if (const auto* VD = llvm::dyn_cast<clang::VarDecl>(D)) {
      if (VD->isStaticDataMember() && VD->getType()->isIntegralOrEnumerationType()) {
        if (const clang::Expr* Init = VD->getAnyInitializer()) {
          clang::Expr::EvalResult Eval;
          if (Init->EvaluateAsInt(Eval, Ctx) &&
              Eval.Val.getInt().getSExtValue() == Offset) {
            if (OutOwnerRD) *OutOwnerRD = RD->getCanonicalDecl();
            return VD->getCanonicalDecl();
          }
        }
      }
    } else if (const auto* ED = llvm::dyn_cast<clang::EnumDecl>(D)) {
      for (const auto* ECD : ED->enumerators()) {
        if (ECD->getInitVal().getSExtValue() == Offset) {
          if (OutOwnerRD) *OutOwnerRD = RD->getCanonicalDecl();
          return ECD->getCanonicalDecl();
        }
      }
    }
  }
  for (const auto& Base : RD->bases()) {
    if (const auto* BaseRD = Base.getType()->getAsCXXRecordDecl()) {
      if (const clang::ValueDecl* Found =
              FindOffsetVarInHierarchy(BaseRD, Offset, Ctx, OutOwnerRD)) {
        return Found;
      }
    }
  }
  return nullptr;
}

DfgVisitor::OffsetFieldAccess DfgVisitor::ExtractOffsetFieldAccess(
    clang::CallExpr* CE) {
  OffsetFieldAccess Access;
  if (!CE) return Access;
  clang::FunctionDecl* FD = CE->getDirectCallee();
  if (!FD) return Access;

  Access.CallLoc = CE->getExprLoc();

  // 1. TaggedField<T, kOffset>::load / store / Relaxed_Load / Release_Store / etc.
  if (auto* MD = llvm::dyn_cast<clang::CXXMethodDecl>(FD)) {
    if (MD->isStatic()) {
      if (auto* CTSD =
              llvm::dyn_cast_or_null<clang::ClassTemplateSpecializationDecl>(
                  MD->getParent())) {
        if (CTSD->getName() == "TaggedField") {
          const auto& Args = CTSD->getTemplateArgs();
          if (Args.size() >= 2 &&
              Args[0].getKind() == clang::TemplateArgument::Type &&
              Args[1].getKind() == clang::TemplateArgument::Integral) {
            int64_t Offset = Args[1].getAsIntegral().getSExtValue();
            if (Offset > 0 && CE->getNumArgs() >= 1) {
              llvm::StringRef Op = MD->getName();
              bool IsLoad = (Op == "load" || Op == "Relaxed_Load" ||
                             Op == "Acquire_Load" || Op == "SeqCst_Load" ||
                             Op == "Acquire_Load_No_Unpack" ||
                             Op == "Relaxed_Load_Map_Word" ||
                             Op == "Relaxed_Load_Raw");
              bool IsStore = (Op == "store" || Op == "Relaxed_Store" ||
                              Op == "Release_Store" || Op == "SeqCst_Store" ||
                              Op == "Relaxed_Store_Map_Word" ||
                              Op == "Release_Store_Map_Word" ||
                              Op == "Release_CompareAndSwap" ||
                              Op == "Relaxed_CompareAndSwap" ||
                              Op == "AcquireRelease_CompareAndSwap" ||
                              Op == "SeqCst_CompareAndSwap" ||
                              Op == "SeqCst_Swap" || Op == "Relaxed_Swap");
              if (IsLoad || IsStore) {
                clang::Expr* HostExpr = CE->getArg(0);
                // In 2-arg load(PtrComprCageBase, host) or 3-arg store, check if arg 0 is cage_base
                if (CE->getNumArgs() >= 2) {
                  std::string Arg0Type =
                      CE->getArg(0)->getType().getAsString();
                  if (Arg0Type.find("CageBase") != std::string::npos ||
                      Arg0Type.find("Isolate") != std::string::npos) {
                    HostExpr = CE->getArg(1);
                  }
                }
                const clang::CXXRecordDecl* HolderRD =
                    ExtractRecordFromHostExpr(HostExpr, CurrentFunction);
                if (HolderRD) {
                  Access.IsValid = true;
                  Access.IsStore = IsStore;
                  Access.HolderRD = HolderRD;
                  Access.Offset = Offset;
                  Access.ValueType = Args[0].getAsType();
                  Access.Taint = ClassifyRecordHeapTaint(HolderRD);
                  if (Access.Taint == HeapTaint::None) {
                    Access.Taint = HeapTaint::InSandbox;
                  }
                  if (IsStore && CE->getNumArgs() >= 2) {
                    Access.ValueExpr = CE->getArg(CE->getNumArgs() - 1);
                  }
                  return Access;
                }
              }
            }
          }
        }
      }
    }
  }

  // 2. ReadField<T>(offset) / WriteField<T>(offset, value) and variants
  llvm::StringRef Name = FD->getName();
  bool IsReadField =
      (Name == "ReadField" || Name == "Relaxed_ReadField" ||
       Name == "Acquire_ReadField" || Name == "SeqCst_ReadField" ||
       Name == "ReadSandboxedPointerField" ||
       Name == "ReadTrustedPointerField" ||
       Name == "ReadProtectedPointerField" ||
       Name == "ReadBoundedSizeField" ||
       Name == "ReadExternalPointerField" ||
       Name == "ReadCppHeapPointerField");
  bool IsWriteField =
      (Name == "WriteField" || Name == "Relaxed_WriteField" ||
       Name == "Release_WriteField" || Name == "SeqCst_WriteField" ||
       Name == "WriteSandboxedPointerField" ||
       Name == "WriteTrustedPointerField" ||
       Name == "WriteProtectedPointerField" ||
       Name == "WriteBoundedSizeField" ||
       Name == "WriteExternalPointerField" ||
       Name == "WriteCppHeapPointerField" ||
       Name == "WriteLazilyInitializedExternalPointerField");

  if ((IsReadField || IsWriteField) && CE->getNumArgs() >= 1) {
    unsigned OffsetArgIdx = 0;
    clang::Expr* HostExpr = nullptr;
    if (auto* MCE = llvm::dyn_cast<clang::CXXMemberCallExpr>(CE)) {
      HostExpr = MCE->getImplicitObjectArgument();
    } else if (CE->getNumArgs() >= 2) {
      HostExpr = CE->getArg(0);
      OffsetArgIdx = 1;
    }
    if (OffsetArgIdx < CE->getNumArgs()) {
      clang::Expr* OffsetExpr = CE->getArg(OffsetArgIdx);
      clang::Expr::EvalResult Eval;
      if (OffsetExpr && !OffsetExpr->isValueDependent() &&
          OffsetExpr->EvaluateAsInt(Eval, *Context)) {
        int64_t Offset = Eval.Val.getInt().getSExtValue();
        const clang::CXXRecordDecl* HolderRD =
            ExtractRecordFromHostExpr(HostExpr, CurrentFunction);
        if (Offset > 0 && HolderRD &&
            (!IsWriteField || OffsetArgIdx + 1 < CE->getNumArgs())) {
          unsigned ValArgIdx =
              IsWriteField ? (CE->getNumArgs() - 1) : OffsetArgIdx;
          Access.IsValid = true;
          Access.IsStore = IsWriteField;
          Access.HolderRD = HolderRD;
          Access.Offset = Offset;
          Access.ValueType = IsReadField ? CE->getType()
                                         : CE->getArg(ValArgIdx)->getType();
          if (Name == "ReadTrustedPointerField" ||
              Name == "WriteTrustedPointerField" ||
              Name == "ReadProtectedPointerField" ||
              Name == "WriteProtectedPointerField") {
            Access.Taint = HeapTaint::TrustedSpace;
          } else {
            Access.Taint = ClassifyRecordHeapTaint(HolderRD);
            if (Access.Taint == HeapTaint::None) {
              Access.Taint = HeapTaint::InSandbox;
            }
          }
          if (auto* DRE = llvm::dyn_cast<clang::DeclRefExpr>(
                  OffsetExpr->IgnoreParenImpCasts())) {
            if (llvm::isa<clang::VarDecl>(DRE->getDecl()) ||
                llvm::isa<clang::EnumConstantDecl>(DRE->getDecl())) {
              Access.OffsetVar =
                  llvm::cast<clang::ValueDecl>(DRE->getDecl()->getCanonicalDecl());
            }
          }
          if (IsWriteField) {
            Access.ValueExpr = CE->getArg(ValArgIdx);
          }
          return Access;
        }
      }
    }
  }

  return Access;
}

Node::Id DfgVisitor::GetOrCreateOffsetFieldNode(
    const OffsetFieldAccess& Access) {
  if (!Access.IsValid || !Access.HolderRD || Access.Offset <= 0) return 0;

  const clang::CXXRecordDecl* OwnerRD = Access.HolderRD->getCanonicalDecl();
  const clang::ValueDecl* OffsetVar = Access.OffsetVar;
  if (!OffsetVar) {
    OffsetVar = FindOffsetVarInHierarchy(OwnerRD, Access.Offset, *Context,
                                         &OwnerRD);
  } else if (const clang::DeclContext* DC = OffsetVar->getDeclContext()) {
    if (const auto* VarRD = llvm::dyn_cast_or_null<clang::CXXRecordDecl>(
            DC->getOuterLexicalRecordContext())) {
      OwnerRD = VarRD->getCanonicalDecl();
    }
  }

  std::string OwnerUSR = GetUSRForDecl(OwnerRD).str();
  if (OwnerUSR.empty()) return 0;

  std::string KeyStr = OwnerUSR + "/offset_" + std::to_string(Access.Offset);
  Node::Id ID = HashFnv1a64(KeyStr);

  Node N;
  N.ID = ID;
  N.Kind = NodeKind::Field;
  if (OffsetVar) {
    N.Name = OffsetVar->getNameAsString();
    N.Loc = GetSourceLocation(OffsetVar->getLocation());
  } else {
    N.Name = OwnerRD->getNameAsString() + "::@offset_" +
             std::to_string(Access.Offset);
    N.Loc = GetSourceLocation(OwnerRD->getLocation());
  }
  if (N.Loc.File.empty()) {
    N.Loc = GetSourceLocation(Access.CallLoc);
  }
  HeapTaint Taint = Access.Taint;
  if (Taint != HeapTaint::TrustedSpace) {
    HeapTaint OwnerTaint = ClassifyRecordHeapTaint(OwnerRD);
    if (OwnerTaint != HeapTaint::None) {
      Taint = OwnerTaint;
    }
  }
  if (Taint == HeapTaint::None) {
    Taint = HeapTaint::InSandbox;
  }
  N.Type = GetTypeString(Access.ValueType).str() + HeapTaintTypeSuffix(Taint);
  EmitNode(std::move(N));
  return ID;
}

DfgVisitor::BitFieldInfo DfgVisitor::ExtractBitFieldInfo(clang::CallExpr* CE) {
  BitFieldInfo Info;
  if (!CE) return Info;
  clang::FunctionDecl* FD = CE->getDirectCallee();
  auto* MD = llvm::dyn_cast_or_null<clang::CXXMethodDecl>(FD);
  if (!MD || !MD->isStatic()) return Info;

  llvm::StringRef Op = MD->getName();
  if (Op != "encode" && Op != "update" && Op != "decode") {
    return Info;
  }

  auto* CTSD = llvm::dyn_cast_or_null<clang::ClassTemplateSpecializationDecl>(
      MD->getParent());
  if (!CTSD ||
      (CTSD->getName() != "BitField" && CTSD->getName() != "BitSetComputer")) {
    return Info;
  }

  const auto& Args = CTSD->getTemplateArgs();
  if (Args.size() < 3 ||
      Args[0].getKind() != clang::TemplateArgument::Type ||
      Args[1].getKind() != clang::TemplateArgument::Integral ||
      Args[2].getKind() != clang::TemplateArgument::Integral) {
    return Info;
  }

  Info.IsValid = true;
  Info.OpName = Op.str();
  Info.FieldType = Args[0].getAsType();
  if (CTSD->getName() == "BitSetComputer") {
    // BitSetComputer<T, kBitsPerItem, kBitsPerWord, U>
    Info.Size = Args[1].getAsIntegral().getSExtValue();
    Info.Shift = -Args[2].getAsIntegral().getSExtValue();
  } else {
    // BitField<T, shift, size, U>
    Info.Shift = Args[1].getAsIntegral().getSExtValue();
    Info.Size = Args[2].getAsIntegral().getSExtValue();
  }
  if (Args.size() >= 4 && Args[3].getKind() == clang::TemplateArgument::Type) {
    Info.BaseType = Args[3].getAsType();
  }

  if (clang::Expr* Callee = CE->getCallee()) {
    if (auto* DRE =
            llvm::dyn_cast<clang::DeclRefExpr>(Callee->IgnoreParenImpCasts())) {
      if (clang::NestedNameSpecifier NNS = DRE->getQualifier()) {
        if (const clang::Type* T = NNS.getAsType()) {
          if (const auto* TT = T->getAs<clang::TypedefType>()) {
            Info.TypedefDecl = TT->getDecl();
          }
        }
      }
    }
  }

  return Info;
}

const clang::FieldDecl* DfgVisitor::ExtractBackingFieldFromExpr(
    clang::Expr* E, unsigned Depth) {
  if (!E || Depth > 6) return nullptr;
  E = PeelExpr(E);
  if (!E) return nullptr;

  if (auto* ME = llvm::dyn_cast<clang::MemberExpr>(E)) {
    if (auto* FD = llvm::dyn_cast<clang::FieldDecl>(ME->getMemberDecl())) {
      return FD->getCanonicalDecl();
    }
  }

  if (auto* MCE = llvm::dyn_cast<clang::CXXMemberCallExpr>(E)) {
    if (auto* MD = MCE->getMethodDecl()) {
      if (const clang::FieldDecl* F = ResolveBackingField(MD)) {
        return F;
      }
      if (clang::Expr* Recv = MCE->getImplicitObjectArgument()) {
        llvm::StringRef MName = MD->getName();
        if (FindWrapperConfig(PeelExpr(Recv)->getType()) || MName == "load" ||
            MName == "Acquire_Load" || MName == "Relaxed_Load" ||
            MName == "SeqCst_Load" || MName == "ptr_location" ||
            MName == "location" || MName == "ptr") {
          return ExtractBackingFieldFromExpr(Recv, Depth + 1);
        }
      }
    }
  }

  if (auto* DRE = llvm::dyn_cast<clang::DeclRefExpr>(E)) {
    if (auto* VD = llvm::dyn_cast<clang::VarDecl>(DRE->getDecl())) {
      if (!ContextStack.empty()) {
        if (const clang::FieldDecl* F =
                ContextStack.back().LocalVarToBackingField.lookup(VD)) {
          return F;
        }
      }
      if (VD->isLocalVarDecl() && VD->hasInit()) {
        return ExtractBackingFieldFromExpr(VD->getInit(), Depth + 1);
      }
    }
  }

  if (auto* CE = llvm::dyn_cast<clang::CallExpr>(E)) {
    BitFieldInfo Bf = ExtractBitFieldInfo(CE);
    if (Bf.IsValid &&
        (Bf.OpName == "update" ||
         (Bf.OpName == "encode" && CE->getNumArgs() == 3)) &&
        CE->getNumArgs() >= 1) {
      return ExtractBackingFieldFromExpr(CE->getArg(0), Depth + 1);
    }
    if (auto* FD = CE->getDirectCallee()) {
      if (CE->getNumArgs() >= 1 &&
          WrapperConfigManager::Get().IsIdentityFunction(
              FD->getNameAsString())) {
        return ExtractBackingFieldFromExpr(CE->getArg(0), Depth + 1);
      }
    }
  }

  if (auto* BO = llvm::dyn_cast<clang::BinaryOperator>(E)) {
    if (const clang::FieldDecl* F =
            ExtractBackingFieldFromExpr(BO->getLHS(), Depth + 1)) {
      return F;
    }
    return ExtractBackingFieldFromExpr(BO->getRHS(), Depth + 1);
  }

  return nullptr;
}

Node::Id DfgVisitor::GetOrCreateTypedefBitFieldNode(const BitFieldInfo& Info) {
  if (!Info.TypedefDecl) return 0;
  std::string BaseKey =
      GetUSRForDecl(Info.TypedefDecl->getCanonicalDecl()).str();
  if (BaseKey.empty()) return 0;

  std::string KeyStr = BaseKey + "/bitfield_" + std::to_string(Info.Shift) +
                       "_" + std::to_string(Info.Size);
  Node::Id ID = HashFnv1a64(KeyStr);

  Node N;
  N.ID = ID;
  N.Kind = NodeKind::Field;
  N.Name = Info.TypedefDecl->getNameAsString();
  N.Type = GetTypeString(Info.FieldType).str();
  if (Info.Size > 0) {
    N.Type += " : " + std::to_string(Info.Size);
  }
  SourceLocation TdLoc = GetSourceLocation(Info.TypedefDecl->getLocation());
  if (!TdLoc.File.empty()) {
    N.Loc = TdLoc;
  }
  EmitNode(std::move(N));
  return ID;
}

Node::Id DfgVisitor::GetOrCreateBitFieldNode(
    const clang::FieldDecl* BackingField, const BitFieldInfo& Info) {
  if (!BackingField) {
    return GetOrCreateTypedefBitFieldNode(Info);
  }
  BackingField = BackingField->getCanonicalDecl();
  std::string BaseKey = GetUSRForDecl(BackingField).str();
  if (BaseKey.empty()) return 0;

  std::string KeyStr = BaseKey + "/bitfield_" + std::to_string(Info.Shift) +
                       "_" + std::to_string(Info.Size);
  Node::Id ID = HashFnv1a64(KeyStr);

  std::string BitLabel =
      Info.TypedefDecl
          ? Info.TypedefDecl->getNameAsString()
          : (std::to_string(Info.Shift) + ":" + std::to_string(Info.Size));

  Node N;
  N.ID = ID;
  N.Kind = NodeKind::Field;
  N.Name = BackingField->getNameAsString() + "[" + BitLabel + "]";
  N.Type = GetTypeString(Info.FieldType).str();
  if (Info.Size > 0) {
    N.Type += " : " + std::to_string(Info.Size);
  }
  N.Type += HeapTaintTypeSuffix(ClassifyFieldHeapTaint(BackingField));
  if (Info.TypedefDecl) {
    SourceLocation TdLoc = GetSourceLocation(Info.TypedefDecl->getLocation());
    if (!TdLoc.File.empty()) {
      N.Loc = TdLoc;
    }
  }
  if (N.Loc.File.empty()) {
    N.Loc = GetSourceLocation(BackingField->getLocation());
  }
  EmitNode(std::move(N));
  return ID;
}

void DfgVisitor::FlushBitFieldWriteToField(PendingBitFieldWrite& Write,
                                           const clang::FieldDecl* Field) {
  if (Write.CallNodeId == 0) return;
  if (Field) {
    Node::Id FieldBitId = GetOrCreateBitFieldNode(Field, Write.Info);
    if (FieldBitId != 0) {
      Edge E;
      E.From = Write.CallNodeId;
      E.To = FieldBitId;
      E.Kind = EdgeKind::Assignment;
      E.Loc = Write.CallLoc;
      EmitEdge(std::move(E));
      Write.Flushed = true;
    }
  }
  if (Write.Info.TypedefDecl) {
    Node::Id TypedefBitId = GetOrCreateTypedefBitFieldNode(Write.Info);
    if (TypedefBitId != 0) {
      Edge E;
      E.From = Write.CallNodeId;
      E.To = TypedefBitId;
      E.Kind = EdgeKind::Assignment;
      E.Loc = Write.CallLoc;
      EmitEdge(std::move(E));
      Write.Flushed = true;
    }
  }
}

void DfgVisitor::FlushRawWordWriteToRecordBitFields(
    clang::Expr* RhsExpr, Node::Id RhsId, const clang::FieldDecl* Field,
    EdgeKind Kind, const SourceLocation& Loc) {
  if (!RhsExpr || RhsId == 0 || !Field) return;
  clang::QualType FieldQT = Field->getType();
  if (FieldQT.isNull() || !FieldQT->isIntegralOrEnumerationType()) return;
  clang::QualType RhsQT = RhsExpr->getType();
  if (RhsQT.isNull() || !RhsQT->isIntegralOrEnumerationType() ||
      RhsExpr->isTypeDependent() || RhsExpr->isValueDependent()) {
    return;
  }
  clang::Expr::EvalResult Eval;
  if (RhsExpr->EvaluateAsInt(Eval, *Context)) {
    return;
  }
  const auto* ParentRD =
      llvm::dyn_cast_or_null<clang::CXXRecordDecl>(Field->getParent());
  if (!ParentRD || !ParentRD->hasDefinition()) return;
  ParentRD = ParentRD->getDefinition();

  for (const clang::Decl* D : ParentRD->decls()) {
    const auto* TD = llvm::dyn_cast<clang::TypedefNameDecl>(D);
    if (!TD) continue;
    clang::QualType UnderQT = TD->getUnderlyingType();
    if (UnderQT.isNull()) continue;
    const auto* CTSD =
        llvm::dyn_cast_or_null<clang::ClassTemplateSpecializationDecl>(
            UnderQT->getAsCXXRecordDecl());
    if (!CTSD || !CTSD->getIdentifier() ||
        (CTSD->getName() != "BitField" && CTSD->getName() != "BitSetComputer")) {
      continue;
    }
    const auto& Args = CTSD->getTemplateArgs();
    if (Args.size() < 3 ||
        Args[0].getKind() != clang::TemplateArgument::Type ||
        Args[1].getKind() != clang::TemplateArgument::Integral ||
        Args[2].getKind() != clang::TemplateArgument::Integral) {
      continue;
    }
    BitFieldInfo Info;
    Info.IsValid = true;
    Info.FieldType = Args[0].getAsType();
    if (CTSD->getName() == "BitSetComputer") {
      Info.Size = Args[1].getAsIntegral().getSExtValue();
      Info.Shift = -Args[2].getAsIntegral().getSExtValue();
    } else {
      Info.Shift = Args[1].getAsIntegral().getSExtValue();
      Info.Size = Args[2].getAsIntegral().getSExtValue();
    }
    if (Args.size() >= 4 &&
        Args[3].getKind() == clang::TemplateArgument::Type) {
      Info.BaseType = Args[3].getAsType();
    }
    Info.TypedefDecl = TD;

    Node::Id FieldBitId = GetOrCreateBitFieldNode(Field, Info);
    if (FieldBitId != 0 && FieldBitId != RhsId) {
      Edge E;
      E.From = RhsId;
      E.To = FieldBitId;
      E.Kind = Kind;
      E.Loc = Loc;
      EmitEdge(std::move(E));
    }
    Node::Id TypedefBitId = GetOrCreateTypedefBitFieldNode(Info);
    if (TypedefBitId != 0 && TypedefBitId != RhsId) {
      Edge E;
      E.From = RhsId;
      E.To = TypedefBitId;
      E.Kind = Kind;
      E.Loc = Loc;
      EmitEdge(std::move(E));
    }
  }
}

void DfgVisitor::FlushUnboundBitFieldWrites() {
  if (ContextStack.empty()) return;
  auto& Ctx = ContextStack.back();
  for (auto& Write : Ctx.PendingWrites) {
    if (!Write.Flushed) {
      FlushBitFieldWriteToField(Write, nullptr);
    }
  }
}

bool DfgVisitor::EnsureBitFieldCallProcessed(clang::CallExpr* CE) {
  if (!CE || ContextStack.empty()) return false;
  BitFieldInfo Info = ExtractBitFieldInfo(CE);
  if (!Info.IsValid) return false;

  auto& Ctx = ContextStack.back();
  if (!Ctx.ProcessedBitFieldCalls.insert(CE).second) {
    return true;
  }

  Node::Id CallId = GetNodeIdForExpr(CE);
  SourceLocation CallLoc = GetSourceLocation(CE->getBeginLoc());
  if (CE->getCallee()) {
    SourceLocation CalleeLoc = GetSourceLocation(CE->getCallee()->getExprLoc());
    if (!CalleeLoc.File.empty()) CallLoc = CalleeLoc;
  }

  if (Info.OpName == "decode") {
    if (CE->getNumArgs() >= 1) {
      const clang::FieldDecl* F = ExtractBackingFieldFromExpr(CE->getArg(0));
      if (F) {
        Node::Id FieldBitId = GetOrCreateBitFieldNode(F, Info);
        if (FieldBitId != 0 && CallId != 0) {
          Edge E;
          E.From = FieldBitId;
          E.To = CallId;
          E.Kind = EdgeKind::Return;
          E.Loc = CallLoc;
          EmitEdge(std::move(E));
        }
      } else {
        Node::Id ArgId = GetNodeIdForExpr(CE->getArg(0));
        if (ArgId != 0 && CallId != 0 && ArgId != CallId) {
          Edge E;
          E.From = ArgId;
          E.To = CallId;
          E.Kind = EdgeKind::ArgPass;
          E.Loc = GetSourceLocation(CE->getArg(0)->getBeginLoc());
          if (E.Loc.File.empty()) E.Loc = CallLoc;
          EmitEdge(std::move(E));
        }
      }
      if (Info.TypedefDecl) {
        Node::Id TypedefBitId = GetOrCreateTypedefBitFieldNode(Info);
        if (TypedefBitId != 0 && CallId != 0) {
          Edge E;
          E.From = TypedefBitId;
          E.To = CallId;
          E.Kind = EdgeKind::Return;
          E.Loc = CallLoc;
          EmitEdge(std::move(E));
        }
      }
    }
    return true;
  }

  if ((Info.OpName == "update" && CE->getNumArgs() >= 2) ||
      (Info.OpName == "encode" && CE->getNumArgs() == 3)) {
    // BitField::update(previous, value) or
    // BitSetComputer::encode(previous, item, value)
    clang::Expr* PrevExpr = CE->getArg(0);
    Node::Id PrevId = GetNodeIdForExpr(PrevExpr);
    if (PrevId != 0 && CallId != 0 && PrevId != CallId) {
      Edge E;
      E.From = PrevId;
      E.To = CallId;
      E.Kind = EdgeKind::ArgPass;
      E.Loc = GetSourceLocation(PrevExpr->getBeginLoc());
      EmitEdge(std::move(E));
    }
    clang::Expr* ValExpr = CE->getArg(CE->getNumArgs() - 1);
    Node::Id ValId = GetNodeIdForExpr(ValExpr);
    if (ValId != 0 && CallId != 0) {
      Edge E;
      E.From = ValId;
      E.To = CallId;
      E.Kind = EdgeKind::ArgPass;
      E.Loc = GetSourceLocation(ValExpr->getBeginLoc());
      EmitEdge(std::move(E));
    }

    size_t wIdx = Ctx.PendingWrites.size();
    Ctx.PendingWrites.push_back({Info, CallId, CallLoc, false});

    std::vector<size_t> Combined = CollectBitFieldWrites(CE->getArg(0));
    Combined.push_back(wIdx);
    Ctx.ExprBitFieldWrites[PeelExpr(CE)] = Combined;

    if (const clang::FieldDecl* F = ExtractBackingFieldFromExpr(CE->getArg(0))) {
      for (size_t idx : Combined) {
        FlushBitFieldWriteToField(Ctx.PendingWrites[idx], F);
      }
    }
    return true;
  }

  if (Info.OpName == "encode" && CE->getNumArgs() >= 1) {
    clang::Expr* ValExpr = CE->getArg(0);
    Node::Id ValId = GetNodeIdForExpr(ValExpr);
    if (ValId != 0 && CallId != 0) {
      Edge E;
      E.From = ValId;
      E.To = CallId;
      E.Kind = EdgeKind::ArgPass;
      E.Loc = GetSourceLocation(ValExpr->getBeginLoc());
      EmitEdge(std::move(E));
    }

    size_t wIdx = Ctx.PendingWrites.size();
    Ctx.PendingWrites.push_back({Info, CallId, CallLoc, false});
    Ctx.ExprBitFieldWrites[PeelExpr(CE)].push_back(wIdx);
    return true;
  }

  return true;
}

std::vector<size_t> DfgVisitor::CollectBitFieldWrites(clang::Expr* E,
                                                      unsigned Depth) {
  std::vector<size_t> Result;
  if (!E || Depth > 8 || ContextStack.empty()) return Result;
  E = PeelExpr(E);
  if (!E) return Result;

  if (auto* CE = llvm::dyn_cast<clang::CallExpr>(E)) {
    if (ExtractBitFieldInfo(CE).IsValid) {
      EnsureBitFieldCallProcessed(CE);
    }
  }

  auto& Ctx = ContextStack.back();
  auto It = Ctx.ExprBitFieldWrites.find(E);
  if (It != Ctx.ExprBitFieldWrites.end()) {
    return It->second;
  }

  if (auto* DRE = llvm::dyn_cast<clang::DeclRefExpr>(E)) {
    if (auto* VD = llvm::dyn_cast<clang::VarDecl>(DRE->getDecl())) {
      auto VIt = Ctx.VarBitFieldWrites.find(VD);
      if (VIt != Ctx.VarBitFieldWrites.end()) {
        return VIt->second;
      }
    }
  }

  if (auto* BO = llvm::dyn_cast<clang::BinaryOperator>(E)) {
    std::vector<size_t> L = CollectBitFieldWrites(BO->getLHS(), Depth + 1);
    std::vector<size_t> R = CollectBitFieldWrites(BO->getRHS(), Depth + 1);
    Result.insert(Result.end(), L.begin(), L.end());
    Result.insert(Result.end(), R.begin(), R.end());
    return Result;
  }

  if (auto* CO = llvm::dyn_cast<clang::AbstractConditionalOperator>(E)) {
    if (clang::Expr* TExpr = CO->getTrueExpr()) {
      std::vector<size_t> T = CollectBitFieldWrites(TExpr, Depth + 1);
      Result.insert(Result.end(), T.begin(), T.end());
    }
    if (clang::Expr* FExpr = CO->getFalseExpr()) {
      std::vector<size_t> F = CollectBitFieldWrites(FExpr, Depth + 1);
      Result.insert(Result.end(), F.begin(), F.end());
    }
    return Result;
  }

  return Result;
}

void DfgConsumer::HandleTranslationUnit(clang::ASTContext& Context) {
  if (Context.getDiagnostics().hasErrorOccurred()) {
    std::cerr << "[Consumer] Skipping file due to compilation errors: "
              << InFile << std::endl;
    if (Callback) {
      Callback(InFile);
    }
    return;
  }
  std::cerr << "[Consumer] Processing file: " << InFile << std::endl;

  // Traverse AST
  Visitor.TraverseDecl(Context.getTranslationUnitDecl());

  if (Callback) {
    Callback(InFile);
  }
}

}  // namespace dfg
