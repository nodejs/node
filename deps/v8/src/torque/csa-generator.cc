// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/torque/csa-generator.h"

#include "src/torque/type-oracle.h"
#include "src/torque/utils.h"

namespace v8 {
namespace internal {
namespace torque {

base::Optional<Stack<std::string>> CSAGenerator::EmitGraph(
    Stack<std::string> parameters) {
  for (Block* block : cfg_.blocks()) {
    out_ << "  PLabel<";
    PrintCommaSeparatedList(out_, block->InputTypes(), [](const Type* t) {
      return t->GetGeneratedTNodeTypeName();
    });
    out_ << "> " << BlockName(block) << "(this, compiler::CodeAssemblerLabel::"
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
    out_ << "    TNode<" << t->GetGeneratedTNodeTypeName() << "> "
         << stack.Top() << ";\n";
  }
  out_ << "    Bind(&" << BlockName(block);
  for (const std::string& name : stack) {
    out_ << ", &" << name;
  }
  out_ << ");\n";
  for (const Instruction& instruction : block->instructions()) {
    EmitInstruction(instruction, &stack);
  }
  return stack;
}

void CSAGenerator::EmitInstruction(const Instruction& instruction,
                                   Stack<std::string>* stack) {
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
  stack->Push("Uninitialized<" + instruction.type->GetGeneratedTNodeTypeName() +
              ">()");
}

void CSAGenerator::EmitInstruction(
    const PushCodePointerInstruction& instruction, Stack<std::string>* stack) {
  stack->Push(
      "UncheckedCast<Code>(HeapConstant(Builtins::CallableFor(isolate(), "
      "Builtins::k" +
      instruction.external_name + ").code()))");
}

void CSAGenerator::EmitInstruction(const ModuleConstantInstruction& instruction,
                                   Stack<std::string>* stack) {
  const Type* type = instruction.constant->type();
  std::vector<std::string> results;
  for (const Type* lowered : LowerType(type)) {
    results.push_back(FreshNodeName());
    stack->Push(results.back());
    out_ << "    TNode<" << lowered->GetGeneratedTNodeTypeName() << "> "
         << stack->Top() << ";\n";
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
  out_ << instruction.constant->constant_name() << "()";
  if (type->IsStructType()) {
    out_ << ".Flatten();\n";
  } else {
    out_ << ";\n";
  }
}

void CSAGenerator::EmitInstruction(const CallCsaMacroInstruction& instruction,
                                   Stack<std::string>* stack) {
  std::vector<std::string> constexpr_arguments =
      instruction.constexpr_arguments;
  std::vector<std::string> args;
  TypeVector parameter_types =
      instruction.macro->signature().parameter_types.types;
  for (auto it = parameter_types.rbegin(); it != parameter_types.rend(); ++it) {
    const Type* type = *it;
    VisitResult arg;
    if (type->IsConstexpr()) {
      args.push_back(std::move(constexpr_arguments.back()));
      constexpr_arguments.pop_back();
    } else {
      std::stringstream s;
      size_t slot_count = LoweredSlotCount(type);
      VisitResult arg = VisitResult(type, stack->TopRange(slot_count));
      EmitCSAValue(arg, *stack, s);
      args.push_back(s.str());
      stack->PopMany(slot_count);
    }
  }
  std::reverse(args.begin(), args.end());

  const Type* return_type = instruction.macro->signature().return_type;
  std::vector<std::string> results;
  for (const Type* type : LowerType(return_type)) {
    results.push_back(FreshNodeName());
    stack->Push(results.back());
    out_ << "    TNode<" << type->GetGeneratedTNodeTypeName() << "> "
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
      out_ << results[0] << " = UncheckedCast<"
           << return_type->GetGeneratedTNodeTypeName() << ">(";
    }
  }
  out_ << instruction.macro->name() << "(";
  PrintCommaSeparatedList(out_, args);
  if (return_type->IsStructType()) {
    out_ << ").Flatten();\n";
  } else {
    if (results.size() == 1) out_ << ")";
    out_ << ");\n";
  }
}

void CSAGenerator::EmitInstruction(
    const CallCsaMacroAndBranchInstruction& instruction,
    Stack<std::string>* stack) {
  std::vector<std::string> constexpr_arguments =
      instruction.constexpr_arguments;
  std::vector<std::string> args;
  TypeVector parameter_types =
      instruction.macro->signature().parameter_types.types;
  for (auto it = parameter_types.rbegin(); it != parameter_types.rend(); ++it) {
    const Type* type = *it;
    VisitResult arg;
    if (type->IsConstexpr()) {
      args.push_back(std::move(constexpr_arguments.back()));
      constexpr_arguments.pop_back();
    } else {
      std::stringstream s;
      size_t slot_count = LoweredSlotCount(type);
      VisitResult arg = VisitResult(type, stack->TopRange(slot_count));
      EmitCSAValue(arg, *stack, s);
      args.push_back(s.str());
      stack->PopMany(slot_count);
    }
  }
  std::reverse(args.begin(), args.end());

  std::vector<std::string> results;
  const Type* return_type = instruction.macro->signature().return_type;
  if (return_type != TypeOracle::GetNeverType()) {
    for (const Type* type :
         LowerType(instruction.macro->signature().return_type)) {
      results.push_back(FreshNodeName());
      out_ << "    TNode<" << type->GetGeneratedTNodeTypeName() << "> "
           << results.back() << ";\n";
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
      out_ << "    TVariable<"
           << label_parameters[j]->GetGeneratedTNodeTypeName() << "> "
           << var_names[i][j] << "(this);\n";
    }
    out_ << "    Label " << label_names[i] << "(this);\n";
  }

  out_ << "    ";
  if (results.size() == 1) {
    out_ << results[0] << " = ";
  } else if (results.size() > 1) {
    out_ << "std::tie(";
    PrintCommaSeparatedList(out_, results);
    out_ << ") = ";
  }
  out_ << instruction.macro->name() << "(";
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
  out_ << ");\n";
  if (instruction.return_continuation) {
    out_ << "    Goto(&" << BlockName(*instruction.return_continuation);
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
    out_ << "      Bind(&" << label_names[i] << ");\n";
    out_ << "      Goto(&" << BlockName(instruction.label_blocks[i]);
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
    out_ << "   TailCallBuiltin(Builtins::k" << instruction.builtin->name()
         << ", ";
    PrintCommaSeparatedList(out_, arguments);
    out_ << ");\n";
  } else {
    if (result_types.size() == 1) {
      std::string generated_type = result_types[0]->GetGeneratedTNodeTypeName();
      stack->Push(FreshNodeName());
      out_ << "    TNode<" << generated_type << "> " << stack->Top() << " = ";
      if (generated_type != "Object") out_ << "CAST(";
      out_ << "CallBuiltin(Builtins::k" << instruction.builtin->name() << ", ";
      PrintCommaSeparatedList(out_, arguments);
      if (generated_type != "Object") out_ << ")";
      out_ << ");\n";
      out_ << "    USE(" << stack->Top() << ");\n";
    } else {
      DCHECK_EQ(0, result_types.size());
      // TODO(tebbi): Actually, builtins have to return a value, so we should
      // not have to handle this case.
      out_ << "    CallBuiltin(Builtins::k" << instruction.builtin->name()
           << ", ";
      PrintCommaSeparatedList(out_, arguments);
      out_ << ");\n";
    }
  }
}

void CSAGenerator::EmitInstruction(
    const CallBuiltinPointerInstruction& instruction,
    Stack<std::string>* stack) {
  std::vector<std::string> function_and_arguments =
      stack->PopMany(1 + instruction.argc);
  std::vector<const Type*> result_types =
      LowerType(instruction.example_builtin->signature().return_type);
  if (result_types.size() != 1) {
    ReportError("builtins must have exactly one result");
  }
  if (instruction.is_tailcall) {
    out_ << "    Tail (Builtins::CallableFor(isolate(), Builtins::k"
         << instruction.example_builtin->name() << ").descriptor(), ";
    PrintCommaSeparatedList(out_, function_and_arguments);
    out_ << ");\n";
  } else {
    stack->Push(FreshNodeName());
    std::string generated_type = result_types[0]->GetGeneratedTNodeTypeName();
    out_ << "    TNode<" << generated_type << "> " << stack->Top() << " = ";
    if (generated_type != "Object") out_ << "CAST(";
    out_ << "CallStub(Builtins::CallableFor(isolate(), Builtins::k"
         << instruction.example_builtin->name() << ").descriptor(), ";
    PrintCommaSeparatedList(out_, function_and_arguments);
    out_ << ")";
    if (generated_type != "Object") out_ << ")";
    out_ << "; \n";
    out_ << "    USE(" << stack->Top() << ");\n";
  }
}

void CSAGenerator::EmitInstruction(const CallRuntimeInstruction& instruction,
                                   Stack<std::string>* stack) {
  std::vector<std::string> arguments = stack->PopMany(instruction.argc);
  std::vector<const Type*> result_types =
      LowerType(instruction.runtime_function->signature().return_type);
  if (result_types.size() > 1) {
    ReportError("runtime function must have at most one result");
  }
  if (instruction.is_tailcall) {
    out_ << "    TailCallRuntime(Runtime::k"
         << instruction.runtime_function->name() << ", ";
    PrintCommaSeparatedList(out_, arguments);
    out_ << ");\n";
  } else {
    if (result_types.size() == 1) {
      stack->Push(FreshNodeName());
      out_ << "    TNode<" << result_types[0]->GetGeneratedTNodeTypeName()
           << "> " << stack->Top() << " = CAST(CallRuntime(Runtime::k"
           << instruction.runtime_function->name() << ", ";
      PrintCommaSeparatedList(out_, arguments);
      out_ << "));\n";
      out_ << "    USE(" << stack->Top() << ");\n";
    } else {
      DCHECK_EQ(0, result_types.size());
      // TODO(tebbi): Actually, runtime functions have to return a value, so we
      // should not have to handle this case.
      out_ << "    CallRuntime(Runtime::k"
           << instruction.runtime_function->name() << ", ";
      PrintCommaSeparatedList(out_, arguments);
      out_ << ");\n";
    }
  }
}

void CSAGenerator::EmitInstruction(const BranchInstruction& instruction,
                                   Stack<std::string>* stack) {
  out_ << "    Branch(" << stack->Pop() << ", &"
       << BlockName(instruction.if_true) << ", &"
       << BlockName(instruction.if_false);
  for (const std::string& value : *stack) {
    out_ << ", " << value;
  }
  out_ << ");\n";
}

void CSAGenerator::EmitInstruction(
    const ConstexprBranchInstruction& instruction, Stack<std::string>* stack) {
  out_ << "    if (" << instruction.condition << ") {\n";
  out_ << "      Goto(&" << BlockName(instruction.if_true);
  for (const std::string& value : *stack) {
    out_ << ", " << value;
  }
  out_ << ");\n";
  out_ << "    } else {\n";
  out_ << "      Goto(&" << BlockName(instruction.if_false);
  for (const std::string& value : *stack) {
    out_ << ", " << value;
  }
  out_ << ");\n";

  out_ << "    }\n";
}

void CSAGenerator::EmitInstruction(const GotoInstruction& instruction,
                                   Stack<std::string>* stack) {
  out_ << "    Goto(&" << BlockName(instruction.destination);
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
  out_ << "    Goto(" << instruction.destination << ");\n";
}

void CSAGenerator::EmitInstruction(const ReturnInstruction& instruction,
                                   Stack<std::string>* stack) {
  if (*linkage_ == Builtin::kVarArgsJavaScript) {
    out_ << "    " << ARGUMENTS_VARIABLE_STRING << "->PopAndReturn(";
  } else {
    out_ << "    Return(";
  }
  out_ << stack->Pop() << ");\n";
}

void CSAGenerator::EmitInstruction(
    const PrintConstantStringInstruction& instruction,
    Stack<std::string>* stack) {
  out_ << "    Print(" << StringLiteralQuote(instruction.message) << ");\n";
}

void CSAGenerator::EmitInstruction(const DebugBreakInstruction& instruction,
                                   Stack<std::string>* stack) {
  if (instruction.never_continues) {
    out_ << "    Unreachable();\n";
  } else {
    out_ << "    DebugBreak();\n";
  }
}

void CSAGenerator::EmitInstruction(const UnsafeCastInstruction& instruction,
                                   Stack<std::string>* stack) {
  stack->Poke(stack->AboveTop() - 1,
              "UncheckedCast<" +
                  instruction.destination_type->GetGeneratedTNodeTypeName() +
                  ">(" + stack->Top() + ")");
}

// static
void CSAGenerator::EmitCSAValue(VisitResult result,
                                const Stack<std::string>& values,
                                std::ostream& out) {
  if (!result.IsOnStack()) {
    out << result.constexpr_value();
  } else if (auto* struct_type = StructType::DynamicCast(result.type())) {
    out << struct_type->name() << "{";
    bool first = true;
    for (auto& field : struct_type->fields()) {
      if (!first) {
        out << ", ";
      }
      first = false;
      EmitCSAValue(ProjectStructField(result, field.name), values, out);
    }
    out << "}";
  } else {
    DCHECK_EQ(1, result.stack_range().Size());
    out << "TNode<" << result.type()->GetGeneratedTNodeTypeName() << ">{"
        << values.Peek(result.stack_range().begin()) << "}";
  }
}

}  // namespace torque
}  // namespace internal
}  // namespace v8
