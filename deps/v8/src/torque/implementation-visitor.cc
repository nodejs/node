// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "src/torque/csa-generator.h"
#include "src/torque/implementation-visitor.h"
#include "src/torque/parameter-difference.h"

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
      UNREACHABLE();
  }
}

const Type* ImplementationVisitor::Visit(Statement* stmt) {
  CurrentSourcePosition::Scope scope(stmt->pos);
  const Type* result;
  switch (stmt->kind) {
#define ENUM_ITEM(name)               \
  case AstNode::Kind::k##name:        \
    result = Visit(name::cast(stmt)); \
    break;
    AST_STATEMENT_NODE_KIND_LIST(ENUM_ITEM)
#undef ENUM_ITEM
    default:
      UNREACHABLE();
  }
  DCHECK_EQ(result == TypeOracle::GetNeverType(),
            assembler().CurrentBlockIsComplete());
  return result;
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
    source << "#include \"src/torque-assembler.h\"";
  } else {
    source << "#include \"src/builtins/builtins-" +
                  DashifyString(module->name()) + "-gen.h\"";
  }
  source << "\n";
  source << "#include \"src/objects/arguments.h\"\n";
  source << "#include \"src/builtins/builtins-utils-gen.h\"\n";
  source << "#include \"src/builtins/builtins.h\"\n";
  source << "#include \"src/code-factory.h\"\n";
  source << "#include \"src/elements-kind.h\"\n";
  source << "#include \"src/heap/factory-inl.h\"\n";
  source << "#include \"src/objects.h\"\n";
  source << "#include \"src/objects/bigint.h\"\n";

  source << "#include \"builtins-" + DashifyString(module->name()) +
                "-from-dsl-gen.h\"\n\n";

  source << "namespace v8 {\n"
         << "namespace internal {\n"
         << "\n"
         << "using Node = compiler::Node;\n"
         << "\n";

  std::string upper_name(module->name());
  transform(upper_name.begin(), upper_name.end(), upper_name.begin(),
            ::toupper);
  std::string headerDefine =
      std::string("V8_TORQUE_") + upper_name + "_FROM_DSL_BASE_H__";
  header << "#ifndef " << headerDefine << "\n";
  header << "#define " << headerDefine << "\n\n";
  if (module->IsDefault()) {
    header << "#include \"src/torque-assembler.h\"";
  } else {
    header << "#include \"src/builtins/builtins-" +
                  DashifyString(module->name()) + "-gen.h\"\n";
  }
  header << "\n\n ";

  header << "namespace v8 {\n"
         << "namespace internal {\n"
         << "\n";

  header << "class " << GetDSLAssemblerName(module) << ": public "
         << GetBaseAssemblerName(module) << " {\n";
  header << " public:\n";
  header << "  explicit " << GetDSLAssemblerName(module)
         << "(compiler::CodeAssemblerState* state) : "
         << GetBaseAssemblerName(module) << "(state) {}\n";

  header << "\n";
  header << "  using Node = compiler::Node;\n";
  header << "  template <class T>\n";
  header << "  using TNode = compiler::TNode<T>;\n";
  header << "  template <class T>\n";
  header << "  using SloppyTNode = compiler::SloppyTNode<T>;\n\n";
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

  source << "}  // namespace internal\n"
         << "}  // namespace v8\n"
         << "\n";

  header << "};\n\n";
  header << "}  // namespace internal\n"
         << "}  // namespace v8\n"
         << "\n";
  header << "#endif  // " << headerDefine << "\n";
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

  assembler_ = CfgAssembler(Stack<const Type*>{});

  VisitResult expression_result = Visit(decl->expression);
  VisitResult return_result =
      GenerateImplicitConvert(signature.return_type, expression_result);

  CSAGenerator csa_generator{assembler().Result(), source_out()};
  Stack<std::string> values = *csa_generator.EmitGraph(Stack<std::string>{});

  assembler_ = base::nullopt;

  source_out() << "return ";
  CSAGenerator::EmitCSAValue(return_result, values, source_out());
  source_out() << ";\n";
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
  header_out() << "\n    std::tuple<";
  bool first = true;
  for (const Type* type : LowerType(struct_type)) {
    if (!first) {
      header_out() << ", ";
    }
    first = false;
    header_out() << type->GetGeneratedTypeName();
  }
  header_out() << "> Flatten() const {\n"
               << "      return std::tuple_cat(";
  first = true;
  for (auto& field : struct_type->fields()) {
    if (!first) {
      header_out() << ", ";
    }
    first = false;
    if (field.type->IsStructType()) {
      header_out() << field.name << ".Flatten()";
    } else {
      header_out() << "std::make_tuple(" << field.name << ")";
    }
  }
  header_out() << ");\n";
  header_out() << "    }\n";
  header_out() << "  };\n";
}

void ImplementationVisitor::Visit(TorqueMacroDeclaration* decl,
                                  const Signature& sig, Statement* body) {
  Signature signature = MakeSignature(decl->signature.get());
  const Type* return_type = signature.return_type;
  bool can_return = return_type != TypeOracle::GetNeverType();
  bool has_return_value =
      can_return && return_type != TypeOracle::GetVoidType();
  std::string name = GetGeneratedCallableName(
      decl->name, declarations()->GetCurrentSpecializationTypeNamesVector());
  const TypeVector& list = signature.types();
  Macro* macro = declarations()->LookupMacro(name, list);

  CurrentCallableActivator activator(global_context_, macro, decl);

  if (body != nullptr) {
    header_out() << "  ";
    GenerateMacroFunctionDeclaration(header_out(), "", macro);
    header_out() << ";\n";

    GenerateMacroFunctionDeclaration(
        source_out(), GetDSLAssemblerName(CurrentModule()) + "::", macro);
    source_out() << " {\n";

    Stack<std::string> lowered_parameters;
    Stack<const Type*> lowered_parameter_types;

    for (const std::string& name : macro->parameter_names()) {
      Parameter* parameter = Parameter::cast(declarations()->LookupValue(name));
      const Type* type = parameter->type();
      if (type->IsConstexpr()) {
        parameter->set_value(
            VisitResult(parameter->type(), parameter->external_name()));
      } else {
        LowerParameter(type, parameter->external_name(), &lowered_parameters);
        StackRange range = lowered_parameter_types.PushMany(LowerType(type));
        parameter->set_value(VisitResult(type, range));
      }
    }

    DCHECK_EQ(lowered_parameters.Size(), lowered_parameter_types.Size());
    assembler_ = CfgAssembler(lowered_parameter_types);

    for (const LabelDeclaration& label_info : sig.labels) {
      Label* label = declarations()->LookupLabel(label_info.name);
      Stack<const Type*> label_input_stack;
      for (Variable* v : label->GetParameters()) {
        label_input_stack.PushMany(LowerType(v->type()));
      }
      CreateBlockForLabel(label, label_input_stack);
    }

    Label* macro_end = declarations()->DeclareLabel("macro_end");
    if (can_return) {
      Stack<const Type*> result_stack;
      CreateBlockForLabel(macro_end,
                          Stack<const Type*>{LowerType(signature.return_type)});
    }

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
    if (!result->IsNever()) {
      GenerateLabelGoto(macro_end);
    }

    for (const LabelDeclaration& label_info : sig.labels) {
      Label* label = declarations()->LookupLabel(label_info.name);
      GenerateLabelBind(label);
      std::vector<std::string> label_parameter_variables;
      for (size_t i = 0; i < label->GetParameterCount(); ++i) {
        label_parameter_variables.push_back(
            ExternalLabelParameterName(label, i));
      }
      assembler().Emit(GotoExternalInstruction{label->external_label_name(),
                                               label_parameter_variables});
    }

    if (macro->HasReturns() || !result->IsNever()) {
      GenerateLabelBind(macro_end);
    }

    CSAGenerator csa_generator{assembler().Result(), source_out()};
    base::Optional<Stack<std::string>> values =
        csa_generator.EmitGraph(lowered_parameters);

    assembler_ = base::nullopt;

    if (has_return_value) {
      source_out() << "  return ";
      CSAGenerator::EmitCSAValue(GetAndClearReturnValue(), *values,
                                 source_out());
      source_out() << ";\n";
    }
    source_out() << "}\n\n";
  }
}

namespace {
std::string AddParameter(Value* parameter, size_t i,
                         Stack<std::string>* parameters,
                         Stack<const Type*>* parameter_types) {
  std::string name = "parameter" + std::to_string(i);
  parameters->Push(name);
  StackRange range = parameter_types->PushMany(LowerType(parameter->type()));
  parameter->set_value(VisitResult(parameter->type(), range));
  return name;
}
}  // namespace

void ImplementationVisitor::Visit(TorqueBuiltinDeclaration* decl,
                                  const Signature& signature, Statement* body) {
  std::string name = GetGeneratedCallableName(
      decl->name, declarations()->GetCurrentSpecializationTypeNamesVector());
  source_out() << "TF_BUILTIN(" << name << ", "
               << GetDSLAssemblerName(CurrentModule()) << ") {\n";
  Builtin* builtin = declarations()->LookupBuiltin(name);
  CurrentCallableActivator activator(global_context_, builtin, decl);

  Stack<const Type*> parameter_types;
  Stack<std::string> parameters;

  // Context
  Value* val =
      declarations()->LookupValue(decl->signature->parameters.names[0]);
  std::string parameter0 = AddParameter(val, 0, &parameters, &parameter_types);
  source_out() << "  TNode<Context> " << parameter0
               << " = UncheckedCast<Context>(Parameter("
               << "Descriptor::kContext));\n";
  source_out() << "  USE(" << parameter0 << ");\n";

  size_t first = 1;
  if (builtin->IsVarArgsJavaScript()) {
    assert(decl->signature->parameters.has_varargs);
    ExternConstant* arguments =
        ExternConstant::cast(declarations()->LookupValue(
            decl->signature->parameters.arguments_variable));
    std::string arguments_name = arguments->value().constexpr_value();
    source_out()
        << "  Node* argc = Parameter(Descriptor::kJSActualArgumentsCount);\n";
    source_out() << "  CodeStubArguments arguments_impl(this, "
                    "ChangeInt32ToIntPtr(argc));\n";
    Value* receiver =
        declarations()->LookupValue(decl->signature->parameters.names[1]);
    std::string parameter1 =
        AddParameter(receiver, 1, &parameters, &parameter_types);

    source_out() << "  TNode<Object> " << parameter1
                 << " = arguments_impl.GetReceiver();\n";
    source_out() << "auto " << CSAGenerator::ARGUMENTS_VARIABLE_STRING
                 << " = &arguments_impl;\n";
    source_out() << "USE(arguments);\n";
    source_out() << "USE(" << parameter1 << ");\n";
    first = 2;
  }

  for (size_t i = 0; i < decl->signature->parameters.names.size(); ++i) {
    if (i < first) continue;
    const std::string& parameter_name = decl->signature->parameters.names[i];
    Value* parameter = declarations()->LookupValue(parameter_name);
    std::string var = AddParameter(parameter, i, &parameters, &parameter_types);
    source_out() << "  " << parameter->type()->GetGeneratedTypeName() << " "
                 << var << " = "
                 << "UncheckedCast<"
                 << parameter->type()->GetGeneratedTNodeTypeName()
                 << ">(Parameter(Descriptor::k"
                 << CamelifyString(parameter_name) << "));\n";
    source_out() << "  USE(" << var << ");\n";
  }

  assembler_ = CfgAssembler(parameter_types);
  const Type* body_result = Visit(body);
  if (body_result != TypeOracle::GetNeverType()) {
    ReportError("control reaches end of builtin, expected return of a value");
  }
  CSAGenerator csa_generator{assembler().Result(), source_out(),
                             builtin->kind()};
  csa_generator.EmitGraph(parameters);
  assembler_ = base::nullopt;
  source_out() << "}\n\n";
}

const Type* ImplementationVisitor::Visit(VarDeclarationStatement* stmt) {
  base::Optional<const Type*> type;
  if (stmt->type) type = declarations()->GetType(*stmt->type);
  base::Optional<VisitResult> init_result;
  if (stmt->initializer) {
    StackScope scope(this);
    init_result = Visit(*stmt->initializer);
    if (type) {
      init_result = GenerateImplicitConvert(*type, *init_result);
    }
    init_result = scope.Yield(*init_result);
  } else {
    DCHECK(type.has_value());
    if ((*type)->IsConstexpr()) {
      ReportError("constexpr variables need an initializer");
    }
    TypeVector lowered_types = LowerType(*type);
    for (const Type* type : lowered_types) {
      assembler().Emit(PushUninitializedInstruction{type});
    }
    init_result =
        VisitResult(*type, assembler().TopRange(lowered_types.size()));
  }
  Variable* var;
  if (stmt->const_qualified) {
    var = declarations()->DeclareVariable(stmt->name, init_result->type(),
                                          stmt->const_qualified);
  } else {
    var = Variable::cast(declarations()->LookupValue(stmt->name));
  }
  var->set_value(*init_result);
  return TypeOracle::GetVoidType();
}

const Type* ImplementationVisitor::Visit(TailCallStatement* stmt) {
  return Visit(stmt->call, true).type();
}

VisitResult ImplementationVisitor::Visit(ConditionalExpression* expr) {
  Label* true_label;
  Label* false_label;
  Block* done_block = assembler().NewBlock();
  Block* true_conversion_block = assembler().NewBlock();
  {
    Declarations::NodeScopeActivator scope(declarations(), expr->condition);

    true_label = declarations()->LookupLabel(kTrueLabelName);
    CreateBlockForLabel(true_label, assembler().CurrentStack());
    false_label = declarations()->LookupLabel(kFalseLabelName);
    CreateBlockForLabel(false_label, assembler().CurrentStack());
    done_block = assembler().NewBlock();

    {
      StackScope condition_scope(this);
      VisitResult condition_result = Visit(expr->condition);
      if (!condition_result.type()->IsNever()) {
        condition_result = condition_scope.Yield(GenerateImplicitConvert(
            TypeOracle::GetBoolType(), condition_result));
        assembler().Branch(true_label->block(), false_label->block());
      }
    }
  }

  VisitResult left;
  VisitResult right;

  {
    // The code for both paths of the conditional need to be generated first
    // before evaluating the conditional expression because the common type of
    // the result of both the true and false of the condition needs to be known
    // to convert both branches to a common type.
    assembler().Bind(true_label->block());
    StackScope left_scope(this);
    left = Visit(expr->if_true);
    assembler().Goto(true_conversion_block);

    const Type* common_type;
    {
      assembler().Bind(false_label->block());
      StackScope right_scope(this);
      right = Visit(expr->if_false);
      common_type = GetCommonType(left.type(), right.type());
      right = right_scope.Yield(GenerateImplicitConvert(common_type, right));
      assembler().Goto(done_block);
    }

    assembler().Bind(true_conversion_block);
    left = left_scope.Yield(GenerateImplicitConvert(common_type, left));
    assembler().Goto(done_block);
  }

  assembler().Bind(done_block);
  CHECK_EQ(left, right);
  return left;
}

VisitResult ImplementationVisitor::Visit(LogicalOrExpression* expr) {
  VisitResult left_result;
  {
    Declarations::NodeScopeActivator scope(declarations(), expr->left);
    Label* false_label = declarations()->LookupLabel(kFalseLabelName);
    CreateBlockForLabel(false_label, assembler().CurrentStack());
    left_result = Visit(expr->left);
    if (left_result.type()->IsBool()) {
      Label* true_label = declarations()->LookupLabel(kTrueLabelName);
      assembler().Branch(true_label->block(), false_label->block());
      assembler().Bind(false_label->block());
    } else if (left_result.type()->IsNever()) {
      assembler().Bind(false_label->block());
    } else if (!left_result.type()->IsConstexprBool()) {
      ReportError(
          "expected type bool, constexpr bool, or never on left-hand side of "
          "operator ||");
    }
  }

  if (left_result.type()->IsConstexprBool()) {
    VisitResult right_result = Visit(expr->right);
    if (!right_result.type()->IsConstexprBool()) {
      ReportError(
          "expected type constexpr bool on right-hand side of operator "
          "||");
    }
    return VisitResult(TypeOracle::GetConstexprBoolType(),
                       std::string("(") + left_result.constexpr_value() +
                           " || " + right_result.constexpr_value() + ")");
  }

  VisitResult right_result = Visit(expr->right);
  if (right_result.type()->IsBool()) {
    Label* true_label = declarations()->LookupLabel(kTrueLabelName);
    Label* false_label = declarations()->LookupLabel(kFalseLabelName);
    assembler().Branch(true_label->block(), false_label->block());
    return VisitResult::NeverResult();
  } else if (!right_result.type()->IsNever()) {
    ReportError(
        "expected type bool or never on right-hand side of operator ||");
  }
  return right_result;
}

VisitResult ImplementationVisitor::Visit(LogicalAndExpression* expr) {
  VisitResult left_result;
  {
    Declarations::NodeScopeActivator scope(declarations(), expr->left);
    Label* true_label = declarations()->LookupLabel(kTrueLabelName);
    CreateBlockForLabel(true_label, assembler().CurrentStack());
    left_result = Visit(expr->left);
    if (left_result.type()->IsBool()) {
      Label* false_label = declarations()->LookupLabel(kFalseLabelName);
      assembler().Branch(true_label->block(), false_label->block());
      assembler().Bind(true_label->block());
    } else if (left_result.type()->IsNever()) {
      assembler().Bind(true_label->block());
    } else if (!left_result.type()->IsConstexprBool()) {
      ReportError(
          "expected type bool, constexpr bool, or never on left-hand side of "
          "operator &&");
    }
  }

  if (left_result.type()->IsConstexprBool()) {
    VisitResult right_result = Visit(expr->right);
    if (!right_result.type()->IsConstexprBool()) {
      ReportError(
          "expected type constexpr bool on right-hand side of operator "
          "&&");
    }
    return VisitResult(TypeOracle::GetConstexprBoolType(),
                       std::string("(") + left_result.constexpr_value() +
                           " && " + right_result.constexpr_value() + ")");
  }

  VisitResult right_result = Visit(expr->right);
  if (right_result.type()->IsBool()) {
    Label* true_label = declarations()->LookupLabel(kTrueLabelName);
    Label* false_label = declarations()->LookupLabel(kFalseLabelName);
    assembler().Branch(true_label->block(), false_label->block());
    return VisitResult::NeverResult();
  } else if (!right_result.type()->IsNever()) {
    ReportError(
        "expected type bool or never on right-hand side of operator &&");
  }
  return right_result;
}

VisitResult ImplementationVisitor::Visit(IncrementDecrementExpression* expr) {
  StackScope scope(this);
  LocationReference location_ref = GetLocationReference(expr->location);
  VisitResult current_value = GenerateFetchFromLocation(location_ref);
  VisitResult one = {TypeOracle::GetConstInt31Type(), "1"};
  Arguments args;
  args.parameters = {current_value, one};
  VisitResult assignment_value = GenerateCall(
      expr->op == IncrementDecrementOperator::kIncrement ? "+" : "-", args);
  GenerateAssignToLocation(location_ref, assignment_value);
  return scope.Yield(expr->postfix ? current_value : assignment_value);
}

VisitResult ImplementationVisitor::Visit(AssignmentExpression* expr) {
  StackScope scope(this);
  LocationReference location_ref = GetLocationReference(expr->location);
  VisitResult assignment_value;
  if (expr->op) {
    VisitResult location_value = GenerateFetchFromLocation(location_ref);
    assignment_value = Visit(expr->value);
    Arguments args;
    args.parameters = {location_value, assignment_value};
    assignment_value = GenerateCall(*expr->op, args);
    GenerateAssignToLocation(location_ref, assignment_value);
  } else {
    assignment_value = Visit(expr->value);
    GenerateAssignToLocation(location_ref, assignment_value);
  }
  return scope.Yield(assignment_value);
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
  return VisitResult{result_type, expr->number};
}

VisitResult ImplementationVisitor::Visit(AssumeTypeImpossibleExpression* expr) {
  VisitResult result = Visit(expr->expression);
  const Type* result_type =
      SubtractType(result.type(), declarations()->GetType(expr->excluded_type));
  if (result_type->IsNever()) {
    ReportError("unreachable code");
  }
  CHECK_EQ(LowerType(result_type), TypeVector{result_type});
  assembler().Emit(UnsafeCastInstruction{result_type});
  result.SetType(result_type);
  return result;
}

VisitResult ImplementationVisitor::Visit(StringLiteralExpression* expr) {
  return VisitResult{
      TypeOracle::GetConstStringType(),
      "\"" + expr->literal.substr(1, expr->literal.size() - 2) + "\""};
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
  assembler().Emit(PushCodePointerInstruction{builtin->name(), type});
  return VisitResult(type, assembler().TopRange(1));
}

VisitResult ImplementationVisitor::Visit(IdentifierExpression* expr) {
  StackScope scope(this);
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
    return scope.Yield(GetBuiltinCode(builtin));
  }

  return scope.Yield(GenerateFetchFromLocation(GetLocationReference(expr)));
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
  StackRange arguments = assembler().TopRange(0);
  for (Expression* e : stmt->arguments) {
    StackScope scope(this);
    VisitResult result = Visit(e);
    Variable* var = label->GetParameter(i++);
    result = GenerateImplicitConvert(var->type(), result);
    arguments.Extend(scope.Yield(result).stack_range());
  }

  GenerateLabelGoto(label, arguments);
  label->MarkUsed();
  return TypeOracle::GetNeverType();
}

const Type* ImplementationVisitor::Visit(IfStatement* stmt) {
  bool has_else = stmt->if_false.has_value();

  if (stmt->is_constexpr) {
    VisitResult expression_result = Visit(stmt->condition);

    if (!(expression_result.type() == TypeOracle::GetConstexprBoolType())) {
      std::stringstream stream;
      stream << "expression should return type constexpr bool "
             << "but returns type " << *expression_result.type();
      ReportError(stream.str());
    }

    Block* true_block = assembler().NewBlock();
    Block* false_block = assembler().NewBlock();
    Block* done_block = assembler().NewBlock();

    assembler().Emit(ConstexprBranchInstruction{
        expression_result.constexpr_value(), true_block, false_block});

    assembler().Bind(true_block);
    const Type* left_result;
    {
      StackScope stack_scope(this);
      left_result = Visit(stmt->if_true);
    }
    if (left_result == TypeOracle::GetVoidType()) {
      assembler().Goto(done_block);
    }

    assembler().Bind(false_block);
    const Type* right_result = TypeOracle::GetVoidType();
    if (has_else) {
      StackScope stack_scope(this);
      right_result = Visit(*stmt->if_false);
    }
    if (right_result == TypeOracle::GetVoidType()) {
      assembler().Goto(done_block);
    }

    if (left_result->IsNever() != right_result->IsNever()) {
      std::stringstream stream;
      stream << "either both or neither branches in a constexpr if statement "
                "must reach their end at"
             << PositionAsString(stmt->pos);
      ReportError(stream.str());
    }

    if (left_result != TypeOracle::GetNeverType()) {
      assembler().Bind(done_block);
    }
    return left_result;
  } else {
    Label* true_label = nullptr;
    Label* false_label = nullptr;
    {
      Declarations::NodeScopeActivator scope(declarations(), &*stmt->condition);
      true_label = declarations()->LookupLabel(kTrueLabelName);
      CreateBlockForLabel(true_label, assembler().CurrentStack());
      false_label = declarations()->LookupLabel(kFalseLabelName);
      CreateBlockForLabel(false_label, assembler().CurrentStack());
    }

    Block* done_block;
    bool live = false;
    if (has_else) {
      done_block = assembler().NewBlock();
    } else {
      done_block = false_label->block();
      live = true;
    }
    std::vector<Statement*> blocks = {stmt->if_true};
    std::vector<Label*> labels = {true_label, false_label};
    if (has_else) blocks.push_back(*stmt->if_false);
    if (GenerateExpressionBranch(stmt->condition, labels, blocks, done_block)) {
      live = true;
    }
    if (live) {
      assembler().Bind(done_block);
    }
    return live ? TypeOracle::GetVoidType() : TypeOracle::GetNeverType();
  }
}

const Type* ImplementationVisitor::Visit(WhileStatement* stmt) {
  Label* body_label = nullptr;
  Label* exit_label = nullptr;
  {
    Declarations::NodeScopeActivator scope(declarations(), stmt->condition);
    body_label = declarations()->LookupLabel(kTrueLabelName);
    CreateBlockForLabel(body_label, assembler().CurrentStack());
    exit_label = declarations()->LookupLabel(kFalseLabelName);
    CreateBlockForLabel(exit_label, assembler().CurrentStack());
  }

  Block* header_block = assembler().NewBlock();
  assembler().Goto(header_block);

  assembler().Bind(header_block);

  Declarations::NodeScopeActivator scope(declarations(), stmt->body);
  BreakContinueActivator activator(global_context_, exit_label->block(),
                                   header_block);

  GenerateExpressionBranch(stmt->condition, {body_label, exit_label},
                           {stmt->body}, header_block);

  GenerateLabelBind(exit_label);
  return TypeOracle::GetVoidType();
}

const Type* ImplementationVisitor::Visit(BlockStatement* block) {
  Declarations::NodeScopeActivator scope(declarations(), block);
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
  assembler().Emit(PrintConstantStringInstruction{"halting because of '" +
                                                  stmt->reason + "' at " +
                                                  PositionAsString(stmt->pos)});
#endif
  assembler().Emit(DebugBreakInstruction{stmt->never_continues});
  if (stmt->never_continues) {
    return TypeOracle::GetNeverType();
  } else {
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
    CreateBlockForLabel(true_label, assembler().CurrentStack());
    false_label = declarations()->LookupLabel(kFalseLabelName);
    CreateBlockForLabel(false_label, assembler().CurrentStack());

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
    assembler().Emit(PrintConstantStringInstruction{
        "assert '" + FormatAssertSource(stmt->source) + "' failed at " +
        PositionAsString(stmt->pos)});
    assembler().Emit(DebugBreakInstruction{true});

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
      if (return_result.IsOnStack()) {
        StackRange return_value_range =
            GenerateLabelGoto(end, return_result.stack_range());
        SetReturnValue(VisitResult(return_result.type(), return_value_range));
      } else {
        GenerateLabelGoto(end);
        SetReturnValue(return_result);
      }
    } else if (current_callable->IsBuiltin()) {
      assembler().Emit(ReturnInstruction{});
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
  StackScope stack_scope(this);

  VisitResult expression_result = Visit(stmt->iterable);
  VisitResult begin = stmt->begin
                          ? Visit(*stmt->begin)
                          : VisitResult(TypeOracle::GetConstInt31Type(), "0");

  VisitResult end = stmt->end
                        ? Visit(*stmt->end)
                        : GenerateCall(".length", {{expression_result}, {}});

  const Type* common_type = GetCommonType(begin.type(), end.type());
  VisitResult index = GenerateImplicitConvert(common_type, begin);

  Block* body_block = assembler().NewBlock();
  Block* increment_block = assembler().NewBlock(assembler().CurrentStack());
  Block* exit_block = assembler().NewBlock(assembler().CurrentStack());

  Block* header_block = assembler().NewBlock();

  assembler().Goto(header_block);

  assembler().Bind(header_block);

  BreakContinueActivator activator(global_context_, exit_block,
                                   increment_block);

  {
    StackScope comparison_scope(this);
    VisitResult result = GenerateCall("<", {{index, end}, {}});
    if (result.type() != TypeOracle::GetBoolType()) {
      ReportError("operator < with arguments(", *index.type(), ", ",
                  *end.type(),
                  ")  used in for-of loop has to return type bool, but "
                  "returned type ",
                  *result.type());
    }
    comparison_scope.Yield(result);
  }
  assembler().Branch(body_block, exit_block);

  assembler().Bind(body_block);
  {
    StackScope body_scope(this);

    VisitResult element_result;
    {
      StackScope element_scope(this);
      VisitResult result = GenerateCall("[]", {{expression_result, index}, {}});
      if (stmt->var_declaration->type) {
        const Type* declared_type =
            declarations()->GetType(*stmt->var_declaration->type);
        result = GenerateImplicitConvert(declared_type, result);
      }
      element_result = element_scope.Yield(result);
    }
    Variable* element_var = Variable::cast(
        declarations()->LookupValue(stmt->var_declaration->name));
    element_var->set_value(element_result);
    Visit(stmt->body);
  }
  assembler().Goto(increment_block);

  assembler().Bind(increment_block);
  {
    Arguments increment_args;
    increment_args.parameters = {index, {TypeOracle::GetConstInt31Type(), "1"}};
    VisitResult increment_result = GenerateCall("+", increment_args);

    GenerateAssignToLocation(LocationReference::VariableAccess(index),
                             increment_result);
  }

  assembler().Goto(header_block);

  assembler().Bind(exit_block);
  return TypeOracle::GetVoidType();
}

VisitResult ImplementationVisitor::Visit(TryLabelExpression* expr) {
  Block* done_block = assembler().NewBlock();
  VisitResult try_result;
  Label* label = nullptr;

  // Output labels for the goto handlers and for the merge after the try.
  {
    // Activate a new scope to see handler labels
    Declarations::NodeScopeActivator scope(declarations(), expr);
    {
      LabelBlock* block = expr->label_block;
      CurrentSourcePosition::Scope source_position(block->pos);
      label = declarations()->LookupLabel(block->label);

      Declarations::NodeScopeActivator scope(declarations(), block->body);
      Stack<const Type*> label_input_stack = assembler().CurrentStack();
      for (Variable* v : label->GetParameters()) {
        StackRange range = label_input_stack.PushMany(LowerType(v->type()));
        v->set_value(VisitResult(v->type(), range));
        v->Define();
      }
      CreateBlockForLabel(label, label_input_stack);
    }

    // Visit try
    {
      StackScope stack_scope(this);
      try_result = Visit(expr->try_expression);
      if (try_result.type() != TypeOracle::GetNeverType()) {
        try_result = stack_scope.Yield(try_result);
        assembler().Goto(done_block);
      }
    }
  }

  if (label->IsUsed()) {
    // Visit and output the code for the label block. If the label block falls
    // through, then the try must not return a value. Also, if the try doesn't
    // fall through, but the label does, then overall the try-label block
    // returns type void.
    GenerateLabelBind(label);
    const Type* label_result;
    {
      StackScope stack_scope(this);
      label_result = Visit(expr->label_block->body);
    }
    if (!try_result.type()->IsVoidOrNever() && label_result->IsVoid()) {
      ReportError(
          "otherwise clauses cannot fall through in a non-void expression");
    }
    if (label_result != TypeOracle::GetNeverType()) {
      assembler().Goto(done_block);
    }
    if (label_result->IsVoid() && try_result.type()->IsNever()) {
      try_result =
          VisitResult(TypeOracle::GetVoidType(), try_result.stack_range());
    }
  }

  if (!try_result.type()->IsNever()) {
    assembler().Bind(done_block);
  }
  return try_result;
}

VisitResult ImplementationVisitor::Visit(StatementExpression* expr) {
  return VisitResult{Visit(expr->statement), assembler().TopRange(0)};
}

const Type* ImplementationVisitor::Visit(BreakStatement* stmt) {
  Block* break_block = global_context_.GetCurrentBreak();
  if (break_block == nullptr) {
    ReportError("break used outside of loop");
  }
  assembler().Goto(break_block);
  return TypeOracle::GetNeverType();
}

const Type* ImplementationVisitor::Visit(ContinueStatement* stmt) {
  Block* continue_block = global_context_.GetCurrentContinue();
  if (continue_block == nullptr) {
    ReportError("continue used outside of loop");
  }
  assembler().Goto(continue_block);
  return TypeOracle::GetNeverType();
}

const Type* ImplementationVisitor::Visit(ForLoopStatement* stmt) {
  Declarations::NodeScopeActivator scope(declarations(), stmt);
  StackScope stack_scope(this);

  if (stmt->var_declaration) Visit(*stmt->var_declaration);

  Label* body_label = declarations()->LookupLabel(kTrueLabelName);
  CreateBlockForLabel(body_label, assembler().CurrentStack());
  Label* exit_label = declarations()->LookupLabel(kFalseLabelName);
  CreateBlockForLabel(exit_label, assembler().CurrentStack());

  Block* header_block = assembler().NewBlock();
  assembler().Goto(header_block);
  assembler().Bind(header_block);

  // The continue label is where "continue" statements jump to. If no action
  // expression is provided, we jump directly to the header.
  Block* continue_block = header_block;

  // The action label is only needed when an action expression was provided.
  Block* action_block = nullptr;
  if (stmt->action) {
    action_block = assembler().NewBlock();

    // The action expression needs to be executed on a continue.
    continue_block = action_block;
  }

  BreakContinueActivator activator(global_context_, exit_label->block(),
                                   continue_block);

  std::vector<Label*> labels = {body_label, exit_label};
  bool generate_action = true;
  if (stmt->test) {
    generate_action = GenerateExpressionBranch(*stmt->test, labels,
                                               {stmt->body}, continue_block);
  } else {
    GenerateLabelGoto(body_label);
    generate_action =
        GenerateLabeledStatementBlocks({stmt->body}, labels, continue_block);
  }

  if (generate_action && stmt->action) {
    assembler().Bind(action_block);
    Visit(*stmt->action);
    assembler().Goto(header_block);
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
    return "TorqueAssembler";
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

void ImplementationVisitor::GenerateMacroFunctionDeclaration(
    std::ostream& o, const std::string& macro_prefix, Macro* macro) {
  GenerateFunctionDeclaration(o, macro_prefix, macro->name(),
                              macro->signature(), macro->parameter_names());
}

void ImplementationVisitor::GenerateFunctionDeclaration(
    std::ostream& o, const std::string& macro_prefix, const std::string& name,
    const Signature& signature, const NameVector& parameter_names) {
  if (global_context_.verbose()) {
    std::cout << "generating source for declaration " << name << "\n";
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
    const Parameter* parameter =
        Parameter::cast(declarations()->LookupValue(name));
    const Type* parameter_type = *type_iterator;
    const std::string& generated_type_name =
        parameter_type->GetGeneratedTypeName();
    o << generated_type_name << " " << parameter->external_name();
    type_iterator++;
    first = false;
  }

  for (const LabelDeclaration& label_info : signature.labels) {
    Label* label = declarations()->LookupLabel(label_info.name);
    if (!first) {
      o << ", ";
    }
    o << "Label* " << label->external_label_name();
    size_t i = 0;
    for (Variable* var : label->GetParameters()) {
      std::string generated_type_name("TVariable<");
      generated_type_name += var->type()->GetGeneratedTNodeTypeName();
      generated_type_name += ">*";
      o << ", ";
      o << generated_type_name << " " << ExternalLabelParameterName(label, i);
      ++i;
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

Callable* ImplementationVisitor::LookupCall(
    const std::string& name, const Arguments& arguments,
    const TypeVector& specialization_types) {
  Callable* result = nullptr;
  TypeVector parameter_types(arguments.parameters.GetTypeVector());
  bool has_template_arguments = !specialization_types.empty();
  std::string mangled_name = name;
  if (has_template_arguments) {
    mangled_name = GetGeneratedCallableName(name, specialization_types);
  }
  Declarable* declarable = declarations()->Lookup(mangled_name);
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

  if (has_template_arguments) {
    Generic* generic = *result->generic();
    CallableNode* callable = generic->declaration()->callable;
    if (generic->declaration()->body) {
      QueueGenericSpecialization({generic, specialization_types}, callable,
                                 callable->signature.get(),
                                 generic->declaration()->body);
    }
  }

  return result;
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
  if (to_copy.IsOnStack()) {
    return VisitResult(to_copy.type(),
                       assembler().Peek(to_copy.stack_range(), to_copy.type()));
  }
  return to_copy;
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
  StackRange stack_range = assembler().TopRange(0);
  for (size_t i = 0; i < struct_type->fields().size(); ++i) {
    const NameAndType& field = struct_type->fields()[i];
    StackScope scope(this);
    VisitResult value = Visit(decl->expressions[i]);
    value = GenerateImplicitConvert(field.type, value);
    stack_range.Extend(scope.Yield(value).stack_range());
  }
  return VisitResult(struct_type, stack_range);
}

LocationReference ImplementationVisitor::GetLocationReference(
    Expression* location) {
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
      return LocationReference::Temporary(Visit(location), "expression");
  }
}

LocationReference ImplementationVisitor::GetLocationReference(
    FieldAccessExpression* expr) {
  LocationReference reference = GetLocationReference(expr->object);
  if (reference.IsVariableAccess() &&
      reference.variable().type()->IsStructType()) {
    return LocationReference::VariableAccess(
        ProjectStructField(reference.variable(), expr->field));
  }
  if (reference.IsTemporary() && reference.temporary().type()->IsStructType()) {
    return LocationReference::Temporary(
        ProjectStructField(reference.temporary(), expr->field),
        reference.temporary_description());
  }
  return LocationReference::FieldAccess(GenerateFetchFromLocation(reference),
                                        expr->field);
}

LocationReference ImplementationVisitor::GetLocationReference(
    ElementAccessExpression* expr) {
  VisitResult array = Visit(expr->array);
  VisitResult index = Visit(expr->index);
  return LocationReference::ArrayAccess(array, index);
}

LocationReference ImplementationVisitor::GetLocationReference(
    IdentifierExpression* expr) {
  Value* value = declarations()->LookupValue(expr->name);
  if (auto* constant = ModuleConstant::DynamicCast(value)) {
    if (constant->type()->IsConstexpr()) {
      return LocationReference::Temporary(
          VisitResult(constant->type(), constant->constant_name() + "()"),
          "module constant " + expr->name);
    }
    assembler().Emit(ModuleConstantInstruction{constant});
    StackRange stack_range =
        assembler().TopRange(LoweredSlotCount(constant->type()));
    return LocationReference::Temporary(
        VisitResult(constant->type(), stack_range),
        "module constant " + expr->name);
  }
  if (value->IsConst()) {
    return LocationReference::Temporary(value->value(),
                                        "constant value " + expr->name);
  }
  DCHECK(value->IsVariable());
  return LocationReference::VariableAccess(value->value());
}

VisitResult ImplementationVisitor::GenerateFetchFromLocation(
    const LocationReference& reference) {
  if (reference.IsTemporary()) {
    return GenerateCopy(reference.temporary());
  } else if (reference.IsVariableAccess()) {
    return GenerateCopy(reference.variable());
  } else {
    DCHECK(reference.IsCallAccess());
    return GenerateCall(reference.eval_function(),
                        Arguments{reference.call_arguments(), {}});
  }
}

void ImplementationVisitor::GenerateAssignToLocation(
    const LocationReference& reference, const VisitResult& assignment_value) {
  if (reference.IsCallAccess()) {
    Arguments arguments{reference.call_arguments(), {}};
    arguments.parameters.push_back(assignment_value);
    GenerateCall(reference.assign_function(), arguments);
  } else if (reference.IsVariableAccess()) {
    VisitResult variable = reference.variable();
    VisitResult converted_value =
        GenerateImplicitConvert(variable.type(), assignment_value);
    assembler().Poke(variable.stack_range(), converted_value.stack_range(),
                     variable.type());
  } else {
    DCHECK(reference.IsTemporary());
    ReportError("cannot assign to ", reference.temporary_description());
  }
}

VisitResult ImplementationVisitor::GeneratePointerCall(
    Expression* callee, const Arguments& arguments, bool is_tailcall) {
  StackScope scope(this);
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

  callee_result = GenerateCopy(callee_result);
  StackRange arg_range = assembler().TopRange(0);
  for (size_t current = 0; current < arguments.parameters.size(); ++current) {
    const Type* to_type = type->parameter_types()[current];
    arg_range.Extend(
        GenerateImplicitConvert(to_type, arguments.parameters[current])
            .stack_range());
  }

  Builtin* example_builtin =
      declarations()->FindSomeInternalBuiltinWithType(type);
  if (!example_builtin) {
    std::stringstream stream;
    stream << "unable to find any builtin with type \"" << *type << "\"";
    ReportError(stream.str());
  }

  assembler().Emit(CallBuiltinPointerInstruction{is_tailcall, example_builtin,
                                                 arg_range.Size()});

  if (is_tailcall) {
    return VisitResult::NeverResult();
  }
  DCHECK_EQ(1, LoweredSlotCount(type->return_type()));
  return scope.Yield(VisitResult(type->return_type(), assembler().TopRange(1)));
}

VisitResult ImplementationVisitor::GenerateCall(
    const std::string& callable_name, Arguments arguments,
    const TypeVector& specialization_types, bool is_tailcall) {
  Callable* callable =
      LookupCall(callable_name, arguments, specialization_types);

  // Operators used in a branching context can also be function calls that never
  // return but have a True and False label
  if (arguments.labels.size() == 0 &&
      callable->signature().labels.size() == 2) {
    Label* true_label = declarations()->LookupLabel(kTrueLabelName);
    arguments.labels.push_back(true_label);
    Label* false_label = declarations()->LookupLabel(kFalseLabelName);
    arguments.labels.push_back(false_label);
  }

  const Type* return_type = callable->signature().return_type;

  std::vector<VisitResult> converted_arguments;
  StackRange argument_range = assembler().TopRange(0);
  std::vector<std::string> constexpr_arguments;
  for (size_t current = 0; current < arguments.parameters.size(); ++current) {
    const Type* to_type = (current >= callable->signature().types().size())
                              ? TypeOracle::GetObjectType()
                              : callable->signature().types()[current];
    VisitResult converted =
        GenerateImplicitConvert(to_type, arguments.parameters[current]);
    converted_arguments.push_back(converted);
    if (converted.IsOnStack()) {
      argument_range.Extend(converted.stack_range());
    } else {
      constexpr_arguments.push_back(converted.constexpr_value());
    }
  }

  if (global_context_.verbose()) {
    std::cout << "generating code for call to " << callable_name << "\n";
  }

  size_t label_count = callable->signature().labels.size();
  if (label_count != arguments.labels.size()) {
    std::stringstream s;
    s << "unexpected number of otherwise labels for " << callable->name()
      << " (expected " << std::to_string(label_count) << " found "
      << std::to_string(arguments.labels.size()) << ")";
    ReportError(s.str());
  }

  if (auto* builtin = Builtin::DynamicCast(callable)) {
    assembler().Emit(
        CallBuiltinInstruction{is_tailcall, builtin, argument_range.Size()});
    if (is_tailcall) {
      return VisitResult::NeverResult();
    } else {
      size_t slot_count = LoweredSlotCount(return_type);
      DCHECK_LE(slot_count, 1);
      // TODO(tebbi): Actually, builtins have to return a value, so we should
      // assert slot_count == 1 here.
      return VisitResult(return_type, assembler().TopRange(slot_count));
    }
  } else if (auto* macro = Macro::DynamicCast(callable)) {
    if (is_tailcall) {
      ReportError("can't tail call a macro");
    }
    if (return_type->IsConstexpr()) {
      DCHECK_EQ(0, arguments.labels.size());
      std::stringstream result;
      result << "(" << macro->name() << "(";
      bool first = true;
      for (VisitResult arg : arguments.parameters) {
        DCHECK(!arg.IsOnStack());
        if (!first) {
          result << ", ";
        }
        first = false;
        result << arg.constexpr_value();
      }
      result << "))";
      return VisitResult(return_type, result.str());
    } else if (arguments.labels.empty() &&
               return_type != TypeOracle::GetNeverType()) {
      assembler().Emit(CallCsaMacroInstruction{macro, constexpr_arguments});
      size_t return_slot_count = LoweredSlotCount(return_type);
      return VisitResult(return_type, assembler().TopRange(return_slot_count));
    } else {
      base::Optional<Block*> return_continuation;
      if (return_type != TypeOracle::GetNeverType()) {
        return_continuation = assembler().NewBlock();
      }

      std::vector<Block*> label_blocks;

      for (size_t i = 0; i < label_count; ++i) {
        label_blocks.push_back(assembler().NewBlock());
      }

      assembler().Emit(CallCsaMacroAndBranchInstruction{
          macro, constexpr_arguments, return_continuation, label_blocks});

      for (size_t i = 0; i < label_count; ++i) {
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
        assembler().Bind(label_blocks[i]);
        assembler().Goto(
            label->block(),
            LowerParameterTypes(callable->signature().labels[i].types).size());

        size_t j = 0;
        for (auto t : callable->signature().labels[i].types) {
          Variable* variable = label->GetParameter(j);
          if (!(variable->type() == t)) {
            std::stringstream s;
            s << "mismatch of label parameters (expected " << *t << " got "
              << *label->GetParameter(j)->type() << " for parameter "
              << std::to_string(i + 1) << ")";
            ReportError(s.str());
          }
          j++;
        }
        label->MarkUsed();
      }

      if (return_continuation) {
        assembler().Bind(*return_continuation);
        size_t return_slot_count = LoweredSlotCount(return_type);
        return VisitResult(return_type,
                           assembler().TopRange(return_slot_count));
      } else {
        return VisitResult::NeverResult();
      }
    }
  } else if (auto* runtime_function = RuntimeFunction::DynamicCast(callable)) {
    assembler().Emit(CallRuntimeInstruction{is_tailcall, runtime_function,
                                            argument_range.Size()});
    if (is_tailcall) {
      return VisitResult::NeverResult();
    } else {
      size_t slot_count = LoweredSlotCount(return_type);
      DCHECK_LE(slot_count, 1);
      // TODO(tebbi): Actually, runtime functions have to return a value, so
      // we should assert slot_count == 1 here.
      return VisitResult(return_type, assembler().TopRange(slot_count));
    }
  } else {
    UNREACHABLE();
  }
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
  StackScope scope(this);
  Arguments arguments;
  std::string name = expr->callee.name;
  TypeVector specialization_types =
      GetTypeVector(expr->callee.generic_arguments);
  bool has_template_arguments = !specialization_types.empty();
  for (Expression* arg : expr->arguments)
    arguments.parameters.push_back(Visit(arg));
  arguments.labels = LabelsFromIdentifiers(expr->labels);
  VisitResult result;
  if (!has_template_arguments &&
      declarations()->Lookup(expr->callee.name)->IsValue()) {
    return scope.Yield(
        GeneratePointerCall(&expr->callee, arguments, is_tailcall));
  } else {
    return scope.Yield(
        GenerateCall(name, arguments, specialization_types, is_tailcall));
  }
}

bool ImplementationVisitor::GenerateLabeledStatementBlocks(
    const std::vector<Statement*>& blocks,
    const std::vector<Label*>& statement_labels, Block* merge_block) {
  bool live = false;
  auto label_iterator = statement_labels.begin();
  for (Statement* block : blocks) {
    GenerateLabelBind(*label_iterator++);
    const Type* stmt_result;
    {
      StackScope stack_scope(this);
      stmt_result = Visit(block);
    }
    if (stmt_result != TypeOracle::GetNeverType()) {
      assembler().Goto(merge_block);
      live = true;
    }
  }
  return live;
}

void ImplementationVisitor::GenerateBranch(const VisitResult& condition,
                                           Label* true_label,
                                           Label* false_label) {
  DCHECK_EQ(condition,
            VisitResult(TypeOracle::GetBoolType(), assembler().TopRange(1)));
  assembler().Branch(true_label->block(), false_label->block());
}

bool ImplementationVisitor::GenerateExpressionBranch(
    Expression* expression, const std::vector<Label*>& statement_labels,
    const std::vector<Statement*>& statement_blocks, Block* merge_block) {
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
                                        merge_block);
}

VisitResult ImplementationVisitor::GenerateImplicitConvert(
    const Type* destination_type, VisitResult source) {
  StackScope scope(this);
  if (source.type() == TypeOracle::GetNeverType()) {
    ReportError("it is not allowed to use a value of type never");
  }

  if (destination_type == source.type()) {
    return scope.Yield(GenerateCopy(source));
  }

  if (TypeOracle::IsImplicitlyConvertableFrom(destination_type,
                                              source.type())) {
    std::string name =
        GetGeneratedCallableName(kFromConstexprMacroName, {destination_type});
    return scope.Yield(GenerateCall(name, {{source}, {}}, {}, false));
  } else if (IsAssignableFrom(destination_type, source.type())) {
    source.SetType(destination_type);
    return scope.Yield(GenerateCopy(source));
  } else {
    std::stringstream s;
    s << "cannot use expression of type " << *source.type()
      << " as a value of type " << *destination_type;
    ReportError(s.str());
  }
}

void ImplementationVisitor::CreateBlockForLabel(Label* label,
                                                Stack<const Type*> stack) {
  label->set_block(assembler().NewBlock(std::move(stack), label->IsDeferred()));
}

void ImplementationVisitor::GenerateLabelBind(Label* label) {
  assembler().Bind(label->block());
}

StackRange ImplementationVisitor::GenerateLabelGoto(
    Label* label, base::Optional<StackRange> arguments) {
  return assembler().Goto(label->block(), arguments ? arguments->Size() : 0);
}

std::vector<Label*> ImplementationVisitor::LabelsFromIdentifiers(
    const std::vector<std::string>& names) {
  std::vector<Label*> result;
  result.reserve(names.size());
  for (const auto& name : names) {
    result.push_back(declarations()->LookupLabel(name));
  }
  return result;
}

StackRange ImplementationVisitor::LowerParameter(
    const Type* type, const std::string& parameter_name,
    Stack<std::string>* lowered_parameters) {
  if (type->IsStructType()) {
    const StructType* struct_type = StructType::cast(type);
    StackRange range = lowered_parameters->TopRange(0);
    for (auto& field : struct_type->fields()) {
      StackRange parameter_range = LowerParameter(
          field.type, parameter_name + "." + field.name, lowered_parameters);
      range.Extend(parameter_range);
    }
    return range;
  } else {
    lowered_parameters->Push(parameter_name);
    return lowered_parameters->TopRange(1);
  }
}

std::string ImplementationVisitor::ExternalLabelParameterName(Label* label,
                                                              size_t i) {
  return label->external_label_name() + "_parameter_" + std::to_string(i);
}

}  // namespace torque
}  // namespace internal
}  // namespace v8
