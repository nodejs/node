// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/torque/declaration-visitor.h"
#include "src/torque/ast.h"

namespace v8 {
namespace internal {
namespace torque {

void DeclarationVisitor::Visit(Declaration* decl) {
  CurrentSourcePosition::Scope scope(decl->pos);
  switch (decl->kind) {
#define ENUM_ITEM(name)        \
  case AstNode::Kind::k##name: \
    return Visit(name::cast(decl));
    AST_DECLARATION_NODE_KIND_LIST(ENUM_ITEM)
#undef ENUM_ITEM
    default:
      UNIMPLEMENTED();
  }
}

void DeclarationVisitor::Visit(CallableNode* decl, const Signature& signature,
                               base::Optional<Statement*> body) {
  switch (decl->kind) {
#define ENUM_ITEM(name)        \
  case AstNode::Kind::k##name: \
    return Visit(name::cast(decl), signature, body);
    AST_CALLABLE_NODE_KIND_LIST(ENUM_ITEM)
#undef ENUM_ITEM
    default:
      UNIMPLEMENTED();
  }
}

Builtin* DeclarationVisitor::CreateBuiltin(BuiltinDeclaration* decl,
                                           std::string external_name,
                                           std::string readable_name,
                                           Signature signature,
                                           base::Optional<Statement*> body) {
  const bool javascript = decl->javascript_linkage;
  const bool varargs = decl->signature->parameters.has_varargs;
  Builtin::Kind kind = !javascript ? Builtin::kStub
                                   : varargs ? Builtin::kVarArgsJavaScript
                                             : Builtin::kFixedArgsJavaScript;

  if (signature.types().size() == 0 ||
      !(signature.types()[0] ==
        Declarations::LookupGlobalType(CONTEXT_TYPE_STRING))) {
    std::stringstream stream;
    stream << "first parameter to builtin " << decl->name
           << " is not a context but should be";
    ReportError(stream.str());
  }

  if (varargs && !javascript) {
    std::stringstream stream;
    stream << "builtin " << decl->name
           << " with rest parameters must be a JavaScript builtin";
    ReportError(stream.str());
  }

  if (javascript) {
    if (signature.types().size() < 2 ||
        !(signature.types()[1] ==
          Declarations::LookupGlobalType(OBJECT_TYPE_STRING))) {
      std::stringstream stream;
      stream << "second parameter to javascript builtin " << decl->name
             << " is " << *signature.types()[1] << " but should be Object";
      ReportError(stream.str());
    }
  }

  if (const StructType* struct_type =
          StructType::DynamicCast(signature.return_type)) {
    std::stringstream stream;
    stream << "builtins (in this case" << decl->name
           << ") cannot return structs (in this case " << struct_type->name()
           << ")";
    ReportError(stream.str());
  }

  return Declarations::CreateBuiltin(
      std::move(external_name), std::move(readable_name), kind,
      std::move(signature), decl->transitioning, body);
}

void DeclarationVisitor::Visit(ExternalRuntimeDeclaration* decl,
                               const Signature& signature,
                               base::Optional<Statement*> body) {
  if (GlobalContext::verbose()) {
    std::cout << "found declaration of external runtime " << decl->name
              << " with signature ";
  }

  if (signature.parameter_types.types.size() == 0 ||
      !(signature.parameter_types.types[0] ==
        Declarations::LookupGlobalType(CONTEXT_TYPE_STRING))) {
    std::stringstream stream;
    stream << "first parameter to runtime " << decl->name
           << " is not a context but should be";
    ReportError(stream.str());
  }

  if (signature.return_type->IsStructType()) {
    std::stringstream stream;
    stream << "runtime functions (in this case" << decl->name
           << ") cannot return structs (in this case "
           << static_cast<const StructType*>(signature.return_type)->name()
           << ")";
    ReportError(stream.str());
  }

  Declarations::DeclareRuntimeFunction(decl->name, signature,
                                       decl->transitioning);
}

void DeclarationVisitor::Visit(ExternalMacroDeclaration* decl,
                               const Signature& signature,
                               base::Optional<Statement*> body) {
  if (GlobalContext::verbose()) {
    std::cout << "found declaration of external macro " << decl->name
              << " with signature ";
  }

  Declarations::DeclareMacro(decl->name, decl->external_assembler_name,
                             signature, decl->transitioning, body, decl->op);
}

void DeclarationVisitor::Visit(TorqueBuiltinDeclaration* decl,
                               const Signature& signature,
                               base::Optional<Statement*> body) {
  Declarations::Declare(
      decl->name, CreateBuiltin(decl, decl->name, decl->name, signature, body));
}

void DeclarationVisitor::Visit(TorqueMacroDeclaration* decl,
                               const Signature& signature,
                               base::Optional<Statement*> body) {
  Declarations::DeclareMacro(decl->name, base::nullopt, signature,
                             decl->transitioning, body, decl->op);
}

void DeclarationVisitor::Visit(IntrinsicDeclaration* decl,
                               const Signature& signature,
                               base::Optional<Statement*> body) {
  Declarations::DeclareIntrinsic(decl->name, signature);
}

void DeclarationVisitor::Visit(ConstDeclaration* decl) {
  Declarations::DeclareNamespaceConstant(
      decl->name, Declarations::GetType(decl->type), decl->expression);
}

void DeclarationVisitor::Visit(StandardDeclaration* decl) {
  Signature signature = MakeSignature(decl->callable->signature.get());
  Visit(decl->callable, signature, decl->body);
}

void DeclarationVisitor::Visit(GenericDeclaration* decl) {
  Declarations::DeclareGeneric(decl->callable->name, decl);
}

void DeclarationVisitor::Visit(SpecializationDeclaration* decl) {
  if ((decl->body != nullptr) == decl->external) {
    std::stringstream stream;
    stream << "specialization of " << decl->name
           << " must either be marked 'extern' or have a body";
    ReportError(stream.str());
  }

  std::vector<Generic*> generic_list = Declarations::LookupGeneric(decl->name);
  // Find the matching generic specialization based on the concrete parameter
  // list.
  Generic* matching_generic = nullptr;
  Signature signature_with_types = MakeSignature(decl->signature.get());
  for (Generic* generic : generic_list) {
    Signature generic_signature_with_types = MakeSpecializedSignature(
        SpecializationKey{generic, GetTypeVector(decl->generic_parameters)});
    if (signature_with_types.HasSameTypesAs(generic_signature_with_types,
                                            ParameterMode::kIgnoreImplicit)) {
      if (matching_generic != nullptr) {
        std::stringstream stream;
        stream << "specialization of " << decl->name
               << " is ambigous, it matches more than one generic declaration ("
               << *matching_generic << " and " << *generic << ")";
        ReportError(stream.str());
      }
      matching_generic = generic;
    }
  }

  if (matching_generic == nullptr) {
    std::stringstream stream;
    if (generic_list.size() == 0) {
      stream << "no generic defined with the name " << decl->name;
      ReportError(stream.str());
    }
    stream << "specialization of " << decl->name
           << " doesn't match any generic declaration\n";
    stream << "specialization signature:";
    stream << "\n  " << signature_with_types;
    stream << "\ncandidates are:";
    for (Generic* generic : generic_list) {
      stream << "\n  "
             << MakeSpecializedSignature(SpecializationKey{
                    generic, GetTypeVector(decl->generic_parameters)});
    }
    ReportError(stream.str());
  }

  Specialize(SpecializationKey{matching_generic,
                               GetTypeVector(decl->generic_parameters)},
             matching_generic->declaration()->callable, decl->signature.get(),
             decl->body);
}

void DeclarationVisitor::Visit(ExternConstDeclaration* decl) {
  const Type* type = Declarations::GetType(decl->type);
  if (!type->IsConstexpr()) {
    std::stringstream stream;
    stream << "extern constants must have constexpr type, but found: \""
           << *type << "\"\n";
    ReportError(stream.str());
  }

  Declarations::DeclareExternConstant(decl->name, type, decl->literal);
}

void DeclarationVisitor::DeclareMethods(
    AggregateType* container_type, const std::vector<Declaration*>& methods) {
  // Declare the class' methods
  IdentifierExpression* constructor_this = MakeNode<IdentifierExpression>(
      std::vector<std::string>{}, kThisParameterName);
  AggregateType* constructor_this_type =
      container_type->IsStructType()
          ? container_type
          : ClassType::cast(container_type)->struct_type();
  for (auto declaration : methods) {
    CurrentSourcePosition::Scope pos_scope(declaration->pos);
    StandardDeclaration* standard_declaration =
        StandardDeclaration::DynamicCast(declaration);
    DCHECK(standard_declaration);
    TorqueMacroDeclaration* method =
        TorqueMacroDeclaration::DynamicCast(standard_declaration->callable);
    Signature signature = MakeSignature(method->signature.get());
    signature.parameter_names.insert(
        signature.parameter_names.begin() + signature.implicit_count,
        kThisParameterName);
    Statement* body = *(standard_declaration->body);
    std::string method_name(method->name);
    if (method->name == kConstructMethodName) {
      signature.parameter_types.types.insert(
          signature.parameter_types.types.begin() + signature.implicit_count,
          constructor_this_type);
      // Constructor
      if (!signature.return_type->IsVoid()) {
        ReportError("constructors musn't have a return type");
      }
      if (signature.labels.size() != 0) {
        ReportError("constructors musn't have labels");
      }
      method_name = kConstructMethodName;
      Declarations::CreateMethod(constructor_this_type, method_name, signature,
                                 false, body);
    } else {
      signature.parameter_types.types.insert(
          signature.parameter_types.types.begin() + signature.implicit_count,
          container_type);
      Declarations::CreateMethod(container_type, method_name, signature, false,
                                 body);
    }
  }

  if (container_type->Constructors().size() != 0) return;

  // Generate default constructor.
  Signature constructor_signature;
  constructor_signature.parameter_types.var_args = false;
  constructor_signature.return_type = TypeOracle::GetVoidType();
  std::vector<const AggregateType*> hierarchy = container_type->GetHierarchy();

  std::vector<Statement*> statements;
  std::vector<Statement*> initializer_statements;

  size_t parameter_number = 0;
  constructor_signature.parameter_names.push_back(kThisParameterName);
  constructor_signature.parameter_types.types.push_back(constructor_this_type);
  std::vector<Expression*> super_arguments;
  for (auto current_type : hierarchy) {
    for (auto& f : current_type->fields()) {
      std::string parameter_name("p" + std::to_string(parameter_number++));
      constructor_signature.parameter_names.push_back(parameter_name);
      constructor_signature.parameter_types.types.push_back(
          f.name_and_type.type);
      IdentifierExpression* value = MakeNode<IdentifierExpression>(
          std::vector<std::string>{}, parameter_name);
      if (container_type != current_type) {
        super_arguments.push_back(MakeNode<IdentifierExpression>(
            std::vector<std::string>{}, parameter_name));
      } else {
        LocationExpression* location = MakeNode<FieldAccessExpression>(
            constructor_this, f.name_and_type.name);
        Statement* statement = MakeNode<ExpressionStatement>(
            MakeNode<AssignmentExpression>(location, base::nullopt, value));
        initializer_statements.push_back(statement);
      }
    }
  }

  if (hierarchy.size() > 1) {
    IdentifierExpression* super_identifier = MakeNode<IdentifierExpression>(
        std::vector<std::string>{}, kSuperMethodName);
    Statement* statement =
        MakeNode<ExpressionStatement>(MakeNode<CallMethodExpression>(
            constructor_this, super_identifier, super_arguments,
            std::vector<std::string>{}));
    statements.push_back(statement);
  }

  for (auto s : initializer_statements) {
    statements.push_back(s);
  }

  Statement* constructor_body = MakeNode<BlockStatement>(false, statements);

  Declarations::CreateMethod(constructor_this_type, kConstructMethodName,
                             constructor_signature, false, constructor_body);
}

void DeclarationVisitor::Visit(StructDeclaration* decl) {
  std::vector<Field> fields;
  size_t offset = 0;
  for (auto& field : decl->fields) {
    const Type* field_type = Declarations::GetType(field.name_and_type.type);
    fields.push_back({field.name_and_type.type->pos,
                      {field.name_and_type.name, field_type},
                      offset,
                      false});
    offset += LoweredSlotCount(field_type);
  }
  StructType* struct_type = Declarations::DeclareStruct(decl->name, fields);
  DeclareMethods(struct_type, decl->methods);
}

void DeclarationVisitor::Visit(ClassDeclaration* decl) {
  // Compute the offset of the class' first member. If the class extends
  // another class, it's the size of the extended class, otherwise zero.
  size_t first_field_offset = 0;
  const Type* super_type = Declarations::LookupType(decl->super);
  if (super_type != TypeOracle::GetTaggedType()) {
    const ClassType* super_class = ClassType::DynamicCast(super_type);
    if (!super_class) {
      ReportError("class \"", decl->name,
                  "\" must extend either Tagged or an already declared class");
    }
    first_field_offset = super_class->size();
  }

  // The generates clause must create a TNode<>
  std::string generates = decl->name;
  if (decl->generates) {
    if (generates.length() < 7 || generates.substr(0, 6) != "TNode<" ||
        generates.substr(generates.length() - 1, 1) != ">") {
      ReportError("generated type \"", generates,
                  "\" should be of the form \"TNode<...>\"");
    }
    generates = generates.substr(6, generates.length() - 7);
  }

  std::vector<Field> fields;
  size_t class_offset = first_field_offset;
  bool seen_strong = false;
  bool seen_weak = false;
  for (ClassFieldExpression& field : decl->fields) {
    const Type* field_type = Declarations::GetType(field.name_and_type.type);
    if (field_type->IsSubtypeOf(TypeOracle::GetTaggedType())) {
      if (field.weak) {
        seen_weak = true;
      } else {
        if (seen_weak) {
          ReportError("cannot declare strong field \"",
                      field.name_and_type.name,
                      "\" after weak Tagged references");
        }
        seen_strong = true;
      }
    } else {
      if (seen_strong || seen_weak) {
        ReportError("cannot declare scalar field \"", field.name_and_type.name,
                    "\" after strong or weak Tagged references");
      }
    }
    if (!field_type->IsSubtypeOf(TypeOracle::GetTaggedType())) {
      ReportError(
          "field \"", field.name_and_type.name, "\" of class \"", decl->name,
          "\" must be a subtype of Tagged (other types not yet supported)");
    }
    fields.push_back({field.name_and_type.type->pos,
                      {field.name_and_type.name, field_type},
                      class_offset,
                      field.weak});
    class_offset += kTaggedSize;
  }

  auto new_class = Declarations::DeclareClass(
      super_type, decl->name, decl->transient, generates, fields, class_offset);
  DeclareMethods(new_class, decl->methods);

  // For each field, construct AST snippits that implement a CSA accessor
  // function and define a corresponding '.field' operator. The
  // implementation iterator will turn the snippits into code.
  for (auto& field : new_class->fields()) {
    IdentifierExpression* parameter =
        MakeNode<IdentifierExpression>(std::string{"o"});

    // Load accessor
    std::string camel_field_name = CamelifyString(field.name_and_type.name);
    std::string load_macro_name = "Load" + new_class->name() + camel_field_name;
    std::string load_operator_name = "." + field.name_and_type.name;
    Signature load_signature;
    load_signature.parameter_names.push_back("o");
    load_signature.parameter_types.types.push_back(new_class);
    load_signature.parameter_types.var_args = false;
    load_signature.return_type = field.name_and_type.type;
    Statement* load_body =
        MakeNode<ReturnStatement>(MakeNode<LoadObjectFieldExpression>(
            parameter, field.name_and_type.name));
    Declarations::DeclareMacro(load_macro_name, base::nullopt, load_signature,
                               false, load_body, load_operator_name);

    // Store accessor
    IdentifierExpression* value = MakeNode<IdentifierExpression>(
        std::vector<std::string>{}, std::string{"v"});
    std::string store_macro_name =
        "Store" + new_class->name() + camel_field_name;
    std::string store_operator_name = "." + field.name_and_type.name + "=";
    Signature store_signature;
    store_signature.parameter_names.push_back("o");
    store_signature.parameter_names.push_back("v");
    store_signature.parameter_types.types.push_back(new_class);
    store_signature.parameter_types.types.push_back(field.name_and_type.type);
    store_signature.parameter_types.var_args = false;
    // TODO(danno): Store macros probably should return their value argument
    store_signature.return_type = TypeOracle::GetVoidType();
    Statement* store_body =
        MakeNode<ExpressionStatement>(MakeNode<StoreObjectFieldExpression>(
            parameter, field.name_and_type.name, value));
    Declarations::DeclareMacro(store_macro_name, base::nullopt, store_signature,
                               false, store_body, store_operator_name);
  }

  GlobalContext::RegisterClass(decl->name, new_class);
}

void DeclarationVisitor::Visit(CppIncludeDeclaration* decl) {
  GlobalContext::AddCppInclude(decl->include_path);
}

void DeclarationVisitor::Visit(TypeDeclaration* decl) {
  std::string generates = decl->generates ? *decl->generates : std::string("");
  if (decl->generates) {
    if (generates.length() < 7 || generates.substr(0, 6) != "TNode<" ||
        generates.substr(generates.length() - 1, 1) != ">") {
      ReportError("generated type \"", generates,
                  "\" should be of the form \"TNode<...>\"");
    }
    generates = generates.substr(6, generates.length() - 7);
  }

  const AbstractType* type = Declarations::DeclareAbstractType(
      decl->name, decl->transient, generates, {}, decl->extends);

  if (decl->constexpr_generates) {
    if (decl->transient) {
      ReportError("cannot declare a transient type that is also constexpr");
    }
    std::string constexpr_name = CONSTEXPR_TYPE_PREFIX + decl->name;
    base::Optional<std::string> constexpr_extends;
    if (decl->extends)
      constexpr_extends = CONSTEXPR_TYPE_PREFIX + *decl->extends;
    Declarations::DeclareAbstractType(constexpr_name, false,
                                      *decl->constexpr_generates, type,
                                      constexpr_extends);
  }
}

void DeclarationVisitor::DeclareSpecializedTypes(const SpecializationKey& key) {
  size_t i = 0;
  const std::size_t generic_parameter_count =
      key.generic->declaration()->generic_parameters.size();
  if (generic_parameter_count != key.specialized_types.size()) {
    std::stringstream stream;
    stream << "Wrong generic argument count for specialization of \""
           << key.generic->name() << "\", expected: " << generic_parameter_count
           << ", actual: " << key.specialized_types.size();
    ReportError(stream.str());
  }

  for (auto type : key.specialized_types) {
    std::string generic_type_name =
        key.generic->declaration()->generic_parameters[i++];
    Declarations::DeclareType(generic_type_name, type, true);
  }
}

Signature DeclarationVisitor::MakeSpecializedSignature(
    const SpecializationKey& key) {
  CurrentScope::Scope generic_scope(key.generic->ParentScope());
  // Create a temporary fake-namespace just to temporarily declare the
  // specialization aliases for the generic types to create a signature.
  Namespace tmp_namespace("_tmp");
  CurrentScope::Scope tmp_namespace_scope(&tmp_namespace);
  DeclareSpecializedTypes(key);
  return MakeSignature(key.generic->declaration()->callable->signature.get());
}

Callable* DeclarationVisitor::SpecializeImplicit(const SpecializationKey& key) {
  if (!key.generic->declaration()->body &&
      IntrinsicDeclaration::DynamicCast(key.generic->declaration()->callable) ==
          nullptr) {
    ReportError("missing specialization of ", key.generic->name(),
                " with types <", key.specialized_types, "> declared at ",
                key.generic->pos());
  }
  CurrentScope::Scope generic_scope(key.generic->ParentScope());
  Callable* result =
      Specialize(key, key.generic->declaration()->callable, base::nullopt,
                 key.generic->declaration()->body);
  CurrentScope::Scope callable_scope(result);
  DeclareSpecializedTypes(key);
  return result;
}

Callable* DeclarationVisitor::Specialize(
    const SpecializationKey& key, CallableNode* declaration,
    base::Optional<const CallableNodeSignature*> signature,
    base::Optional<Statement*> body) {
  // TODO(tebbi): The error should point to the source position where the
  // instantiation was requested.
  CurrentSourcePosition::Scope pos_scope(key.generic->declaration()->pos);
  size_t generic_parameter_count =
      key.generic->declaration()->generic_parameters.size();
  if (generic_parameter_count != key.specialized_types.size()) {
    std::stringstream stream;
    stream << "number of template parameters ("
           << std::to_string(key.specialized_types.size())
           << ") to intantiation of generic " << declaration->name
           << " doesnt match the generic's declaration ("
           << std::to_string(generic_parameter_count) << ")";
    ReportError(stream.str());
  }
  if (key.generic->GetSpecialization(key.specialized_types)) {
    ReportError("cannot redeclare specialization of ", key.generic->name(),
                " with types <", key.specialized_types, ">");
  }

  Signature type_signature =
      signature ? MakeSignature(*signature) : MakeSpecializedSignature(key);

  std::string generated_name = Declarations::GetGeneratedCallableName(
      declaration->name, key.specialized_types);
  std::stringstream readable_name;
  readable_name << declaration->name << "<";
  bool first = true;
  for (const Type* t : key.specialized_types) {
    if (!first) readable_name << ", ";
    readable_name << *t;
    first = false;
  }
  readable_name << ">";
  Callable* callable;
  if (MacroDeclaration::DynamicCast(declaration) != nullptr) {
    callable = Declarations::CreateMacro(generated_name, readable_name.str(),
                                         base::nullopt, type_signature,
                                         declaration->transitioning, *body);
  } else if (IntrinsicDeclaration::DynamicCast(declaration) != nullptr) {
    callable = Declarations::CreateIntrinsic(declaration->name, type_signature);
  } else {
    BuiltinDeclaration* builtin = BuiltinDeclaration::cast(declaration);
    callable = CreateBuiltin(builtin, generated_name, readable_name.str(),
                             type_signature, *body);
  }
  key.generic->AddSpecialization(key.specialized_types, callable);
  return callable;
}

}  // namespace torque
}  // namespace internal
}  // namespace v8
