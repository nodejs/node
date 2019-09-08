// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TORQUE_DECLARABLE_H_
#define V8_TORQUE_DECLARABLE_H_

#include <cassert>
#include <string>
#include <unordered_map>

#include "src/base/functional.h"
#include "src/base/logging.h"
#include "src/torque/ast.h"
#include "src/torque/types.h"
#include "src/torque/utils.h"

namespace v8 {
namespace internal {
namespace torque {

class Scope;
class Namespace;
class TypeArgumentInference;

DECLARE_CONTEXTUAL_VARIABLE(CurrentScope, Scope*);

struct QualifiedName {
  std::vector<std::string> namespace_qualification;
  std::string name;

  QualifiedName(std::vector<std::string> namespace_qualification,
                std::string name)
      : namespace_qualification(std::move(namespace_qualification)),
        name(std::move(name)) {}
  explicit QualifiedName(std::string name)
      : QualifiedName({}, std::move(name)) {}

  friend std::ostream& operator<<(std::ostream& os, const QualifiedName& name);
};

class Declarable {
 public:
  virtual ~Declarable() = default;
  enum Kind {
    kNamespace,
    kTorqueMacro,
    kExternMacro,
    kMethod,
    kBuiltin,
    kRuntimeFunction,
    kIntrinsic,
    kGeneric,
    kGenericStructType,
    kTypeAlias,
    kExternConstant,
    kNamespaceConstant
  };
  Kind kind() const { return kind_; }
  bool IsNamespace() const { return kind() == kNamespace; }
  bool IsMacro() const { return IsTorqueMacro() || IsExternMacro(); }
  bool IsTorqueMacro() const { return kind() == kTorqueMacro || IsMethod(); }
  bool IsMethod() const { return kind() == kMethod; }
  bool IsExternMacro() const { return kind() == kExternMacro; }
  bool IsIntrinsic() const { return kind() == kIntrinsic; }
  bool IsBuiltin() const { return kind() == kBuiltin; }
  bool IsRuntimeFunction() const { return kind() == kRuntimeFunction; }
  bool IsGeneric() const { return kind() == kGeneric; }
  bool IsGenericStructType() const { return kind() == kGenericStructType; }
  bool IsTypeAlias() const { return kind() == kTypeAlias; }
  bool IsExternConstant() const { return kind() == kExternConstant; }
  bool IsNamespaceConstant() const { return kind() == kNamespaceConstant; }
  bool IsValue() const { return IsExternConstant() || IsNamespaceConstant(); }
  bool IsScope() const { return IsNamespace() || IsCallable(); }
  bool IsCallable() const {
    return IsMacro() || IsBuiltin() || IsRuntimeFunction() || IsIntrinsic() ||
           IsMethod();
  }
  virtual const char* type_name() const { return "<<unknown>>"; }
  Scope* ParentScope() const { return parent_scope_; }

  // The SourcePosition of the whole declarable. For example, for a macro
  // this will encompass not only the signature, but also the body.
  SourcePosition Position() const { return position_; }
  void SetPosition(const SourcePosition& position) { position_ = position; }

  // The SourcePosition of the identifying name of the declarable. For example,
  // for a macro this will be the SourcePosition of the name.
  // Note that this SourcePosition might not make sense for all kinds of
  // declarables, in that case, the default SourcePosition is returned.
  SourcePosition IdentifierPosition() const {
    return identifier_position_.source.IsValid() ? identifier_position_
                                                 : position_;
  }
  void SetIdentifierPosition(const SourcePosition& position) {
    identifier_position_ = position;
  }

  bool IsUserDefined() const { return is_user_defined_; }
  void SetIsUserDefined(bool is_user_defined) {
    is_user_defined_ = is_user_defined;
  }

 protected:
  explicit Declarable(Kind kind) : kind_(kind) {}

 private:
  const Kind kind_;
  Scope* const parent_scope_ = CurrentScope::Get();
  SourcePosition position_ = CurrentSourcePosition::Get();
  SourcePosition identifier_position_ = SourcePosition::Invalid();
  bool is_user_defined_ = true;
};

#define DECLARE_DECLARABLE_BOILERPLATE(x, y)                  \
  static x* cast(Declarable* declarable) {                    \
    DCHECK(declarable->Is##x());                              \
    return static_cast<x*>(declarable);                       \
  }                                                           \
  static const x* cast(const Declarable* declarable) {        \
    DCHECK(declarable->Is##x());                              \
    return static_cast<const x*>(declarable);                 \
  }                                                           \
  const char* type_name() const override { return #y; }       \
  static x* DynamicCast(Declarable* declarable) {             \
    if (!declarable) return nullptr;                          \
    if (!declarable->Is##x()) return nullptr;                 \
    return static_cast<x*>(declarable);                       \
  }                                                           \
  static const x* DynamicCast(const Declarable* declarable) { \
    if (!declarable) return nullptr;                          \
    if (!declarable->Is##x()) return nullptr;                 \
    return static_cast<const x*>(declarable);                 \
  }

class Scope : public Declarable {
 public:
  DECLARE_DECLARABLE_BOILERPLATE(Scope, scope)
  explicit Scope(Declarable::Kind kind) : Declarable(kind) {}

  std::vector<Declarable*> LookupShallow(const QualifiedName& name) {
    if (name.namespace_qualification.empty()) return declarations_[name.name];
    Scope* child = nullptr;
    for (Declarable* declarable :
         declarations_[name.namespace_qualification.front()]) {
      if (Scope* scope = Scope::DynamicCast(declarable)) {
        if (child != nullptr) {
          ReportError("ambiguous reference to scope ",
                      name.namespace_qualification.front());
        }
        child = scope;
      }
    }
    if (child == nullptr) return {};
    return child->LookupShallow(
        QualifiedName({name.namespace_qualification.begin() + 1,
                       name.namespace_qualification.end()},
                      name.name));
  }

  std::vector<Declarable*> Lookup(const QualifiedName& name) {
    std::vector<Declarable*> result;
    if (ParentScope()) {
      result = ParentScope()->Lookup(name);
    }
    for (Declarable* declarable : LookupShallow(name)) {
      result.push_back(declarable);
    }
    return result;
  }
  template <class T>
  T* AddDeclarable(const std::string& name, T* declarable) {
    declarations_[name].push_back(declarable);
    return declarable;
  }

 private:
  std::unordered_map<std::string, std::vector<Declarable*>> declarations_;
};

class Namespace : public Scope {
 public:
  DECLARE_DECLARABLE_BOILERPLATE(Namespace, namespace)
  explicit Namespace(const std::string& name)
      : Scope(Declarable::kNamespace), name_(name) {}
  const std::string& name() const { return name_; }
  bool IsDefaultNamespace() const;
  bool IsTestNamespace() const;

 private:
  std::string name_;
};

inline Namespace* CurrentNamespace() {
  Scope* scope = CurrentScope::Get();
  while (true) {
    if (Namespace* n = Namespace::DynamicCast(scope)) {
      return n;
    }
    scope = scope->ParentScope();
  }
}

class Value : public Declarable {
 public:
  DECLARE_DECLARABLE_BOILERPLATE(Value, value)
  const Identifier* name() const { return name_; }
  virtual bool IsConst() const { return true; }
  VisitResult value() const { return *value_; }
  const Type* type() const { return type_; }

  void set_value(VisitResult value) {
    DCHECK(!value_);
    value_ = value;
  }

 protected:
  Value(Kind kind, const Type* type, Identifier* name)
      : Declarable(kind), type_(type), name_(name) {}

 private:
  const Type* type_;
  Identifier* name_;
  base::Optional<VisitResult> value_;
};

class NamespaceConstant : public Value {
 public:
  DECLARE_DECLARABLE_BOILERPLATE(NamespaceConstant, constant)

  const std::string& external_name() const { return external_name_; }
  Expression* body() const { return body_; }

 private:
  friend class Declarations;
  explicit NamespaceConstant(Identifier* constant_name,
                             std::string external_name, const Type* type,
                             Expression* body)
      : Value(Declarable::kNamespaceConstant, type, constant_name),
        external_name_(std::move(external_name)),
        body_(body) {}

  std::string external_name_;
  Expression* body_;
};

class ExternConstant : public Value {
 public:
  DECLARE_DECLARABLE_BOILERPLATE(ExternConstant, constant)

 private:
  friend class Declarations;
  explicit ExternConstant(Identifier* name, const Type* type, std::string value)
      : Value(Declarable::kExternConstant, type, name) {
    set_value(VisitResult(type, std::move(value)));
  }
};

class Callable : public Scope {
 public:
  DECLARE_DECLARABLE_BOILERPLATE(Callable, callable)
  const std::string& ExternalName() const { return external_name_; }
  const std::string& ReadableName() const { return readable_name_; }
  const Signature& signature() const { return signature_; }
  bool IsTransitioning() const { return signature().transitioning; }
  const NameVector& parameter_names() const {
    return signature_.parameter_names;
  }
  bool HasReturnValue() const {
    return !signature_.return_type->IsVoidOrNever();
  }
  void IncrementReturns() { ++returns_; }
  bool HasReturns() const { return returns_; }
  base::Optional<Statement*> body() const { return body_; }
  bool IsExternal() const { return !body_.has_value(); }
  virtual bool ShouldBeInlined() const { return false; }
  virtual bool ShouldGenerateExternalCode() const { return !ShouldBeInlined(); }

 protected:
  Callable(Declarable::Kind kind, std::string external_name,
           std::string readable_name, Signature signature,
           base::Optional<Statement*> body)
      : Scope(kind),
        external_name_(std::move(external_name)),

        readable_name_(std::move(readable_name)),
        signature_(std::move(signature)),
        returns_(0),
        body_(body) {
    DCHECK(!body || *body);
  }

 private:
  std::string external_name_;
  std::string readable_name_;
  Signature signature_;
  size_t returns_;
  base::Optional<Statement*> body_;
};

class Macro : public Callable {
 public:
  DECLARE_DECLARABLE_BOILERPLATE(Macro, macro)
  bool ShouldBeInlined() const override {
    for (const LabelDeclaration& label : signature().labels) {
      for (const Type* type : label.types) {
        if (type->IsStructType()) return true;
      }
    }
    // Intrinsics that are used internally in Torque and implemented as torque
    // code should be inlined and not generate C++ definitions.
    if (ReadableName()[0] == '%') return true;
    return Callable::ShouldBeInlined();
  }

  void SetUsed() { used_ = true; }
  bool IsUsed() const { return used_; }

 protected:
  Macro(Declarable::Kind kind, std::string external_name,
        std::string readable_name, const Signature& signature,
        base::Optional<Statement*> body)
      : Callable(kind, std::move(external_name), std::move(readable_name),
                 signature, body),
        used_(false) {
    if (signature.parameter_types.var_args) {
      ReportError("Varargs are not supported for macros.");
    }
  }

 private:
  bool used_;
};

class ExternMacro : public Macro {
 public:
  DECLARE_DECLARABLE_BOILERPLATE(ExternMacro, ExternMacro)

  const std::string& external_assembler_name() const {
    return external_assembler_name_;
  }

 private:
  friend class Declarations;
  ExternMacro(const std::string& name, std::string external_assembler_name,
              Signature signature)
      : Macro(Declarable::kExternMacro, name, name, std::move(signature),
              base::nullopt),
        external_assembler_name_(std::move(external_assembler_name)) {}

  std::string external_assembler_name_;
};

class TorqueMacro : public Macro {
 public:
  DECLARE_DECLARABLE_BOILERPLATE(TorqueMacro, TorqueMacro)
  bool IsExportedToCSA() const { return exported_to_csa_; }

 protected:
  TorqueMacro(Declarable::Kind kind, std::string external_name,
              std::string readable_name, const Signature& signature,
              base::Optional<Statement*> body, bool is_user_defined,
              bool exported_to_csa)
      : Macro(kind, std::move(external_name), std::move(readable_name),
              signature, body),
        exported_to_csa_(exported_to_csa) {
    SetIsUserDefined(is_user_defined);
  }

 private:
  friend class Declarations;
  TorqueMacro(std::string external_name, std::string readable_name,
              const Signature& signature, base::Optional<Statement*> body,
              bool is_user_defined, bool exported_to_csa)
      : TorqueMacro(Declarable::kTorqueMacro, std::move(external_name),
                    std::move(readable_name), signature, body, is_user_defined,
                    exported_to_csa) {}

  bool exported_to_csa_ = false;
};

class Method : public TorqueMacro {
 public:
  DECLARE_DECLARABLE_BOILERPLATE(Method, Method)
  bool ShouldBeInlined() const override {
    return Macro::ShouldBeInlined() ||
           signature()
               .parameter_types.types[signature().implicit_count]
               ->IsStructType();
  }
  AggregateType* aggregate_type() const { return aggregate_type_; }

 private:
  friend class Declarations;
  Method(AggregateType* aggregate_type, std::string external_name,
         std::string readable_name, const Signature& signature, Statement* body)
      : TorqueMacro(Declarable::kMethod, std::move(external_name),
                    std::move(readable_name), signature, body, true, false),
        aggregate_type_(aggregate_type) {}
  AggregateType* aggregate_type_;
};

class Builtin : public Callable {
 public:
  enum Kind { kStub, kFixedArgsJavaScript, kVarArgsJavaScript };
  DECLARE_DECLARABLE_BOILERPLATE(Builtin, builtin)
  Kind kind() const { return kind_; }
  bool IsStub() const { return kind_ == kStub; }
  bool IsVarArgsJavaScript() const { return kind_ == kVarArgsJavaScript; }
  bool IsFixedArgsJavaScript() const { return kind_ == kFixedArgsJavaScript; }

 private:
  friend class Declarations;
  Builtin(std::string external_name, std::string readable_name,
          Builtin::Kind kind, const Signature& signature,
          base::Optional<Statement*> body)
      : Callable(Declarable::kBuiltin, std::move(external_name),
                 std::move(readable_name), signature, body),
        kind_(kind) {}

  Kind kind_;
};

class RuntimeFunction : public Callable {
 public:
  DECLARE_DECLARABLE_BOILERPLATE(RuntimeFunction, runtime)

 private:
  friend class Declarations;
  RuntimeFunction(const std::string& name, const Signature& signature)
      : Callable(Declarable::kRuntimeFunction, name, name, signature,
                 base::nullopt) {}
};

class Intrinsic : public Callable {
 public:
  DECLARE_DECLARABLE_BOILERPLATE(Intrinsic, intrinsic)

 private:
  friend class Declarations;
  Intrinsic(std::string name, const Signature& signature)
      : Callable(Declarable::kIntrinsic, name, name, signature, base::nullopt) {
    if (signature.parameter_types.var_args) {
      ReportError("Varargs are not supported for intrinsics.");
    }
  }
};

template <class T>
class SpecializationMap {
 private:
  using Map = std::unordered_map<TypeVector, T*, base::hash<TypeVector>>;

 public:
  SpecializationMap() {}

  void Add(const TypeVector& type_arguments, T* specialization) {
    DCHECK_EQ(0, specializations_.count(type_arguments));
    specializations_[type_arguments] = specialization;
  }
  base::Optional<T*> Get(const TypeVector& type_arguments) const {
    auto it = specializations_.find(type_arguments);
    if (it != specializations_.end()) return it->second;
    return base::nullopt;
  }

  using iterator = typename Map::const_iterator;
  iterator begin() const { return specializations_.begin(); }
  iterator end() const { return specializations_.end(); }

 private:
  Map specializations_;
};

class Generic : public Declarable {
 public:
  DECLARE_DECLARABLE_BOILERPLATE(Generic, generic)

  const std::string& name() const { return name_; }
  CallableDeclaration* declaration() const {
    return generic_declaration_->declaration;
  }
  const std::vector<Identifier*> generic_parameters() const {
    return generic_declaration_->generic_parameters;
  }
  SpecializationMap<Callable>& specializations() { return specializations_; }

  base::Optional<Statement*> CallableBody();

  TypeArgumentInference InferSpecializationTypes(
      const TypeVector& explicit_specialization_types,
      const TypeVector& arguments);

 private:
  friend class Declarations;
  Generic(const std::string& name, GenericDeclaration* generic_declaration)
      : Declarable(Declarable::kGeneric),
        name_(name),
        generic_declaration_(generic_declaration) {}

  std::string name_;
  GenericDeclaration* generic_declaration_;
  SpecializationMap<Callable> specializations_;
};

class GenericStructType : public Declarable {
 public:
  DECLARE_DECLARABLE_BOILERPLATE(GenericStructType, generic_type)
  const std::string& name() const { return name_; }
  StructDeclaration* declaration() const { return declaration_; }
  const std::vector<Identifier*>& generic_parameters() const {
    return declaration_->generic_parameters;
  }
  SpecializationMap<const StructType>& specializations() {
    return specializations_;
  }

 private:
  friend class Declarations;
  GenericStructType(const std::string& name, StructDeclaration* declaration)
      : Declarable(Declarable::kGenericStructType),
        name_(name),
        declaration_(declaration) {
    DCHECK_GT(declaration->generic_parameters.size(), 0);
  }

  std::string name_;
  StructDeclaration* declaration_;
  SpecializationMap<const StructType> specializations_;
};

class TypeAlias : public Declarable {
 public:
  DECLARE_DECLARABLE_BOILERPLATE(TypeAlias, type_alias)

  const Type* type() const {
    if (type_) return *type_;
    return Resolve();
  }
  const Type* Resolve() const;
  bool IsRedeclaration() const { return redeclaration_; }
  SourcePosition GetDeclarationPosition() const {
    return declaration_position_;
  }

 private:
  friend class Declarations;
  friend class TypeVisitor;

  explicit TypeAlias(
      const Type* type, bool redeclaration,
      SourcePosition declaration_position = SourcePosition::Invalid())
      : Declarable(Declarable::kTypeAlias),
        type_(type),
        redeclaration_(redeclaration),
        declaration_position_(declaration_position) {}
  explicit TypeAlias(
      TypeDeclaration* type, bool redeclaration,
      SourcePosition declaration_position = SourcePosition::Invalid())
      : Declarable(Declarable::kTypeAlias),
        delayed_(type),
        redeclaration_(redeclaration),
        declaration_position_(declaration_position) {}

  mutable bool being_resolved_ = false;
  mutable base::Optional<TypeDeclaration*> delayed_;
  mutable base::Optional<const Type*> type_;
  bool redeclaration_;
  const SourcePosition declaration_position_;
};

std::ostream& operator<<(std::ostream& os, const Callable& m);
std::ostream& operator<<(std::ostream& os, const Builtin& b);
std::ostream& operator<<(std::ostream& os, const RuntimeFunction& b);
std::ostream& operator<<(std::ostream& os, const Generic& g);

#undef DECLARE_DECLARABLE_BOILERPLATE

}  // namespace torque
}  // namespace internal
}  // namespace v8

#endif  // V8_TORQUE_DECLARABLE_H_
