// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TORQUE_TYPES_H_
#define V8_TORQUE_TYPES_H_

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "src/base/optional.h"
#include "src/torque/ast.h"
#include "src/torque/constants.h"
#include "src/torque/source-positions.h"
#include "src/torque/utils.h"

namespace v8 {
namespace internal {
namespace torque {

class AggregateType;
struct Identifier;
class Macro;
class Method;
class GenericStructType;
class StructType;
class ClassType;
class Value;
class Namespace;

class TypeBase {
 public:
  enum class Kind {
    kTopType,
    kAbstractType,
    kBuiltinPointerType,
    kUnionType,
    kStructType,
    kClassType
  };
  virtual ~TypeBase() = default;
  bool IsTopType() const { return kind() == Kind::kTopType; }
  bool IsAbstractType() const { return kind() == Kind::kAbstractType; }
  bool IsBuiltinPointerType() const {
    return kind() == Kind::kBuiltinPointerType;
  }
  bool IsUnionType() const { return kind() == Kind::kUnionType; }
  bool IsStructType() const { return kind() == Kind::kStructType; }
  bool IsClassType() const { return kind() == Kind::kClassType; }
  bool IsAggregateType() const { return IsStructType() || IsClassType(); }

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

class V8_EXPORT_PRIVATE Type : public TypeBase {
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
  std::string GetGeneratedTypeName() const;
  std::string GetGeneratedTNodeTypeName() const;
  virtual bool IsConstexpr() const {
    if (parent()) DCHECK(!parent()->IsConstexpr());
    return false;
  }
  virtual bool IsTransient() const { return false; }
  virtual const Type* NonConstexprVersion() const { return this; }
  virtual const Type* ConstexprVersion() const { return nullptr; }
  base::Optional<const ClassType*> ClassSupertype() const;
  virtual std::vector<std::string> GetRuntimeTypes() const { return {}; }
  static const Type* CommonSupertype(const Type* a, const Type* b);
  void AddAlias(std::string alias) const { aliases_.insert(std::move(alias)); }

 protected:
  Type(TypeBase::Kind kind, const Type* parent)
      : TypeBase(kind), parent_(parent) {}
  const Type* parent() const { return parent_; }
  void set_parent(const Type* t) { parent_ = t; }
  int Depth() const;
  virtual std::string ToExplicitString() const = 0;
  virtual std::string GetGeneratedTypeNameImpl() const = 0;
  virtual std::string GetGeneratedTNodeTypeNameImpl() const = 0;

 private:
  bool IsAbstractName(const std::string& name) const;

  // If {parent_} is not nullptr, then this type is a subtype of {parent_}.
  const Type* parent_;
  mutable std::set<std::string> aliases_;
};

using TypeVector = std::vector<const Type*>;

inline size_t hash_value(const TypeVector& types) {
  size_t hash = 0;
  for (const Type* t : types) {
    hash = base::hash_combine(hash, t);
  }
  return hash;
}

struct NameAndType {
  std::string name;
  const Type* type;
};

std::ostream& operator<<(std::ostream& os, const NameAndType& name_and_type);

template <typename T>
struct SpecializationKey {
  T* generic;
  TypeVector specialized_types;
};

struct Field {
  // TODO(danno): This likely should be refactored, the handling of the types
  // using the universal grab-bag utility with std::tie, as well as the
  // reliance of string types is quite clunky.
  std::tuple<size_t, std::string> GetFieldSizeInformation() const;

  SourcePosition pos;
  const AggregateType* aggregate;
  base::Optional<NameAndType> index;
  NameAndType name_and_type;
  size_t offset;
  bool is_weak;
  bool const_qualified;
  bool generate_verify;
};

std::ostream& operator<<(std::ostream& os, const Field& name_and_type);

class TopType final : public Type {
 public:
  DECLARE_TYPE_BOILERPLATE(TopType)
  std::string MangledName() const override { return "top"; }
  std::string GetGeneratedTypeNameImpl() const override { UNREACHABLE(); }
  std::string GetGeneratedTNodeTypeNameImpl() const override {
    return source_type_->GetGeneratedTNodeTypeName();
  }
  std::string ToExplicitString() const override {
    std::stringstream s;
    s << "inaccessible " + source_type_->ToString();
    return s.str();
  }

  const Type* source_type() const { return source_type_; }
  const std::string reason() const { return reason_; }

 private:
  friend class TypeOracle;
  explicit TopType(std::string reason, const Type* source_type)
      : Type(Kind::kTopType, nullptr),
        reason_(std::move(reason)),
        source_type_(source_type) {}
  std::string reason_;
  const Type* source_type_;
};

class AbstractType final : public Type {
 public:
  DECLARE_TYPE_BOILERPLATE(AbstractType)
  const std::string& name() const { return name_; }
  std::string ToExplicitString() const override { return name(); }
  std::string MangledName() const override {
    std::string str(name());
    std::replace(str.begin(), str.end(), ' ', '_');
    return "AT" + str;
  }
  std::string GetGeneratedTypeNameImpl() const override {
    return IsConstexpr() ? generated_type_ : "TNode<" + generated_type_ + ">";
  }
  std::string GetGeneratedTNodeTypeNameImpl() const override;
  bool IsConstexpr() const override {
    bool is_constexpr = non_constexpr_version_ != nullptr;
    DCHECK_EQ(is_constexpr, IsConstexprName(name()));
    return is_constexpr;
  }

  const Type* NonConstexprVersion() const override {
    if (non_constexpr_version_) return non_constexpr_version_;
    if (!IsConstexpr()) return this;
    return nullptr;
  }

  const AbstractType* ConstexprVersion() const override {
    if (constexpr_version_) return constexpr_version_;
    if (IsConstexpr()) return this;
    return nullptr;
  }

  std::vector<std::string> GetRuntimeTypes() const override { return {name()}; }

 private:
  friend class TypeOracle;
  AbstractType(const Type* parent, bool transient, const std::string& name,
               const std::string& generated_type,
               const Type* non_constexpr_version)
      : Type(Kind::kAbstractType, parent),
        transient_(transient),
        name_(name),
        generated_type_(generated_type),
        non_constexpr_version_(non_constexpr_version) {
    if (parent) DCHECK(parent->IsConstexpr() == IsConstexpr());
    DCHECK_EQ(!IsConstexprName(name), non_constexpr_version == nullptr);
    DCHECK_IMPLIES(IsConstexprName(name),
                   !non_constexpr_version->IsConstexpr());
  }

  void SetConstexprVersion(const AbstractType* type) const {
    DCHECK_EQ(GetConstexprName(name()), type->name());
    constexpr_version_ = type;
  }

  bool IsTransient() const override { return transient_; }

  bool transient_;
  const std::string name_;
  const std::string generated_type_;
  const Type* non_constexpr_version_;
  mutable const AbstractType* constexpr_version_ = nullptr;
};

// For now, builtin pointers are restricted to Torque-defined builtins.
class V8_EXPORT_PRIVATE BuiltinPointerType final : public Type {
 public:
  DECLARE_TYPE_BOILERPLATE(BuiltinPointerType)
  std::string ToExplicitString() const override;
  std::string MangledName() const override;
  std::string GetGeneratedTypeNameImpl() const override {
    return parent()->GetGeneratedTypeName();
  }
  std::string GetGeneratedTNodeTypeNameImpl() const override {
    return parent()->GetGeneratedTNodeTypeName();
  }

  const TypeVector& parameter_types() const { return parameter_types_; }
  const Type* return_type() const { return return_type_; }

  friend size_t hash_value(const BuiltinPointerType& p) {
    size_t result = base::hash_value(p.return_type_);
    for (const Type* parameter : p.parameter_types_) {
      result = base::hash_combine(result, parameter);
    }
    return result;
  }
  bool operator==(const BuiltinPointerType& other) const {
    return parameter_types_ == other.parameter_types_ &&
           return_type_ == other.return_type_;
  }
  size_t function_pointer_type_id() const { return function_pointer_type_id_; }

  std::vector<std::string> GetRuntimeTypes() const override { return {"Smi"}; }

 private:
  friend class TypeOracle;
  BuiltinPointerType(const Type* parent, TypeVector parameter_types,
                     const Type* return_type, size_t function_pointer_type_id)
      : Type(Kind::kBuiltinPointerType, parent),
        parameter_types_(parameter_types),
        return_type_(return_type),
        function_pointer_type_id_(function_pointer_type_id) {}

  const TypeVector parameter_types_;
  const Type* const return_type_;
  const size_t function_pointer_type_id_;
};

bool operator<(const Type& a, const Type& b);
struct TypeLess {
  bool operator()(const Type* const a, const Type* const b) const {
    return *a < *b;
  }
};

class V8_EXPORT_PRIVATE UnionType final : public Type {
 public:
  DECLARE_TYPE_BOILERPLATE(UnionType)
  std::string ToExplicitString() const override;
  std::string MangledName() const override;
  std::string GetGeneratedTypeNameImpl() const override {
    return "TNode<" + GetGeneratedTNodeTypeName() + ">";
  }
  std::string GetGeneratedTNodeTypeNameImpl() const override;

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

  bool IsTransient() const override {
    for (const Type* member : types_) {
      if (member->IsTransient()) {
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
      EraseIf(&types_,
              [&](const Type* member) { return member->IsSubtypeOf(t); });
      types_.insert(t);
    }
  }

  void Subtract(const Type* t);

  static UnionType FromType(const Type* t) {
    const UnionType* union_type = UnionType::DynamicCast(t);
    return union_type ? UnionType(*union_type) : UnionType(t);
  }

  std::vector<std::string> GetRuntimeTypes() const override {
    std::vector<std::string> result;
    for (const Type* member : types_) {
      std::vector<std::string> sub_result = member->GetRuntimeTypes();
      result.insert(result.end(), sub_result.begin(), sub_result.end());
    }
    return result;
  }

 private:
  explicit UnionType(const Type* t) : Type(Kind::kUnionType, t), types_({t}) {}
  void RecomputeParent();

  std::set<const Type*, TypeLess> types_;
};

const Type* SubtractType(const Type* a, const Type* b);

class AggregateType : public Type {
 public:
  DECLARE_TYPE_BOILERPLATE(AggregateType)
  std::string MangledName() const override { return name_; }
  std::string GetGeneratedTypeNameImpl() const override { UNREACHABLE(); }
  std::string GetGeneratedTNodeTypeNameImpl() const override { UNREACHABLE(); }

  virtual void Finalize() const = 0;

  virtual bool HasIndexedField() const { return false; }

  void SetFields(std::vector<Field> fields) { fields_ = std::move(fields); }
  const std::vector<Field>& fields() const {
    if (!is_finalized_) Finalize();
    return fields_;
  }
  bool HasField(const std::string& name) const;
  const Field& LookupField(const std::string& name) const;
  const std::string& name() const { return name_; }
  Namespace* nspace() const { return namespace_; }

  std::string GetGeneratedMethodName(const std::string& name) const {
    return "_method_" + name_ + "_" + name;
  }

  virtual const Field& RegisterField(Field field) {
    fields_.push_back(field);
    return fields_.back();
  }

  void RegisterMethod(Method* method) { methods_.push_back(method); }
  const std::vector<Method*>& Methods() const {
    if (!is_finalized_) Finalize();
    return methods_;
  }
  std::vector<Method*> Methods(const std::string& name) const;

  std::vector<const AggregateType*> GetHierarchy() const;
  std::vector<std::string> GetRuntimeTypes() const override { return {name_}; }

 protected:
  AggregateType(Kind kind, const Type* parent, Namespace* nspace,
                const std::string& name)
      : Type(kind, parent),
        is_finalized_(false),
        namespace_(nspace),
        name_(name) {}

  void CheckForDuplicateFields() const;
  // Use this lookup if you do not want to trigger finalization on this type.
  const Field& LookupFieldInternal(const std::string& name) const;

 protected:
  mutable bool is_finalized_;
  std::vector<Field> fields_;

 private:
  Namespace* namespace_;
  std::string name_;
  std::vector<Method*> methods_;
};

class StructType final : public AggregateType {
 public:
  DECLARE_TYPE_BOILERPLATE(StructType)

  using MaybeSpecializationKey =
      base::Optional<SpecializationKey<GenericStructType>>;

  std::string ToExplicitString() const override;
  std::string GetGeneratedTypeNameImpl() const override;
  std::string MangledName() const override;
  const MaybeSpecializationKey& GetSpecializedFrom() const {
    return specialized_from_;
  }

  static base::Optional<const Type*> MatchUnaryGeneric(
      const Type* type, GenericStructType* generic);
  static base::Optional<const Type*> MatchUnaryGeneric(
      const StructType* type, GenericStructType* generic);

 private:
  friend class TypeOracle;
  StructType(Namespace* nspace, const StructDeclaration* decl,
             MaybeSpecializationKey specialized_from = base::nullopt)
      : AggregateType(Kind::kStructType, nullptr, nspace,
                      ComputeName(decl->name->value, specialized_from)),
        decl_(decl),
        specialized_from_(specialized_from) {}

  void Finalize() const override;

  static std::string ComputeName(const std::string& basename,
                                 MaybeSpecializationKey specialized_from);

  const StructDeclaration* decl_;
  MaybeSpecializationKey specialized_from_;
};

class TypeAlias;

class ClassType final : public AggregateType {
 public:
  static constexpr ClassFlags kInternalFlags = ClassFlag::kHasIndexedField;

  DECLARE_TYPE_BOILERPLATE(ClassType)
  std::string ToExplicitString() const override;
  std::string GetGeneratedTypeNameImpl() const override;
  std::string GetGeneratedTNodeTypeNameImpl() const override;
  bool IsExtern() const { return flags_ & ClassFlag::kExtern; }
  bool ShouldGeneratePrint() const {
    return (flags_ & ClassFlag::kGeneratePrint || !IsExtern()) &&
           !HasUndefinedLayout();
  }
  bool ShouldGenerateVerify() const {
    return (flags_ & ClassFlag::kGenerateVerify || !IsExtern()) &&
           !HasUndefinedLayout();
  }
  bool IsTransient() const override { return flags_ & ClassFlag::kTransient; }
  bool IsAbstract() const { return flags_ & ClassFlag::kAbstract; }
  bool IsInstantiatedAbstractClass() const {
    return flags_ & ClassFlag::kInstantiatedAbstractClass;
  }
  bool HasSameInstanceTypeAsParent() const {
    return flags_ & ClassFlag::kHasSameInstanceTypeAsParent;
  }
  bool GenerateCppClassDefinitions() const {
    return flags_ & ClassFlag::kGenerateCppClassDefinitions || !IsExtern();
  }
  bool HasIndexedField() const override;
  size_t size() const { return size_; }
  const ClassType* GetSuperClass() const {
    if (parent() == nullptr) return nullptr;
    return parent()->IsClassType() ? ClassType::DynamicCast(parent()) : nullptr;
  }
  void SetSize(size_t size) { size_ = size; }
  void GenerateAccessors();
  bool AllowInstantiation() const;
  const Field& RegisterField(Field field) override {
    if (field.index) {
      flags_ |= ClassFlag::kHasIndexedField;
    }
    return AggregateType::RegisterField(field);
  }
  void Finalize() const override;

  std::vector<Field> ComputeAllFields() const;

  const InstanceTypeConstraints& GetInstanceTypeConstraints() const {
    return decl_->instance_type_constraints;
  }
  bool IsHighestInstanceTypeWithinParent() const {
    return flags_ & ClassFlag::kHighestInstanceTypeWithinParent;
  }
  bool IsLowestInstanceTypeWithinParent() const {
    return flags_ & ClassFlag::kLowestInstanceTypeWithinParent;
  }
  bool HasUndefinedLayout() const {
    return flags_ & ClassFlag::kUndefinedLayout;
  }
  SourcePosition GetPosition() const { return decl_->pos; }

 private:
  friend class TypeOracle;
  friend class TypeVisitor;
  ClassType(const Type* parent, Namespace* nspace, const std::string& name,
            ClassFlags flags, const std::string& generates,
            const ClassDeclaration* decl, const TypeAlias* alias);

  size_t size_;
  mutable ClassFlags flags_;
  const std::string generates_;
  const ClassDeclaration* decl_;
  const TypeAlias* alias_;
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
  TypeVector ComputeTypeVector() const {
    TypeVector result;
    for (auto& visit_result : *this) {
      result.push_back(visit_result.type());
    }
    return result;
  }
};

std::ostream& operator<<(std::ostream& os, const TypeVector& types);

using NameAndTypeVector = std::vector<NameAndType>;

struct LabelDefinition {
  std::string name;
  NameAndTypeVector parameters;
};

using LabelDefinitionVector = std::vector<LabelDefinition>;

struct LabelDeclaration {
  Identifier* name;
  TypeVector types;
};

using LabelDeclarationVector = std::vector<LabelDeclaration>;

struct ParameterTypes {
  TypeVector types;
  bool var_args;
};

std::ostream& operator<<(std::ostream& os, const ParameterTypes& parameters);

enum class ParameterMode { kProcessImplicit, kIgnoreImplicit };

using NameVector = std::vector<Identifier*>;

struct Signature {
  Signature(NameVector n, base::Optional<std::string> arguments_variable,
            ParameterTypes p, size_t i, const Type* r, LabelDeclarationVector l,
            bool transitioning)
      : parameter_names(std::move(n)),
        arguments_variable(arguments_variable),
        parameter_types(std::move(p)),
        implicit_count(i),
        return_type(r),
        labels(std::move(l)),
        transitioning(transitioning) {}
  Signature() = default;
  const TypeVector& types() const { return parameter_types.types; }
  NameVector parameter_names;
  base::Optional<std::string> arguments_variable;
  ParameterTypes parameter_types;
  size_t implicit_count = 0;
  size_t ExplicitCount() const { return types().size() - implicit_count; }
  const Type* return_type;
  LabelDeclarationVector labels;
  bool transitioning = false;
  bool HasSameTypesAs(
      const Signature& other,
      ParameterMode mode = ParameterMode::kProcessImplicit) const;
  TypeVector GetImplicitTypes() const {
    return TypeVector(parameter_types.types.begin(),
                      parameter_types.types.begin() + implicit_count);
  }
  TypeVector GetExplicitTypes() const {
    return TypeVector(parameter_types.types.begin() + implicit_count,
                      parameter_types.types.end());
  }
};

void PrintSignature(std::ostream& os, const Signature& sig, bool with_names);
std::ostream& operator<<(std::ostream& os, const Signature& sig);

bool IsAssignableFrom(const Type* to, const Type* from);

TypeVector LowerType(const Type* type);
size_t LoweredSlotCount(const Type* type);
TypeVector LowerParameterTypes(const TypeVector& parameters);
TypeVector LowerParameterTypes(const ParameterTypes& parameter_types,
                               size_t vararg_count = 0);

}  // namespace torque
}  // namespace internal
}  // namespace v8

#endif  // V8_TORQUE_TYPES_H_
