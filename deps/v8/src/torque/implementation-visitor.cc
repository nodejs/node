// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/torque/implementation-visitor.h"

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

void ImplementationVisitor::Visit(ModuleDeclaration* decl) {
  Module* module = decl->GetModule();

  std::ostream& source = module->source_stream();
  std::ostream& header = module->header_stream();

  if (decl->IsDefault()) {
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
  if (decl->IsDefault()) {
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

  Module* saved_module = module_;
  module_ = module;
  Declarations::NodeScopeActivator scope(declarations(), decl);
  for (auto& child : decl->declarations) Visit(child);
  module_ = saved_module;

  DrainSpecializationQueue();

  source << "}  // namepsace internal" << std::endl
         << "}  // namespace v8" << std::endl
         << "" << std::endl;

  header << "};" << std::endl << "" << std::endl;
  header << "}  // namepsace internal" << std::endl
         << "}  // namespace v8" << std::endl
         << "" << std::endl;
  header << "#endif  // " << headerDefine << std::endl;
}

void ImplementationVisitor::Visit(TorqueMacroDeclaration* decl,
                                  const Signature& sig, Statement* body) {
  Signature signature = MakeSignature(decl, decl->signature.get());
  std::string name = GetGeneratedCallableName(
      decl->name, declarations()->GetCurrentSpecializationTypeNamesVector());
  const TypeVector& list = signature.types();
  Macro* macro = declarations()->LookupMacro(name, list);

  CurrentCallableActivator activator(global_context_, macro, decl);

  header_out() << "  ";
  GenerateMacroFunctionDeclaration(header_out(), "", macro);
  header_out() << ";" << std::endl;

  GenerateMacroFunctionDeclaration(
      source_out(), GetDSLAssemblerName(CurrentModule()) + "::", macro);
  source_out() << " {" << std::endl;

  const Variable* result_var = nullptr;
  if (macro->HasReturnValue()) {
    const Type* return_type = macro->signature().return_type;
    if (!return_type->IsConstexpr()) {
      GenerateIndent();
      source_out() << "Node* return_default = &*SmiConstant(0);" << std::endl;
    }
    VisitResult init = {
        return_type,
        return_type->IsConstexpr()
            ? (return_type->GetGeneratedTypeName() + "()")
            : (std::string("UncheckedCast<") +
               return_type->GetGeneratedTNodeTypeName() + ">(return_default)")};
    result_var =
        GenerateVariableDeclaration(decl, kReturnValueVariable, {}, init);
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
    source_out() << "return " << result_var->GetValueForRead() << ";"
                 << std::endl;
  }
  source_out() << "}" << std::endl << std::endl;
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
  source_out() << "TNode<Context> " << val->GetValueForDeclaration()
               << " = UncheckedCast<Context>(Parameter("
               << (builtin->IsVarArgsJavaScript() ? "Builtin" : "")
               << "Descriptor::kContext));" << std::endl;
  GenerateIndent();
  source_out() << "USE(" << val->GetValueForDeclaration() << ");" << std::endl;

  size_t first = 1;
  if (builtin->IsVarArgsJavaScript()) {
    assert(decl->signature->parameters.has_varargs);
    Constant* arguments = Constant::cast(declarations()->LookupValue(
        decl->signature->parameters.arguments_variable));
    std::string arguments_name = arguments->GetValueForDeclaration();
    GenerateIndent();
    source_out()
        << "Node* argc = Parameter(BuiltinDescriptor::kArgumentsCount);"
        << std::endl;
    GenerateIndent();
    source_out() << "CodeStubArguments arguments_impl(this, "
                    "ChangeInt32ToIntPtr(argc));"
                 << std::endl;
    const Value* receiver =
        declarations()->LookupValue(decl->signature->parameters.names[1]);
    GenerateIndent();
    source_out() << "TNode<Object> " << receiver->GetValueForDeclaration()
                 << " = arguments_impl.GetReceiver();" << std::endl;
    GenerateIndent();
    source_out() << "auto arguments = &arguments_impl;" << std::endl;
    GenerateIndent();
    source_out() << "USE(arguments);" << std::endl;
    GenerateIndent();
    source_out() << "USE(" << receiver->GetValueForDeclaration() << ");"
                 << std::endl;
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
  return GetTypeOracle().GetVoidType();
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
    source_out() << "return " << left.variable() << ";" << std::endl;
  }
  source_out() << ";" << std::endl;
  GenerateIndent();
  source_out() << "auto " << f2 << " = [=]() ";
  {
    ScopedIndent indent(this, false);
    source_out() << "" << std::endl;
    right = Visit(expr->if_false);
    GenerateIndent();
    source_out() << "return " << right.variable() << ";" << std::endl;
  }
  source_out() << ";" << std::endl;

  const Type* common_type = GetCommonType(left.type(), right.type());
  std::string result_var = NewTempVariable();
  const Variable* result =
      GenerateVariableDeclaration(expr, result_var, common_type);

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
      GenerateBranch(condition_result, true_label, false_label);
    }

    GenerateLabelBind(true_label);
    GenerateIndent();
    source_out() << result->GetValueForWrite() << " = " << f1 << "();"
                 << std::endl;
    GenerateLabelGoto(done_label);

    GenerateLabelBind(false_label);
    GenerateIndent();
    source_out() << result->GetValueForWrite() << " = " << f2 << "();"
                 << std::endl;
    GenerateLabelGoto(done_label);

    GenerateLabelBind(done_label);
  }
  return VisitResult(common_type, result->GetValueForRead());
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
      source_out() << "GotoIf(" << left_result.variable() << ", "
                   << true_label->generated() << ");" << std::endl;
    } else if (!left_result.type()->IsConstexprBool()) {
      GenerateLabelBind(false_label);
    }
  }
  VisitResult right_result = Visit(expr->right);
  if (right_result.type() != left_result.type()) {
    std::stringstream stream;
    stream << "types of left and right expression of logical OR don't match (\""
           << left_result.type() << "\" vs. \"" << right_result.type() << "\")";
    ReportError(stream.str());
  }
  if (left_result.type()->IsConstexprBool()) {
    return VisitResult(left_result.type(), std::string("(") +
                                               left_result.variable() + " || " +
                                               right_result.variable() + ")");
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
      source_out() << "GotoIfNot(" << left_result.variable() << ", "
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
        << left_result.type() << "\" vs. \"" << right_result.type() << "\")";
    ReportError(stream.str());
  }
  if (left_result.type()->IsConstexprBool()) {
    return VisitResult(left_result.type(), std::string("(") +
                                               left_result.variable() + " && " +
                                               right_result.variable() + ")");
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
  VisitResult one = {GetTypeOracle().GetConstInt31Type(), "1"};
  Arguments args;
  args.parameters = {current_value, one};
  VisitResult assignment_value = GenerateOperation(
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
    assignment_value = GenerateOperation(*expr->op, args);
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
    if (Internals::IsValidSmi(i)) {
      if (sizeof(void*) == sizeof(double) && ((i >> 30) != (i >> 31))) {
        result_type = declarations()->LookupType(CONST_INT32_TYPE_STRING);
      } else {
        result_type = declarations()->LookupType(CONST_INT31_TYPE_STRING);
      }
    }
  }
  std::string temp = GenerateNewTempVariable(result_type);
  source_out() << expr->number << ";" << std::endl;
  return VisitResult{result_type, temp};
}

VisitResult ImplementationVisitor::Visit(StringLiteralExpression* expr) {
  std::string temp = GenerateNewTempVariable(GetTypeOracle().GetStringType());
  source_out() << "StringConstant(\""
               << expr->literal.substr(1, expr->literal.size() - 2) << "\");"
               << std::endl;
  return VisitResult{GetTypeOracle().GetStringType(), temp};
}

VisitResult ImplementationVisitor::GetBuiltinCode(Builtin* builtin) {
  if (builtin->IsExternal() || builtin->kind() != Builtin::kStub) {
    ReportError(
        "creating function pointers is only allowed for internal builtins with "
        "stub linkage");
  }
  const Type* type = declarations()->GetFunctionPointerType(
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
    Generic* generic = declarations()->LookupGeneric(expr->name);
    TypeVector specialization_types = GetTypeVector(expr->generic_arguments);
    name = GetGeneratedCallableName(name, specialization_types);
    CallableNode* callable = generic->declaration()->callable;
    QueueGenericSpecialization({generic, specialization_types}, callable,
                               callable->signature.get(),
                               generic->declaration()->body);
  }

  if (Builtin* builtin = Builtin::DynamicCast(declarations()->Lookup(name))) {
    return GetBuiltinCode(builtin);
  }

  return GenerateFetchFromLocation(expr, GetLocationReference(expr));
}

VisitResult ImplementationVisitor::Visit(CastExpression* expr) {
  Arguments args;
  args.parameters = {Visit(expr->value)};
  args.labels = LabelsFromIdentifiers({expr->otherwise});
  return GenerateOperation("cast<>", args, declarations()->GetType(expr->type));
}

VisitResult ImplementationVisitor::Visit(ConvertExpression* expr) {
  Arguments args;
  args.parameters = {Visit(expr->value)};
  return GenerateOperation("convert<>", args,
                           declarations()->GetType(expr->type));
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
  return GetTypeOracle().GetNeverType();
}

const Type* ImplementationVisitor::Visit(IfStatement* stmt) {
  ScopedIndent indent(this);

  bool has_else = stmt->if_false.has_value();

  if (stmt->is_constexpr) {
    VisitResult expression_result = Visit(stmt->condition);

    if (!(expression_result.type() == GetTypeOracle().GetConstexprBoolType())) {
      std::stringstream stream;
      stream << "expression should return type \"constexpr bool\" but doesn't";
      ReportError(stream.str());
    }

    const Type* left_result;
    const Type* right_result = GetTypeOracle().GetVoidType();
    {
      GenerateIndent();
      source_out() << "if ((" << expression_result.variable() << ")) ";
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
    return live ? GetTypeOracle().GetVoidType()
                : GetTypeOracle().GetNeverType();
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
  return GetTypeOracle().GetVoidType();
}

const Type* ImplementationVisitor::Visit(BlockStatement* block) {
  Declarations::NodeScopeActivator scope(declarations(), block);
  ScopedIndent indent(this);
  const Type* type = GetTypeOracle().GetVoidType();
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
    return GetTypeOracle().GetNeverType();
  } else {
    source_out() << "DebugBreak();" << std::endl;
    return GetTypeOracle().GetVoidType();
  }
}

const Type* ImplementationVisitor::Visit(AssertStatement* stmt) {
#if defined(DEBUG)
  // CSA_ASSERT & co. are not used here on purpose for two reasons. First,
  // Torque allows and handles two types of expressions in the if protocol
  // automagically, ones that return TNode<BoolT> and those that use the
  // BranchIf(..., Label* true, Label* false) idiom. Because the machinery to
  // handle this is embedded in the expression handling and to it's not possible
  // to make the decision to use CSA_ASSERT or CSA_ASSERT_BRANCH isn't trivial
  // up-front. Secondly, on failure, the assert text should be the corresponding
  // Torque code, not the -gen.cc code, which would be the case when using
  // CSA_ASSERT_XXX.
  Label* true_label = nullptr;
  Label* false_label = nullptr;
  Declarations::NodeScopeActivator scope(declarations(), stmt->expression);
  true_label = declarations()->LookupLabel(kTrueLabelName);
  GenerateLabelDefinition(true_label);
  false_label = declarations()->LookupLabel(kFalseLabelName);
  GenerateLabelDefinition(false_label);

  VisitResult expression_result = Visit(stmt->expression);
  if (expression_result.type() == GetTypeOracle().GetBoolType()) {
    GenerateBranch(expression_result, true_label, false_label);
  } else {
    if (expression_result.type() != GetTypeOracle().GetNeverType()) {
      std::stringstream s;
      s << "unexpected return type " << expression_result.type()
        << " for branch expression";
      ReportError(s.str());
    }
  }

  GenerateLabelBind(false_label);
  GenerateIndent();
  source_out() << "Print(\""
               << "assert '" << stmt->source << "' failed at "
               << PositionAsString(stmt->pos) << "\");" << std::endl;
  GenerateIndent();
  source_out() << "Unreachable();" << std::endl;

  GenerateLabelBind(true_label);
#endif
  return GetTypeOracle().GetVoidType();
}

const Type* ImplementationVisitor::Visit(ExpressionStatement* stmt) {
  const Type* type = Visit(stmt->expression).type();
  return type->IsNever() ? type : GetTypeOracle().GetVoidType();
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
        << current_callable->signature().return_type;
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
        source_out() << "arguments->PopAndReturn(" << return_result.variable()
                     << ");" << std::endl;
      } else {
        GenerateIndent();
        source_out() << "Return(" << return_result.variable() << ");"
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
  return GetTypeOracle().GetNeverType();
}

const Type* ImplementationVisitor::Visit(ForOfLoopStatement* stmt) {
  Declarations::NodeScopeActivator scope(declarations(), stmt);

  VisitResult expression_result = Visit(stmt->iterable);
  VisitResult begin =
      stmt->begin ? Visit(*stmt->begin)
                  : VisitResult(GetTypeOracle().GetConstInt31Type(), "0");

  VisitResult end =
      stmt->end ? Visit(*stmt->end)
                : GenerateOperation(".length", {{expression_result}, {}});

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

  VisitResult index_for_read = {index_var->type(),
                                index_var->GetValueForRead()};

  Label* header_label = declarations()->DeclarePrivateLabel("header");
  GenerateLabelDefinition(header_label, stmt);

  GenerateLabelGoto(header_label);

  GenerateLabelBind(header_label);

  BreakContinueActivator activator(global_context_, exit_label,
                                   increment_label);

  VisitResult result = GenerateOperation("<", {{index_for_read, end}, {}});
  GenerateBranch(result, body_label, exit_label);

  GenerateLabelBind(body_label);
  VisitResult element_result =
      GenerateOperation("[]", {{expression_result, index_for_read}, {}});
  GenerateVariableDeclaration(stmt->var_declaration,
                              stmt->var_declaration->name, {}, element_result);
  Visit(stmt->body);
  GenerateLabelGoto(increment_label);

  GenerateLabelBind(increment_label);
  Arguments increment_args;
  increment_args.parameters = {index_for_read,
                               {GetTypeOracle().GetConstInt31Type(), "1"}};
  VisitResult increment_result = GenerateOperation("+", increment_args);

  GenerateAssignToVariable(index_var, increment_result);

  GenerateLabelGoto(header_label);

  GenerateLabelBind(exit_label);
  return GetTypeOracle().GetVoidType();
}

const Type* ImplementationVisitor::Visit(TryCatchStatement* stmt) {
  ScopedIndent indent(this);
  Label* try_done = declarations()->DeclarePrivateLabel("try_done");
  GenerateLabelDefinition(try_done);
  const Type* try_result = GetTypeOracle().GetNeverType();
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
      try_result = GetTypeOracle().GetVoidType();
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
    try_result = GetTypeOracle().GetVoidType();
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
  return GetTypeOracle().GetNeverType();
}

const Type* ImplementationVisitor::Visit(ContinueStatement* stmt) {
  Label* continue_label = global_context_.GetCurrentContinue();
  if (continue_label == nullptr) {
    ReportError("continue used outside of loop");
  }
  GenerateLabelGoto(continue_label);
  return GetTypeOracle().GetNeverType();
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
  return GetTypeOracle().GetVoidType();
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
  if (global_context_.verbose()) {
    std::cout << "generating source for declaration " << *macro << ""
              << std::endl;
  }

  // Quite a hack here. Make sure that TNode is namespace qualified if the
  // macro name is also qualified.
  std::string return_type_name(
      macro->signature().return_type->GetGeneratedTypeName());
  if (macro_prefix != "" && (return_type_name.length() > 5) &&
      (return_type_name.substr(0, 5) == "TNode")) {
    o << "compiler::";
  }
  o << return_type_name;
  o << " " << macro_prefix << macro->name() << "(";

  DCHECK_EQ(macro->signature().types().size(), macro->parameter_names().size());
  auto type_iterator = macro->signature().types().begin();
  bool first = true;
  for (const std::string& name : macro->parameter_names()) {
    if (!first) {
      o << ", ";
    }
    const Value* parameter = declarations()->LookupValue(name);
    const Type* parameter_type = *type_iterator;
    const std::string& generated_type_name =
        parameter_type->GetGeneratedTypeName();
    o << generated_type_name << " " << parameter->GetValueForDeclaration();
    type_iterator++;
    first = false;
  }

  for (const LabelDeclaration& label_info : macro->signature().labels) {
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
      o << generated_type_name << " " << var->GetValueForDeclaration();
    }
  }

  o << ")";
}

VisitResult ImplementationVisitor::GenerateOperation(
    const std::string& operation, Arguments arguments,
    base::Optional<const Type*> return_type) {
  TypeVector parameter_types(arguments.parameters.GetTypeVector());

  auto i = global_context_.op_handlers_.find(operation);
  if (i != global_context_.op_handlers_.end()) {
    for (auto handler : i->second) {
      if (GetTypeOracle().IsCompatibleSignature(handler.parameter_types,
                                                parameter_types)) {
        // Operators used in a bit context can also be function calls that never
        // return but have a True and False label
        if (!return_type && handler.result_type->IsNever()) {
          if (arguments.labels.size() == 0) {
            Label* true_label = declarations()->LookupLabel(kTrueLabelName);
            arguments.labels.push_back(true_label);
            Label* false_label = declarations()->LookupLabel(kFalseLabelName);
            arguments.labels.push_back(false_label);
          }
        }

        if (!return_type || (GetTypeOracle().IsAssignableFrom(
                                *return_type, handler.result_type))) {
          return GenerateCall(handler.macro_name, arguments, false);
        }
      }
    }
  }
  std::stringstream s;
  s << "cannot find implementation of operation \"" << operation
    << "\" with types " << parameter_types;
  ReportError(s.str());
  return VisitResult(GetTypeOracle().GetVoidType(), "");
}

void ImplementationVisitor::GenerateChangedVarsFromControlSplit(AstNode* node) {
  const std::set<const Variable*>& changed_vars =
      global_context_.GetControlSplitChangedVariables(
          node, declarations()->GetCurrentSpecializationTypeNamesVector());
  source_out() << "{";
  bool first = true;
  for (auto v : changed_vars) {
    if (v->type()->IsConstexpr()) continue;
    if (first) {
      first = false;
    } else {
      source_out() << ", ";
    }
    source_out() << v->GetValueForDeclaration();
  }
  source_out() << "}";
}

const Type* ImplementationVisitor::GetCommonType(const Type* left,
                                                 const Type* right) {
  const Type* common_type = GetTypeOracle().GetVoidType();
  if (GetTypeOracle().IsAssignableFrom(left, right)) {
    common_type = left;
  } else if (GetTypeOracle().IsAssignableFrom(right, left)) {
    common_type = right;
  } else {
    std::stringstream s;
    s << "illegal combination of types " << left << " and " << right;
    ReportError(s.str());
  }
  return common_type;
}

VisitResult ImplementationVisitor::GenerateCopy(const VisitResult& to_copy) {
  std::string temp = GenerateNewTempVariable(to_copy.type());
  source_out() << to_copy.variable() << ";" << std::endl;
  GenerateIndent();
  source_out() << "USE(" << temp << ");" << std::endl;
  return VisitResult(to_copy.type(), temp);
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

void ImplementationVisitor::GenerateAssignToVariable(Variable* var,
                                                     VisitResult value) {
  VisitResult casted_value = GenerateImplicitConvert(var->type(), value);
  GenerateIndent();
  source_out() << var->GetValueForWrite() << " = " << casted_value.variable()
               << ";" << std::endl;
  var->Define();
}

void ImplementationVisitor::GenerateAssignToLocation(
    LocationExpression* location, const LocationReference& reference,
    VisitResult assignment_value) {
  if (IdentifierExpression::cast(location)) {
    Value* value = reference.value;
    if (value->IsConst()) {
      std::stringstream s;
      s << "\"" << value->name()
        << "\" is declared const (maybe implicitly) and cannot be assigned to";
      ReportError(s.str());
    }
    Variable* var = Variable::cast(value);
    GenerateAssignToVariable(var, assignment_value);
  } else if (auto access = FieldAccessExpression::cast(location)) {
    GenerateOperation(std::string(".") + access->field + "=",
                      {{reference.base, assignment_value}, {}});
  } else {
    DCHECK_NOT_NULL(ElementAccessExpression::cast(location));
    GenerateOperation(
        "[]=", {{reference.base, reference.index, assignment_value}, {}});
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
    variable = declarations()->DeclareVariable(name, *type);
    // Because the variable is being defined during code generation, it must be
    // assumed that it changes along all control split paths because it's no
    // longer possible to run the control-flow anlaysis in the declaration pass
    // over the variable.
    global_context_.MarkVariableChanged(
        node, declarations()->GetCurrentSpecializationTypeNamesVector(),
        variable);
  }

  GenerateIndent();
  if (variable->type()->IsConstexpr()) {
    source_out() << variable->type()->GetGeneratedTypeName();
    source_out() << " " << variable->GetValueForDeclaration() << "_impl;"
                 << std::endl;
  } else {
    source_out() << "TVARIABLE(";
    source_out() << variable->type()->GetGeneratedTNodeTypeName();
    source_out() << ", " << variable->GetValueForDeclaration() << "_impl);"
                 << std::endl;
  }
  GenerateIndent();
  source_out() << "auto " << variable->GetValueForDeclaration() << " = &"
               << variable->GetValueForDeclaration() << "_impl;" << std::endl;
  GenerateIndent();
  source_out() << "USE(" << variable->GetValueForDeclaration() << ");"
               << std::endl;
  if (initialization) {
    GenerateAssignToVariable(variable, *initialization);
  }
  return variable;
}

void ImplementationVisitor::GenerateParameter(
    const std::string& parameter_name) {
  const Value* val = declarations()->LookupValue(parameter_name);
  std::string var = val->GetValueForDeclaration();
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
           << callee_result.type();
    ReportError(stream.str());
  }
  const FunctionPointerType* type =
      FunctionPointerType::cast(callee_result.type());

  std::vector<std::string> variables;
  for (size_t current = 0; current < arguments.parameters.size(); ++current) {
    const Type* to_type = type->parameter_types()[current];
    VisitResult result =
        GenerateImplicitConvert(to_type, arguments.parameters[current]);
    variables.push_back(result.variable());
  }

  std::string result_variable_name;
  bool no_result = type->return_type()->IsVoidOrNever() || is_tailcall;
  if (no_result) {
    GenerateIndent();
  } else {
    result_variable_name = GenerateNewTempVariable(type->return_type());
    source_out() << "UncheckedCast<";
    source_out() << type->return_type()->GetGeneratedTNodeTypeName();
    source_out() << ">(";
  }

  Builtin* example_builtin =
      declarations()->FindSomeInternalBuiltinWithType(type);

  if (is_tailcall) {
    source_out() << "TailCallStub(";
  } else {
    source_out() << "CallStub(";
  }
  source_out() << "Builtins::CallableFor(isolate(), Builtins::k"
               << example_builtin->name() << ").descriptor(), "
               << callee_result.variable() << ", ";

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
    const std::string& callable_name, const Arguments& arguments,
    bool is_tailcall) {
  TypeVector parameter_types(arguments.parameters.GetTypeVector());
  Callable* callable = LookupCall(callable_name, parameter_types);
  const Type* result_type = callable->signature().return_type;

  std::vector<std::string> variables;
  for (size_t current = 0; current < arguments.parameters.size(); ++current) {
    const Type* to_type = (current >= callable->signature().types().size())
                              ? GetTypeOracle().GetObjectType()
                              : callable->signature().types()[current];
    VisitResult result =
        GenerateImplicitConvert(to_type, arguments.parameters[current]);
    variables.push_back(result.variable());
  }

  std::string result_variable_name;
  if (result_type->IsVoidOrNever() || is_tailcall) {
    GenerateIndent();
  } else {
    result_variable_name = GenerateNewTempVariable(result_type);
    if (!result_type->IsConstexpr()) {
      source_out() << "UncheckedCast<";
      source_out() << result_type->GetGeneratedTNodeTypeName();
      source_out() << ">(";
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
        s << "mismatch of label parameters (expected " << t << " got "
          << label->GetParameter(j)->type() << " for parameter "
          << std::to_string(i + 1) << ")";
        ReportError(s.str());
      }
      j++;
      source_out() << variable->GetValueForDeclaration();
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
  Visit(decl->callable, {}, decl->body);
}

void ImplementationVisitor::Visit(SpecializationDeclaration* decl) {
  Generic* generic = declarations()->LookupGeneric(decl->name);
  TypeVector specialization_types = GetTypeVector(decl->generic_parameters);
  CallableNode* callable = generic->declaration()->callable;
  SpecializeGeneric({{generic, specialization_types},
                     callable,
                     decl->signature.get(),
                     decl->body});
}

VisitResult ImplementationVisitor::Visit(CallExpression* expr,
                                         bool is_tailcall) {
  Arguments arguments;
  std::string name = expr->callee.name;
  bool has_template_arguments = expr->callee.generic_arguments.size() != 0;
  if (has_template_arguments) {
    Generic* generic = declarations()->LookupGeneric(expr->callee.name);
    TypeVector specialization_types =
        GetTypeVector(expr->callee.generic_arguments);
    name = GetGeneratedCallableName(name, specialization_types);
    CallableNode* callable = generic->declaration()->callable;
    QueueGenericSpecialization({generic, specialization_types}, callable,
                               callable->signature.get(),
                               generic->declaration()->body);
  }
  for (Expression* arg : expr->arguments)
    arguments.parameters.push_back(Visit(arg));
  arguments.labels = LabelsFromIdentifiers(expr->labels);
  if (expr->is_operator) {
    if (is_tailcall) {
      std::stringstream s;
      s << "can't tail call an operator";
      ReportError(s.str());
    }
    return GenerateOperation(name, arguments);
  }
  VisitResult result;
  if (!has_template_arguments &&
      declarations()->Lookup(expr->callee.name)->IsValue()) {
    result = GeneratePointerCall(&expr->callee, arguments, is_tailcall);
  } else {
    result = GenerateCall(name, arguments, is_tailcall);
  }
  if (!result.type()->IsVoidOrNever()) {
    GenerateIndent();
    source_out() << "USE(" << result.variable() << ");" << std::endl;
  }
  if (is_tailcall) {
    result = {GetTypeOracle().GetNeverType(), ""};
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
  source_out() << "Branch(" << condition.variable() << ", "
               << true_label->generated() << ", " << false_label->generated()
               << ");" << std::endl;
}

bool ImplementationVisitor::GenerateExpressionBranch(
    Expression* expression, const std::vector<Label*>& statement_labels,
    const std::vector<Statement*>& statement_blocks, Label* merge_label) {
  // Activate a new scope to define True/False catch labels
  Declarations::NodeScopeActivator scope(declarations(), expression);

  VisitResult expression_result = Visit(expression);
  if (expression_result.type() == GetTypeOracle().GetBoolType()) {
    GenerateBranch(expression_result, statement_labels[0], statement_labels[1]);
  } else {
    if (expression_result.type() != GetTypeOracle().GetNeverType()) {
      std::stringstream s;
      s << "unexpected return type " << expression_result.type()
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
  if (GetTypeOracle().IsImplicitlyConverableFrom(destination_type,
                                                 source.type())) {
    VisitResult result(source.type(), source.variable());
    Arguments args;
    args.parameters = {result};
    return GenerateOperation("convert<>", args, destination_type);
  } else if (GetTypeOracle().IsAssignableFrom(destination_type,
                                              source.type())) {
    return VisitResult(destination_type, source.variable());
  } else {
    std::stringstream s;
    s << "cannot use expression of type " << source.type()
      << " as a value of type " << destination_type;
    ReportError(s.str());
  }
  return VisitResult(GetTypeOracle().GetVoidType(), "");
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
