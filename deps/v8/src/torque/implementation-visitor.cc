// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "src/common/globals.h"
#include "src/torque/csa-generator.h"
#include "src/torque/declaration-visitor.h"
#include "src/torque/implementation-visitor.h"
#include "src/torque/parameter-difference.h"
#include "src/torque/server-data.h"
#include "src/torque/type-inference.h"
#include "src/torque/type-visitor.h"

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

void ImplementationVisitor::BeginCSAFiles() {
  for (SourceId file : SourceFileMap::AllSources()) {
    std::ostream& source = GlobalContext::GeneratedPerFile(file).csa_ccfile;
    std::ostream& header = GlobalContext::GeneratedPerFile(file).csa_headerfile;

    for (const std::string& include_path : GlobalContext::CppIncludes()) {
      source << "#include " << StringLiteralQuote(include_path) << "\n";
    }

    for (SourceId file : SourceFileMap::AllSources()) {
      source << "#include \"torque-generated/" +
                    SourceFileMap::PathFromV8RootWithoutExtension(file) +
                    "-tq-csa.h\"\n";
    }
    source << "\n";

    source << "namespace v8 {\n"
           << "namespace internal {\n"
           << "\n";

    std::string headerDefine =
        "V8_GEN_TORQUE_GENERATED_" +
        UnderlinifyPath(SourceFileMap::PathFromV8Root(file)) + "_H_";
    header << "#ifndef " << headerDefine << "\n";
    header << "#define " << headerDefine << "\n\n";
    header << "#include \"src/builtins/builtins-promise.h\"\n";
    header << "#include \"src/compiler/code-assembler.h\"\n";
    header << "#include \"src/codegen/code-stub-assembler.h\"\n";
    header << "#include \"src/utils/utils.h\"\n";
    header << "#include \"torque-generated/field-offsets-tq.h\"\n";
    header << "#include \"torque-generated/csa-types-tq.h\"\n";
    header << "\n";

    header << "namespace v8 {\n"
           << "namespace internal {\n"
           << "\n";
  }
}

void ImplementationVisitor::EndCSAFiles() {
  for (SourceId file : SourceFileMap::AllSources()) {
    std::ostream& source = GlobalContext::GeneratedPerFile(file).csa_ccfile;
    std::ostream& header = GlobalContext::GeneratedPerFile(file).csa_headerfile;

    std::string headerDefine =
        "V8_GEN_TORQUE_GENERATED_" +
        UnderlinifyPath(SourceFileMap::PathFromV8Root(file)) + "_H_";

    source << "}  // namespace internal\n"
           << "}  // namespace v8\n"
           << "\n";

    header << "}  // namespace internal\n"
           << "}  // namespace v8\n"
           << "\n";
    header << "#endif  // " << headerDefine << "\n";
  }
}

void ImplementationVisitor::Visit(NamespaceConstant* decl) {
  Signature signature{{}, base::nullopt, {{}, false}, 0, decl->type(),
                      {}, false};

  BindingsManagersScope bindings_managers_scope;

  header_out() << "  ";
  GenerateFunctionDeclaration(header_out(), "", decl->external_name(),
                              signature, {});
  header_out() << ";\n";

  GenerateFunctionDeclaration(source_out(), "", decl->external_name(),
                              signature, {});
  source_out() << " {\n";
  source_out() << "  compiler::CodeAssembler ca_(state_);\n";

  DCHECK(!signature.return_type->IsVoidOrNever());

  assembler_ = CfgAssembler(Stack<const Type*>{});

  VisitResult expression_result = Visit(decl->body());
  VisitResult return_result =
      GenerateImplicitConvert(signature.return_type, expression_result);

  CSAGenerator csa_generator{assembler().Result(), source_out()};
  Stack<std::string> values = *csa_generator.EmitGraph(Stack<std::string>{});

  assembler_ = base::nullopt;

  source_out() << "  return ";
  CSAGenerator::EmitCSAValue(return_result, values, source_out());
  source_out() << ";\n";
  source_out() << "}\n\n";
}

void ImplementationVisitor::Visit(TypeAlias* alias) {
  if (alias->IsRedeclaration()) return;
  if (const ClassType* class_type = ClassType::DynamicCast(alias->type())) {
    if (class_type->IsExtern() && !class_type->nspace()->IsDefaultNamespace()) {
      Error(
          "extern classes are currently only supported in the default "
          "namespace");
    }
  }
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
    parameter_bindings.Add(kThisParameterName, LocalValue{*this_reference},
                           true);
  }

  size_t i = 0;
  for (auto arg : arguments) {
    if (this_reference && i == signature.implicit_count) i++;
    const bool mark_as_used = signature.implicit_count > i;
    const Identifier* name = macro->parameter_names()[i++];
    parameter_bindings.Add(name,
                           LocalValue{LocationReference::Temporary(
                               arg, "parameter " + name->value)},
                           mark_as_used);
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
    macro_end_binding.emplace(&LabelBindingsManager::Get(), kMacroEndLabelName,
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
  CurrentCallable::Scope current_callable(macro);
  const Signature& signature = macro->signature();
  const Type* return_type = macro->signature().return_type;
  bool can_return = return_type != TypeOracle::GetNeverType();
  bool has_return_value =
      can_return && return_type != TypeOracle::GetVoidType();

  GenerateMacroFunctionDeclaration(header_out(), "", macro);
  header_out() << ";\n";

  GenerateMacroFunctionDeclaration(source_out(), "", macro);
  source_out() << " {\n";
  source_out() << "  compiler::CodeAssembler ca_(state_);\n";

  Stack<std::string> lowered_parameters;
  Stack<const Type*> lowered_parameter_types;

  std::vector<VisitResult> arguments;

  base::Optional<LocationReference> this_reference;
  if (Method* method = Method::DynamicCast(macro)) {
    const Type* this_type = method->aggregate_type();
    LowerParameter(this_type, ExternalParameterName(kThisParameterName),
                   &lowered_parameters);
    StackRange range = lowered_parameter_types.PushMany(LowerType(this_type));
    VisitResult this_result = VisitResult(this_type, range);
    // For classes, mark 'this' as a temporary to prevent assignment to it.
    // Note that using a VariableAccess for non-class types is technically
    // incorrect because changes to the 'this' variable do not get reflected
    // to the caller. Therefore struct methods should always be inlined and a
    // C++ version should never be generated, since it would be incorrect.
    // However, in order to be able to type- and semantics-check even unused
    // struct methods, set the this_reference to be the local variable copy of
    // the passed-in this, which allows the visitor to at least find and report
    // errors.
    this_reference =
        (this_type->IsClassType())
            ? LocationReference::Temporary(this_result, "this parameter")
            : LocationReference::VariableAccess(this_result);
  }

  for (size_t i = 0; i < macro->signature().parameter_names.size(); ++i) {
    if (this_reference && i == macro->signature().implicit_count) continue;
    const std::string& name = macro->parameter_names()[i]->value;
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
      LowerLabelParameter(label_info.types[i],
                          ExternalLabelParameterName(label_info.name->value, i),
                          &label_parameter_variables);
    }
    assembler().Emit(GotoExternalInstruction{
        ExternalLabelName(label_info.name->value), label_parameter_variables});
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

void ImplementationVisitor::Visit(TorqueMacro* macro) {
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
                         BlockBindings<LocalValue>* parameter_bindings,
                         bool mark_as_used) {
  const Identifier* name = builtin->signature().parameter_names[i];
  const Type* type = builtin->signature().types()[i];
  std::string external_name = "parameter" + std::to_string(i);
  parameters->Push(external_name);
  StackRange range = parameter_types->PushMany(LowerType(type));
  parameter_bindings->Add(
      name,
      LocalValue{LocationReference::Temporary(VisitResult(type, range),
                                              "parameter " + name->value)},
      mark_as_used);
  return external_name;
}

}  // namespace

void ImplementationVisitor::Visit(Builtin* builtin) {
  if (builtin->IsExternal()) return;
  CurrentScope::Scope current_scope(builtin);
  CurrentCallable::Scope current_callable(builtin);
  CurrentReturnValue::Scope current_return_value;

  const std::string& name = builtin->ExternalName();
  const Signature& signature = builtin->signature();
  source_out() << "TF_BUILTIN(" << name << ", CodeStubAssembler) {\n"
               << "  compiler::CodeAssemblerState* state_ = state();"
               << "  compiler::CodeAssembler ca_(state());\n";

  Stack<const Type*> parameter_types;
  Stack<std::string> parameters;

  BindingsManagersScope bindings_managers_scope;

  BlockBindings<LocalValue> parameter_bindings(&ValueBindingsManager::Get());

  if (builtin->IsVarArgsJavaScript() || builtin->IsFixedArgsJavaScript()) {
    if (builtin->IsVarArgsJavaScript()) {
      DCHECK(signature.parameter_types.var_args);
      if (signature.ExplicitCount() > 0) {
        Error("Cannot mix explicit parameters with varargs.")
            .Position(signature.parameter_names[signature.implicit_count]->pos);
      }

      source_out()
          << "  Node* argc = Parameter(Descriptor::kJSActualArgumentsCount);\n";
      source_out() << "  TNode<IntPtrT> "
                      "arguments_length(ChangeInt32ToIntPtr(UncheckedCast<"
                      "Int32T>(argc)));\n";
      source_out() << "  TNode<RawPtrT> arguments_frame = "
                      "UncheckedCast<RawPtrT>(LoadFramePointer());\n";
      source_out() << "  TorqueStructArguments "
                      "torque_arguments(GetFrameArguments(arguments_frame, "
                      "arguments_length));\n";
      source_out()
          << "  CodeStubArguments arguments(this, torque_arguments);\n";

      parameters.Push("torque_arguments.frame");
      parameters.Push("torque_arguments.base");
      parameters.Push("torque_arguments.length");
      const Type* arguments_type = TypeOracle::GetArgumentsType();
      StackRange range = parameter_types.PushMany(LowerType(arguments_type));
      parameter_bindings.Add(*signature.arguments_variable,
                             LocalValue{LocationReference::Temporary(
                                 VisitResult(arguments_type, range),
                                 "parameter " + *signature.arguments_variable)},
                             true);
    }

    for (size_t i = 0; i < signature.implicit_count; ++i) {
      const std::string& param_name = signature.parameter_names[i]->value;
      SourcePosition param_pos = signature.parameter_names[i]->pos;
      std::string generated_name = AddParameter(
          i, builtin, &parameters, &parameter_types, &parameter_bindings, true);
      const Type* actual_type = signature.parameter_types.types[i];
      std::vector<const Type*> expected_types;
      if (param_name == "context") {
        source_out() << "  TNode<NativeContext> " << generated_name
                     << " = UncheckedCast<NativeContext>(Parameter("
                     << "Descriptor::kContext));\n";
        source_out() << "  USE(" << generated_name << ");\n";
        expected_types = {TypeOracle::GetNativeContextType(),
                          TypeOracle::GetContextType()};
      } else if (param_name == "receiver") {
        source_out()
            << "  TNode<Object> " << generated_name << " = "
            << (builtin->IsVarArgsJavaScript()
                    ? "arguments.GetReceiver()"
                    : "UncheckedCast<Object>(Parameter(Descriptor::kReceiver))")
            << ";\n";
        source_out() << "USE(" << generated_name << ");\n";
        expected_types = {TypeOracle::GetJSAnyType()};
      } else if (param_name == "newTarget") {
        source_out() << "  TNode<Object> " << generated_name
                     << " = UncheckedCast<Object>(Parameter("
                     << "Descriptor::kJSNewTarget));\n";
        source_out() << "USE(" << generated_name << ");\n";
        expected_types = {TypeOracle::GetJSAnyType()};
      } else if (param_name == "target") {
        source_out() << "  TNode<JSFunction> " << generated_name
                     << " = UncheckedCast<JSFunction>(Parameter("
                     << "Descriptor::kJSTarget));\n";
        source_out() << "USE(" << generated_name << ");\n";
        expected_types = {TypeOracle::GetJSFunctionType()};
      } else {
        Error(
            "Unexpected implicit parameter \"", param_name,
            "\" for JavaScript calling convention, "
            "expected \"context\", \"receiver\", \"target\", or \"newTarget\"")
            .Position(param_pos);
        expected_types = {actual_type};
      }
      if (std::find(expected_types.begin(), expected_types.end(),
                    actual_type) == expected_types.end()) {
        Error("According to JavaScript calling convention, expected parameter ",
              param_name, " to have type ", PrintList(expected_types, " or "),
              " but found type ", *actual_type)
            .Position(param_pos);
      }
    }

    for (size_t i = signature.implicit_count;
         i < signature.parameter_names.size(); ++i) {
      const std::string& parameter_name = signature.parameter_names[i]->value;
      const Type* type = signature.types()[i];
      const bool mark_as_used = signature.implicit_count > i;
      std::string var = AddParameter(i, builtin, &parameters, &parameter_types,
                                     &parameter_bindings, mark_as_used);
      source_out() << "  " << type->GetGeneratedTypeName() << " " << var
                   << " = "
                   << "UncheckedCast<" << type->GetGeneratedTNodeTypeName()
                   << ">(Parameter(Descriptor::k"
                   << CamelifyString(parameter_name) << "));\n";
      source_out() << "  USE(" << var << ");\n";
    }

  } else {
    DCHECK(builtin->IsStub());

    // Context
    const bool context_is_implicit = signature.implicit_count > 0;
    std::string parameter0 =
        AddParameter(0, builtin, &parameters, &parameter_types,
                     &parameter_bindings, context_is_implicit);
    source_out() << "  TNode<Context> " << parameter0
                 << " = UncheckedCast<Context>(Parameter("
                 << "Descriptor::kContext));\n";
    source_out() << "  USE(" << parameter0 << ");\n";

    for (size_t i = 1; i < signature.parameter_names.size(); ++i) {
      const Type* type = signature.types()[i];
      const bool mark_as_used = signature.implicit_count > i;
      std::string var = AddParameter(i, builtin, &parameters, &parameter_types,
                                     &parameter_bindings, mark_as_used);
      source_out() << "  " << type->GetGeneratedTypeName() << " " << var
                   << " = "
                   << "UncheckedCast<" << type->GetGeneratedTNodeTypeName()
                   << ">(Parameter(Descriptor::ParameterIndex<" << (i - 1)
                   << ">()));\n";
      source_out() << "  USE(" << var << ");\n";
    }
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
  // const qualified variables are required to be initialized properly.
  if (stmt->const_qualified && !stmt->initializer) {
    ReportError("local constant \"", stmt->name, "\" is not initialized.");
  }

  base::Optional<const Type*> type;
  if (stmt->type) {
    type = TypeVisitor::ComputeType(*stmt->type);
  }
  base::Optional<VisitResult> init_result;
  if (stmt->initializer) {
    StackScope scope(this);
    init_result = Visit(*stmt->initializer);
    if (type) {
      init_result = GenerateImplicitConvert(*type, *init_result);
    }
    type = init_result->type();
    if ((*type)->IsConstexpr() && !stmt->const_qualified) {
      Error("Use 'const' instead of 'let' for variable '", stmt->name->value,
            "' of constexpr type '", (*type)->ToString(), "'.")
          .Position(stmt->name->pos)
          .Throw();
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
          "uninitialized variable '" + stmt->name->value + "' of type " +
              type->ToString() + " originally defined at " +
              PositionAsString(stmt->pos),
          type)});
    }
    init_result =
        VisitResult(*type, assembler().TopRange(lowered_types.size()));
  }
  LocationReference ref = stmt->const_qualified
                              ? LocationReference::Temporary(
                                    *init_result, "const " + stmt->name->value)
                              : LocationReference::VariableAccess(*init_result);
  block_bindings->Add(stmt->name, LocalValue{std::move(ref)});
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
  StackScope outer_scope(this);
  VisitResult left_result = Visit(expr->left);

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

  Block* true_block = assembler().NewBlock();
  Block* false_block = assembler().NewBlock();
  Block* done_block = assembler().NewBlock();

  left_result = GenerateImplicitConvert(TypeOracle::GetBoolType(), left_result);
  GenerateBranch(left_result, true_block, false_block);

  assembler().Bind(true_block);
  VisitResult true_result = GenerateBoolConstant(true);
  assembler().Goto(done_block);

  assembler().Bind(false_block);
  VisitResult false_result;
  {
    StackScope false_block_scope(this);
    false_result = false_block_scope.Yield(
        GenerateImplicitConvert(TypeOracle::GetBoolType(), Visit(expr->right)));
  }
  assembler().Goto(done_block);

  assembler().Bind(done_block);
  DCHECK_EQ(true_result, false_result);
  return outer_scope.Yield(true_result);
}

VisitResult ImplementationVisitor::Visit(LogicalAndExpression* expr) {
  StackScope outer_scope(this);
  VisitResult left_result = Visit(expr->left);

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

  Block* true_block = assembler().NewBlock();
  Block* false_block = assembler().NewBlock();
  Block* done_block = assembler().NewBlock();

  left_result = GenerateImplicitConvert(TypeOracle::GetBoolType(), left_result);
  GenerateBranch(left_result, true_block, false_block);

  assembler().Bind(true_block);
  VisitResult true_result;
  {
    StackScope true_block_scope(this);
    true_result = true_block_scope.Yield(
        GenerateImplicitConvert(TypeOracle::GetBoolType(), Visit(expr->right)));
  }
  assembler().Goto(done_block);

  assembler().Bind(false_block);
  VisitResult false_result = GenerateBoolConstant(false);
  assembler().Goto(done_block);

  assembler().Bind(done_block);
  DCHECK_EQ(true_result, false_result);
  return outer_scope.Yield(true_result);
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
  int32_t i = static_cast<int32_t>(expr->number);
  const Type* result_type = TypeOracle::GetConstFloat64Type();
  if (i == expr->number) {
    if ((i >> 30) == (i >> 31)) {
      result_type = TypeOracle::GetConstInt31Type();
    } else {
      result_type = TypeOracle::GetConstInt32Type();
    }
  }
  return VisitResult{result_type, ToString(expr->number)};
}

VisitResult ImplementationVisitor::Visit(AssumeTypeImpossibleExpression* expr) {
  VisitResult result = Visit(expr->expression);
  const Type* result_type = SubtractType(
      result.type(), TypeVisitor::ComputeType(expr->excluded_type));
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

VisitResult ImplementationVisitor::Visit(LocationExpression* expr) {
  StackScope scope(this);
  return scope.Yield(GenerateFetchFromLocation(GetLocationReference(expr)));
}

const Type* ImplementationVisitor::Visit(GotoStatement* stmt) {
  Binding<LocalLabel>* label = LookupLabel(stmt->label->value);
  size_t parameter_count = label->parameter_types.size();
  if (stmt->arguments.size() != parameter_count) {
    ReportError("goto to label has incorrect number of parameters (expected ",
                parameter_count, " found ", stmt->arguments.size(), ")");
  }

  if (GlobalContext::collect_language_server_data()) {
    LanguageServerData::AddDefinition(stmt->label->pos,
                                      label->declaration_position());
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
  std::replace_if(
      str_no_newlines.begin(), str_no_newlines.end(),
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
  bool do_check = !stmt->debug_only || GlobalContext::force_assert_statements();
#if defined(DEBUG)
  do_check = true;
#endif
  Block* resume_block;

  if (!do_check) {
    Block* unreachable_block = assembler().NewBlock(assembler().CurrentStack());
    resume_block = assembler().NewBlock(assembler().CurrentStack());
    assembler().Goto(resume_block);
    assembler().Bind(unreachable_block);
  }

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

  if (!do_check) {
    assembler().Bind(resume_block);
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
      current_callable->IsMacro() ? LookupLabel(kMacroEndLabelName) : nullptr;
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
          TypeVisitor::ComputeType(expr->label_block->parameters.types[i]);
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
      Identifier* name = expr->label_block->parameters.names[i];
      parameter_bindings.Add(name,
                             LocalValue{LocationReference::Temporary(
                                 parameters[i], "parameter " + name->value)});
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

void ImplementationVisitor::CheckInitializersWellformed(
    const std::string& aggregate_name,
    const std::vector<Field>& aggregate_fields,
    const std::vector<NameAndExpression>& initializers,
    bool ignore_first_field) {
  size_t fields_offset = ignore_first_field ? 1 : 0;
  size_t fields_size = aggregate_fields.size() - fields_offset;
  for (size_t i = 0; i < std::min(fields_size, initializers.size()); i++) {
    const std::string& field_name =
        aggregate_fields[i + fields_offset].name_and_type.name;
    Identifier* found_name = initializers[i].name;
    if (field_name != found_name->value) {
      Error("Expected field name \"", field_name, "\" instead of \"",
            found_name->value, "\"")
          .Position(found_name->pos)
          .Throw();
    }
  }
  if (fields_size != initializers.size()) {
    ReportError("expected ", fields_size, " initializers for ", aggregate_name,
                " found ", initializers.size());
  }
}

InitializerResults ImplementationVisitor::VisitInitializerResults(
    const ClassType* class_type,
    const std::vector<NameAndExpression>& initializers) {
  InitializerResults result;
  for (const NameAndExpression& initializer : initializers) {
    result.names.push_back(initializer.name);
    Expression* e = initializer.expression;
    const Field& field = class_type->LookupField(initializer.name->value);
    bool has_index = field.index.has_value();
    if (SpreadExpression* s = SpreadExpression::DynamicCast(e)) {
      if (!has_index) {
        ReportError(
            "spread expressions can only be used to initialize indexed class "
            "fields ('",
            initializer.name->value, "' is not)");
      }
      e = s->spreadee;
    } else if (has_index) {
      ReportError("the indexed class field '", initializer.name->value,
                  "' must be initialized with a spread operator");
    }
    result.field_value_map[field.name_and_type.name] = Visit(e);
  }
  return result;
}

LocationReference ImplementationVisitor::GenerateFieldReference(
    VisitResult object, const Field& field, const ClassType* class_type) {
  StackRange result_range = assembler().TopRange(0);
  result_range.Extend(GenerateCopy(object).stack_range());
  VisitResult offset;
  if (field.offset.has_value()) {
    offset =
        VisitResult(TypeOracle::GetConstInt31Type(), ToString(*field.offset));
    offset = GenerateImplicitConvert(TypeOracle::GetIntPtrType(), offset);
  } else {
    StackScope stack_scope(this);
    for (const Field& f : class_type->ComputeAllFields()) {
      if (f.offset) {
        offset =
            VisitResult(TypeOracle::GetConstInt31Type(), ToString(*f.offset));
      }
      if (f.name_and_type.name == field.name_and_type.name) break;
      if (f.index) {
        if (!offset.IsOnStack()) {
          offset = GenerateImplicitConvert(TypeOracle::GetIntPtrType(), offset);
        }
        VisitResult array_length = GenerateArrayLength(object, f);
        size_t element_size;
        std::string element_size_string;
        std::tie(element_size, element_size_string) =
            *SizeOf(f.name_and_type.type);
        VisitResult array_element_size =
            VisitResult(TypeOracle::GetConstInt31Type(), element_size_string);
        // In contrast to the code used for allocation, we don't need overflow
        // checks here because we already know all the offsets fit into memory.
        VisitResult array_size =
            GenerateCall("*", {{array_length, array_element_size}, {}});
        offset = GenerateCall("+", {{offset, array_size}, {}});
      }
    }
    DCHECK(offset.IsOnStack());
    offset = stack_scope.Yield(offset);
  }
  result_range.Extend(offset.stack_range());
  if (field.index) {
    VisitResult length = GenerateArrayLength(object, field);
    result_range.Extend(length.stack_range());
    const Type* slice_type = TypeOracle::GetSliceType(field.name_and_type.type);
    return LocationReference::HeapSlice(VisitResult(slice_type, result_range));

  } else {
    VisitResult heap_reference(
        TypeOracle::GetReferenceType(field.name_and_type.type), result_range);
    return LocationReference::HeapReference(heap_reference);
  }
}

// This is used to generate field references during initialization, where we can
// re-use the offsets used for computing the allocation size.
LocationReference ImplementationVisitor::GenerateFieldReference(
    VisitResult object, const Field& field,
    const LayoutForInitialization& layout) {
  StackRange result_range = assembler().TopRange(0);
  result_range.Extend(GenerateCopy(object).stack_range());
  VisitResult offset = GenerateImplicitConvert(
      TypeOracle::GetIntPtrType(), layout.offsets.at(field.name_and_type.name));
  result_range.Extend(offset.stack_range());
  if (field.index) {
    VisitResult length =
        GenerateCopy(layout.array_lengths.at(field.name_and_type.name));
    result_range.Extend(length.stack_range());
    const Type* slice_type = TypeOracle::GetSliceType(field.name_and_type.type);
    return LocationReference::HeapSlice(VisitResult(slice_type, result_range));
  } else {
    VisitResult heap_reference(
        TypeOracle::GetReferenceType(field.name_and_type.type), result_range);
    return LocationReference::HeapReference(heap_reference);
  }
}

void ImplementationVisitor::InitializeClass(
    const ClassType* class_type, VisitResult allocate_result,
    const InitializerResults& initializer_results,
    const LayoutForInitialization& layout) {
  if (const ClassType* super = class_type->GetSuperClass()) {
    InitializeClass(super, allocate_result, initializer_results, layout);
  }

  for (Field f : class_type->fields()) {
    VisitResult initializer_value =
        initializer_results.field_value_map.at(f.name_and_type.name);
    LocationReference field =
        GenerateFieldReference(allocate_result, f, layout);
    if (f.index) {
      DCHECK(field.IsHeapSlice());
      VisitResult slice = field.GetVisitResult();
      GenerateCall(QualifiedName({TORQUE_INTERNAL_NAMESPACE_STRING},
                                 "InitializeFieldsFromIterator"),
                   {{slice, initializer_value}, {}});
    } else {
      GenerateAssignToLocation(field, initializer_value);
    }
  }
}

VisitResult ImplementationVisitor::GenerateArrayLength(
    Expression* array_length, Namespace* nspace,
    const std::map<std::string, LocationReference>& bindings) {
  StackScope stack_scope(this);
  CurrentSourcePosition::Scope pos_scope(array_length->pos);
  // Switch to the namespace where the class was declared.
  CurrentScope::Scope current_scope_scope(nspace);
  // Reset local bindings and install local binding for the preceding fields.
  BindingsManagersScope bindings_managers_scope;
  BlockBindings<LocalValue> field_bindings(&ValueBindingsManager::Get());
  for (auto& p : bindings) {
    field_bindings.Add(p.first, LocalValue{p.second}, true);
  }
  VisitResult length = Visit(array_length);
  VisitResult converted_length =
      GenerateCall("Convert", Arguments{{length}, {}},
                   {TypeOracle::GetIntPtrType(), length.type()}, false);
  return stack_scope.Yield(converted_length);
}

VisitResult ImplementationVisitor::GenerateArrayLength(VisitResult object,
                                                       const Field& field) {
  DCHECK(field.index);

  StackScope stack_scope(this);
  const ClassType* class_type = *object.type()->ClassSupertype();
  std::map<std::string, LocationReference> bindings;
  for (Field f : class_type->ComputeAllFields()) {
    if (f.index) break;
    bindings.insert(
        {f.name_and_type.name, GenerateFieldReference(object, f, class_type)});
  }
  return stack_scope.Yield(
      GenerateArrayLength(*field.index, class_type->nspace(), bindings));
}

VisitResult ImplementationVisitor::GenerateArrayLength(
    const ClassType* class_type, const InitializerResults& initializer_results,
    const Field& field) {
  DCHECK(field.index);

  StackScope stack_scope(this);
  std::map<std::string, LocationReference> bindings;
  for (Field f : class_type->ComputeAllFields()) {
    if (f.index) break;
    const std::string& fieldname = f.name_and_type.name;
    VisitResult value = initializer_results.field_value_map.at(fieldname);
    bindings.insert({fieldname, LocationReference::Temporary(
                                    value, "initial field " + fieldname)});
  }
  return stack_scope.Yield(
      GenerateArrayLength(*field.index, class_type->nspace(), bindings));
}

LayoutForInitialization ImplementationVisitor::GenerateLayoutForInitialization(
    const ClassType* class_type,
    const InitializerResults& initializer_results) {
  LayoutForInitialization layout;
  VisitResult offset;
  for (Field f : class_type->ComputeAllFields()) {
    if (f.offset.has_value()) {
      offset =
          VisitResult(TypeOracle::GetConstInt31Type(), ToString(*f.offset));
    }
    layout.offsets[f.name_and_type.name] = offset;
    if (f.index) {
      size_t element_size;
      std::string element_size_string;
      std::tie(element_size, element_size_string) =
          *SizeOf(f.name_and_type.type);
      VisitResult array_element_size =
          VisitResult(TypeOracle::GetConstInt31Type(), element_size_string);
      VisitResult array_length =
          GenerateArrayLength(class_type, initializer_results, f);
      layout.array_lengths[f.name_and_type.name] = array_length;
      Arguments arguments;
      arguments.parameters = {offset, array_length, array_element_size};
      offset = GenerateCall(QualifiedName({TORQUE_INTERNAL_NAMESPACE_STRING},
                                          "AddIndexedFieldSizeToObjectSize"),
                            arguments);
    } else {
      DCHECK(f.offset.has_value());
    }
  }
  if (class_type->size().SingleValue()) {
    layout.size = VisitResult(TypeOracle::GetConstInt31Type(),
                              ToString(*class_type->size().SingleValue()));
  } else {
    layout.size = offset;
  }
  if ((size_t{1} << class_type->size().AlignmentLog2()) <
      TargetArchitecture::TaggedSize()) {
    Arguments arguments;
    arguments.parameters = {layout.size};
    layout.size = GenerateCall(
        QualifiedName({TORQUE_INTERNAL_NAMESPACE_STRING}, "AlignTagged"),
        arguments);
  }
  return layout;
}

VisitResult ImplementationVisitor::Visit(NewExpression* expr) {
  StackScope stack_scope(this);
  const Type* type = TypeVisitor::ComputeType(expr->type);
  const ClassType* class_type = ClassType::DynamicCast(type);
  if (class_type == nullptr) {
    ReportError("type for new expression must be a class, \"", *type,
                "\" is not");
  }

  if (!class_type->AllowInstantiation()) {
    // Classes that are only used for testing should never be instantiated.
    ReportError(*class_type,
                " cannot be allocated with new (it's used for testing)");
  }

  InitializerResults initializer_results =
      VisitInitializerResults(class_type, expr->initializers);

  const Field& map_field = class_type->LookupField("map");
  if (*map_field.offset != 0) {
    ReportError("class initializers must have a map as first parameter");
  }
  const std::map<std::string, VisitResult>& initializer_fields =
      initializer_results.field_value_map;
  auto it_object_map = initializer_fields.find(map_field.name_and_type.name);
  VisitResult object_map;
  if (class_type->IsExtern()) {
    if (it_object_map == initializer_fields.end()) {
      ReportError("Constructor for ", class_type->name(),
                  " needs Map argument!");
    }
    object_map = it_object_map->second;
  } else {
    if (it_object_map != initializer_fields.end()) {
      ReportError(
          "Constructor for ", class_type->name(),
          " must not specify Map argument; it is automatically inserted.");
    }
    Arguments get_struct_map_arguments;
    get_struct_map_arguments.parameters.push_back(
        VisitResult(TypeOracle::GetConstexprInstanceTypeType(),
                    CapifyStringWithUnderscores(class_type->name()) + "_TYPE"));
    object_map = GenerateCall(
        QualifiedName({TORQUE_INTERNAL_NAMESPACE_STRING}, "GetInstanceTypeMap"),
        get_struct_map_arguments, {}, false);
    CurrentSourcePosition::Scope current_pos(expr->pos);
    initializer_results.names.insert(initializer_results.names.begin(),
                                     MakeNode<Identifier>("map"));
    initializer_results.field_value_map[map_field.name_and_type.name] =
        object_map;
  }

  CheckInitializersWellformed(class_type->name(),
                              class_type->ComputeAllFields(),
                              expr->initializers, !class_type->IsExtern());

  LayoutForInitialization layout =
      GenerateLayoutForInitialization(class_type, initializer_results);

  Arguments allocate_arguments;
  allocate_arguments.parameters.push_back(layout.size);
  allocate_arguments.parameters.push_back(object_map);
  VisitResult allocate_result = GenerateCall(
      QualifiedName({TORQUE_INTERNAL_NAMESPACE_STRING}, "Allocate"),
      allocate_arguments, {class_type}, false);
  DCHECK(allocate_result.IsOnStack());

  InitializeClass(class_type, allocate_result, initializer_results, layout);

  return stack_scope.Yield(GenerateCall(
      "%RawDownCast", Arguments{{allocate_result}, {}}, {class_type}));
}

const Type* ImplementationVisitor::Visit(BreakStatement* stmt) {
  base::Optional<Binding<LocalLabel>*> break_label =
      TryLookupLabel(kBreakLabelName);
  if (!break_label) {
    ReportError("break used outside of loop");
  }
  assembler().Goto((*break_label)->block);
  return TypeOracle::GetNeverType();
}

const Type* ImplementationVisitor::Visit(ContinueStatement* stmt) {
  base::Optional<Binding<LocalLabel>*> continue_label =
      TryLookupLabel(kContinueLabelName);
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

VisitResult ImplementationVisitor::Visit(SpreadExpression* expr) {
  ReportError(
      "spread operators are only currently supported in indexed class field "
      "initialization expressions");
}

void ImplementationVisitor::GenerateImplementation(const std::string& dir) {
  for (SourceId file : SourceFileMap::AllSources()) {
    std::string path_from_root =
        SourceFileMap::PathFromV8RootWithoutExtension(file);

    std::string new_source(
        GlobalContext::GeneratedPerFile(file).csa_ccfile.str());

    std::string source_file_name = dir + "/" + path_from_root + "-tq-csa.cc";
    WriteFile(source_file_name, new_source);
    std::string new_header(
        GlobalContext::GeneratedPerFile(file).csa_headerfile.str());
    std::string header_file_name = dir + "/" + path_from_root + "-tq-csa.h";
    WriteFile(header_file_name, new_header);
  }
}

void ImplementationVisitor::GenerateMacroFunctionDeclaration(
    std::ostream& o, const std::string& macro_prefix, Macro* macro) {
  GenerateFunctionDeclaration(o, macro_prefix, macro->ExternalName(),
                              macro->signature(), macro->parameter_names());
}

std::vector<std::string> ImplementationVisitor::GenerateFunctionDeclaration(
    std::ostream& o, const std::string& macro_prefix, const std::string& name,
    const Signature& signature, const NameVector& parameter_names,
    bool pass_code_assembler_state) {
  std::vector<std::string> generated_parameter_names;
  if (signature.return_type->IsVoidOrNever()) {
    o << "void";
  } else {
    o << signature.return_type->GetGeneratedTypeName();
  }
  o << " " << macro_prefix << name << "(";

  bool first = true;
  if (pass_code_assembler_state) {
    first = false;
    o << "compiler::CodeAssemblerState* state_";
  }

  DCHECK_GE(signature.types().size(), parameter_names.size());
  for (size_t i = 0; i < signature.types().size(); ++i) {
    if (!first) o << ", ";
    first = false;
    const Type* parameter_type = signature.types()[i];
    const std::string& generated_type_name =
        parameter_type->GetGeneratedTypeName();

    generated_parameter_names.push_back(ExternalParameterName(
        i < parameter_names.size() ? parameter_names[i]->value
                                   : std::to_string(i)));
    o << generated_type_name << " " << generated_parameter_names.back();
  }

  for (const LabelDeclaration& label_info : signature.labels) {
    if (!first) o << ", ";
    first = false;
    generated_parameter_names.push_back(
        ExternalLabelName(label_info.name->value));
    o << "compiler::CodeAssemblerLabel* " << generated_parameter_names.back();
    size_t i = 0;
    for (const Type* type : label_info.types) {
      std::string generated_type_name;
      if (type->IsStructType()) {
        generated_type_name = "\n#error no structs allowed in labels\n";
      } else {
        generated_type_name = "compiler::TypedCodeAssemblerVariable<";
        generated_type_name += type->GetGeneratedTNodeTypeName();
        generated_type_name += ">*";
      }
      o << ", ";
      generated_parameter_names.push_back(
          ExternalLabelParameterName(label_info.name->value, i));
      o << generated_type_name << " " << generated_parameter_names.back();
      ++i;
    }
  }

  o << ")";
  return generated_parameter_names;
}

namespace {

void FailCallableLookup(
    const std::string& reason, const QualifiedName& name,
    const TypeVector& parameter_types,
    const std::vector<Binding<LocalLabel>*>& labels,
    const std::vector<Signature>& candidates,
    const std::vector<std::pair<GenericCallable*, std::string>>
        inapplicable_generics) {
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
  if (inapplicable_generics.size() != 0) {
    stream << "\nfailed to instantiate all of these generic declarations:";
    for (auto& failure : inapplicable_generics) {
      GenericCallable* generic = failure.first;
      const std::string& reason = failure.second;
      stream << "\n  " << generic->name() << " defined at "
             << generic->Position() << ":\n    " << reason << "\n";
    }
  }
  ReportError(stream.str());
}

Callable* GetOrCreateSpecialization(
    const SpecializationKey<GenericCallable>& key) {
  if (base::Optional<Callable*> specialization =
          key.generic->GetSpecialization(key.specialized_types)) {
    return *specialization;
  }
  return DeclarationVisitor::SpecializeImplicit(key);
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

// Try to lookup a callable with the provided argument types. Do not report
// an error if no matching callable was found, but return false instead.
// This is used to test the presence of overloaded field accessors.
bool ImplementationVisitor::TestLookupCallable(
    const QualifiedName& name, const TypeVector& parameter_types) {
  return LookupCallable(name, Declarations::TryLookup(name), parameter_types,
                        {}, {}, true) != nullptr;
}

template <class Container>
Callable* ImplementationVisitor::LookupCallable(
    const QualifiedName& name, const Container& declaration_container,
    const TypeVector& parameter_types,
    const std::vector<Binding<LocalLabel>*>& labels,
    const TypeVector& specialization_types, bool silence_errors) {
  Callable* result = nullptr;

  std::vector<Declarable*> overloads;
  std::vector<Signature> overload_signatures;
  std::vector<std::pair<GenericCallable*, std::string>> inapplicable_generics;
  for (auto* declarable : declaration_container) {
    if (GenericCallable* generic = GenericCallable::DynamicCast(declarable)) {
      TypeArgumentInference inference = generic->InferSpecializationTypes(
          specialization_types, parameter_types);
      if (inference.HasFailed()) {
        inapplicable_generics.push_back(
            std::make_pair(generic, inference.GetFailureReason()));
        continue;
      }
      overloads.push_back(generic);
      overload_signatures.push_back(
          DeclarationVisitor::MakeSpecializedSignature(
              SpecializationKey<GenericCallable>{generic,
                                                 inference.GetResult()}));
    } else if (Callable* callable = Callable::DynamicCast(declarable)) {
      overloads.push_back(callable);
      overload_signatures.push_back(callable->signature());
    }
  }
  // Indices of candidates in overloads/overload_signatures.
  std::vector<size_t> candidates;
  for (size_t i = 0; i < overloads.size(); ++i) {
    const Signature& signature = overload_signatures[i];
    if (IsCompatibleSignature(signature, parameter_types, labels.size())) {
      candidates.push_back(i);
    }
  }

  if (overloads.empty() && inapplicable_generics.empty()) {
    if (silence_errors) return nullptr;
    std::stringstream stream;
    stream << "no matching declaration found for " << name;
    ReportError(stream.str());
  } else if (candidates.empty()) {
    if (silence_errors) return nullptr;
    FailCallableLookup("cannot find suitable callable with name", name,
                       parameter_types, labels, overload_signatures,
                       inapplicable_generics);
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
                         candidate_signatures, inapplicable_generics);
    }
  }

  if (GenericCallable* generic =
          GenericCallable::DynamicCast(overloads[best])) {
    TypeArgumentInference inference = generic->InferSpecializationTypes(
        specialization_types, parameter_types);
    result = GetOrCreateSpecialization(
        SpecializationKey<GenericCallable>{generic, inference.GetResult()});
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
                        arguments.parameters.ComputeTypeVector(),
                        arguments.labels, specialization_types);
}

Method* ImplementationVisitor::LookupMethod(
    const std::string& name, const AggregateType* receiver_type,
    const Arguments& arguments, const TypeVector& specialization_types) {
  TypeVector types(arguments.parameters.ComputeTypeVector());
  types.insert(types.begin(), receiver_type);
  return Method::cast(LookupCallable({{}, name}, receiver_type->Methods(name),
                                     types, arguments.labels,
                                     specialization_types));
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

VisitResult ImplementationVisitor::Visit(StructExpression* expr) {
  StackScope stack_scope(this);

  auto& initializers = expr->initializers;
  std::vector<VisitResult> values;
  std::vector<const Type*> term_argument_types;
  values.reserve(initializers.size());
  term_argument_types.reserve(initializers.size());

  // Compute values and types of all initializer arguments
  for (const NameAndExpression& initializer : initializers) {
    VisitResult value = Visit(initializer.expression);
    values.push_back(value);
    term_argument_types.push_back(value.type());
  }

  // Compute and check struct type from given struct name and argument types
  const StructType* struct_type = TypeVisitor::ComputeTypeForStructExpression(
      expr->type, term_argument_types);
  CheckInitializersWellformed(struct_type->name(), struct_type->fields(),
                              initializers);

  // Implicitly convert values and thereby build the struct on the stack
  StackRange struct_range = assembler().TopRange(0);
  auto& fields = struct_type->fields();
  for (size_t i = 0; i < values.size(); i++) {
    values[i] =
        GenerateImplicitConvert(fields[i].name_and_type.type, values[i]);
    struct_range.Extend(values[i].stack_range());
  }

  return stack_scope.Yield(VisitResult(struct_type, struct_range));
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
    case AstNode::Kind::kDereferenceExpression:
      return GetLocationReference(
          static_cast<DereferenceExpression*>(location));
    default:
      return LocationReference::Temporary(Visit(location), "expression");
  }
}

LocationReference ImplementationVisitor::GetLocationReference(
    FieldAccessExpression* expr) {
  return GenerateFieldAccess(GetLocationReference(expr->object),
                             expr->field->value, expr->field->pos);
}

LocationReference ImplementationVisitor::GenerateFieldAccess(
    LocationReference reference, const std::string& fieldname,
    base::Optional<SourcePosition> pos) {
  if (reference.IsVariableAccess() &&
      reference.variable().type()->IsStructType()) {
    const StructType* type = StructType::cast(reference.variable().type());
    const Field& field = type->LookupField(fieldname);
    if (GlobalContext::collect_language_server_data() && pos.has_value()) {
      LanguageServerData::AddDefinition(*pos, field.pos);
    }
    if (field.const_qualified) {
      VisitResult t_value = ProjectStructField(reference.variable(), fieldname);
      return LocationReference::Temporary(
          t_value, "for constant field '" + field.name_and_type.name + "'");
    } else {
      return LocationReference::VariableAccess(
          ProjectStructField(reference.variable(), fieldname));
    }
  }
  if (reference.IsTemporary() && reference.temporary().type()->IsStructType()) {
    if (GlobalContext::collect_language_server_data() && pos.has_value()) {
      const StructType* type = StructType::cast(reference.temporary().type());
      const Field& field = type->LookupField(fieldname);
      LanguageServerData::AddDefinition(*pos, field.pos);
    }
    return LocationReference::Temporary(
        ProjectStructField(reference.temporary(), fieldname),
        reference.temporary_description());
  }
  if (reference.ReferencedType()->IsBitFieldStructType()) {
    const BitFieldStructType* bitfield_struct =
        BitFieldStructType::cast(reference.ReferencedType());
    const BitField& field = bitfield_struct->LookupField(fieldname);
    return LocationReference::BitFieldAccess(reference, field);
  }
  if (reference.IsHeapReference()) {
    VisitResult ref = reference.heap_reference();
    auto generic_type = StructType::MatchUnaryGeneric(
        ref.type(), TypeOracle::GetReferenceGeneric());
    if (!generic_type) {
      ReportError(
          "Left-hand side of field access expression is marked as a reference "
          "but is not of type Reference<...>. Found type: ",
          ref.type()->ToString());
    }
    if (const StructType* struct_type =
            StructType::DynamicCast(*generic_type)) {
      const Field& field = struct_type->LookupField(fieldname);
      // Update the Reference's type to refer to the field type within the
      // struct.
      ref.SetType(TypeOracle::GetReferenceType(field.name_and_type.type));
      if (!field.offset.has_value()) {
        Error("accessing field with unknown offset").Throw();
      }
      if (*field.offset != 0) {
        // Copy the Reference struct up the stack and update the new copy's
        // |offset| value to point to the struct field.
        StackScope scope(this);
        ref = GenerateCopy(ref);
        VisitResult ref_offset = ProjectStructField(ref, "offset");
        VisitResult struct_offset{
            TypeOracle::GetIntPtrType()->ConstexprVersion(),
            std::to_string(*field.offset)};
        VisitResult updated_offset =
            GenerateCall("+", Arguments{{ref_offset, struct_offset}, {}});
        assembler().Poke(ref_offset.stack_range(), updated_offset.stack_range(),
                         ref_offset.type());
        ref = scope.Yield(ref);
      }
      return LocationReference::HeapReference(ref);
    }
  }
  VisitResult object_result = GenerateFetchFromLocation(reference);
  if (base::Optional<const ClassType*> class_type =
          object_result.type()->ClassSupertype()) {
    // This is a hack to distinguish the situation where we want to use
    // overloaded field accessors from when we want to create a reference.
    bool has_explicit_overloads = TestLookupCallable(
        QualifiedName{"." + fieldname}, {object_result.type()});
    if ((*class_type)->HasField(fieldname) && !has_explicit_overloads) {
      const Field& field = (*class_type)->LookupField(fieldname);
      if (GlobalContext::collect_language_server_data() && pos.has_value()) {
        LanguageServerData::AddDefinition(*pos, field.pos);
      }
      return GenerateFieldReference(object_result, field, *class_type);
    }
  }
  return LocationReference::FieldAccess(object_result, fieldname);
}

LocationReference ImplementationVisitor::GetLocationReference(
    ElementAccessExpression* expr) {
  LocationReference reference = GetLocationReference(expr->array);
  VisitResult index = Visit(expr->index);
  if (reference.IsHeapSlice()) {
    Arguments arguments{{index}, {}};
    const AggregateType* slice_type =
        AggregateType::cast(reference.heap_slice().type());
    Method* method = LookupMethod("AtIndex", slice_type, arguments, {});
    // The reference has to be treated like a normal value when calling methods
    // on the underlying slice implementation.
    LocationReference slice_value = LocationReference::Temporary(
        reference.GetVisitResult(), "slice as value");
    return LocationReference::HeapReference(
        GenerateCall(method, std::move(slice_value), arguments, {}, false));
  } else {
    return LocationReference::ArrayAccess(GenerateFetchFromLocation(reference),
                                          index);
  }
}

LocationReference ImplementationVisitor::GetLocationReference(
    IdentifierExpression* expr) {
  if (expr->namespace_qualification.empty()) {
    if (base::Optional<Binding<LocalValue>*> value =
            TryLookupLocalValue(expr->name->value)) {
      if (GlobalContext::collect_language_server_data()) {
        LanguageServerData::AddDefinition(expr->name->pos,
                                          (*value)->declaration_position());
      }
      if (expr->generic_arguments.size() != 0) {
        ReportError("cannot have generic parameters on local name ",
                    expr->name);
      }
      const LocationReference& ref = (*value)->value;
      if (ref.IsVariableAccess()) {
        // Attach the binding to enable the never-assigned-to lint check.
        return LocationReference::VariableAccess(ref.GetVisitResult(), *value);
      }
      return ref;
    }
  }

  if (expr->IsThis()) {
    ReportError("\"this\" cannot be qualified");
  }
  QualifiedName name =
      QualifiedName(expr->namespace_qualification, expr->name->value);
  if (base::Optional<Builtin*> builtin = Declarations::TryLookupBuiltin(name)) {
    if (GlobalContext::collect_language_server_data()) {
      LanguageServerData::AddDefinition(expr->name->pos,
                                        (*builtin)->Position());
    }
    return LocationReference::Temporary(GetBuiltinCode(*builtin),
                                        "builtin " + expr->name->value);
  }
  if (expr->generic_arguments.size() != 0) {
    GenericCallable* generic = Declarations::LookupUniqueGeneric(name);
    Callable* specialization =
        GetOrCreateSpecialization(SpecializationKey<GenericCallable>{
            generic, TypeVisitor::ComputeTypeVector(expr->generic_arguments)});
    if (Builtin* builtin = Builtin::DynamicCast(specialization)) {
      DCHECK(!builtin->IsExternal());
      return LocationReference::Temporary(GetBuiltinCode(builtin),
                                          "builtin " + expr->name->value);
    } else {
      ReportError("cannot create function pointer for non-builtin ",
                  generic->name());
    }
  }
  Value* value = Declarations::LookupValue(name);
  if (GlobalContext::collect_language_server_data()) {
    LanguageServerData::AddDefinition(expr->name->pos, value->name()->pos);
  }
  if (auto* constant = NamespaceConstant::DynamicCast(value)) {
    if (constant->type()->IsConstexpr()) {
      return LocationReference::Temporary(
          VisitResult(constant->type(), constant->external_name() + "(state_)"),
          "namespace constant " + expr->name->value);
    }
    assembler().Emit(NamespaceConstantInstruction{constant});
    StackRange stack_range =
        assembler().TopRange(LoweredSlotCount(constant->type()));
    return LocationReference::Temporary(
        VisitResult(constant->type(), stack_range),
        "namespace constant " + expr->name->value);
  }
  ExternConstant* constant = ExternConstant::cast(value);
  return LocationReference::Temporary(constant->value(),
                                      "extern value " + expr->name->value);
}

LocationReference ImplementationVisitor::GetLocationReference(
    DereferenceExpression* expr) {
  VisitResult ref = Visit(expr->reference);
  if (!Type::MatchUnaryGeneric(ref.type(), TypeOracle::GetReferenceGeneric())) {
    ReportError("Operator * expects a reference but found a value of type ",
                *ref.type());
  }
  return LocationReference::HeapReference(ref);
}

VisitResult ImplementationVisitor::GenerateFetchFromLocation(
    const LocationReference& reference) {
  if (reference.IsTemporary()) {
    return GenerateCopy(reference.temporary());
  } else if (reference.IsVariableAccess()) {
    return GenerateCopy(reference.variable());
  } else if (reference.IsHeapReference()) {
    const Type* referenced_type = reference.ReferencedType();
    if (referenced_type == TypeOracle::GetFloat64OrHoleType()) {
      return GenerateCall(QualifiedName({TORQUE_INTERNAL_NAMESPACE_STRING},
                                        "LoadFloat64OrHole"),
                          Arguments{{reference.heap_reference()}, {}});
    } else if (auto* struct_type = StructType::DynamicCast(referenced_type)) {
      StackRange result_range = assembler().TopRange(0);
      for (const Field& field : struct_type->fields()) {
        StackScope scope(this);
        const std::string& fieldname = field.name_and_type.name;
        VisitResult field_value = scope.Yield(GenerateFetchFromLocation(
            GenerateFieldAccess(reference, fieldname)));
        result_range.Extend(field_value.stack_range());
      }
      return VisitResult(referenced_type, result_range);
    } else {
      GenerateCopy(reference.heap_reference());
      assembler().Emit(LoadReferenceInstruction{reference.ReferencedType()});
      DCHECK_EQ(1, LoweredSlotCount(reference.ReferencedType()));
      return VisitResult(reference.ReferencedType(), assembler().TopRange(1));
    }
  } else if (reference.IsBitFieldAccess()) {
    // First fetch the bitfield struct, then get the bits out of it.
    VisitResult bit_field_struct =
        GenerateFetchFromLocation(reference.bit_field_struct_location());
    assembler().Emit(LoadBitFieldInstruction{
        BitFieldStructType::cast(bit_field_struct.type()),
        reference.bit_field()});
    return VisitResult(reference.ReferencedType(), assembler().TopRange(1));
  } else {
    if (reference.IsHeapSlice()) {
      ReportError(
          "fetching a value directly from an indexed field isn't allowed");
    }
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

    // Local variables are detected by the existence of a binding. Assignment
    // to local variables is recorded to support lint errors.
    if (reference.binding()) {
      (*reference.binding())->SetWritten();
    }
  } else if (reference.IsHeapSlice()) {
    ReportError("assigning a value directly to an indexed field isn't allowed");
  } else if (reference.IsHeapReference()) {
    const Type* referenced_type = reference.ReferencedType();
    if (referenced_type == TypeOracle::GetFloat64OrHoleType()) {
      GenerateCall(
          QualifiedName({TORQUE_INTERNAL_NAMESPACE_STRING},
                        "StoreFloat64OrHole"),
          Arguments{{reference.heap_reference(), assignment_value}, {}});
    } else if (auto* struct_type = StructType::DynamicCast(referenced_type)) {
      if (assignment_value.type() != referenced_type) {
        ReportError("Cannot assign to ", *referenced_type,
                    " with value of type ", *assignment_value.type());
      }
      for (const Field& field : struct_type->fields()) {
        const std::string& fieldname = field.name_and_type.name;
        GenerateAssignToLocation(
            GenerateFieldAccess(reference, fieldname),
            ProjectStructField(assignment_value, fieldname));
      }
    } else {
      GenerateCopy(reference.heap_reference());
      VisitResult converted_assignment_value =
          GenerateImplicitConvert(referenced_type, assignment_value);
      if (referenced_type == TypeOracle::GetFloat64Type()) {
        VisitResult silenced_float_value = GenerateCall(
            "Float64SilenceNaN", Arguments{{assignment_value}, {}});
        assembler().Poke(converted_assignment_value.stack_range(),
                         silenced_float_value.stack_range(), referenced_type);
      }
      assembler().Emit(StoreReferenceInstruction{referenced_type});
    }
  } else if (reference.IsBitFieldAccess()) {
    // First fetch the bitfield struct, then set the updated bits, then store
    // it back to where we found it.
    VisitResult bit_field_struct =
        GenerateFetchFromLocation(reference.bit_field_struct_location());
    VisitResult converted_value =
        GenerateImplicitConvert(reference.ReferencedType(), assignment_value);
    GenerateCopy(bit_field_struct);
    GenerateCopy(converted_value);
    assembler().Emit(StoreBitFieldInstruction{
        BitFieldStructType::cast(bit_field_struct.type()),
        reference.bit_field()});
    GenerateAssignToLocation(
        reference.bit_field_struct_location(),
        VisitResult(bit_field_struct.type(), assembler().TopRange(1)));
  } else {
    DCHECK(reference.IsTemporary());
    ReportError("cannot assign to temporary ",
                reference.temporary_description());
  }
}

VisitResult ImplementationVisitor::GeneratePointerCall(
    Expression* callee, const Arguments& arguments, bool is_tailcall) {
  StackScope scope(this);
  TypeVector parameter_types(arguments.parameters.ComputeTypeVector());
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
  if (!IsCompatibleSignature(sig, parameter_types, 0)) {
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
  const Type* return_type = callable->signature().return_type;

  if (is_tailcall) {
    if (Builtin* builtin = Builtin::DynamicCast(CurrentCallable::Get())) {
      const Type* outer_return_type = builtin->signature().return_type;
      if (!return_type->IsSubtypeOf(outer_return_type)) {
        Error("Cannot tailcall, type of result is ", *return_type,
              " but should be a subtype of ", *outer_return_type, ".");
      }
    } else {
      Error("Tail calls are only allowed from builtins");
    }
  }

  std::vector<VisitResult> implicit_arguments;
  for (size_t i = 0; i < callable->signature().implicit_count; ++i) {
    std::string implicit_name = callable->signature().parameter_names[i]->value;
    base::Optional<Binding<LocalValue>*> val =
        TryLookupLocalValue(implicit_name);
    if (!val) {
      ReportError("implicit parameter '", implicit_name,
                  "' required for call to '", callable->ReadableName(),
                  "' is not defined");
    }
    implicit_arguments.push_back(GenerateFetchFromLocation((*val)->value));
  }

  std::vector<VisitResult> converted_arguments;
  StackRange argument_range = assembler().TopRange(0);
  std::vector<std::string> constexpr_arguments;

  size_t current = 0;
  for (; current < callable->signature().implicit_count; ++current) {
    AddCallParameter(callable, implicit_arguments[current],
                     callable->signature().parameter_types.types[current],
                     &converted_arguments, &argument_range,
                     &constexpr_arguments);
  }

  if (this_reference) {
    DCHECK(callable->IsMethod());
    Method* method = Method::cast(callable);
    // By now, the this reference should either be a variable, a temporary or
    // a Slice. In either case the fetch of the VisitResult should succeed.
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
    macro->SetUsed();
    if (return_type->IsConstexpr()) {
      DCHECK_EQ(0, arguments.labels.size());
      std::stringstream result;
      result << "(";
      bool first = true;
      if (auto* extern_macro = ExternMacro::DynamicCast(macro)) {
        result << extern_macro->external_assembler_name() << "(state_)."
               << extern_macro->ExternalName() << "(";
      } else {
        result << macro->ExternalName() << "(state_";
        first = false;
      }
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
          if (!t->IsSubtypeOf(parameter_type)) {
            ReportError("mismatch of label parameters (label expects ",
                        *parameter_type, " but macro produces ", *t,
                        " for parameter ", i + 1, ")");
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
    if (intrinsic->ExternalName() == "%SizeOf") {
      if (specialization_types.size() != 1) {
        ReportError("%SizeOf must take a single type parameter");
      }
      const Type* type = specialization_types[0];
      std::string size_string;
      if (base::Optional<std::tuple<size_t, std::string>> size = SizeOf(type)) {
        size_string = std::get<1>(*size);
      } else {
        Error("size of ", *type, " is not known.");
      }
      return VisitResult(return_type, size_string);
    } else if (intrinsic->ExternalName() == "%RawConstexprCast") {
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
      assembler().Emit(CallIntrinsicInstruction{intrinsic, specialization_types,
                                                constexpr_arguments});
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

  if (expr->callee->name->value == "&" && expr->arguments.size() == 1) {
    if (auto* loc_expr = LocationExpression::DynamicCast(expr->arguments[0])) {
      LocationReference ref = GetLocationReference(loc_expr);
      if (ref.IsHeapReference()) return scope.Yield(ref.heap_reference());
      if (ref.IsHeapSlice()) return scope.Yield(ref.heap_slice());
    }
    ReportError("Unable to create a heap reference.");
  }

  Arguments arguments;
  QualifiedName name = QualifiedName(expr->callee->namespace_qualification,
                                     expr->callee->name->value);
  TypeVector specialization_types =
      TypeVisitor::ComputeTypeVector(expr->callee->generic_arguments);
  bool has_template_arguments = !specialization_types.empty();
  for (Expression* arg : expr->arguments)
    arguments.parameters.push_back(Visit(arg));
  arguments.labels = LabelsFromIdentifiers(expr->labels);
  if (!has_template_arguments && name.namespace_qualification.empty() &&
      TryLookupLocalValue(name.name)) {
    return scope.Yield(
        GeneratePointerCall(expr->callee, arguments, is_tailcall));
  } else {
    if (GlobalContext::collect_language_server_data()) {
      Callable* callable = LookupCallable(name, Declarations::Lookup(name),
                                          arguments, specialization_types);
      LanguageServerData::AddDefinition(expr->callee->name->pos,
                                        callable->IdentifierPosition());
    }
    return scope.Yield(
        GenerateCall(name, arguments, specialization_types, is_tailcall));
  }
}

VisitResult ImplementationVisitor::Visit(CallMethodExpression* expr) {
  StackScope scope(this);
  Arguments arguments;
  std::string method_name = expr->method->name->value;
  TypeVector specialization_types =
      TypeVisitor::ComputeTypeVector(expr->method->generic_arguments);
  LocationReference target = GetLocationReference(expr->target);
  if (!target.IsVariableAccess()) {
    VisitResult result = GenerateFetchFromLocation(target);
    target = LocationReference::Temporary(result, "method target result");
  }
  const AggregateType* target_type =
      AggregateType::DynamicCast(target.ReferencedType());
  if (!target_type) {
    ReportError("target of method call not a struct or class type");
  }
  for (Expression* arg : expr->arguments) {
    arguments.parameters.push_back(Visit(arg));
  }
  arguments.labels = LabelsFromIdentifiers(expr->labels);
  TypeVector argument_types = arguments.parameters.ComputeTypeVector();
  DCHECK_EQ(expr->method->namespace_qualification.size(), 0);
  QualifiedName qualified_name = QualifiedName(method_name);
  Callable* callable = nullptr;
  callable = LookupMethod(method_name, target_type, arguments, {});
  if (GlobalContext::collect_language_server_data()) {
    LanguageServerData::AddDefinition(expr->method->name->pos,
                                      callable->IdentifierPosition());
  }
  return scope.Yield(GenerateCall(callable, target, arguments, {}, false));
}

VisitResult ImplementationVisitor::Visit(IntrinsicCallExpression* expr) {
  StackScope scope(this);
  Arguments arguments;
  TypeVector specialization_types =
      TypeVisitor::ComputeTypeVector(expr->generic_arguments);
  for (Expression* arg : expr->arguments)
    arguments.parameters.push_back(Visit(arg));
  return scope.Yield(
      GenerateCall(expr->name->value, arguments, specialization_types, false));
}

void ImplementationVisitor::GenerateBranch(const VisitResult& condition,
                                           Block* true_block,
                                           Block* false_block) {
  DCHECK_EQ(condition,
            VisitResult(TypeOracle::GetBoolType(), assembler().TopRange(1)));
  assembler().Branch(true_block, false_block);
}

VisitResult ImplementationVisitor::GenerateBoolConstant(bool constant) {
  return GenerateImplicitConvert(TypeOracle::GetBoolType(),
                                 VisitResult(TypeOracle::GetConstexprBoolType(),
                                             constant ? "true" : "false"));
}

void ImplementationVisitor::GenerateExpressionBranch(Expression* expression,
                                                     Block* true_block,
                                                     Block* false_block) {
  StackScope stack_scope(this);
  VisitResult expression_result = this->Visit(expression);
  expression_result = stack_scope.Yield(
      GenerateImplicitConvert(TypeOracle::GetBoolType(), expression_result));
  GenerateBranch(expression_result, true_block, false_block);
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

  if (auto from = TypeOracle::ImplicitlyConvertableFrom(destination_type,
                                                        source.type())) {
    return scope.Yield(GenerateCall(kFromConstexprMacroName,
                                    Arguments{{source}, {}},
                                    {destination_type, *from}, false));
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
    const std::vector<Identifier*>& names) {
  std::vector<Binding<LocalLabel>*> result;
  result.reserve(names.size());
  for (const auto& name : names) {
    Binding<LocalLabel>* label = LookupLabel(name->value);
    result.push_back(label);

    // Link up labels in "otherwise" part of the call expression with
    // either the label in the signature of the calling macro or the label
    // block ofa surrounding "try".
    if (GlobalContext::collect_language_server_data()) {
      LanguageServerData::AddDefinition(name->pos,
                                        label->declaration_position());
    }
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

void ImplementationVisitor::LowerLabelParameter(
    const Type* type, const std::string& parameter_name,
    std::vector<std::string>* lowered_parameters) {
  if (const StructType* struct_type = StructType::DynamicCast(type)) {
    for (auto& field : struct_type->fields()) {
      LowerLabelParameter(
          field.name_and_type.type,
          "&((*" + parameter_name + ")." + field.name_and_type.name + ")",
          lowered_parameters);
    }
  } else {
    lowered_parameters->push_back(parameter_name);
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

DEFINE_CONTEXTUAL_VARIABLE(ImplementationVisitor::ValueBindingsManager)
DEFINE_CONTEXTUAL_VARIABLE(ImplementationVisitor::LabelBindingsManager)
DEFINE_CONTEXTUAL_VARIABLE(ImplementationVisitor::CurrentCallable)
DEFINE_CONTEXTUAL_VARIABLE(ImplementationVisitor::CurrentFileStreams)
DEFINE_CONTEXTUAL_VARIABLE(ImplementationVisitor::CurrentReturnValue)

bool IsCompatibleSignature(const Signature& sig, const TypeVector& types,
                           size_t label_count) {
  auto i = sig.parameter_types.types.begin() + sig.implicit_count;
  if ((sig.parameter_types.types.size() - sig.implicit_count) > types.size())
    return false;
  if (sig.labels.size() != label_count) return false;
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
          TryLookupLabel(kCatchLabelName)) {
    catch_block = assembler().NewBlock(base::nullopt, true);
  }
  return catch_block;
}

void ImplementationVisitor::GenerateCatchBlock(
    base::Optional<Block*> catch_block) {
  if (catch_block) {
    base::Optional<Binding<LocalLabel>*> catch_handler =
        TryLookupLabel(kCatchLabelName);
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
  CurrentCallable::Scope current_callable(nullptr);
  const std::vector<std::unique_ptr<Declarable>>& all_declarables =
      GlobalContext::AllDeclarables();
  // This has to be an index-based loop because all_declarables can be extended
  // during the loop.
  for (size_t i = 0; i < all_declarables.size(); ++i) {
    try {
      Visit(all_declarables[i].get());
    } catch (TorqueAbortCompilation&) {
      // Recover from compile errors here. The error is recorded already.
    }
  }
}

void ImplementationVisitor::Visit(Declarable* declarable) {
  CurrentScope::Scope current_scope(declarable->ParentScope());
  CurrentSourcePosition::Scope current_source_position(declarable->Position());
  CurrentFileStreams::Scope current_file_streams(
      &GlobalContext::GeneratedPerFile(declarable->Position().source));
  if (Callable* callable = Callable::DynamicCast(declarable)) {
    if (!callable->ShouldGenerateExternalCode())
      CurrentFileStreams::Get() = nullptr;
  }
  switch (declarable->kind()) {
    case Declarable::kExternMacro:
      return Visit(ExternMacro::cast(declarable));
    case Declarable::kTorqueMacro:
      return Visit(TorqueMacro::cast(declarable));
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
    case Declarable::kGenericCallable:
    case Declarable::kGenericType:
      return;
  }
}

std::string MachineTypeString(const Type* type) {
  if (type->IsSubtypeOf(TypeOracle::GetSmiType())) {
    return "MachineType::TaggedSigned()";
  }
  if (type->IsSubtypeOf(TypeOracle::GetHeapObjectType())) {
    return "MachineType::TaggedPointer()";
  }
  if (type->IsSubtypeOf(TypeOracle::GetTaggedType())) {
    return "MachineType::AnyTagged()";
  }
  return "MachineTypeOf<" + type->GetGeneratedTNodeTypeName() + ">::value";
}

void ImplementationVisitor::GenerateBuiltinDefinitionsAndInterfaceDescriptors(
    const std::string& output_directory) {
  std::stringstream builtin_definitions;
  std::string builtin_definitions_file_name = "builtin-definitions-tq.h";

  // This file contains plain interface descriptor definitions and has to be
  // included in the middle of interface-descriptors.h. Thus it is not a normal
  // header file and uses the .inc suffix instead of the .h suffix.
  std::stringstream interface_descriptors;
  std::string interface_descriptors_file_name = "interface-descriptors-tq.inc";
  {
    IncludeGuardScope builtin_definitions_include_guard(
        builtin_definitions, builtin_definitions_file_name);

    builtin_definitions
        << "\n"
           "#define BUILTIN_LIST_FROM_TORQUE(CPP, TFJ, TFC, TFS, TFH, "
           "ASM) "
           "\\\n";
    for (auto& declarable : GlobalContext::AllDeclarables()) {
      Builtin* builtin = Builtin::DynamicCast(declarable.get());
      if (!builtin || builtin->IsExternal()) continue;
      if (builtin->IsStub()) {
        builtin_definitions << "TFC(" << builtin->ExternalName() << ", "
                            << builtin->ExternalName();
        std::string descriptor_name = builtin->ExternalName() + "Descriptor";
        constexpr size_t kFirstNonContextParameter = 1;
        size_t parameter_count =
            builtin->parameter_names().size() - kFirstNonContextParameter;

        interface_descriptors << "class " << descriptor_name
                              << " : public TorqueInterfaceDescriptor<"
                              << parameter_count << "> {\n";
        interface_descriptors << "  DECLARE_DESCRIPTOR_WITH_BASE("
                              << descriptor_name
                              << ", TorqueInterfaceDescriptor)\n";

        interface_descriptors << "  MachineType ReturnType() override {\n";
        interface_descriptors
            << "    return "
            << MachineTypeString(builtin->signature().return_type) << ";\n";
        interface_descriptors << "  }\n";

        interface_descriptors << "  std::array<MachineType, " << parameter_count
                              << "> ParameterTypes() override {\n";
        interface_descriptors << "    return {";
        for (size_t i = kFirstNonContextParameter;
             i < builtin->parameter_names().size(); ++i) {
          bool last = i + 1 == builtin->parameter_names().size();
          const Type* type = builtin->signature().parameter_types.types[i];
          interface_descriptors << MachineTypeString(type)
                                << (last ? "" : ", ");
        }
        interface_descriptors << "};\n";

        interface_descriptors << "  }\n";
        interface_descriptors << "};\n\n";
      } else {
        builtin_definitions << "TFJ(" << builtin->ExternalName();
        if (builtin->IsVarArgsJavaScript()) {
          builtin_definitions << ", kDontAdaptArgumentsSentinel";
        } else {
          DCHECK(builtin->IsFixedArgsJavaScript());
          // FixedArg javascript builtins need to offer the parameter
          // count.
          int parameter_count =
              static_cast<int>(builtin->signature().ExplicitCount());
          builtin_definitions << ", " << parameter_count;
          // And the receiver is explicitly declared.
          builtin_definitions << ", kReceiver";
          for (size_t i = builtin->signature().implicit_count;
               i < builtin->parameter_names().size(); ++i) {
            Identifier* parameter = builtin->parameter_names()[i];
            builtin_definitions << ", k" << CamelifyString(parameter->value);
          }
        }
      }
      builtin_definitions << ") \\\n";
    }
    builtin_definitions << "\n";

    builtin_definitions
        << "#define TORQUE_FUNCTION_POINTER_TYPE_TO_BUILTIN_MAP(V) \\\n";
    for (const BuiltinPointerType* type :
         TypeOracle::AllBuiltinPointerTypes()) {
      Builtin* example_builtin =
          Declarations::FindSomeInternalBuiltinWithType(type);
      if (!example_builtin) {
        CurrentSourcePosition::Scope current_source_position(
            SourcePosition{CurrentSourceFile::Get(), {-1, -1}, {-1, -1}});
        ReportError("unable to find any builtin with type \"", *type, "\"");
      }
      builtin_definitions << "  V(" << type->function_pointer_type_id() << ","
                          << example_builtin->ExternalName() << ")\\\n";
    }
    builtin_definitions << "\n";
  }
  WriteFile(output_directory + "/" + builtin_definitions_file_name,
            builtin_definitions.str());
  WriteFile(output_directory + "/" + interface_descriptors_file_name,
            interface_descriptors.str());
}

namespace {

enum class FieldSectionType : uint32_t {
  kNoSection = 0,
  kWeakSection = 1 << 0,
  kStrongSection = 2 << 0,
  kScalarSection = 3 << 0
};

bool IsPointerSection(FieldSectionType type) {
  return type == FieldSectionType::kWeakSection ||
         type == FieldSectionType::kStrongSection;
}

using FieldSections = base::Flags<FieldSectionType>;

std::string ToString(FieldSectionType type) {
  switch (type) {
    case FieldSectionType::kNoSection:
      return "NoSection";
      break;
    case FieldSectionType::kWeakSection:
      return "WeakFields";
      break;
    case FieldSectionType::kStrongSection:
      return "StrongFields";
      break;
    case FieldSectionType::kScalarSection:
      return "ScalarFields";
      break;
  }
  UNREACHABLE();
}

class FieldOffsetsGenerator {
 public:
  explicit FieldOffsetsGenerator(const ClassType* type) : type_(type) {}

  virtual void WriteField(const Field& f, const std::string& size_string) = 0;
  virtual void WriteMarker(const std::string& marker) = 0;

  virtual ~FieldOffsetsGenerator() { CHECK(is_finished_); }

  void RecordOffsetFor(const Field& f) {
    CHECK(!is_finished_);
    UpdateSection(f);

    // Emit kHeaderSize before any indexed field.
    if (f.index.has_value() && !header_size_emitted_) {
      WriteMarker("kHeaderSize");
      header_size_emitted_ = true;
    }

    // We don't know statically how much space an indexed field takes, so report
    // it as zero.
    std::string size_string = "0";
    if (!f.index.has_value()) {
      size_t field_size;
      std::tie(field_size, size_string) = f.GetFieldSizeInformation();
    }
    WriteField(f, size_string);
  }

  void Finish() {
    End(current_section_);
    if (!(completed_sections_ & FieldSectionType::kWeakSection)) {
      Begin(FieldSectionType::kWeakSection);
      End(FieldSectionType::kWeakSection);
    }
    if (!(completed_sections_ & FieldSectionType::kStrongSection)) {
      Begin(FieldSectionType::kStrongSection);
      End(FieldSectionType::kStrongSection);
    }
    is_finished_ = true;

    // In the presence of indexed fields, we already emitted kHeaderSize before
    // the indexed field.
    if (!type_->IsShape() && !type_->HasIndexedField()) {
      WriteMarker("kHeaderSize");
    }
    if (type_->HasStaticSize()) {
      WriteMarker("kSize");
    }
  }

 protected:
  const ClassType* type_;

 private:
  FieldSectionType GetSectionFor(const Field& f) {
    const Type* field_type = f.name_and_type.type;
    if (field_type == TypeOracle::GetVoidType()) {
      // Allow void type for marker constants of size zero.
      return current_section_;
    }
    StructType::Classification struct_contents =
        StructType::ClassificationFlag::kEmpty;
    if (const StructType* field_as_struct =
            StructType::DynamicCast(field_type)) {
      struct_contents = field_as_struct->ClassifyContents();
    }
    if (struct_contents == StructType::ClassificationFlag::kMixed) {
      // We can't declare what section a struct goes in if it has multiple
      // categories of data within.
      Error(
          "Classes do not support fields which are structs containing both "
          "tagged and untagged data.")
          .Position(f.pos);
    }
    // Currently struct-valued fields are only allowed to have tagged data; see
    // TypeVisitor::VisitClassFieldsAndMethods.
    if (field_type->IsSubtypeOf(TypeOracle::GetTaggedType()) ||
        struct_contents == StructType::ClassificationFlag::kTagged) {
      if (f.is_weak) {
        return FieldSectionType::kWeakSection;
      } else {
        return FieldSectionType::kStrongSection;
      }
    } else {
      return FieldSectionType::kScalarSection;
    }
  }
  void UpdateSection(const Field& f) {
    FieldSectionType type = GetSectionFor(f);
    if (current_section_ == type) return;
    if (IsPointerSection(type)) {
      if (completed_sections_ & type) {
        std::stringstream s;
        s << "cannot declare field " << f.name_and_type.name << " in class "
          << type_->name() << ", because section " << ToString(type)
          << " to which it belongs has already been finished.";
        Error(s.str()).Position(f.pos);
      }
    }
    End(current_section_);
    current_section_ = type;
    Begin(current_section_);
  }
  void Begin(FieldSectionType type) {
    DCHECK(type != FieldSectionType::kNoSection);
    if (!IsPointerSection(type)) return;
    WriteMarker("kStartOf" + ToString(type) + "Offset");
  }
  void End(FieldSectionType type) {
    if (!IsPointerSection(type)) return;
    completed_sections_ |= type;
    WriteMarker("kEndOf" + ToString(type) + "Offset");
  }

  FieldSectionType current_section_ = FieldSectionType::kNoSection;
  FieldSections completed_sections_ = FieldSectionType::kNoSection;
  bool is_finished_ = false;
  bool header_size_emitted_ = false;
};

class MacroFieldOffsetsGenerator : public FieldOffsetsGenerator {
 public:
  MacroFieldOffsetsGenerator(std::ostream& out, const ClassType* type)
      : FieldOffsetsGenerator(type), out_(out) {
    out_ << "#define ";
    out_ << "TORQUE_GENERATED_" << CapifyStringWithUnderscores(type_->name())
         << "_FIELDS(V) \\\n";
  }
  void WriteField(const Field& f, const std::string& size_string) override {
    out_ << "V(k" << CamelifyString(f.name_and_type.name) << "Offset, "
         << size_string << ") \\\n";
  }
  void WriteMarker(const std::string& marker) override {
    out_ << "V(" << marker << ", 0) \\\n";
  }

 private:
  std::ostream& out_;
};

void GenerateClassExport(const ClassType* type, std::ostream& header,
                         std::ostream& inl_header) {
  const ClassType* super = type->GetSuperClass();
  std::string parent = "TorqueGenerated" + type->name() + "<" + type->name() +
                       ", " + super->name() + ">";
  header << "class " << type->name() << " : public " << parent << " {\n";
  header << " public:\n";
  if (type->ShouldGenerateBodyDescriptor()) {
    header << "  class BodyDescriptor;\n";
  }
  header << "  TQ_OBJECT_CONSTRUCTORS(" << type->name() << ")\n";
  header << "};\n\n";
  inl_header << "TQ_OBJECT_CONSTRUCTORS_IMPL(" << type->name() << ")\n";
}

}  // namespace

void ImplementationVisitor::GenerateClassFieldOffsets(
    const std::string& output_directory) {
  std::stringstream header;
  std::string file_name = "field-offsets-tq.h";
  {
    IncludeGuardScope include_guard(header, file_name);

    for (const ClassType* type : TypeOracle::GetClasses()) {
      // TODO(danno): Remove this once all classes use ClassFieldOffsetGenerator
      // to generate field offsets without the use of macros.
      if (!type->GenerateCppClassDefinitions() && !type->HasUndefinedLayout()) {
        MacroFieldOffsetsGenerator g(header, type);
        for (auto f : type->fields()) {
          CurrentSourcePosition::Scope scope(f.pos);
          g.RecordOffsetFor(f);
        }
        g.Finish();
        header << "\n";
      }
    }

    header << "#define TORQUE_BODY_DESCRIPTOR_LIST_GENERATOR(V, _)\\\n";
    for (const ClassType* type : TypeOracle::GetClasses()) {
      if (type->ShouldGenerateBodyDescriptor()) {
        std::string type_name =
            CapifyStringWithUnderscores(type->name()) + "_TYPE";
        header << "V(_, " << type_name << ", " << type->name() << ")\\\n";
      }
    }
    header << "\n";
  }
  const std::string output_header_path = output_directory + "/" + file_name;
  WriteFile(output_header_path, header.str());
}

void ImplementationVisitor::GenerateBitFields(
    const std::string& output_directory) {
  std::stringstream header;
  std::string file_name = "bit-fields-tq.h";
  {
    IncludeGuardScope include_guard(header, file_name);
    header << "#include \"src/base/bit-field.h\"\n\n";
    NamespaceScope namespaces(header, {"v8", "internal"});

    for (const auto& type : TypeOracle::GetBitFieldStructTypes()) {
      bool all_single_bits = true;  // Track whether every field is one bit.

      header << "#define DEFINE_TORQUE_GENERATED_"
             << CapifyStringWithUnderscores(type->name()) << "() \\\n";
      std::string type_name = type->GetConstexprGeneratedTypeName();
      for (const auto& field : type->fields()) {
        const char* suffix = field.num_bits == 1 ? "Bit" : "Bits";
        all_single_bits = all_single_bits && field.num_bits == 1;
        std::string field_type_name =
            field.name_and_type.type->GetConstexprGeneratedTypeName();
        header << "  using " << CamelifyString(field.name_and_type.name)
               << suffix << " = base::BitField<" << field_type_name << ", "
               << field.offset << ", " << field.num_bits << ", " << type_name
               << ">; \\\n";
      }

      // If every field is one bit, we can also generate a convenient enum.
      if (all_single_bits) {
        header << "  enum Flag { \\\n";
        header << "    kNone = 0, \\\n";
        for (const auto& field : type->fields()) {
          header << "    k" << CamelifyString(field.name_and_type.name)
                 << " = 1 << " << field.offset << ", \\\n";
        }
        header << "  }; \\\n";
      }

      header << "\n";
    }
  }
  const std::string output_header_path = output_directory + "/" + file_name;
  WriteFile(output_header_path, header.str());
}

namespace {

class ClassFieldOffsetGenerator : public FieldOffsetsGenerator {
 public:
  ClassFieldOffsetGenerator(std::ostream& header, const ClassType* type)
      : FieldOffsetsGenerator(type),
        hdr_(header),
        previous_field_end_("P::kHeaderSize") {}
  void WriteField(const Field& f, const std::string& size_string) override {
    std::string field = "k" + CamelifyString(f.name_and_type.name) + "Offset";
    std::string field_end = field + "End";
    hdr_ << "  static constexpr int " << field << " = " << previous_field_end_
         << ";\n";
    hdr_ << "  static constexpr int " << field_end << " = " << field << " + "
         << size_string << " - 1;\n";
    previous_field_end_ = field_end + " + 1";
  }
  void WriteMarker(const std::string& marker) override {
    hdr_ << "  static constexpr int " << marker << " = " << previous_field_end_
         << ";\n";
  }

 private:
  std::ostream& hdr_;
  std::string previous_field_end_;
};

class CppClassGenerator {
 public:
  CppClassGenerator(const ClassType* type, std::ostream& header,
                    std::ostream& inl_header, std::ostream& impl)
      : type_(type),
        super_(type->GetSuperClass()),
        name_(type->name()),
        gen_name_("TorqueGenerated" + name_),
        gen_name_T_(gen_name_ + "<D, P>"),
        gen_name_I_(gen_name_ + "<" + name_ + ", " + super_->name() + ">"),
        hdr_(header),
        inl_(inl_header),
        impl_(impl) {}
  const std::string template_decl() const {
    return "template <class D, class P>";
  }

  void GenerateClass();

 private:
  void GenerateClassConstructors();
  void GenerateFieldAccessor(const Field& f);
  void GenerateFieldAccessorForUntagged(const Field& f);
  void GenerateFieldAccessorForSmi(const Field& f);
  void GenerateFieldAccessorForObject(const Field& f);

  void GenerateClassCasts();

  const ClassType* type_;
  const ClassType* super_;
  const std::string name_;
  const std::string gen_name_;
  const std::string gen_name_T_;
  const std::string gen_name_I_;
  std::ostream& hdr_;
  std::ostream& inl_;
  std::ostream& impl_;
};

base::Optional<std::vector<Field>> GetOrderedUniqueIndexFields(
    const ClassType& type) {
  std::vector<Field> result;
  std::set<std::string> index_names;
  for (const Field& field : type.ComputeAllFields()) {
    if (field.index) {
      auto name_and_type = ExtractSimpleFieldArraySize(type, *field.index);
      if (!name_and_type) {
        return base::nullopt;
      }
      index_names.insert(name_and_type->name);
    }
  }

  for (const Field& field : type.ComputeAllFields()) {
    if (index_names.count(field.name_and_type.name) != 0) {
      result.push_back(field);
    }
  }

  return result;
}

void CppClassGenerator::GenerateClass() {
  hdr_ << template_decl() << "\n";
  hdr_ << "class " << gen_name_ << " : public P {\n";
  hdr_ << "  static_assert(std::is_same<" << name_ << ", D>::value,\n"
       << "    \"Use this class as direct base for " << name_ << ".\");\n";
  hdr_ << "  static_assert(std::is_same<" << super_->name() << ", P>::value,\n"
       << "    \"Pass in " << super_->name()
       << " as second template parameter for " << gen_name_ << ".\");\n";
  hdr_ << " public: \n";
  hdr_ << "  using Super = P;\n\n";
  if (!type_->ShouldExport() && !type_->IsExtern()) {
    hdr_ << " protected: // not extern or @export\n";
  }
  for (const Field& f : type_->fields()) {
    GenerateFieldAccessor(f);
  }
  if (!type_->ShouldExport() && !type_->IsExtern()) {
    hdr_ << " public:\n";
  }

  GenerateClassCasts();

  if (type_->ShouldGeneratePrint()) {
    hdr_ << "\n  DECL_PRINTER(" << name_ << ")\n";
  }

  if (type_->ShouldGenerateVerify()) {
    IfDefScope hdr_scope(hdr_, "VERIFY_HEAP");
    hdr_ << "  V8_EXPORT_PRIVATE void " << name_
         << "Verify(Isolate* isolate);\n";

    IfDefScope impl_scope(impl_, "VERIFY_HEAP");
    impl_ << "\ntemplate <>\n";
    impl_ << "void " << gen_name_I_ << "::" << name_
          << "Verify(Isolate* isolate) {\n";
    impl_ << "  TorqueGeneratedClassVerifiers::" << name_ << "Verify(" << name_
          << "::cast(*this), "
             "isolate);\n";
    impl_ << "}\n";
  }

  hdr_ << "\n";
  ClassFieldOffsetGenerator g(hdr_, type_);
  for (auto f : type_->fields()) {
    CurrentSourcePosition::Scope scope(f.pos);
    g.RecordOffsetFor(f);
  }
  g.Finish();
  hdr_ << "\n";

  auto index_fields = GetOrderedUniqueIndexFields(*type_);

  if (!index_fields.has_value()) {
    hdr_ << "  // SizeFor implementations not generated due to complex array "
            "lengths\n\n";
  } else if (!type_->IsAbstract() &&
             !type_->IsSubtypeOf(TypeOracle::GetJSObjectType())) {
    hdr_ << "  V8_INLINE static constexpr int32_t SizeFor(";
    bool first = true;
    for (const Field& field : *index_fields) {
      if (!first) hdr_ << ", ";
      hdr_ << field.name_and_type.type->HandlifiedCppTypeName() << " "
           << field.name_and_type.name;
      first = false;
    }
    hdr_ << ") {\n";
    if (index_fields->empty()) {
      hdr_ << "    DCHECK(kHeaderSize == kSize && kHeaderSize == "
           << *type_->size().SingleValue() << ");\n";
    }
    hdr_ << "    int32_t size = kHeaderSize;\n";
    for (const Field& field : type_->ComputeAllFields()) {
      if (field.index) {
        auto index_name_and_type =
            *ExtractSimpleFieldArraySize(*type_, *field.index);
        size_t field_size = 0;
        std::tie(field_size, std::ignore) = field.GetFieldSizeInformation();
        hdr_ << "    size += ";
        auto index_field = std::find_if(
            index_fields->begin(), index_fields->end(),
            [&](const Field& field) {
              return field.name_and_type.name == index_name_and_type.name;
            });
        if (index_field->name_and_type.type->IsSubtypeOf(
                TypeOracle::GetSmiType())) {
          hdr_ << "Smi::ToInt(" << index_name_and_type.name << ")";
        } else {
          hdr_ << index_name_and_type.name;
        }
        hdr_ << " * " << field_size << ";\n";
      }
    }
    hdr_ << "    return size;\n";
    hdr_ << "  }\n\n";
    hdr_ << "  V8_INLINE static constexpr int32_t SizeFor(D o) {\n";
    hdr_ << "    return SizeFor(";
    first = true;
    for (auto field : *index_fields) {
      if (!first) hdr_ << ", ";
      // Subclasses of FixedArrayBase need to use the synchronized length
      // accessor to be consistent (theoretically, FixedArrayBase classes
      // can concurrently change size e.g. through left-trimming, although
      // in practice this won't happen for Torque-generated classes) as well as
      // explicitly convert to a Smi, since the C++-side accessors are
      // int-based.
      if (field.aggregate == TypeOracle::GetFixedArrayBaseType() &&
          field.name_and_type.name == "length") {
        hdr_ << "Smi::FromInt(o.synchronized_length())";
      } else {
        hdr_ << "o." << field.name_and_type.name << "()";
      }
      first = false;
    }
    hdr_ << ");\n  }\n";
    hdr_ << "\n";
  }

  hdr_ << "  friend class Factory;\n\n";

  GenerateClassConstructors();

  hdr_ << "};\n\n";

  if (!type_->IsExtern()) {
    GenerateClassExport(type_, hdr_, inl_);
  }
}

void CppClassGenerator::GenerateClassCasts() {
  hdr_ << "  V8_INLINE static D cast(Object object) {\n";
  hdr_ << "    return D(object.ptr());\n";
  hdr_ << "  }\n";

  hdr_ << "  V8_INLINE static D unchecked_cast(Object object) {\n";
  hdr_ << "    return bit_cast<D>(object);\n";
  hdr_ << "  }\n";
}

void CppClassGenerator::GenerateClassConstructors() {
  hdr_ << " public:\n";
  hdr_ << "  template <class DAlias = D>\n";
  hdr_ << "  constexpr " << gen_name_ << "() : P() {\n";
  hdr_ << "    static_assert(std::is_base_of<" << gen_name_ << ", \n";
  hdr_ << "      DAlias>::value,\n";
  hdr_ << "      \"class " << gen_name_ << " should be used as direct base for "
       << name_ << ".\");\n";
  hdr_ << "  }\n";

  hdr_ << "protected:\n";
  hdr_ << "  inline explicit " << gen_name_ << "(Address ptr);\n";

  inl_ << "template<class D, class P>\n";
  inl_ << "inline " << gen_name_T_ << "::" << gen_name_ << "(Address ptr)\n";
  inl_ << "  : P(ptr) {\n";
  inl_ << "  SLOW_DCHECK(this->Is" << name_ << "());\n";
  inl_ << "}\n";
}

// TODO(sigurds): Keep in sync with DECL_ACCESSORS and ACCESSORS macro.
void CppClassGenerator::GenerateFieldAccessor(const Field& f) {
  const Type* field_type = f.name_and_type.type;
  if (field_type == TypeOracle::GetVoidType()) return;

  // TODO(danno): Support generation of struct accessors
  if (f.name_and_type.type->IsStructType()) return;

  if (!f.name_and_type.type->IsSubtypeOf(TypeOracle::GetTaggedType())) {
    return GenerateFieldAccessorForUntagged(f);
  }
  if (f.name_and_type.type->IsSubtypeOf(TypeOracle::GetSmiType())) {
    return GenerateFieldAccessorForSmi(f);
  }
  if (f.name_and_type.type->IsSubtypeOf(TypeOracle::GetObjectType())) {
    return GenerateFieldAccessorForObject(f);
  }

  Error("Generation of field accessor for ", type_->name(),
        "::", f.name_and_type.name, " failed (type ", *field_type,
        " is not supported).")
      .Position(f.pos);
}

void CppClassGenerator::GenerateFieldAccessorForUntagged(const Field& f) {
  DCHECK(!f.name_and_type.type->IsSubtypeOf(TypeOracle::GetTaggedType()));
  const Type* field_type = f.name_and_type.type;
  if (field_type == TypeOracle::GetVoidType()) return;
  const Type* constexpr_version = field_type->ConstexprVersion();
  if (!constexpr_version) {
    Error("Field accessor for ", type_->name(), ":: ", f.name_and_type.name,
          " cannot be generated because its type ", *field_type,
          " is neither a subclass of Object nor does the type have a constexpr "
          "version.")
        .Position(f.pos);
    return;
  }
  const std::string& name = f.name_and_type.name;
  const std::string type = constexpr_version->GetGeneratedTypeName();
  std::string offset = "k" + CamelifyString(name) + "Offset";

  // Generate declarations in header.
  if (f.index) {
    hdr_ << "  inline " << type << " " << name << "(int i) const;\n";
    hdr_ << "  inline void set_" << name << "(int i, " << type
         << " value);\n\n";
  } else {
    hdr_ << "  inline " << type << " " << name << "() const;\n";
    hdr_ << "  inline void set_" << name << "(" << type << " value);\n\n";
  }

  // Generate implementation in inline header.
  inl_ << "template <class D, class P>\n";
  inl_ << type << " " << gen_name_ << "<D, P>::" << name << "(";
  if (f.index) {
    inl_ << "int i";
  }
  inl_ << ") const {\n";
  if (f.index) {
    size_t field_size;
    std::string size_string;
    std::tie(field_size, size_string) = f.GetFieldSizeInformation();
    inl_ << "  int offset = " << offset << " + i * " << field_size << ";\n";
    inl_ << "  return this->template ReadField<" << type << ">(offset);\n";
  } else {
    inl_ << "  return this->template ReadField<" << type << ">(" << offset
         << ");\n";
  }
  inl_ << "}\n";

  inl_ << "template <class D, class P>\n";
  inl_ << "void " << gen_name_ << "<D, P>::set_" << name << "(";
  if (f.index) {
    inl_ << "int i, ";
  }
  inl_ << type << " value) {\n";
  if (f.index) {
    size_t field_size;
    std::string size_string;
    std::tie(field_size, size_string) = f.GetFieldSizeInformation();
    inl_ << "  int offset = " << offset << " + i * " << field_size << ";\n";
    inl_ << "  this->template WriteField<" << type << ">(offset, value);\n";
  } else {
    inl_ << "  this->template WriteField<" << type << ">(" << offset
         << ", value);\n";
  }
  inl_ << "}\n\n";
}

void CppClassGenerator::GenerateFieldAccessorForSmi(const Field& f) {
  DCHECK(f.name_and_type.type->IsSubtypeOf(TypeOracle::GetSmiType()));
  const std::string type = "Smi";
  const std::string& name = f.name_and_type.name;
  const std::string offset = "k" + CamelifyString(name) + "Offset";

  // Generate declarations in header.
  if (f.index) {
    hdr_ << "  inline " << type << " " << name << "(int i) const;\n";
    hdr_ << "  inline void set_" << name << "(int i, " << type
         << " value);\n\n";
  }
  hdr_ << "  inline " << type << " " << name << "() const;\n";
  hdr_ << "  inline void set_" << name << "(" << type << " value);\n\n";

  // Generate implementation in inline header.
  inl_ << "template <class D, class P>\n";
  inl_ << type << " " << gen_name_ << "<D, P>::" << name << "(";
  if (f.index) {
    inl_ << "int i";
  }
  inl_ << ") const {\n";
  if (f.index) {
    inl_ << "  int offset = " << offset << " + i * kTaggedSize;\n";
    inl_ << "  return this->template ReadField<Smi>(offset);\n";
    inl_ << "}\n";
  } else {
    inl_ << "  return TaggedField<Smi, " << offset << ">::load(*this);\n";
    inl_ << "}\n";
  }

  inl_ << "template <class D, class P>\n";
  inl_ << "void " << gen_name_ << "<D, P>::set_" << name << "(";
  if (f.index) {
    inl_ << "int i, ";
  }
  inl_ << type << " value) {\n";
  inl_ << "  DCHECK(value.IsSmi());\n";
  if (f.index) {
    inl_ << "  int offset = " << offset << " + i * kTaggedSize;\n";
    inl_ << "  WRITE_FIELD(*this, offset, value);\n";
  } else {
    inl_ << "  WRITE_FIELD(*this, " << offset << ", value);\n";
  }
  inl_ << "}\n\n";
}

void CppClassGenerator::GenerateFieldAccessorForObject(const Field& f) {
  const Type* field_type = f.name_and_type.type;
  DCHECK(field_type->IsSubtypeOf(TypeOracle::GetObjectType()));
  const std::string& name = f.name_and_type.name;
  const std::string offset = "k" + CamelifyString(name) + "Offset";
  base::Optional<const ClassType*> class_type = field_type->ClassSupertype();

  std::string type =
      class_type ? (*class_type)->GetGeneratedTNodeTypeName() : "Object";

  // Generate declarations in header.
  if (!class_type && field_type != TypeOracle::GetObjectType()) {
    hdr_ << "  // Torque type: " << field_type->ToString() << "\n";
  }

  hdr_ << "  inline " << type << " " << name << "(" << (f.index ? "int i" : "")
       << ") const;\n";
  hdr_ << "  inline " << type << " " << name << "(const Isolate* isolates"
       << (f.index ? ", int i" : "") << ") const;\n";
  hdr_ << "  inline void set_" << name << "(" << (f.index ? "int i, " : "")
       << type << " value, WriteBarrierMode mode = UPDATE_WRITE_BARRIER);\n\n";

  std::string type_check;
  for (const RuntimeType& runtime_type : field_type->GetRuntimeTypes()) {
    if (!type_check.empty()) type_check += " || ";
    type_check += "value.Is" + runtime_type.type + "()";
  }

  // Generate implementation in inline header.
  inl_ << "template <class D, class P>\n";
  inl_ << type << " " << gen_name_ << "<D, P>::" << name << "("
       << (f.index ? "int i" : "") << ") const {\n";
  inl_ << "  const Isolate* isolate = GetIsolateForPtrCompr(*this);\n";
  inl_ << "  return " << gen_name_ << "::" << name << "(isolate"
       << (f.index ? ", i" : "") << ");\n";
  inl_ << "}\n";

  inl_ << "template <class D, class P>\n";
  inl_ << type << " " << gen_name_ << "<D, P>::" << name
       << "(const Isolate* isolate" << (f.index ? ", int i" : "")
       << ") const {\n";

  if (class_type) {
    if (f.index) {
      inl_ << "  int offset = " << offset << " + i * kTaggedSize;\n";
      inl_ << "  return " << type
           << "::cast(RELAXED_READ_FIELD(*this, offset));\n";
    } else {
      inl_ << "  return TaggedField<" << type << ", " << offset
           << ">::load(isolate, *this);\n";
    }
  } else {
    // TODO(tebbi): load value as HeapObject when possible
    if (f.index) {
      inl_ << "  int offset = " << offset << " + i * kTaggedSize;\n";
      inl_ << "  Object value = Object::cast(RELAXED_READ_FIELD(*this, "
              "offset));\n";
    } else {
      inl_ << "  Object value = TaggedField<Object, " << offset
           << ">::load(isolate, *this);\n";
    }
    inl_ << "  DCHECK(" << type_check << ");\n";
    inl_ << "  return value;\n";
  }
  inl_ << "}\n";

  inl_ << "template <class D, class P>\n";
  inl_ << "void " << gen_name_ << "<D, P>::set_" << name << "(";
  if (f.index) {
    inl_ << "int i, ";
  }
  inl_ << type << " value, WriteBarrierMode mode) {\n";
  inl_ << "  SLOW_DCHECK(" << type_check << ");\n";
  if (f.index) {
    inl_ << "  int offset = " << offset << " + i * kTaggedSize;\n";
    inl_ << "  WRITE_FIELD(*this, offset, value);\n";
  } else {
    inl_ << "  WRITE_FIELD(*this, " << offset << ", value);\n";
  }
  inl_ << "  CONDITIONAL_WRITE_BARRIER(*this, " << offset
       << ", value, mode);\n";
  inl_ << "}\n\n";
}

void EmitClassDefinitionHeadersIncludes(const std::string& basename,
                                        std::stringstream& header,
                                        std::stringstream& inline_header) {
  header << "#include \"src/objects/fixed-array.h\"\n";
  header << "#include \"src/objects/objects.h\"\n";
  header << "#include \"src/objects/smi.h\"\n";
  header << "#include \"torque-generated/field-offsets-tq.h\"\n";
  header << "#include <type_traits>\n\n";

  inline_header << "#include \"torque-generated/class-definitions-tq.h\"\n";
  inline_header << "#include "
                   "\"torque-generated/objects-body-descriptors-tq-inl.h\"\n\n";
  inline_header << "#include \"src/objects/js-promise.h\"\n";
  inline_header << "#include \"src/objects/js-weak-refs.h\"\n";
  inline_header << "#include \"src/objects/module.h\"\n";
  inline_header << "#include \"src/objects/objects-inl.h\"\n";
  inline_header << "#include \"src/objects/script.h\"\n";
  inline_header << "#include \"src/objects/shared-function-info.h\"\n";
  inline_header << "#include \"src/objects/tagged-field.h\"\n\n";
}

void EmitClassDefinitionHeadersForwardDeclarations(std::stringstream& header) {
  // Generate forward declarations for every class.
  for (const ClassType* type : TypeOracle::GetClasses()) {
    header << "class " << type->GetGeneratedTNodeTypeName() << ";\n";
  }
  header << "using BuiltinPtr = Smi;\n\n";
}

void GenerateStructLayoutDescription(std::ostream& header,
                                     const StructType* type) {
  header << "struct TorqueGenerated" << CamelifyString(type->name())
         << "Offsets {\n";
  for (const Field& field : type->fields()) {
    header << "  static constexpr int k"
           << CamelifyString(field.name_and_type.name)
           << "Offset = " << *field.offset << ";\n";
  }
  header << "  static constexpr int kSize = " << type->PackedSize() << ";\n";
  header << "};\n\n";
}

}  // namespace

void ImplementationVisitor::GenerateClassDefinitions(
    const std::string& output_directory) {
  std::stringstream external_header;
  std::stringstream inline_external_header;
  std::stringstream internal_header;
  std::stringstream inline_internal_header;
  std::stringstream implementation;
  std::stringstream factory_header;
  std::stringstream factory_impl;
  std::string basename = "class-definitions-tq";
  std::string internal_basename = "internal-" + basename;
  std::string file_basename = output_directory + "/" + basename;
  std::string internal_file_basename =
      output_directory + "/" + internal_basename;
  std::string factory_basename = "factory-tq";
  std::string factory_file_basename = output_directory + "/" + factory_basename;

  {
    IncludeGuardScope header_guard(external_header, basename + ".h");

    IncludeGuardScope inline_header_guard(inline_external_header,
                                          basename + "-inl.h");

    IncludeGuardScope internal_header_guard(internal_header,
                                            internal_basename + ".h");

    IncludeGuardScope internal_inline_header_guard(
        inline_internal_header, internal_basename + "-inl.h");

    external_header
        << "#include \"torque-generated/internal-class-definitions-tq.h\"\n";

    EmitClassDefinitionHeadersIncludes(basename, external_header,
                                       inline_external_header);

    EmitClassDefinitionHeadersIncludes(internal_basename, internal_header,
                                       inline_internal_header);

    IncludeObjectMacrosScope header_macros(external_header);
    IncludeObjectMacrosScope inline_header_macros(inline_external_header);

    IncludeObjectMacrosScope internal_header_macros(internal_header);
    IncludeObjectMacrosScope internal_inline_header_macros(
        inline_internal_header);

    NamespaceScope header_namespaces(external_header, {"v8", "internal"});
    NamespaceScope inline_header_namespaces(inline_external_header,
                                            {"v8", "internal"});
    NamespaceScope internal_header_namespaces(internal_header,
                                              {"v8", "internal"});
    NamespaceScope internal_inline_header_namespaces(inline_internal_header,
                                                     {"v8", "internal"});

    EmitClassDefinitionHeadersForwardDeclarations(external_header);
    EmitClassDefinitionHeadersForwardDeclarations(internal_header);

    factory_impl << "#include \"src/heap/factory.h\"\n";
    factory_impl << "#include \"src/heap/factory-inl.h\"\n";
    factory_impl << "#include \"src/heap/heap.h\"\n";
    factory_impl << "#include \"src/heap/heap-inl.h\"\n";
    factory_impl << "#include \"src/execution/isolate.h\"\n\n";
    factory_impl
        << "#include "
           "\"torque-generated/internal-class-definitions-tq-inl.h\"\n\n";
    NamespaceScope factory_impl_namespaces(factory_impl, {"v8", "internal"});
    factory_impl << "\n";

    implementation
        << "#include \"torque-generated/class-definitions-tq.h\"\n\n";
    implementation << "#include \"torque-generated/class-verifiers-tq.h\"\n\n";
    implementation << "#include \"src/objects/arguments-inl.h\"\n";
    implementation << "#include \"src/objects/js-collection-inl.h\"\n";
    implementation << "#include \"src/objects/embedder-data-array-inl.h\"\n";
    implementation << "#include \"src/objects/js-generator-inl.h\"\n";
    implementation << "#include \"src/objects/js-regexp-inl.h\"\n";
    implementation << "#include \"src/objects/js-weak-refs-inl.h\"\n";
    implementation
        << "#include \"src/objects/js-regexp-string-iterator-inl.h\"\n";
    implementation << "#include \"src/objects/literal-objects-inl.h\"\n";
    implementation << "#include \"src/objects/microtask-inl.h\"\n";
    implementation << "#include \"src/objects/module-inl.h\"\n";
    implementation << "#include \"src/objects/promise-inl.h\"\n";
    implementation
        << "#include \"src/objects/property-descriptor-object-inl.h\"\n";
    implementation << "#include \"src/objects/stack-frame-info-inl.h\"\n";
    implementation << "#include \"src/objects/struct-inl.h\"\n";
    implementation << "#include \"src/objects/template-objects-inl.h\"\n\n";
    implementation
        << "#include "
           "\"torque-generated/internal-class-definitions-tq-inl.h\"\n\n";
    NamespaceScope implementation_namespaces(implementation,
                                             {"v8", "internal"});

    std::set<const StructType*, TypeLess> structs_used_in_classes;

    for (const ClassType* type : TypeOracle::GetClasses()) {
      std::stringstream& header = (type->IsExtern() || type->ShouldExport())
                                      ? external_header
                                      : internal_header;
      std::stringstream& inline_header =
          (type->IsExtern() || type->ShouldExport()) ? inline_external_header
                                                     : inline_internal_header;

      if (type->GenerateCppClassDefinitions()) {
        CppClassGenerator g(type, header, inline_header, implementation);
        g.GenerateClass();
      }
      for (const Field& f : type->fields()) {
        const Type* field_type = f.name_and_type.type;
        if (const StructType* field_as_struct =
                StructType::DynamicCast(field_type)) {
          structs_used_in_classes.insert(field_as_struct);
        }
      }
      if (type->ShouldExport()) {
        factory_header << type->HandlifiedCppTypeName() << " New"
                       << type->name() << "(";
        factory_impl << type->HandlifiedCppTypeName() << " Factory::New"
                     << type->name() << "(";

        for (const Field& f : type->ComputeAllFields()) {
          if (f.name_and_type.name == "map") continue;
          if (!f.index) {
            std::string type_string =
                f.name_and_type.type->HandlifiedCppTypeName();
            factory_header << type_string << " " << f.name_and_type.name
                           << ", ";
            factory_impl << type_string << " " << f.name_and_type.name << ", ";
          }
        }

        factory_header << "AllocationType allocation_type);\n";
        factory_impl << "AllocationType allocation_type) {\n";

        factory_impl << " int size = ";
        const ClassType* super = type->GetSuperClass();
        std::string gen_name = "TorqueGenerated" + type->name();
        std::string gen_name_T =
            gen_name + "<" + type->name() + ", " + super->name() + ">";
        factory_impl << gen_name_T << "::SizeFor(";

        bool first = true;
        auto index_fields = GetOrderedUniqueIndexFields(*type);
        CHECK(index_fields.has_value());
        for (auto index_field : *index_fields) {
          if (!first) {
            factory_impl << ", ";
          }
          factory_impl << index_field.name_and_type.name;
          first = false;
        }

        factory_impl << ");\n";
        factory_impl << "  ReadOnlyRoots roots(isolate());\n";
        factory_impl << "  HeapObject result =\n";
        factory_impl << "    "
                        "isolate()->heap()->AllocateRawWith<Heap::kRetryOrFail>"
                        "(size, allocation_type);\n";
        factory_impl << "  result.set_map_after_allocation(roots."
                     << SnakeifyString(type->name())
                     << "_map(), SKIP_WRITE_BARRIER);\n";
        factory_impl << "  " << type->HandlifiedCppTypeName()
                     << " result_handle(" << type->name()
                     << "::cast(result), isolate());\n";

        for (const Field& f : type->ComputeAllFields()) {
          if (f.name_and_type.name == "map") continue;
          if (!f.index) {
            factory_impl << "  result_handle->set_"
                         << SnakeifyString(f.name_and_type.name) << "(";
            if (f.name_and_type.type->IsSubtypeOf(
                    TypeOracle::GetTaggedType()) &&
                !f.name_and_type.type->IsSubtypeOf(TypeOracle::GetSmiType())) {
              factory_impl << "*" << f.name_and_type.name
                           << ", SKIP_WRITE_BARRIER";
            } else {
              factory_impl << f.name_and_type.name;
            }
            factory_impl << ");\n";
          }
        }

        factory_impl << "  return result_handle;\n";
        factory_impl << "}\n\n";
      }
    }

    for (const StructType* type : structs_used_in_classes) {
      if (type != TypeOracle::GetFloat64OrHoleType()) {
        GenerateStructLayoutDescription(external_header, type);
      }
    }
  }
  WriteFile(file_basename + ".h", external_header.str());
  WriteFile(file_basename + "-inl.h", inline_external_header.str());
  WriteFile(file_basename + ".cc", implementation.str());
  WriteFile(internal_file_basename + ".h", internal_header.str());
  WriteFile(internal_file_basename + "-inl.h", inline_internal_header.str());
  WriteFile(factory_file_basename + ".inc", factory_header.str());
  WriteFile(factory_file_basename + ".cc", factory_impl.str());
}

namespace {
void GeneratePrintDefinitionsForClass(std::ostream& impl, const ClassType* type,
                                      const std::string& gen_name,
                                      const std::string& gen_name_T,
                                      const std::string template_params) {
  impl << template_params << "\n";
  impl << "void " << gen_name_T << "::" << type->name()
       << "Print(std::ostream& os) {\n";
  impl << "  this->PrintHeader(os, \"" << gen_name << "\");\n";
  auto hierarchy = type->GetHierarchy();
  std::map<std::string, const AggregateType*> field_names;
  for (const AggregateType* aggregate_type : hierarchy) {
    for (const Field& f : aggregate_type->fields()) {
      if (f.name_and_type.name == "map") continue;
      if (!f.index.has_value()) {
        if ((aggregate_type == TypeOracle::GetFixedArrayBaseType() &&
             f.name_and_type.name == "length") ||
            !f.name_and_type.type->IsSubtypeOf(TypeOracle::GetTaggedType())) {
          impl << "  os << \"\\n - " << f.name_and_type.name << ": \" << "
               << "this->" << f.name_and_type.name << "();\n";
        } else {
          impl << "  os << \"\\n - " << f.name_and_type.name << ": \" << "
               << "Brief(this->" << f.name_and_type.name << "());\n";
        }
      }
    }
  }
  impl << "  os << \"\\n\";\n";
  impl << "}\n\n";
}
}  // namespace

void ImplementationVisitor::GeneratePrintDefinitions(
    const std::string& output_directory) {
  std::stringstream impl;
  std::string file_name = "objects-printer-tq.cc";
  {
    IfDefScope object_print(impl, "OBJECT_PRINT");

    impl << "#include \"src/objects/objects.h\"\n\n";
    impl << "#include <iosfwd>\n\n";
    impl << "#include "
            "\"torque-generated/internal-class-definitions-tq-inl.h\"\n";
    impl << "#include \"src/objects/struct-inl.h\"\n\n";
    impl << "#include \"src/objects/template-objects-inl.h\"\n\n";

    NamespaceScope impl_namespaces(impl, {"v8", "internal"});

    for (const ClassType* type : TypeOracle::GetClasses()) {
      if (!type->ShouldGeneratePrint()) continue;

      if (type->GenerateCppClassDefinitions()) {
        const ClassType* super = type->GetSuperClass();
        std::string gen_name = "TorqueGenerated" + type->name();
        std::string gen_name_T =
            gen_name + "<" + type->name() + ", " + super->name() + ">";
        std::string template_decl = "template <>";
        GeneratePrintDefinitionsForClass(impl, type, gen_name, gen_name_T,
                                         template_decl);
      } else {
        GeneratePrintDefinitionsForClass(impl, type, type->name(), type->name(),
                                         "");
      }
    }
  }

  std::string new_contents(impl.str());
  WriteFile(output_directory + "/" + file_name, new_contents);
}

void ImplementationVisitor::GenerateBodyDescriptors(
    const std::string& output_directory) {
  std::string file_name = "objects-body-descriptors-tq-inl";
  std::stringstream h_contents;
  {
    IncludeGuardScope include_guard(h_contents, file_name + ".h");

    h_contents << "\n#include \"src/objects/objects-body-descriptors.h\"\n";
    h_contents << "\n#include \"torque-generated/class-definitions-tq.h\"\n";
    h_contents
        << "\n#include \"torque-generated/internal-class-definitions-tq.h\"\n";
    h_contents << "\n#include "
                  "\"torque-generated/internal-class-definitions-tq-inl.h\"\n";

    NamespaceScope h_namespaces(h_contents, {"v8", "internal"});

    for (const ClassType* type : TypeOracle::GetClasses()) {
      std::string name = type->name();
      if (!type->ShouldGenerateBodyDescriptor()) continue;

      const ClassType* super_class = type->GetSuperClass();
      std::string super_name = super_class->name();
      h_contents << "class " << name
                 << "::BodyDescriptor final : public BodyDescriptorBase {\n";

      h_contents << " public:\n";

      h_contents << "  static bool IsValidSlot(Map map, HeapObject obj, int "
                    "offset) {\n";

      if (super_class == TypeOracle::GetHeapObjectType() ||
          super_class == TypeOracle::GetFixedArrayBaseType()) {
        h_contents << "    if (offset < " << super_name
                   << "::kHeaderSize) return true;\n";
      } else {
        h_contents << "    if (" << super_name
                   << "::BodyDescriptor::IsValidSlot(map, obj, offset)) return "
                      "true;\n";
      }

      h_contents << "    return offset >= " << name
                 << "::kStartOfStrongFieldsOffset"
                 << " && offset < " << name << ""
                 << "::kEndOfStrongFieldsOffset;\n";
      h_contents << "  }\n\n";

      h_contents << "  template <typename ObjectVisitor>\n";
      h_contents << "  static inline void IterateBody(Map map, HeapObject obj, "
                    "int object_size, ObjectVisitor* v) {\n";

      // There may be MaybeObjects embedded in the strong pointer section, which
      // are not suppored.
      for (auto& f : type->fields()) {
        for (const Type* t : LowerType(f.name_and_type.type)) {
          if (t->IsSubtypeOf(TypeOracle::GetTaggedType()) &&
              !t->IsSubtypeOf(TypeOracle::GetObjectType())) {
            Error("Cannot generate body descriptor for field ",
                  f.name_and_type.name, " of class ", name, " because ", *t,
                  " can contain tagged weak pointers.");
          }
        }
      }

      if (super_class != TypeOracle::GetHeapObjectType() &&
          super_class != TypeOracle::GetFixedArrayBaseType()) {
        h_contents
            << "    " << super_name
            << "::BodyDescriptor::IterateBody(map, obj, object_size, v);\n";
      }

      h_contents << "    if (" << name
                 << "::kStartOfStrongFieldsOffset != " << name
                 << "::kEndOfStrongFieldsOffset) {\n";
      h_contents << "      IteratePointers(obj, " << name
                 << "::kStartOfStrongFieldsOffset, " << name
                 << "::kEndOfStrongFieldsOffset, v);\n";
      h_contents << "    }\n";

      h_contents << "    if (" << name
                 << "::kStartOfWeakFieldsOffset != " << name
                 << "::kEndOfWeakFieldsOffset) {\n";
      h_contents << "      IterateCustomWeakPointers(obj, " << name
                 << "::kStartOfWeakFieldsOffset, " << name
                 << "::kEndOfWeakFieldsOffset, v);\n";
      h_contents << "    }\n";

      // Since all of the index fields are at the end of the object and must
      // only be Tagged values, emit only a single IteratePointers from the
      // beginning of the first indexed field to the end of the object.
      bool first_index_seen = false;
      for (const Field& field : type->ComputeAllFields()) {
        if (field.index && !first_index_seen) {
          std::string indexed_field_name =
              CamelifyString(field.name_and_type.name);
          if (field.name_and_type.type->IsSubtypeOf(
                  TypeOracle::GetObjectType())) {
            h_contents << "    BodyDescriptorBase::IteratePointers(obj, "
                       << name << "::k" << indexed_field_name << "Offset, "
                       << name << "::SizeFor(" << name << "::cast(obj)), v);\n";
          } else {
            Error(
                "generating body descriptors for indexed fields not subtype of "
                "Object isn't (yet) supported");
          }
          first_index_seen = true;
        }
        if (first_index_seen) {
          for (const Type* t : LowerType(field.name_and_type.type)) {
            if (!t->IsSubtypeOf(TypeOracle::GetObjectType())) {
              Error("cannot generate class body descriptor for \"",
                    type->name(),
                    "\", all fields of including and after the first indexed "
                    "member must no comprised only of subtypes of Object "
                    "(field \"",
                    field.name_and_type.name, "\" is not)");
            }
          }
        }
      }

      h_contents << "  }\n\n";

      h_contents
          << "  static inline int SizeOf(Map map, HeapObject raw_object) {\n";
      h_contents << "    " << name << " object = " << name
                 << "::cast(raw_object);\n";
      h_contents << "    return " << name << "::SizeFor(object);\n";
      h_contents << "  }\n\n";

      h_contents << "};\n";
    }
  }

  WriteFile(output_directory + "/" + file_name + ".h", h_contents.str());
}

namespace {

// Generate verification code for a single piece of class data, which might be
// nested within a struct or might be a single element in an indexed field (or
// both).
void GenerateFieldValueVerifier(const std::string& class_name,
                                const Field& class_field,
                                const Field& leaf_field, size_t struct_offset,
                                std::string field_size,
                                std::ostream& cc_contents) {
  const Type* field_type = leaf_field.name_and_type.type;

  bool maybe_object =
      !field_type->IsSubtypeOf(TypeOracle::GetStrongTaggedType());
  const char* object_type = maybe_object ? "MaybeObject" : "Object";
  const char* verify_fn =
      maybe_object ? "VerifyMaybeObjectPointer" : "VerifyPointer";
  std::string index_offset = std::to_string(struct_offset);
  if (class_field.index) {
    index_offset += " + i * " + field_size;
  }
  // Name the local var based on the field name for nicer CHECK output.
  const std::string value = leaf_field.name_and_type.name + "__value";

  // Read the field.
  cc_contents << "    " << object_type << " " << value << " = TaggedField<"
              << object_type << ", " << class_name << "::k"
              << CamelifyString(class_field.name_and_type.name)
              << "Offset>::load(o, " << index_offset << ");\n";

  // Call VerifyPointer or VerifyMaybeObjectPointer on it.
  cc_contents << "    " << object_type << "::" << verify_fn << "(isolate, "
              << value << ");\n";

  // Check that the value is of an appropriate type. We can skip this part for
  // the Object type because it would not check anything beyond what we already
  // checked with VerifyPointer.
  if (field_type != TypeOracle::GetObjectType()) {
    std::stringstream type_check;
    bool at_start = true;
    // If weak pointers are allowed, then start by checking for a cleared value.
    if (maybe_object) {
      type_check << value << ".IsCleared()";
      at_start = false;
    }
    for (const RuntimeType& runtime_type : field_type->GetRuntimeTypes()) {
      if (!at_start) type_check << " || ";
      at_start = false;
      if (maybe_object) {
        bool strong = runtime_type.weak_ref_to.empty();
        if (strong && runtime_type.type == "MaybeObject") {
          // Rather than a generic Weak<T>, this is a basic type Tagged or
          // WeakHeapObject. We can't validate anything more about the type of
          // the object pointed to, so just check that it's weak.
          type_check << value << ".IsWeak()";
        } else {
          type_check << "(" << (strong ? "!" : "") << value << ".IsWeak() && "
                     << value << ".GetHeapObjectOrSmi().Is"
                     << (strong ? runtime_type.type : runtime_type.weak_ref_to)
                     << "())";
        }
      } else {
        type_check << value << ".Is" << runtime_type.type << "()";
      }
    }
    cc_contents << "    CHECK(" << type_check.str() << ");\n";
  }
}

void GenerateClassFieldVerifier(const std::string& class_name,
                                const ClassType& class_type, const Field& f,
                                std::ostream& h_contents,
                                std::ostream& cc_contents) {
  if (!f.generate_verify) return;
  const Type* field_type = f.name_and_type.type;

  // We only verify tagged types, not raw numbers or pointers. Structs
  // consisting of tagged types are also included.
  if (!field_type->IsSubtypeOf(TypeOracle::GetTaggedType()) &&
      !field_type->IsStructType())
    return;
  if (field_type == TypeOracle::GetFloat64OrHoleType()) return;
  // Do not verify if the field may be uninitialized.
  if (TypeOracle::GetUninitializedType()->IsSubtypeOf(field_type)) return;

  if (f.index) {
    base::Optional<NameAndType> array_length =
        ExtractSimpleFieldArraySize(class_type, *f.index);
    if (!array_length) {
      Error("Cannot generate verifier for array field with complex length.")
          .Position((*f.index)->pos)
          .Throw();
    }

    std::string length_field_offset =
        class_name + "::k" + CamelifyString(array_length->name) + "Offset";
    cc_contents << "  for (int i = 0; i < ";
    if (array_length->type == TypeOracle::GetSmiType()) {
      // We already verified the index field because it was listed earlier, so
      // we can assume it's safe to read here.
      cc_contents << "TaggedField<Smi, " << length_field_offset
                  << ">::load(o).value()";
    } else {
      const Type* constexpr_version = array_length->type->ConstexprVersion();
      if (constexpr_version == nullptr) {
        Error("constexpr representation for type ",
              array_length->type->ToString(),
              " is required due to usage as index")
            .Position(f.pos);
      }
      cc_contents << "o.ReadField<" << constexpr_version->GetGeneratedTypeName()
                  << ">(" << length_field_offset << ")";
    }
    cc_contents << "; ++i) {\n";
  } else {
    cc_contents << "  {\n";
  }

  if (const StructType* struct_type = StructType::DynamicCast(field_type)) {
    for (const Field& field : struct_type->fields()) {
      if (field_type->IsSubtypeOf(TypeOracle::GetTaggedType())) {
        GenerateFieldValueVerifier(class_name, f, field, *field.offset,
                                   std::to_string(struct_type->PackedSize()),
                                   cc_contents);
      }
    }
  } else {
    GenerateFieldValueVerifier(class_name, f, f, 0, "kTaggedSize", cc_contents);
  }

  cc_contents << "  }\n";
}

}  // namespace

void ImplementationVisitor::GenerateClassVerifiers(
    const std::string& output_directory) {
  std::string file_name = "class-verifiers-tq";
  std::stringstream h_contents;
  std::stringstream cc_contents;
  {
    IncludeGuardScope include_guard(h_contents, file_name + ".h");
    IfDefScope verify_heap_h(h_contents, "VERIFY_HEAP");
    IfDefScope verify_heap_cc(cc_contents, "VERIFY_HEAP");

    cc_contents << "\n#include \"src/objects/objects.h\"\n";

    for (const std::string& include_path : GlobalContext::CppIncludes()) {
      cc_contents << "#include " << StringLiteralQuote(include_path) << "\n";
    }
    cc_contents << "#include \"torque-generated/" << file_name << ".h\"\n";
    cc_contents << "#include "
                   "\"torque-generated/internal-class-definitions-tq-inl.h\"\n";

    IncludeObjectMacrosScope object_macros(cc_contents);

    NamespaceScope h_namespaces(h_contents, {"v8", "internal"});
    NamespaceScope cc_namespaces(cc_contents, {"v8", "internal"});

    // Generate forward declarations to avoid including any headers.
    h_contents << "class Isolate;\n";
    for (const ClassType* type : TypeOracle::GetClasses()) {
      if (!type->ShouldGenerateVerify()) continue;
      h_contents << "class " << type->name() << ";\n";
    }

    const char* verifier_class = "TorqueGeneratedClassVerifiers";

    h_contents << "class " << verifier_class << "{\n";
    h_contents << " public:\n";

    for (const ClassType* type : TypeOracle::GetClasses()) {
      std::string name = type->name();
      if (!type->ShouldGenerateVerify()) continue;

      std::string method_name = name + "Verify";

      h_contents << "  static void " << method_name << "(" << name
                 << " o, Isolate* isolate);\n";

      cc_contents << "void " << verifier_class << "::" << method_name << "("
                  << name << " o, Isolate* isolate) {\n";

      // First, do any verification for the super class. Not all classes have
      // verifiers, so skip to the nearest super class that has one.
      const ClassType* super_type = type->GetSuperClass();
      while (super_type && !super_type->ShouldGenerateVerify()) {
        super_type = super_type->GetSuperClass();
      }
      if (super_type) {
        std::string super_name = super_type->name();
        if (super_name == "HeapObject") {
          // Special case: HeapObjectVerify checks the Map type and dispatches
          // to more specific types, so calling it here would cause infinite
          // recursion. We could consider moving that behavior into a
          // different method to make the contract of *Verify methods more
          // consistent, but for now we'll just avoid the bad case.
          cc_contents << "  " << super_name << "Verify(o, isolate);\n";
        } else {
          cc_contents << "  o." << super_name << "Verify(isolate);\n";
        }
      }

      // Second, verify that this object is what it claims to be.
      cc_contents << "  CHECK(o.Is" << name << "());\n";

      // Third, verify its properties.
      for (auto f : type->fields()) {
        GenerateClassFieldVerifier(name, *type, f, h_contents, cc_contents);
      }

      cc_contents << "}\n";
    }

    h_contents << "};\n";
  }
  WriteFile(output_directory + "/" + file_name + ".h", h_contents.str());
  WriteFile(output_directory + "/" + file_name + ".cc", cc_contents.str());
}

void ImplementationVisitor::GenerateEnumVerifiers(
    const std::string& output_directory) {
  std::string file_name = "enum-verifiers-tq";
  std::stringstream cc_contents;
  {
    cc_contents << "#include \"src/compiler/code-assembler.h\"\n";
    for (const std::string& include_path : GlobalContext::CppIncludes()) {
      cc_contents << "#include " << StringLiteralQuote(include_path) << "\n";
    }
    cc_contents << "\n";

    NamespaceScope cc_namespaces(cc_contents, {"v8", "internal", ""});

    cc_contents << "class EnumVerifier {\n";
    for (const auto& desc : GlobalContext::Get().ast()->EnumDescriptions()) {
      cc_contents << "  // " << desc.name << " (" << desc.pos << ")\n";
      cc_contents << "  void VerifyEnum_" << desc.name << "("
                  << desc.constexpr_generates
                  << " x) {\n"
                     "    switch(x) {\n";
      for (const auto& entry : desc.entries) {
        cc_contents << "      case " << entry << ": break;\n";
      }
      if (desc.is_open) cc_contents << "      default: break;\n";
      cc_contents << "    }\n  }\n\n";
    }
    cc_contents << "};\n";
  }

  WriteFile(output_directory + "/" + file_name + ".cc", cc_contents.str());
}

void ImplementationVisitor::GenerateExportedMacrosAssembler(
    const std::string& output_directory) {
  std::string file_name = "exported-macros-assembler-tq";
  std::stringstream h_contents;
  std::stringstream cc_contents;
  {
    IncludeGuardScope include_guard(h_contents, file_name + ".h");

    h_contents << "#include \"src/compiler/code-assembler.h\"\n";
    h_contents << "#include \"src/execution/frames.h\"\n";
    h_contents << "#include \"torque-generated/csa-types-tq.h\"\n";
    h_contents
        << "#include \"torque-generated/internal-class-definitions-tq.h\"\n";
    cc_contents << "#include \"src/objects/free-space.h\"\n";
    cc_contents << "#include \"src/objects/js-regexp-string-iterator.h\"\n";
    cc_contents << "#include \"src/objects/ordered-hash-table.h\"\n";
    cc_contents << "#include \"src/objects/property-descriptor-object.h\"\n";
    cc_contents << "#include \"src/objects/synthetic-module.h\"\n";
    cc_contents << "#include \"src/objects/template-objects.h\"\n";
    {
      IfDefScope intl_scope(cc_contents, "V8_INTL_SUPPORT");
      cc_contents << "#include \"src/objects/js-break-iterator.h\"\n";
      cc_contents << "#include \"src/objects/js-collator.h\"\n";
      cc_contents << "#include \"src/objects/js-date-time-format.h\"\n";
      cc_contents << "#include \"src/objects/js-display-names.h\"\n";
      cc_contents << "#include \"src/objects/js-list-format.h\"\n";
      cc_contents << "#include \"src/objects/js-locale.h\"\n";
      cc_contents << "#include \"src/objects/js-number-format.h\"\n";
      cc_contents << "#include \"src/objects/js-plural-rules.h\"\n";
      cc_contents << "#include \"src/objects/js-relative-time-format.h\"\n";
      cc_contents << "#include \"src/objects/js-segment-iterator.h\"\n";
      cc_contents << "#include \"src/objects/js-segmenter.h\"\n";
    }
    cc_contents << "#include \"torque-generated/" << file_name << ".h\"\n";

    for (SourceId file : SourceFileMap::AllSources()) {
      cc_contents << "#include \"torque-generated/" +
                         SourceFileMap::PathFromV8RootWithoutExtension(file) +
                         "-tq-csa.h\"\n";
    }

    NamespaceScope h_namespaces(h_contents, {"v8", "internal"});
    NamespaceScope cc_namespaces(cc_contents, {"v8", "internal"});

    h_contents << "class V8_EXPORT_PRIVATE "
                  "TorqueGeneratedExportedMacrosAssembler {\n"
               << " public:\n"
               << "  explicit TorqueGeneratedExportedMacrosAssembler"
                  "(compiler::CodeAssemblerState* state) : state_(state) {\n"
               << "    USE(state_);\n"
               << "  }\n";

    for (auto& declarable : GlobalContext::AllDeclarables()) {
      TorqueMacro* macro = TorqueMacro::DynamicCast(declarable.get());
      if (!(macro && macro->IsExportedToCSA())) continue;

      h_contents << "  ";
      GenerateFunctionDeclaration(h_contents, "", macro->ReadableName(),
                                  macro->signature(), macro->parameter_names(),
                                  false);
      h_contents << ";\n";

      std::vector<std::string> parameter_names = GenerateFunctionDeclaration(
          cc_contents,
          "TorqueGeneratedExportedMacrosAssembler::", macro->ReadableName(),
          macro->signature(), macro->parameter_names(), false);
      cc_contents << "{\n";
      cc_contents << "return " << macro->ExternalName() << "(state_";
      for (auto& name : parameter_names) {
        cc_contents << ", " << name;
      }
      cc_contents << ");\n";
      cc_contents << "}\n";
    }

    h_contents << " private:\n"
               << "  compiler::CodeAssemblerState* state_;\n"
               << "};\n";
  }
  WriteFile(output_directory + "/" + file_name + ".h", h_contents.str());
  WriteFile(output_directory + "/" + file_name + ".cc", cc_contents.str());
}

void ImplementationVisitor::GenerateCSATypes(
    const std::string& output_directory) {
  std::string file_name = "csa-types-tq";
  std::stringstream h_contents;
  {
    IncludeGuardScope include_guard(h_contents, file_name + ".h");
    h_contents << "#include \"src/compiler/code-assembler.h\"\n\n";

    NamespaceScope h_namespaces(h_contents, {"v8", "internal"});

    // Generates headers for all structs in a topologically-sorted order, since
    // TypeOracle keeps them in the order of their resolution
    for (const auto& type : TypeOracle::GetAggregateTypes()) {
      const StructType* struct_type = StructType::DynamicCast(type.get());
      if (!struct_type) continue;
      h_contents << "struct " << struct_type->GetGeneratedTypeNameImpl()
                 << " {\n";
      for (auto& field : struct_type->fields()) {
        h_contents << "  " << field.name_and_type.type->GetGeneratedTypeName();
        h_contents << " " << field.name_and_type.name << ";\n";
      }
      h_contents << "\n  std::tuple<";
      bool first = true;
      for (const Type* type : LowerType(struct_type)) {
        if (!first) {
          h_contents << ", ";
        }
        first = false;
        h_contents << type->GetGeneratedTypeName();
      }
      h_contents << "> Flatten() const {\n"
                 << "    return std::tuple_cat(";
      first = true;
      for (auto& field : struct_type->fields()) {
        if (!first) {
          h_contents << ", ";
        }
        first = false;
        if (field.name_and_type.type->IsStructType()) {
          h_contents << field.name_and_type.name << ".Flatten()";
        } else {
          h_contents << "std::make_tuple(" << field.name_and_type.name << ")";
        }
      }
      h_contents << ");\n";
      h_contents << "  }\n";
      h_contents << "};\n";
    }
  }
  WriteFile(output_directory + "/" + file_name + ".h", h_contents.str());
}

void ReportAllUnusedMacros() {
  for (const auto& declarable : GlobalContext::AllDeclarables()) {
    if (!declarable->IsMacro() || declarable->IsExternMacro()) continue;

    Macro* macro = Macro::cast(declarable.get());
    if (macro->IsUsed()) continue;

    if (macro->IsTorqueMacro() && TorqueMacro::cast(macro)->IsExportedToCSA()) {
      continue;
    }
    // TODO(gsps): Mark methods of generic structs used if they are used in any
    // instantiation
    if (Method* method = Method::DynamicCast(macro)) {
      if (StructType* struct_type =
              StructType::DynamicCast(method->aggregate_type())) {
        if (struct_type->GetSpecializedFrom().has_value()) {
          continue;
        }
      }
    }

    std::vector<std::string> ignored_prefixes = {"Convert<", "Cast<",
                                                 "FromConstexpr<"};
    const std::string name = macro->ReadableName();
    const bool ignore =
        std::any_of(ignored_prefixes.begin(), ignored_prefixes.end(),
                    [&name](const std::string& prefix) {
                      return StringStartsWith(name, prefix);
                    });

    if (!ignore) {
      Lint("Macro '", macro->ReadableName(), "' is never used.")
          .Position(macro->IdentifierPosition());
    }
  }
}

}  // namespace torque
}  // namespace internal
}  // namespace v8
