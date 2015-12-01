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

class BytecodeArrayBuilder {
 public:
  BytecodeArrayBuilder(Isolate* isolate, Zone* zone);
  Handle<BytecodeArray> ToBytecodeArray();

  // Set number of parameters expected by function.
  void set_parameter_count(int number_of_params);
  int parameter_count() const;

  // Set number of locals required for bytecode array.
  void set_locals_count(int number_of_locals);
  int locals_count() const;

  Register Parameter(int parameter_index);

  // Constant loads to accumulator.
  BytecodeArrayBuilder& LoadLiteral(v8::internal::Smi* value);
  BytecodeArrayBuilder& LoadLiteral(Handle<Object> object);
  BytecodeArrayBuilder& LoadUndefined();
  BytecodeArrayBuilder& LoadNull();
  BytecodeArrayBuilder& LoadTheHole();
  BytecodeArrayBuilder& LoadTrue();
  BytecodeArrayBuilder& LoadFalse();

  // Global loads to accumulator.
  BytecodeArrayBuilder& LoadGlobal(int slot_index);

  // Register-accumulator transfers.
  BytecodeArrayBuilder& LoadAccumulatorWithRegister(Register reg);
  BytecodeArrayBuilder& StoreAccumulatorInRegister(Register reg);

  // Load properties. The property name should be in the accumulator.
  BytecodeArrayBuilder& LoadNamedProperty(Register object, int feedback_slot,
                                          LanguageMode language_mode);
  BytecodeArrayBuilder& LoadKeyedProperty(Register object, int feedback_slot,
                                          LanguageMode language_mode);

  // Store properties. The value to be stored should be in the accumulator.
  BytecodeArrayBuilder& StoreNamedProperty(Register object, Register name,
                                           int feedback_slot,
                                           LanguageMode language_mode);
  BytecodeArrayBuilder& StoreKeyedProperty(Register object, Register key,
                                           int feedback_slot,
                                           LanguageMode language_mode);

  // Call a JS function. The JSFunction or Callable to be called should be in
  // |callable|, the receiver should be in |receiver| and all subsequent
  // arguments should be in registers <receiver + 1> to
  // <receiver + 1 + arg_count>.
  BytecodeArrayBuilder& Call(Register callable, Register receiver,
                             size_t arg_count);

  // Operators (register == lhs, accumulator = rhs).
  BytecodeArrayBuilder& BinaryOperation(Token::Value binop, Register reg);

  // Tests.
  BytecodeArrayBuilder& CompareOperation(Token::Value op, Register reg,
                                         LanguageMode language_mode);

  // Casts
  BytecodeArrayBuilder& CastAccumulatorToBoolean();

  // Flow Control.
  BytecodeArrayBuilder& Bind(BytecodeLabel* label);
  BytecodeArrayBuilder& Jump(BytecodeLabel* label);
  BytecodeArrayBuilder& JumpIfTrue(BytecodeLabel* label);
  BytecodeArrayBuilder& JumpIfFalse(BytecodeLabel* label);
  BytecodeArrayBuilder& Return();

  BytecodeArrayBuilder& EnterBlock();
  BytecodeArrayBuilder& LeaveBlock();

 private:
  ZoneVector<uint8_t>* bytecodes() { return &bytecodes_; }
  const ZoneVector<uint8_t>* bytecodes() const { return &bytecodes_; }
  Isolate* isolate() const { return isolate_; }

  static Bytecode BytecodeForBinaryOperation(Token::Value op);
  static Bytecode BytecodeForCompareOperation(Token::Value op);
  static bool FitsInIdxOperand(int value);
  static bool FitsInIdxOperand(size_t value);
  static bool FitsInImm8Operand(int value);
  static bool IsJumpWithImm8Operand(Bytecode jump_bytecode);
  static Bytecode GetJumpWithConstantOperand(Bytecode jump_with_smi8_operand);

  template <size_t N>
  INLINE(void Output(uint8_t(&bytes)[N]));
  void Output(Bytecode bytecode, uint8_t operand0, uint8_t operand1,
              uint8_t operand2);
  void Output(Bytecode bytecode, uint8_t operand0, uint8_t operand1);
  void Output(Bytecode bytecode, uint8_t operand0);
  void Output(Bytecode bytecode);
  void PatchJump(const ZoneVector<uint8_t>::iterator& jump_target,
                 ZoneVector<uint8_t>::iterator jump_location);
  BytecodeArrayBuilder& OutputJump(Bytecode jump_bytecode,
                                   BytecodeLabel* label);

  void EnsureReturn();

  bool OperandIsValid(Bytecode bytecode, int operand_index,
                      uint8_t operand_value) const;
  bool LastBytecodeInSameBlock() const;

  size_t GetConstantPoolEntry(Handle<Object> object);

  // Scope helpers used by TemporaryRegisterScope
  int BorrowTemporaryRegister();
  void ReturnTemporaryRegister(int reg_index);

  Isolate* isolate_;
  ZoneVector<uint8_t> bytecodes_;
  bool bytecode_generated_;
  size_t last_block_end_;
  size_t last_bytecode_start_;
  bool return_seen_in_block_;

  IdentityMap<size_t> constants_map_;
  ZoneVector<Handle<Object>> constants_;

  int parameter_count_;
  int local_register_count_;
  int temporary_register_count_;
  int temporary_register_next_;

  friend class TemporaryRegisterScope;
  DISALLOW_IMPLICIT_CONSTRUCTORS(BytecodeArrayBuilder);
};


// A label representing a branch target in a bytecode array. When a
// label is bound, it represents a known position in the bytecode
// array. For labels that are forward references there can be at most
// one reference whilst it is unbound.
class BytecodeLabel final {
 public:
  BytecodeLabel() : bound_(false), offset_(kInvalidOffset) {}
  ~BytecodeLabel() { DCHECK(bound_ && offset_ != kInvalidOffset); }

 private:
  static const size_t kInvalidOffset = static_cast<size_t>(-1);

  INLINE(void bind_to(size_t offset)) {
    DCHECK(!bound_ && offset != kInvalidOffset);
    offset_ = offset;
    bound_ = true;
  }
  INLINE(void set_referrer(size_t offset)) {
    DCHECK(!bound_ && offset != kInvalidOffset);
    offset_ = offset;
  }
  INLINE(size_t offset() const) { return offset_; }
  INLINE(bool is_bound() const) { return bound_; }
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
  DISALLOW_COPY_AND_ASSIGN(BytecodeLabel);
};


// A stack-allocated class than allows the instantiator to allocate
// temporary registers that are cleaned up when scope is closed.
class TemporaryRegisterScope {
 public:
  explicit TemporaryRegisterScope(BytecodeArrayBuilder* builder);
  ~TemporaryRegisterScope();
  Register NewRegister();

 private:
  void* operator new(size_t size);
  void operator delete(void* p);

  BytecodeArrayBuilder* builder_;
  int count_;
  int last_register_index_;

  DISALLOW_COPY_AND_ASSIGN(TemporaryRegisterScope);
};


}  // namespace interpreter
}  // namespace internal
}  // namespace v8

#endif  // V8_INTERPRETER_BYTECODE_ARRAY_BUILDER_H_
