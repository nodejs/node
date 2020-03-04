// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/torque/type-visitor.h"

#include "src/common/globals.h"
#include "src/torque/declarable.h"
#include "src/torque/global-context.h"
#include "src/torque/server-data.h"
#include "src/torque/type-inference.h"
#include "src/torque/type-oracle.h"

namespace v8 {
namespace internal {
namespace torque {

const Type* TypeVisitor::ComputeType(TypeDeclaration* decl,
                                     MaybeSpecializationKey specialized_from,
                                     Scope* specialization_requester) {
  SourcePosition requester_position = CurrentSourcePosition::Get();
  CurrentSourcePosition::Scope scope(decl->pos);
  Scope* current_scope = CurrentScope::Get();
  if (specialized_from) {
    current_scope = TypeOracle::CreateGenericTypeInstantiationNamespace();
    current_scope->SetSpecializationRequester(
        {requester_position, specialization_requester,
         Type::ComputeName(decl->name->value, specialized_from)});
  }
  CurrentScope::Scope new_current_scope_scope(current_scope);
  if (specialized_from) {
    auto& params = specialized_from->generic->generic_parameters();
    auto arg_types_iterator = specialized_from->specialized_types.begin();
    for (auto param : params) {
      TypeAlias* alias =
          Declarations::DeclareType(param.name, *arg_types_iterator);
      alias->SetIsUserDefined(false);
      arg_types_iterator++;
    }
  }

  switch (decl->kind) {
#define ENUM_ITEM(name)        \
  case AstNode::Kind::k##name: \
    return ComputeType(name::cast(decl), specialized_from);
    AST_TYPE_DECLARATION_NODE_KIND_LIST(ENUM_ITEM)
#undef ENUM_ITEM
    default:
      UNIMPLEMENTED();
  }
}

const Type* TypeVisitor::ComputeType(TypeAliasDeclaration* decl,
                                     MaybeSpecializationKey specialized_from) {
  const Type* type = ComputeType(decl->type);
  type->AddAlias(decl->name->value);
  return type;
}

namespace {
std::string ComputeGeneratesType(base::Optional<std::string> opt_gen,
                                 bool enforce_tnode_type) {
  if (!opt_gen) return "";
  const std::string& generates = *opt_gen;
  if (enforce_tnode_type) {
    if (generates.length() < 7 || generates.substr(0, 6) != "TNode<" ||
        generates.substr(generates.length() - 1, 1) != ">") {
      ReportError("generated type \"", generates,
                  "\" should be of the form \"TNode<...>\"");
    }
    return generates.substr(6, generates.length() - 7);
  }
  return generates;
}
}  // namespace

const AbstractType* TypeVisitor::ComputeType(
    AbstractTypeDeclaration* decl, MaybeSpecializationKey specialized_from) {
  std::string generates =
      ComputeGeneratesType(decl->generates, !decl->is_constexpr);

  const Type* parent_type = nullptr;
  if (decl->extends) {
    parent_type = Declarations::LookupType(*decl->extends);
    if (parent_type->IsUnionType()) {
      // UnionType::IsSupertypeOf requires that types can only extend from non-
      // union types in order to work correctly.
      ReportError("type \"", decl->name->value,
                  "\" cannot extend a type union");
    }
  }

  if (generates == "" && parent_type) {
    generates = parent_type->GetGeneratedTNodeTypeName();
  }

  if (decl->is_constexpr && decl->transient) {
    ReportError("cannot declare a transient type that is also constexpr");
  }

  const AbstractType* non_constexpr_version = nullptr;
  if (decl->is_constexpr) {
    QualifiedName non_constexpr_name{GetNonConstexprName(decl->name->value)};
    const Type* non_constexpr_type =
        Declarations::LookupType(non_constexpr_name);
    non_constexpr_version = AbstractType::DynamicCast(non_constexpr_type);
    DCHECK_NOT_NULL(non_constexpr_version);
  }

  return TypeOracle::GetAbstractType(parent_type, decl->name->value,
                                     decl->transient, generates,
                                     non_constexpr_version, specialized_from);
}

void DeclareMethods(AggregateType* container_type,
                    const std::vector<Declaration*>& methods) {
  for (auto declaration : methods) {
    CurrentSourcePosition::Scope pos_scope(declaration->pos);
    TorqueMacroDeclaration* method =
        TorqueMacroDeclaration::DynamicCast(declaration);
    Signature signature = TypeVisitor::MakeSignature(method);
    signature.parameter_names.insert(
        signature.parameter_names.begin() + signature.implicit_count,
        MakeNode<Identifier>(kThisParameterName));
    Statement* body = *(method->body);
    const std::string& method_name(method->name->value);
    signature.parameter_types.types.insert(
        signature.parameter_types.types.begin() + signature.implicit_count,
        container_type);
    Declarations::CreateMethod(container_type, method_name, signature, body);
  }
}

const StructType* TypeVisitor::ComputeType(
    StructDeclaration* decl, MaybeSpecializationKey specialized_from) {
  StructType* struct_type = TypeOracle::GetStructType(decl, specialized_from);
  CurrentScope::Scope struct_namespace_scope(struct_type->nspace());
  CurrentSourcePosition::Scope position_activator(decl->pos);

  size_t offset = 0;
  bool packable = true;
  for (auto& field : decl->fields) {
    CurrentSourcePosition::Scope position_activator(
        field.name_and_type.type->pos);
    const Type* field_type = TypeVisitor::ComputeType(field.name_and_type.type);
    if (field_type->IsConstexpr()) {
      ReportError("struct field \"", field.name_and_type.name->value,
                  "\" carries constexpr type \"", *field_type, "\"");
    }
    Field f{field.name_and_type.name->pos,
            struct_type,
            base::nullopt,
            {field.name_and_type.name->value, field_type},
            offset,
            false,
            field.const_qualified,
            false};
    auto optional_size = SizeOf(f.name_and_type.type);
    // Structs may contain fields that aren't representable in packed form. If
    // so, then this field and any subsequent fields should have their offsets
    // marked as invalid.
    if (!optional_size.has_value()) {
      packable = false;
    }
    if (!packable) {
      f.offset = Field::kInvalidOffset;
    }
    struct_type->RegisterField(f);
    // Offsets are assigned based on an assumption of no space between members.
    // This might lead to invalid alignment in some cases, but most structs are
    // never actually packed in memory together (they just represent a batch of
    // CSA TNode values that should be passed around together). For any struct
    // that is used as a class field, we verify its offsets when setting up the
    // class type.
    if (optional_size.has_value()) {
      size_t field_size = 0;
      std::tie(field_size, std::ignore) = *optional_size;
      offset += field_size;
    }
  }
  return struct_type;
}

const ClassType* TypeVisitor::ComputeType(
    ClassDeclaration* decl, MaybeSpecializationKey specialized_from) {
  ClassType* new_class;
  // TODO(sigurds): Remove this hack by introducing a declarable for classes.
  const TypeAlias* alias =
      Declarations::LookupTypeAlias(QualifiedName(decl->name->value));
  GlobalContext::RegisterClass(alias);
  DCHECK_EQ(*alias->delayed_, decl);
  bool is_shape = decl->flags & ClassFlag::kIsShape;
  if (is_shape && !(decl->flags & ClassFlag::kExtern)) {
    ReportError("Shapes must be extern, add \"extern\" to the declaration.");
  }
  if (is_shape && decl->flags & ClassFlag::kUndefinedLayout) {
    ReportError("Shapes need to define their layout.");
  }
  if (decl->flags & ClassFlag::kExtern) {
    if (!decl->super) {
      ReportError("Extern class must extend another type.");
    }
    const Type* super_type = TypeVisitor::ComputeType(*decl->super);
    if (super_type != TypeOracle::GetStrongTaggedType()) {
      const ClassType* super_class = ClassType::DynamicCast(super_type);
      if (!super_class) {
        ReportError(
            "class \"", decl->name->value,
            "\" must extend either StrongTagged or an already declared class");
      }
      if (super_class->HasUndefinedLayout() &&
          !(decl->flags & ClassFlag::kUndefinedLayout)) {
        Error("Class \"", decl->name->value,
              "\" defines its layout but extends a class which does not")
            .Position(decl->pos);
      }
    }

    std::string generates = decl->name->value;
    if (is_shape) {
      const ClassType* super_class = ClassType::DynamicCast(super_type);
      if (!super_class ||
          !super_class->IsSubtypeOf(TypeOracle::GetJSObjectType())) {
        Error("Shapes need to extend a subclass of ",
              *TypeOracle::GetJSObjectType())
            .Throw();
      }
      // Shapes use their super class in CSA code since they have incomplete
      // support for type-checks on the C++ side.
      generates = super_class->name();
    }
    if (decl->generates) {
      bool enforce_tnode_type = true;
      generates = ComputeGeneratesType(decl->generates, enforce_tnode_type);
    }

    new_class = TypeOracle::GetClassType(super_type, decl->name->value,
                                         decl->flags, generates, decl, alias);
  } else {
    if (!decl->super) {
      ReportError("Intern class ", decl->name->value,
                  " must extend class Struct.");
    }
    const Type* super_type = TypeVisitor::ComputeType(*decl->super);
    const ClassType* super_class = ClassType::DynamicCast(super_type);
    const Type* struct_type = Declarations::LookupGlobalType("Struct");
    if (!super_class || super_class != struct_type) {
      ReportError("Intern class ", decl->name->value,
                  " must extend class Struct.");
    }
    if (decl->generates) {
      ReportError("Only extern classes can specify a generated type.");
    }
    new_class = TypeOracle::GetClassType(
        super_type, decl->name->value,
        decl->flags | ClassFlag::kGeneratePrint | ClassFlag::kGenerateVerify,
        decl->name->value, decl, alias);
  }
  return new_class;
}

const Type* TypeVisitor::ComputeType(TypeExpression* type_expression) {
  if (auto* basic = BasicTypeExpression::DynamicCast(type_expression)) {
    QualifiedName qualified_name{basic->namespace_qualification, basic->name};
    auto& args = basic->generic_arguments;
    const Type* type;
    SourcePosition pos = SourcePosition::Invalid();

    if (args.empty()) {
      auto* alias = Declarations::LookupTypeAlias(qualified_name);
      type = alias->type();
      pos = alias->GetDeclarationPosition();
    } else {
      auto* generic_type =
          Declarations::LookupUniqueGenericType(qualified_name);
      type = TypeOracle::GetGenericTypeInstance(generic_type,
                                                ComputeTypeVector(args));
      pos = generic_type->declaration()->name->pos;
    }

    if (GlobalContext::collect_language_server_data()) {
      LanguageServerData::AddDefinition(type_expression->pos, pos);
    }
    return type;

  } else if (auto* union_type =
                 UnionTypeExpression::DynamicCast(type_expression)) {
    return TypeOracle::GetUnionType(ComputeType(union_type->a),
                                    ComputeType(union_type->b));
  } else {
    auto* function_type_exp = FunctionTypeExpression::cast(type_expression);
    TypeVector argument_types;
    for (TypeExpression* type_exp : function_type_exp->parameters) {
      argument_types.push_back(ComputeType(type_exp));
    }
    return TypeOracle::GetBuiltinPointerType(
        argument_types, ComputeType(function_type_exp->return_type));
  }
}

Signature TypeVisitor::MakeSignature(const CallableDeclaration* declaration) {
  LabelDeclarationVector definition_vector;
  for (const auto& label : declaration->labels) {
    LabelDeclaration def = {label.name, ComputeTypeVector(label.types)};
    definition_vector.push_back(def);
  }
  base::Optional<std::string> arguments_variable;
  if (declaration->parameters.has_varargs)
    arguments_variable = declaration->parameters.arguments_variable;
  Signature result{declaration->parameters.names,
                   arguments_variable,
                   {ComputeTypeVector(declaration->parameters.types),
                    declaration->parameters.has_varargs},
                   declaration->parameters.implicit_count,
                   ComputeType(declaration->return_type),
                   definition_vector,
                   declaration->transitioning};
  return result;
}

void TypeVisitor::VisitClassFieldsAndMethods(
    ClassType* class_type, const ClassDeclaration* class_declaration) {
  const ClassType* super_class = class_type->GetSuperClass();
  size_t class_offset = super_class ? super_class->size() : 0;
  bool seen_indexed_field = false;
  for (const ClassFieldExpression& field_expression :
       class_declaration->fields) {
    CurrentSourcePosition::Scope position_activator(
        field_expression.name_and_type.type->pos);
    const Type* field_type = ComputeType(field_expression.name_and_type.type);
    if (class_type->IsShape()) {
      if (!field_type->IsSubtypeOf(TypeOracle::GetObjectType())) {
        ReportError(
            "in-object properties only support subtypes of Object, but "
            "found type ",
            *field_type);
      }
      if (field_expression.weak) {
        ReportError("in-object properties cannot be weak");
      }
    }
    if (!(class_declaration->flags & ClassFlag::kExtern)) {
      if (!field_type->IsSubtypeOf(TypeOracle::GetObjectType())) {
        ReportError(
            "non-extern classes only support subtypes of type Object, but "
            "found type ",
            *field_type);
      }
      if (field_expression.weak) {
        ReportError("non-extern classes do not support weak fields");
      }
    }
    if (const StructType* struct_type = StructType::DynamicCast(field_type)) {
      for (const Field& struct_field : struct_type->fields()) {
        if (!struct_field.name_and_type.type->IsSubtypeOf(
                TypeOracle::GetTaggedType())) {
          // If we ever actually need different sizes of struct fields, then we
          // can define the packing and alignment rules. Until then, let's keep
          // it simple. This restriction also helps keep the tagged and untagged
          // regions separate in the class layout (see also
          // FieldOffsetsGenerator::GetSectionFor).
          Error(
              "Classes do not support fields which are structs containing "
              "untagged data.");
        }
      }
    }
    base::Optional<NameAndType> index_field;
    if (field_expression.index) {
      if (seen_indexed_field ||
          (super_class && super_class->HasIndexedField())) {
        ReportError(
            "only one indexable field is currently supported per class");
      }
      seen_indexed_field = true;
      index_field = class_type->LookupFieldInternal(*field_expression.index)
                        .name_and_type;
    } else {
      if (seen_indexed_field) {
        ReportError("cannot declare non-indexable field \"",
                    field_expression.name_and_type.name,
                    "\" after an indexable field "
                    "declaration");
      }
    }
    const Field& field = class_type->RegisterField(
        {field_expression.name_and_type.name->pos,
         class_type,
         index_field,
         {field_expression.name_and_type.name->value, field_type},
         class_offset,
         field_expression.weak,
         field_expression.const_qualified,
         field_expression.generate_verify});
    size_t field_size;
    std::tie(field_size, std::ignore) = field.GetFieldSizeInformation();
    // Our allocations don't support alignments beyond kTaggedSize.
    size_t alignment = std::min(
        static_cast<size_t>(TargetArchitecture::TaggedSize()), field_size);
    if (alignment > 0 && class_offset % alignment != 0) {
      ReportError("field ", field_expression.name_and_type.name, " at offset ",
                  class_offset, " is not ", alignment, "-byte aligned.");
    }
    if (!field_expression.index) {
      class_offset += field_size;
    }
  }
  class_type->SetSize(class_offset);
  class_type->GenerateAccessors();
  DeclareMethods(class_type, class_declaration->methods);
}

void TypeVisitor::VisitStructMethods(
    StructType* struct_type, const StructDeclaration* struct_declaration) {
  DeclareMethods(struct_type, struct_declaration->methods);
}

const StructType* TypeVisitor::ComputeTypeForStructExpression(
    TypeExpression* type_expression,
    const std::vector<const Type*>& term_argument_types) {
  auto* basic = BasicTypeExpression::DynamicCast(type_expression);
  if (!basic) {
    ReportError("expected basic type expression referring to struct");
  }

  QualifiedName qualified_name{basic->namespace_qualification, basic->name};
  base::Optional<GenericType*> maybe_generic_type =
      Declarations::TryLookupGenericType(qualified_name);

  StructDeclaration* decl =
      maybe_generic_type
          ? StructDeclaration::DynamicCast((*maybe_generic_type)->declaration())
          : nullptr;

  // Compute types of non-generic structs as usual
  if (!(maybe_generic_type && decl)) {
    const Type* type = ComputeType(type_expression);
    const StructType* struct_type = StructType::DynamicCast(type);
    if (!struct_type) {
      ReportError(*type, " is not a struct, but used like one");
    }
    return struct_type;
  }

  auto generic_type = *maybe_generic_type;
  auto explicit_type_arguments = ComputeTypeVector(basic->generic_arguments);

  std::vector<TypeExpression*> term_parameters;
  auto& fields = decl->fields;
  term_parameters.reserve(fields.size());
  for (auto& field : fields) {
    term_parameters.push_back(field.name_and_type.type);
  }

  CurrentScope::Scope generic_scope(generic_type->ParentScope());
  TypeArgumentInference inference(generic_type->generic_parameters(),
                                  explicit_type_arguments, term_parameters,
                                  term_argument_types);

  if (inference.HasFailed()) {
    ReportError("failed to infer type arguments for struct ", basic->name,
                " initialization: ", inference.GetFailureReason());
  }
  if (GlobalContext::collect_language_server_data()) {
    LanguageServerData::AddDefinition(type_expression->pos,
                                      generic_type->declaration()->name->pos);
  }
  return StructType::cast(
      TypeOracle::GetGenericTypeInstance(generic_type, inference.GetResult()));
}

}  // namespace torque
}  // namespace internal
}  // namespace v8
