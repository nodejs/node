// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTERPRETER_INTERPRETER_H_
#define V8_INTERPRETER_INTERPRETER_H_

#include <memory>

// Clients of this interface shouldn't depend on lots of interpreter internals.
// Do not include anything from src/interpreter other than
// src/interpreter/bytecodes.h here!
#include "src/base/macros.h"
#include "src/builtins/builtins.h"
#include "src/interpreter/bytecodes.h"
#include "src/runtime/runtime.h"

namespace v8 {
namespace internal {

class Isolate;
class Callable;
class UnoptimizedCompilationJob;
class FunctionLiteral;
class ParseInfo;
class RootVisitor;
class SetupIsolateDelegate;
template <typename>
class ZoneVector;

namespace interpreter {

class InterpreterAssembler;

class Interpreter {
 public:
  explicit Interpreter(Isolate* isolate);
  virtual ~Interpreter() = default;

  // Returns the interrupt budget which should be used for the profiler counter.
  V8_EXPORT_PRIVATE static int InterruptBudget();

  // Creates a compilation job which will generate bytecode for |literal|.
  // Additionally, if |eager_inner_literals| is not null, adds any eagerly
  // compilable inner FunctionLiterals to this list.
  static std::unique_ptr<UnoptimizedCompilationJob> NewCompilationJob(
      ParseInfo* parse_info, FunctionLiteral* literal,
      AccountingAllocator* allocator,
      std::vector<FunctionLiteral*>* eager_inner_literals);

  // If the bytecode handler for |bytecode| and |operand_scale| has not yet
  // been loaded, deserialize it. Then return the handler.
  V8_EXPORT_PRIVATE Code GetBytecodeHandler(Bytecode bytecode,
                                            OperandScale operand_scale);

  // Set the bytecode handler for |bytecode| and |operand_scale|.
  void SetBytecodeHandler(Bytecode bytecode, OperandScale operand_scale,
                          Code handler);

  // GC support.
  void IterateDispatchTable(RootVisitor* v);

  // Disassembler support.
  V8_EXPORT_PRIVATE const char* LookupNameOfBytecodeHandler(const Code code);

  V8_EXPORT_PRIVATE Local<v8::Object> GetDispatchCountersObject();

  void ForEachBytecode(const std::function<void(Bytecode, OperandScale)>& f);

  void Initialize();

  bool IsDispatchTableInitialized() const;

  Address dispatch_table_address() {
    return reinterpret_cast<Address>(&dispatch_table_[0]);
  }

  Address bytecode_dispatch_counters_table() {
    return reinterpret_cast<Address>(bytecode_dispatch_counters_table_.get());
  }

  Address address_of_interpreter_entry_trampoline_instruction_start() const {
    return reinterpret_cast<Address>(
        &interpreter_entry_trampoline_instruction_start_);
  }

 private:
  friend class SetupInterpreter;
  friend class v8::internal::SetupIsolateDelegate;

  uintptr_t GetDispatchCounter(Bytecode from, Bytecode to) const;

  // Get dispatch table index of bytecode.
  static size_t GetDispatchTableIndex(Bytecode bytecode,
                                      OperandScale operand_scale);

  static const int kNumberOfWideVariants = BytecodeOperands::kOperandScaleCount;
  static const int kDispatchTableSize = kNumberOfWideVariants * (kMaxUInt8 + 1);
  static const int kNumberOfBytecodes = static_cast<int>(Bytecode::kLast) + 1;

  Isolate* isolate_;
  Address dispatch_table_[kDispatchTableSize];
  std::unique_ptr<uintptr_t[]> bytecode_dispatch_counters_table_;
  Address interpreter_entry_trampoline_instruction_start_;

  DISALLOW_COPY_AND_ASSIGN(Interpreter);
};

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

#endif  // V8_INTERPRETER_INTERPRETER_H_
