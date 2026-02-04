// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/unittests/interpreter/bytecode-expectations-printer.h"

#include <iomanip>
#include <iostream>
#include <vector>

#include "include/libplatform/libplatform.h"
#include "include/v8-function.h"
#include "src/api/api-inl.h"
#include "src/base/logging.h"
#include "src/codegen/bailout-reason.h"
#include "src/codegen/source-position-table.h"
#include "src/flags/save-flags.h"
#include "src/interpreter/bytecode-array-iterator.h"
#include "src/interpreter/bytecode-generator.h"
#include "src/interpreter/bytecodes.h"
#include "src/interpreter/interpreter-intrinsics.h"
#include "src/interpreter/interpreter.h"
#include "src/objects/heap-number-inl.h"
#include "src/objects/module-inl.h"
#include "src/objects/objects-inl.h"
#include "src/runtime/runtime.h"
#include "src/utils/ostreams.h"

namespace v8 {
namespace internal {
namespace interpreter {

static const char* NameForNativeContextIntrinsicIndex(uint32_t idx) {
  switch (idx) {
    case Context::REFLECT_APPLY_INDEX:
      return "reflect_apply";
    case Context::REFLECT_CONSTRUCT_INDEX:
      return "reflect_construct";
    default:
      return "UnknownIntrinsicIndex";
  }
}

// static
const char* const BytecodeExpectationsPrinter::kIndent = "  ";

v8::Local<v8::String> BytecodeExpectationsPrinter::V8StringFromUTF8(
    const char* data) const {
  return v8::String::NewFromUtf8(isolate_, data).ToLocalChecked();
}

std::string BytecodeExpectationsPrinter::WrapCodeInFunction(
    const char* function_name, const std::string& function_body) const {
  std::ostringstream program_stream;
  program_stream << "function " << function_name << "() {" << function_body
                 << "}\n"
                 << function_name << "();";

  return program_stream.str();
}

v8::Local<v8::Script> BytecodeExpectationsPrinter::CompileScript(
    const char* program) const {
  v8::Local<v8::String> source = V8StringFromUTF8(program);
  return v8::Script::Compile(isolate_->GetCurrentContext(), source)
      .ToLocalChecked();
}

v8::Local<v8::Module> BytecodeExpectationsPrinter::CompileModule(
    const char* program) const {
  ScriptOrigin origin(Local<v8::Value>(), 0, 0, false, -1, Local<v8::Value>(),
                      false, false, true);
  v8::ScriptCompiler::Source source(V8StringFromUTF8(program), origin);
  return v8::ScriptCompiler::CompileModule(isolate_, &source).ToLocalChecked();
}

void BytecodeExpectationsPrinter::Run(v8::Local<v8::Script> script) const {
  MaybeLocal<Value> result = script->Run(isolate_->GetCurrentContext());
  USE(result);
}

i::Handle<v8::internal::BytecodeArray>
BytecodeExpectationsPrinter::GetBytecodeArrayForGlobal(
    const char* global_name) const {
  const v8::Local<v8::Context>& context = isolate_->GetCurrentContext();
  v8::Local<v8::String> v8_global_name = V8StringFromUTF8(global_name);
  v8::Local<v8::Function> function = v8::Local<v8::Function>::Cast(
      context->Global()->Get(context, v8_global_name).ToLocalChecked());
  i::DirectHandle<i::JSFunction> js_function =
      i::Cast<i::JSFunction>(v8::Utils::OpenDirectHandle(*function));

  i::Handle<i::BytecodeArray> bytecodes = i::handle(
      js_function->shared()->GetBytecodeArray(i_isolate()), i_isolate());

  return bytecodes;
}

i::Handle<i::BytecodeArray>
BytecodeExpectationsPrinter::GetBytecodeArrayForModule(
    v8::Local<v8::Module> module) const {
  i::DirectHandle<i::Module> i_module = v8::Utils::OpenDirectHandle(*module);
  return i::handle(
      Cast<SharedFunctionInfo>(Cast<i::SourceTextModule>(i_module)->code())
          ->GetBytecodeArray(i_isolate()),
      i_isolate());
}

i::Handle<i::BytecodeArray>
BytecodeExpectationsPrinter::GetBytecodeArrayForScript(
    v8::Local<v8::Script> script) const {
  i::DirectHandle<i::JSFunction> js_function =
      v8::Utils::OpenDirectHandle(*script);
  return i::handle(js_function->shared()->GetBytecodeArray(i_isolate()),
                   i_isolate());
}

i::Handle<i::BytecodeArray>
BytecodeExpectationsPrinter::GetBytecodeArrayOfCallee(
    const char* source_code) const {
  Local<v8::Context> context = isolate_->GetCurrentContext();
  Local<v8::Script> script =
      v8::Script::Compile(
          context,
          v8::String::NewFromUtf8(isolate_, source_code).ToLocalChecked())
          .ToLocalChecked();
  i::DirectHandle<i::Object> i_object =
      v8::Utils::OpenDirectHandle(*script->Run(context).ToLocalChecked());
  i::DirectHandle<i::JSFunction> js_function = i::Cast<i::JSFunction>(i_object);
  CHECK(js_function->shared()->HasBytecodeArray());
  return i::handle(js_function->shared()->GetBytecodeArray(i_isolate()),
                   i_isolate());
}

void BytecodeExpectationsPrinter::PrintEscapedString(
    std::ostream* stream, const std::string& string) const {
  for (char c : string) {
    switch (c) {
      case '"':
        *stream << "\\\"";
        break;
      case '\\':
        *stream << "\\\\";
        break;
      default:
        *stream << c;
        break;
    }
  }
}

void BytecodeExpectationsPrinter::PrintBytecodeOperand(
    std::ostream* stream, const BytecodeArrayIterator& bytecode_iterator,
    const Bytecode& bytecode, int op_index, int parameter_count) const {
  OperandType op_type = Bytecodes::GetOperandType(bytecode, op_index);
  OperandSize op_size = Bytecodes::GetOperandSize(
      bytecode, op_index, bytecode_iterator.current_operand_scale());

  const char* size_tag;
  switch (op_size) {
    case OperandSize::kByte:
      size_tag = "8";
      break;
    case OperandSize::kShort:
      size_tag = "16";
      break;
    case OperandSize::kQuad:
      size_tag = "32";
      break;
    default:
      UNREACHABLE();
  }

  if (Bytecodes::IsRegisterOperandType(op_type)) {
    Register register_value = bytecode_iterator.GetRegisterOperand(op_index);
    *stream << 'R';
    if (op_size != OperandSize::kByte) *stream << size_tag;
    if (register_value.is_current_context()) {
      *stream << "(context)";
    } else if (register_value.is_function_closure()) {
      *stream << "(closure)";
    } else if (register_value.is_parameter()) {
      int parameter_index = register_value.ToParameterIndex();
      if (parameter_index == 0) {
        *stream << "(this)";
      } else {
        *stream << "(arg" << (parameter_index - 1) << ')';
      }
    } else {
      *stream << '(' << register_value.index() << ')';
    }
  } else {
    switch (op_type) {
      case OperandType::kFlag8:
        *stream << "Flag" << size_tag << '(';
        *stream << "0x" << std::hex
                << bytecode_iterator.GetFlag8Operand(op_index) << std::dec;
        break;
      case OperandType::kFlag16:
        *stream << "Flag" << size_tag << '(';
        *stream << "0x" << std::hex
                << bytecode_iterator.GetFlag16Operand(op_index) << std::dec;
        break;
      case OperandType::kEmbeddedFeedback: {
        // Ignore embedded feedback bytes in bytecode expectation test.
        DCHECK(Bytecodes::IsEmbeddedFeedbackBytecode(bytecode));
        DCHECK_EQ(op_index, kEmbeddedFeedbackOperandIndex);
        *stream << "EmbeddedFeedback(";
        break;
      }
      case OperandType::kConstantPoolIndex: {
        *stream << 'U' << size_tag << '(';
        *stream << bytecode_iterator.GetConstantPoolIndexOperand(op_index);

        Handle<Object> constant =
            bytecode_iterator.GetConstantForOperand(op_index, i_isolate());
        *stream << ":";
        // For strings, just print the value, since the instance type isn't that
        // interesting (and is in the constant pool if we need it).
        if (Handle<String> string; TryCast(constant, &string)) {
          PrintV8String(stream, *string);
        } else {
          // Otherwise print the full constant with instance type, same as in
          // the constant pool. This is a bit redundant with the constant pool
          // printing and the index, but it can help align diffs a bit better if
          // the constant pool changes.
          PrintConstant(stream, constant);
        }
        break;
      }
      case OperandType::kFeedbackSlot:
        *stream << "FBV";
        if (op_size != OperandSize::kByte) *stream << size_tag;
        *stream << '(';
        *stream << bytecode_iterator.GetFeedbackSlotOperand(op_index);
        break;
      case OperandType::kContextSlot:
        *stream << 'C' << size_tag << '(';
        *stream << bytecode_iterator.GetContextSlotOperand(op_index);
        break;
      case OperandType::kCoverageSlot:
        *stream << 'c' << size_tag << '(';
        *stream << bytecode_iterator.GetCoverageSlotOperand(op_index);
        break;
      case OperandType::kUImm:
        *stream << 'U' << size_tag << '(';
        *stream << bytecode_iterator.GetUnsignedImmediateOperand(op_index);
        break;
      case OperandType::kImm:
        *stream << 'I' << size_tag << '(';
        *stream << bytecode_iterator.GetImmediateOperand(op_index);
        break;
      case OperandType::kRegCount:
        *stream << "RegCount" << size_tag << '(';
        *stream << bytecode_iterator.GetRegisterCountOperand(op_index);
        break;
      case OperandType::kRuntimeId: {
        *stream << 'U' << size_tag << '(';
        Runtime::FunctionId id =
            bytecode_iterator.GetRuntimeIdOperand(op_index);
        *stream << "Runtime::k" << i::Runtime::FunctionForId(id)->name;
        break;
      }
      case OperandType::kIntrinsicId: {
        *stream << 'U' << size_tag << '(';
        Runtime::FunctionId id =
            bytecode_iterator.GetIntrinsicIdOperand(op_index);
        *stream << "Runtime::k" << i::Runtime::FunctionForId(id)->name;
        break;
      }
      case OperandType::kNativeContextIndex: {
        *stream << 'U' << size_tag << '(';
        uint32_t idx = bytecode_iterator.GetNativeContextIndexOperand(op_index);
        *stream << "%" << NameForNativeContextIntrinsicIndex(idx);
        break;
      }
      case OperandType::kAbortReason: {
        *stream << 'U' << size_tag << '(';
        AbortReason reason = bytecode_iterator.GetAbortReasonOperand(op_index);
        if (IsValidAbortReason(static_cast<int>(reason))) {
          *stream << "AbortReason::" << GetAbortReason(reason);
        } else {
          *stream << "Invalid abort reason: " << static_cast<int>(reason);
        }
        break;
      }
      default:
        UNREACHABLE();
    }

    *stream << ')';
  }
}

void BytecodeExpectationsPrinter::PrintBytecode(
    std::ostream* stream, const BytecodeArrayIterator& bytecode_iterator,
    int parameter_count) const {
  Bytecode bytecode = bytecode_iterator.current_bytecode();
  OperandScale operand_scale = bytecode_iterator.current_operand_scale();
  if (Bytecodes::OperandScaleRequiresPrefixBytecode(operand_scale)) {
    Bytecode prefix = Bytecodes::OperandScaleToPrefixBytecode(operand_scale);
    *stream << "B(" << Bytecodes::ToString(prefix) << "), ";
  }
  *stream << "B(" << Bytecodes::ToString(bytecode) << ')';
  int operands_count = Bytecodes::NumberOfOperands(bytecode);
  for (int op_index = 0; op_index < operands_count; ++op_index) {
    *stream << ", ";
    PrintBytecodeOperand(stream, bytecode_iterator, bytecode, op_index,
                         parameter_count);
  }
}

void BytecodeExpectationsPrinter::PrintSourcePosition(
    std::ostream* stream, SourcePositionTableIterator* source_iterator,
    int bytecode_offset) const {
  static const size_t kPositionWidth = 4;
  if (!source_iterator->done() &&
      source_iterator->code_offset() == bytecode_offset) {
    *stream << "/* " << std::setw(kPositionWidth)
            << source_iterator->source_position().ScriptOffset();
    if (source_iterator->is_statement()) {
      *stream << (source_iterator->is_breakable() ? " S> */ " : " s> */ ");
    } else {
      *stream << (source_iterator->is_breakable() ? " E> */ " : " e> */ ");
    }
    source_iterator->Advance();
  } else {
    *stream << "   " << std::setw(kPositionWidth) << ' ' << "       ";
  }
}

void BytecodeExpectationsPrinter::PrintV8String(
    std::ostream* stream, i::Tagged<i::String> string) const {
  *stream << '"';
  for (int i = 0, length = string->length(); i < length; ++i) {
    *stream << i::AsEscapedUC16ForJSON(string->Get(i));
  }
  *stream << '"';
}

void BytecodeExpectationsPrinter::PrintConstant(
    std::ostream* stream, i::DirectHandle<i::Object> constant) const {
  if (IsSmi(*constant)) {
    *stream << "Smi [";
    i::Smi::SmiPrint(i::Cast<i::Smi>(*constant), *stream);
    *stream << "]";
  } else {
    *stream << i::Cast<i::HeapObject>(*constant)->map()->instance_type();
    if (IsHeapNumber(*constant)) {
      *stream << " [";
      i::Cast<i::HeapNumber>(*constant)->HeapNumberShortPrint(*stream);
      *stream << "]";
    } else if (IsString(*constant)) {
      *stream << " [";
      PrintV8String(stream, i::Cast<i::String>(*constant));
      *stream << "]";
    }
  }
}

void BytecodeExpectationsPrinter::PrintFrameSize(
    std::ostream* stream,
    i::DirectHandle<i::BytecodeArray> bytecode_array) const {
  int32_t frame_size = bytecode_array->frame_size();

  DCHECK(IsAligned(frame_size, kSystemPointerSize));
  *stream << "frame size: " << frame_size / kSystemPointerSize
          << "\nparameter count: " << bytecode_array->parameter_count() << '\n';
}

void BytecodeExpectationsPrinter::PrintBytecodeSequence(
    std::ostream* stream, i::Handle<i::BytecodeArray> bytecode_array) const {
  *stream << "bytecode array length: " << bytecode_array->length()
          << "\nbytecodes: [\n";

  SourcePositionTableIterator source_iterator(
      bytecode_array->SourcePositionTable());
  BytecodeArrayIterator bytecode_iterator(bytecode_array);
  for (; !bytecode_iterator.done(); bytecode_iterator.Advance()) {
    *stream << kIndent;
    PrintSourcePosition(stream, &source_iterator,
                        bytecode_iterator.current_offset());
    PrintBytecode(stream, bytecode_iterator, bytecode_array->parameter_count());
    *stream << ",\n";
  }
  *stream << "]\n";
}

void BytecodeExpectationsPrinter::PrintConstantPool(
    std::ostream* stream, i::Tagged<i::TrustedFixedArray> constant_pool) const {
  *stream << "constant pool: [\n";
  int num_constants = constant_pool->length();
  if (num_constants > 0) {
    for (int i = 0; i < num_constants; ++i) {
      *stream << kIndent;
      PrintConstant(stream, direct_handle(constant_pool->get(i), i_isolate()));
      *stream << ",\n";
    }
  }
  *stream << "]\n";
}

void BytecodeExpectationsPrinter::PrintCodeSnippet(
    std::ostream* stream, const std::string& body) const {
  *stream << "snippet: \"\n";
  std::stringstream body_stream(body);
  std::string body_line;
  while (std::getline(body_stream, body_line)) {
    *stream << kIndent;
    PrintEscapedString(stream, body_line);
    *stream << '\n';
  }
  *stream << "\"\n";
}

void BytecodeExpectationsPrinter::PrintHandlers(
    std::ostream* stream,
    i::DirectHandle<i::BytecodeArray> bytecode_array) const {
  *stream << "handlers: [\n";
  HandlerTable table(*bytecode_array);
  for (int i = 0, num_entries = table.NumberOfRangeEntries(); i < num_entries;
       ++i) {
    *stream << "  [" << table.GetRangeStart(i) << ", " << table.GetRangeEnd(i)
            << ", " << table.GetRangeHandler(i) << "],\n";
  }
  *stream << "]\n";
}

void BytecodeExpectationsPrinter::PrintBytecodeArray(
    std::ostream* stream, i::Handle<i::BytecodeArray> bytecode_array) const {
  PrintFrameSize(stream, bytecode_array);
  PrintBytecodeSequence(stream, bytecode_array);
  PrintConstantPool(stream, bytecode_array->constant_pool());
  PrintHandlers(stream, bytecode_array);
}

static constexpr const char* kDefaultTopFunctionName = "__genbckexp_wrapper__";

void BytecodeExpectationsPrinter::PrintExpectation(
    std::ostream* stream, const std::string& snippet) const {
  const char* test_function_name = options_.test_function_name.empty()
                                       ? kDefaultTopFunctionName
                                       : options_.test_function_name.c_str();
  std::string source_code =
      options_.wrap ? WrapCodeInFunction(test_function_name, snippet) : snippet;

  SaveFlags save_flags;

  if (!options_.extra_flags.empty()) {
    v8::V8::SetFlagsFromString(options_.extra_flags.c_str());
  }

  i::v8_flags.compilation_cache = false;
  i::v8_flags.lazy = false;
  i::v8_flags.flush_bytecode = false;
  i::Handle<i::BytecodeArray> bytecode_array;
  if (options_.module) {
    CHECK(options_.top_level && !options_.wrap);
    v8::Local<v8::Module> module = CompileModule(source_code.c_str());
    bytecode_array = GetBytecodeArrayForModule(module);
  } else if (options_.print_callee) {
    bytecode_array = GetBytecodeArrayOfCallee(source_code.c_str());
  } else {
    v8::Local<v8::Script> script = CompileScript(source_code.c_str());
    if (options_.top_level) {
      bytecode_array = GetBytecodeArrayForScript(script);
    } else {
      Run(script);
      bytecode_array = GetBytecodeArrayForGlobal(test_function_name);
    }
  }

  PrintCodeSnippet(stream, snippet);
  PrintBytecodeArray(stream, bytecode_array);
  *stream << std::endl;
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
