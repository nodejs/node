// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTERPRETER_BYTECODE_ARRAY_BUILDER_H_
#define V8_INTERPRETER_BYTECODE_ARRAY_BUILDER_H_

#include <vector>

#include "src/ast.h"
#include "src/identity-map.h"
#include "src/interpreter/bytecodes.h"
#include "src/zone.h"
#include "src/zone-containers.h"

namespace v8 {
namespace internal {

class Isolate;

namespace interpreter {

class BytecodeLabel;
class Register;

// TODO(rmcilroy): Unify this with CreateArgumentsParameters::Type in Turbofan
// when rest parameters implementation has settled down.
enum class CreateArgumentsType { kMappedArguments, kUnmappedArguments };

class BytecodeArrayBuilder {
 public:
  BytecodeArrayBuilder(Isolate* isolate, Zone* zone);
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

  // Gets a constant pool entry for the |object|.
  size_t GetConstantPoolEntry(Handle<Object> object);

  // Constant loads to accumulator.
  BytecodeArrayBuilder& LoadLiteral(v8::internal::Smi* value);
  BytecodeArrayBuilder& LoadLiteral(Handle<Object> object);
  BytecodeArrayBuilder& LoadUndefined();
  BytecodeArrayBuilder& LoadNull();
  BytecodeArrayBuilder& LoadTheHole();
  BytecodeArrayBuilder& LoadTrue();
  BytecodeArrayBuilder& LoadFalse();

  // Global loads to the accumulator and stores from the accumulator.
  BytecodeArrayBuilder& LoadGlobal(size_t name_index, int feedback_slot,
                                   LanguageMode language_mode,
                                   TypeofMode typeof_mode);
  BytecodeArrayBuilder& StoreGlobal(size_t name_index, int feedback_slot,
                                    LanguageMode language_mode);

  // Load the object at |slot_index| in |context| into the accumulator.
  BytecodeArrayBuilder& LoadContextSlot(Register context, int slot_index);

  // Stores the object in the accumulator into |slot_index| of |context|.
  BytecodeArrayBuilder& StoreContextSlot(Register context, int slot_index);

  // Register-accumulator transfers.
  BytecodeArrayBuilder& LoadAccumulatorWithRegister(Register reg);
  BytecodeArrayBuilder& StoreAccumulatorInRegister(Register reg);

  // Named load property.
  BytecodeArrayBuilder& LoadNamedProperty(Register object, size_t name_index,
                                          int feedback_slot,
                                          LanguageMode language_mode);
  // Keyed load property. The key should be in the accumulator.
  BytecodeArrayBuilder& LoadKeyedProperty(Register object, int feedback_slot,
                                          LanguageMode language_mode);

  // Store properties. The value to be stored should be in the accumulator.
  BytecodeArrayBuilder& StoreNamedProperty(Register object, size_t name_index,
                                           int feedback_slot,
                                           LanguageMode language_mode);
  BytecodeArrayBuilder& StoreKeyedProperty(Register object, Register key,
                                           int feedback_slot,
                                           LanguageMode language_mode);

  // Create a new closure for the SharedFunctionInfo in the accumulator.
  BytecodeArrayBuilder& CreateClosure(PretenureFlag tenured);

  // Create a new arguments object in the accumulator.
  BytecodeArrayBuilder& CreateArguments(CreateArgumentsType type);

  // Literals creation.  Constant elements should be in the accumulator.
  BytecodeArrayBuilder& CreateRegExpLiteral(int literal_index, Register flags);
  BytecodeArrayBuilder& CreateArrayLiteral(int literal_index, int flags);
  BytecodeArrayBuilder& CreateObjectLiteral(int literal_index, int flags);

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
                             size_t arg_count);

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
  BytecodeArrayBuilder& ForInPrepare(Register receiver);
  BytecodeArrayBuilder& ForInNext(Register for_in_state, Register index);
  BytecodeArrayBuilder& ForInDone(Register for_in_state);

  // Accessors
  Zone* zone() const { return zone_; }

 private:
  ZoneVector<uint8_t>* bytecodes() { return &bytecodes_; }
  const ZoneVector<uint8_t>* bytecodes() const { return &bytecodes_; }
  Isolate* isolate() const { return isolate_; }

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
  static Bytecode BytecodeForCreateArguments(CreateArgumentsType type);
  static Bytecode BytecodeForDelete(LanguageMode language_mode);

  static bool FitsInIdx8Operand(int value);
  static bool FitsInIdx8Operand(size_t value);
  static bool FitsInImm8Operand(int value);
  static bool FitsInIdx16Operand(int value);
  static bool FitsInIdx16Operand(size_t value);

  static Bytecode GetJumpWithConstantOperand(Bytecode jump_with_smi8_operand);
  static Bytecode GetJumpWithToBoolean(Bytecode jump);

  template <size_t N>
  INLINE(void Output(Bytecode bytecode, uint32_t(&oprands)[N]));
  void Output(Bytecode bytecode, uint32_t operand0, uint32_t operand1,
              uint32_t operand2);
  void Output(Bytecode bytecode, uint32_t operand0, uint32_t operand1);
  void Output(Bytecode bytecode, uint32_t operand0);
  void Output(Bytecode bytecode);

  BytecodeArrayBuilder& OutputJump(Bytecode jump_bytecode,
                                   BytecodeLabel* label);
  void PatchJump(const ZoneVector<uint8_t>::iterator& jump_target,
                 ZoneVector<uint8_t>::iterator jump_location);

  void LeaveBasicBlock();
  void EnsureReturn();

  bool OperandIsValid(Bytecode bytecode, int operand_index,
                      uint32_t operand_value) const;
  bool LastBytecodeInSameBlock() const;

  bool NeedToBooleanCast();

  int BorrowTemporaryRegister();
  void ReturnTemporaryRegister(int reg_index);
  int PrepareForConsecutiveTemporaryRegisters(size_t count);
  void BorrowConsecutiveTemporaryRegister(int reg_index);
  bool TemporaryRegisterIsLive(Register reg) const;

  Register first_temporary_register() const;
  Register last_temporary_register() const;

  Isolate* isolate_;
  Zone* zone_;
  ZoneVector<uint8_t> bytecodes_;
  bool bytecode_generated_;
  size_t last_block_end_;
  size_t last_bytecode_start_;
  bool exit_seen_in_block_;

  IdentityMap<size_t> constants_map_;
  ZoneVector<Handle<Object>> constants_;

  int parameter_count_;
  int local_register_count_;
  int context_register_count_;
  int temporary_register_count_;

  ZoneSet<int> free_temporaries_;

  friend class TemporaryRegisterScope;
  DISALLOW_COPY_AND_ASSIGN(BytecodeArrayBuilder);
};


// A label representing a branch target in a bytecode array. When a
// label is bound, it represents a known position in the bytecode
// array. For labels that are forward references there can be at most
// one reference whilst it is unbound.
class BytecodeLabel final {
 public:
  BytecodeLabel() : bound_(false), offset_(kInvalidOffset) {}

  INLINE(bool is_bound() const) { return bound_; }

 private:
  static const size_t kInvalidOffset = static_cast<size_t>(-1);

  INLINE(void bind_to(size_t offset)) {
    DCHECK(!bound_ && offset != kInvalidOffset);
    offset_ = offset;
    bound_ = true;
  }
  INLINE(void set_referrer(size_t offset)) {
    DCHECK(!bound_ && offset != kInvalidOffset && offset_ == kInvalidOffset);
    offset_ = offset;
  }
  INLINE(size_t offset() const) { return offset_; }
  INLINE(bool is_forward_target() const) {
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


// A stack-allocated class than allows the instantiator to allocate
// temporary registers that are cleaned up when scope is closed.
// TODO(oth): Deprecate TemporaryRegisterScope use. Code should be
// using result scopes as far as possible.
class TemporaryRegisterScope {
 public:
  explicit TemporaryRegisterScope(BytecodeArrayBuilder* builder);
  ~TemporaryRegisterScope();
  Register NewRegister();

  void PrepareForConsecutiveAllocations(size_t count);
  Register NextConsecutiveRegister();

  bool RegisterIsAllocatedInThisScope(Register reg) const;

 private:
  void* operator new(size_t size);
  void operator delete(void* p);

  BytecodeArrayBuilder* builder_;
  const TemporaryRegisterScope* outer_;
  ZoneVector<int> allocated_;
  int next_consecutive_register_;
  int next_consecutive_count_;

  DISALLOW_COPY_AND_ASSIGN(TemporaryRegisterScope);
};


}  // namespace interpreter
}  // namespace internal
}  // namespace v8

#endif  // V8_INTERPRETER_BYTECODE_ARRAY_BUILDER_H_
