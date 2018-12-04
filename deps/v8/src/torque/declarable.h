// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TORQUE_DECLARABLE_H_
#define V8_TORQUE_DECLARABLE_H_

#include <cassert>
#include <string>

#include "src/base/functional.h"
#include "src/base/logging.h"
#include "src/torque/ast.h"
#include "src/torque/types.h"
#include "src/torque/utils.h"

namespace v8 {
namespace internal {
namespace torque {

class Block;
class Generic;
class Scope;
class ScopeChain;

class Declarable {
 public:
  virtual ~Declarable() = default;
  enum Kind {
    kVariable,
    kParameter,
    kMacro,
    kMacroList,
    kBuiltin,
    kRuntimeFunction,
    kGeneric,
    kGenericList,
    kTypeAlias,
    kLabel,
    kExternConstant,
    kModuleConstant
  };
  Kind kind() const { return kind_; }
  bool IsMacro() const { return kind() == kMacro; }
  bool IsBuiltin() const { return kind() == kBuiltin; }
  bool IsRuntimeFunction() const { return kind() == kRuntimeFunction; }
  bool IsGeneric() const { return kind() == kGeneric; }
  bool IsTypeAlias() const { return kind() == kTypeAlias; }
  bool IsParameter() const { return kind() == kParameter; }
  bool IsLabel() const { return kind() == kLabel; }
  bool IsVariable() const { return kind() == kVariable; }
  bool IsMacroList() const { return kind() == kMacroList; }
  bool IsGenericList() const { return kind() == kGenericList; }
  bool IsExternConstant() const { return kind() == kExternConstant; }
  bool IsModuleConstant() const { return kind() == kModuleConstant; }
  bool IsValue() const {
    return IsVariable() || IsExternConstant() || IsParameter() ||
           IsModuleConstant();
  }
  virtual const char* type_name() const { return "<<unknown>>"; }

 protected:
  explicit Declarable(Kind kind) : kind_(kind) {}

 private:
  const Kind kind_;
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

class Value : public Declarable {
 public:
  DECLARE_DECLARABLE_BOILERPLATE(Value, value);
  const std::string& name() const { return name_; }
  virtual bool IsConst() const { return true; }
  VisitResult value() const { return *value_; }
  const Type* type() const { return type_; }

  void set_value(VisitResult value) {
    DCHECK(!value_);
    value_ = value;
  }

 protected:
  Value(Kind kind, const Type* type, const std::string& name)
      : Declarable(kind), type_(type), name_(name) {}

 private:
  const Type* type_;
  std::string name_;
  base::Optional<VisitResult> value_;
};

class Parameter : public Value {
 public:
  DECLARE_DECLARABLE_BOILERPLATE(Parameter, parameter);

  const std::string& external_name() const { return external_name_; }

 private:
  friend class Declarations;
  Parameter(const std::string& name, std::string external_name,
            const Type* type)
      : Value(Declarable::kParameter, type, name),
        external_name_(external_name) {}

  std::string external_name_;
};

class ModuleConstant : public Value {
 public:
  DECLARE_DECLARABLE_BOILERPLATE(ModuleConstant, constant);

  const std::string& constant_name() const { return constant_name_; }

 private:
  friend class Declarations;
  explicit ModuleConstant(std::string constant_name, const Type* type)
      : Value(Declarable::kModuleConstant, type, constant_name),
        constant_name_(std::move(constant_name)) {}

  std::string constant_name_;
};

class Variable : public Value {
 public:
  DECLARE_DECLARABLE_BOILERPLATE(Variable, variable);
  bool IsConst() const override { return const_; }
  void Define() {
    if (defined_ && IsConst()) {
      ReportError("Cannot re-define a const-bound variable.");
    }
    defined_ = true;
  }
  bool IsDefined() const { return defined_; }

 private:
  friend class Declarations;
  Variable(std::string name, const Type* type, bool is_const)
      : Value(Declarable::kVariable, type, name),
        defined_(false),
        const_(is_const) {
    DCHECK_IMPLIES(type->IsConstexpr(), IsConst());
  }

  std::string value_;
  bool defined_;
  bool const_;
};

class Label : public Declarable {
 public:
  void AddVariable(Variable* var) { parameters_.push_back(var); }
  Block* block() const { return *block_; }
  void set_block(Block* block) {
    DCHECK(!block_);
    block_ = block;
  }
  const std::string& external_label_name() const {
    return *external_label_name_;
  }
  const std::string& name() const { return name_; }
  void set_external_label_name(std::string external_label_name) {
    DCHECK(!block_);
    DCHECK(!external_label_name_);
    external_label_name_ = std::move(external_label_name);
  }
  Variable* GetParameter(size_t i) const { return parameters_[i]; }
  size_t GetParameterCount() const { return parameters_.size(); }
  const std::vector<Variable*>& GetParameters() const { return parameters_; }

  DECLARE_DECLARABLE_BOILERPLATE(Label, label);
  void MarkUsed() { used_ = true; }
  bool IsUsed() const { return used_; }
  bool IsDeferred() const { return deferred_; }

 private:
  friend class Declarations;
  explicit Label(std::string name, bool deferred = false)
      : Declarable(Declarable::kLabel),
        name_(std::move(name)),
        used_(false),
        deferred_(deferred) {}

  std::string name_;
  base::Optional<Block*> block_;
  base::Optional<std::string> external_label_name_;
  std::vector<Variable*> parameters_;
  static size_t next_id_;
  bool used_;
  bool deferred_;
};

class ExternConstant : public Value {
 public:
  DECLARE_DECLARABLE_BOILERPLATE(ExternConstant, constant);

 private:
  friend class Declarations;
  explicit ExternConstant(std::string name, const Type* type, std::string value)
      : Value(Declarable::kExternConstant, type, std::move(name)) {
    set_value(VisitResult(type, std::move(value)));
  }
};

class Callable : public Declarable {
 public:
  static Callable* cast(Declarable* declarable) {
    assert(declarable->IsMacro() || declarable->IsBuiltin() ||
           declarable->IsRuntimeFunction());
    return static_cast<Callable*>(declarable);
  }
  static const Callable* cast(const Declarable* declarable) {
    assert(declarable->IsMacro() || declarable->IsBuiltin() ||
           declarable->IsRuntimeFunction());
    return static_cast<const Callable*>(declarable);
  }
  const std::string& name() const { return name_; }
  const Signature& signature() const { return signature_; }
  const NameVector& parameter_names() const {
    return signature_.parameter_names;
  }
  bool HasReturnValue() const {
    return !signature_.return_type->IsVoidOrNever();
  }
  void IncrementReturns() { ++returns_; }
  bool HasReturns() const { return returns_; }
  base::Optional<Generic*> generic() const { return generic_; }

 protected:
  Callable(Declarable::Kind kind, const std::string& name,
           const Signature& signature, base::Optional<Generic*> generic)
      : Declarable(kind),
        name_(name),
        signature_(signature),
        returns_(0),
        generic_(generic) {}

 private:
  std::string name_;
  Signature signature_;
  size_t returns_;
  base::Optional<Generic*> generic_;
};

class Macro : public Callable {
 public:
  DECLARE_DECLARABLE_BOILERPLATE(Macro, macro);

 private:
  friend class Declarations;
  Macro(const std::string& name, const Signature& signature,
        base::Optional<Generic*> generic)
      : Callable(Declarable::kMacro, name, signature, generic) {
    if (signature.parameter_types.var_args) {
      ReportError("Varargs are not supported for macros.");
    }
  }
};

class MacroList : public Declarable {
 public:
  DECLARE_DECLARABLE_BOILERPLATE(MacroList, macro_list);
  const std::vector<Macro*>& list() { return list_; }
  Macro* AddMacro(Macro* macro) {
    list_.emplace_back(macro);
    return macro;
  }

 private:
  friend class Declarations;
  MacroList() : Declarable(Declarable::kMacroList) {}

  std::vector<Macro*> list_;
};

class Builtin : public Callable {
 public:
  enum Kind { kStub, kFixedArgsJavaScript, kVarArgsJavaScript };
  DECLARE_DECLARABLE_BOILERPLATE(Builtin, builtin);
  Kind kind() const { return kind_; }
  bool IsStub() const { return kind_ == kStub; }
  bool IsVarArgsJavaScript() const { return kind_ == kVarArgsJavaScript; }
  bool IsFixedArgsJavaScript() const { return kind_ == kFixedArgsJavaScript; }
  bool IsExternal() const { return external_; }

 private:
  friend class Declarations;
  Builtin(const std::string& name, Builtin::Kind kind, bool external,
          const Signature& signature, base::Optional<Generic*> generic)
      : Callable(Declarable::kBuiltin, name, signature, generic),
        kind_(kind),
        external_(external) {}

  Kind kind_;
  bool external_;
};

class RuntimeFunction : public Callable {
 public:
  DECLARE_DECLARABLE_BOILERPLATE(RuntimeFunction, runtime);

 private:
  friend class Declarations;
  RuntimeFunction(const std::string& name, const Signature& signature,
                  base::Optional<Generic*> generic)
      : Callable(Declarable::kRuntimeFunction, name, signature, generic) {}
};

class Generic : public Declarable {
 public:
  DECLARE_DECLARABLE_BOILERPLATE(Generic, generic);

  GenericDeclaration* declaration() const { return declaration_; }
  const std::string& name() const { return name_; }
  Module* module() const { return module_; }

 private:
  friend class Declarations;
  Generic(const std::string& name, Module* module,
          GenericDeclaration* declaration)
      : Declarable(Declarable::kGeneric),
        name_(name),
        module_(module),
        declaration_(declaration) {}

  std::string name_;
  Module* module_;
  GenericDeclaration* declaration_;
};

class GenericList : public Declarable {
 public:
  DECLARE_DECLARABLE_BOILERPLATE(GenericList, generic_list);
  const std::vector<Generic*>& list() { return list_; }
  Generic* AddGeneric(Generic* generic) {
    list_.push_back(generic);
    return generic;
  }

 private:
  friend class Declarations;
  GenericList() : Declarable(Declarable::kGenericList) {}

  std::vector<Generic*> list_;
};

typedef std::pair<Generic*, TypeVector> SpecializationKey;

class TypeAlias : public Declarable {
 public:
  DECLARE_DECLARABLE_BOILERPLATE(TypeAlias, type_alias);

  const Type* type() const { return type_; }

 private:
  friend class Declarations;
  explicit TypeAlias(const Type* type)
      : Declarable(Declarable::kTypeAlias), type_(type) {}

  const Type* type_;
};

void PrintLabel(std::ostream& os, const Label& l, bool with_names);

std::ostream& operator<<(std::ostream& os, const Callable& m);
std::ostream& operator<<(std::ostream& os, const Variable& v);
std::ostream& operator<<(std::ostream& os, const Builtin& b);
std::ostream& operator<<(std::ostream& os, const Label& l);
std::ostream& operator<<(std::ostream& os, const RuntimeFunction& b);
std::ostream& operator<<(std::ostream& os, const Generic& g);

#undef DECLARE_DECLARABLE_BOILERPLATE

}  // namespace torque
}  // namespace internal
}  // namespace v8

#endif  // V8_TORQUE_DECLARABLE_H_
