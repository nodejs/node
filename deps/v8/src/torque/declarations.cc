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

DEFINE_CONTEXTUAL_VARIABLE(GlobalContext)

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

const Type* Declarations::LookupType(std::string name) {
  return LookupType(QualifiedName(std::move(name)));
}

const Type* Declarations::LookupGlobalType(const std::string& name) {
  TypeAlias* declaration = EnsureUnique(
      FilterDeclarables<TypeAlias>(LookupGlobalScope(name)), name, "type");
  return declaration->type();
}

const Type* Declarations::GetType(TypeExpression* type_expression) {
  if (auto* basic = BasicTypeExpression::DynamicCast(type_expression)) {
    std::string name =
        (basic->is_constexpr ? CONSTEXPR_TYPE_PREFIX : "") + basic->name;
    const TypeAlias* alias =
        LookupTypeAlias(QualifiedName{basic->namespace_qualification, name});
    if (GlobalContext::collect_language_server_data()) {
      LanguageServerData::AddDefinition(type_expression->pos,
                                        alias->GetDeclarationPosition());
    }
    return alias->type();
  } else if (auto* union_type =
                 UnionTypeExpression::DynamicCast(type_expression)) {
    return TypeOracle::GetUnionType(GetType(union_type->a),
                                    GetType(union_type->b));
  } else if (auto* reference_type =
                 ReferenceTypeExpression::DynamicCast(type_expression)) {
    return TypeOracle::GetReferenceType(
        GetType(reference_type->referenced_type));
  } else {
    auto* function_type_exp = FunctionTypeExpression::cast(type_expression);
    TypeVector argument_types;
    for (TypeExpression* type_exp : function_type_exp->parameters) {
      argument_types.push_back(GetType(type_exp));
    }
    return TypeOracle::GetBuiltinPointerType(
        argument_types, GetType(function_type_exp->return_type));
  }
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

Namespace* Declarations::DeclareNamespace(const std::string& name) {
  return Declare(name, std::unique_ptr<Namespace>(new Namespace(name)));
}

const AbstractType* Declarations::DeclareAbstractType(
    const Identifier* name, bool transient, std::string generated,
    base::Optional<const AbstractType*> non_constexpr_version,
    const base::Optional<Identifier*>& parent) {
  CheckAlreadyDeclared<TypeAlias>(name->value, "type");
  const Type* parent_type = nullptr;
  if (parent) {
    auto parent_type_alias = LookupTypeAlias(QualifiedName{(*parent)->value});
    parent_type = parent_type_alias->type();
    if (GlobalContext::collect_language_server_data()) {
      LanguageServerData::AddDefinition(
          (*parent)->pos, parent_type_alias->GetDeclarationPosition());
    }
  }
  if (generated == "" && parent) {
    generated = parent_type->GetGeneratedTNodeTypeName();
  }
  const AbstractType* type = TypeOracle::GetAbstractType(
      parent_type, name->value, transient, generated, non_constexpr_version);
  DeclareType(name, type, false);
  return type;
}

void Declarations::DeclareType(const Identifier* name, const Type* type,
                               bool redeclaration) {
  CheckAlreadyDeclared<TypeAlias>(name->value, "type");
  Declare(name->value, std::unique_ptr<TypeAlias>(
                           new TypeAlias(type, redeclaration, name->pos)));
}

StructType* Declarations::DeclareStruct(const Identifier* name) {
  StructType* new_type = TypeOracle::GetStructType(name->value);
  DeclareType(name, new_type, false);
  return new_type;
}

ClassType* Declarations::DeclareClass(const Type* super_type,
                                      const Identifier* name, bool is_extern,
                                      bool generate_print, bool transient,
                                      const std::string& generates) {
  ClassType* new_type = TypeOracle::GetClassType(
      super_type, name->value, is_extern, generate_print, transient, generates);
  DeclareType(name, new_type, false);
  return new_type;
}

Macro* Declarations::CreateMacro(
    std::string external_name, std::string readable_name,
    base::Optional<std::string> external_assembler_name, Signature signature,
    bool transitioning, base::Optional<Statement*> body) {
  if (!external_assembler_name) {
    external_assembler_name = CurrentNamespace()->ExternalName();
  }
  return RegisterDeclarable(std::unique_ptr<Macro>(
      new Macro(std::move(external_name), std::move(readable_name),
                std::move(*external_assembler_name), std::move(signature),
                transitioning, body)));
}

Macro* Declarations::DeclareMacro(
    const std::string& name,
    base::Optional<std::string> external_assembler_name,
    const Signature& signature, bool transitioning,
    base::Optional<Statement*> body, base::Optional<std::string> op) {
  if (TryLookupMacro(name, signature.GetExplicitTypes())) {
    ReportError("cannot redeclare macro ", name,
                " with identical explicit parameters");
  }
  Macro* macro = CreateMacro(name, name, std::move(external_assembler_name),
                             signature, transitioning, body);
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
                                   bool transitioning, Statement* body) {
  std::string generated_name{container_type->GetGeneratedMethodName(name)};
  Method* result = RegisterDeclarable(std::unique_ptr<Method>(
      new Method(container_type, container_type->GetGeneratedMethodName(name),
                 name, CurrentNamespace()->ExternalName(), std::move(signature),
                 transitioning, body)));
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
                                     bool transitioning,
                                     base::Optional<Statement*> body) {
  return RegisterDeclarable(std::unique_ptr<Builtin>(
      new Builtin(std::move(external_name), std::move(readable_name), kind,
                  std::move(signature), transitioning, body)));
}

Builtin* Declarations::DeclareBuiltin(const std::string& name,
                                      Builtin::Kind kind,
                                      const Signature& signature,
                                      bool transitioning,
                                      base::Optional<Statement*> body) {
  CheckAlreadyDeclared<Builtin>(name, "builtin");
  return Declare(
      name, CreateBuiltin(name, name, kind, signature, transitioning, body));
}

RuntimeFunction* Declarations::DeclareRuntimeFunction(
    const std::string& name, const Signature& signature, bool transitioning) {
  CheckAlreadyDeclared<RuntimeFunction>(name, "runtime function");
  return Declare(name,
                 RegisterDeclarable(std::unique_ptr<RuntimeFunction>(
                     new RuntimeFunction(name, signature, transitioning))));
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
  NamespaceConstant* result = new NamespaceConstant(name, type, body);
  Declare(name->value, std::unique_ptr<Declarable>(result));
  return result;
}

Generic* Declarations::DeclareGeneric(const std::string& name,
                                      GenericDeclaration* generic) {
  return Declare(name, std::unique_ptr<Generic>(new Generic(name, generic)));
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
