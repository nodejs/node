// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "src/torque/csa-generator.h"
#include "src/torque/declaration-visitor.h"
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
  StackScope stack_scope(this);
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

void ImplementationVisitor::BeginNamespaceFile(Namespace* nspace) {
  std::ostream& source = nspace->source_stream();
  std::ostream& header = nspace->header_stream();

  for (const std::string& include_path : GlobalContext::CppIncludes()) {
    source << "#include " << StringLiteralQuote(include_path) << "\n";
  }

  for (Namespace* n : GlobalContext::Get().GetNamespaces()) {
    source << "#include \"torque-generated/builtins-" +
                  DashifyString(n->name()) + "-from-dsl-gen.h\"\n";
  }
  source << "\n";

  source << "namespace v8 {\n"
         << "namespace internal {\n"
         << "\n";

  std::string upper_name(nspace->name());
  transform(upper_name.begin(), upper_name.end(), upper_name.begin(),
            ::toupper);
  std::string headerDefine =
      std::string("V8_TORQUE_") + upper_name + "_FROM_DSL_BASE_H__";
  header << "#ifndef " << headerDefine << "\n";
  header << "#define " << headerDefine << "\n\n";
  header << "#include \"src/compiler/code-assembler.h\"\n";
  if (nspace != GlobalContext::GetDefaultNamespace()) {
    header << "#include \"src/code-stub-assembler.h\"\n";
  }
  header << "\n";

  header << "namespace v8 {\n"
         << "namespace internal {\n"
         << "\n";

  header << "class " << nspace->ExternalName() << " {\n";
  header << " public:\n";
  header << "  explicit " << nspace->ExternalName()
         << "(compiler::CodeAssemblerState* state) : state_(state), ca_(state) "
            "{ USE(state_, ca_); }\n";
}

void ImplementationVisitor::EndNamespaceFile(Namespace* nspace) {
  std::ostream& source = nspace->source_stream();
  std::ostream& header = nspace->header_stream();

  std::string upper_name(nspace->name());
  transform(upper_name.begin(), upper_name.end(), upper_name.begin(),
            ::toupper);
  std::string headerDefine =
      std::string("V8_TORQUE_") + upper_name + "_FROM_DSL_BASE_H__";

  source << "}  // namespace internal\n"
         << "}  // namespace v8\n"
         << "\n";

  header << " private:\n"
         << "  compiler::CodeAssemblerState* const state_;\n"
         << "  compiler::CodeAssembler ca_;"
         << "}; \n\n";
  header << "}  // namespace internal\n"
         << "}  // namespace v8\n"
         << "\n";
  header << "#endif  // " << headerDefine << "\n";
}

void ImplementationVisitor::Visit(NamespaceConstant* decl) {
  Signature signature{{}, base::nullopt, {{}, false}, 0, decl->type(), {}};
  const std::string& name = decl->name();

  BindingsManagersScope bindings_managers_scope;

  header_out() << "  ";
  GenerateFunctionDeclaration(header_out(), "", name, signature, {});
  header_out() << ";\n";

  GenerateFunctionDeclaration(source_out(),
                              CurrentNamespace()->ExternalName() + "::", name,
                              signature, {});
  source_out() << " {\n";

  DCHECK(!signature.return_type->IsVoidOrNever());

  assembler_ = CfgAssembler(Stack<const Type*>{});

  VisitResult expression_result = Visit(decl->body());
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

void ImplementationVisitor::Visit(TypeAlias* alias) {
  if (alias->IsRedeclaration()) return;
  const StructType* struct_type = StructType::DynamicCast(alias->type());
  if (!struct_type) return;
  const std::string& name = struct_type->name();
  header_out() << "  struct " << name << " {\n";
  for (auto& field : struct_type->fields()) {
    header_out() << "    " << field.name_and_type.type->GetGeneratedTypeName();
    header_out() << " " << field.name_and_type.name << ";\n";
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
    if (field.name_and_type.type->IsStructType()) {
      header_out() << field.name_and_type.name << ".Flatten()";
    } else {
      header_out() << "std::make_tuple(" << field.name_and_type.name << ")";
    }
  }
  header_out() << ");\n";
  header_out() << "    }\n";
  header_out() << "  };\n";
}

VisitResult ImplementationVisitor::InlineMacro(
    Macro* macro, base::Optional<LocationReference> this_reference,
    const std::vector<VisitResult>& arguments,
    const std::vector<Block*> label_blocks) {
  CurrentScope::Scope current_scope(macro);
  BindingsManagersScope bindings_managers_scope;
  CurrentCallable::Scope current_callable(macro);
  CurrentReturnValue::Scope current_return_value;
  const Signature& signature = macro->signature();
  const Type* return_type = macro->signature().return_type;
  bool can_return = return_type != TypeOracle::GetNeverType();

  CurrentConstructorInfo::Scope current_constructor;
  if (macro->IsConstructor())
    CurrentConstructorInfo::Get() = ConstructorInfo{0};

  BlockBindings<LocalValue> parameter_bindings(&ValueBindingsManager::Get());
  BlockBindings<LocalLabel> label_bindings(&LabelBindingsManager::Get());
  DCHECK_EQ(macro->signature().parameter_names.size(),
            arguments.size() + (this_reference ? 1 : 0));
  DCHECK_EQ(this_reference.has_value(), macro->IsMethod());

  // Bind the this for methods. Methods that modify a struct-type "this" must
  // only be called if the this is in a variable, in which case the
  // LocalValue is non-const. Otherwise, the LocalValue used for the parameter
  // binding is const, and thus read-only, which will cause errors if
  // modified, e.g. when called by a struct method that sets the structs
  // fields. This prevents using temporary struct values for anything other
  // than read operations.
  if (this_reference) {
    DCHECK(macro->IsMethod());
    LocalValue this_value = LocalValue{!this_reference->IsVariableAccess(),
                                       this_reference->GetVisitResult()};
    parameter_bindings.Add(kThisParameterName, this_value);
  }

  size_t i = 0;
  for (auto arg : arguments) {
    if (this_reference && i == signature.implicit_count) i++;
    const std::string& name = macro->parameter_names()[i++];
    parameter_bindings.Add(name, LocalValue{true, arg});
  }

  DCHECK_EQ(label_blocks.size(), signature.labels.size());
  for (size_t i = 0; i < signature.labels.size(); ++i) {
    const LabelDeclaration& label_info = signature.labels[i];
    label_bindings.Add(label_info.name,
                       LocalLabel{label_blocks[i], label_info.types});
  }

  Block* macro_end;
  base::Optional<Binding<LocalLabel>> macro_end_binding;
  if (can_return) {
    Stack<const Type*> stack = assembler().CurrentStack();
    std::vector<const Type*> lowered_return_types = LowerType(return_type);
    stack.PushMany(lowered_return_types);
    if (!return_type->IsConstexpr()) {
      SetReturnValue(VisitResult(return_type,
                                 stack.TopRange(lowered_return_types.size())));
    }
    // The stack copy used to initialize the _macro_end block is only used
    // as a template for the actual gotos generated by return statements. It
    // doesn't correspond to any real return values, and thus shouldn't contain
    // top types, because these would pollute actual return value types that get
    // unioned with them for return statements, erroneously forcing them to top.
    for (auto i = stack.begin(); i != stack.end(); ++i) {
      if ((*i)->IsTopType()) {
        *i = TopType::cast(*i)->source_type();
      }
    }
    macro_end = assembler().NewBlock(std::move(stack));
    macro_end_binding.emplace(&LabelBindingsManager::Get(), "_macro_end",
                              LocalLabel{macro_end, {return_type}});
  } else {
    SetReturnValue(VisitResult::NeverResult());
  }

  const Type* result = Visit(*macro->body());

  if (result->IsNever()) {
    if (!return_type->IsNever() && !macro->HasReturns()) {
      std::stringstream s;
      s << "macro " << macro->ReadableName()
        << " that never returns must have return type never";
      ReportError(s.str());
    }
  } else {
    if (return_type->IsNever()) {
      std::stringstream s;
      s << "macro " << macro->ReadableName()
        << " has implicit return at end of its declartion but return type "
           "never";
      ReportError(s.str());
    } else if (!macro->signature().return_type->IsVoid()) {
      std::stringstream s;
      s << "macro " << macro->ReadableName()
        << " expects to return a value but doesn't on all paths";
      ReportError(s.str());
    }
  }
  if (!result->IsNever()) {
    assembler().Goto(macro_end);
  }

  if (macro->HasReturns() || !result->IsNever()) {
    assembler().Bind(macro_end);
  }

  return GetAndClearReturnValue();
}

void ImplementationVisitor::VisitMacroCommon(Macro* macro) {
  // Do not generate code for inlined macros.
  if (macro->ShouldBeInlined()) {
    return;
  }

  CurrentCallable::Scope current_callable(macro);
  const Signature& signature = macro->signature();
  const Type* return_type = macro->signature().return_type;
  bool can_return = return_type != TypeOracle::GetNeverType();
  bool has_return_value =
      can_return && return_type != TypeOracle::GetVoidType();

  // Struct methods should never generate code, they should always be inlined
  DCHECK(!macro->IsMethod() ||
         Method::cast(macro)->aggregate_type()->IsClassType());

  header_out() << "  ";
  GenerateMacroFunctionDeclaration(header_out(), "", macro);
  header_out() << ";\n";

  GenerateMacroFunctionDeclaration(
      source_out(), CurrentNamespace()->ExternalName() + "::", macro);
  source_out() << " {\n";

  Stack<std::string> lowered_parameters;
  Stack<const Type*> lowered_parameter_types;

  std::vector<VisitResult> arguments;

  base::Optional<LocationReference> this_reference;
  if (Method* method = Method::DynamicCast(macro)) {
    const Type* this_type = method->aggregate_type();
    DCHECK(this_type->IsClassType());
    lowered_parameter_types.Push(this_type);
    lowered_parameters.Push(ExternalParameterName(kThisParameterName));
    VisitResult this_result =
        VisitResult(this_type, lowered_parameters.TopRange(1));
    // Mark the this as a temporary to prevent assignment to it.
    this_reference =
        LocationReference::Temporary(this_result, "this parameter");
  }

  for (size_t i = 0; i < macro->signature().parameter_names.size(); ++i) {
    if (this_reference && i == macro->signature().implicit_count) continue;
    const std::string& name = macro->parameter_names()[i];
    std::string external_name = ExternalParameterName(name);
    const Type* type = macro->signature().types()[i];

    if (type->IsConstexpr()) {
      arguments.push_back(VisitResult(type, external_name));
    } else {
      LowerParameter(type, external_name, &lowered_parameters);
      StackRange range = lowered_parameter_types.PushMany(LowerType(type));
      arguments.push_back(VisitResult(type, range));
    }
  }

  DCHECK_EQ(lowered_parameters.Size(), lowered_parameter_types.Size());
  assembler_ = CfgAssembler(lowered_parameter_types);

  std::vector<Block*> label_blocks;
  for (const LabelDeclaration& label_info : signature.labels) {
    Stack<const Type*> label_input_stack;
    for (const Type* type : label_info.types) {
      label_input_stack.PushMany(LowerType(type));
    }
    Block* block = assembler().NewBlock(std::move(label_input_stack));
    label_blocks.push_back(block);
  }

  VisitResult return_value =
      InlineMacro(macro, this_reference, arguments, label_blocks);
  Block* end = assembler().NewBlock();
  if (return_type != TypeOracle::GetNeverType()) {
    assembler().Goto(end);
  }

  for (size_t i = 0; i < label_blocks.size(); ++i) {
    Block* label_block = label_blocks[i];
    const LabelDeclaration& label_info = signature.labels[i];
    assembler().Bind(label_block);
    std::vector<std::string> label_parameter_variables;
    for (size_t i = 0; i < label_info.types.size(); ++i) {
      label_parameter_variables.push_back(
          ExternalLabelParameterName(label_info.name, i));
    }
    assembler().Emit(GotoExternalInstruction{ExternalLabelName(label_info.name),
                                             label_parameter_variables});
  }

  if (return_type != TypeOracle::GetNeverType()) {
    assembler().Bind(end);
  }

  CSAGenerator csa_generator{assembler().Result(), source_out()};
  base::Optional<Stack<std::string>> values =
      csa_generator.EmitGraph(lowered_parameters);

  assembler_ = base::nullopt;

  if (has_return_value) {
    source_out() << "  return ";
    CSAGenerator::EmitCSAValue(return_value, *values, source_out());
    source_out() << ";\n";
  }
  source_out() << "}\n\n";
}

void ImplementationVisitor::Visit(Macro* macro) {
  if (macro->IsExternal()) return;
  VisitMacroCommon(macro);
}

void ImplementationVisitor::Visit(Method* method) {
  DCHECK(!method->IsExternal());
  VisitMacroCommon(method);
}

namespace {

std::string AddParameter(size_t i, Builtin* builtin,
                         Stack<std::string>* parameters,
                         Stack<const Type*>* parameter_types,
                         BlockBindings<LocalValue>* parameter_bindings) {
  const std::string& name = builtin->signature().parameter_names[i];
  const Type* type = builtin->signature().types()[i];
  std::string external_name = "parameter" + std::to_string(i);
  parameters->Push(external_name);
  StackRange range = parameter_types->PushMany(LowerType(type));
  parameter_bindings->Add(name, LocalValue{true, VisitResult(type, range)});
  return external_name;
}

}  // namespace

void ImplementationVisitor::Visit(Builtin* builtin) {
  if (builtin->IsExternal()) return;
  CurrentScope::Scope current_scope(builtin);
  const std::string& name = builtin->ExternalName();
  const Signature& signature = builtin->signature();
  source_out() << "TF_BUILTIN(" << name << ", CodeStubAssembler) {\n"
               << "  compiler::CodeAssemblerState* state_ = state();"
               << "  compiler::CodeAssembler ca_(state());\n";

  CurrentCallable::Scope current_callable(builtin);
  CurrentReturnValue::Scope current_return_value;

  Stack<const Type*> parameter_types;
  Stack<std::string> parameters;

  BindingsManagersScope bindings_managers_scope;

  BlockBindings<LocalValue> parameter_bindings(&ValueBindingsManager::Get());

  // Context
  std::string parameter0 = AddParameter(0, builtin, &parameters,
                                        &parameter_types, &parameter_bindings);
  source_out() << "  TNode<Context> " << parameter0
               << " = UncheckedCast<Context>(Parameter("
               << "Descriptor::kContext));\n";
  source_out() << "  USE(" << parameter0 << ");\n";

  size_t first = 1;
  if (builtin->IsVarArgsJavaScript()) {
    DCHECK(signature.parameter_types.var_args);
    source_out()
        << "  Node* argc = Parameter(Descriptor::kJSActualArgumentsCount);\n";
    source_out() << "  CodeStubArguments arguments_impl(this, "
                    "ChangeInt32ToIntPtr(argc));\n";
    std::string parameter1 = AddParameter(
        1, builtin, &parameters, &parameter_types, &parameter_bindings);

    source_out() << "  TNode<Object> " << parameter1
                 << " = arguments_impl.GetReceiver();\n";
    source_out() << "auto " << CSAGenerator::ARGUMENTS_VARIABLE_STRING
                 << " = &arguments_impl;\n";
    source_out() << "USE(arguments);\n";
    source_out() << "USE(" << parameter1 << ");\n";
    parameter_bindings.Add(
        *signature.arguments_variable,
        LocalValue{true,
                   VisitResult(TypeOracle::GetArgumentsType(), "arguments")});
    first = 2;
  }

  for (size_t i = 0; i < signature.parameter_names.size(); ++i) {
    if (i < first) continue;
    const std::string& parameter_name = signature.parameter_names[i];
    const Type* type = signature.types()[i];
    std::string var = AddParameter(i, builtin, &parameters, &parameter_types,
                                   &parameter_bindings);
    source_out() << "  " << type->GetGeneratedTypeName() << " " << var << " = "
                 << "UncheckedCast<" << type->GetGeneratedTNodeTypeName()
                 << ">(Parameter(Descriptor::k"
                 << CamelifyString(parameter_name) << "));\n";
    source_out() << "  USE(" << var << ");\n";
  }

  assembler_ = CfgAssembler(parameter_types);
  const Type* body_result = Visit(*builtin->body());
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
  BlockBindings<LocalValue> block_bindings(&ValueBindingsManager::Get());
  return Visit(stmt, &block_bindings);
}

const Type* ImplementationVisitor::Visit(
    VarDeclarationStatement* stmt, BlockBindings<LocalValue>* block_bindings) {
  if (!stmt->const_qualified && !stmt->type) {
    ReportError(
        "variable declaration is missing type. Only 'const' bindings can "
        "infer the type.");
  }
  // const qualified variables are required to be initialized properly.
  if (stmt->const_qualified && !stmt->initializer) {
    ReportError("local constant \"", stmt->name, "\" is not initialized.");
  }

  base::Optional<const Type*> type;
  if (stmt->type) {
    type = Declarations::GetType(*stmt->type);
    if ((*type)->IsConstexpr() && !stmt->const_qualified) {
      ReportError(
          "cannot declare variable with constexpr type. Use 'const' instead.");
    }
  }
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
      assembler().Emit(PushUninitializedInstruction{TypeOracle::GetTopType(
          "unitialized variable '" + stmt->name + "' of type " +
              type->ToString() + " originally defined at " +
              PositionAsString(stmt->pos),
          type)});
    }
    init_result =
        VisitResult(*type, assembler().TopRange(lowered_types.size()));
  }
  block_bindings->Add(stmt->name,
                      LocalValue{stmt->const_qualified, *init_result});
  return TypeOracle::GetVoidType();
}

const Type* ImplementationVisitor::Visit(TailCallStatement* stmt) {
  return Visit(stmt->call, true).type();
}

VisitResult ImplementationVisitor::Visit(ConditionalExpression* expr) {
  Block* true_block = assembler().NewBlock(assembler().CurrentStack());
  Block* false_block = assembler().NewBlock(assembler().CurrentStack());
  Block* done_block = assembler().NewBlock();
  Block* true_conversion_block = assembler().NewBlock();
  GenerateExpressionBranch(expr->condition, true_block, false_block);

  VisitResult left;
  VisitResult right;

  {
    // The code for both paths of the conditional need to be generated first
    // before evaluating the conditional expression because the common type of
    // the result of both the true and false of the condition needs to be known
    // to convert both branches to a common type.
    assembler().Bind(true_block);
    StackScope left_scope(this);
    left = Visit(expr->if_true);
    assembler().Goto(true_conversion_block);

    const Type* common_type;
    {
      assembler().Bind(false_block);
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
    Block* false_block = assembler().NewBlock(assembler().CurrentStack());
    Binding<LocalLabel> false_binding{&LabelBindingsManager::Get(),
                                      kFalseLabelName, LocalLabel{false_block}};
    left_result = Visit(expr->left);
    if (left_result.type()->IsBool()) {
      Block* true_block = LookupSimpleLabel(kTrueLabelName);
      assembler().Branch(true_block, false_block);
      assembler().Bind(false_block);
    } else if (left_result.type()->IsNever()) {
      assembler().Bind(false_block);
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
    Block* true_block = LookupSimpleLabel(kTrueLabelName);
    Block* false_block = LookupSimpleLabel(kFalseLabelName);
    assembler().Branch(true_block, false_block);
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
    Block* true_block = assembler().NewBlock(assembler().CurrentStack());
    Binding<LocalLabel> false_binding{&LabelBindingsManager::Get(),
                                      kTrueLabelName, LocalLabel{true_block}};
    left_result = Visit(expr->left);
    if (left_result.type()->IsBool()) {
      Block* false_block = LookupSimpleLabel(kFalseLabelName);
      assembler().Branch(true_block, false_block);
      assembler().Bind(true_block);
    } else if (left_result.type()->IsNever()) {
      assembler().Bind(true_block);
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
    Block* true_block = LookupSimpleLabel(kTrueLabelName);
    Block* false_block = LookupSimpleLabel(kFalseLabelName);
    assembler().Branch(true_block, false_block);
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
  const Type* result_type = Declarations::LookupType(CONST_FLOAT64_TYPE_STRING);
  if (i == d) {
    if ((i >> 30) == (i >> 31)) {
      result_type = Declarations::LookupType(CONST_INT31_TYPE_STRING);
    } else {
      result_type = Declarations::LookupType(CONST_INT32_TYPE_STRING);
    }
  }
  return VisitResult{result_type, expr->number};
}

VisitResult ImplementationVisitor::Visit(AssumeTypeImpossibleExpression* expr) {
  VisitResult result = Visit(expr->expression);
  const Type* result_type =
      SubtractType(result.type(), Declarations::GetType(expr->excluded_type));
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
  const Type* type = TypeOracle::GetBuiltinPointerType(
      builtin->signature().parameter_types.types,
      builtin->signature().return_type);
  assembler().Emit(
      PushBuiltinPointerInstruction{builtin->ExternalName(), type});
  return VisitResult(type, assembler().TopRange(1));
}

VisitResult ImplementationVisitor::Visit(IdentifierExpression* expr) {
  StackScope scope(this);
  return scope.Yield(GenerateFetchFromLocation(GetLocationReference(expr)));
}

const Type* ImplementationVisitor::Visit(GotoStatement* stmt) {
  LocalLabel* label = LookupLabel(stmt->label);
  size_t parameter_count = label->parameter_types.size();
  if (stmt->arguments.size() != parameter_count) {
    ReportError("goto to label has incorrect number of parameters (expected ",
                parameter_count, " found ", stmt->arguments.size(), ")");
  }

  size_t i = 0;
  StackRange arguments = assembler().TopRange(0);
  for (Expression* e : stmt->arguments) {
    StackScope scope(this);
    VisitResult result = Visit(e);
    const Type* parameter_type = label->parameter_types[i++];
    result = GenerateImplicitConvert(parameter_type, result);
    arguments.Extend(scope.Yield(result).stack_range());
  }

  assembler().Goto(label->block, arguments.Size());
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
    const Type* left_result = Visit(stmt->if_true);
    if (left_result == TypeOracle::GetVoidType()) {
      assembler().Goto(done_block);
    }

    assembler().Bind(false_block);
    const Type* right_result = TypeOracle::GetVoidType();
    if (has_else) {
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
    Block* true_block = assembler().NewBlock(assembler().CurrentStack(),
                                             IsDeferred(stmt->if_true));
    Block* false_block =
        assembler().NewBlock(assembler().CurrentStack(),
                             stmt->if_false && IsDeferred(*stmt->if_false));
    GenerateExpressionBranch(stmt->condition, true_block, false_block);

    Block* done_block;
    bool live = false;
    if (has_else) {
      done_block = assembler().NewBlock();
    } else {
      done_block = false_block;
      live = true;
    }

    assembler().Bind(true_block);
    {
      const Type* result = Visit(stmt->if_true);
      if (result == TypeOracle::GetVoidType()) {
        live = true;
        assembler().Goto(done_block);
      }
    }

    if (has_else) {
      assembler().Bind(false_block);
      const Type* result = Visit(*stmt->if_false);
      if (result == TypeOracle::GetVoidType()) {
        live = true;
        assembler().Goto(done_block);
      }
    }

    if (live) {
      assembler().Bind(done_block);
    }
    return live ? TypeOracle::GetVoidType() : TypeOracle::GetNeverType();
  }
}

const Type* ImplementationVisitor::Visit(WhileStatement* stmt) {
  Block* body_block = assembler().NewBlock(assembler().CurrentStack());
  Block* exit_block = assembler().NewBlock(assembler().CurrentStack());

  Block* header_block = assembler().NewBlock();
  assembler().Goto(header_block);

  assembler().Bind(header_block);
  GenerateExpressionBranch(stmt->condition, body_block, exit_block);

  assembler().Bind(body_block);
  {
    BreakContinueActivator activator{exit_block, header_block};
    const Type* body_result = Visit(stmt->body);
    if (body_result != TypeOracle::GetNeverType()) {
      assembler().Goto(header_block);
    }
  }

  assembler().Bind(exit_block);
  return TypeOracle::GetVoidType();
}

const Type* ImplementationVisitor::Visit(BlockStatement* block) {
  BlockBindings<LocalValue> block_bindings(&ValueBindingsManager::Get());
  const Type* type = TypeOracle::GetVoidType();
  for (Statement* s : block->statements) {
    CurrentSourcePosition::Scope source_position(s->pos);
    if (type->IsNever()) {
      ReportError("statement after non-returning statement");
    }
    if (auto* var_declaration = VarDeclarationStatement::DynamicCast(s)) {
      type = Visit(var_declaration, &block_bindings);
    } else {
      type = Visit(s);
    }
  }
  return type;
}

const Type* ImplementationVisitor::Visit(DebugStatement* stmt) {
#if defined(DEBUG)
  assembler().Emit(PrintConstantStringInstruction{"halting because of '" +
                                                  stmt->reason + "' at " +
                                                  PositionAsString(stmt->pos)});
#endif
  assembler().Emit(AbortInstruction{stmt->never_continues
                                        ? AbortInstruction::Kind::kUnreachable
                                        : AbortInstruction::Kind::kDebugBreak});
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
    Block* true_block = assembler().NewBlock(assembler().CurrentStack());
    Block* false_block = assembler().NewBlock(assembler().CurrentStack(), true);
    GenerateExpressionBranch(stmt->expression, true_block, false_block);

    assembler().Bind(false_block);

    assembler().Emit(AbortInstruction{
        AbortInstruction::Kind::kAssertionFailure,
        "Torque assert '" + FormatAssertSource(stmt->source) + "' failed"});

    assembler().Bind(true_block);
  }
  return TypeOracle::GetVoidType();
}

const Type* ImplementationVisitor::Visit(ExpressionStatement* stmt) {
  const Type* type = Visit(stmt->expression).type();
  return type->IsNever() ? type : TypeOracle::GetVoidType();
}

const Type* ImplementationVisitor::Visit(ReturnStatement* stmt) {
  Callable* current_callable = CurrentCallable::Get();
  if (current_callable->signature().return_type->IsNever()) {
    std::stringstream s;
    s << "cannot return from a function with return type never";
    ReportError(s.str());
  }
  LocalLabel* end =
      current_callable->IsMacro() ? LookupLabel("_macro_end") : nullptr;
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

  BreakContinueActivator activator(exit_block, increment_block);

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
    VisitResult element_result;
    {
      StackScope element_scope(this);
      VisitResult result = GenerateCall("[]", {{expression_result, index}, {}});
      if (stmt->var_declaration->type) {
        const Type* declared_type =
            Declarations::GetType(*stmt->var_declaration->type);
        result = GenerateImplicitConvert(declared_type, result);
      }
      element_result = element_scope.Yield(result);
    }
    Binding<LocalValue> element_var_binding{&ValueBindingsManager::Get(),
                                            stmt->var_declaration->name,
                                            LocalValue{true, element_result}};
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

VisitResult ImplementationVisitor::TemporaryUninitializedStruct(
    const StructType* struct_type, const std::string& reason) {
  StackRange range = assembler().TopRange(0);
  for (const Field& f : struct_type->fields()) {
    if (const StructType* struct_type =
            StructType::DynamicCast(f.name_and_type.type)) {
      range.Extend(
          TemporaryUninitializedStruct(struct_type, reason).stack_range());
    } else {
      std::string descriptor = "unitialized field '" + f.name_and_type.name +
                               "' declared at " + PositionAsString(f.pos) +
                               " (" + reason + ")";
      TypeVector lowered_types = LowerType(f.name_and_type.type);
      for (const Type* type : lowered_types) {
        assembler().Emit(PushUninitializedInstruction{
            TypeOracle::GetTopType(descriptor, type)});
      }
      range.Extend(assembler().TopRange(lowered_types.size()));
    }
  }
  return VisitResult(struct_type, range);
}

VisitResult ImplementationVisitor::Visit(TryLabelExpression* expr) {
  size_t parameter_count = expr->label_block->parameters.names.size();
  std::vector<VisitResult> parameters;

  Block* label_block = nullptr;
  Block* done_block = assembler().NewBlock();
  VisitResult try_result;

  {
    CurrentSourcePosition::Scope source_position(expr->label_block->pos);
    if (expr->label_block->parameters.has_varargs) {
      ReportError("cannot use ... for label parameters");
    }
    Stack<const Type*> label_input_stack = assembler().CurrentStack();
    TypeVector parameter_types;
    for (size_t i = 0; i < parameter_count; ++i) {
      const Type* type =
          Declarations::GetType(expr->label_block->parameters.types[i]);
      parameter_types.push_back(type);
      if (type->IsConstexpr()) {
        ReportError("no constexpr type allowed for label arguments");
      }
      StackRange range = label_input_stack.PushMany(LowerType(type));
      parameters.push_back(VisitResult(type, range));
    }
    label_block = assembler().NewBlock(label_input_stack,
                                       IsDeferred(expr->label_block->body));

    Binding<LocalLabel> label_binding{&LabelBindingsManager::Get(),
                                      expr->label_block->label,
                                      LocalLabel{label_block, parameter_types}};

    // Visit try
    StackScope stack_scope(this);
    try_result = Visit(expr->try_expression);
    if (try_result.type() != TypeOracle::GetNeverType()) {
      try_result = stack_scope.Yield(try_result);
      assembler().Goto(done_block);
    }
  }

  // Visit and output the code for the label block. If the label block falls
  // through, then the try must not return a value. Also, if the try doesn't
  // fall through, but the label does, then overall the try-label block
  // returns type void.
  assembler().Bind(label_block);
  const Type* label_result;
  {
    BlockBindings<LocalValue> parameter_bindings(&ValueBindingsManager::Get());
    for (size_t i = 0; i < parameter_count; ++i) {
      parameter_bindings.Add(expr->label_block->parameters.names[i],
                             LocalValue{true, parameters[i]});
    }

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

  if (!try_result.type()->IsNever()) {
    assembler().Bind(done_block);
  }
  return try_result;
}

VisitResult ImplementationVisitor::Visit(StatementExpression* expr) {
  return VisitResult{Visit(expr->statement), assembler().TopRange(0)};
}

VisitResult ImplementationVisitor::Visit(NewExpression* expr) {
  StackScope stack_scope(this);
  const Type* type = Declarations::GetType(expr->type);
  const ClassType* class_type = ClassType::DynamicCast(type);
  if (class_type == nullptr) {
    ReportError("type for new expression must be a class, \"", *type,
                "\" is not");
  }

  // In order to ensure "atomicity" of object allocation, a class' constructors
  // operate on a per-class internal struct rather than the class directly until
  // the constructor has successfully completed and all class members are
  // available. Create the appropriate unitialized struct and pass it to the
  // matching class constructor with the arguments that were passed to new{}
  StructType* class_this_struct = class_type->struct_type();
  VisitResult unitialized_struct = TemporaryUninitializedStruct(
      class_this_struct,
      "it's not set in the constructor for class " + class_type->name());
  Arguments constructor_arguments;
  for (auto p : expr->parameters) {
    constructor_arguments.parameters.push_back(Visit(p));
  }
  LocationReference unitialized_struct_ref =
      LocationReference::VariableAccess(unitialized_struct);
  Callable* callable =
      LookupConstructor(unitialized_struct_ref, constructor_arguments, {});
  GenerateCall(callable, unitialized_struct_ref, constructor_arguments,
               {class_type}, false);
  VisitResult new_struct_result = unitialized_struct;

  // Output the code to generate an unitialized object of the class size in the
  // GC heap.
  Arguments allocate_arguments;
  allocate_arguments.parameters.push_back(VisitResult(
      TypeOracle::GetConstInt31Type(), std::to_string(class_type->size())));
  VisitResult allocate_result =
      GenerateCall("%Allocate", allocate_arguments, {class_type}, false);
  DCHECK(allocate_result.IsOnStack());

  // Fill in the fields of the newly allocated class by copying the values
  // from the struct that was built by the constructor. So that the generaeted
  // code is a bit more readable, assign the values from the first class
  // member to the last, in order. To do this, first build a list of fields
  // to assign to in reverse order by visiting the class heirarchy.
  std::vector<std::pair<const Field*, VisitResult>> store_pairs;
  const ClassType* current_class = class_type;
  while (current_class != nullptr) {
    auto& fields = current_class->fields();
    for (auto i = fields.rbegin(); i != fields.rend(); ++i) {
      store_pairs.push_back(std::make_pair(
          &*i, ProjectStructField(new_struct_result, i->name_and_type.name)));
    }
    current_class = current_class->GetSuperClass();
    if (current_class) {
      new_struct_result = ProjectStructField(new_struct_result,
                                             kConstructorStructSuperFieldName);
    }
  }

  // Now that the reversed list of fields and the assignment VisitResults are
  // available, emit the copies in reverse order of the reversed list to
  // produce the class field assignments in the expected order.
  for (auto i = store_pairs.rbegin(); i != store_pairs.rend(); ++i) {
    assembler().Emit(
        PeekInstruction(allocate_result.stack_range().begin(), class_type));
    assembler().Emit(PeekInstruction(i->second.stack_range().begin(),
                                     i->first->name_and_type.type));
    assembler().Emit(
        StoreObjectFieldInstruction(class_type, i->first->name_and_type.name));
  }

  return stack_scope.Yield(allocate_result);
}

const Type* ImplementationVisitor::Visit(BreakStatement* stmt) {
  base::Optional<Binding<LocalLabel>*> break_label = TryLookupLabel("_break");
  if (!break_label) {
    ReportError("break used outside of loop");
  }
  assembler().Goto((*break_label)->block);
  return TypeOracle::GetNeverType();
}

const Type* ImplementationVisitor::Visit(ContinueStatement* stmt) {
  base::Optional<Binding<LocalLabel>*> continue_label =
      TryLookupLabel("_continue");
  if (!continue_label) {
    ReportError("continue used outside of loop");
  }
  assembler().Goto((*continue_label)->block);
  return TypeOracle::GetNeverType();
}

const Type* ImplementationVisitor::Visit(ForLoopStatement* stmt) {
  BlockBindings<LocalValue> loop_bindings(&ValueBindingsManager::Get());

  if (stmt->var_declaration) Visit(*stmt->var_declaration, &loop_bindings);

  Block* body_block = assembler().NewBlock(assembler().CurrentStack());
  Block* exit_block = assembler().NewBlock(assembler().CurrentStack());

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

  if (stmt->test) {
    GenerateExpressionBranch(*stmt->test, body_block, exit_block);
  } else {
    assembler().Goto(body_block);
  }

  assembler().Bind(body_block);
  {
    BreakContinueActivator activator(exit_block, continue_block);
    const Type* body_result = Visit(stmt->body);
    if (body_result != TypeOracle::GetNeverType()) {
      assembler().Goto(continue_block);
    }
  }

  if (stmt->action) {
    assembler().Bind(action_block);
    const Type* action_result = Visit(*stmt->action);
    if (action_result != TypeOracle::GetNeverType()) {
      assembler().Goto(header_block);
    }
  }

  assembler().Bind(exit_block);
  return TypeOracle::GetVoidType();
}

void ImplementationVisitor::GenerateImplementation(const std::string& dir,
                                                   Namespace* nspace) {
  std::string new_source(nspace->source());
  std::string base_file_name =
      "builtins-" + DashifyString(nspace->name()) + "-from-dsl-gen";

  std::string source_file_name = dir + "/" + base_file_name + ".cc";
  ReplaceFileContentsIfDifferent(source_file_name, new_source);
  std::string new_header(nspace->header());
  std::string header_file_name = dir + "/" + base_file_name + ".h";
  ReplaceFileContentsIfDifferent(header_file_name, new_header);
}

void ImplementationVisitor::GenerateMacroFunctionDeclaration(
    std::ostream& o, const std::string& macro_prefix, Macro* macro) {
  GenerateFunctionDeclaration(o, macro_prefix, macro->ExternalName(),
                              macro->signature(), macro->parameter_names());
}

void ImplementationVisitor::GenerateFunctionDeclaration(
    std::ostream& o, const std::string& macro_prefix, const std::string& name,
    const Signature& signature, const NameVector& parameter_names) {
  if (GlobalContext::verbose()) {
    std::cout << "generating source for declaration " << name << "\n";
  }

  if (signature.return_type->IsVoidOrNever()) {
    o << "void";
  } else {
    o << signature.return_type->GetGeneratedTypeName();
  }
  o << " " << macro_prefix << name << "(";

  DCHECK_EQ(signature.types().size(), parameter_names.size());
  auto type_iterator = signature.types().begin();
  bool first = true;
  for (const std::string& name : parameter_names) {
    if (!first) {
      o << ", ";
    }
    const Type* parameter_type = *type_iterator;
    const std::string& generated_type_name =
        parameter_type->GetGeneratedTypeName();
    o << generated_type_name << " " << ExternalParameterName(name);
    type_iterator++;
    first = false;
  }

  for (const LabelDeclaration& label_info : signature.labels) {
    if (!first) {
      o << ", ";
    }
    o << "compiler::CodeAssemblerLabel* " << ExternalLabelName(label_info.name);
    size_t i = 0;
    for (const Type* type : label_info.types) {
      std::string generated_type_name("compiler::TypedCodeAssemblerVariable<");
      generated_type_name += type->GetGeneratedTNodeTypeName();
      generated_type_name += ">*";
      o << ", ";
      o << generated_type_name << " "
        << ExternalLabelParameterName(label_info.name, i);
      ++i;
    }
  }

  o << ")";
}

namespace {

void FailCallableLookup(const std::string& reason, const QualifiedName& name,
                        const TypeVector& parameter_types,
                        const std::vector<Binding<LocalLabel>*>& labels,
                        const std::vector<Signature>& candidates) {
  std::stringstream stream;
  stream << "\n" << reason << ": \n  " << name << "(" << parameter_types << ")";
  if (labels.size() != 0) {
    stream << " labels ";
    for (size_t i = 0; i < labels.size(); ++i) {
      stream << labels[i]->name() << "(" << labels[i]->parameter_types << ")";
    }
  }
  stream << "\ncandidates are:";
  for (const Signature& signature : candidates) {
    stream << "\n  " << name;
    PrintSignature(stream, signature, false);
  }
  ReportError(stream.str());
}

Callable* GetOrCreateSpecialization(const SpecializationKey& key) {
  if (base::Optional<Callable*> specialization =
          key.generic->GetSpecialization(key.specialized_types)) {
    return *specialization;
  }
  return DeclarationVisitor().SpecializeImplicit(key);
}

}  // namespace

base::Optional<Binding<LocalValue>*> ImplementationVisitor::TryLookupLocalValue(
    const std::string& name) {
  return ValueBindingsManager::Get().TryLookup(name);
}

base::Optional<Binding<LocalLabel>*> ImplementationVisitor::TryLookupLabel(
    const std::string& name) {
  return LabelBindingsManager::Get().TryLookup(name);
}

Binding<LocalLabel>* ImplementationVisitor::LookupLabel(
    const std::string& name) {
  base::Optional<Binding<LocalLabel>*> label = TryLookupLabel(name);
  if (!label) ReportError("cannot find label ", name);
  return *label;
}

Block* ImplementationVisitor::LookupSimpleLabel(const std::string& name) {
  LocalLabel* label = LookupLabel(name);
  if (!label->parameter_types.empty()) {
    ReportError("label ", name,
                "was expected to have no parameters, but has parameters (",
                label->parameter_types, ")");
  }
  return label->block;
}

template <class Container>
Callable* ImplementationVisitor::LookupCallable(
    const QualifiedName& name, const Container& declaration_container,
    const TypeVector& parameter_types,
    const std::vector<Binding<LocalLabel>*>& labels,
    const TypeVector& specialization_types) {
  Callable* result = nullptr;

  std::vector<Declarable*> overloads;
  std::vector<Signature> overload_signatures;
  for (auto* declarable : declaration_container) {
    if (Generic* generic = Generic::DynamicCast(declarable)) {
      base::Optional<TypeVector> inferred_specialization_types =
          generic->InferSpecializationTypes(specialization_types,
                                            parameter_types);
      if (!inferred_specialization_types) continue;
      overloads.push_back(generic);
      overload_signatures.push_back(
          DeclarationVisitor().MakeSpecializedSignature(
              SpecializationKey{generic, *inferred_specialization_types}));
    } else if (Callable* callable = Callable::DynamicCast(declarable)) {
      overloads.push_back(callable);
      overload_signatures.push_back(callable->signature());
    }
  }
  // Indices of candidates in overloads/overload_signatures.
  std::vector<size_t> candidates;
  for (size_t i = 0; i < overloads.size(); ++i) {
    const Signature& signature = overload_signatures[i];
    bool try_bool_context = labels.size() == 0 &&
                            signature.return_type == TypeOracle::GetNeverType();
    base::Optional<Binding<LocalLabel>*> true_label;
    base::Optional<Binding<LocalLabel>*> false_label;
    if (try_bool_context) {
      true_label = TryLookupLabel(kTrueLabelName);
      false_label = TryLookupLabel(kFalseLabelName);
    }
    if (IsCompatibleSignature(signature, parameter_types, labels) ||
        (true_label && false_label &&
         IsCompatibleSignature(signature, parameter_types,
                               {*true_label, *false_label}))) {
      candidates.push_back(i);
    }
  }

  if (overloads.empty()) {
    std::stringstream stream;
    stream << "no matching declaration found for " << name;
    ReportError(stream.str());
  } else if (candidates.empty()) {
    FailCallableLookup("cannot find suitable callable with name", name,
                       parameter_types, labels, overload_signatures);
  }

  auto is_better_candidate = [&](size_t a, size_t b) {
    return ParameterDifference(overload_signatures[a].GetExplicitTypes(),
                               parameter_types)
        .StrictlyBetterThan(ParameterDifference(
            overload_signatures[b].GetExplicitTypes(), parameter_types));
  };

  size_t best = *std::min_element(candidates.begin(), candidates.end(),
                                  is_better_candidate);
  // This check is contained in libstdc++'s std::min_element.
  DCHECK(!is_better_candidate(best, best));
  for (size_t candidate : candidates) {
    if (candidate != best && !is_better_candidate(best, candidate)) {
      std::vector<Signature> candidate_signatures;
      for (size_t i : candidates) {
        candidate_signatures.push_back(overload_signatures[i]);
      }
      FailCallableLookup("ambiguous callable ", name, parameter_types, labels,
                         candidate_signatures);
    }
  }

  if (Generic* generic = Generic::DynamicCast(overloads[best])) {
    result = GetOrCreateSpecialization(
        SpecializationKey{generic, *generic->InferSpecializationTypes(
                                       specialization_types, parameter_types)});
  } else {
    result = Callable::cast(overloads[best]);
  }

  size_t caller_size = parameter_types.size();
  size_t callee_size =
      result->signature().types().size() - result->signature().implicit_count;
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

template <class Container>
Callable* ImplementationVisitor::LookupCallable(
    const QualifiedName& name, const Container& declaration_container,
    const Arguments& arguments, const TypeVector& specialization_types) {
  return LookupCallable(name, declaration_container,
                        arguments.parameters.GetTypeVector(), arguments.labels,
                        specialization_types);
}

Method* ImplementationVisitor::LookupMethod(
    const std::string& name, LocationReference this_reference,
    const Arguments& arguments, const TypeVector& specialization_types) {
  TypeVector types(arguments.parameters.GetTypeVector());
  types.insert(types.begin(), this_reference.GetVisitResult().type());
  return Method::cast(
      LookupCallable({{}, name},
                     AggregateType::cast(this_reference.GetVisitResult().type())
                         ->Methods(name),
                     types, arguments.labels, specialization_types));
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
  StackScope stack_scope(this);
  const Type* raw_type = Declarations::LookupType(
      QualifiedName(decl->namespace_qualification, decl->name));
  if (!raw_type->IsStructType()) {
    std::stringstream s;
    s << decl->name << " is not a struct but used like one ";
    ReportError(s.str());
  }
  const StructType* struct_type = StructType::cast(raw_type);
  // Push unitialized 'this'
  VisitResult uninitialized_struct = TemporaryUninitializedStruct(
      struct_type,
      "it's not set in the constructor for struct " + struct_type->name());
  Arguments constructor_arguments;
  for (auto p : decl->expressions) {
    constructor_arguments.parameters.push_back(Visit(p));
  }
  LocationReference this_ref =
      LocationReference::VariableAccess(uninitialized_struct);
  Callable* callable = LookupConstructor(this_ref, constructor_arguments, {});
  GenerateCall(callable, this_ref, constructor_arguments, {}, false);
  return stack_scope.Yield(uninitialized_struct);
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
  if (expr->namespace_qualification.empty()) {
    if (base::Optional<Binding<LocalValue>*> value =
            TryLookupLocalValue(expr->name)) {
      if (expr->generic_arguments.size() != 0) {
        ReportError("cannot have generic parameters on local name ",
                    expr->name);
      }
      if ((*value)->is_const) {
        return LocationReference::Temporary((*value)->value,
                                            "constant value " + expr->name);
      }
      return LocationReference::VariableAccess((*value)->value);
    }
  }

  if (expr->IsThis()) {
    ReportError("\"this\" cannot be qualified");
  }
  QualifiedName name = QualifiedName(expr->namespace_qualification, expr->name);
  if (base::Optional<Builtin*> builtin = Declarations::TryLookupBuiltin(name)) {
    return LocationReference::Temporary(GetBuiltinCode(*builtin),
                                        "builtin " + expr->name);
  }
  if (expr->generic_arguments.size() != 0) {
    Generic* generic = Declarations::LookupUniqueGeneric(name);
    Callable* specialization = GetOrCreateSpecialization(
        SpecializationKey{generic, GetTypeVector(expr->generic_arguments)});
    if (Builtin* builtin = Builtin::DynamicCast(specialization)) {
      DCHECK(!builtin->IsExternal());
      return LocationReference::Temporary(GetBuiltinCode(builtin),
                                          "builtin " + expr->name);
    } else {
      ReportError("cannot create function pointer for non-builtin ",
                  generic->name());
    }
  }
  Value* value = Declarations::LookupValue(name);
  if (auto* constant = NamespaceConstant::DynamicCast(value)) {
    if (constant->type()->IsConstexpr()) {
      return LocationReference::Temporary(
          VisitResult(constant->type(), constant->ExternalAssemblerName() +
                                            "(state_)." +
                                            constant->constant_name() + "()"),
          "namespace constant " + expr->name);
    }
    assembler().Emit(NamespaceConstantInstruction{constant});
    StackRange stack_range =
        assembler().TopRange(LoweredSlotCount(constant->type()));
    return LocationReference::Temporary(
        VisitResult(constant->type(), stack_range),
        "namespace constant " + expr->name);
  }
  ExternConstant* constant = ExternConstant::cast(value);
  return LocationReference::Temporary(constant->value(),
                                      "extern value " + expr->name);
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
    ReportError("cannot assign to temporary ",
                reference.temporary_description());
  }
}

VisitResult ImplementationVisitor::GeneratePointerCall(
    Expression* callee, const Arguments& arguments, bool is_tailcall) {
  StackScope scope(this);
  TypeVector parameter_types(arguments.parameters.GetTypeVector());
  VisitResult callee_result = Visit(callee);
  if (!callee_result.type()->IsBuiltinPointerType()) {
    std::stringstream stream;
    stream << "Expected a function pointer type but found "
           << *callee_result.type();
    ReportError(stream.str());
  }
  const BuiltinPointerType* type =
      BuiltinPointerType::cast(callee_result.type());

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

  assembler().Emit(
      CallBuiltinPointerInstruction{is_tailcall, type, arg_range.Size()});

  if (is_tailcall) {
    return VisitResult::NeverResult();
  }
  DCHECK_EQ(1, LoweredSlotCount(type->return_type()));
  return scope.Yield(VisitResult(type->return_type(), assembler().TopRange(1)));
}

void ImplementationVisitor::AddCallParameter(
    Callable* callable, VisitResult parameter, const Type* parameter_type,
    std::vector<VisitResult>* converted_arguments, StackRange* argument_range,
    std::vector<std::string>* constexpr_arguments) {
  VisitResult converted = GenerateImplicitConvert(parameter_type, parameter);
  converted_arguments->push_back(converted);
  if (!callable->ShouldBeInlined()) {
    if (converted.IsOnStack()) {
      argument_range->Extend(converted.stack_range());
    } else {
      constexpr_arguments->push_back(converted.constexpr_value());
    }
  }
}

VisitResult ImplementationVisitor::GenerateCall(
    Callable* callable, base::Optional<LocationReference> this_reference,
    Arguments arguments, const TypeVector& specialization_types,
    bool is_tailcall) {
  // Operators used in a branching context can also be function calls that never
  // return but have a True and False label
  if (arguments.labels.size() == 0 &&
      callable->signature().labels.size() == 2) {
    Binding<LocalLabel>* true_label = LookupLabel(kTrueLabelName);
    arguments.labels.push_back(true_label);
    Binding<LocalLabel>* false_label = LookupLabel(kFalseLabelName);
    arguments.labels.push_back(false_label);
  }

  const Type* return_type = callable->signature().return_type;

  std::vector<VisitResult> converted_arguments;
  StackRange argument_range = assembler().TopRange(0);
  std::vector<std::string> constexpr_arguments;

  size_t current = 0;
  for (; current < callable->signature().implicit_count; ++current) {
    std::string implicit_name = callable->signature().parameter_names[current];
    base::Optional<Binding<LocalValue>*> val =
        TryLookupLocalValue(implicit_name);
    if (!val) {
      ReportError("implicit parameter '", implicit_name,
                  "' required for call to '", callable->ReadableName(),
                  "' is not defined");
    }
    AddCallParameter(callable, (*val)->value,
                     callable->signature().parameter_types.types[current],
                     &converted_arguments, &argument_range,
                     &constexpr_arguments);
  }

  if (this_reference) {
    DCHECK(callable->IsMethod());
    Method* method = Method::cast(callable);
    // By now, the this reference should either be a variable or
    // a temporary, in both cases the fetch of the VisitResult should succeed.
    VisitResult this_value = this_reference->GetVisitResult();
    if (method->ShouldBeInlined()) {
      if (!this_value.type()->IsSubtypeOf(method->aggregate_type())) {
        ReportError("this parameter must be a subtype of ",
                    *method->aggregate_type(), " but it is of type ",
                    this_value.type());
      }
    } else {
      AddCallParameter(callable, this_value, method->aggregate_type(),
                       &converted_arguments, &argument_range,
                       &constexpr_arguments);
    }
    ++current;
  }

  for (auto arg : arguments.parameters) {
    const Type* to_type = (current >= callable->signature().types().size())
                              ? TypeOracle::GetObjectType()
                              : callable->signature().types()[current++];
    AddCallParameter(callable, arg, to_type, &converted_arguments,
                     &argument_range, &constexpr_arguments);
  }

  if (GlobalContext::verbose()) {
    std::cout << "generating code for call to " << callable->ReadableName()
              << "\n";
  }

  size_t label_count = callable->signature().labels.size();
  if (label_count != arguments.labels.size()) {
    std::stringstream s;
    s << "unexpected number of otherwise labels for "
      << callable->ReadableName() << " (expected "
      << std::to_string(label_count) << " found "
      << std::to_string(arguments.labels.size()) << ")";
    ReportError(s.str());
  }

  if (callable->IsTransitioning()) {
    if (!CurrentCallable::Get()->IsTransitioning()) {
      std::stringstream s;
      s << *CurrentCallable::Get()
        << " isn't marked transitioning but calls the transitioning "
        << *callable;
      ReportError(s.str());
    }
  }

  if (auto* builtin = Builtin::DynamicCast(callable)) {
    base::Optional<Block*> catch_block = GetCatchBlock();
    assembler().Emit(CallBuiltinInstruction{
        is_tailcall, builtin, argument_range.Size(), catch_block});
    GenerateCatchBlock(catch_block);
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
      result << "(" << macro->external_assembler_name() << "(state_)."
             << macro->ExternalName() << "(";
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
    } else if (macro->ShouldBeInlined()) {
      std::vector<Block*> label_blocks;
      for (Binding<LocalLabel>* label : arguments.labels) {
        label_blocks.push_back(label->block);
      }
      return InlineMacro(macro, this_reference, converted_arguments,
                         label_blocks);
    } else if (arguments.labels.empty() &&
               return_type != TypeOracle::GetNeverType()) {
      base::Optional<Block*> catch_block = GetCatchBlock();
      assembler().Emit(
          CallCsaMacroInstruction{macro, constexpr_arguments, catch_block});
      GenerateCatchBlock(catch_block);
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
      base::Optional<Block*> catch_block = GetCatchBlock();
      assembler().Emit(CallCsaMacroAndBranchInstruction{
          macro, constexpr_arguments, return_continuation, label_blocks,
          catch_block});
      GenerateCatchBlock(catch_block);

      for (size_t i = 0; i < label_count; ++i) {
        Binding<LocalLabel>* label = arguments.labels[i];
        size_t callee_label_parameters =
            callable->signature().labels[i].types.size();
        if (label->parameter_types.size() != callee_label_parameters) {
          std::stringstream s;
          s << "label " << label->name()
            << " doesn't have the right number of parameters (found "
            << std::to_string(label->parameter_types.size()) << " expected "
            << std::to_string(callee_label_parameters) << ")";
          ReportError(s.str());
        }
        assembler().Bind(label_blocks[i]);
        assembler().Goto(
            label->block,
            LowerParameterTypes(callable->signature().labels[i].types).size());

        size_t j = 0;
        for (auto t : callable->signature().labels[i].types) {
          const Type* parameter_type = label->parameter_types[j];
          if (parameter_type != t) {
            ReportError("mismatch of label parameters (expected ", *t, " got ",
                        parameter_type, " for parameter ", i + 1, ")");
          }
          j++;
        }
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
    base::Optional<Block*> catch_block = GetCatchBlock();
    assembler().Emit(CallRuntimeInstruction{
        is_tailcall, runtime_function, argument_range.Size(), catch_block});
    GenerateCatchBlock(catch_block);
    if (is_tailcall || return_type == TypeOracle::GetNeverType()) {
      return VisitResult::NeverResult();
    } else {
      size_t slot_count = LoweredSlotCount(return_type);
      DCHECK_LE(slot_count, 1);
      // TODO(tebbi): Actually, runtime functions have to return a value, so
      // we should assert slot_count == 1 here.
      return VisitResult(return_type, assembler().TopRange(slot_count));
    }
  } else if (auto* intrinsic = Intrinsic::DynamicCast(callable)) {
    if (intrinsic->ExternalName() == "%RawConstexprCast") {
      if (intrinsic->signature().parameter_types.types.size() != 1 ||
          constexpr_arguments.size() != 1) {
        ReportError(
            "%RawConstexprCast must take a single parameter with constexpr "
            "type");
      }
      if (!return_type->IsConstexpr()) {
        std::stringstream s;
        s << *return_type
          << " return type for %RawConstexprCast is not constexpr";
        ReportError(s.str());
      }
      std::stringstream result;
      result << "static_cast<" << return_type->GetGeneratedTypeName() << ">(";
      result << constexpr_arguments[0];
      result << ")";
      return VisitResult(return_type, result.str());
    } else {
      assembler().Emit(
          CallIntrinsicInstruction{intrinsic, constexpr_arguments});
      size_t return_slot_count =
          LoweredSlotCount(intrinsic->signature().return_type);
      return VisitResult(return_type, assembler().TopRange(return_slot_count));
    }
  } else {
    UNREACHABLE();
  }
}

VisitResult ImplementationVisitor::GenerateCall(
    const QualifiedName& callable_name, Arguments arguments,
    const TypeVector& specialization_types, bool is_tailcall) {
  Callable* callable =
      LookupCallable(callable_name, Declarations::Lookup(callable_name),
                     arguments, specialization_types);
  return GenerateCall(callable, base::nullopt, arguments, specialization_types,
                      is_tailcall);
}

VisitResult ImplementationVisitor::Visit(CallExpression* expr,
                                         bool is_tailcall) {
  StackScope scope(this);
  Arguments arguments;
  QualifiedName name =
      QualifiedName(expr->callee->namespace_qualification, expr->callee->name);
  TypeVector specialization_types =
      GetTypeVector(expr->callee->generic_arguments);
  bool has_template_arguments = !specialization_types.empty();
  for (Expression* arg : expr->arguments)
    arguments.parameters.push_back(Visit(arg));
  arguments.labels = LabelsFromIdentifiers(expr->labels);
  if (!has_template_arguments && name.namespace_qualification.empty() &&
      TryLookupLocalValue(name.name)) {
    return scope.Yield(
        GeneratePointerCall(expr->callee, arguments, is_tailcall));
  } else {
    return scope.Yield(
        GenerateCall(name, arguments, specialization_types, is_tailcall));
  }
}

VisitResult ImplementationVisitor::Visit(CallMethodExpression* expr) {
  StackScope scope(this);
  Arguments arguments;
  std::string method_name = expr->method->name;
  TypeVector specialization_types =
      GetTypeVector(expr->method->generic_arguments);
  LocationReference target = GetLocationReference(expr->target);
  if (!target.IsVariableAccess()) {
    VisitResult result = GenerateFetchFromLocation(target);
    target = LocationReference::Temporary(result, "method target result");
  }
  const AggregateType* target_type =
      AggregateType::DynamicCast(target.GetVisitResult().type());
  if (!target_type) {
    ReportError("target of method call not a struct or class type");
  }
  if (method_name == kConstructMethodName || method_name == kSuperMethodName) {
    if (CurrentConstructorInfo::Get()) {
      ConstructorInfo& info = *CurrentConstructorInfo::Get();
      if (method_name == kSuperMethodName) {
        if (info.super_calls != 0) {
          ReportError("\"super\" can only be called once from a constructor");
        }
        ++info.super_calls;
        DCHECK(target_type->IsStructType());
        base::Optional<const ClassType*> derived_from =
            StructType::cast(target_type)->GetDerivedFrom();
        if (!derived_from) {
          ReportError("\"super\" can only be called from class constructors");
        }
        if ((*derived_from)->GetSuperClass() == nullptr) {
          ReportError(
              "\"super\" can only be called in constructors for derived "
              "classes");
        }
      } else {
        ReportError("cannot call a constructor from a constructor");
      }
    } else {
      ReportError(
          "cannot call a constructor or \"super\" from a non-constructor");
    }
  }
  for (Expression* arg : expr->arguments) {
    arguments.parameters.push_back(Visit(arg));
  }
  arguments.labels = LabelsFromIdentifiers(expr->labels);
  TypeVector argument_types = arguments.parameters.GetTypeVector();
  DCHECK_EQ(expr->method->namespace_qualification.size(), 0);
  QualifiedName qualified_name = QualifiedName(method_name);
  Callable* callable = nullptr;
  if (method_name == kConstructMethodName) {
    callable = LookupConstructor(target, arguments, {});
  } else if (method_name == kSuperMethodName) {
    LocationReference super_this =
        LocationReference::VariableAccess(ProjectStructField(
            target.GetVisitResult(), kConstructorStructSuperFieldName));
    callable = LookupConstructor(super_this, arguments, {});
    VisitResult super_result =
        GenerateCall(callable, super_this, arguments, {}, false);
    return scope.Yield(super_result);
  } else {
    callable = LookupMethod(method_name, target, arguments, {});
  }
  return scope.Yield(GenerateCall(callable, target, arguments, {}, false));
}

VisitResult ImplementationVisitor::Visit(LoadObjectFieldExpression* expr) {
  VisitResult base_result = Visit(expr->base);
  auto class_type = ClassType::DynamicCast(base_result.type());
  if (!class_type) {
    ReportError(
        "base expression for a LoadObjectFieldExpression is not a class type "
        "but instead ",
        *base_result.type());
  }
  VisitResult result = base_result;
  assembler().Emit(LoadObjectFieldInstruction{class_type, expr->field_name});
  const Field& field = class_type->LookupField(expr->field_name);
  result.SetType(field.name_and_type.type);
  return result;
}

VisitResult ImplementationVisitor::Visit(StoreObjectFieldExpression* expr) {
  VisitResult base_result = Visit(expr->base);
  auto class_type = ClassType::DynamicCast(base_result.type());
  if (!class_type) {
    ReportError(
        "base expression for a StoreObjectFieldExpression is not a class type "
        "but instead ",
        *base_result.type());
  }
  VisitResult value = Visit(expr->value);
  assembler().Emit(StoreObjectFieldInstruction{class_type, expr->field_name});
  return VisitResult(value.type(), assembler().TopRange(0));
}

VisitResult ImplementationVisitor::Visit(IntrinsicCallExpression* expr) {
  StackScope scope(this);
  Arguments arguments;
  TypeVector specialization_types = GetTypeVector(expr->generic_arguments);
  for (Expression* arg : expr->arguments)
    arguments.parameters.push_back(Visit(arg));
  return scope.Yield(
      GenerateCall(expr->name, arguments, specialization_types, false));
}

void ImplementationVisitor::GenerateBranch(const VisitResult& condition,
                                           Block* true_block,
                                           Block* false_block) {
  DCHECK_EQ(condition,
            VisitResult(TypeOracle::GetBoolType(), assembler().TopRange(1)));
  assembler().Branch(true_block, false_block);
}

void ImplementationVisitor::GenerateExpressionBranch(Expression* expression,
                                                     Block* true_block,
                                                     Block* false_block) {
  // Conditional expressions can either explicitly return a bit
  // type, or they can be backed by macros that don't return but
  // take a true and false label. By declaring the labels before
  // visiting the conditional expression, those label-based
  // macro conditionals will be able to find them through normal
  // label lookups.
  Binding<LocalLabel> true_binding{&LabelBindingsManager::Get(), kTrueLabelName,
                                   LocalLabel{true_block}};
  Binding<LocalLabel> false_binding{&LabelBindingsManager::Get(),
                                    kFalseLabelName, LocalLabel{false_block}};
  StackScope stack_scope(this);
  VisitResult expression_result = Visit(expression);
  if (!expression_result.type()->IsNever()) {
    expression_result = stack_scope.Yield(
        GenerateImplicitConvert(TypeOracle::GetBoolType(), expression_result));
    GenerateBranch(expression_result, true_block, false_block);
  }
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
    return scope.Yield(GenerateCall(kFromConstexprMacroName, {{source}, {}},
                                    {destination_type, source.type()}, false));
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

StackRange ImplementationVisitor::GenerateLabelGoto(
    LocalLabel* label, base::Optional<StackRange> arguments) {
  return assembler().Goto(label->block, arguments ? arguments->Size() : 0);
}

std::vector<Binding<LocalLabel>*> ImplementationVisitor::LabelsFromIdentifiers(
    const std::vector<std::string>& names) {
  std::vector<Binding<LocalLabel>*> result;
  result.reserve(names.size());
  for (const auto& name : names) {
    result.push_back(LookupLabel(name));
  }
  return result;
}

StackRange ImplementationVisitor::LowerParameter(
    const Type* type, const std::string& parameter_name,
    Stack<std::string>* lowered_parameters) {
  if (const StructType* struct_type = StructType::DynamicCast(type)) {
    StackRange range = lowered_parameters->TopRange(0);
    for (auto& field : struct_type->fields()) {
      StackRange parameter_range = LowerParameter(
          field.name_and_type.type,
          parameter_name + "." + field.name_and_type.name, lowered_parameters);
      range.Extend(parameter_range);
    }
    return range;
  } else {
    lowered_parameters->Push(parameter_name);
    return lowered_parameters->TopRange(1);
  }
}

std::string ImplementationVisitor::ExternalLabelName(
    const std::string& label_name) {
  return "label_" + label_name;
}

std::string ImplementationVisitor::ExternalLabelParameterName(
    const std::string& label_name, size_t i) {
  return "label_" + label_name + "_parameter_" + std::to_string(i);
}

std::string ImplementationVisitor::ExternalParameterName(
    const std::string& name) {
  return std::string("p_") + name;
}

DEFINE_CONTEXTUAL_VARIABLE(ImplementationVisitor::ValueBindingsManager);
DEFINE_CONTEXTUAL_VARIABLE(ImplementationVisitor::LabelBindingsManager);
DEFINE_CONTEXTUAL_VARIABLE(ImplementationVisitor::CurrentCallable);
DEFINE_CONTEXTUAL_VARIABLE(ImplementationVisitor::CurrentReturnValue);
DEFINE_CONTEXTUAL_VARIABLE(ImplementationVisitor::CurrentConstructorInfo);

bool IsCompatibleSignature(const Signature& sig, const TypeVector& types,
                           const std::vector<Binding<LocalLabel>*>& labels) {
  auto i = sig.parameter_types.types.begin() + sig.implicit_count;
  if ((sig.parameter_types.types.size() - sig.implicit_count) > types.size())
    return false;
  // TODO(danno): The test below is actually insufficient. The labels'
  // parameters must be checked too. ideally, the named part of
  // LabelDeclarationVector would be factored out so that the label count and
  // parameter types could be passed separately.
  if (sig.labels.size() != labels.size()) return false;
  for (auto current : types) {
    if (i == sig.parameter_types.types.end()) {
      if (!sig.parameter_types.var_args) return false;
      if (!IsAssignableFrom(TypeOracle::GetObjectType(), current)) return false;
    } else {
      if (!IsAssignableFrom(*i++, current)) return false;
    }
  }
  return true;
}

base::Optional<Block*> ImplementationVisitor::GetCatchBlock() {
  base::Optional<Block*> catch_block;
  if (base::Optional<Binding<LocalLabel>*> catch_handler =
          TryLookupLabel("_catch")) {
    catch_block = assembler().NewBlock(base::nullopt, true);
  }
  return catch_block;
}

void ImplementationVisitor::GenerateCatchBlock(
    base::Optional<Block*> catch_block) {
  if (catch_block) {
    base::Optional<Binding<LocalLabel>*> catch_handler =
        TryLookupLabel("_catch");
    if (assembler().CurrentBlockIsComplete()) {
      assembler().Bind(*catch_block);
      assembler().Goto((*catch_handler)->block, 1);
    } else {
      CfgAssemblerScopedTemporaryBlock temp(&assembler(), *catch_block);
      assembler().Goto((*catch_handler)->block, 1);
    }
  }
}

void ImplementationVisitor::VisitAllDeclarables() {
  const std::vector<std::unique_ptr<Declarable>>& all_declarables =
      GlobalContext::AllDeclarables();
  // This has to be an index-based loop because all_declarables can be extended
  // during the loop.
  for (size_t i = 0; i < all_declarables.size(); ++i) {
    Visit(all_declarables[i].get());
  }
}

void ImplementationVisitor::Visit(Declarable* declarable) {
  CurrentConstructorInfo::Scope current_constructor(base::nullopt);
  CurrentScope::Scope current_scope(declarable->ParentScope());
  CurrentSourcePosition::Scope current_source_position(declarable->pos());
  switch (declarable->kind()) {
    case Declarable::kMacro:
      return Visit(Macro::cast(declarable));
    case Declarable::kMethod:
      return Visit(Method::cast(declarable));
    case Declarable::kBuiltin:
      return Visit(Builtin::cast(declarable));
    case Declarable::kTypeAlias:
      return Visit(TypeAlias::cast(declarable));
    case Declarable::kNamespaceConstant:
      return Visit(NamespaceConstant::cast(declarable));
    case Declarable::kRuntimeFunction:
    case Declarable::kIntrinsic:
    case Declarable::kExternConstant:
    case Declarable::kNamespace:
    case Declarable::kGeneric:
      return;
  }
}

void ImplementationVisitor::GenerateBuiltinDefinitions(std::string& file_name) {
  std::stringstream new_contents_stream;
  new_contents_stream
      << "#ifndef V8_BUILTINS_BUILTIN_DEFINITIONS_FROM_DSL_H_\n"
         "#define V8_BUILTINS_BUILTIN_DEFINITIONS_FROM_DSL_H_\n"
         "\n"
         "#define BUILTIN_LIST_FROM_DSL(CPP, API, TFJ, TFC, TFS, TFH, ASM) "
         "\\\n";
  for (auto& declarable : GlobalContext::AllDeclarables()) {
    Builtin* builtin = Builtin::DynamicCast(declarable.get());
    if (!builtin || builtin->IsExternal()) continue;
    int firstParameterIndex = 1;
    bool declareParameters = true;
    if (builtin->IsStub()) {
      new_contents_stream << "TFS(" << builtin->ExternalName();
    } else {
      new_contents_stream << "TFJ(" << builtin->ExternalName();
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
  new_contents_stream << "\n";

  new_contents_stream
      << "#define TORQUE_FUNCTION_POINTER_TYPE_TO_BUILTIN_MAP(V) \\\n";
  for (const BuiltinPointerType* type : TypeOracle::AllBuiltinPointerTypes()) {
    Builtin* example_builtin =
        Declarations::FindSomeInternalBuiltinWithType(type);
    if (!example_builtin) {
      CurrentSourcePosition::Scope current_source_position(
          SourcePosition{CurrentSourceFile::Get(), -1, -1});
      ReportError("unable to find any builtin with type \"", *type, "\"");
    }
    new_contents_stream << "  V(" << type->function_pointer_type_id() << ","
                        << example_builtin->ExternalName() << ")\\\n";
  }
  new_contents_stream << "\n";

  new_contents_stream
      << "#endif  // V8_BUILTINS_BUILTIN_DEFINITIONS_FROM_DSL_H_\n";

  std::string new_contents(new_contents_stream.str());
  ReplaceFileContentsIfDifferent(file_name, new_contents);
}

void ImplementationVisitor::GenerateClassDefinitions(std::string& file_name) {
  std::stringstream new_contents_stream;
  new_contents_stream << "#ifndef V8_CLASS_BUILTIN_DEFINITIONS_FROM_DSL_H_\n"
                         "#define V8_CLASS_BUILTIN_DEFINITIONS_FROM_DSL_H_\n"
                         "\n\n";

  for (auto i : GlobalContext::GetClasses()) {
    // TODO(danno): Ideally (and we've got several core V8 dev's feedback
    // supporting this), Torque should generate the constants for the offsets
    // directly and not go through the existing layer of macros, which actually
    // currently just serves to additionally obfuscate where these values come
    // from.
    new_contents_stream << "#define ";
    new_contents_stream << CapifyStringWithUnderscores(i.first)
                        << "_FIELDS(V) \\\n";
    const ClassType* type = i.second;
    std::vector<Field> fields = type->fields();
    new_contents_stream << "V(kStartOfStrongFieldsOffset, 0) \\\n";
    for (auto f : fields) {
      if (!f.is_weak) {
        new_contents_stream << "V(k" << CamelifyString(f.name_and_type.name)
                            << "Offset, kTaggedSize) \\\n";
      }
    }
    new_contents_stream << "V(kEndOfStrongFieldsOffset, 0) \\\n";
    new_contents_stream << "V(kStartOfWeakFieldsOffset, 0) \\\n";
    for (auto f : fields) {
      if (f.is_weak) {
        new_contents_stream << "V(k" << CamelifyString(f.name_and_type.name)
                            << "Offset, kTaggedSize) \\\n";
      }
    }
    new_contents_stream << "V(kEndOfWeakFieldsOffset, 0) \\\n";
    new_contents_stream << "V(kSize, 0) \\\n";
    new_contents_stream << "\n";
  }

  new_contents_stream
      << "\n#endif  // V8_CLASS_BUILTIN_DEFINITIONS_FROM_DSL_H_\n";

  std::string new_contents(new_contents_stream.str());
  ReplaceFileContentsIfDifferent(file_name, new_contents);
}

}  // namespace torque
}  // namespace internal
}  // namespace v8
