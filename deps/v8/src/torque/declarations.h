// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TORQUE_DECLARATIONS_H_
#define V8_TORQUE_DECLARATIONS_H_

#include <string>

#include "src/torque/declarable.h"
#include "src/torque/utils.h"

namespace v8 {
namespace internal {
namespace torque {

static constexpr const char* const kFromConstexprMacroName = "FromConstexpr";
static constexpr const char* kTrueLabelName = "_True";
static constexpr const char* kFalseLabelName = "_False";

template <class T>
std::vector<T*> FilterDeclarables(const std::vector<Declarable*> list) {
  std::vector<T*> result;
  for (Declarable* declarable : list) {
    if (T* t = T::DynamicCast(declarable)) {
      result.push_back(t);
    }
  }
  return result;
}

class Declarations {
 public:
  static std::vector<Declarable*> TryLookup(const QualifiedName& name) {
    return CurrentScope::Get()->Lookup(name);
  }

  static std::vector<Declarable*> TryLookupShallow(const QualifiedName& name) {
    return CurrentScope::Get()->LookupShallow(name);
  }

  template <class T>
  static std::vector<T*> TryLookup(const QualifiedName& name) {
    return FilterDeclarables<T>(TryLookup(name));
  }

  static std::vector<Declarable*> Lookup(const QualifiedName& name) {
    std::vector<Declarable*> d = TryLookup(name);
    if (d.empty()) {
      ReportError("cannot find \"", name, "\"");
    }
    return d;
  }

  static std::vector<Declarable*> LookupGlobalScope(const std::string& name);

  static const TypeAlias* LookupTypeAlias(const QualifiedName& name);
  static const Type* LookupType(const QualifiedName& name);
  static const Type* LookupType(std::string name);
  static const Type* LookupGlobalType(const std::string& name);
  static const Type* GetType(TypeExpression* type_expression);

  static Builtin* FindSomeInternalBuiltinWithType(
      const BuiltinPointerType* type);

  static Value* LookupValue(const QualifiedName& name);

  static Macro* TryLookupMacro(const std::string& name,
                               const TypeVector& types);
  static base::Optional<Builtin*> TryLookupBuiltin(const QualifiedName& name);

  static std::vector<Generic*> LookupGeneric(const std::string& name);
  static Generic* LookupUniqueGeneric(const QualifiedName& name);

  static Namespace* DeclareNamespace(const std::string& name);

  static const AbstractType* DeclareAbstractType(
      const Identifier* name, bool transient, std::string generated,
      base::Optional<const AbstractType*> non_constexpr_version,
      const base::Optional<Identifier*>& parent = {});

  static void DeclareType(const Identifier* name, const Type* type,
                          bool redeclaration);

  static StructType* DeclareStruct(const Identifier* name);

  static ClassType* DeclareClass(const Type* super, const Identifier* name,
                                 bool is_extern, bool generate_print,
                                 bool transient, const std::string& generates);

  static Macro* CreateMacro(std::string external_name,
                            std::string readable_name,
                            base::Optional<std::string> external_assembler_name,
                            Signature signature, bool transitioning,
                            base::Optional<Statement*> body);
  static Macro* DeclareMacro(
      const std::string& name,
      base::Optional<std::string> external_assembler_name,
      const Signature& signature, bool transitioning,
      base::Optional<Statement*> body, base::Optional<std::string> op = {});

  static Method* CreateMethod(AggregateType* class_type,
                              const std::string& name, Signature signature,
                              bool transitioning, Statement* body);

  static Intrinsic* CreateIntrinsic(const std::string& name,
                                    const Signature& signature);

  static Intrinsic* DeclareIntrinsic(const std::string& name,
                                     const Signature& signature);

  static Builtin* CreateBuiltin(std::string external_name,
                                std::string readable_name, Builtin::Kind kind,
                                Signature signature, bool transitioning,
                                base::Optional<Statement*> body);
  static Builtin* DeclareBuiltin(const std::string& name, Builtin::Kind kind,
                                 const Signature& signature, bool transitioning,
                                 base::Optional<Statement*> body);

  static RuntimeFunction* DeclareRuntimeFunction(const std::string& name,
                                                 const Signature& signature,
                                                 bool transitioning);

  static void DeclareExternConstant(Identifier* name, const Type* type,
                                    std::string value);
  static NamespaceConstant* DeclareNamespaceConstant(Identifier* name,
                                                     const Type* type,
                                                     Expression* body);

  static Generic* DeclareGeneric(const std::string& name,
                                 GenericDeclaration* generic);

  template <class T>
  static T* Declare(const std::string& name, T* d) {
    CurrentScope::Get()->AddDeclarable(name, d);
    return d;
  }
  template <class T>
  static T* Declare(const std::string& name, std::unique_ptr<T> d) {
    return CurrentScope::Get()->AddDeclarable(name,
                                              RegisterDeclarable(std::move(d)));
  }
  static Macro* DeclareOperator(const std::string& name, Macro* m);

  static std::string GetGeneratedCallableName(
      const std::string& name, const TypeVector& specialized_types);
};

}  // namespace torque
}  // namespace internal
}  // namespace v8

#endif  // V8_TORQUE_DECLARATIONS_H_
