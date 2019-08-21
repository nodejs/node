// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>

#include "src/common/globals.h"
#include "src/torque/ast.h"
#include "src/torque/declarable.h"
#include "src/torque/type-oracle.h"
#include "src/torque/type-visitor.h"
#include "src/torque/types.h"

namespace v8 {
namespace internal {
namespace torque {

std::string Type::ToString() const {
  if (aliases_.size() == 0) return ToExplicitString();
  if (aliases_.size() == 1) return *aliases_.begin();
  std::stringstream result;
  int i = 0;
  for (const std::string& alias : aliases_) {
    if (i == 0) {
      result << alias << " (aka. ";
    } else if (i == 1) {
      result << alias;
    } else {
      result << ", " << alias;
    }
    ++i;
  }
  result << ")";
  return result.str();
}

bool Type::IsSubtypeOf(const Type* supertype) const {
  if (supertype->IsTopType()) return true;
  if (IsNever()) return true;
  if (const UnionType* union_type = UnionType::DynamicCast(supertype)) {
    return union_type->IsSupertypeOf(this);
  }
  const Type* subtype = this;
  while (subtype != nullptr) {
    if (subtype == supertype) return true;
    subtype = subtype->parent();
  }
  return false;
}

base::Optional<const ClassType*> Type::ClassSupertype() const {
  for (const Type* t = this; t != nullptr; t = t->parent()) {
    if (auto* class_type = ClassType::DynamicCast(t)) return class_type;
  }
  return base::nullopt;
}

// static
const Type* Type::CommonSupertype(const Type* a, const Type* b) {
  int diff = a->Depth() - b->Depth();
  const Type* a_supertype = a;
  const Type* b_supertype = b;
  for (; diff > 0; --diff) a_supertype = a_supertype->parent();
  for (; diff < 0; ++diff) b_supertype = b_supertype->parent();
  while (a_supertype && b_supertype) {
    if (a_supertype == b_supertype) return a_supertype;
    a_supertype = a_supertype->parent();
    b_supertype = b_supertype->parent();
  }
  ReportError("types " + a->ToString() + " and " + b->ToString() +
              " have no common supertype");
}

int Type::Depth() const {
  int result = 0;
  for (const Type* current = parent_; current; current = current->parent_) {
    ++result;
  }
  return result;
}

bool Type::IsAbstractName(const std::string& name) const {
  if (!IsAbstractType()) return false;
  return AbstractType::cast(this)->name() == name;
}

std::string Type::GetGeneratedTypeName() const {
  std::string result = GetGeneratedTypeNameImpl();
  if (result.empty() || result == "compiler::TNode<>") {
    ReportError("Generated type is required for type '", ToString(),
                "'. Use 'generates' clause in definition.");
  }
  return result;
}

std::string Type::GetGeneratedTNodeTypeName() const {
  std::string result = GetGeneratedTNodeTypeNameImpl();
  if (result.empty() || IsConstexpr()) {
    ReportError("Generated TNode type is required for type '", ToString(),
                "'. Use 'generates' clause in definition.");
  }
  return result;
}

std::string AbstractType::GetGeneratedTNodeTypeNameImpl() const {
  return generated_type_;
}

std::string BuiltinPointerType::ToExplicitString() const {
  std::stringstream result;
  result << "builtin (";
  PrintCommaSeparatedList(result, parameter_types_);
  result << ") => " << *return_type_;
  return result.str();
}

std::string BuiltinPointerType::MangledName() const {
  std::stringstream result;
  result << "FT";
  for (const Type* t : parameter_types_) {
    std::string arg_type_string = t->MangledName();
    result << arg_type_string.size() << arg_type_string;
  }
  std::string return_type_string = return_type_->MangledName();
  result << return_type_string.size() << return_type_string;
  return result.str();
}

std::string UnionType::ToExplicitString() const {
  std::stringstream result;
  result << "(";
  bool first = true;
  for (const Type* t : types_) {
    if (!first) {
      result << " | ";
    }
    first = false;
    result << *t;
  }
  result << ")";
  return result.str();
}

std::string UnionType::MangledName() const {
  std::stringstream result;
  result << "UT";
  for (const Type* t : types_) {
    std::string arg_type_string = t->MangledName();
    result << arg_type_string.size() << arg_type_string;
  }
  return result.str();
}

std::string UnionType::GetGeneratedTNodeTypeNameImpl() const {
  if (types_.size() <= 3) {
    std::set<std::string> members;
    for (const Type* t : types_) {
      members.insert(t->GetGeneratedTNodeTypeName());
    }
    if (members == std::set<std::string>{"Smi", "HeapNumber"}) {
      return "Number";
    }
    if (members == std::set<std::string>{"Smi", "HeapNumber", "BigInt"}) {
      return "Numeric";
    }
  }
  return parent()->GetGeneratedTNodeTypeName();
}

void UnionType::RecomputeParent() {
  const Type* parent = nullptr;
  for (const Type* t : types_) {
    if (parent == nullptr) {
      parent = t;
    } else {
      parent = CommonSupertype(parent, t);
    }
  }
  set_parent(parent);
}

void UnionType::Subtract(const Type* t) {
  for (auto it = types_.begin(); it != types_.end();) {
    if ((*it)->IsSubtypeOf(t)) {
      it = types_.erase(it);
    } else {
      ++it;
    }
  }
  if (types_.size() == 0) types_.insert(TypeOracle::GetNeverType());
  RecomputeParent();
}

const Type* SubtractType(const Type* a, const Type* b) {
  UnionType result = UnionType::FromType(a);
  result.Subtract(b);
  return TypeOracle::GetUnionType(result);
}

void AggregateType::CheckForDuplicateFields() const {
  // Check the aggregate hierarchy and currently defined class for duplicate
  // field declarations.
  auto hierarchy = GetHierarchy();
  std::map<std::string, const AggregateType*> field_names;
  for (const AggregateType* aggregate_type : hierarchy) {
    for (const Field& field : aggregate_type->fields()) {
      const std::string& field_name = field.name_and_type.name;
      auto i = field_names.find(field_name);
      if (i != field_names.end()) {
        CurrentSourcePosition::Scope current_source_position(field.pos);
        std::string aggregate_type_name =
            aggregate_type->IsClassType() ? "class" : "struct";
        if (i->second == this) {
          ReportError(aggregate_type_name, " '", name(),
                      "' declares a field with the name '", field_name,
                      "' more than once");
        } else {
          ReportError(aggregate_type_name, " '", name(),
                      "' declares a field with the name '", field_name,
                      "' that masks an inherited field from class '",
                      i->second->name(), "'");
        }
      }
      field_names[field_name] = aggregate_type;
    }
  }
}

std::vector<const AggregateType*> AggregateType::GetHierarchy() const {
  if (!is_finalized_) Finalize();
  std::vector<const AggregateType*> hierarchy;
  const AggregateType* current_container_type = this;
  while (current_container_type != nullptr) {
    hierarchy.push_back(current_container_type);
    current_container_type =
        current_container_type->IsClassType()
            ? ClassType::cast(current_container_type)->GetSuperClass()
            : nullptr;
  }
  std::reverse(hierarchy.begin(), hierarchy.end());
  return hierarchy;
}

bool AggregateType::HasField(const std::string& name) const {
  if (!is_finalized_) Finalize();
  for (auto& field : fields_) {
    if (field.name_and_type.name == name) return true;
  }
  if (parent() != nullptr) {
    if (auto parent_class = ClassType::DynamicCast(parent())) {
      return parent_class->HasField(name);
    }
  }
  return false;
}

const Field& AggregateType::LookupFieldInternal(const std::string& name) const {
  for (auto& field : fields_) {
    if (field.name_and_type.name == name) return field;
  }
  if (parent() != nullptr) {
    if (auto parent_class = ClassType::DynamicCast(parent())) {
      return parent_class->LookupField(name);
    }
  }
  ReportError("no field ", name, " found");
}

const Field& AggregateType::LookupField(const std::string& name) const {
  if (!is_finalized_) Finalize();
  return LookupFieldInternal(name);
}

std::string StructType::GetGeneratedTypeNameImpl() const {
  return "TorqueStruct" + MangledName();
}

// static
std::string StructType::ComputeName(const std::string& basename,
                                    const std::vector<const Type*>& args) {
  if (args.size() == 0) return basename;
  std::stringstream s;
  s << basename << "<";
  bool first = true;
  for (auto t : args) {
    if (!first) {
      s << ", ";
    }
    s << t->ToString();
    first = false;
  }
  s << ">";
  return s.str();
}

std::vector<Method*> AggregateType::Methods(const std::string& name) const {
  if (!is_finalized_) Finalize();
  std::vector<Method*> result;
  std::copy_if(methods_.begin(), methods_.end(), std::back_inserter(result),
               [name](Macro* macro) { return macro->ReadableName() == name; });
  return result;
}

std::string StructType::ToExplicitString() const {
  std::stringstream result;
  result << "struct " << name();
  return result.str();
}

constexpr ClassFlags ClassType::kInternalFlags;

ClassType::ClassType(const Type* parent, Namespace* nspace,
                     const std::string& name, ClassFlags flags,
                     const std::string& generates, const ClassDeclaration* decl,
                     const TypeAlias* alias)
    : AggregateType(Kind::kClassType, parent, nspace, name),
      size_(0),
      flags_(flags & ~(kInternalFlags)),
      generates_(generates),
      decl_(decl),
      alias_(alias) {
  DCHECK_EQ(flags & kInternalFlags, 0);
}

bool ClassType::HasIndexedField() const {
  if (!is_finalized_) Finalize();
  return flags_ & ClassFlag::kHasIndexedField;
}

std::string ClassType::GetGeneratedTNodeTypeNameImpl() const {
  return generates_;
}

std::string ClassType::GetGeneratedTypeNameImpl() const {
  return IsConstexpr() ? GetGeneratedTNodeTypeName()
                       : "compiler::TNode<" + GetGeneratedTNodeTypeName() + ">";
}

std::string ClassType::ToExplicitString() const {
  std::stringstream result;
  result << "class " << name();
  return result.str();
}

bool ClassType::AllowInstantiation() const {
  return (!IsExtern() || nspace()->IsDefaultNamespace()) &&
         (!IsAbstract() || IsInstantiatedAbstractClass());
}

void ClassType::Finalize() const {
  if (is_finalized_) return;
  CurrentScope::Scope scope_activator(alias_->ParentScope());
  CurrentSourcePosition::Scope position_activator(decl_->pos);
  if (parent()) {
    if (const ClassType* super_class = ClassType::DynamicCast(parent())) {
      if (super_class->HasIndexedField()) flags_ |= ClassFlag::kHasIndexedField;
      if (!super_class->IsAbstract() && !HasSameInstanceTypeAsParent()) {
        Error(
            "Super class must either be abstract (annotate super class with "
            "@abstract) "
            "or this class must have the same instance type as the super class "
            "(annotate this class with @hasSameInstanceTypeAsParent).")
            .Position(this->decl_->name->pos);
      }
    }
  }
  TypeVisitor::VisitClassFieldsAndMethods(const_cast<ClassType*>(this),
                                          this->decl_);
  is_finalized_ = true;
  if (GenerateCppClassDefinitions() || !IsExtern()) {
    for (const Field& f : fields()) {
      if (f.is_weak) {
        Error("Generation of C++ class for Torque class ", name(),
              " is not supported yet, because field ", f.name_and_type.name,
              ": ", *f.name_and_type.type, " is a weak field.")
            .Position(f.pos);
      }
    }
  }
  CheckForDuplicateFields();
}

void ClassType::GenerateAccessors() {
  // For each field, construct AST snippets that implement a CSA accessor
  // function and define a corresponding '.field' operator. The
  // implementation iterator will turn the snippets into code.
  for (auto& field : fields_) {
    if (field.index || field.name_and_type.type == TypeOracle::GetVoidType()) {
      continue;
    }
    CurrentSourcePosition::Scope position_activator(field.pos);
    IdentifierExpression* parameter =
        MakeNode<IdentifierExpression>(MakeNode<Identifier>(std::string{"o"}));

    // Load accessor
    std::string camel_field_name = CamelifyString(field.name_and_type.name);
    std::string load_macro_name = "Load" + this->name() + camel_field_name;
    Signature load_signature;
    load_signature.parameter_names.push_back(MakeNode<Identifier>("o"));
    load_signature.parameter_types.types.push_back(this);
    load_signature.parameter_types.var_args = false;
    load_signature.return_type = field.name_and_type.type;
    Statement* load_body =
        MakeNode<ReturnStatement>(MakeNode<FieldAccessExpression>(
            parameter, MakeNode<Identifier>(field.name_and_type.name)));
    Declarations::DeclareMacro(load_macro_name, true, base::nullopt,
                               load_signature, false, load_body, base::nullopt,
                               false);

    // Store accessor
    IdentifierExpression* value = MakeNode<IdentifierExpression>(
        std::vector<std::string>{}, MakeNode<Identifier>(std::string{"v"}));
    std::string store_macro_name = "Store" + this->name() + camel_field_name;
    Signature store_signature;
    store_signature.parameter_names.push_back(MakeNode<Identifier>("o"));
    store_signature.parameter_names.push_back(MakeNode<Identifier>("v"));
    store_signature.parameter_types.types.push_back(this);
    store_signature.parameter_types.types.push_back(field.name_and_type.type);
    store_signature.parameter_types.var_args = false;
    // TODO(danno): Store macros probably should return their value argument
    store_signature.return_type = TypeOracle::GetVoidType();
    Statement* store_body =
        MakeNode<ExpressionStatement>(MakeNode<AssignmentExpression>(
            MakeNode<FieldAccessExpression>(
                parameter, MakeNode<Identifier>(field.name_and_type.name)),
            value));
    Declarations::DeclareMacro(store_macro_name, true, base::nullopt,
                               store_signature, false, store_body,
                               base::nullopt, false);
  }
}

void PrintSignature(std::ostream& os, const Signature& sig, bool with_names) {
  os << "(";
  for (size_t i = 0; i < sig.parameter_types.types.size(); ++i) {
    if (i == 0 && sig.implicit_count != 0) os << "implicit ";
    if (sig.implicit_count > 0 && sig.implicit_count == i) {
      os << ")(";
    } else {
      if (i > 0) os << ", ";
    }
    if (with_names && !sig.parameter_names.empty()) {
      if (i < sig.parameter_names.size()) {
        os << sig.parameter_names[i] << ": ";
      }
    }
    os << *sig.parameter_types.types[i];
  }
  if (sig.parameter_types.var_args) {
    if (sig.parameter_names.size()) os << ", ";
    os << "...";
  }
  os << ")";
  os << ": " << *sig.return_type;

  if (sig.labels.empty()) return;

  os << " labels ";
  for (size_t i = 0; i < sig.labels.size(); ++i) {
    if (i > 0) os << ", ";
    os << sig.labels[i].name;
    if (sig.labels[i].types.size() > 0) os << "(" << sig.labels[i].types << ")";
  }
}

std::ostream& operator<<(std::ostream& os, const NameAndType& name_and_type) {
  os << name_and_type.name;
  os << ": ";
  os << *name_and_type.type;
  return os;
}

std::ostream& operator<<(std::ostream& os, const Field& field) {
  os << field.name_and_type;
  if (field.is_weak) {
    os << " (weak)";
  }
  return os;
}

std::ostream& operator<<(std::ostream& os, const Signature& sig) {
  PrintSignature(os, sig, true);
  return os;
}

std::ostream& operator<<(std::ostream& os, const TypeVector& types) {
  PrintCommaSeparatedList(os, types);
  return os;
}

std::ostream& operator<<(std::ostream& os, const ParameterTypes& p) {
  PrintCommaSeparatedList(os, p.types);
  if (p.var_args) {
    if (p.types.size() > 0) os << ", ";
    os << "...";
  }
  return os;
}

bool Signature::HasSameTypesAs(const Signature& other,
                               ParameterMode mode) const {
  auto compare_types = types();
  auto other_compare_types = other.types();
  if (mode == ParameterMode::kIgnoreImplicit) {
    compare_types = GetExplicitTypes();
    other_compare_types = other.GetExplicitTypes();
  }
  if (!(compare_types == other_compare_types &&
        parameter_types.var_args == other.parameter_types.var_args &&
        return_type == other.return_type)) {
    return false;
  }
  if (labels.size() != other.labels.size()) {
    return false;
  }
  size_t i = 0;
  for (const auto& l : labels) {
    if (l.types != other.labels[i++].types) {
      return false;
    }
  }
  return true;
}

bool IsAssignableFrom(const Type* to, const Type* from) {
  if (to == from) return true;
  if (from->IsSubtypeOf(to)) return true;
  return TypeOracle::IsImplicitlyConvertableFrom(to, from);
}

bool operator<(const Type& a, const Type& b) {
  return a.MangledName() < b.MangledName();
}

VisitResult ProjectStructField(VisitResult structure,
                               const std::string& fieldname) {
  BottomOffset begin = structure.stack_range().begin();

  // Check constructor this super classes for fields.
  const StructType* type = StructType::cast(structure.type());
  auto& fields = type->fields();
  for (auto& field : fields) {
    BottomOffset end = begin + LoweredSlotCount(field.name_and_type.type);
    if (field.name_and_type.name == fieldname) {
      return VisitResult(field.name_and_type.type, StackRange{begin, end});
    }
    begin = end;
  }

  ReportError("struct '", type->name(), "' doesn't contain a field '",
              fieldname, "'");
}

namespace {
void AppendLoweredTypes(const Type* type, std::vector<const Type*>* result) {
  DCHECK_NE(type, TypeOracle::GetNeverType());
  if (type->IsConstexpr()) return;
  if (type == TypeOracle::GetVoidType()) return;
  if (auto* s = StructType::DynamicCast(type)) {
    for (const Field& field : s->fields()) {
      AppendLoweredTypes(field.name_and_type.type, result);
    }
  } else if (type->IsReferenceType()) {
    result->push_back(TypeOracle::GetHeapObjectType());
    result->push_back(TypeOracle::GetIntPtrType());
  } else {
    result->push_back(type);
  }
}
}  // namespace

TypeVector LowerType(const Type* type) {
  TypeVector result;
  AppendLoweredTypes(type, &result);
  return result;
}

size_t LoweredSlotCount(const Type* type) { return LowerType(type).size(); }

TypeVector LowerParameterTypes(const TypeVector& parameters) {
  std::vector<const Type*> result;
  for (const Type* t : parameters) {
    AppendLoweredTypes(t, &result);
  }
  return result;
}

TypeVector LowerParameterTypes(const ParameterTypes& parameter_types,
                               size_t arg_count) {
  std::vector<const Type*> result = LowerParameterTypes(parameter_types.types);
  for (size_t i = parameter_types.types.size(); i < arg_count; ++i) {
    DCHECK(parameter_types.var_args);
    AppendLoweredTypes(TypeOracle::GetObjectType(), &result);
  }
  return result;
}

VisitResult VisitResult::NeverResult() {
  VisitResult result;
  result.type_ = TypeOracle::GetNeverType();
  return result;
}

std::tuple<size_t, std::string> Field::GetFieldSizeInformation() const {
  std::string size_string = "#no size";
  const Type* field_type = this->name_and_type.type;
  size_t field_size = 0;
  if (field_type->IsSubtypeOf(TypeOracle::GetTaggedType())) {
    field_size = kTaggedSize;
    size_string = "kTaggedSize";
  } else if (field_type->IsSubtypeOf(TypeOracle::GetRawPtrType())) {
    field_size = kSystemPointerSize;
    size_string = "kSystemPointerSize";
  } else if (field_type->IsSubtypeOf(TypeOracle::GetVoidType())) {
    field_size = 0;
    size_string = "0";
  } else if (field_type->IsSubtypeOf(TypeOracle::GetInt8Type())) {
    field_size = kUInt8Size;
    size_string = "kUInt8Size";
  } else if (field_type->IsSubtypeOf(TypeOracle::GetUint8Type())) {
    field_size = kUInt8Size;
    size_string = "kUInt8Size";
  } else if (field_type->IsSubtypeOf(TypeOracle::GetInt16Type())) {
    field_size = kUInt16Size;
    size_string = "kUInt16Size";
  } else if (field_type->IsSubtypeOf(TypeOracle::GetUint16Type())) {
    field_size = kUInt16Size;
    size_string = "kUInt16Size";
  } else if (field_type->IsSubtypeOf(TypeOracle::GetInt32Type())) {
    field_size = kInt32Size;
    size_string = "kInt32Size";
  } else if (field_type->IsSubtypeOf(TypeOracle::GetUint32Type())) {
    field_size = kInt32Size;
    size_string = "kInt32Size";
  } else if (field_type->IsSubtypeOf(TypeOracle::GetFloat64Type())) {
    field_size = kDoubleSize;
    size_string = "kDoubleSize";
  } else if (field_type->IsSubtypeOf(TypeOracle::GetIntPtrType())) {
    field_size = kIntptrSize;
    size_string = "kIntptrSize";
  } else if (field_type->IsSubtypeOf(TypeOracle::GetUIntPtrType())) {
    field_size = kIntptrSize;
    size_string = "kIntptrSize";
  } else {
    ReportError("fields of type ", *field_type, " are not (yet) supported");
  }
  return std::make_tuple(field_size, size_string);
}

}  // namespace torque
}  // namespace internal
}  // namespace v8
