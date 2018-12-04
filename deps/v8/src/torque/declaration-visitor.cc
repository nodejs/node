// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/torque/declaration-visitor.h"

namespace v8 {
namespace internal {
namespace torque {

void DeclarationVisitor::Visit(Expression* expr) {
  CurrentSourcePosition::Scope scope(expr->pos);
  switch (expr->kind) {
#define ENUM_ITEM(name)        \
  case AstNode::Kind::k##name: \
    return Visit(name::cast(expr));
    AST_EXPRESSION_NODE_KIND_LIST(ENUM_ITEM)
#undef ENUM_ITEM
    default:
      UNIMPLEMENTED();
  }
}

void DeclarationVisitor::Visit(Statement* stmt) {
  CurrentSourcePosition::Scope scope(stmt->pos);
  switch (stmt->kind) {
#define ENUM_ITEM(name)        \
  case AstNode::Kind::k##name: \
    return Visit(name::cast(stmt));
    AST_STATEMENT_NODE_KIND_LIST(ENUM_ITEM)
#undef ENUM_ITEM
    default:
      UNIMPLEMENTED();
  }
}

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
                               Statement* body) {
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

Builtin* DeclarationVisitor::BuiltinDeclarationCommon(
    BuiltinDeclaration* decl, bool external, const Signature& signature) {
  const bool javascript = decl->javascript_linkage;
  const bool varargs = decl->signature->parameters.has_varargs;
  Builtin::Kind kind = !javascript ? Builtin::kStub
                                   : varargs ? Builtin::kVarArgsJavaScript
                                             : Builtin::kFixedArgsJavaScript;

  if (signature.types().size() == 0 ||
      !(signature.types()[0] ==
        declarations()->LookupGlobalType(CONTEXT_TYPE_STRING))) {
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
          declarations()->LookupGlobalType(OBJECT_TYPE_STRING))) {
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

  std::string generated_name = GetGeneratedCallableName(
      decl->name, declarations()->GetCurrentSpecializationTypeNamesVector());
  return declarations()->DeclareBuiltin(generated_name, kind, external,
                                        signature);
}

void DeclarationVisitor::Visit(ExternalRuntimeDeclaration* decl,
                               const Signature& signature, Statement* body) {
  if (global_context_.verbose()) {
    std::cout << "found declaration of external runtime " << decl->name
              << " with signature ";
  }

  if (signature.parameter_types.types.size() == 0 ||
      !(signature.parameter_types.types[0] ==
        declarations()->LookupGlobalType(CONTEXT_TYPE_STRING))) {
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

  declarations()->DeclareRuntimeFunction(decl->name, signature);
}

void DeclarationVisitor::Visit(ExternalMacroDeclaration* decl,
                               const Signature& signature, Statement* body) {
  if (global_context_.verbose()) {
    std::cout << "found declaration of external macro " << decl->name
              << " with signature ";
  }

  std::string generated_name = GetGeneratedCallableName(
      decl->name, declarations()->GetCurrentSpecializationTypeNamesVector());
  declarations()->DeclareMacro(generated_name, signature, decl->op);
}

void DeclarationVisitor::Visit(TorqueBuiltinDeclaration* decl,
                               const Signature& signature, Statement* body) {
  Builtin* builtin = BuiltinDeclarationCommon(decl, false, signature);
  CurrentCallableActivator activator(global_context_, builtin, decl);
  DeclareSignature(signature);
  if (signature.parameter_types.var_args) {
    declarations()->DeclareExternConstant(
        decl->signature->parameters.arguments_variable,
        TypeOracle::GetArgumentsType(), "arguments");
  }
  torque_builtins_.push_back(builtin);
  Visit(body);
}

void DeclarationVisitor::Visit(TorqueMacroDeclaration* decl,
                               const Signature& signature, Statement* body) {
  std::string generated_name = GetGeneratedCallableName(
      decl->name, declarations()->GetCurrentSpecializationTypeNamesVector());
  Macro* macro =
      declarations()->DeclareMacro(generated_name, signature, decl->op);

  CurrentCallableActivator activator(global_context_, macro, decl);

  DeclareSignature(signature);

  if (body != nullptr) {
    Visit(body);
  }
}

void DeclarationVisitor::Visit(ConstDeclaration* decl) {
  declarations()->DeclareModuleConstant(decl->name,
                                        declarations()->GetType(decl->type));
  Visit(decl->expression);
}

void DeclarationVisitor::Visit(StandardDeclaration* decl) {
  Signature signature = MakeSignature(decl->callable->signature.get());
  Visit(decl->callable, signature, decl->body);
}

void DeclarationVisitor::Visit(GenericDeclaration* decl) {
  declarations()->DeclareGeneric(decl->callable->name, CurrentModule(), decl);
}

void DeclarationVisitor::Visit(SpecializationDeclaration* decl) {
  if ((decl->body != nullptr) == decl->external) {
    std::stringstream stream;
    stream << "specialization of " << decl->name
           << " must either be marked 'extern' or have a body";
    ReportError(stream.str());
  }

  GenericList* generic_list = declarations()->LookupGeneric(decl->name);
  // Find the matching generic specialization based on the concrete parameter
  // list.
  CallableNode* matching_callable = nullptr;
  SpecializationKey matching_key;
  Signature signature_with_types = MakeSignature(decl->signature.get());
  for (Generic* generic : generic_list->list()) {
    SpecializationKey key = {generic, GetTypeVector(decl->generic_parameters)};
    CallableNode* callable_candidate = generic->declaration()->callable;
    // Abuse the Specialization nodes' scope to temporarily declare the
    // specialization aliases for the generic types to compare signatures. This
    // scope is never used for anything else, so it's OK to pollute it.
    Declarations::CleanNodeScopeActivator specialization_activator(
        declarations(), decl);
    DeclareSpecializedTypes(key);
    Signature generic_signature_with_types =
        MakeSignature(generic->declaration()->callable->signature.get());
    if (signature_with_types.HasSameTypesAs(generic_signature_with_types)) {
      if (matching_callable != nullptr) {
        std::stringstream stream;
        stream << "specialization of " << callable_candidate->name
               << " is ambigous, it matches more than one generic declaration ("
               << *matching_key.first << " and " << *key.first << ")";
        ReportError(stream.str());
      }
      matching_callable = callable_candidate;
      matching_key = key;
    }
  }

  if (matching_callable == nullptr) {
    std::stringstream stream;
    stream << "specialization of " << decl->name
           << " doesn't match any generic declaration";
    ReportError(stream.str());
  }

  // Make sure the declarations of the parameter types for the specialization
  // are the ones from the matching generic.
  {
    Declarations::CleanNodeScopeActivator specialization_activator(
        declarations(), decl);
    DeclareSpecializedTypes(matching_key);
  }

  SpecializeGeneric({matching_key, matching_callable, decl->signature.get(),
                     decl->body, decl->pos});
}

void DeclarationVisitor::Visit(ReturnStatement* stmt) {
  if (stmt->value) {
    Visit(*stmt->value);
  }
}

Variable* DeclarationVisitor::DeclareVariable(const std::string& name,
                                              const Type* type, bool is_const) {
  Variable* result = declarations()->DeclareVariable(name, type, is_const);
  return result;
}

Parameter* DeclarationVisitor::DeclareParameter(const std::string& name,
                                                const Type* type) {
  return declarations()->DeclareParameter(
      name, GetParameterVariableFromName(name), type);
}

void DeclarationVisitor::Visit(VarDeclarationStatement* stmt) {
  std::string variable_name = stmt->name;
  if (!stmt->const_qualified) {
    if (!stmt->type) {
      ReportError(
          "variable declaration is missing type. Only 'const' bindings can "
          "infer the type.");
    }
    const Type* type = declarations()->GetType(*stmt->type);
    if (type->IsConstexpr()) {
      ReportError(
          "cannot declare variable with constexpr type. Use 'const' instead.");
    }
    DeclareVariable(variable_name, type, stmt->const_qualified);
    if (global_context_.verbose()) {
      std::cout << "declared variable " << variable_name << " with type "
                << *type << "\n";
    }
  }

  // const qualified variables are required to be initialized properly.
  if (stmt->const_qualified && !stmt->initializer) {
    std::stringstream stream;
    stream << "local constant \"" << variable_name << "\" is not initialized.";
    ReportError(stream.str());
  }

  if (stmt->initializer) {
    Visit(*stmt->initializer);
    if (global_context_.verbose()) {
      std::cout << "variable has initialization expression at "
                << CurrentPositionAsString() << "\n";
    }
  }
}

void DeclarationVisitor::Visit(ExternConstDeclaration* decl) {
  const Type* type = declarations()->GetType(decl->type);
  if (!type->IsConstexpr()) {
    std::stringstream stream;
    stream << "extern constants must have constexpr type, but found: \""
           << *type << "\"\n";
    ReportError(stream.str());
  }

  declarations()->DeclareExternConstant(decl->name, type, decl->literal);
}

void DeclarationVisitor::Visit(StructDeclaration* decl) {
  std::vector<NameAndType> fields;
  for (auto& field : decl->fields) {
    const Type* field_type = declarations()->GetType(field.type);
    fields.push_back({field.name, field_type});
  }
  declarations()->DeclareStruct(CurrentModule(), decl->name, fields);
}

void DeclarationVisitor::Visit(LogicalOrExpression* expr) {
  {
    Declarations::NodeScopeActivator scope(declarations(), expr->left);
    declarations()->DeclareLabel(kFalseLabelName);
    Visit(expr->left);
  }
  Visit(expr->right);
}

void DeclarationVisitor::Visit(LogicalAndExpression* expr) {
  {
    Declarations::NodeScopeActivator scope(declarations(), expr->left);
    declarations()->DeclareLabel(kTrueLabelName);
    Visit(expr->left);
  }
  Visit(expr->right);
}

void DeclarationVisitor::DeclareExpressionForBranch(
    Expression* node, base::Optional<Statement*> true_statement,
    base::Optional<Statement*> false_statement) {
  Declarations::NodeScopeActivator scope(declarations(), node);
  // Conditional expressions can either explicitly return a bit
  // type, or they can be backed by macros that don't return but
  // take a true and false label. By declaring the labels before
  // visiting the conditional expression, those label-based
  // macro conditionals will be able to find them through normal
  // label lookups.
  declarations()->DeclareLabel(kTrueLabelName, true_statement);
  declarations()->DeclareLabel(kFalseLabelName, false_statement);
  Visit(node);
}

void DeclarationVisitor::Visit(ConditionalExpression* expr) {
  DeclareExpressionForBranch(expr->condition);
  Visit(expr->if_true);
  Visit(expr->if_false);
}

void DeclarationVisitor::Visit(IfStatement* stmt) {
  DeclareExpressionForBranch(stmt->condition, stmt->if_true, stmt->if_false);
  Visit(stmt->if_true);
  if (stmt->if_false) Visit(*stmt->if_false);
}

void DeclarationVisitor::Visit(WhileStatement* stmt) {
  Declarations::NodeScopeActivator scope(declarations(), stmt);
  DeclareExpressionForBranch(stmt->condition);
  Visit(stmt->body);
}

void DeclarationVisitor::Visit(ForOfLoopStatement* stmt) {
  // Scope for for iteration variable
  Declarations::NodeScopeActivator scope(declarations(), stmt);
  Visit(stmt->var_declaration);
  Visit(stmt->iterable);
  if (stmt->begin) Visit(*stmt->begin);
  if (stmt->end) Visit(*stmt->end);
  Visit(stmt->body);
}

void DeclarationVisitor::Visit(ForLoopStatement* stmt) {
  Declarations::NodeScopeActivator scope(declarations(), stmt);
  if (stmt->var_declaration) Visit(*stmt->var_declaration);

  // Same as DeclareExpressionForBranch, but without the extra scope.
  // If no test expression is present we can not use it for the scope.
  declarations()->DeclareLabel(kTrueLabelName);
  declarations()->DeclareLabel(kFalseLabelName);
  if (stmt->test) Visit(*stmt->test);

  Visit(stmt->body);
  if (stmt->action) Visit(*stmt->action);
}

void DeclarationVisitor::Visit(TryLabelExpression* stmt) {
  // Activate a new scope to declare the handler's label parameters, they should
  // not be visible outside the label block.
  {
    Declarations::NodeScopeActivator scope(declarations(), stmt);

    // Declare label
    {
      LabelBlock* block = stmt->label_block;
      CurrentSourcePosition::Scope scope(block->pos);
      Label* shared_label =
          declarations()->DeclareLabel(block->label, block->body);
      {
        Declarations::NodeScopeActivator scope(declarations(), block->body);
        if (block->parameters.has_varargs) {
          std::stringstream stream;
          stream << "cannot use ... for label parameters";
          ReportError(stream.str());
        }

        size_t i = 0;
        for (const auto& p : block->parameters.names) {
          const Type* type =
              declarations()->GetType(block->parameters.types[i]);
          if (type->IsConstexpr()) {
            ReportError("no constexpr type allowed for label arguments");
          }

          shared_label->AddVariable(DeclareVariable(p, type, false));
          ++i;
        }
        if (global_context_.verbose()) {
          std::cout << " declaring label " << block->label << "\n";
        }
      }
    }

    Visit(stmt->try_expression);
  }

  Visit(stmt->label_block->body);
}

void DeclarationVisitor::GenerateHeader(std::string& file_name) {
  std::stringstream new_contents_stream;
  new_contents_stream
      << "#ifndef V8_BUILTINS_BUILTIN_DEFINITIONS_FROM_DSL_H_\n"
         "#define V8_BUILTINS_BUILTIN_DEFINITIONS_FROM_DSL_H_\n"
         "\n"
         "#define BUILTIN_LIST_FROM_DSL(CPP, API, TFJ, TFC, TFS, TFH, ASM) "
         "\\\n";
  for (auto builtin : torque_builtins_) {
    int firstParameterIndex = 1;
    bool declareParameters = true;
    if (builtin->IsStub()) {
      new_contents_stream << "TFS(" << builtin->name();
    } else {
      new_contents_stream << "TFJ(" << builtin->name();
      if (builtin->IsVarArgsJavaScript()) {
        new_contents_stream
            << ", SharedFunctionInfo::kDontAdaptArgumentsSentinel";
        declareParameters = false;
      } else {
        assert(builtin->IsFixedArgsJavaScript());
        // FixedArg javascript builtins need to offer the parameter
        // count.
        assert(builtin->parameter_names().size() >= 2);
        new_contents_stream << ", " << (builtin->parameter_names().size() - 2);
        // And the receiver is explicitly declared.
        new_contents_stream << ", kReceiver";
        firstParameterIndex = 2;
      }
    }
    if (declareParameters) {
      int index = 0;
      for (const auto& parameter : builtin->parameter_names()) {
        if (index >= firstParameterIndex) {
          new_contents_stream << ", k" << CamelifyString(parameter);
        }
        index++;
      }
    }
    new_contents_stream << ") \\\n";
  }
  new_contents_stream
      << "\n"
         "#endif  // V8_BUILTINS_BUILTIN_DEFINITIONS_FROM_DSL_H_\n";

  std::string new_contents(new_contents_stream.str());
  ReplaceFileContentsIfDifferent(file_name, new_contents);
}

void DeclarationVisitor::Visit(IdentifierExpression* expr) {
  if (expr->generic_arguments.size() != 0) {
    TypeVector specialization_types;
    for (auto t : expr->generic_arguments) {
      specialization_types.push_back(declarations()->GetType(t));
    }
    // Specialize all versions of the generic, since the exact parameter type
    // list cannot be resolved until the call's parameter expressions are
    // evaluated. This is an overly conservative but simple way to make sure
    // that the correct specialization exists.
    for (auto generic : declarations()->LookupGeneric(expr->name)->list()) {
      CallableNode* callable = generic->declaration()->callable;
      if (generic->declaration()->body) {
        QueueGenericSpecialization({generic, specialization_types}, callable,
                                   callable->signature.get(),
                                   generic->declaration()->body);
      }
    }
  }
}

void DeclarationVisitor::Visit(StatementExpression* expr) {
  Visit(expr->statement);
}

void DeclarationVisitor::Visit(CallExpression* expr) {
  Visit(&expr->callee);
  for (Expression* arg : expr->arguments) Visit(arg);
}

void DeclarationVisitor::Visit(TypeDeclaration* decl) {
  std::string generates = decl->generates ? *decl->generates : std::string("");
  const AbstractType* type = declarations()->DeclareAbstractType(
      decl->name, generates, {}, decl->extends);

  if (decl->constexpr_generates) {
    std::string constexpr_name = CONSTEXPR_TYPE_PREFIX + decl->name;
    base::Optional<std::string> constexpr_extends;
    if (decl->extends)
      constexpr_extends = CONSTEXPR_TYPE_PREFIX + *decl->extends;
    declarations()->DeclareAbstractType(
        constexpr_name, *decl->constexpr_generates, type, constexpr_extends);
  }
}

void DeclarationVisitor::DeclareSignature(const Signature& signature) {
  auto type_iterator = signature.parameter_types.types.begin();
  for (const auto& name : signature.parameter_names) {
    const Type* t(*type_iterator++);
    if (name.size() != 0) {
      DeclareParameter(name, t);
    }
  }
  for (auto& label : signature.labels) {
    auto label_params = label.types;
    Label* new_label = declarations()->DeclareLabel(label.name);
    new_label->set_external_label_name("label_" + label.name);
    size_t i = 0;
    for (auto var_type : label_params) {
      if (var_type->IsConstexpr()) {
        ReportError("no constexpr type allowed for label arguments");
      }

      std::string var_name = label.name + std::to_string(i++);
      new_label->AddVariable(
          declarations()->CreateVariable(var_name, var_type, false));
    }
  }
}

void DeclarationVisitor::DeclareSpecializedTypes(const SpecializationKey& key) {
  size_t i = 0;
  Generic* generic = key.first;
  const std::size_t generic_parameter_count =
      generic->declaration()->generic_parameters.size();
  if (generic_parameter_count != key.second.size()) {
    std::stringstream stream;
    stream << "Wrong generic argument count for specialization of \""
           << generic->name() << "\", expected: " << generic_parameter_count
           << ", actual: " << key.second.size();
    ReportError(stream.str());
  }

  for (auto type : key.second) {
    std::string generic_type_name =
        generic->declaration()->generic_parameters[i++];
    declarations()->DeclareType(generic_type_name, type);
  }
}

void DeclarationVisitor::Specialize(const SpecializationKey& key,
                                    CallableNode* callable,
                                    const CallableNodeSignature* signature,
                                    Statement* body) {
  Generic* generic = key.first;

  // TODO(tebbi): The error should point to the source position where the
  // instantiation was requested.
  CurrentSourcePosition::Scope pos_scope(generic->declaration()->pos);
  size_t generic_parameter_count =
      generic->declaration()->generic_parameters.size();
  if (generic_parameter_count != key.second.size()) {
    std::stringstream stream;
    stream << "number of template parameters ("
           << std::to_string(key.second.size())
           << ") to intantiation of generic " << callable->name
           << " doesnt match the generic's declaration ("
           << std::to_string(generic_parameter_count) << ")";
    ReportError(stream.str());
  }

  Signature type_signature;
  {
    // Manually activate the specialized generic's scope when declaring the
    // generic parameter specializations.
    Declarations::GenericScopeActivator namespace_scope(declarations(), key);
    DeclareSpecializedTypes(key);
    type_signature = MakeSignature(signature);
  }

  Visit(callable, type_signature, body);
}

}  // namespace torque
}  // namespace internal
}  // namespace v8
