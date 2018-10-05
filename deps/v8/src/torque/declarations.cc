// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/torque/declarations.h"
#include "src/torque/declarable.h"
#include "src/torque/type-oracle.h"

namespace v8 {
namespace internal {
namespace torque {

Scope* Declarations::GetModuleScope(const Module* module) {
  auto i = module_scopes_.find(module);
  if (i != module_scopes_.end()) return i->second;
  Scope* result = chain_.NewScope();
  module_scopes_[module] = result;
  return result;
}

Scope* Declarations::GetNodeScope(const AstNode* node, bool reset_scope) {
  std::pair<const AstNode*, TypeVector> key(
      node, current_generic_specialization_ == nullptr
                ? TypeVector()
                : current_generic_specialization_->second);
  if (!reset_scope) {
    auto i = scopes_.find(key);
    if (i != scopes_.end()) return i->second;
  }
  Scope* result = chain_.NewScope();
  scopes_[key] = result;
  return result;
}

Scope* Declarations::GetGenericScope(Generic* generic,
                                     const TypeVector& types) {
  std::pair<const AstNode*, TypeVector> key(generic->declaration()->callable,
                                            types);
  auto i = scopes_.find(key);
  if (i != scopes_.end()) return i->second;
  Scope* result = chain_.NewScope();
  scopes_[key] = result;
  return result;
}

bool Declarations::IsDeclaredInCurrentScope(const std::string& name) {
  return chain_.ShallowLookup(name) != nullptr;
}

void Declarations::CheckAlreadyDeclared(const std::string& name,
                                        const char* new_type) {
  if (IsDeclaredInCurrentScope(name)) {
    std::stringstream s;
    s << "cannot redeclare " << name << " (type " << new_type << ")";
    ReportError(s.str());
  }
}

const Type* Declarations::LookupType(const std::string& name) {
  Declarable* raw = Lookup(name);
  if (raw->IsTypeAlias()) {
    return TypeAlias::cast(raw)->type();
  }
  std::stringstream s;
  s << "declaration \"" << name << "\" is not a Type";
  ReportError(s.str());
  return nullptr;
}

const Type* Declarations::LookupGlobalType(const std::string& name) {
  Declarable* raw = LookupGlobalScope(name);
  if (!raw->IsTypeAlias()) {
    std::stringstream s;
    s << "declaration \"" << name << "\" is not a Type";
    ReportError(s.str());
  }
  return TypeAlias::cast(raw)->type();
}

const Type* Declarations::GetType(TypeExpression* type_expression) {
  if (auto* basic = BasicTypeExpression::DynamicCast(type_expression)) {
    std::string name =
        (basic->is_constexpr ? CONSTEXPR_TYPE_PREFIX : "") + basic->name;
    return LookupType(name);
  } else if (auto* union_type = UnionTypeExpression::cast(type_expression)) {
    return TypeOracle::GetUnionType(GetType(union_type->a),
                                    GetType(union_type->b));
  } else {
    auto* function_type_exp = FunctionTypeExpression::cast(type_expression);
    TypeVector argument_types;
    for (TypeExpression* type_exp : function_type_exp->parameters) {
      argument_types.push_back(GetType(type_exp));
    }
    return TypeOracle::GetFunctionPointerType(
        argument_types, GetType(function_type_exp->return_type));
  }
}

Builtin* Declarations::FindSomeInternalBuiltinWithType(
    const FunctionPointerType* type) {
  for (auto& declarable : declarables_) {
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

Value* Declarations::LookupValue(const std::string& name) {
  Declarable* d = Lookup(name);
  if (!d->IsValue()) {
    std::stringstream s;
    s << "declaration \"" << name << "\" is not a Value";
    ReportError(s.str());
  }
  return Value::cast(d);
}

Label* Declarations::LookupLabel(const std::string& name) {
  Declarable* d = Lookup(name);
  if (!d->IsLabel()) {
    std::stringstream s;
    s << "declaration \"" << name << "\" is not a Label";
    ReportError(s.str());
  }
  return Label::cast(d);
}

Macro* Declarations::TryLookupMacro(const std::string& name,
                                    const TypeVector& types) {
  Declarable* declarable = TryLookup(name);
  if (declarable != nullptr) {
    if (declarable->IsMacroList()) {
      for (auto& m : MacroList::cast(declarable)->list()) {
        if (m->signature().parameter_types.types == types &&
            !m->signature().parameter_types.var_args) {
          return m;
        }
      }
    }
  }
  return nullptr;
}

Macro* Declarations::LookupMacro(const std::string& name,
                                 const TypeVector& types) {
  Macro* result = TryLookupMacro(name, types);
  if (result != nullptr) return result;
  std::stringstream stream;
  stream << "macro " << name << " with parameter types " << types
         << " is not defined";
  ReportError(stream.str());
  return nullptr;
}

Builtin* Declarations::LookupBuiltin(const std::string& name) {
  Declarable* declarable = Lookup(name);
  if (declarable != nullptr) {
    if (declarable->IsBuiltin()) {
      return Builtin::cast(declarable);
    }
    ReportError(name + " is not a builtin");
  }
  ReportError(std::string("builtin ") + name + " is not defined");
  return nullptr;
}

GenericList* Declarations::LookupGeneric(const std::string& name) {
  Declarable* declarable_list = Lookup(name);
  if (declarable_list != nullptr) {
    if (declarable_list->IsGenericList()) {
      return GenericList::cast(declarable_list);
    }
    ReportError(name + " is not a generic");
  }
  ReportError(std::string("generic ") + name + " is not defined");
  return nullptr;
}

ModuleConstant* Declarations::LookupModuleConstant(const std::string& name) {
  Declarable* declarable = Lookup(name);
  if (declarable != nullptr) {
    if (declarable->IsModuleConstant()) {
      return ModuleConstant::cast(declarable);
    }
    ReportError(name + " is not a constant");
  }
  ReportError(std::string("constant \"") + name + "\" is not defined");
  return nullptr;
}

const AbstractType* Declarations::DeclareAbstractType(
    const std::string& name, const std::string& generated,
    base::Optional<const AbstractType*> non_constexpr_version,
    const base::Optional<std::string>& parent) {
  CheckAlreadyDeclared(name, "type");
  const Type* parent_type = nullptr;
  if (parent) {
    Declarable* maybe_parent_type = Lookup(*parent);
    if (maybe_parent_type == nullptr) {
      std::stringstream s;
      s << "cannot find parent type \"" << *parent << "\"";
      ReportError(s.str());
    }
    if (!maybe_parent_type->IsTypeAlias()) {
      std::stringstream s;
      s << "parent \"" << *parent << "\" of type \"" << name << "\""
        << " is not a type";
      ReportError(s.str());
    }
    parent_type = TypeAlias::cast(maybe_parent_type)->type();
  }
  const AbstractType* type = TypeOracle::GetAbstractType(
      parent_type, name, generated, non_constexpr_version);
  DeclareType(name, type);
  return type;
}

void Declarations::DeclareType(const std::string& name, const Type* type) {
  CheckAlreadyDeclared(name, "type");
  TypeAlias* result = new TypeAlias(type);
  Declare(name, std::unique_ptr<TypeAlias>(result));
}

void Declarations::DeclareStruct(Module* module, const std::string& name,
                                 const std::vector<NameAndType>& fields) {
  const StructType* new_type = TypeOracle::GetStructType(module, name, fields);
  DeclareType(name, new_type);
}

Label* Declarations::DeclareLabel(const std::string& name,
                                  base::Optional<Statement*> statement) {
  CheckAlreadyDeclared(name, "label");
  bool deferred = false;
  if (statement) {
    BlockStatement* block = BlockStatement::DynamicCast(*statement);
    deferred = block && block->deferred;
  }
  Label* result = new Label(name, deferred);
  Declare(name, std::unique_ptr<Declarable>(result));
  return result;
}

MacroList* Declarations::GetMacroListForName(const std::string& name,
                                             const Signature& signature) {
  auto previous = chain_.Lookup(name);
  MacroList* macro_list = nullptr;
  if (previous == nullptr) {
    macro_list = new MacroList();
    Declare(name, std::unique_ptr<Declarable>(macro_list));
  } else if (!previous->IsMacroList()) {
    std::stringstream s;
    s << "cannot redeclare non-macro " << name << " as a macro";
    ReportError(s.str());
  } else {
    macro_list = MacroList::cast(previous);
  }
  for (auto& macro : macro_list->list()) {
    if (signature.parameter_types.types ==
            macro->signature().parameter_types.types &&
        signature.parameter_types.var_args ==
            macro->signature().parameter_types.var_args) {
      std::stringstream s;
      s << "cannot redeclare " << name
        << " as a macro with identical parameter list "
        << signature.parameter_types;
      ReportError(s.str());
    }
  }
  return macro_list;
}

Macro* Declarations::DeclareMacro(const std::string& name,
                                  const Signature& signature,
                                  base::Optional<std::string> op) {
  Macro* macro = RegisterDeclarable(
      std::unique_ptr<Macro>(new Macro(name, signature, GetCurrentGeneric())));
  GetMacroListForName(name, signature)->AddMacro(macro);
  if (op) GetMacroListForName(*op, signature)->AddMacro(macro);
  return macro;
}

Builtin* Declarations::DeclareBuiltin(const std::string& name,
                                      Builtin::Kind kind, bool external,
                                      const Signature& signature) {
  CheckAlreadyDeclared(name, "builtin");
  Builtin* result =
      new Builtin(name, kind, external, signature, GetCurrentGeneric());
  Declare(name, std::unique_ptr<Declarable>(result));
  return result;
}

RuntimeFunction* Declarations::DeclareRuntimeFunction(
    const std::string& name, const Signature& signature) {
  CheckAlreadyDeclared(name, "runtime function");
  RuntimeFunction* result =
      new RuntimeFunction(name, signature, GetCurrentGeneric());
  Declare(name, std::unique_ptr<Declarable>(result));
  return result;
}

Variable* Declarations::CreateVariable(const std::string& var, const Type* type,
                                       bool is_const) {
  return RegisterDeclarable(
      std::unique_ptr<Variable>(new Variable(var, type, is_const)));
}

Variable* Declarations::DeclareVariable(const std::string& var,
                                        const Type* type, bool is_const) {
  CheckAlreadyDeclared(var, "variable");
  Variable* result = new Variable(var, type, is_const);
  Declare(var, std::unique_ptr<Declarable>(result));
  return result;
}

Parameter* Declarations::DeclareParameter(const std::string& name,
                                          std::string external_name,
                                          const Type* type) {
  CheckAlreadyDeclared(name, "parameter");
  Parameter* result = new Parameter(name, std::move(external_name), type);
  Declare(name, std::unique_ptr<Declarable>(result));
  return result;
}

void Declarations::DeclareExternConstant(const std::string& name,
                                         const Type* type, std::string value) {
  CheckAlreadyDeclared(name, "constant, parameter or arguments");
  ExternConstant* result = new ExternConstant(name, type, value);
  Declare(name, std::unique_ptr<Declarable>(result));
}

ModuleConstant* Declarations::DeclareModuleConstant(const std::string& name,
                                                    const Type* type) {
  CheckAlreadyDeclared(name, "module constant");
  ModuleConstant* result = new ModuleConstant(name, type);
  Declare(name, std::unique_ptr<Declarable>(result));
  return result;
}

Generic* Declarations::DeclareGeneric(const std::string& name, Module* module,
                                      GenericDeclaration* generic) {
  auto previous = chain_.Lookup(name);
  GenericList* generic_list = nullptr;
  if (previous == nullptr) {
    generic_list = new GenericList();
    Declare(name, std::unique_ptr<Declarable>(generic_list));
  } else if (!previous->IsGenericList()) {
    std::stringstream s;
    s << "cannot redeclare non-generic " << name << " as a generic";
    ReportError(s.str());
  } else {
    generic_list = GenericList::cast(previous);
  }
  Generic* result = RegisterDeclarable(
      std::unique_ptr<Generic>(new Generic(name, module, generic)));
  generic_list->AddGeneric(result);
  generic_declaration_scopes_[result] = GetScopeChainSnapshot();
  return result;
}

TypeVector Declarations::GetCurrentSpecializationTypeNamesVector() {
  TypeVector result;
  if (current_generic_specialization_ != nullptr) {
    result = current_generic_specialization_->second;
  }
  return result;
}

base::Optional<Generic*> Declarations::GetCurrentGeneric() {
  if (current_generic_specialization_ != nullptr) {
    return current_generic_specialization_->first;
  }
  return base::nullopt;
}

std::string GetGeneratedCallableName(const std::string& name,
                                     const TypeVector& specialized_types) {
  std::string result = name;
  for (auto type : specialized_types) {
    std::string type_string = type->MangledName();
    result += std::to_string(type_string.size()) + type_string;
  }
  return result;
}

}  // namespace torque
}  // namespace internal
}  // namespace v8
