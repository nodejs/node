// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/torque/csa-generator.h"

#include "src/globals.h"
#include "src/torque/type-oracle.h"
#include "src/torque/utils.h"

namespace v8 {
namespace internal {
namespace torque {

base::Optional<Stack<std::string>> CSAGenerator::EmitGraph(
    Stack<std::string> parameters) {
  for (Block* block : cfg_.blocks()) {
    out_ << "  compiler::CodeAssemblerParameterizedLabel<";
    PrintCommaSeparatedList(out_, block->InputTypes(), [](const Type* t) {
      return t->GetGeneratedTNodeTypeName();
    });
    out_ << "> " << BlockName(block) << "(&ca_, compiler::CodeAssemblerLabel::"
         << (block->IsDeferred() ? "kDeferred" : "kNonDeferred") << ");\n";
  }

  EmitInstruction(GotoInstruction{cfg_.start()}, &parameters);
  for (Block* block : cfg_.blocks()) {
    if (cfg_.end() && *cfg_.end() == block) continue;
    out_ << "\n  if (" << BlockName(block) << ".is_used()) {\n";
    EmitBlock(block);
    out_ << "  }\n";
  }
  if (cfg_.end()) {
    out_ << "\n";
    return EmitBlock(*cfg_.end());
  }
  return base::nullopt;
}

Stack<std::string> CSAGenerator::EmitBlock(const Block* block) {
  Stack<std::string> stack;
  for (const Type* t : block->InputTypes()) {
    stack.Push(FreshNodeName());
    out_ << "    compiler::TNode<" << t->GetGeneratedTNodeTypeName() << "> "
         << stack.Top() << ";\n";
  }
  out_ << "    ca_.Bind(&" << BlockName(block);
  for (const std::string& name : stack) {
    out_ << ", &" << name;
  }
  out_ << ");\n";
  for (const Instruction& instruction : block->instructions()) {
    EmitInstruction(instruction, &stack);
  }
  return stack;
}

void CSAGenerator::EmitSourcePosition(SourcePosition pos, bool always_emit) {
  const std::string& file = SourceFileMap::GetSource(pos.source);
  if (always_emit || !previous_position_.CompareStartIgnoreColumn(pos)) {
    // Lines in Torque SourcePositions are zero-based, while the
    // CodeStubAssembler and downwind systems are one-based.
    out_ << "    ca_.SetSourcePosition(\"" << file << "\", "
         << (pos.start.line + 1) << ");\n";
    previous_position_ = pos;
  }
}

void CSAGenerator::EmitInstruction(const Instruction& instruction,
                                   Stack<std::string>* stack) {
  EmitSourcePosition(instruction->pos);
  switch (instruction.kind()) {
#define ENUM_ITEM(T)          \
  case InstructionKind::k##T: \
    return EmitInstruction(instruction.Cast<T>(), stack);
    TORQUE_INSTRUCTION_LIST(ENUM_ITEM)
#undef ENUM_ITEM
  }
}

void CSAGenerator::EmitInstruction(const PeekInstruction& instruction,
                                   Stack<std::string>* stack) {
  stack->Push(stack->Peek(instruction.slot));
}

void CSAGenerator::EmitInstruction(const PokeInstruction& instruction,
                                   Stack<std::string>* stack) {
  stack->Poke(instruction.slot, stack->Top());
  stack->Pop();
}

void CSAGenerator::EmitInstruction(const DeleteRangeInstruction& instruction,
                                   Stack<std::string>* stack) {
  stack->DeleteRange(instruction.range);
}

void CSAGenerator::EmitInstruction(
    const PushUninitializedInstruction& instruction,
    Stack<std::string>* stack) {
  // TODO(tebbi): This can trigger an error in CSA if it is used. Instead, we
  // should prevent usage of uninitialized in the type system. This
  // requires "if constexpr" being evaluated at Torque time.
  stack->Push("ca_.Uninitialized<" +
              instruction.type->GetGeneratedTNodeTypeName() + ">()");
}

void CSAGenerator::EmitInstruction(
    const PushBuiltinPointerInstruction& instruction,
    Stack<std::string>* stack) {
  stack->Push("ca_.UncheckedCast<BuiltinPtr>(ca_.SmiConstant(Builtins::k" +
              instruction.external_name + "))");
}

void CSAGenerator::EmitInstruction(
    const NamespaceConstantInstruction& instruction,
    Stack<std::string>* stack) {
  const Type* type = instruction.constant->type();
  std::vector<std::string> results;
  for (const Type* lowered : LowerType(type)) {
    results.push_back(FreshNodeName());
    stack->Push(results.back());
    out_ << "    compiler::TNode<" << lowered->GetGeneratedTNodeTypeName()
         << "> " << stack->Top() << ";\n";
    out_ << "    USE(" << stack->Top() << ");\n";
  }
  out_ << "    ";
  if (type->IsStructType()) {
    out_ << "std::tie(";
    PrintCommaSeparatedList(out_, results);
    out_ << ") = ";
  } else if (results.size() == 1) {
    out_ << results[0] << " = ";
  }
  out_ << instruction.constant->ExternalAssemblerName() << "(state_)."
       << instruction.constant->name()->value << "()";
  if (type->IsStructType()) {
    out_ << ".Flatten();\n";
  } else {
    out_ << ";\n";
  }
}

void CSAGenerator::ProcessArgumentsCommon(
    const TypeVector& parameter_types, std::vector<std::string>* args,
    std::vector<std::string>* constexpr_arguments, Stack<std::string>* stack) {
  for (auto it = parameter_types.rbegin(); it != parameter_types.rend(); ++it) {
    const Type* type = *it;
    VisitResult arg;
    if (type->IsConstexpr()) {
      args->push_back(std::move(constexpr_arguments->back()));
      constexpr_arguments->pop_back();
    } else {
      std::stringstream s;
      size_t slot_count = LoweredSlotCount(type);
      VisitResult arg = VisitResult(type, stack->TopRange(slot_count));
      EmitCSAValue(arg, *stack, s);
      args->push_back(s.str());
      stack->PopMany(slot_count);
    }
  }
  std::reverse(args->begin(), args->end());
}

void CSAGenerator::EmitInstruction(const CallIntrinsicInstruction& instruction,
                                   Stack<std::string>* stack) {
  std::vector<std::string> constexpr_arguments =
      instruction.constexpr_arguments;
  std::vector<std::string> args;
  TypeVector parameter_types =
      instruction.intrinsic->signature().parameter_types.types;
  ProcessArgumentsCommon(parameter_types, &args, &constexpr_arguments, stack);

  Stack<std::string> pre_call_stack = *stack;
  const Type* return_type = instruction.intrinsic->signature().return_type;
  std::vector<std::string> results;
  for (const Type* type : LowerType(return_type)) {
    results.push_back(FreshNodeName());
    stack->Push(results.back());
    out_ << "    compiler::TNode<" << type->GetGeneratedTNodeTypeName() << "> "
         << stack->Top() << ";\n";
    out_ << "    USE(" << stack->Top() << ");\n";
  }
  out_ << "    ";

  if (return_type->IsStructType()) {
    out_ << "std::tie(";
    PrintCommaSeparatedList(out_, results);
    out_ << ") = ";
  } else {
    if (results.size() == 1) {
      out_ << results[0] << " = ";
    }
  }

  if (instruction.intrinsic->ExternalName() == "%RawDownCast") {
    if (parameter_types.size() != 1) {
      ReportError("%RawDownCast must take a single parameter");
    }
    if (!return_type->IsSubtypeOf(parameter_types[0])) {
      ReportError("%RawDownCast error: ", *return_type, " is not a subtype of ",
                  *parameter_types[0]);
    }
    if (return_type->IsSubtypeOf(TypeOracle::GetTaggedType())) {
      if (return_type->GetGeneratedTNodeTypeName() !=
          parameter_types[0]->GetGeneratedTNodeTypeName()) {
        out_ << "TORQUE_CAST";
      }
    }
  } else if (instruction.intrinsic->ExternalName() == "%FromConstexpr") {
    if (parameter_types.size() != 1 || !parameter_types[0]->IsConstexpr()) {
      ReportError(
          "%FromConstexpr must take a single parameter with constexpr "
          "type");
    }
    if (return_type->IsConstexpr()) {
      ReportError("%FromConstexpr must return a non-constexpr type");
    }
    if (return_type->IsSubtypeOf(TypeOracle::GetSmiType())) {
      out_ << "ca_.SmiConstant";
    } else if (return_type->IsSubtypeOf(TypeOracle::GetNumberType())) {
      out_ << "ca_.NumberConstant";
    } else if (return_type->IsSubtypeOf(TypeOracle::GetStringType())) {
      out_ << "ca_.StringConstant";
    } else if (return_type->IsSubtypeOf(TypeOracle::GetObjectType())) {
      ReportError(
          "%FromConstexpr cannot cast to subclass of HeapObject unless it's a "
          "String or Number");
    } else if (return_type->IsSubtypeOf(TypeOracle::GetIntPtrType())) {
      out_ << "ca_.IntPtrConstant";
    } else if (return_type->IsSubtypeOf(TypeOracle::GetUIntPtrType())) {
      out_ << "ca_.UintPtrConstant";
    } else if (return_type->IsSubtypeOf(TypeOracle::GetInt32Type())) {
      out_ << "ca_.Int32Constant";
    } else {
      std::stringstream s;
      s << "%FromConstexpr does not support return type " << *return_type;
      ReportError(s.str());
    }
  } else if (instruction.intrinsic->ExternalName() ==
             "%GetAllocationBaseSize") {
    if (instruction.specialization_types.size() != 1) {
      ReportError(
          "incorrect number of specialization classes for "
          "%GetAllocationBaseSize (should be one)");
    }
    const ClassType* class_type =
        ClassType::cast(instruction.specialization_types[0]);
    // Special case classes that may not always have a fixed size (e.g.
    // JSObjects). Their size must be fetched from the map.
    if (class_type != TypeOracle::GetJSObjectType()) {
      out_ << "CodeStubAssembler(state_).IntPtrConstant((";
      args[0] = std::to_string(class_type->size());
    } else {
      out_ << "CodeStubAssembler(state_).TimesTaggedSize(CodeStubAssembler("
              "state_).LoadMapInstanceSizeInWords(";
    }
  } else if (instruction.intrinsic->ExternalName() == "%Allocate") {
    out_ << "ca_.UncheckedCast<" << return_type->GetGeneratedTNodeTypeName()
         << ">(CodeStubAssembler(state_).Allocate";
  } else if (instruction.intrinsic->ExternalName() ==
             "%AllocateInternalClass") {
    out_ << "CodeStubAssembler(state_).AllocateUninitializedFixedArray";
  } else {
    ReportError("no built in intrinsic with name " +
                instruction.intrinsic->ExternalName());
  }

  out_ << "(";
  PrintCommaSeparatedList(out_, args);
  if (instruction.intrinsic->ExternalName() == "%Allocate") out_ << ")";
  if (instruction.intrinsic->ExternalName() == "%GetAllocationBaseSize")
    out_ << "))";
  if (return_type->IsStructType()) {
    out_ << ").Flatten();\n";
  } else {
    out_ << ");\n";
  }
  if (instruction.intrinsic->ExternalName() == "%Allocate") {
    out_ << "    CodeStubAssembler(state_).InitializeFieldsWithRoot("
         << results[0] << ", ";
    out_ << "CodeStubAssembler(state_).IntPtrConstant("
         << std::to_string(ClassType::cast(return_type)->size()) << "), ";
    PrintCommaSeparatedList(out_, args);
    out_ << ", RootIndex::kUndefinedValue);\n";
  }
}

void CSAGenerator::EmitInstruction(const CallCsaMacroInstruction& instruction,
                                   Stack<std::string>* stack) {
  std::vector<std::string> constexpr_arguments =
      instruction.constexpr_arguments;
  std::vector<std::string> args;
  TypeVector parameter_types =
      instruction.macro->signature().parameter_types.types;
  ProcessArgumentsCommon(parameter_types, &args, &constexpr_arguments, stack);

  Stack<std::string> pre_call_stack = *stack;
  const Type* return_type = instruction.macro->signature().return_type;
  std::vector<std::string> results;
  for (const Type* type : LowerType(return_type)) {
    results.push_back(FreshNodeName());
    stack->Push(results.back());
    out_ << "    compiler::TNode<" << type->GetGeneratedTNodeTypeName() << "> "
         << stack->Top() << ";\n";
    out_ << "    USE(" << stack->Top() << ");\n";
  }
  std::string catch_name =
      PreCallableExceptionPreparation(instruction.catch_block);
  out_ << "    ";
  bool needs_flattening =
      return_type->IsStructType() || return_type->IsReferenceType();
  if (needs_flattening) {
    out_ << "std::tie(";
    PrintCommaSeparatedList(out_, results);
    out_ << ") = ";
  } else {
    if (results.size() == 1) {
      out_ << results[0] << " = ca_.UncheckedCast<"
           << return_type->GetGeneratedTNodeTypeName() << ">(";
    } else {
      DCHECK_EQ(0, results.size());
    }
  }
  out_ << instruction.macro->external_assembler_name() << "(state_)."
       << instruction.macro->ExternalName() << "(";
  PrintCommaSeparatedList(out_, args);
  if (needs_flattening) {
    out_ << ").Flatten();\n";
  } else {
    if (results.size() == 1) out_ << ")";
    out_ << ");\n";
  }
  PostCallableExceptionPreparation(catch_name, return_type,
                                   instruction.catch_block, &pre_call_stack);
}

void CSAGenerator::EmitInstruction(
    const CallCsaMacroAndBranchInstruction& instruction,
    Stack<std::string>* stack) {
  std::vector<std::string> constexpr_arguments =
      instruction.constexpr_arguments;
  std::vector<std::string> args;
  TypeVector parameter_types =
      instruction.macro->signature().parameter_types.types;
  ProcessArgumentsCommon(parameter_types, &args, &constexpr_arguments, stack);

  Stack<std::string> pre_call_stack = *stack;
  std::vector<std::string> results;
  const Type* return_type = instruction.macro->signature().return_type;
  if (return_type != TypeOracle::GetNeverType()) {
    for (const Type* type :
         LowerType(instruction.macro->signature().return_type)) {
      results.push_back(FreshNodeName());
      out_ << "    compiler::TNode<" << type->GetGeneratedTNodeTypeName()
           << "> " << results.back() << ";\n";
      out_ << "    USE(" << results.back() << ");\n";
    }
  }

  std::vector<std::string> label_names;
  std::vector<std::vector<std::string>> var_names;
  const LabelDeclarationVector& labels = instruction.macro->signature().labels;
  DCHECK_EQ(labels.size(), instruction.label_blocks.size());
  for (size_t i = 0; i < labels.size(); ++i) {
    TypeVector label_parameters = labels[i].types;
    label_names.push_back("label" + std::to_string(i));
    var_names.push_back({});
    for (size_t j = 0; j < label_parameters.size(); ++j) {
      var_names[i].push_back("result_" + std::to_string(i) + "_" +
                             std::to_string(j));
      out_ << "    compiler::TypedCodeAssemblerVariable<"
           << label_parameters[j]->GetGeneratedTNodeTypeName() << "> "
           << var_names[i][j] << "(&ca_);\n";
    }
    out_ << "    compiler::CodeAssemblerLabel " << label_names[i]
         << "(&ca_);\n";
  }

  std::string catch_name =
      PreCallableExceptionPreparation(instruction.catch_block);
  out_ << "    ";
  if (results.size() == 1) {
    out_ << results[0] << " = ";
  } else if (results.size() > 1) {
    out_ << "std::tie(";
    PrintCommaSeparatedList(out_, results);
    out_ << ") = ";
  }
  out_ << instruction.macro->external_assembler_name() << "(state_)."
       << instruction.macro->ExternalName() << "(";
  PrintCommaSeparatedList(out_, args);
  bool first = args.empty();
  for (size_t i = 0; i < label_names.size(); ++i) {
    if (!first) out_ << ", ";
    out_ << "&" << label_names[i];
    first = false;
    for (size_t j = 0; j < var_names[i].size(); ++j) {
      out_ << ", &" << var_names[i][j];
    }
  }
  if (return_type->IsStructType()) {
    out_ << ").Flatten();\n";
  } else {
    out_ << ");\n";
  }

  PostCallableExceptionPreparation(catch_name, return_type,
                                   instruction.catch_block, &pre_call_stack);

  if (instruction.return_continuation) {
    out_ << "    ca_.Goto(&" << BlockName(*instruction.return_continuation);
    for (const std::string& value : *stack) {
      out_ << ", " << value;
    }
    for (const std::string& result : results) {
      out_ << ", " << result;
    }
    out_ << ");\n";
  }
  for (size_t i = 0; i < label_names.size(); ++i) {
    out_ << "    if (" << label_names[i] << ".is_used()) {\n";
    out_ << "      ca_.Bind(&" << label_names[i] << ");\n";
    out_ << "      ca_.Goto(&" << BlockName(instruction.label_blocks[i]);
    for (const std::string& value : *stack) {
      out_ << ", " << value;
    }
    for (const std::string& var : var_names[i]) {
      out_ << ", " << var << ".value()";
    }
    out_ << ");\n";

    out_ << "    }\n";
  }
}

void CSAGenerator::EmitInstruction(const CallBuiltinInstruction& instruction,
                                   Stack<std::string>* stack) {
  std::vector<std::string> arguments = stack->PopMany(instruction.argc);
  std::vector<const Type*> result_types =
      LowerType(instruction.builtin->signature().return_type);
  if (instruction.is_tailcall) {
    out_ << "   CodeStubAssembler(state_).TailCallBuiltin(Builtins::k"
         << instruction.builtin->ExternalName() << ", ";
    PrintCommaSeparatedList(out_, arguments);
    out_ << ");\n";
  } else {
    std::string result_name = FreshNodeName();
    if (result_types.size() == 1) {
      out_ << "    compiler::TNode<"
           << result_types[0]->GetGeneratedTNodeTypeName() << "> "
           << result_name << ";\n";
    }
    std::string catch_name =
        PreCallableExceptionPreparation(instruction.catch_block);
    Stack<std::string> pre_call_stack = *stack;
    if (result_types.size() == 1) {
      std::string generated_type = result_types[0]->GetGeneratedTNodeTypeName();
      stack->Push(result_name);
      out_ << "    " << result_name << " = ";
      if (generated_type != "Object") out_ << "TORQUE_CAST(";
      out_ << "CodeStubAssembler(state_).CallBuiltin(Builtins::k"
           << instruction.builtin->ExternalName() << ", ";
      PrintCommaSeparatedList(out_, arguments);
      if (generated_type != "Object") out_ << ")";
      out_ << ");\n";
      out_ << "    USE(" << result_name << ");\n";
    } else {
      DCHECK_EQ(0, result_types.size());
      // TODO(tebbi): Actually, builtins have to return a value, so we should
      // not have to handle this case.
      out_ << "    CodeStubAssembler(state_).CallBuiltin(Builtins::k"
           << instruction.builtin->ExternalName() << ", ";
      PrintCommaSeparatedList(out_, arguments);
      out_ << ");\n";
    }
    PostCallableExceptionPreparation(
        catch_name,
        result_types.size() == 0 ? TypeOracle::GetVoidType() : result_types[0],
        instruction.catch_block, &pre_call_stack);
  }
}

void CSAGenerator::EmitInstruction(
    const CallBuiltinPointerInstruction& instruction,
    Stack<std::string>* stack) {
  std::vector<std::string> function_and_arguments =
      stack->PopMany(1 + instruction.argc);
  std::vector<const Type*> result_types =
      LowerType(instruction.type->return_type());
  if (result_types.size() != 1) {
    ReportError("builtins must have exactly one result");
  }
  if (instruction.is_tailcall) {
    ReportError("tail-calls to builtin pointers are not supported");
  }

  stack->Push(FreshNodeName());
  std::string generated_type = result_types[0]->GetGeneratedTNodeTypeName();
  out_ << "    compiler::TNode<" << generated_type << "> " << stack->Top()
       << " = ";
  if (generated_type != "Object") out_ << "TORQUE_CAST(";
  out_ << "CodeStubAssembler(state_).CallBuiltinPointer(Builtins::"
          "CallableFor(ca_."
          "isolate(),"
          "ExampleBuiltinForTorqueFunctionPointerType("
       << instruction.type->function_pointer_type_id() << ")).descriptor(), ";
  PrintCommaSeparatedList(out_, function_and_arguments);
  out_ << ")";
  if (generated_type != "Object") out_ << ")";
  out_ << "; \n";
  out_ << "    USE(" << stack->Top() << ");\n";
}

std::string CSAGenerator::PreCallableExceptionPreparation(
    base::Optional<Block*> catch_block) {
  std::string catch_name;
  if (catch_block) {
    catch_name = FreshCatchName();
    out_ << "    compiler::CodeAssemblerExceptionHandlerLabel " << catch_name
         << "_label(&ca_, compiler::CodeAssemblerLabel::kDeferred);\n";
    out_ << "    { compiler::CodeAssemblerScopedExceptionHandler s(&ca_, &"
         << catch_name << "_label);\n";
  }
  return catch_name;
}

void CSAGenerator::PostCallableExceptionPreparation(
    const std::string& catch_name, const Type* return_type,
    base::Optional<Block*> catch_block, Stack<std::string>* stack) {
  if (catch_block) {
    std::string block_name = BlockName(*catch_block);
    out_ << "    }\n";
    out_ << "    if (" << catch_name << "_label.is_used()) {\n";
    out_ << "      compiler::CodeAssemblerLabel " << catch_name
         << "_skip(&ca_);\n";
    if (!return_type->IsNever()) {
      out_ << "      ca_.Goto(&" << catch_name << "_skip);\n";
    }
    out_ << "      compiler::TNode<Object> " << catch_name
         << "_exception_object;\n";
    out_ << "      ca_.Bind(&" << catch_name << "_label, &" << catch_name
         << "_exception_object);\n";
    out_ << "      ca_.Goto(&" << block_name;
    for (size_t i = 0; i < stack->Size(); ++i) {
      out_ << ", " << stack->begin()[i];
    }
    out_ << ", " << catch_name << "_exception_object);\n";
    if (!return_type->IsNever()) {
      out_ << "      ca_.Bind(&" << catch_name << "_skip);\n";
    }
    out_ << "    }\n";
  }
}

void CSAGenerator::EmitInstruction(const CallRuntimeInstruction& instruction,
                                   Stack<std::string>* stack) {
  std::vector<std::string> arguments = stack->PopMany(instruction.argc);
  const Type* return_type =
      instruction.runtime_function->signature().return_type;
  std::vector<const Type*> result_types;
  if (return_type != TypeOracle::GetNeverType()) {
    result_types = LowerType(return_type);
  }
  if (result_types.size() > 1) {
    ReportError("runtime function must have at most one result");
  }
  if (instruction.is_tailcall) {
    out_ << "    CodeStubAssembler(state_).TailCallRuntime(Runtime::k"
         << instruction.runtime_function->ExternalName() << ", ";
    PrintCommaSeparatedList(out_, arguments);
    out_ << ");\n";
  } else {
    std::string result_name = FreshNodeName();
    if (result_types.size() == 1) {
      out_ << "    compiler::TNode<"
           << result_types[0]->GetGeneratedTNodeTypeName() << "> "
           << result_name << ";\n";
    }
    std::string catch_name =
        PreCallableExceptionPreparation(instruction.catch_block);
    Stack<std::string> pre_call_stack = *stack;
    if (result_types.size() == 1) {
      stack->Push(result_name);
      out_ << "    " << result_name
           << " = TORQUE_CAST(CodeStubAssembler(state_).CallRuntime(Runtime::k"
           << instruction.runtime_function->ExternalName() << ", ";
      PrintCommaSeparatedList(out_, arguments);
      out_ << "));\n";
      out_ << "    USE(" << result_name << ");\n";
    } else {
      DCHECK_EQ(0, result_types.size());
      out_ << "    CodeStubAssembler(state_).CallRuntime(Runtime::k"
           << instruction.runtime_function->ExternalName() << ", ";
      PrintCommaSeparatedList(out_, arguments);
      out_ << ");\n";
      if (return_type == TypeOracle::GetNeverType()) {
        out_ << "    CodeStubAssembler(state_).Unreachable();\n";
      } else {
        DCHECK(return_type == TypeOracle::GetVoidType());
      }
    }
    PostCallableExceptionPreparation(catch_name, return_type,
                                     instruction.catch_block, &pre_call_stack);
  }
}

void CSAGenerator::EmitInstruction(const BranchInstruction& instruction,
                                   Stack<std::string>* stack) {
  out_ << "    ca_.Branch(" << stack->Pop() << ", &"
       << BlockName(instruction.if_true) << ", &"
       << BlockName(instruction.if_false);
  for (const std::string& value : *stack) {
    out_ << ", " << value;
  }
  out_ << ");\n";
}

void CSAGenerator::EmitInstruction(
    const ConstexprBranchInstruction& instruction, Stack<std::string>* stack) {
  out_ << "    if ((" << instruction.condition << ")) {\n";
  out_ << "      ca_.Goto(&" << BlockName(instruction.if_true);
  for (const std::string& value : *stack) {
    out_ << ", " << value;
  }
  out_ << ");\n";
  out_ << "    } else {\n";
  out_ << "      ca_.Goto(&" << BlockName(instruction.if_false);
  for (const std::string& value : *stack) {
    out_ << ", " << value;
  }
  out_ << ");\n";

  out_ << "    }\n";
}

void CSAGenerator::EmitInstruction(const GotoInstruction& instruction,
                                   Stack<std::string>* stack) {
  out_ << "    ca_.Goto(&" << BlockName(instruction.destination);
  for (const std::string& value : *stack) {
    out_ << ", " << value;
  }
  out_ << ");\n";
}

void CSAGenerator::EmitInstruction(const GotoExternalInstruction& instruction,
                                   Stack<std::string>* stack) {
  for (auto it = instruction.variable_names.rbegin();
       it != instruction.variable_names.rend(); ++it) {
    out_ << "    *" << *it << " = " << stack->Pop() << ";\n";
  }
  out_ << "    ca_.Goto(" << instruction.destination << ");\n";
}

void CSAGenerator::EmitInstruction(const ReturnInstruction& instruction,
                                   Stack<std::string>* stack) {
  if (*linkage_ == Builtin::kVarArgsJavaScript) {
    out_ << "    " << ARGUMENTS_VARIABLE_STRING << ".PopAndReturn(";
  } else {
    out_ << "    CodeStubAssembler(state_).Return(";
  }
  out_ << stack->Pop() << ");\n";
}

void CSAGenerator::EmitInstruction(
    const PrintConstantStringInstruction& instruction,
    Stack<std::string>* stack) {
  out_ << "    CodeStubAssembler(state_).Print("
       << StringLiteralQuote(instruction.message) << ");\n";
}

void CSAGenerator::EmitInstruction(const AbortInstruction& instruction,
                                   Stack<std::string>* stack) {
  switch (instruction.kind) {
    case AbortInstruction::Kind::kUnreachable:
      DCHECK(instruction.message.empty());
      out_ << "    CodeStubAssembler(state_).Unreachable();\n";
      break;
    case AbortInstruction::Kind::kDebugBreak:
      DCHECK(instruction.message.empty());
      out_ << "    CodeStubAssembler(state_).DebugBreak();\n";
      break;
    case AbortInstruction::Kind::kAssertionFailure: {
      std::string file =
          StringLiteralQuote(SourceFileMap::GetSource(instruction.pos.source));
      out_ << "    CodeStubAssembler(state_).FailAssert("
           << StringLiteralQuote(instruction.message) << ", " << file << ", "
           << instruction.pos.start.line + 1 << ");\n";
      break;
    }
  }
}

void CSAGenerator::EmitInstruction(const UnsafeCastInstruction& instruction,
                                   Stack<std::string>* stack) {
  stack->Poke(stack->AboveTop() - 1,
              "ca_.UncheckedCast<" +
                  instruction.destination_type->GetGeneratedTNodeTypeName() +
                  ">(" + stack->Top() + ")");
}

void CSAGenerator::EmitInstruction(
    const CreateFieldReferenceInstruction& instruction,
    Stack<std::string>* stack) {
  const Field& field =
      instruction.class_type->LookupField(instruction.field_name);
  std::string offset_name = FreshNodeName();
  stack->Push(offset_name);

  out_ << "    compiler::TNode<IntPtrT> " << offset_name
       << " = ca_.IntPtrConstant(";
  if (instruction.class_type->IsExtern()) {
    out_ << field.aggregate->GetGeneratedTNodeTypeName() << "::k"
         << CamelifyString(field.name_and_type.name) << "Offset";
  } else {
    out_ << "FixedArray::kHeaderSize + " << field.offset;
  }
  out_ << ");\n"
       << "    USE(" << stack->Top() << ");\n";
}

void CSAGenerator::EmitInstruction(const LoadReferenceInstruction& instruction,
                                   Stack<std::string>* stack) {
  std::string result_name = FreshNodeName();

  std::string offset = stack->Pop();
  std::string object = stack->Pop();
  stack->Push(result_name);

  out_ << "    " << instruction.type->GetGeneratedTypeName() << result_name
       << " = CodeStubAssembler(state_).LoadReference<"
       << instruction.type->GetGeneratedTNodeTypeName()
       << ">(CodeStubAssembler::Reference{" << object << ", " << offset
       << "});\n";
}

void CSAGenerator::EmitInstruction(const StoreReferenceInstruction& instruction,
                                   Stack<std::string>* stack) {
  std::string value = stack->Pop();
  std::string offset = stack->Pop();
  std::string object = stack->Pop();

  out_ << "    CodeStubAssembler(state_).StoreReference(CodeStubAssembler::"
          "Reference{"
       << object << ", " << offset << "}, " << value << ");\n";
}

// static
void CSAGenerator::EmitCSAValue(VisitResult result,
                                const Stack<std::string>& values,
                                std::ostream& out) {
  if (!result.IsOnStack()) {
    out << result.constexpr_value();
  } else if (auto* struct_type = StructType::DynamicCast(result.type())) {
    out << struct_type->GetGeneratedTypeName() << "{";
    bool first = true;
    for (auto& field : struct_type->fields()) {
      if (!first) {
        out << ", ";
      }
      first = false;
      EmitCSAValue(ProjectStructField(result, field.name_and_type.name), values,
                   out);
    }
    out << "}";
  } else if (result.type()->IsReferenceType()) {
    DCHECK_EQ(2, result.stack_range().Size());
    size_t offset = result.stack_range().begin().offset;
    out << "CodeStubAssembler::Reference{" << values.Peek(BottomOffset{offset})
        << ", " << values.Peek(BottomOffset{offset + 1}) << "}";
  } else {
    DCHECK_EQ(1, result.stack_range().Size());
    out << "compiler::TNode<" << result.type()->GetGeneratedTNodeTypeName()
        << ">{" << values.Peek(result.stack_range().begin()) << "}";
  }
}

}  // namespace torque
}  // namespace internal
}  // namespace v8
