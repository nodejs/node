// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "src/torque/implementation-visitor.h"
#include "src/torque/parameter-difference.h"

#include "include/v8.h"

namespace v8 {
namespace internal {
namespace torque {

VisitResult ImplementationVisitor::Visit(Expression* expr) {
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
  return VisitResult();
}

const Type* ImplementationVisitor::Visit(Statement* stmt) {
  CurrentSourcePosition::Scope scope(stmt->pos);
  GenerateIndent();
  source_out() << "// " << CurrentPositionAsString() << "\n";
  switch (stmt->kind) {
#define ENUM_ITEM(name)        \
  case AstNode::Kind::k##name: \
    return Visit(name::cast(stmt));
    AST_STATEMENT_NODE_KIND_LIST(ENUM_ITEM)
#undef ENUM_ITEM
    default:
      UNIMPLEMENTED();
  }
  UNREACHABLE();
  return nullptr;
}

void ImplementationVisitor::Visit(Declaration* decl) {
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

void ImplementationVisitor::Visit(CallableNode* decl,
                                  const Signature& signature, Statement* body) {
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

void ImplementationVisitor::BeginModuleFile(Module* module) {
  std::ostream& source = module->source_stream();
  std::ostream& header = module->header_stream();

  if (module->IsDefault()) {
    source << "#include \"src/code-stub-assembler.h\"";
  } else {
    source << "#include \"src/builtins/builtins-" +
                  DashifyString(module->name()) + "-gen.h\"";
  }
  source << std::endl;
  source << "#include \"src/builtins/builtins-utils-gen.h\"" << std::endl;
  source << "#include \"src/builtins/builtins.h\"" << std::endl;
  source << "#include \"src/code-factory.h\"" << std::endl;
  source << "#include \"src/elements-kind.h\"" << std::endl;
  source << "#include \"src/heap/factory-inl.h\"" << std::endl;
  source << "#include \"src/objects.h\"" << std::endl;

  source << "#include \"builtins-" + DashifyString(module->name()) +
                "-from-dsl-gen.h\"";
  source << std::endl << std::endl;

  source << "namespace v8 {" << std::endl
         << "namespace internal {" << std::endl
         << "" << std::endl
         << "using Node = compiler::Node;" << std::endl
         << "" << std::endl;

  std::string upper_name(module->name());
  transform(upper_name.begin(), upper_name.end(), upper_name.begin(),
            ::toupper);
  std::string headerDefine =
      std::string("V8_TORQUE_") + upper_name + "_FROM_DSL_BASE_H__";
  header << "#ifndef " << headerDefine << std::endl;
  header << "#define " << headerDefine << std::endl << std::endl;
  if (module->IsDefault()) {
    header << "#include \"src/code-stub-assembler.h\"";
  } else {
    header << "#include \"src/builtins/builtins-" +
                  DashifyString(module->name()) + "-gen.h\""
           << std::endl;
  }
  header << std::endl << std::endl;

  header << "namespace v8 {" << std::endl
         << "namespace internal {" << std::endl
         << "" << std::endl;

  header << "class " << GetDSLAssemblerName(module) << ": public "
         << GetBaseAssemblerName(module) << " {" << std::endl;
  header << " public:" << std::endl;
  header << "  explicit " << GetDSLAssemblerName(module)
         << "(compiler::CodeAssemblerState* state) : "
         << GetBaseAssemblerName(module) << "(state) {}" << std::endl;

  header << std::endl;
  header << "  using Node = compiler::Node;" << std::endl;
  header << "  template <class T>" << std::endl;
  header << "  using TNode = compiler::TNode<T>;" << std::endl;
  header << "  template <class T>" << std::endl;
  header << "  using SloppyTNode = compiler::SloppyTNode<T>;" << std::endl
         << std::endl;
}

void ImplementationVisitor::EndModuleFile(Module* module) {
  std::ostream& source = module->source_stream();
  std::ostream& header = module->header_stream();

  DrainSpecializationQueue();

  std::string upper_name(module->name());
  transform(upper_name.begin(), upper_name.end(), upper_name.begin(),
            ::toupper);
  std::string headerDefine =
      std::string("V8_TORQUE_") + upper_name + "_FROM_DSL_BASE_H__";

  source << "}  // namepsace internal" << std::endl
         << "}  // namespace v8" << std::endl
         << "" << std::endl;

  header << "};" << std::endl << "" << std::endl;
  header << "}  // namepsace internal" << std::endl
         << "}  // namespace v8" << std::endl
         << "" << std::endl;
  header << "#endif  // " << headerDefine << std::endl;
}

void ImplementationVisitor::Visit(ModuleDeclaration* decl) {
  Module* module = decl->GetModule();
  Module* saved_module = module_;
  module_ = module;
  Declarations::ModuleScopeActivator scope(declarations(), decl->GetModule());
  for (auto& child : decl->declarations) Visit(child);
  module_ = saved_module;
}

void ImplementationVisitor::Visit(ConstDeclaration* decl) {
  Signature signature = MakeSignatureFromReturnType(decl->type);
  std::string name = decl->name;

  header_out() << "  ";
  GenerateFunctionDeclaration(header_out(), "", name, signature, {});
  header_out() << ";\n";

  GenerateFunctionDeclaration(source_out(),
                              GetDSLAssemblerName(CurrentModule()) + "::", name,
                              signature, {});
  source_out() << " {\n";

  DCHECK(!signature.return_type->IsVoidOrNever());

  VisitResult expression_result = Visit(decl->expression);
  VisitResult return_result =
      GenerateImplicitConvert(signature.return_type, expression_result);

  GenerateIndent();
  source_out() << "return " << return_result.RValue() << ";\n";
  source_out() << "}\n\n";
}

void ImplementationVisitor::Visit(StructDeclaration* decl) {
  header_out() << "  struct " << decl->name << " {\n";
  const StructType* struct_type =
      static_cast<const StructType*>(declarations()->LookupType(decl->name));
  for (auto& field : struct_type->fields()) {
    header_out() << "    " << field.type->GetGeneratedTypeName();
    header_out() << " " << field.name << ";\n";
  }
  header_out() << "  } "
               << ";\n";
}

void ImplementationVisitor::Visit(TorqueMacroDeclaration* decl,
                                  const Signature& sig, Statement* body) {
  Signature signature = MakeSignature(decl->signature.get());
  std::string name = GetGeneratedCallableName(
      decl->name, declarations()->GetCurrentSpecializationTypeNamesVector());
  const TypeVector& list = signature.types();
  Macro* macro = declarations()->LookupMacro(name, list);

  CurrentCallableActivator activator(global_context_, macro, decl);

  if (body != nullptr) {
    header_out() << "  ";
    GenerateMacroFunctionDeclaration(header_out(), "", macro);
    header_out() << ";" << std::endl;

    GenerateMacroFunctionDeclaration(
        source_out(), GetDSLAssemblerName(CurrentModule()) + "::", macro);
    source_out() << " {" << std::endl;

    const Variable* result_var = nullptr;
    if (macro->HasReturnValue()) {
      result_var =
          GenerateVariableDeclaration(decl, kReturnValueVariable, {}, {});
    }
    Label* macro_end = declarations()->DeclareLabel("macro_end");
    GenerateLabelDefinition(macro_end, decl);

    const Type* result = Visit(body);
    if (result->IsNever()) {
      if (!macro->signature().return_type->IsNever() && !macro->HasReturns()) {
        std::stringstream s;
        s << "macro " << decl->name
          << " that never returns must have return type never";
        ReportError(s.str());
      }
    } else {
      if (macro->signature().return_type->IsNever()) {
        std::stringstream s;
        s << "macro " << decl->name
          << " has implicit return at end of its declartion but return type "
             "never";
        ReportError(s.str());
      } else if (!macro->signature().return_type->IsVoid()) {
        std::stringstream s;
        s << "macro " << decl->name
          << " expects to return a value but doesn't on all paths";
        ReportError(s.str());
      }
    }
    if (macro->HasReturns()) {
      if (!result->IsNever()) {
        GenerateLabelGoto(macro_end);
      }
      GenerateLabelBind(macro_end);
    }
    if (result_var != nullptr) {
      GenerateIndent();
      source_out() << "return "
                   << RValueFlattenStructs(
                          VisitResult(result_var->type(), result_var))
                   << ";" << std::endl;
    }
    source_out() << "}" << std::endl << std::endl;
  }
}

void ImplementationVisitor::Visit(TorqueBuiltinDeclaration* decl,
                                  const Signature& signature, Statement* body) {
  std::string name = GetGeneratedCallableName(
      decl->name, declarations()->GetCurrentSpecializationTypeNamesVector());
  source_out() << "TF_BUILTIN(" << name << ", "
               << GetDSLAssemblerName(CurrentModule()) << ") {" << std::endl;
  Builtin* builtin = declarations()->LookupBuiltin(name);
  CurrentCallableActivator activator(global_context_, builtin, decl);

  // Context
  const Value* val =
      declarations()->LookupValue(decl->signature->parameters.names[0]);
  GenerateIndent();
  source_out() << "TNode<Context> " << val->value()
               << " = UncheckedCast<Context>(Parameter("
               << "Descriptor::kContext));" << std::endl;
  GenerateIndent();
  source_out() << "USE(" << val->value() << ");" << std::endl;

  size_t first = 1;
  if (builtin->IsVarArgsJavaScript()) {
    assert(decl->signature->parameters.has_varargs);
    ExternConstant* arguments =
        ExternConstant::cast(declarations()->LookupValue(
            decl->signature->parameters.arguments_variable));
    std::string arguments_name = arguments->value();
    GenerateIndent();
    source_out()
        << "Node* argc = Parameter(Descriptor::kJSActualArgumentsCount);"
        << std::endl;
    GenerateIndent();
    source_out() << "CodeStubArguments arguments_impl(this, "
                    "ChangeInt32ToIntPtr(argc));"
                 << std::endl;
    const Value* receiver =
        declarations()->LookupValue(decl->signature->parameters.names[1]);
    GenerateIndent();
    source_out() << "TNode<Object> " << receiver->value()
                 << " = arguments_impl.GetReceiver();" << std::endl;
    GenerateIndent();
    source_out() << "auto arguments = &arguments_impl;" << std::endl;
    GenerateIndent();
    source_out() << "USE(arguments);" << std::endl;
    GenerateIndent();
    source_out() << "USE(" << receiver->value() << ");" << std::endl;
    first = 2;
  }

  GenerateParameterList(decl->signature->parameters.names, first);
  Visit(body);
  source_out() << "}" << std::endl << std::endl;
}

const Type* ImplementationVisitor::Visit(VarDeclarationStatement* stmt) {
  base::Optional<VisitResult> init_result;
  if (stmt->initializer) {
    init_result = Visit(*stmt->initializer);
  }
  GenerateVariableDeclaration(stmt, stmt->name, {}, init_result);
  return TypeOracle::GetVoidType();
}

const Type* ImplementationVisitor::Visit(TailCallStatement* stmt) {
  return Visit(stmt->call, true).type();
}

VisitResult ImplementationVisitor::Visit(ConditionalExpression* expr) {
  std::string f1 = NewTempVariable();
  std::string f2 = NewTempVariable();

  // The code for both paths of the conditional need to be generated first in
  // lambdas before evaluating the conditional expression because the common
  // type of the result of both the true and false of the condition needs to be
  // known when declaring the variable to hold the result of the conditional.
  VisitResult left, right;
  GenerateIndent();
  source_out() << "auto " << f1 << " = [=]() ";
  {
    ScopedIndent indent(this, false);
    source_out() << "" << std::endl;
    left = Visit(expr->if_true);
    GenerateIndent();
    source_out() << "return " << RValueFlattenStructs(left) << ";" << std::endl;
  }
  source_out() << ";" << std::endl;
  GenerateIndent();
  source_out() << "auto " << f2 << " = [=]() ";
  {
    ScopedIndent indent(this, false);
    source_out() << "" << std::endl;
    right = Visit(expr->if_false);
    GenerateIndent();
    source_out() << "return " << RValueFlattenStructs(right) << ";"
                 << std::endl;
  }
  source_out() << ";" << std::endl;

  const Type* common_type = GetCommonType(left.type(), right.type());
  std::string result_var = NewTempVariable();
  Variable* result = GenerateVariableDeclaration(expr, result_var, common_type);

  {
    ScopedIndent indent(this);
    Declarations::NodeScopeActivator scope(declarations(), expr->condition);

    Label* true_label = declarations()->LookupLabel(kTrueLabelName);
    GenerateLabelDefinition(true_label);
    Label* false_label = declarations()->LookupLabel(kFalseLabelName);
    GenerateLabelDefinition(false_label);
    Label* done_label = declarations()->DeclarePrivateLabel(kDoneLabelName);
    GenerateLabelDefinition(done_label, expr);

    VisitResult condition_result = Visit(expr->condition);
    if (!condition_result.type()->IsNever()) {
      condition_result =
          GenerateImplicitConvert(TypeOracle::GetBoolType(), condition_result);
      GenerateBranch(condition_result, true_label, false_label);
    }
    GenerateLabelBind(true_label);
    GenerateIndent();
    VisitResult left_result = {right.type(), f1 + "()"};
    GenerateAssignToVariable(result, left_result);
    GenerateLabelGoto(done_label);

    GenerateLabelBind(false_label);
    GenerateIndent();
    VisitResult right_result = {right.type(), f2 + "()"};
    GenerateAssignToVariable(result, right_result);
    GenerateLabelGoto(done_label);

    GenerateLabelBind(done_label);
  }
  return VisitResult(common_type, result);
}

VisitResult ImplementationVisitor::Visit(LogicalOrExpression* expr) {
  VisitResult left_result;
  {
    Declarations::NodeScopeActivator scope(declarations(), expr->left);
    Label* false_label = declarations()->LookupLabel(kFalseLabelName);
    GenerateLabelDefinition(false_label);
    left_result = Visit(expr->left);
    if (left_result.type()->IsBool()) {
      Label* true_label = declarations()->LookupLabel(kTrueLabelName);
      GenerateIndent();
      source_out() << "GotoIf(" << RValueFlattenStructs(left_result) << ", "
                   << true_label->generated() << ");" << std::endl;
    } else if (!left_result.type()->IsConstexprBool()) {
      GenerateLabelBind(false_label);
    }
  }
  VisitResult right_result = Visit(expr->right);
  if (right_result.type() != left_result.type()) {
    std::stringstream stream;
    stream << "types of left and right expression of logical OR don't match (\""
           << *left_result.type() << "\" vs. \"" << *right_result.type()
           << "\")";
    ReportError(stream.str());
  }
  if (left_result.type()->IsConstexprBool()) {
    return VisitResult(left_result.type(),
                       std::string("(") + RValueFlattenStructs(left_result) +
                           " || " + RValueFlattenStructs(right_result) + ")");
  } else {
    return right_result;
  }
}

VisitResult ImplementationVisitor::Visit(LogicalAndExpression* expr) {
  VisitResult left_result;
  {
    Declarations::NodeScopeActivator scope(declarations(), expr->left);
    Label* true_label = declarations()->LookupLabel(kTrueLabelName);
    GenerateLabelDefinition(true_label);
    left_result = Visit(expr->left);
    if (left_result.type()->IsBool()) {
      Label* false_label = declarations()->LookupLabel(kFalseLabelName);
      GenerateIndent();
      source_out() << "GotoIfNot(" << RValueFlattenStructs(left_result) << ", "
                   << false_label->generated() << ");" << std::endl;
    } else if (!left_result.type()->IsConstexprBool()) {
      GenerateLabelBind(true_label);
    }
  }
  VisitResult right_result = Visit(expr->right);
  if (right_result.type() != left_result.type()) {
    std::stringstream stream;
    stream
        << "types of left and right expression of logical AND don't match (\""
        << *left_result.type() << "\" vs. \"" << *right_result.type() << "\")";
    ReportError(stream.str());
  }
  if (left_result.type()->IsConstexprBool()) {
    return VisitResult(left_result.type(),
                       std::string("(") + RValueFlattenStructs(left_result) +
                           " && " + RValueFlattenStructs(right_result) + ")");
  } else {
    return right_result;
  }
}

VisitResult ImplementationVisitor::Visit(IncrementDecrementExpression* expr) {
  VisitResult value_copy;
  auto location_ref = GetLocationReference(expr->location);
  VisitResult current_value =
      GenerateFetchFromLocation(expr->location, location_ref);
  if (expr->postfix) {
    value_copy = GenerateCopy(current_value);
  }
  VisitResult one = {TypeOracle::GetConstInt31Type(), "1"};
  Arguments args;
  args.parameters = {current_value, one};
  VisitResult assignment_value = GenerateCall(
      expr->op == IncrementDecrementOperator::kIncrement ? "+" : "-", args);
  GenerateAssignToLocation(expr->location, location_ref, assignment_value);
  return expr->postfix ? value_copy : assignment_value;
}

VisitResult ImplementationVisitor::Visit(AssignmentExpression* expr) {
  LocationReference location_ref = GetLocationReference(expr->location);
  VisitResult assignment_value;
  if (expr->op) {
    VisitResult location_value =
        GenerateFetchFromLocation(expr->location, location_ref);
    assignment_value = Visit(expr->value);
    Arguments args;
    args.parameters = {assignment_value, assignment_value};
    assignment_value = GenerateCall(*expr->op, args);
    GenerateAssignToLocation(expr->location, location_ref, assignment_value);
  } else {
    assignment_value = Visit(expr->value);
    GenerateAssignToLocation(expr->location, location_ref, assignment_value);
  }
  return assignment_value;
}

VisitResult ImplementationVisitor::Visit(NumberLiteralExpression* expr) {
  // TODO(tebbi): Do not silently loose precision; support 64bit literals.
  double d = std::stod(expr->number.c_str());
  int32_t i = static_cast<int32_t>(d);
  const Type* result_type =
      declarations()->LookupType(CONST_FLOAT64_TYPE_STRING);
  if (i == d) {
    if ((i >> 30) == (i >> 31)) {
      result_type = declarations()->LookupType(CONST_INT31_TYPE_STRING);
    } else {
      result_type = declarations()->LookupType(CONST_INT32_TYPE_STRING);
    }
  }
  std::string temp = GenerateNewTempVariable(result_type);
  source_out() << expr->number << ";" << std::endl;
  return VisitResult{result_type, temp};
}

VisitResult ImplementationVisitor::Visit(StringLiteralExpression* expr) {
  std::string temp = GenerateNewTempVariable(TypeOracle::GetConstStringType());
  source_out() << "\"" << expr->literal.substr(1, expr->literal.size() - 2)
               << "\";" << std::endl;
  return VisitResult{TypeOracle::GetConstStringType(), temp};
}

VisitResult ImplementationVisitor::GetBuiltinCode(Builtin* builtin) {
  if (builtin->IsExternal() || builtin->kind() != Builtin::kStub) {
    ReportError(
        "creating function pointers is only allowed for internal builtins with "
        "stub linkage");
  }
  const Type* type = TypeOracle::GetFunctionPointerType(
      builtin->signature().parameter_types.types,
      builtin->signature().return_type);
  std::string code =
      "HeapConstant(Builtins::CallableFor(isolate(), Builtins::k" +
      builtin->name() + ").code())";
  return VisitResult(type, code);
}

VisitResult ImplementationVisitor::Visit(IdentifierExpression* expr) {
  std::string name = expr->name;
  if (expr->generic_arguments.size() != 0) {
    GenericList* generic_list = declarations()->LookupGeneric(expr->name);
    for (Generic* generic : generic_list->list()) {
      TypeVector specialization_types = GetTypeVector(expr->generic_arguments);
      name = GetGeneratedCallableName(name, specialization_types);
      CallableNode* callable = generic->declaration()->callable;
      QueueGenericSpecialization({generic, specialization_types}, callable,
                                 callable->signature.get(),
                                 generic->declaration()->body);
    }
  }

  if (Builtin* builtin = Builtin::DynamicCast(declarations()->Lookup(name))) {
    return GetBuiltinCode(builtin);
  }

  return GenerateFetchFromLocation(expr, GetLocationReference(expr));
}

const Type* ImplementationVisitor::Visit(GotoStatement* stmt) {
  Label* label = declarations()->LookupLabel(stmt->label);

  if (stmt->arguments.size() != label->GetParameterCount()) {
    std::stringstream stream;
    stream << "goto to label has incorrect number of parameters (expected "
           << std::to_string(label->GetParameterCount()) << " found "
           << std::to_string(stmt->arguments.size()) << ")";
    ReportError(stream.str());
  }

  size_t i = 0;
  for (Expression* e : stmt->arguments) {
    VisitResult result = Visit(e);
    Variable* var = label->GetParameter(i++);
    GenerateAssignToVariable(var, result);
  }

  GenerateLabelGoto(label);
  label->MarkUsed();
  return TypeOracle::GetNeverType();
}

const Type* ImplementationVisitor::Visit(IfStatement* stmt) {
  ScopedIndent indent(this);

  bool has_else = stmt->if_false.has_value();

  if (stmt->is_constexpr) {
    VisitResult expression_result = Visit(stmt->condition);

    if (!(expression_result.type() == TypeOracle::GetConstexprBoolType())) {
      std::stringstream stream;
      stream << "expression should return type constexpr bool "
             << "but returns type " << *expression_result.type();
      ReportError(stream.str());
    }

    const Type* left_result;
    const Type* right_result = TypeOracle::GetVoidType();
    {
      GenerateIndent();
      source_out() << "if ((" << RValueFlattenStructs(expression_result)
                   << ")) ";
      ScopedIndent indent(this, false);
      source_out() << std::endl;
      left_result = Visit(stmt->if_true);
    }

    if (has_else) {
      source_out() << " else ";
      ScopedIndent indent(this, false);
      source_out() << std::endl;
      right_result = Visit(*stmt->if_false);
    }
    if (left_result->IsNever() != right_result->IsNever()) {
      std::stringstream stream;
      stream << "either both or neither branches in a constexpr if statement "
                "must reach their end at"
             << PositionAsString(stmt->pos);
      ReportError(stream.str());
    }

    source_out() << std::endl;

    return left_result;
  } else {
    Label* true_label = nullptr;
    Label* false_label = nullptr;
    {
      Declarations::NodeScopeActivator scope(declarations(), &*stmt->condition);
      true_label = declarations()->LookupLabel(kTrueLabelName);
      GenerateLabelDefinition(true_label);
      false_label = declarations()->LookupLabel(kFalseLabelName);
      GenerateLabelDefinition(false_label, !has_else ? stmt : nullptr);
    }

    Label* done_label = nullptr;
    bool live = false;
    if (has_else) {
      done_label = declarations()->DeclarePrivateLabel("if_done_label");
      GenerateLabelDefinition(done_label, stmt);
    } else {
      done_label = false_label;
      live = true;
    }
    std::vector<Statement*> blocks = {stmt->if_true};
    std::vector<Label*> labels = {true_label, false_label};
    if (has_else) blocks.push_back(*stmt->if_false);
    if (GenerateExpressionBranch(stmt->condition, labels, blocks, done_label)) {
      live = true;
    }
    if (live) {
      GenerateLabelBind(done_label);
    }
    return live ? TypeOracle::GetVoidType() : TypeOracle::GetNeverType();
  }
}

const Type* ImplementationVisitor::Visit(WhileStatement* stmt) {
  ScopedIndent indent(this);

  Label* body_label = nullptr;
  Label* exit_label = nullptr;
  {
    Declarations::NodeScopeActivator scope(declarations(), stmt->condition);
    body_label = declarations()->LookupLabel(kTrueLabelName);
    GenerateLabelDefinition(body_label);
    exit_label = declarations()->LookupLabel(kFalseLabelName);
    GenerateLabelDefinition(exit_label);
  }

  Label* header_label = declarations()->DeclarePrivateLabel("header");
  GenerateLabelDefinition(header_label, stmt);
  GenerateLabelGoto(header_label);
  GenerateLabelBind(header_label);

  Declarations::NodeScopeActivator scope(declarations(), stmt->body);
  BreakContinueActivator activator(global_context_, exit_label, header_label);

  GenerateExpressionBranch(stmt->condition, {body_label, exit_label},
                           {stmt->body}, header_label);

  GenerateLabelBind(exit_label);
  return TypeOracle::GetVoidType();
}

const Type* ImplementationVisitor::Visit(BlockStatement* block) {
  Declarations::NodeScopeActivator scope(declarations(), block);
  ScopedIndent indent(this);
  const Type* type = TypeOracle::GetVoidType();
  for (Statement* s : block->statements) {
    if (type->IsNever()) {
      std::stringstream stream;
      stream << "statement after non-returning statement";
      ReportError(stream.str());
    }
    type = Visit(s);
  }
  return type;
}

const Type* ImplementationVisitor::Visit(DebugStatement* stmt) {
#if defined(DEBUG)
  GenerateIndent();
  source_out() << "Print(\""
               << "halting because of '" << stmt->reason << "' at "
               << PositionAsString(stmt->pos) << "\");" << std::endl;
#endif
  GenerateIndent();
  if (stmt->never_continues) {
    source_out() << "Unreachable();" << std::endl;
    return TypeOracle::GetNeverType();
  } else {
    source_out() << "DebugBreak();" << std::endl;
    return TypeOracle::GetVoidType();
  }
}

namespace {

std::string FormatAssertSource(const std::string& str) {
  // Replace all whitespace characters with a space character.
  std::string str_no_newlines = str;
  std::replace_if(str_no_newlines.begin(), str_no_newlines.end(),
                  [](unsigned char c) { return isspace(c); }, ' ');

  // str might include indentation, squash multiple space characters into one.
  std::string result;
  std::unique_copy(str_no_newlines.begin(), str_no_newlines.end(),
                   std::back_inserter(result),
                   [](char a, char b) { return a == ' ' && b == ' '; });
  return result;
}

}  // namespace

const Type* ImplementationVisitor::Visit(AssertStatement* stmt) {
  bool do_check = !stmt->debug_only;
#if defined(DEBUG)
  do_check = true;
#endif
  if (do_check) {
    // CSA_ASSERT & co. are not used here on purpose for two reasons. First,
    // Torque allows and handles two types of expressions in the if protocol
    // automagically, ones that return TNode<BoolT> and those that use the
    // BranchIf(..., Label* true, Label* false) idiom. Because the machinery to
    // handle this is embedded in the expression handling and to it's not
    // possible to make the decision to use CSA_ASSERT or CSA_ASSERT_BRANCH
    // isn't trivial up-front. Secondly, on failure, the assert text should be
    // the corresponding Torque code, not the -gen.cc code, which would be the
    // case when using CSA_ASSERT_XXX.
    Label* true_label = nullptr;
    Label* false_label = nullptr;
    Declarations::NodeScopeActivator scope(declarations(), stmt->expression);
    true_label = declarations()->LookupLabel(kTrueLabelName);
    GenerateLabelDefinition(true_label);
    false_label = declarations()->LookupLabel(kFalseLabelName);
    GenerateLabelDefinition(false_label);

    VisitResult expression_result = Visit(stmt->expression);
    if (expression_result.type() == TypeOracle::GetBoolType()) {
      GenerateBranch(expression_result, true_label, false_label);
    } else {
      if (expression_result.type() != TypeOracle::GetNeverType()) {
        std::stringstream s;
        s << "unexpected return type " << *expression_result.type()
          << " for branch expression";
        ReportError(s.str());
      }
    }

    GenerateLabelBind(false_label);
    GenerateIndent();
    source_out() << "Print(\""
                 << "assert '" << FormatAssertSource(stmt->source)
                 << "' failed at " << PositionAsString(stmt->pos) << "\");"
                 << std::endl;
    GenerateIndent();
    source_out() << "Unreachable();" << std::endl;

    GenerateLabelBind(true_label);
  }
  return TypeOracle::GetVoidType();
}

const Type* ImplementationVisitor::Visit(ExpressionStatement* stmt) {
  const Type* type = Visit(stmt->expression).type();
  return type->IsNever() ? type : TypeOracle::GetVoidType();
}

const Type* ImplementationVisitor::Visit(ReturnStatement* stmt) {
  Callable* current_callable = global_context_.GetCurrentCallable();
  if (current_callable->signature().return_type->IsNever()) {
    std::stringstream s;
    s << "cannot return from a function with return type never";
    ReportError(s.str());
  }
  Label* end = current_callable->IsMacro()
                   ? declarations()->LookupLabel("macro_end")
                   : nullptr;
  if (current_callable->HasReturnValue()) {
    if (!stmt->value) {
      std::stringstream s;
      s << "return expression needs to be specified for a return type of "
        << *current_callable->signature().return_type;
      ReportError(s.str());
    }
    VisitResult expression_result = Visit(*stmt->value);
    VisitResult return_result = GenerateImplicitConvert(
        current_callable->signature().return_type, expression_result);
    if (current_callable->IsMacro()) {
      Variable* var =
          Variable::cast(declarations()->LookupValue(kReturnValueVariable));
      GenerateAssignToVariable(var, return_result);
      GenerateLabelGoto(end);
    } else if (current_callable->IsBuiltin()) {
      if (Builtin::cast(current_callable)->IsVarArgsJavaScript()) {
        GenerateIndent();
        source_out() << "arguments->PopAndReturn("
                     << RValueFlattenStructs(return_result) << ");"
                     << std::endl;
      } else {
        GenerateIndent();
        source_out() << "Return(" << RValueFlattenStructs(return_result) << ");"
                     << std::endl;
      }
    } else {
      UNREACHABLE();
    }
  } else {
    if (stmt->value) {
      std::stringstream s;
      s << "return expression can't be specified for a void or never return "
           "type";
      ReportError(s.str());
    }
    GenerateLabelGoto(end);
  }
  current_callable->IncrementReturns();
  return TypeOracle::GetNeverType();
}

const Type* ImplementationVisitor::Visit(ForOfLoopStatement* stmt) {
  Declarations::NodeScopeActivator scope(declarations(), stmt);

  VisitResult expression_result = Visit(stmt->iterable);
  VisitResult begin = stmt->begin
                          ? Visit(*stmt->begin)
                          : VisitResult(TypeOracle::GetConstInt31Type(), "0");

  VisitResult end = stmt->end
                        ? Visit(*stmt->end)
                        : GenerateCall(".length", {{expression_result}, {}});

  Label* body_label = declarations()->DeclarePrivateLabel("body");
  GenerateLabelDefinition(body_label);
  Label* increment_label = declarations()->DeclarePrivateLabel("increment");
  GenerateLabelDefinition(increment_label);
  Label* exit_label = declarations()->DeclarePrivateLabel("exit");
  GenerateLabelDefinition(exit_label);

  const Type* common_type = GetCommonType(begin.type(), end.type());
  Variable* index_var = GenerateVariableDeclaration(
      stmt, std::string(kForIndexValueVariable) + "_" + NewTempVariable(),
      common_type, begin);

  VisitResult index_for_read = {index_var->type(), index_var};

  Label* header_label = declarations()->DeclarePrivateLabel("header");
  GenerateLabelDefinition(header_label, stmt);

  GenerateLabelGoto(header_label);

  GenerateLabelBind(header_label);

  BreakContinueActivator activator(global_context_, exit_label,
                                   increment_label);

  VisitResult result = GenerateCall("<", {{index_for_read, end}, {}});
  GenerateBranch(result, body_label, exit_label);

  GenerateLabelBind(body_label);
  VisitResult element_result =
      GenerateCall("[]", {{expression_result, index_for_read}, {}});
  GenerateVariableDeclaration(stmt->var_declaration,
                              stmt->var_declaration->name, {}, element_result);
  Visit(stmt->body);
  GenerateLabelGoto(increment_label);

  GenerateLabelBind(increment_label);
  Arguments increment_args;
  increment_args.parameters = {index_for_read,
                               {TypeOracle::GetConstInt31Type(), "1"}};
  VisitResult increment_result = GenerateCall("+", increment_args);

  GenerateAssignToVariable(index_var, increment_result);

  GenerateLabelGoto(header_label);

  GenerateLabelBind(exit_label);
  return TypeOracle::GetVoidType();
}

const Type* ImplementationVisitor::Visit(TryLabelStatement* stmt) {
  ScopedIndent indent(this);
  Label* try_done = declarations()->DeclarePrivateLabel("try_done");
  GenerateLabelDefinition(try_done);
  const Type* try_result = TypeOracle::GetNeverType();
  std::vector<Label*> labels;

  // Output labels for the goto handlers and for the merge after the try.
  {
    // Activate a new scope to see handler labels
    Declarations::NodeScopeActivator scope(declarations(), stmt);
    for (LabelBlock* block : stmt->label_blocks) {
      CurrentSourcePosition::Scope scope(block->pos);
      Label* label = declarations()->LookupLabel(block->label);
      labels.push_back(label);
      GenerateLabelDefinition(label);
    }

    size_t i = 0;
    for (auto label : labels) {
      Declarations::NodeScopeActivator scope(declarations(),
                                             stmt->label_blocks[i]->body);
      for (auto& v : label->GetParameters()) {
        GenerateVariableDeclaration(stmt, v->name(), v->type());
        v->Define();
      }
      ++i;
    }

    Label* try_begin_label = declarations()->DeclarePrivateLabel("try_begin");
    GenerateLabelDefinition(try_begin_label);
    GenerateLabelGoto(try_begin_label);

    // Visit try
    if (GenerateLabeledStatementBlocks({stmt->try_block},
                                       std::vector<Label*>({try_begin_label}),
                                       try_done)) {
      try_result = TypeOracle::GetVoidType();
    }
  }

  // Make sure that each label clause is actually used. It's not just a friendly
  // thing to do, it will cause problems downstream in the compiler if there are
  // bound labels that are never jumped to.
  auto label_iterator = stmt->label_blocks.begin();
  for (auto label : labels) {
    CurrentSourcePosition::Scope scope((*label_iterator)->pos);
    if (!label->IsUsed()) {
      std::stringstream s;
      s << "label ";
      s << (*label_iterator)->label;
      s << " has a handler block but is never referred to in try block";
      ReportError(s.str());
    }
    label_iterator++;
  }

  // Visit and output the code for each catch block, one-by-one.
  std::vector<Statement*> bodies;
  for (LabelBlock* block : stmt->label_blocks) bodies.push_back(block->body);
  if (GenerateLabeledStatementBlocks(bodies, labels, try_done)) {
    try_result = TypeOracle::GetVoidType();
  }

  if (!try_result->IsNever()) {
    GenerateLabelBind(try_done);
  }
  return try_result;
}

const Type* ImplementationVisitor::Visit(BreakStatement* stmt) {
  Label* break_label = global_context_.GetCurrentBreak();
  if (break_label == nullptr) {
    ReportError("break used outside of loop");
  }
  GenerateLabelGoto(break_label);
  return TypeOracle::GetNeverType();
}

const Type* ImplementationVisitor::Visit(ContinueStatement* stmt) {
  Label* continue_label = global_context_.GetCurrentContinue();
  if (continue_label == nullptr) {
    ReportError("continue used outside of loop");
  }
  GenerateLabelGoto(continue_label);
  return TypeOracle::GetNeverType();
}

const Type* ImplementationVisitor::Visit(ForLoopStatement* stmt) {
  Declarations::NodeScopeActivator scope(declarations(), stmt);

  if (stmt->var_declaration) Visit(*stmt->var_declaration);

  Label* body_label = nullptr;
  Label* exit_label = nullptr;
  {
    Declarations::NodeScopeActivator scope(declarations(), stmt->test);
    body_label = declarations()->LookupLabel(kTrueLabelName);
    GenerateLabelDefinition(body_label);
    exit_label = declarations()->LookupLabel(kFalseLabelName);
    GenerateLabelDefinition(exit_label);
  }

  Label* header_label = declarations()->DeclarePrivateLabel("header");
  GenerateLabelDefinition(header_label, stmt);
  GenerateLabelGoto(header_label);
  GenerateLabelBind(header_label);

  Label* assignment_label = declarations()->DeclarePrivateLabel("assignment");
  GenerateLabelDefinition(assignment_label);

  BreakContinueActivator activator(global_context_, exit_label,
                                   assignment_label);

  std::vector<Label*> labels = {body_label, exit_label};
  if (GenerateExpressionBranch(stmt->test, labels, {stmt->body},
                               assignment_label)) {
    ScopedIndent indent(this);
    GenerateLabelBind(assignment_label);
    Visit(stmt->action);
    GenerateLabelGoto(header_label);
  }

  GenerateLabelBind(exit_label);
  return TypeOracle::GetVoidType();
}

void ImplementationVisitor::GenerateImplementation(const std::string& dir,
                                                   Module* module) {
  std::string new_source(module->source());
  std::string base_file_name =
      "builtins-" + DashifyString(module->name()) + "-from-dsl-gen";

  std::string source_file_name = dir + "/" + base_file_name + ".cc";
  ReplaceFileContentsIfDifferent(source_file_name, new_source);
  std::string new_header(module->header());
  std::string header_file_name = dir + "/" + base_file_name + ".h";
  ReplaceFileContentsIfDifferent(header_file_name, new_header);
}

std::string ImplementationVisitor::GetBaseAssemblerName(Module* module) {
  if (module == global_context_.GetDefaultModule()) {
    return "CodeStubAssembler";
  } else {
    std::string assembler_name(CamelifyString(module->name()) +
                               "BuiltinsAssembler");
    return assembler_name;
  }
}

std::string ImplementationVisitor::GetDSLAssemblerName(Module* module) {
  std::string assembler_name(CamelifyString(module->name()) +
                             "BuiltinsFromDSLAssembler");
  return assembler_name;
}

void ImplementationVisitor::GenerateIndent() {
  for (size_t i = 0; i <= indent_; ++i) {
    source_out() << "  ";
  }
}

void ImplementationVisitor::GenerateMacroFunctionDeclaration(
    std::ostream& o, const std::string& macro_prefix, Macro* macro) {
  GenerateFunctionDeclaration(o, macro_prefix, macro->name(),
                              macro->signature(), macro->parameter_names());
}

void ImplementationVisitor::GenerateFunctionDeclaration(
    std::ostream& o, const std::string& macro_prefix, const std::string& name,
    const Signature& signature, const NameVector& parameter_names) {
  if (global_context_.verbose()) {
    std::cout << "generating source for declaration " << name << ""
              << std::endl;
  }

  // Quite a hack here. Make sure that TNode is namespace qualified if the
  // macro/constant name is also qualified.
  std::string return_type_name(signature.return_type->GetGeneratedTypeName());
  if (const StructType* struct_type =
          StructType::DynamicCast(signature.return_type)) {
    o << GetDSLAssemblerName(struct_type->module()) << "::";
  } else if (macro_prefix != "" && (return_type_name.length() > 5) &&
             (return_type_name.substr(0, 5) == "TNode")) {
    o << "compiler::";
  }
  o << return_type_name;
  o << " " << macro_prefix << name << "(";

  DCHECK_EQ(signature.types().size(), parameter_names.size());
  auto type_iterator = signature.types().begin();
  bool first = true;
  for (const std::string& name : parameter_names) {
    if (!first) {
      o << ", ";
    }
    const Value* parameter = declarations()->LookupValue(name);
    const Type* parameter_type = *type_iterator;
    const std::string& generated_type_name =
        parameter_type->GetGeneratedTypeName();
    o << generated_type_name << " " << parameter->value();
    type_iterator++;
    first = false;
  }

  for (const LabelDeclaration& label_info : signature.labels) {
    Label* label = declarations()->LookupLabel(label_info.name);
    if (!first) {
      o << ", ";
    }
    o << "Label* " << label->generated();
    for (Variable* var : label->GetParameters()) {
      std::string generated_type_name("TVariable<");
      generated_type_name += var->type()->GetGeneratedTNodeTypeName();
      generated_type_name += ">*";
      o << ", ";
      o << generated_type_name << " " << var->value();
    }
  }

  o << ")";
}

namespace {

void PrintMacroSignatures(std::stringstream& s, const std::string& name,
                          const std::vector<Macro*>& macros) {
  for (Macro* m : macros) {
    s << "\n  " << name;
    PrintSignature(s, m->signature(), false);
  }
}

void FailMacroLookup(const std::string& reason, const std::string& name,
                     const Arguments& arguments,
                     const std::vector<Macro*>& candidates) {
  std::stringstream stream;
  stream << "\n"
         << reason << ": \n  " << name << "("
         << arguments.parameters.GetTypeVector() << ")";
  if (arguments.labels.size() != 0) {
    stream << " labels ";
    for (auto l : arguments.labels) {
      PrintLabel(stream, *l, false);
    }
  }
  stream << "\ncandidates are:";
  PrintMacroSignatures(stream, name, candidates);
  ReportError(stream.str());
}

}  // namespace

Callable* ImplementationVisitor::LookupCall(const std::string& name,
                                            const Arguments& arguments) {
  Callable* result = nullptr;
  TypeVector parameter_types(arguments.parameters.GetTypeVector());
  Declarable* declarable = declarations()->Lookup(name);
  if (declarable->IsBuiltin()) {
    result = Builtin::cast(declarable);
  } else if (declarable->IsRuntimeFunction()) {
    result = RuntimeFunction::cast(declarable);
  } else if (declarable->IsMacroList()) {
    std::vector<Macro*> candidates;
    std::vector<Macro*> macros_with_same_name;
    for (Macro* m : MacroList::cast(declarable)->list()) {
      bool try_bool_context =
          arguments.labels.size() == 0 &&
          m->signature().return_type == TypeOracle::GetNeverType();
      Label* true_label = nullptr;
      Label* false_label = nullptr;
      if (try_bool_context) {
        true_label = declarations()->TryLookupLabel(kTrueLabelName);
        false_label = declarations()->TryLookupLabel(kFalseLabelName);
      }
      if (IsCompatibleSignature(m->signature(), parameter_types,
                                arguments.labels) ||
          (true_label && false_label &&
           IsCompatibleSignature(m->signature(), parameter_types,
                                 {true_label, false_label}))) {
        candidates.push_back(m);
      } else {
        macros_with_same_name.push_back(m);
      }
    }

    if (candidates.empty() && macros_with_same_name.empty()) {
      std::stringstream stream;
      stream << "no matching declaration found for " << name;
      ReportError(stream.str());
    } else if (candidates.empty()) {
      FailMacroLookup("cannot find macro with name", name, arguments,
                      macros_with_same_name);
    }

    auto is_better_candidate = [&](Macro* a, Macro* b) {
      return ParameterDifference(a->signature().parameter_types.types,
                                 parameter_types)
          .StrictlyBetterThan(ParameterDifference(
              b->signature().parameter_types.types, parameter_types));
    };

    Macro* best = *std::min_element(candidates.begin(), candidates.end(),
                                    is_better_candidate);
    for (Macro* candidate : candidates) {
      if (candidate != best && !is_better_candidate(best, candidate)) {
        FailMacroLookup("ambiguous macro", name, arguments, candidates);
      }
    }
    result = best;
  } else {
    std::stringstream stream;
    stream << "can't call " << declarable->type_name() << " " << name
           << " because it's not callable"
           << ": call parameters were (" << parameter_types << ")";
    ReportError(stream.str());
  }

  size_t caller_size = parameter_types.size();
  size_t callee_size = result->signature().types().size();
  if (caller_size != callee_size &&
      !result->signature().parameter_types.var_args) {
    std::stringstream stream;
    stream << "parameter count mismatch calling " << *result << " - expected "
           << std::to_string(callee_size) << ", found "
           << std::to_string(caller_size);
    ReportError(stream.str());
  }

  return result;
}

void ImplementationVisitor::GetFlattenedStructsVars(
    const Variable* base, std::set<const Variable*>& vars) {
  const Type* type = base->type();
  if (base->IsConst()) return;
  if (type->IsStructType()) {
    const StructType* struct_type = StructType::cast(type);
    for (auto& field : struct_type->fields()) {
      std::string field_var_name = base->name() + "." + field.name;
      GetFlattenedStructsVars(
          Variable::cast(declarations()->LookupValue(field_var_name)), vars);
    }
  } else {
    vars.insert(base);
  }
}

void ImplementationVisitor::GenerateChangedVarsFromControlSplit(AstNode* node) {
  const std::set<const Variable*>& changed_vars =
      global_context_.GetControlSplitChangedVariables(
          node, declarations()->GetCurrentSpecializationTypeNamesVector());
  std::set<const Variable*> flattened_vars;
  for (auto v : changed_vars) {
    GetFlattenedStructsVars(v, flattened_vars);
  }
  source_out() << "{";
  PrintCommaSeparatedList(source_out(), flattened_vars,
                          [&](const Variable* v) { return v->value(); });
  source_out() << "}";
}

const Type* ImplementationVisitor::GetCommonType(const Type* left,
                                                 const Type* right) {
  const Type* common_type;
  if (IsAssignableFrom(left, right)) {
    common_type = left;
  } else if (IsAssignableFrom(right, left)) {
    common_type = right;
  } else {
    common_type = TypeOracle::GetUnionType(left, right);
  }
  common_type = common_type->NonConstexprVersion();
  return common_type;
}

VisitResult ImplementationVisitor::GenerateCopy(const VisitResult& to_copy) {
  std::string temp = GenerateNewTempVariable(to_copy.type());
  source_out() << RValueFlattenStructs(to_copy) << ";" << std::endl;
  GenerateIndent();
  source_out() << "USE(" << temp << ");" << std::endl;
  return VisitResult(to_copy.type(), temp);
}

VisitResult ImplementationVisitor::Visit(StructExpression* decl) {
  const Type* raw_type = declarations()->LookupType(decl->name);
  if (!raw_type->IsStructType()) {
    std::stringstream s;
    s << decl->name << " is not a struct but used like one ";
    ReportError(s.str());
  }
  const StructType* struct_type = StructType::cast(raw_type);
  if (struct_type->fields().size() != decl->expressions.size()) {
    std::stringstream s;
    s << "initializer count mismatch for struct " << decl->name << " (expected "
      << struct_type->fields().size() << ", found " << decl->expressions.size()
      << ")";
    ReportError(s.str());
  }
  std::vector<VisitResult> expression_results;
  for (auto& field : struct_type->fields()) {
    VisitResult value = Visit(decl->expressions[expression_results.size()]);
    value = GenerateImplicitConvert(field.type, value);
    expression_results.push_back(value);
  }
  std::string result_var_name = GenerateNewTempVariable(struct_type);
  source_out() << "{";
  PrintCommaSeparatedList(
      source_out(), expression_results,
      [&](const VisitResult& result) { return RValueFlattenStructs(result); });
  source_out() << "};\n";
  return VisitResult(struct_type, result_var_name);
}

LocationReference ImplementationVisitor::GetLocationReference(
    LocationExpression* location) {
  switch (location->kind) {
    case AstNode::Kind::kIdentifierExpression:
      return GetLocationReference(static_cast<IdentifierExpression*>(location));
    case AstNode::Kind::kFieldAccessExpression:
      return GetLocationReference(
          static_cast<FieldAccessExpression*>(location));
    case AstNode::Kind::kElementAccessExpression:
      return GetLocationReference(
          static_cast<ElementAccessExpression*>(location));
    default:
      UNREACHABLE();
  }
}

LocationReference ImplementationVisitor::GetLocationReference(
    FieldAccessExpression* expr) {
  VisitResult result = Visit(expr->object);
  if (result.type()->IsStructType()) {
    if (result.declarable()) {
      return LocationReference(
          declarations()->LookupValue((*result.declarable())->name() + "." +
                                      expr->field),
          {}, {});

    } else {
      return LocationReference(
          nullptr,
          VisitResult(result.type(), result.RValue() + "." + expr->field), {});
    }
  }
  return LocationReference(nullptr, result, {});
}

std::string ImplementationVisitor::RValueFlattenStructs(VisitResult result) {
  if (result.declarable()) {
    const Value* value = *result.declarable();
    const Type* type = value->type();
    if (const StructType* struct_type = StructType::DynamicCast(type)) {
      std::stringstream s;
      s << struct_type->name() << "{";
      PrintCommaSeparatedList(
          s, struct_type->fields(), [&](const NameAndType& field) {
            std::string field_declaration = value->name() + "." + field.name;
            Variable* field_variable =
                Variable::cast(declarations()->LookupValue(field_declaration));
            return RValueFlattenStructs(
                VisitResult(field_variable->type(), field_variable));
          });
      s << "}";
      return s.str();
    }
  }
  return result.RValue();
}

VisitResult ImplementationVisitor::GenerateFetchFromLocation(
    LocationExpression* location, LocationReference reference) {
  switch (location->kind) {
    case AstNode::Kind::kIdentifierExpression:
      return GenerateFetchFromLocation(
          static_cast<IdentifierExpression*>(location), reference);
    case AstNode::Kind::kFieldAccessExpression:
      return GenerateFetchFromLocation(
          static_cast<FieldAccessExpression*>(location), reference);
    case AstNode::Kind::kElementAccessExpression:
      return GenerateFetchFromLocation(
          static_cast<ElementAccessExpression*>(location), reference);
    default:
      UNREACHABLE();
  }
}

VisitResult ImplementationVisitor::GenerateFetchFromLocation(
    FieldAccessExpression* expr, LocationReference reference) {
  const Type* type = reference.base.type();
  if (reference.value != nullptr) {
    return GenerateFetchFromLocation(reference);
  } else if (const StructType* struct_type = StructType::DynamicCast(type)) {
    auto& fields = struct_type->fields();
    auto i = std::find_if(
        fields.begin(), fields.end(),
        [&](const NameAndType& f) { return f.name == expr->field; });
    if (i == fields.end()) {
      std::stringstream s;
      s << "\"" << expr->field << "\" is not a field of struct type \""
        << struct_type->name() << "\"";
      ReportError(s.str());
    }
    return VisitResult(i->type, reference.base.RValue());
  } else {
    Arguments arguments;
    arguments.parameters = {reference.base};
    return GenerateCall(std::string(".") + expr->field, arguments);
  }
}

void ImplementationVisitor::GenerateAssignToVariable(Variable* var,
                                                     VisitResult value) {
  if (var->type()->IsStructType()) {
    if (value.type() != var->type()) {
      std::stringstream s;
      s << "incompatable assignment from type " << *value.type() << " to "
        << *var->type();
      ReportError(s.str());
    }
    const StructType* struct_type = StructType::cast(var->type());
    for (auto& field : struct_type->fields()) {
      std::string field_declaration = var->name() + "." + field.name;
      Variable* field_variable =
          Variable::cast(declarations()->LookupValue(field_declaration));
      if (value.declarable() && (*value.declarable())->IsVariable()) {
        Variable* source_field = Variable::cast(declarations()->LookupValue(
            Variable::cast((*value.declarable()))->name() + "." + field.name));
        GenerateAssignToVariable(
            field_variable, VisitResult{source_field->type(), source_field});
      } else {
        GenerateAssignToVariable(
            field_variable, VisitResult{field_variable->type(),
                                        value.RValue() + "." + field.name});
      }
    }
  } else {
    VisitResult casted_value = GenerateImplicitConvert(var->type(), value);
    GenerateIndent();
    VisitResult var_value = {var->type(), var};
    source_out() << var_value.LValue() << " = "
                 << RValueFlattenStructs(casted_value) << ";" << std::endl;
  }
  var->Define();
}

void ImplementationVisitor::GenerateAssignToLocation(
    LocationExpression* location, const LocationReference& reference,
    VisitResult assignment_value) {
  if (reference.value != nullptr) {
    Value* value = reference.value;
    Variable* var = Variable::cast(value);
    if (var->IsConst()) {
      std::stringstream s;
      s << "\"" << var->name()
        << "\" is declared const (maybe implicitly) and cannot be assigned to";
      ReportError(s.str());
    }
    GenerateAssignToVariable(var, assignment_value);
  } else if (auto access = FieldAccessExpression::cast(location)) {
    GenerateCall(std::string(".") + access->field + "=",
                 {{reference.base, assignment_value}, {}});
  } else {
    DCHECK_NOT_NULL(ElementAccessExpression::cast(location));
    GenerateCall("[]=",
                 {{reference.base, reference.index, assignment_value}, {}});
  }
}

void ImplementationVisitor::GenerateVariableDeclaration(const Variable* var) {
  const Type* var_type = var->type();
  if (var_type->IsStructType()) {
    const StructType* struct_type = StructType::cast(var_type);
    for (auto& field : struct_type->fields()) {
      GenerateVariableDeclaration(Variable::cast(
          declarations()->LookupValue(var->name() + "." + field.name)));
    }
  } else {
    std::string value = var->value();
    GenerateIndent();
    if (var_type->IsConstexpr()) {
      source_out() << var_type->GetGeneratedTypeName();
      source_out() << " " << value << "_impl;" << std::endl;
    } else if (var->IsConst()) {
      source_out() << "TNode<" << var->type()->GetGeneratedTNodeTypeName();
      source_out() << "> " << var->value() << "_impl;\n";
    } else {
      source_out() << "TVARIABLE(";
      source_out() << var_type->GetGeneratedTNodeTypeName();
      source_out() << ", " << value << "_impl);" << std::endl;
    }
    GenerateIndent();
    source_out() << "auto " << value << " = &" << value << "_impl;"
                 << std::endl;
    GenerateIndent();
    source_out() << "USE(" << value << ");" << std::endl;
  }
}

Variable* ImplementationVisitor::GenerateVariableDeclaration(
    AstNode* node, const std::string& name,
    const base::Optional<const Type*>& type,
    const base::Optional<VisitResult>& initialization) {

  Variable* variable = nullptr;
  if (declarations()->TryLookup(name)) {
    variable = Variable::cast(declarations()->LookupValue(name));
  } else {
    variable = declarations()->DeclareVariable(name, *type, false);
    // Because the variable is being defined during code generation, it must be
    // assumed that it changes along all control split paths because it's no
    // longer possible to run the control-flow anlaysis in the declaration pass
    // over the variable.
    global_context_.MarkVariableChanged(
        node, declarations()->GetCurrentSpecializationTypeNamesVector(),
        variable);
  }
  GenerateVariableDeclaration(variable);
  if (initialization) {
    GenerateAssignToVariable(variable, *initialization);
  }
  return variable;
}

void ImplementationVisitor::GenerateParameter(
    const std::string& parameter_name) {
  const Value* val = declarations()->LookupValue(parameter_name);
  std::string var = val->value();
  GenerateIndent();
  source_out() << val->type()->GetGeneratedTypeName() << " " << var << " = ";

  source_out() << "UncheckedCast<" << val->type()->GetGeneratedTNodeTypeName()
               << ">(Parameter(Descriptor::k" << CamelifyString(parameter_name)
               << "));" << std::endl;
  GenerateIndent();
  source_out() << "USE(" << var << ");" << std::endl;
}

void ImplementationVisitor::GenerateParameterList(const NameVector& list,
                                                  size_t first) {
  for (auto p : list) {
    if (first == 0) {
      GenerateParameter(p);
    } else {
      first--;
    }
  }
}

VisitResult ImplementationVisitor::GeneratePointerCall(
    Expression* callee, const Arguments& arguments, bool is_tailcall) {
  TypeVector parameter_types(arguments.parameters.GetTypeVector());
  VisitResult callee_result = Visit(callee);
  if (!callee_result.type()->IsFunctionPointerType()) {
    std::stringstream stream;
    stream << "Expected a function pointer type but found "
           << *callee_result.type();
    ReportError(stream.str());
  }
  const FunctionPointerType* type =
      FunctionPointerType::cast(callee_result.type());

  if (type->parameter_types().size() != parameter_types.size()) {
    std::stringstream stream;
    stream << "parameter count mismatch calling function pointer with Type: "
           << *type << " - expected "
           << std::to_string(type->parameter_types().size()) << ", found "
           << std::to_string(parameter_types.size());
    ReportError(stream.str());
  }

  ParameterTypes types{type->parameter_types(), false};
  Signature sig;
  sig.parameter_types = types;
  if (!IsCompatibleSignature(sig, parameter_types, {})) {
    std::stringstream stream;
    stream << "parameters do not match function pointer signature. Expected: ("
           << type->parameter_types() << ") but got: (" << parameter_types
           << ")";
    ReportError(stream.str());
  }

  std::vector<std::string> variables;
  for (size_t current = 0; current < arguments.parameters.size(); ++current) {
    const Type* to_type = type->parameter_types()[current];
    VisitResult result =
        GenerateImplicitConvert(to_type, arguments.parameters[current]);
    variables.push_back(RValueFlattenStructs(result));
  }

  std::string result_variable_name;
  bool no_result = type->return_type()->IsVoidOrNever() || is_tailcall;
  if (no_result) {
    GenerateIndent();
  } else {
    const Type* return_type = type->return_type();
    result_variable_name = GenerateNewTempVariable(return_type);
    if (return_type->IsStructType()) {
      source_out() << "(";
    } else {
      source_out() << "UncheckedCast<";
      source_out() << type->return_type()->GetGeneratedTNodeTypeName();
      source_out() << ">(";
    }
  }

  Builtin* example_builtin =
      declarations()->FindSomeInternalBuiltinWithType(type);
  if (!example_builtin) {
    std::stringstream stream;
    stream << "unable to find any builtin with type \"" << *type << "\"";
    ReportError(stream.str());
  }

  if (is_tailcall) {
    source_out() << "TailCallStub(";
  } else {
    source_out() << "CallStub(";
  }
  source_out() << "Builtins::CallableFor(isolate(), Builtins::k"
               << example_builtin->name() << ").descriptor(), "
               << RValueFlattenStructs(callee_result) << ", ";

  size_t total_parameters = 0;
  for (size_t i = 0; i < arguments.parameters.size(); ++i) {
    if (total_parameters++ != 0) {
      source_out() << ", ";
    }
    source_out() << variables[i];
  }
  if (!no_result) {
    source_out() << ")";
  }
  source_out() << ");" << std::endl;
  return VisitResult(type->return_type(), result_variable_name);
}

VisitResult ImplementationVisitor::GenerateCall(
    const std::string& callable_name, Arguments arguments, bool is_tailcall) {
  Callable* callable = LookupCall(callable_name, arguments);

  // Operators used in a branching context can also be function calls that never
  // return but have a True and False label
  if (arguments.labels.size() == 0 &&
      callable->signature().labels.size() == 2) {
    Label* true_label = declarations()->LookupLabel(kTrueLabelName);
    arguments.labels.push_back(true_label);
    Label* false_label = declarations()->LookupLabel(kFalseLabelName);
    arguments.labels.push_back(false_label);
  }

  const Type* result_type = callable->signature().return_type;

  std::vector<std::string> variables;
  for (size_t current = 0; current < arguments.parameters.size(); ++current) {
    const Type* to_type = (current >= callable->signature().types().size())
                              ? TypeOracle::GetObjectType()
                              : callable->signature().types()[current];
    VisitResult result =
        GenerateImplicitConvert(to_type, arguments.parameters[current]);
    variables.push_back(RValueFlattenStructs(result));
  }

  std::string result_variable_name;
  if (result_type->IsVoidOrNever() || is_tailcall) {
    GenerateIndent();
  } else {
    result_variable_name = GenerateNewTempVariable(result_type);
    if (!result_type->IsConstexpr()) {
      if (result_type->IsStructType()) {
        source_out() << "(";
      } else {
        source_out() << "UncheckedCast<";
        source_out() << result_type->GetGeneratedTNodeTypeName();
        source_out() << ">(";
      }
    }
  }
  if (callable->IsBuiltin()) {
    if (is_tailcall) {
      source_out() << "TailCallBuiltin(Builtins::k" << callable->name() << ", ";
    } else {
      source_out() << "CallBuiltin(Builtins::k" << callable->name() << ", ";
    }
  } else if (callable->IsMacro()) {
    if (is_tailcall) {
      std::stringstream stream;
      stream << "can't tail call a macro";
      ReportError(stream.str());
    }
    source_out() << callable->name() << "(";
  } else if (callable->IsRuntimeFunction()) {
    if (is_tailcall) {
      source_out() << "TailCallRuntime(Runtime::k" << callable->name() << ", ";
    } else {
      source_out() << "CallRuntime(Runtime::k" << callable->name() << ", ";
    }
  } else {
    UNREACHABLE();
  }
  if (global_context_.verbose()) {
    std::cout << "generating code for call to " << callable_name << "\n";
  }

  size_t total_parameters = 0;
  for (size_t i = 0; i < arguments.parameters.size(); ++i) {
    if (total_parameters++ != 0) {
      source_out() << ", ";
    }
    source_out() << variables[i];
  }

  size_t label_count = callable->signature().labels.size();
  if (label_count != arguments.labels.size()) {
    std::stringstream s;
    s << "unexpected number of otherwise labels for " << callable->name()
      << " (expected " << std::to_string(label_count) << " found "
      << std::to_string(arguments.labels.size()) << ")";
    ReportError(s.str());
  }
  for (size_t i = 0; i < label_count; ++i) {
    if (total_parameters++ != 0) {
      source_out() << ", ";
    }
    Label* label = arguments.labels[i];
    size_t callee_label_parameters =
        callable->signature().labels[i].types.size();
    if (label->GetParameterCount() != callee_label_parameters) {
      std::stringstream s;
      s << "label " << label->name()
        << " doesn't have the right number of parameters (found "
        << std::to_string(label->GetParameterCount()) << " expected "
        << std::to_string(callee_label_parameters) << ")";
      ReportError(s.str());
    }
    source_out() << label->generated();
    size_t j = 0;
    for (auto t : callable->signature().labels[i].types) {
      source_out() << ", ";
      Variable* variable = label->GetParameter(j);
      if (!(variable->type() == t)) {
        std::stringstream s;
        s << "mismatch of label parameters (expected " << *t << " got "
          << *label->GetParameter(j)->type() << " for parameter "
          << std::to_string(i + 1) << ")";
        ReportError(s.str());
      }
      j++;
      source_out() << variable->value();
    }
    label->MarkUsed();
  }

  if (global_context_.verbose()) {
    std::cout << "finished generating code for call to " << callable_name
              << "\n";
  }
  if (!result_type->IsVoidOrNever() && !is_tailcall &&
      !result_type->IsConstexpr()) {
    source_out() << ")";
  }
  source_out() << ");" << std::endl;
  return VisitResult(result_type, result_variable_name);
}

void ImplementationVisitor::Visit(StandardDeclaration* decl) {
  Signature signature = MakeSignature(decl->callable->signature.get());
  Visit(decl->callable, signature, decl->body);
}

void ImplementationVisitor::Visit(SpecializationDeclaration* decl) {
  Signature signature_with_types = MakeSignature(decl->signature.get());
  Declarations::NodeScopeActivator specialization_activator(declarations(),
                                                            decl);
  GenericList* generic_list = declarations()->LookupGeneric(decl->name);
  for (Generic* generic : generic_list->list()) {
    CallableNode* callable = generic->declaration()->callable;
    Signature generic_signature_with_types =
        MakeSignature(callable->signature.get());
    if (signature_with_types.HasSameTypesAs(generic_signature_with_types)) {
      TypeVector specialization_types = GetTypeVector(decl->generic_parameters);
      SpecializeGeneric({{generic, specialization_types},
                         callable,
                         decl->signature.get(),
                         decl->body,
                         decl->pos});
      return;
    }
  }
  // Because the DeclarationVisitor already performed the same lookup
  // as above to find aspecialization match and already threw if it didn't
  // find one, failure to find a match here should never happen.
  // TODO(danno): Remember the specialization found in the declaration visitor
  //              so that the lookup doesn't have to be repeated here.
  UNREACHABLE();
}

VisitResult ImplementationVisitor::Visit(CallExpression* expr,
                                         bool is_tailcall) {
  Arguments arguments;
  std::string name = expr->callee.name;
  bool has_template_arguments = expr->callee.generic_arguments.size() != 0;
  if (has_template_arguments) {
    TypeVector specialization_types =
        GetTypeVector(expr->callee.generic_arguments);
    name = GetGeneratedCallableName(name, specialization_types);
    for (auto generic :
         declarations()->LookupGeneric(expr->callee.name)->list()) {
      CallableNode* callable = generic->declaration()->callable;
      if (generic->declaration()->body) {
        QueueGenericSpecialization({generic, specialization_types}, callable,
                                   callable->signature.get(),
                                   generic->declaration()->body);
      }
    }
  }
  for (Expression* arg : expr->arguments)
    arguments.parameters.push_back(Visit(arg));
  arguments.labels = LabelsFromIdentifiers(expr->labels);
  VisitResult result;
  if (!has_template_arguments &&
      declarations()->Lookup(expr->callee.name)->IsValue()) {
    result = GeneratePointerCall(&expr->callee, arguments, is_tailcall);
  } else {
    result = GenerateCall(name, arguments, is_tailcall);
  }
  if (!result.type()->IsVoidOrNever()) {
    GenerateIndent();
    source_out() << "USE(" << RValueFlattenStructs(result) << ");" << std::endl;
  }
  if (is_tailcall) {
    result = {TypeOracle::GetNeverType(), ""};
  }
  return result;
}

bool ImplementationVisitor::GenerateLabeledStatementBlocks(
    const std::vector<Statement*>& blocks,
    const std::vector<Label*>& statement_labels, Label* merge_label) {
  bool live = false;
  auto label_iterator = statement_labels.begin();
  for (Statement* block : blocks) {
    GenerateIndent();
    source_out() << "if (" << (*label_iterator)->generated() << "->is_used())"
                 << std::endl;
    ScopedIndent indent(this);

    GenerateLabelBind(*label_iterator++);
    if (!Visit(block)->IsNever()) {
      GenerateLabelGoto(merge_label);
      live = true;
    }
  }
  return live;
}

void ImplementationVisitor::GenerateBranch(const VisitResult& condition,
                                           Label* true_label,
                                           Label* false_label) {
  GenerateIndent();
  source_out() << "Branch(" << RValueFlattenStructs(condition) << ", "
               << true_label->generated() << ", " << false_label->generated()
               << ");" << std::endl;
}

bool ImplementationVisitor::GenerateExpressionBranch(
    Expression* expression, const std::vector<Label*>& statement_labels,
    const std::vector<Statement*>& statement_blocks, Label* merge_label) {
  // Activate a new scope to define True/False catch labels
  Declarations::NodeScopeActivator scope(declarations(), expression);

  VisitResult expression_result = Visit(expression);
  if (expression_result.type() == TypeOracle::GetBoolType()) {
    GenerateBranch(expression_result, statement_labels[0], statement_labels[1]);
  } else {
    if (expression_result.type() != TypeOracle::GetNeverType()) {
      std::stringstream s;
      s << "unexpected return type " << *expression_result.type()
        << " for branch expression";
      ReportError(s.str());
    }
  }

  return GenerateLabeledStatementBlocks(statement_blocks, statement_labels,
                                        merge_label);
}

VisitResult ImplementationVisitor::GenerateImplicitConvert(
    const Type* destination_type, VisitResult source) {
  if (destination_type == source.type()) {
    return source;
  }

  if (TypeOracle::IsImplicitlyConvertableFrom(destination_type,
                                              source.type())) {
    std::string name =
        GetGeneratedCallableName(kFromConstexprMacroName, {destination_type});
    return GenerateCall(name, {{source}, {}}, false);
  } else if (IsAssignableFrom(destination_type, source.type())) {
    source.SetType(destination_type);
    return source;
  } else {
    std::stringstream s;
    s << "cannot use expression of type " << *source.type()
      << " as a value of type " << *destination_type;
    ReportError(s.str());
  }
  return VisitResult(TypeOracle::GetVoidType(), "");
}

std::string ImplementationVisitor::NewTempVariable() {
  std::string name("t");
  name += std::to_string(next_temp_++);
  return name;
}

std::string ImplementationVisitor::GenerateNewTempVariable(const Type* type) {
  std::string temp = NewTempVariable();
  GenerateIndent();
  source_out() << type->GetGeneratedTypeName() << " " << temp << " = ";
  return temp;
}

void ImplementationVisitor::GenerateLabelDefinition(Label* label,
                                                    AstNode* node) {
  std::string label_string = label->generated();
  std::string label_string_impl = label_string + "_impl";
  GenerateIndent();
  source_out() << "Label " + label_string_impl + "(this";
  if (node != nullptr) {
    source_out() << ", ";
    GenerateChangedVarsFromControlSplit(node);
  }
  source_out() << ");" << std::endl;
  GenerateIndent();
  source_out() << "Label* " + label_string + " = &" << label_string_impl << ";"
               << std::endl;
  GenerateIndent();
  source_out() << "USE(" << label_string << ");" << std::endl;
}

void ImplementationVisitor::GenerateLabelBind(Label* label) {
  GenerateIndent();
  source_out() << "BIND(" << label->generated() << ");" << std::endl;
}

void ImplementationVisitor::GenerateLabelGoto(Label* label) {
  GenerateIndent();
  source_out() << "Goto(" << label->generated() << ");" << std::endl;
}

std::vector<Label*> ImplementationVisitor::LabelsFromIdentifiers(
    const std::vector<std::string>& names) {
  std::vector<Label*> result;
  for (auto name : names) {
    result.push_back(declarations()->LookupLabel(name));
  }
  return result;
}

}  // namespace torque
}  // namespace internal
}  // namespace v8
