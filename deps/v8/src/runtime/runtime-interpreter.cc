// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/runtime/runtime-utils.h"

#include <iomanip>

#include "src/arguments.h"
#include "src/frames-inl.h"
#include "src/interpreter/bytecode-array-iterator.h"
#include "src/interpreter/bytecodes.h"
#include "src/isolate-inl.h"
#include "src/ostreams.h"

namespace v8 {
namespace internal {

RUNTIME_FUNCTION(Runtime_InterpreterToBoolean) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_CHECKED(Object, x, 0);
  return isolate->heap()->ToBoolean(x->BooleanValue());
}


RUNTIME_FUNCTION(Runtime_InterpreterLogicalNot) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_CHECKED(Object, x, 0);
  return isolate->heap()->ToBoolean(!x->BooleanValue());
}


RUNTIME_FUNCTION(Runtime_InterpreterTypeOf) {
  HandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, x, 0);
  return Object::cast(*Object::TypeOf(isolate, x));
}


RUNTIME_FUNCTION(Runtime_InterpreterNewClosure) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(SharedFunctionInfo, shared, 0);
  CONVERT_SMI_ARG_CHECKED(pretenured_flag, 1);
  Handle<Context> context(isolate->context(), isolate);
  return *isolate->factory()->NewFunctionFromSharedFunctionInfo(
      shared, context, static_cast<PretenureFlag>(pretenured_flag));
}

namespace {

void PrintRegisters(std::ostream& os, bool is_input,
                    Handle<BytecodeArray> bytecode_array, int bytecode_offset,
                    Handle<Object> accumulator) {
  static const int kRegFieldWidth = static_cast<int>(strlen("accumulator"));
  static const char* kInputColourCode = "\033[0;36m";
  static const char* kOutputColourCode = "\033[0;35m";
  static const char* kNormalColourCode = "\033[0;m";
  const char* kArrowDirection = is_input ? " -> " : " <- ";
  if (FLAG_log_colour) {
    os << (is_input ? kInputColourCode : kOutputColourCode);
  }

  // Print accumulator.
  os << "      [ accumulator" << kArrowDirection;
  accumulator->ShortPrint();
  os << " ]" << std::endl;

  // Find the location of the register file.
  JavaScriptFrameIterator frame_iterator(bytecode_array->GetIsolate());
  JavaScriptFrame* frame = frame_iterator.frame();
  Address register_file =
      frame->fp() + InterpreterFrameConstants::kRegisterFilePointerFromFp;

  // Print the registers.
  interpreter::BytecodeArrayIterator bytecode_iterator(bytecode_array);
  bytecode_iterator.set_current_offset(
      bytecode_offset - BytecodeArray::kHeaderSize + kHeapObjectTag);
  interpreter::Bytecode bytecode = bytecode_iterator.current_bytecode();
  int operand_count = interpreter::Bytecodes::NumberOfOperands(bytecode);
  for (int operand_index = 0; operand_index < operand_count; operand_index++) {
    interpreter::OperandType operand_type =
        interpreter::Bytecodes::GetOperandType(bytecode, operand_index);
    bool should_print =
        is_input
            ? interpreter::Bytecodes::IsRegisterInputOperandType(operand_type)
            : interpreter::Bytecodes::IsRegisterOutputOperandType(operand_type);
    if (should_print) {
      interpreter::Register first_reg =
          bytecode_iterator.GetRegisterOperand(operand_index);
      int range = bytecode_iterator.GetRegisterOperandRange(operand_index);
      for (int reg_index = first_reg.index();
           reg_index < first_reg.index() + range; reg_index++) {
        Address reg_location = register_file - reg_index * kPointerSize;
        Object* reg_object = Memory::Object_at(reg_location);
        os << "      [ " << std::setw(kRegFieldWidth)
           << interpreter::Register(reg_index).ToString(
                  bytecode_array->parameter_count())
           << kArrowDirection;
        reg_object->ShortPrint(os);
        os << " ]" << std::endl;
      }
    }
  }
  if (FLAG_log_colour) {
    os << kNormalColourCode;
  }
}

}  // namespace

RUNTIME_FUNCTION(Runtime_InterpreterTraceBytecodeEntry) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(3, args.length());
  CONVERT_ARG_HANDLE_CHECKED(BytecodeArray, bytecode_array, 0);
  CONVERT_SMI_ARG_CHECKED(bytecode_offset, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, accumulator, 2);
  OFStream os(stdout);

  // Print bytecode.
  const uint8_t* bytecode_address =
      reinterpret_cast<const uint8_t*>(*bytecode_array) + bytecode_offset;
  Vector<char> buf = Vector<char>::New(50);
  SNPrintF(buf, "%p", bytecode_address);
  os << " -> " << buf.start() << " (" << bytecode_offset << ") : ";
  interpreter::Bytecodes::Decode(os, bytecode_address,
                                 bytecode_array->parameter_count());
  os << std::endl;

  // Print all input registers and accumulator.
  PrintRegisters(os, true, bytecode_array, bytecode_offset, accumulator);

  os << std::flush;
  return isolate->heap()->undefined_value();
}

RUNTIME_FUNCTION(Runtime_InterpreterTraceBytecodeExit) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(3, args.length());
  CONVERT_ARG_HANDLE_CHECKED(BytecodeArray, bytecode_array, 0);
  CONVERT_SMI_ARG_CHECKED(bytecode_offset, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, accumulator, 2);
  OFStream os(stdout);

  // Print all output registers and accumulator.
  PrintRegisters(os, false, bytecode_array, bytecode_offset, accumulator);
  os << std::flush;
  return isolate->heap()->undefined_value();
}

RUNTIME_FUNCTION(Runtime_InterpreterClearPendingMessage) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(0, args.length());
  Object* message = isolate->thread_local_top()->pending_message_obj_;
  isolate->clear_pending_message();
  return message;
}

RUNTIME_FUNCTION(Runtime_InterpreterSetPendingMessage) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, message, 0);
  isolate->thread_local_top()->pending_message_obj_ = *message;
  return isolate->heap()->undefined_value();
}

}  // namespace internal
}  // namespace v8
