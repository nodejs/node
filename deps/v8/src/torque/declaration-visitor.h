// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TORQUE_DECLARATION_VISITOR_H_
#define V8_TORQUE_DECLARATION_VISITOR_H_

#include <set>
#include <string>

#include "src/base/macros.h"
#include "src/torque/declarations.h"
#include "src/torque/global-context.h"
#include "src/torque/types.h"
#include "src/torque/utils.h"

namespace v8 {
namespace internal {
namespace torque {

Namespace* GetOrCreateNamespace(const std::string& name);

class PredeclarationVisitor {
 public:
  static void Predeclare(Ast* ast) {
    CurrentScope::Scope current_namespace(GlobalContext::GetDefaultNamespace());
    for (Declaration* child : ast->declarations()) Predeclare(child);
  }
  static void ResolvePredeclarations();

 private:
  static void Predeclare(Declaration* decl);
  static void Predeclare(NamespaceDeclaration* decl) {
    CurrentScope::Scope current_scope(GetOrCreateNamespace(decl->name));
    for (Declaration* child : decl->declarations) Predeclare(child);
  }
  static void Predeclare(TypeDeclaration* decl) {
    Declarations::PredeclareTypeAlias(decl->name, decl, false);
  }
  static void Predeclare(StructDeclaration* decl) {
    Declarations::PredeclareTypeAlias(decl->name, decl, false);
  }
  static void Predeclare(GenericTypeDeclaration* generic_decl) {
    Declarations::DeclareGenericType(generic_decl->declaration->name->value,
                                     generic_decl);
  }
  static void Predeclare(GenericCallableDeclaration* generic_decl) {
    Declarations::DeclareGenericCallable(generic_decl->declaration->name->value,
                                         generic_decl);
  }
};

class DeclarationVisitor {
 public:
  static void Visit(Ast* ast) {
    CurrentScope::Scope current_namespace(GlobalContext::GetDefaultNamespace());
    for (Declaration* child : ast->declarations()) Visit(child);
  }
  static void Visit(Declaration* decl);
  static void Visit(NamespaceDeclaration* decl) {
    CurrentScope::Scope current_scope(GetOrCreateNamespace(decl->name));
    for (Declaration* child : decl->declarations) Visit(child);
  }

  static void Visit(TypeDeclaration* decl) {
    // Looking up the type will trigger type computation; this ensures errors
    // are reported even if the type is unused.
    Declarations::LookupType(decl->name);
  }
  static void Visit(StructDeclaration* decl) {
    Declarations::LookupType(decl->name);
  }

  static Builtin* CreateBuiltin(BuiltinDeclaration* decl,
                                std::string external_name,
                                std::string readable_name, Signature signature,
                                base::Optional<Statement*> body);

  static void Visit(ExternalBuiltinDeclaration* decl);
  static void Visit(ExternalRuntimeDeclaration* decl);
  static void Visit(ExternalMacroDeclaration* decl);
  static void Visit(TorqueBuiltinDeclaration* decl);
  static void Visit(TorqueMacroDeclaration* decl);
  static void Visit(IntrinsicDeclaration* decl);

  static void Visit(ConstDeclaration* decl);
  static void Visit(GenericCallableDeclaration* decl) {
    // The PredeclarationVisitor already handled this case.
  }
  static void Visit(GenericTypeDeclaration* decl) {
    // The PredeclarationVisitor already handled this case.
  }
  static void Visit(SpecializationDeclaration* decl);
  static void Visit(ExternConstDeclaration* decl);
  static void Visit(CppIncludeDeclaration* decl);

  static Signature MakeSpecializedSignature(
      const SpecializationKey<GenericCallable>& key);
  static Callable* SpecializeImplicit(
      const SpecializationKey<GenericCallable>& key);
  static Callable* Specialize(
      const SpecializationKey<GenericCallable>& key,
      CallableDeclaration* declaration,
      base::Optional<const SpecializationDeclaration*> explicit_specialization,
      base::Optional<Statement*> body, SourcePosition position);

 private:
  static void DeclareSpecializedTypes(
      const SpecializationKey<GenericCallable>& key);
};

}  // namespace torque
}  // namespace internal
}  // namespace v8

#endif  // V8_TORQUE_DECLARATION_VISITOR_H_
