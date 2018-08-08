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
             << " is " << signature.types()[1] << " but should be Object";
      ReportError(stream.str());
    }
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
  declarations()->DeclareMacro(generated_name, signature);
  if (decl->op) {
    OperationHandler handler(
        {decl->name, signature.parameter_types, signature.return_type});
    auto i = global_context_.op_handlers_.find(*decl->op);
    if (i == global_context_.op_handlers_.end()) {
      global_context_.op_handlers_[*decl->op] = std::vector<OperationHandler>();
      i = global_context_.op_handlers_.find(*decl->op);
    }
    i->second.push_back(handler);
  }

  if (decl->implicit) {
    if (!decl->op || *decl->op != "convert<>") {
      ReportError("implicit can only be used with cast<> operator");
    }

    const TypeVector& parameter_types = signature.parameter_types.types;
    if (parameter_types.size() != 1 || signature.parameter_types.var_args) {
      ReportError(
          "implicit cast operators doesn't only have a single parameter");
    }
    GetTypeOracle().RegisterImplicitConversion(signature.return_type,
                                               parameter_types[0]);
  }
}

void DeclarationVisitor::Visit(TorqueBuiltinDeclaration* decl,
                               const Signature& signature, Statement* body) {
  Builtin* builtin = BuiltinDeclarationCommon(decl, false, signature);
  CurrentCallableActivator activator(global_context_, builtin, decl);
  DeclareSignature(signature);
  if (signature.parameter_types.var_args) {
    declarations()->DeclareConstant(
        decl->signature->parameters.arguments_variable,
        GetTypeOracle().GetArgumentsType(), "arguments");
  }
  torque_builtins_.push_back(builtin);
  Visit(body);
}

void DeclarationVisitor::Visit(TorqueMacroDeclaration* decl,
                               const Signature& signature, Statement* body) {
  std::string generated_name = GetGeneratedCallableName(
      decl->name, declarations()->GetCurrentSpecializationTypeNamesVector());
  Macro* macro = declarations()->DeclareMacro(generated_name, signature);

  CurrentCallableActivator activator(global_context_, macro, decl);

  DeclareSignature(signature);
  if (!signature.return_type->IsVoidOrNever()) {
    declarations()->DeclareVariable(kReturnValueVariable,
                                    signature.return_type);
  }

  PushControlSplit();
  Visit(body);
  auto changed_vars = PopControlSplit();
  global_context_.AddControlSplitChangedVariables(
      decl, declarations()->GetCurrentSpecializationTypeNamesVector(),
      changed_vars);
}

void DeclarationVisitor::Visit(StandardDeclaration* decl) {
  Signature signature =
      MakeSignature(decl->callable, decl->callable->signature.get());
  Visit(decl->callable, signature, decl->body);
}

void DeclarationVisitor::Visit(GenericDeclaration* decl) {
  declarations()->DeclareGeneric(decl->callable->name, CurrentModule(), decl);
}

void DeclarationVisitor::Visit(SpecializationDeclaration* decl) {
  Generic* generic = declarations()->LookupGeneric(decl->name);
  SpecializationKey key = {generic, GetTypeVector(decl->generic_parameters)};
  CallableNode* callable = generic->declaration()->callable;

  {
    Signature signature_with_types =
        MakeSignature(callable, decl->signature.get());
    // Abuse the Specialization nodes' scope to temporarily declare the
    // specialization aliases for the generic types to compare signatures. This
    // scope is never used for anything else, so it's OK to pollute it.
    Declarations::NodeScopeActivator specialization_activator(declarations(),
                                                              decl);
    DeclareSpecializedTypes(key);
    Signature generic_signature_with_types =
        MakeSignature(generic->declaration()->callable,
                      generic->declaration()->callable->signature.get());
    if (!signature_with_types.HasSameTypesAs(generic_signature_with_types)) {
      std::stringstream stream;
      stream << "specialization of " << callable->name
             << " has incompatible parameter list or label list with generic "
                "definition";
      ReportError(stream.str());
    }
  }

  SpecializeGeneric({key, callable, decl->signature.get(), decl->body});
}

void DeclarationVisitor::Visit(ReturnStatement* stmt) {
  const Callable* callable = global_context_.GetCurrentCallable();
  if (callable->IsMacro() && callable->HasReturnValue()) {
    MarkVariableModified(
        Variable::cast(declarations()->LookupValue(kReturnValueVariable)));
  }
  if (stmt->value) {
    Visit(*stmt->value);
  }
}

void DeclarationVisitor::Visit(ForOfLoopStatement* stmt) {
  // Scope for for iteration variable
  Declarations::NodeScopeActivator scope(declarations(), stmt);
  Visit(stmt->var_declaration);
  Visit(stmt->iterable);
  if (stmt->begin) Visit(*stmt->begin);
  if (stmt->end) Visit(*stmt->end);
  PushControlSplit();
  Visit(stmt->body);
  auto changed_vars = PopControlSplit();
  global_context_.AddControlSplitChangedVariables(
      stmt, declarations()->GetCurrentSpecializationTypeNamesVector(),
      changed_vars);
}

void DeclarationVisitor::Visit(TryCatchStatement* stmt) {
  // Activate a new scope to declare catch handler labels, they should not be
  // visible outside the catch.
  {
    Declarations::NodeScopeActivator scope(declarations(), stmt);

    // Declare catch labels
    for (LabelBlock* block : stmt->label_blocks) {
      CurrentSourcePosition::Scope scope(block->pos);
      Label* shared_label = declarations()->DeclareLabel(block->label);
      {
        Declarations::NodeScopeActivator scope(declarations(), block->body);
        if (block->parameters.has_varargs) {
          std::stringstream stream;
          stream << "cannot use ... for label parameters";
          ReportError(stream.str());
        }

        size_t i = 0;
        for (auto p : block->parameters.names) {
          shared_label->AddVariable(declarations()->DeclareVariable(
              p, declarations()->GetType(block->parameters.types[i])));
          ++i;
        }
      }
      if (global_context_.verbose()) {
        std::cout << " declaring catch for exception " << block->label << "\n";
      }
    }

    // Try catch not supported yet
    DCHECK_EQ(stmt->catch_blocks.size(), 0);

    Visit(stmt->try_block);
  }

  for (CatchBlock* block : stmt->catch_blocks) {
    Visit(block->body);
  }

  for (LabelBlock* block : stmt->label_blocks) {
    Visit(block->body);
  }
}

void DeclarationVisitor::Visit(IdentifierExpression* expr) {
  if (expr->generic_arguments.size() != 0) {
    Generic* generic = declarations()->LookupGeneric(expr->name);
    TypeVector specialization_types;
    for (auto t : expr->generic_arguments) {
      specialization_types.push_back(declarations()->GetType(t));
    }
    CallableNode* callable = generic->declaration()->callable;
    QueueGenericSpecialization({generic, specialization_types}, callable,
                               callable->signature.get(),
                               generic->declaration()->body);
  }
}

void DeclarationVisitor::Visit(CallExpression* expr) {
  Visit(&expr->callee);
  for (Expression* arg : expr->arguments) Visit(arg);
}

void DeclarationVisitor::DeclareSpecializedTypes(const SpecializationKey& key) {
  size_t i = 0;
  Generic* generic = key.first;
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
  CurrentSourcePosition::Scope scope(generic->declaration()->pos);
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

  {
    // Manually activate the specialized generic's scope when declaring the
    // generic parameter specializations.
    Declarations::GenericScopeActivator scope(declarations(), key);
    DeclareSpecializedTypes(key);
  }

  Visit(callable, MakeSignature(callable, signature), body);
}

}  // namespace torque
}  // namespace internal
}  // namespace v8
