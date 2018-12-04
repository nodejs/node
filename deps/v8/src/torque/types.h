// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TORQUE_TYPES_H_
#define V8_TORQUE_TYPES_H_

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include "src/base/optional.h"
#include "src/torque/utils.h"

namespace v8 {
namespace internal {
namespace torque {

static const char* const CONSTEXPR_TYPE_PREFIX = "constexpr ";
static const char* const NEVER_TYPE_STRING = "never";
static const char* const CONSTEXPR_BOOL_TYPE_STRING = "constexpr bool";
static const char* const BOOL_TYPE_STRING = "bool";
static const char* const VOID_TYPE_STRING = "void";
static const char* const ARGUMENTS_TYPE_STRING = "constexpr Arguments";
static const char* const CONTEXT_TYPE_STRING = "Context";
static const char* const OBJECT_TYPE_STRING = "Object";
static const char* const CONST_STRING_TYPE_STRING = "constexpr string";
static const char* const CODE_TYPE_STRING = "Code";
static const char* const INTPTR_TYPE_STRING = "intptr";
static const char* const CONST_INT31_TYPE_STRING = "constexpr int31";
static const char* const CONST_INT32_TYPE_STRING = "constexpr int32";
static const char* const CONST_FLOAT64_TYPE_STRING = "constexpr float64";

class Label;
class Value;
class Module;

class TypeBase {
 public:
  enum class Kind {
    kAbstractType,
    kFunctionPointerType,
    kUnionType,
    kStructType
  };
  virtual ~TypeBase() = default;
  bool IsAbstractType() const { return kind() == Kind::kAbstractType; }
  bool IsFunctionPointerType() const {
    return kind() == Kind::kFunctionPointerType;
  }
  bool IsUnionType() const { return kind() == Kind::kUnionType; }
  bool IsStructType() const { return kind() == Kind::kStructType; }

 protected:
  explicit TypeBase(Kind kind) : kind_(kind) {}
  Kind kind() const { return kind_; }

 private:
  const Kind kind_;
};

#define DECLARE_TYPE_BOILERPLATE(x)                         \
  static x* cast(TypeBase* declarable) {                    \
    DCHECK(declarable->Is##x());                            \
    return static_cast<x*>(declarable);                     \
  }                                                         \
  static const x* cast(const TypeBase* declarable) {        \
    DCHECK(declarable->Is##x());                            \
    return static_cast<const x*>(declarable);               \
  }                                                         \
  static x* DynamicCast(TypeBase* declarable) {             \
    if (!declarable) return nullptr;                        \
    if (!declarable->Is##x()) return nullptr;               \
    return static_cast<x*>(declarable);                     \
  }                                                         \
  static const x* DynamicCast(const TypeBase* declarable) { \
    if (!declarable) return nullptr;                        \
    if (!declarable->Is##x()) return nullptr;               \
    return static_cast<const x*>(declarable);               \
  }

class Type : public TypeBase {
 public:
  virtual bool IsSubtypeOf(const Type* supertype) const;

  std::string ToString() const;
  virtual std::string MangledName() const = 0;
  bool IsVoid() const { return IsAbstractName(VOID_TYPE_STRING); }
  bool IsNever() const { return IsAbstractName(NEVER_TYPE_STRING); }
  bool IsBool() const { return IsAbstractName(BOOL_TYPE_STRING); }
  bool IsConstexprBool() const {
    return IsAbstractName(CONSTEXPR_BOOL_TYPE_STRING);
  }
  bool IsVoidOrNever() const { return IsVoid() || IsNever(); }
  virtual std::string GetGeneratedTypeName() const = 0;
  virtual std::string GetGeneratedTNodeTypeName() const = 0;
  virtual bool IsConstexpr() const = 0;
  virtual const Type* NonConstexprVersion() const = 0;
  static const Type* CommonSupertype(const Type* a, const Type* b);
  void AddAlias(std::string alias) const { aliases_.insert(std::move(alias)); }

 protected:
  Type(TypeBase::Kind kind, const Type* parent)
      : TypeBase(kind), parent_(parent) {}
  const Type* parent() const { return parent_; }
  void set_parent(const Type* t) { parent_ = t; }
  int Depth() const;
  virtual std::string ToExplicitString() const = 0;

 private:
  bool IsAbstractName(const std::string& name) const;

  // If {parent_} is not nullptr, then this type is a subtype of {parent_}.
  const Type* parent_;
  mutable std::set<std::string> aliases_;
};

using TypeVector = std::vector<const Type*>;

struct NameAndType {
  std::string name;
  const Type* type;
};

std::ostream& operator<<(std::ostream& os, const NameAndType& name_and_type);

class AbstractType final : public Type {
 public:
  DECLARE_TYPE_BOILERPLATE(AbstractType);
  const std::string& name() const { return name_; }
  std::string ToExplicitString() const override { return name(); }
  std::string MangledName() const override { return "AT" + name(); }
  std::string GetGeneratedTypeName() const override { return generated_type_; }
  std::string GetGeneratedTNodeTypeName() const override;
  bool IsConstexpr() const override {
    return name().substr(0, strlen(CONSTEXPR_TYPE_PREFIX)) ==
           CONSTEXPR_TYPE_PREFIX;
  }
  const Type* NonConstexprVersion() const override {
    if (IsConstexpr()) return *non_constexpr_version_;
    return this;
  }

 private:
  friend class TypeOracle;
  AbstractType(const Type* parent, const std::string& name,
               const std::string& generated_type,
               base::Optional<const AbstractType*> non_constexpr_version)
      : Type(Kind::kAbstractType, parent),
        name_(name),
        generated_type_(generated_type),
        non_constexpr_version_(non_constexpr_version) {
    DCHECK_EQ(non_constexpr_version_.has_value(), IsConstexpr());
    if (parent) DCHECK(parent->IsConstexpr() == IsConstexpr());
  }

  const std::string name_;
  const std::string generated_type_;
  base::Optional<const AbstractType*> non_constexpr_version_;
};

// For now, function pointers are restricted to Code objects of Torque-defined
// builtins.
class FunctionPointerType final : public Type {
 public:
  DECLARE_TYPE_BOILERPLATE(FunctionPointerType);
  std::string ToExplicitString() const override;
  std::string MangledName() const override;
  std::string GetGeneratedTypeName() const override {
    return parent()->GetGeneratedTypeName();
  }
  std::string GetGeneratedTNodeTypeName() const override {
    return parent()->GetGeneratedTNodeTypeName();
  }
  bool IsConstexpr() const override {
    DCHECK(!parent()->IsConstexpr());
    return false;
  }
  const Type* NonConstexprVersion() const override { return this; }

  const TypeVector& parameter_types() const { return parameter_types_; }
  const Type* return_type() const { return return_type_; }

  friend size_t hash_value(const FunctionPointerType& p) {
    size_t result = base::hash_value(p.return_type_);
    for (const Type* parameter : p.parameter_types_) {
      result = base::hash_combine(result, parameter);
    }
    return result;
  }
  bool operator==(const FunctionPointerType& other) const {
    return parameter_types_ == other.parameter_types_ &&
           return_type_ == other.return_type_;
  }

 private:
  friend class TypeOracle;
  FunctionPointerType(const Type* parent, TypeVector parameter_types,
                      const Type* return_type)
      : Type(Kind::kFunctionPointerType, parent),
        parameter_types_(parameter_types),
        return_type_(return_type) {}

  const TypeVector parameter_types_;
  const Type* const return_type_;
};

bool operator<(const Type& a, const Type& b);
struct TypeLess {
  bool operator()(const Type* const a, const Type* const b) const {
    return *a < *b;
  }
};

class UnionType final : public Type {
 public:
  DECLARE_TYPE_BOILERPLATE(UnionType);
  std::string ToExplicitString() const override;
  std::string MangledName() const override;
  std::string GetGeneratedTypeName() const override {
    return "TNode<" + GetGeneratedTNodeTypeName() + ">";
  }
  std::string GetGeneratedTNodeTypeName() const override;

  bool IsConstexpr() const override {
    DCHECK_EQ(false, parent()->IsConstexpr());
    return false;
  }
  const Type* NonConstexprVersion() const override;

  friend size_t hash_value(const UnionType& p) {
    size_t result = 0;
    for (const Type* t : p.types_) {
      result = base::hash_combine(result, t);
    }
    return result;
  }
  bool operator==(const UnionType& other) const {
    return types_ == other.types_;
  }

  base::Optional<const Type*> GetSingleMember() const {
    if (types_.size() == 1) {
      DCHECK_EQ(*types_.begin(), parent());
      return *types_.begin();
    }
    return base::nullopt;
  }

  bool IsSubtypeOf(const Type* other) const override {
    for (const Type* member : types_) {
      if (!member->IsSubtypeOf(other)) return false;
    }
    return true;
  }

  bool IsSupertypeOf(const Type* other) const {
    for (const Type* member : types_) {
      if (other->IsSubtypeOf(member)) {
        return true;
      }
    }
    return false;
  }

  void Extend(const Type* t) {
    if (const UnionType* union_type = UnionType::DynamicCast(t)) {
      for (const Type* member : union_type->types_) {
        Extend(member);
      }
    } else {
      if (t->IsSubtypeOf(this)) return;
      set_parent(CommonSupertype(parent(), t));
      for (const Type* member : types_) {
        if (member->IsSubtypeOf(t)) {
          types_.erase(member);
        }
      }
      types_.insert(t);
    }
  }

  void Subtract(const Type* t);

  static UnionType FromType(const Type* t) {
    const UnionType* union_type = UnionType::DynamicCast(t);
    return union_type ? UnionType(*union_type) : UnionType(t);
  }

 private:
  explicit UnionType(const Type* t) : Type(Kind::kUnionType, t), types_({t}) {}
  void RecomputeParent();

  std::set<const Type*, TypeLess> types_;
};

const Type* SubtractType(const Type* a, const Type* b);

class StructType final : public Type {
 public:
  DECLARE_TYPE_BOILERPLATE(StructType);
  std::string ToExplicitString() const override;
  std::string MangledName() const override { return name_; }
  std::string GetGeneratedTypeName() const override { return GetStructName(); }
  std::string GetGeneratedTNodeTypeName() const override { UNREACHABLE(); }
  const Type* NonConstexprVersion() const override { return this; }

  bool IsConstexpr() const override { return false; }

  const std::vector<NameAndType>& fields() const { return fields_; }
  const Type* GetFieldType(const std::string& fieldname) const {
    for (const NameAndType& field : fields()) {
      if (field.name == fieldname) return field.type;
    }
    std::stringstream s;
    s << "\"" << fieldname << "\" is not a field of struct type \"" << name()
      << "\"";
    ReportError(s.str());
  }
  const std::string& name() const { return name_; }
  Module* module() const { return module_; }

 private:
  friend class TypeOracle;
  StructType(Module* module, const std::string& name,
             const std::vector<NameAndType>& fields)
      : Type(Kind::kStructType, nullptr),
        module_(module),
        name_(name),
        fields_(fields) {}

  const std::string& GetStructName() const { return name_; }

  Module* module_;
  std::string name_;
  std::vector<NameAndType> fields_;
};

inline std::ostream& operator<<(std::ostream& os, const Type& t) {
  os << t.ToString();
  return os;
}

class VisitResult {
 public:
  VisitResult() = default;
  VisitResult(const Type* type, const std::string& constexpr_value)
      : type_(type), constexpr_value_(constexpr_value) {
    DCHECK(type->IsConstexpr());
  }
  static VisitResult NeverResult();
  VisitResult(const Type* type, StackRange stack_range)
      : type_(type), stack_range_(stack_range) {
    DCHECK(!type->IsConstexpr());
  }
  const Type* type() const { return type_; }
  const std::string& constexpr_value() const { return *constexpr_value_; }
  const StackRange& stack_range() const { return *stack_range_; }
  void SetType(const Type* new_type) { type_ = new_type; }
  bool IsOnStack() const { return stack_range_ != base::nullopt; }
  bool operator==(const VisitResult& other) const {
    return type_ == other.type_ && constexpr_value_ == other.constexpr_value_ &&
           stack_range_ == other.stack_range_;
  }

 private:
  const Type* type_ = nullptr;
  base::Optional<std::string> constexpr_value_;
  base::Optional<StackRange> stack_range_;
};

VisitResult ProjectStructField(VisitResult structure,
                               const std::string& fieldname);

class VisitResultVector : public std::vector<VisitResult> {
 public:
  VisitResultVector() : std::vector<VisitResult>() {}
  VisitResultVector(std::initializer_list<VisitResult> init)
      : std::vector<VisitResult>(init) {}
  TypeVector GetTypeVector() const {
    TypeVector result;
    for (auto& visit_result : *this) {
      result.push_back(visit_result.type());
    }
    return result;
  }
};

std::ostream& operator<<(std::ostream& os, const TypeVector& types);

typedef std::vector<NameAndType> NameAndTypeVector;

struct LabelDefinition {
  std::string name;
  NameAndTypeVector parameters;
};

typedef std::vector<LabelDefinition> LabelDefinitionVector;

struct LabelDeclaration {
  std::string name;
  TypeVector types;
};

typedef std::vector<LabelDeclaration> LabelDeclarationVector;

struct ParameterTypes {
  TypeVector types;
  bool var_args;
};

std::ostream& operator<<(std::ostream& os, const ParameterTypes& parameters);

struct Signature {
  const TypeVector& types() const { return parameter_types.types; }
  NameVector parameter_names;
  ParameterTypes parameter_types;
  const Type* return_type;
  LabelDeclarationVector labels;
  bool HasSameTypesAs(const Signature& other) const;
};

struct Arguments {
  VisitResultVector parameters;
  std::vector<Label*> labels;
};

void PrintSignature(std::ostream& os, const Signature& sig, bool with_names);
std::ostream& operator<<(std::ostream& os, const Signature& sig);

bool IsAssignableFrom(const Type* to, const Type* from);
bool IsCompatibleSignature(const Signature& sig, const TypeVector& types,
                           const std::vector<Label*>& labels);

TypeVector LowerType(const Type* type);
size_t LoweredSlotCount(const Type* type);
TypeVector LowerParameterTypes(const TypeVector& parameters);
TypeVector LowerParameterTypes(const ParameterTypes& parameter_types,
                               size_t vararg_count = 0);

}  // namespace torque
}  // namespace internal
}  // namespace v8

#endif  // V8_TORQUE_TYPES_H_
