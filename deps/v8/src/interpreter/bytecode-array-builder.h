// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTERPRETER_BYTECODE_ARRAY_BUILDER_H_
#define V8_INTERPRETER_BYTECODE_ARRAY_BUILDER_H_

#include "src/ast/ast.h"
#include "src/interpreter/bytecodes.h"
#include "src/interpreter/constant-array-builder.h"
#include "src/zone-containers.h"

namespace v8 {
namespace internal {

class Isolate;

namespace interpreter {

class BytecodeLabel;
class ConstantArrayBuilder;
class Register;

// TODO(rmcilroy): Unify this with CreateArgumentsParameters::Type in Turbofan
// when rest parameters implementation has settled down.
enum class CreateArgumentsType { kMappedArguments, kUnmappedArguments };

class BytecodeArrayBuilder final {
 public:
  BytecodeArrayBuilder(Isolate* isolate, Zone* zone);
  ~BytecodeArrayBuilder();

  Handle<BytecodeArray> ToBytecodeArray();

  // Set the number of parameters expected by function.
  void set_parameter_count(int number_of_params);
  int parameter_count() const {
    DCHECK_GE(parameter_count_, 0);
    return parameter_count_;
  }

  // Set the number of locals required for bytecode array.
  void set_locals_count(int number_of_locals);
  int locals_count() const {
    DCHECK_GE(local_register_count_, 0);
    return local_register_count_;
  }

  // Set number of contexts required for bytecode array.
  void set_context_count(int number_of_contexts);
  int context_count() const {
    DCHECK_GE(context_register_count_, 0);
    return context_register_count_;
  }

  Register first_context_register() const;
  Register last_context_register() const;

  // Returns the number of fixed (non-temporary) registers.
  int fixed_register_count() const { return context_count() + locals_count(); }

  Register Parameter(int parameter_index) const;

  // Return true if the register |reg| represents a parameter or a
  // local.
  bool RegisterIsParameterOrLocal(Register reg) const;

  // Return true if the register |reg| represents a temporary register.
  bool RegisterIsTemporary(Register reg) const;

  // Constant loads to accumulator.
  BytecodeArrayBuilder& LoadLiteral(v8::internal::Smi* value);
  BytecodeArrayBuilder& LoadLiteral(Handle<Object> object);
  BytecodeArrayBuilder& LoadUndefined();
  BytecodeArrayBuilder& LoadNull();
  BytecodeArrayBuilder& LoadTheHole();
  BytecodeArrayBuilder& LoadTrue();
  BytecodeArrayBuilder& LoadFalse();
  BytecodeArrayBuilder& LoadBooleanConstant(bool value);

  // Global loads to the accumulator and stores from the accumulator.
  BytecodeArrayBuilder& LoadGlobal(const Handle<String> name, int feedback_slot,
                                   LanguageMode language_mode,
                                   TypeofMode typeof_mode);
  BytecodeArrayBuilder& StoreGlobal(const Handle<String> name,
                                    int feedback_slot,
                                    LanguageMode language_mode);

  // Load the object at |slot_index| in |context| into the accumulator.
  BytecodeArrayBuilder& LoadContextSlot(Register context, int slot_index);

  // Stores the object in the accumulator into |slot_index| of |context|.
  BytecodeArrayBuilder& StoreContextSlot(Register context, int slot_index);

  // Register-accumulator transfers.
  BytecodeArrayBuilder& LoadAccumulatorWithRegister(Register reg);
  BytecodeArrayBuilder& StoreAccumulatorInRegister(Register reg);

  // Register-register transfer.
  BytecodeArrayBuilder& MoveRegister(Register from, Register to);
  BytecodeArrayBuilder& ExchangeRegisters(Register reg0, Register reg1);

  // Named load property.
  BytecodeArrayBuilder& LoadNamedProperty(Register object,
                                          const Handle<String> name,
                                          int feedback_slot,
                                          LanguageMode language_mode);
  // Keyed load property. The key should be in the accumulator.
  BytecodeArrayBuilder& LoadKeyedProperty(Register object, int feedback_slot,
                                          LanguageMode language_mode);

  // Store properties. The value to be stored should be in the accumulator.
  BytecodeArrayBuilder& StoreNamedProperty(Register object,
                                           const Handle<String> name,
                                           int feedback_slot,
                                           LanguageMode language_mode);
  BytecodeArrayBuilder& StoreKeyedProperty(Register object, Register key,
                                           int feedback_slot,
                                           LanguageMode language_mode);

  // Lookup the variable with |name|.
  BytecodeArrayBuilder& LoadLookupSlot(const Handle<String> name,
                                       TypeofMode typeof_mode);

  // Store value in the accumulator into the variable with |name|.
  BytecodeArrayBuilder& StoreLookupSlot(const Handle<String> name,
                                        LanguageMode language_mode);

  // Create a new closure for the SharedFunctionInfo.
  BytecodeArrayBuilder& CreateClosure(Handle<SharedFunctionInfo> shared_info,
                                      PretenureFlag tenured);

  // Create a new arguments object in the accumulator.
  BytecodeArrayBuilder& CreateArguments(CreateArgumentsType type);

  // Literals creation.  Constant elements should be in the accumulator.
  BytecodeArrayBuilder& CreateRegExpLiteral(Handle<String> pattern,
                                            int literal_index, int flags);
  BytecodeArrayBuilder& CreateArrayLiteral(Handle<FixedArray> constant_elements,
                                           int literal_index, int flags);
  BytecodeArrayBuilder& CreateObjectLiteral(
      Handle<FixedArray> constant_properties, int literal_index, int flags);

  // Push the context in accumulator as the new context, and store in register
  // |context|.
  BytecodeArrayBuilder& PushContext(Register context);

  // Pop the current context and replace with |context|.
  BytecodeArrayBuilder& PopContext(Register context);

  // Call a JS function. The JSFunction or Callable to be called should be in
  // |callable|, the receiver should be in |receiver| and all subsequent
  // arguments should be in registers <receiver + 1> to
  // <receiver + 1 + arg_count>.
  BytecodeArrayBuilder& Call(Register callable, Register receiver,
                             size_t arg_count, int feedback_slot);

  // Call the new operator. The |constructor| register is followed by
  // |arg_count| consecutive registers containing arguments to be
  // applied to the constructor.
  BytecodeArrayBuilder& New(Register constructor, Register first_arg,
                            size_t arg_count);

  // Call the runtime function with |function_id|. The first argument should be
  // in |first_arg| and all subsequent arguments should be in registers
  // <first_arg + 1> to <first_arg + 1 + arg_count>.
  BytecodeArrayBuilder& CallRuntime(Runtime::FunctionId function_id,
                                    Register first_arg, size_t arg_count);

  // Call the runtime function with |function_id| that returns a pair of values.
  // The first argument should be in |first_arg| and all subsequent arguments
  // should be in registers <first_arg + 1> to <first_arg + 1 + arg_count>. The
  // return values will be returned in <first_return> and <first_return + 1>.
  BytecodeArrayBuilder& CallRuntimeForPair(Runtime::FunctionId function_id,
                                           Register first_arg, size_t arg_count,
                                           Register first_return);

  // Call the JS runtime function with |context_index|. The the receiver should
  // be in |receiver| and all subsequent arguments should be in registers
  // <receiver + 1> to <receiver + 1 + arg_count>.
  BytecodeArrayBuilder& CallJSRuntime(int context_index, Register receiver,
                                      size_t arg_count);

  // Operators (register holds the lhs value, accumulator holds the rhs value).
  BytecodeArrayBuilder& BinaryOperation(Token::Value binop, Register reg,
                                        Strength strength);

  // Count Operators (value stored in accumulator).
  BytecodeArrayBuilder& CountOperation(Token::Value op, Strength strength);

  // Unary Operators.
  BytecodeArrayBuilder& LogicalNot();
  BytecodeArrayBuilder& TypeOf();

  // Deletes property from an object. This expects that accumulator contains
  // the key to be deleted and the register contains a reference to the object.
  BytecodeArrayBuilder& Delete(Register object, LanguageMode language_mode);
  BytecodeArrayBuilder& DeleteLookupSlot();

  // Tests.
  BytecodeArrayBuilder& CompareOperation(Token::Value op, Register reg,
                                         Strength strength);

  // Casts.
  BytecodeArrayBuilder& CastAccumulatorToBoolean();
  BytecodeArrayBuilder& CastAccumulatorToJSObject();
  BytecodeArrayBuilder& CastAccumulatorToName();
  BytecodeArrayBuilder& CastAccumulatorToNumber();

  // Flow Control.
  BytecodeArrayBuilder& Bind(BytecodeLabel* label);
  BytecodeArrayBuilder& Bind(const BytecodeLabel& target, BytecodeLabel* label);

  BytecodeArrayBuilder& Jump(BytecodeLabel* label);
  BytecodeArrayBuilder& JumpIfTrue(BytecodeLabel* label);
  BytecodeArrayBuilder& JumpIfFalse(BytecodeLabel* label);
  BytecodeArrayBuilder& JumpIfNull(BytecodeLabel* label);
  BytecodeArrayBuilder& JumpIfUndefined(BytecodeLabel* label);

  BytecodeArrayBuilder& Throw();
  BytecodeArrayBuilder& Return();

  // Complex flow control.
  BytecodeArrayBuilder& ForInPrepare(Register cache_type, Register cache_array,
                                     Register cache_length);
  BytecodeArrayBuilder& ForInDone(Register index, Register cache_length);
  BytecodeArrayBuilder& ForInNext(Register receiver, Register cache_type,
                                  Register cache_array, Register index);
  BytecodeArrayBuilder& ForInStep(Register index);

  // Accessors
  Zone* zone() const { return zone_; }

 private:
  ZoneVector<uint8_t>* bytecodes() { return &bytecodes_; }
  const ZoneVector<uint8_t>* bytecodes() const { return &bytecodes_; }
  Isolate* isolate() const { return isolate_; }
  ConstantArrayBuilder* constant_array_builder() {
    return &constant_array_builder_;
  }
  const ConstantArrayBuilder* constant_array_builder() const {
    return &constant_array_builder_;
  }

  static Bytecode BytecodeForBinaryOperation(Token::Value op);
  static Bytecode BytecodeForCountOperation(Token::Value op);
  static Bytecode BytecodeForCompareOperation(Token::Value op);
  static Bytecode BytecodeForWideOperands(Bytecode bytecode);
  static Bytecode BytecodeForLoadIC(LanguageMode language_mode);
  static Bytecode BytecodeForKeyedLoadIC(LanguageMode language_mode);
  static Bytecode BytecodeForStoreIC(LanguageMode language_mode);
  static Bytecode BytecodeForKeyedStoreIC(LanguageMode language_mode);
  static Bytecode BytecodeForLoadGlobal(LanguageMode language_mode,
                                        TypeofMode typeof_mode);
  static Bytecode BytecodeForStoreGlobal(LanguageMode language_mode);
  static Bytecode BytecodeForStoreLookupSlot(LanguageMode language_mode);
  static Bytecode BytecodeForCreateArguments(CreateArgumentsType type);
  static Bytecode BytecodeForDelete(LanguageMode language_mode);

  static bool FitsInIdx8Operand(int value);
  static bool FitsInIdx8Operand(size_t value);
  static bool FitsInImm8Operand(int value);
  static bool FitsInIdx16Operand(int value);
  static bool FitsInIdx16Operand(size_t value);
  static bool FitsInReg8Operand(Register value);
  static bool FitsInReg16Operand(Register value);

  static Bytecode GetJumpWithConstantOperand(Bytecode jump_smi8_operand);
  static Bytecode GetJumpWithConstantWideOperand(Bytecode jump_smi8_operand);
  static Bytecode GetJumpWithToBoolean(Bytecode jump_smi8_operand);

  Register MapRegister(Register reg);
  Register MapRegisters(Register reg, Register args_base, int args_length = 1);

  template <size_t N>
  INLINE(void Output(Bytecode bytecode, uint32_t(&operands)[N]));
  void Output(Bytecode bytecode, uint32_t operand0, uint32_t operand1,
              uint32_t operand2, uint32_t operand3);
  void Output(Bytecode bytecode, uint32_t operand0, uint32_t operand1,
              uint32_t operand2);
  void Output(Bytecode bytecode, uint32_t operand0, uint32_t operand1);
  void Output(Bytecode bytecode, uint32_t operand0);
  void Output(Bytecode bytecode);

  BytecodeArrayBuilder& OutputJump(Bytecode jump_bytecode,
                                   BytecodeLabel* label);
  void PatchJump(const ZoneVector<uint8_t>::iterator& jump_target,
                 const ZoneVector<uint8_t>::iterator& jump_location);
  void PatchIndirectJumpWith8BitOperand(
      const ZoneVector<uint8_t>::iterator& jump_location, int delta);
  void PatchIndirectJumpWith16BitOperand(
      const ZoneVector<uint8_t>::iterator& jump_location, int delta);

  void LeaveBasicBlock();
  void EnsureReturn();

  bool OperandIsValid(Bytecode bytecode, int operand_index,
                      uint32_t operand_value) const;
  bool LastBytecodeInSameBlock() const;

  bool NeedToBooleanCast();
  bool IsRegisterInAccumulator(Register reg);

  bool RegisterIsValid(Register reg) const;

  // Temporary register management.
  int BorrowTemporaryRegister();
  int BorrowTemporaryRegisterNotInRange(int start_index, int end_index);
  void ReturnTemporaryRegister(int reg_index);
  int PrepareForConsecutiveTemporaryRegisters(size_t count);
  void BorrowConsecutiveTemporaryRegister(int reg_index);
  bool TemporaryRegisterIsLive(Register reg) const;

  Register first_temporary_register() const;
  Register last_temporary_register() const;

  // Gets a constant pool entry for the |object|.
  size_t GetConstantPoolEntry(Handle<Object> object);

  Isolate* isolate_;
  Zone* zone_;
  ZoneVector<uint8_t> bytecodes_;
  bool bytecode_generated_;
  ConstantArrayBuilder constant_array_builder_;
  size_t last_block_end_;
  size_t last_bytecode_start_;
  bool exit_seen_in_block_;
  int unbound_jumps_;

  int parameter_count_;
  int local_register_count_;
  int context_register_count_;
  int temporary_register_count_;
  ZoneSet<int> free_temporaries_;

  class PreviousBytecodeHelper;
  friend class BytecodeRegisterAllocator;

  DISALLOW_COPY_AND_ASSIGN(BytecodeArrayBuilder);
};


// A label representing a branch target in a bytecode array. When a
// label is bound, it represents a known position in the bytecode
// array. For labels that are forward references there can be at most
// one reference whilst it is unbound.
class BytecodeLabel final {
 public:
  BytecodeLabel() : bound_(false), offset_(kInvalidOffset) {}

  bool is_bound() const { return bound_; }
  size_t offset() const { return offset_; }

 private:
  static const size_t kInvalidOffset = static_cast<size_t>(-1);

  void bind_to(size_t offset) {
    DCHECK(!bound_ && offset != kInvalidOffset);
    offset_ = offset;
    bound_ = true;
  }

  void set_referrer(size_t offset) {
    DCHECK(!bound_ && offset != kInvalidOffset && offset_ == kInvalidOffset);
    offset_ = offset;
  }

  bool is_forward_target() const {
    return offset() != kInvalidOffset && !is_bound();
  }

  // There are three states for a label:
  //                    bound_   offset_
  //  UNSET             false    kInvalidOffset
  //  FORWARD_TARGET    false    Offset of referring jump
  //  BACKWARD_TARGET    true    Offset of label in bytecode array when bound
  bool bound_;
  size_t offset_;

  friend class BytecodeArrayBuilder;
};

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

#endif  // V8_INTERPRETER_BYTECODE_ARRAY_BUILDER_H_
