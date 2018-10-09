// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interpreter/setup-interpreter.h"

#include "src/handles-inl.h"
#include "src/interpreter/bytecodes.h"
#include "src/interpreter/interpreter-generator.h"
#include "src/interpreter/interpreter.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {
namespace interpreter {

namespace {
void PrintBuiltinSize(Bytecode bytecode, OperandScale operand_scale,
                      Handle<Code> code) {
  PrintF(stdout, "Ignition Handler, %s, %d\n",
         Bytecodes::ToString(bytecode, operand_scale).c_str(),
         code->InstructionSize());
}
}  // namespace

// static
void SetupInterpreter::InstallBytecodeHandlers(Interpreter* interpreter) {
  DCHECK(!interpreter->IsDispatchTableInitialized());
  HandleScope scope(interpreter->isolate_);
  // Canonicalize handles, so that we can share constant pool entries pointing
  // to code targets without dereferencing their handles.
  CanonicalHandleScope canonical(interpreter->isolate_);
  Address* dispatch_table = interpreter->dispatch_table_;

  // Generate bytecode handlers for all bytecodes and scales.
  const OperandScale kOperandScales[] = {
#define VALUE(Name, _) OperandScale::k##Name,
      OPERAND_SCALE_LIST(VALUE)
#undef VALUE
  };

  for (OperandScale operand_scale : kOperandScales) {
#define GENERATE_CODE(Name, ...)                                \
  InstallBytecodeHandler(interpreter->isolate_, dispatch_table, \
                         Bytecode::k##Name, operand_scale);
    BYTECODE_LIST(GENERATE_CODE)
#undef GENERATE_CODE
  }

  // Fill unused entries will the illegal bytecode handler.
  size_t illegal_index = Interpreter::GetDispatchTableIndex(
      Bytecode::kIllegal, OperandScale::kSingle);
  for (size_t index = 0; index < Interpreter::kDispatchTableSize; ++index) {
    if (dispatch_table[index] == kNullAddress) {
      dispatch_table[index] = dispatch_table[illegal_index];
    }
  }

  // Generate the DeserializeLazy handlers, one for each operand scale.
  Heap* heap = interpreter->isolate_->heap();
  DCHECK_EQ(Smi::kZero, heap->deserialize_lazy_handler());
  heap->SetDeserializeLazyHandler(*GenerateDeserializeLazyHandler(
      interpreter->isolate_, OperandScale::kSingle));
  DCHECK_EQ(Smi::kZero, heap->deserialize_lazy_handler_wide());
  heap->SetDeserializeLazyHandlerWide(*GenerateDeserializeLazyHandler(
      interpreter->isolate_, OperandScale::kDouble));
  DCHECK_EQ(Smi::kZero, heap->deserialize_lazy_handler_extra_wide());
  heap->SetDeserializeLazyHandlerExtraWide(*GenerateDeserializeLazyHandler(
      interpreter->isolate_, OperandScale::kQuadruple));

  // Initialization should have been successful.
  DCHECK(interpreter->IsDispatchTableInitialized());
}

// static
void SetupInterpreter::InstallBytecodeHandler(Isolate* isolate,
                                              Address* dispatch_table,
                                              Bytecode bytecode,
                                              OperandScale operand_scale) {
  if (!Bytecodes::BytecodeHasHandler(bytecode, operand_scale)) return;

  size_t index = Interpreter::GetDispatchTableIndex(bytecode, operand_scale);
  // Here we explicitly set the bytecode handler to not be a builtin with an
  // index of kNoBuiltinId.
  // TODO(delphick): Use builtins version instead.
  Handle<Code> code = GenerateBytecodeHandler(isolate, bytecode, operand_scale,
                                              Builtins::kNoBuiltinId);
  dispatch_table[index] = code->entry();

  if (FLAG_print_builtin_size) PrintBuiltinSize(bytecode, operand_scale, code);

#ifdef ENABLE_DISASSEMBLER
  if (FLAG_print_builtin_code) {
    std::string name = Bytecodes::ToString(bytecode, operand_scale);
    code->PrintBuiltinCode(isolate, name.c_str());
  }
#endif  // ENABLE_DISASSEMBLER
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
