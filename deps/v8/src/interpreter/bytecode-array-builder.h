// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTERPRETER_BYTECODE_ARRAY_BUILDER_H_
#define V8_INTERPRETER_BYTECODE_ARRAY_BUILDER_H_

#include "src/ast/ast.h"
#include "src/base/compiler-specific.h"
#include "src/globals.h"
#include "src/interpreter/bytecode-array-writer.h"
#include "src/interpreter/bytecode-register-allocator.h"
#include "src/interpreter/bytecode-register.h"
#include "src/interpreter/bytecodes.h"
#include "src/interpreter/constant-array-builder.h"
#include "src/interpreter/handler-table-builder.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {

class Isolate;

namespace interpreter {

class BytecodeLabel;
class BytecodeNode;
class BytecodePipelineStage;
class BytecodeRegisterOptimizer;
class Register;

class V8_EXPORT_PRIVATE BytecodeArrayBuilder final
    : public NON_EXPORTED_BASE(ZoneObject) {
 public:
  BytecodeArrayBuilder(
      Isolate* isolate, Zone* zone, int parameter_count, int context_count,
      int locals_count, FunctionLiteral* literal = nullptr,
      SourcePositionTableBuilder::RecordingMode source_position_mode =
          SourcePositionTableBuilder::RECORD_SOURCE_POSITIONS);

  Handle<BytecodeArray> ToBytecodeArray(Isolate* isolate);

  // Get the number of parameters expected by function.
  int parameter_count() const {
    DCHECK_GE(parameter_count_, 0);
    return parameter_count_;
  }

  // Get the number of locals required for bytecode array.
  int locals_count() const {
    DCHECK_GE(local_register_count_, 0);
    return local_register_count_;
  }

  // Get number of contexts required for bytecode array.
  int context_count() const {
    DCHECK_GE(context_register_count_, 0);
    return context_register_count_;
  }

  Register first_context_register() const;
  Register last_context_register() const;

  // Returns the number of fixed (non-temporary) registers.
  int fixed_register_count() const { return context_count() + locals_count(); }

  // Returns the number of fixed and temporary registers.
  int total_register_count() const {
    DCHECK_LE(fixed_register_count(),
              register_allocator()->maximum_register_count());
    return register_allocator()->maximum_register_count();
  }

  Register Parameter(int parameter_index) const;

  // Constant loads to accumulator.
  BytecodeArrayBuilder& LoadConstantPoolEntry(size_t entry);
  BytecodeArrayBuilder& LoadLiteral(v8::internal::Smi* value);
  BytecodeArrayBuilder& LoadLiteral(Handle<Object> object);
  BytecodeArrayBuilder& LoadUndefined();
  BytecodeArrayBuilder& LoadNull();
  BytecodeArrayBuilder& LoadTheHole();
  BytecodeArrayBuilder& LoadTrue();
  BytecodeArrayBuilder& LoadFalse();

  // Global loads to the accumulator and stores from the accumulator.
  BytecodeArrayBuilder& LoadGlobal(const Handle<String> name, int feedback_slot,
                                   TypeofMode typeof_mode);
  BytecodeArrayBuilder& StoreGlobal(const Handle<String> name,
                                    int feedback_slot,
                                    LanguageMode language_mode);

  // Load the object at |slot_index| at |depth| in the context chain starting
  // with |context| into the accumulator.
  BytecodeArrayBuilder& LoadContextSlot(Register context, int slot_index,
                                        int depth);

  // Stores the object in the accumulator into |slot_index| at |depth| in the
  // context chain starting with |context|.
  BytecodeArrayBuilder& StoreContextSlot(Register context, int slot_index,
                                         int depth);

  // Load from a module variable into the accumulator. |depth| is the depth of
  // the current context relative to the module context.
  BytecodeArrayBuilder& LoadModuleVariable(int cell_index, int depth);

  // Store from the accumulator into a module variable. |depth| is the depth of
  // the current context relative to the module context.
  BytecodeArrayBuilder& StoreModuleVariable(int cell_index, int depth);

  // Register-accumulator transfers.
  BytecodeArrayBuilder& LoadAccumulatorWithRegister(Register reg);
  BytecodeArrayBuilder& StoreAccumulatorInRegister(Register reg);

  // Register-register transfer.
  BytecodeArrayBuilder& MoveRegister(Register from, Register to);

  // Named load property.
  BytecodeArrayBuilder& LoadNamedProperty(Register object,
                                          const Handle<Name> name,
                                          int feedback_slot);
  // Keyed load property. The key should be in the accumulator.
  BytecodeArrayBuilder& LoadKeyedProperty(Register object, int feedback_slot);

  // Store properties. Flag for NeedsSetFunctionName() should
  // be in the accumulator.
  BytecodeArrayBuilder& StoreDataPropertyInLiteral(
      Register object, Register name, DataPropertyInLiteralFlags flags,
      int feedback_slot);

  // Store properties. The value to be stored should be in the accumulator.
  BytecodeArrayBuilder& StoreNamedProperty(Register object,
                                           const Handle<Name> name,
                                           int feedback_slot,
                                           LanguageMode language_mode);
  BytecodeArrayBuilder& StoreKeyedProperty(Register object, Register key,
                                           int feedback_slot,
                                           LanguageMode language_mode);

  // Lookup the variable with |name|.
  BytecodeArrayBuilder& LoadLookupSlot(const Handle<String> name,
                                       TypeofMode typeof_mode);

  // Lookup the variable with |name|, which is known to be at |slot_index| at
  // |depth| in the context chain if not shadowed by a context extension
  // somewhere in that context chain.
  BytecodeArrayBuilder& LoadLookupContextSlot(const Handle<String> name,
                                              TypeofMode typeof_mode,
                                              int slot_index, int depth);

  // Lookup the variable with |name|, which has its feedback in |feedback_slot|
  // and is known to be global if not shadowed by a context extension somewhere
  // up to |depth| in that context chain.
  BytecodeArrayBuilder& LoadLookupGlobalSlot(const Handle<String> name,
                                             TypeofMode typeof_mode,
                                             int feedback_slot, int depth);

  // Store value in the accumulator into the variable with |name|.
  BytecodeArrayBuilder& StoreLookupSlot(const Handle<String> name,
                                        LanguageMode language_mode);

  // Create a new closure for a SharedFunctionInfo which will be inserted at
  // constant pool index |shared_function_info_entry|.
  BytecodeArrayBuilder& CreateClosure(size_t shared_function_info_entry,
                                      int slot, int flags);

  // Create a new local context for a |scope_info| and a closure which should be
  // in the accumulator.
  BytecodeArrayBuilder& CreateBlockContext(Handle<ScopeInfo> scope_info);

  // Create a new context for a catch block with |exception|, |name|,
  // |scope_info|, and the closure in the accumulator.
  BytecodeArrayBuilder& CreateCatchContext(Register exception,
                                           Handle<String> name,
                                           Handle<ScopeInfo> scope_info);

  // Create a new context with size |slots|.
  BytecodeArrayBuilder& CreateFunctionContext(int slots);

  // Create a new eval context with size |slots|.
  BytecodeArrayBuilder& CreateEvalContext(int slots);

  // Creates a new context with the given |scope_info| for a with-statement
  // with the |object| in a register and the closure in the accumulator.
  BytecodeArrayBuilder& CreateWithContext(Register object,
                                          Handle<ScopeInfo> scope_info);

  // Create a new arguments object in the accumulator.
  BytecodeArrayBuilder& CreateArguments(CreateArgumentsType type);

  // Literals creation.  Constant elements should be in the accumulator.
  BytecodeArrayBuilder& CreateRegExpLiteral(Handle<String> pattern,
                                            int literal_index, int flags);
  BytecodeArrayBuilder& CreateArrayLiteral(size_t constant_elements_entry,
                                           int literal_index, int flags);
  BytecodeArrayBuilder& CreateObjectLiteral(size_t constant_properties_entry,
                                            int literal_index, int flags,
                                            Register output);

  // Push the context in accumulator as the new context, and store in register
  // |context|.
  BytecodeArrayBuilder& PushContext(Register context);

  // Pop the current context and replace with |context|.
  BytecodeArrayBuilder& PopContext(Register context);

  // Call a JS function. The JSFunction or Callable to be called should be in
  // |callable|. The arguments should be in |args|, with the receiver in
  // |args[0]|. The call type of the expression is in |call_type|. Type feedback
  // is recorded in the |feedback_slot| in the type feedback vector.
  BytecodeArrayBuilder& Call(
      Register callable, RegisterList args, int feedback_slot,
      Call::CallType call_type,
      TailCallMode tail_call_mode = TailCallMode::kDisallow);

  // Call the new operator. The accumulator holds the |new_target|.
  // The |constructor| is in a register and arguments are in |args|.
  BytecodeArrayBuilder& New(Register constructor, RegisterList args,
                            int feedback_slot);

  // Call the runtime function with |function_id| and arguments |args|.
  BytecodeArrayBuilder& CallRuntime(Runtime::FunctionId function_id,
                                    RegisterList args);
  // Call the runtime function with |function_id| with single argument |arg|.
  BytecodeArrayBuilder& CallRuntime(Runtime::FunctionId function_id,
                                    Register arg);
  // Call the runtime function with |function_id| with no arguments.
  BytecodeArrayBuilder& CallRuntime(Runtime::FunctionId function_id);

  // Call the runtime function with |function_id| and arguments |args|, that
  // returns a pair of values. The return values will be returned in
  // |return_pair|.
  BytecodeArrayBuilder& CallRuntimeForPair(Runtime::FunctionId function_id,
                                           RegisterList args,
                                           RegisterList return_pair);
  // Call the runtime function with |function_id| with single argument |arg|
  // that returns a pair of values. The return values will be returned in
  // |return_pair|.
  BytecodeArrayBuilder& CallRuntimeForPair(Runtime::FunctionId function_id,
                                           Register arg,
                                           RegisterList return_pair);

  // Call the JS runtime function with |context_index| and arguments |args|.
  BytecodeArrayBuilder& CallJSRuntime(int context_index, RegisterList args);

  // Call the constructor in |args[0]| with new_target in |args[1]| and the
  // arguments starting at |args[2]| onwards. The final argument must be a
  // spread.
  BytecodeArrayBuilder& NewWithSpread(RegisterList args);

  // Operators (register holds the lhs value, accumulator holds the rhs value).
  // Type feedback will be recorded in the |feedback_slot|
  BytecodeArrayBuilder& BinaryOperation(Token::Value binop, Register reg,
                                        int feedback_slot);

  // Count Operators (value stored in accumulator).
  // Type feedback will be recorded in the |feedback_slot|
  BytecodeArrayBuilder& CountOperation(Token::Value op, int feedback_slot);

  // Unary Operators.
  BytecodeArrayBuilder& LogicalNot();
  BytecodeArrayBuilder& TypeOf();

  // Expects a heap object in the accumulator. Returns its super constructor in
  // the register |out| if it passes the IsConstructor test. Otherwise, it
  // throws a TypeError exception.
  BytecodeArrayBuilder& GetSuperConstructor(Register out);

  // Deletes property from an object. This expects that accumulator contains
  // the key to be deleted and the register contains a reference to the object.
  BytecodeArrayBuilder& Delete(Register object, LanguageMode language_mode);

  // Tests.
  BytecodeArrayBuilder& CompareOperation(Token::Value op, Register reg,
                                         int feedback_slot = kNoFeedbackSlot);

  // Converts accumulator and stores result in register |out|.
  BytecodeArrayBuilder& ConvertAccumulatorToObject(Register out);
  BytecodeArrayBuilder& ConvertAccumulatorToName(Register out);
  BytecodeArrayBuilder& ConvertAccumulatorToNumber(Register out);

  // Flow Control.
  BytecodeArrayBuilder& Bind(BytecodeLabel* label);
  BytecodeArrayBuilder& Bind(const BytecodeLabel& target, BytecodeLabel* label);

  BytecodeArrayBuilder& Jump(BytecodeLabel* label);
  BytecodeArrayBuilder& JumpIfTrue(BytecodeLabel* label);
  BytecodeArrayBuilder& JumpIfFalse(BytecodeLabel* label);
  BytecodeArrayBuilder& JumpIfNotHole(BytecodeLabel* label);
  BytecodeArrayBuilder& JumpIfJSReceiver(BytecodeLabel* label);
  BytecodeArrayBuilder& JumpIfNull(BytecodeLabel* label);
  BytecodeArrayBuilder& JumpIfUndefined(BytecodeLabel* label);
  BytecodeArrayBuilder& JumpLoop(BytecodeLabel* label, int loop_depth);

  BytecodeArrayBuilder& StackCheck(int position);

  // Sets the pending message to the value in the accumulator, and returns the
  // previous pending message in the accumulator.
  BytecodeArrayBuilder& SetPendingMessage();

  BytecodeArrayBuilder& Throw();
  BytecodeArrayBuilder& ReThrow();
  BytecodeArrayBuilder& Return();

  // Debugger.
  BytecodeArrayBuilder& Debugger();

  // Complex flow control.
  BytecodeArrayBuilder& ForInPrepare(Register receiver,
                                     RegisterList cache_info_triple);
  BytecodeArrayBuilder& ForInContinue(Register index, Register cache_length);
  BytecodeArrayBuilder& ForInNext(Register receiver, Register index,
                                  RegisterList cache_type_array_pair,
                                  int feedback_slot);
  BytecodeArrayBuilder& ForInStep(Register index);

  // Generators.
  BytecodeArrayBuilder& SuspendGenerator(Register generator);
  BytecodeArrayBuilder& ResumeGenerator(Register generator);

  // Exception handling.
  BytecodeArrayBuilder& MarkHandler(int handler_id,
                                    HandlerTable::CatchPrediction will_catch);
  BytecodeArrayBuilder& MarkTryBegin(int handler_id, Register context);
  BytecodeArrayBuilder& MarkTryEnd(int handler_id);

  // Creates a new handler table entry and returns a {hander_id} identifying the
  // entry, so that it can be referenced by above exception handling support.
  int NewHandlerEntry() { return handler_table_builder()->NewHandlerEntry(); }

  // Gets a constant pool entry for the |object|.
  size_t GetConstantPoolEntry(Handle<Object> object);
  // Allocates a slot in the constant pool which can later be inserted.
  size_t AllocateConstantPoolEntry();
  // Inserts a entry into an allocated constant pool entry.
  void InsertConstantPoolEntryAt(size_t entry, Handle<Object> object);

  void InitializeReturnPosition(FunctionLiteral* literal);

  void SetStatementPosition(Statement* stmt) {
    if (stmt->position() == kNoSourcePosition) return;
    latest_source_info_.MakeStatementPosition(stmt->position());
  }

  void SetExpressionPosition(Expression* expr) {
    if (expr->position() == kNoSourcePosition) return;
    if (!latest_source_info_.is_statement()) {
      // Ensure the current expression position is overwritten with the
      // latest value.
      latest_source_info_.MakeExpressionPosition(expr->position());
    }
  }

  void SetExpressionAsStatementPosition(Expression* expr) {
    if (expr->position() == kNoSourcePosition) return;
    latest_source_info_.MakeStatementPosition(expr->position());
  }

  bool RequiresImplicitReturn() const { return !return_seen_in_block_; }

  // Returns the raw operand value for the given register or register list.
  uint32_t GetInputRegisterOperand(Register reg);
  uint32_t GetOutputRegisterOperand(Register reg);
  uint32_t GetInputRegisterListOperand(RegisterList reg_list);
  uint32_t GetOutputRegisterListOperand(RegisterList reg_list);

  // Accessors
  BytecodeRegisterAllocator* register_allocator() {
    return &register_allocator_;
  }
  const BytecodeRegisterAllocator* register_allocator() const {
    return &register_allocator_;
  }
  Zone* zone() const { return zone_; }

 private:
  friend class BytecodeRegisterAllocator;
  template <Bytecode bytecode, AccumulatorUse accumulator_use,
            OperandType... operand_types>
  friend class BytecodeNodeBuilder;

  // Returns the current source position for the given |bytecode|.
  INLINE(BytecodeSourceInfo CurrentSourcePosition(Bytecode bytecode));

#define DECLARE_BYTECODE_OUTPUT(Name, ...)         \
  template <typename... Operands>                  \
  INLINE(void Output##Name(Operands... operands)); \
  template <typename... Operands>                  \
  INLINE(void Output##Name(BytecodeLabel* label, Operands... operands));
  BYTECODE_LIST(DECLARE_BYTECODE_OUTPUT)
#undef DECLARE_OPERAND_TYPE_INFO

  bool RegisterIsValid(Register reg) const;
  bool RegisterListIsValid(RegisterList reg_list) const;

  // Set position for return.
  void SetReturnPosition();

  // Not implemented as the illegal bytecode is used inside internally
  // to indicate a bytecode field is not valid or an error has occured
  // during bytecode generation.
  BytecodeArrayBuilder& Illegal();

  template <Bytecode bytecode, AccumulatorUse accumulator_use>
  void PrepareToOutputBytecode();

  void LeaveBasicBlock() { return_seen_in_block_ = false; }

  BytecodeArrayWriter* bytecode_array_writer() {
    return &bytecode_array_writer_;
  }
  BytecodePipelineStage* pipeline() { return pipeline_; }
  ConstantArrayBuilder* constant_array_builder() {
    return &constant_array_builder_;
  }
  const ConstantArrayBuilder* constant_array_builder() const {
    return &constant_array_builder_;
  }
  HandlerTableBuilder* handler_table_builder() {
    return &handler_table_builder_;
  }

  Zone* zone_;
  bool bytecode_generated_;
  ConstantArrayBuilder constant_array_builder_;
  HandlerTableBuilder handler_table_builder_;
  bool return_seen_in_block_;
  int parameter_count_;
  int local_register_count_;
  int context_register_count_;
  int return_position_;
  BytecodeRegisterAllocator register_allocator_;
  BytecodeArrayWriter bytecode_array_writer_;
  BytecodePipelineStage* pipeline_;
  BytecodeRegisterOptimizer* register_optimizer_;
  BytecodeSourceInfo latest_source_info_;

  static int const kNoFeedbackSlot = 0;

  DISALLOW_COPY_AND_ASSIGN(BytecodeArrayBuilder);
};

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

#endif  // V8_INTERPRETER_BYTECODE_ARRAY_BUILDER_H_
