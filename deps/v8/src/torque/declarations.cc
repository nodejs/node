// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/torque/declarations.h"
#include "src/torque/declarable.h"
#include "src/torque/global-context.h"
#include "src/torque/server-data.h"
#include "src/torque/type-oracle.h"

namespace v8 {
namespace internal {
namespace torque {
namespace {

template <class T>
std::vector<T> EnsureNonempty(std::vector<T> list, const std::string& name,
                              const char* kind) {
  if (list.empty()) {
    ReportError("there is no ", kind, " named ", name);
  }
  return std::move(list);
}

template <class T, class Name>
T EnsureUnique(const std::vector<T>& list, const Name& name, const char* kind) {
  if (list.empty()) {
    ReportError("there is no ", kind, " named ", name);
  }
  if (list.size() >= 2) {
    ReportError("ambiguous reference to ", kind, " ", name);
  }
  return list.front();
}

template <class T>
void CheckAlreadyDeclared(const std::string& name, const char* new_type) {
  std::vector<T*> declarations =
      FilterDeclarables<T>(Declarations::TryLookupShallow(QualifiedName(name)));
  if (!declarations.empty()) {
    Scope* scope = CurrentScope::Get();
    ReportError("cannot redeclare ", name, " (type ", new_type, scope, ")");
  }
}

}  // namespace

std::vector<Declarable*> Declarations::LookupGlobalScope(
    const std::string& name) {
  std::vector<Declarable*> d =
      GlobalContext::GetDefaultNamespace()->Lookup(QualifiedName(name));
  if (d.empty()) {
    std::stringstream s;
    s << "cannot find \"" << name << "\" in global scope";
    ReportError(s.str());
  }
  return d;
}

const TypeAlias* Declarations::LookupTypeAlias(const QualifiedName& name) {
  TypeAlias* declaration =
      EnsureUnique(FilterDeclarables<TypeAlias>(Lookup(name)), name, "type");
  return declaration;
}

const Type* Declarations::LookupType(const QualifiedName& name) {
  return LookupTypeAlias(name)->type();
}

const Type* Declarations::LookupType(const Identifier* name) {
  const TypeAlias* alias = LookupTypeAlias(QualifiedName(name->value));
  if (GlobalContext::collect_language_server_data()) {
    LanguageServerData::AddDefinition(name->pos,
                                      alias->GetDeclarationPosition());
  }
  return alias->type();
}

const Type* Declarations::LookupGlobalType(const std::string& name) {
  TypeAlias* declaration = EnsureUnique(
      FilterDeclarables<TypeAlias>(LookupGlobalScope(name)), name, "type");
  return declaration->type();
}

Builtin* Declarations::FindSomeInternalBuiltinWithType(
    const BuiltinPointerType* type) {
  for (auto& declarable : GlobalContext::AllDeclarables()) {
    if (Builtin* builtin = Builtin::DynamicCast(declarable.get())) {
      if (!builtin->IsExternal() && builtin->kind() == Builtin::kStub &&
          builtin->signature().return_type == type->return_type() &&
          builtin->signature().parameter_types.types ==
              type->parameter_types()) {
        return builtin;
      }
    }
  }
  return nullptr;
}

Value* Declarations::LookupValue(const QualifiedName& name) {
  return EnsureUnique(FilterDeclarables<Value>(Lookup(name)), name, "value");
}

Macro* Declarations::TryLookupMacro(const std::string& name,
                                    const TypeVector& types) {
  std::vector<Macro*> macros = TryLookup<Macro>(QualifiedName(name));
  for (auto& m : macros) {
    auto signature_types = m->signature().GetExplicitTypes();
    if (signature_types == types && !m->signature().parameter_types.var_args) {
      return m;
    }
  }
  return nullptr;
}

base::Optional<Builtin*> Declarations::TryLookupBuiltin(
    const QualifiedName& name) {
  std::vector<Builtin*> builtins = TryLookup<Builtin>(name);
  if (builtins.empty()) return base::nullopt;
  return EnsureUnique(builtins, name.name, "builtin");
}

std::vector<Generic*> Declarations::LookupGeneric(const std::string& name) {
  return EnsureNonempty(FilterDeclarables<Generic>(Lookup(QualifiedName(name))),
                        name, "generic");
}

Generic* Declarations::LookupUniqueGeneric(const QualifiedName& name) {
  return EnsureUnique(FilterDeclarables<Generic>(Lookup(name)), name,
                      "generic");
}

GenericStructType* Declarations::LookupUniqueGenericStructType(
    const QualifiedName& name) {
  return EnsureUnique(FilterDeclarables<GenericStructType>(Lookup(name)), name,
                      "generic struct");
}

base::Optional<GenericStructType*> Declarations::TryLookupGenericStructType(
    const QualifiedName& name) {
  std::vector<GenericStructType*> results = TryLookup<GenericStructType>(name);
  if (results.empty()) return base::nullopt;
  return EnsureUnique(results, name.name, "generic struct");
}

Namespace* Declarations::DeclareNamespace(const std::string& name) {
  return Declare(name, std::unique_ptr<Namespace>(new Namespace(name)));
}

TypeAlias* Declarations::DeclareType(const Identifier* name, const Type* type) {
  CheckAlreadyDeclared<TypeAlias>(name->value, "type");
  return Declare(name->value, std::unique_ptr<TypeAlias>(
                                  new TypeAlias(type, true, name->pos)));
}

const TypeAlias* Declarations::PredeclareTypeAlias(const Identifier* name,
                                                   TypeDeclaration* type,
                                                   bool redeclaration) {
  CheckAlreadyDeclared<TypeAlias>(name->value, "type");
  std::unique_ptr<TypeAlias> alias_ptr(
      new TypeAlias(type, redeclaration, name->pos));
  return Declare(name->value, std::move(alias_ptr));
}

TorqueMacro* Declarations::CreateTorqueMacro(std::string external_name,
                                             std::string readable_name,
                                             bool exported_to_csa,
                                             Signature signature,
                                             base::Optional<Statement*> body,
                                             bool is_user_defined) {
  // TODO(tebbi): Switch to more predictable names to improve incremental
  // compilation.
  external_name += "_" + std::to_string(GlobalContext::FreshId());
  return RegisterDeclarable(std::unique_ptr<TorqueMacro>(new TorqueMacro(
      std::move(external_name), std::move(readable_name), std::move(signature),
      body, is_user_defined, exported_to_csa)));
}

ExternMacro* Declarations::CreateExternMacro(
    std::string name, std::string external_assembler_name,
    Signature signature) {
  return RegisterDeclarable(std::unique_ptr<ExternMacro>(
      new ExternMacro(std::move(name), std::move(external_assembler_name),
                      std::move(signature))));
}

Macro* Declarations::DeclareMacro(
    const std::string& name, bool accessible_from_csa,
    base::Optional<std::string> external_assembler_name,
    const Signature& signature, base::Optional<Statement*> body,
    base::Optional<std::string> op, bool is_user_defined) {
  if (TryLookupMacro(name, signature.GetExplicitTypes())) {
    ReportError("cannot redeclare macro ", name,
                " with identical explicit parameters");
  }
  Macro* macro;
  if (external_assembler_name) {
    macro =
        CreateExternMacro(name, std::move(*external_assembler_name), signature);
  } else {
    macro = CreateTorqueMacro(name, name, accessible_from_csa, signature, body,
                              is_user_defined);
  }
  Declare(name, macro);
  if (op) {
    if (TryLookupMacro(*op, signature.GetExplicitTypes())) {
      ReportError("cannot redeclare operator ", name,
                  " with identical explicit parameters");
    }
    DeclareOperator(*op, macro);
  }
  return macro;
}

Method* Declarations::CreateMethod(AggregateType* container_type,
                                   const std::string& name, Signature signature,
                                   Statement* body) {
  std::string generated_name{container_type->GetGeneratedMethodName(name)};
  Method* result = RegisterDeclarable(std::unique_ptr<Method>(
      new Method(container_type, container_type->GetGeneratedMethodName(name),
                 name, std::move(signature), body)));
  container_type->RegisterMethod(result);
  return result;
}

Intrinsic* Declarations::CreateIntrinsic(const std::string& name,
                                         const Signature& signature) {
  Intrinsic* result = RegisterDeclarable(std::unique_ptr<Intrinsic>(
      new Intrinsic(std::move(name), std::move(signature))));
  return result;
}

Intrinsic* Declarations::DeclareIntrinsic(const std::string& name,
                                          const Signature& signature) {
  Intrinsic* result = CreateIntrinsic(std::move(name), std::move(signature));
  Declare(name, result);
  return result;
}

Builtin* Declarations::CreateBuiltin(std::string external_name,
                                     std::string readable_name,
                                     Builtin::Kind kind, Signature signature,

                                     base::Optional<Statement*> body) {
  return RegisterDeclarable(std::unique_ptr<Builtin>(
      new Builtin(std::move(external_name), std::move(readable_name), kind,
                  std::move(signature), body)));
}

Builtin* Declarations::DeclareBuiltin(const std::string& name,
                                      Builtin::Kind kind,
                                      const Signature& signature,

                                      base::Optional<Statement*> body) {
  CheckAlreadyDeclared<Builtin>(name, "builtin");
  return Declare(name, CreateBuiltin(name, name, kind, signature, body));
}

RuntimeFunction* Declarations::DeclareRuntimeFunction(
    const std::string& name, const Signature& signature) {
  CheckAlreadyDeclared<RuntimeFunction>(name, "runtime function");
  return Declare(name, RegisterDeclarable(std::unique_ptr<RuntimeFunction>(
                           new RuntimeFunction(name, signature))));
}

void Declarations::DeclareExternConstant(Identifier* name, const Type* type,
                                         std::string value) {
  CheckAlreadyDeclared<Value>(name->value, "constant");
  ExternConstant* result = new ExternConstant(name, type, value);
  Declare(name->value, std::unique_ptr<Declarable>(result));
}

NamespaceConstant* Declarations::DeclareNamespaceConstant(Identifier* name,
                                                          const Type* type,
                                                          Expression* body) {
  CheckAlreadyDeclared<Value>(name->value, "constant");
  std::string external_name =
      name->value + "_" + std::to_string(GlobalContext::FreshId());
  NamespaceConstant* result =
      new NamespaceConstant(name, std::move(external_name), type, body);
  Declare(name->value, std::unique_ptr<Declarable>(result));
  return result;
}

Generic* Declarations::DeclareGeneric(const std::string& name,
                                      GenericDeclaration* generic) {
  return Declare(name, std::unique_ptr<Generic>(new Generic(name, generic)));
}

GenericStructType* Declarations::DeclareGenericStructType(
    const std::string& name, StructDeclaration* decl) {
  return Declare(name, std::unique_ptr<GenericStructType>(
                           new GenericStructType(name, decl)));
}

std::string Declarations::GetGeneratedCallableName(
    const std::string& name, const TypeVector& specialized_types) {
  std::string result = name;
  for (auto type : specialized_types) {
    std::string type_string = type->MangledName();
    result += std::to_string(type_string.size()) + type_string;
  }
  return result;
}

Macro* Declarations::DeclareOperator(const std::string& name, Macro* m) {
  GlobalContext::GetDefaultNamespace()->AddDeclarable(name, m);
  return m;
}

}  // namespace torque
}  // namespace internal
}  // namespace v8
