// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTERPRETER_BYTECODES_H_
#define V8_INTERPRETER_BYTECODES_H_

#include <iosfwd>

// Clients of this interface shouldn't depend on lots of interpreter internals.
// Do not include anything from src/interpreter here!
#include "src/utils.h"

namespace v8 {
namespace internal {
namespace interpreter {

// The list of operand types used by bytecodes.
#define OPERAND_TYPE_LIST(V)       \
                                   \
  /* None operand. */              \
  V(None, OperandSize::kNone)      \
                                   \
  /* Byte operands. */             \
  V(Count8, OperandSize::kByte)    \
  V(Imm8, OperandSize::kByte)      \
  V(Idx8, OperandSize::kByte)      \
  V(MaybeReg8, OperandSize::kByte) \
  V(Reg8, OperandSize::kByte)      \
  V(RegPair8, OperandSize::kByte)  \
                                   \
  /* Short operands. */            \
  V(Count16, OperandSize::kShort)  \
  V(Idx16, OperandSize::kShort)    \
  V(Reg16, OperandSize::kShort)

// The list of bytecodes which are interpreted by the interpreter.
#define BYTECODE_LIST(V)                                                       \
                                                                               \
  /* Loading the accumulator */                                                \
  V(LdaZero, OperandType::kNone)                                               \
  V(LdaSmi8, OperandType::kImm8)                                               \
  V(LdaUndefined, OperandType::kNone)                                          \
  V(LdaNull, OperandType::kNone)                                               \
  V(LdaTheHole, OperandType::kNone)                                            \
  V(LdaTrue, OperandType::kNone)                                               \
  V(LdaFalse, OperandType::kNone)                                              \
  V(LdaConstant, OperandType::kIdx8)                                           \
  V(LdaConstantWide, OperandType::kIdx16)                                      \
                                                                               \
  /* Globals */                                                                \
  V(LdaGlobalSloppy, OperandType::kIdx8, OperandType::kIdx8)                   \
  V(LdaGlobalStrict, OperandType::kIdx8, OperandType::kIdx8)                   \
  V(LdaGlobalInsideTypeofSloppy, OperandType::kIdx8, OperandType::kIdx8)       \
  V(LdaGlobalInsideTypeofStrict, OperandType::kIdx8, OperandType::kIdx8)       \
  V(LdaGlobalSloppyWide, OperandType::kIdx16, OperandType::kIdx16)             \
  V(LdaGlobalStrictWide, OperandType::kIdx16, OperandType::kIdx16)             \
  V(LdaGlobalInsideTypeofSloppyWide, OperandType::kIdx16, OperandType::kIdx16) \
  V(LdaGlobalInsideTypeofStrictWide, OperandType::kIdx16, OperandType::kIdx16) \
  V(StaGlobalSloppy, OperandType::kIdx8, OperandType::kIdx8)                   \
  V(StaGlobalStrict, OperandType::kIdx8, OperandType::kIdx8)                   \
  V(StaGlobalSloppyWide, OperandType::kIdx16, OperandType::kIdx16)             \
  V(StaGlobalStrictWide, OperandType::kIdx16, OperandType::kIdx16)             \
                                                                               \
  /* Context operations */                                                     \
  V(PushContext, OperandType::kReg8)                                           \
  V(PopContext, OperandType::kReg8)                                            \
  V(LdaContextSlot, OperandType::kReg8, OperandType::kIdx8)                    \
  V(StaContextSlot, OperandType::kReg8, OperandType::kIdx8)                    \
  V(LdaContextSlotWide, OperandType::kReg8, OperandType::kIdx16)               \
  V(StaContextSlotWide, OperandType::kReg8, OperandType::kIdx16)               \
                                                                               \
  /* Load-Store lookup slots */                                                \
  V(LdaLookupSlot, OperandType::kIdx8)                                         \
  V(LdaLookupSlotInsideTypeof, OperandType::kIdx8)                             \
  V(LdaLookupSlotWide, OperandType::kIdx16)                                    \
  V(LdaLookupSlotInsideTypeofWide, OperandType::kIdx16)                        \
  V(StaLookupSlotSloppy, OperandType::kIdx8)                                   \
  V(StaLookupSlotStrict, OperandType::kIdx8)                                   \
  V(StaLookupSlotSloppyWide, OperandType::kIdx16)                              \
  V(StaLookupSlotStrictWide, OperandType::kIdx16)                              \
                                                                               \
  /* Register-accumulator transfers */                                         \
  V(Ldar, OperandType::kReg8)                                                  \
  V(Star, OperandType::kReg8)                                                  \
                                                                               \
  /* Register-register transfers */                                            \
  V(Mov, OperandType::kReg8, OperandType::kReg8)                               \
  V(Exchange, OperandType::kReg8, OperandType::kReg16)                         \
  V(ExchangeWide, OperandType::kReg16, OperandType::kReg16)                    \
                                                                               \
  /* LoadIC operations */                                                      \
  V(LoadICSloppy, OperandType::kReg8, OperandType::kIdx8, OperandType::kIdx8)  \
  V(LoadICStrict, OperandType::kReg8, OperandType::kIdx8, OperandType::kIdx8)  \
  V(KeyedLoadICSloppy, OperandType::kReg8, OperandType::kIdx8)                 \
  V(KeyedLoadICStrict, OperandType::kReg8, OperandType::kIdx8)                 \
  /* TODO(rmcilroy): Wide register operands too? */                            \
  V(LoadICSloppyWide, OperandType::kReg8, OperandType::kIdx16,                 \
    OperandType::kIdx16)                                                       \
  V(LoadICStrictWide, OperandType::kReg8, OperandType::kIdx16,                 \
    OperandType::kIdx16)                                                       \
  V(KeyedLoadICSloppyWide, OperandType::kReg8, OperandType::kIdx16)            \
  V(KeyedLoadICStrictWide, OperandType::kReg8, OperandType::kIdx16)            \
                                                                               \
  /* StoreIC operations */                                                     \
  V(StoreICSloppy, OperandType::kReg8, OperandType::kIdx8, OperandType::kIdx8) \
  V(StoreICStrict, OperandType::kReg8, OperandType::kIdx8, OperandType::kIdx8) \
  V(KeyedStoreICSloppy, OperandType::kReg8, OperandType::kReg8,                \
    OperandType::kIdx8)                                                        \
  V(KeyedStoreICStrict, OperandType::kReg8, OperandType::kReg8,                \
    OperandType::kIdx8)                                                        \
  /* TODO(rmcilroy): Wide register operands too? */                            \
  V(StoreICSloppyWide, OperandType::kReg8, OperandType::kIdx16,                \
    OperandType::kIdx16)                                                       \
  V(StoreICStrictWide, OperandType::kReg8, OperandType::kIdx16,                \
    OperandType::kIdx16)                                                       \
  V(KeyedStoreICSloppyWide, OperandType::kReg8, OperandType::kReg8,            \
    OperandType::kIdx16)                                                       \
  V(KeyedStoreICStrictWide, OperandType::kReg8, OperandType::kReg8,            \
    OperandType::kIdx16)                                                       \
                                                                               \
  /* Binary Operators */                                                       \
  V(Add, OperandType::kReg8)                                                   \
  V(Sub, OperandType::kReg8)                                                   \
  V(Mul, OperandType::kReg8)                                                   \
  V(Div, OperandType::kReg8)                                                   \
  V(Mod, OperandType::kReg8)                                                   \
  V(BitwiseOr, OperandType::kReg8)                                             \
  V(BitwiseXor, OperandType::kReg8)                                            \
  V(BitwiseAnd, OperandType::kReg8)                                            \
  V(ShiftLeft, OperandType::kReg8)                                             \
  V(ShiftRight, OperandType::kReg8)                                            \
  V(ShiftRightLogical, OperandType::kReg8)                                     \
                                                                               \
  /* Unary Operators */                                                        \
  V(Inc, OperandType::kNone)                                                   \
  V(Dec, OperandType::kNone)                                                   \
  V(LogicalNot, OperandType::kNone)                                            \
  V(TypeOf, OperandType::kNone)                                                \
  V(DeletePropertyStrict, OperandType::kReg8)                                  \
  V(DeletePropertySloppy, OperandType::kReg8)                                  \
  V(DeleteLookupSlot, OperandType::kNone)                                      \
                                                                               \
  /* Call operations */                                                        \
  V(Call, OperandType::kReg8, OperandType::kReg8, OperandType::kCount8,        \
    OperandType::kIdx8)                                                        \
  V(CallWide, OperandType::kReg8, OperandType::kReg8, OperandType::kCount16,   \
    OperandType::kIdx16)                                                       \
  V(CallRuntime, OperandType::kIdx16, OperandType::kMaybeReg8,                 \
    OperandType::kCount8)                                                      \
  V(CallRuntimeForPair, OperandType::kIdx16, OperandType::kMaybeReg8,          \
    OperandType::kCount8, OperandType::kRegPair8)                              \
  V(CallJSRuntime, OperandType::kIdx16, OperandType::kReg8,                    \
    OperandType::kCount8)                                                      \
                                                                               \
  /* New operator */                                                           \
  V(New, OperandType::kReg8, OperandType::kMaybeReg8, OperandType::kCount8)    \
                                                                               \
  /* Test Operators */                                                         \
  V(TestEqual, OperandType::kReg8)                                             \
  V(TestNotEqual, OperandType::kReg8)                                          \
  V(TestEqualStrict, OperandType::kReg8)                                       \
  V(TestNotEqualStrict, OperandType::kReg8)                                    \
  V(TestLessThan, OperandType::kReg8)                                          \
  V(TestGreaterThan, OperandType::kReg8)                                       \
  V(TestLessThanOrEqual, OperandType::kReg8)                                   \
  V(TestGreaterThanOrEqual, OperandType::kReg8)                                \
  V(TestInstanceOf, OperandType::kReg8)                                        \
  V(TestIn, OperandType::kReg8)                                                \
                                                                               \
  /* Cast operators */                                                         \
  V(ToName, OperandType::kNone)                                                \
  V(ToNumber, OperandType::kNone)                                              \
  V(ToObject, OperandType::kNone)                                              \
                                                                               \
  /* Literals */                                                               \
  V(CreateRegExpLiteral, OperandType::kIdx8, OperandType::kIdx8,               \
    OperandType::kImm8)                                                        \
  V(CreateArrayLiteral, OperandType::kIdx8, OperandType::kIdx8,                \
    OperandType::kImm8)                                                        \
  V(CreateObjectLiteral, OperandType::kIdx8, OperandType::kIdx8,               \
    OperandType::kImm8)                                                        \
  V(CreateRegExpLiteralWide, OperandType::kIdx16, OperandType::kIdx16,         \
    OperandType::kImm8)                                                        \
  V(CreateArrayLiteralWide, OperandType::kIdx16, OperandType::kIdx16,          \
    OperandType::kImm8)                                                        \
  V(CreateObjectLiteralWide, OperandType::kIdx16, OperandType::kIdx16,         \
    OperandType::kImm8)                                                        \
                                                                               \
  /* Closure allocation */                                                     \
  V(CreateClosure, OperandType::kIdx8, OperandType::kImm8)                     \
  V(CreateClosureWide, OperandType::kIdx16, OperandType::kImm8)                \
                                                                               \
  /* Arguments allocation */                                                   \
  V(CreateMappedArguments, OperandType::kNone)                                 \
  V(CreateUnmappedArguments, OperandType::kNone)                               \
                                                                               \
  /* Control Flow */                                                           \
  V(Jump, OperandType::kImm8)                                                  \
  V(JumpConstant, OperandType::kIdx8)                                          \
  V(JumpConstantWide, OperandType::kIdx16)                                     \
  V(JumpIfTrue, OperandType::kImm8)                                            \
  V(JumpIfTrueConstant, OperandType::kIdx8)                                    \
  V(JumpIfTrueConstantWide, OperandType::kIdx16)                               \
  V(JumpIfFalse, OperandType::kImm8)                                           \
  V(JumpIfFalseConstant, OperandType::kIdx8)                                   \
  V(JumpIfFalseConstantWide, OperandType::kIdx16)                              \
  V(JumpIfToBooleanTrue, OperandType::kImm8)                                   \
  V(JumpIfToBooleanTrueConstant, OperandType::kIdx8)                           \
  V(JumpIfToBooleanTrueConstantWide, OperandType::kIdx16)                      \
  V(JumpIfToBooleanFalse, OperandType::kImm8)                                  \
  V(JumpIfToBooleanFalseConstant, OperandType::kIdx8)                          \
  V(JumpIfToBooleanFalseConstantWide, OperandType::kIdx16)                     \
  V(JumpIfNull, OperandType::kImm8)                                            \
  V(JumpIfNullConstant, OperandType::kIdx8)                                    \
  V(JumpIfNullConstantWide, OperandType::kIdx16)                               \
  V(JumpIfUndefined, OperandType::kImm8)                                       \
  V(JumpIfUndefinedConstant, OperandType::kIdx8)                               \
  V(JumpIfUndefinedConstantWide, OperandType::kIdx16)                          \
                                                                               \
  /* Complex flow control For..in */                                           \
  V(ForInPrepare, OperandType::kReg8, OperandType::kReg8, OperandType::kReg8)  \
  V(ForInDone, OperandType::kReg8, OperandType::kReg8)                         \
  V(ForInNext, OperandType::kReg8, OperandType::kReg8, OperandType::kReg8,     \
    OperandType::kReg8)                                                        \
  V(ForInStep, OperandType::kReg8)                                             \
                                                                               \
  /* Non-local flow control */                                                 \
  V(Throw, OperandType::kNone)                                                 \
  V(Return, OperandType::kNone)


// Enumeration of the size classes of operand types used by bytecodes.
enum class OperandSize : uint8_t {
  kNone = 0,
  kByte = 1,
  kShort = 2,
};


// Enumeration of operand types used by bytecodes.
enum class OperandType : uint8_t {
#define DECLARE_OPERAND_TYPE(Name, _) k##Name,
  OPERAND_TYPE_LIST(DECLARE_OPERAND_TYPE)
#undef DECLARE_OPERAND_TYPE
#define COUNT_OPERAND_TYPES(x, _) +1
  // The COUNT_OPERAND macro will turn this into kLast = -1 +1 +1... which will
  // evaluate to the same value as the last operand.
  kLast = -1 OPERAND_TYPE_LIST(COUNT_OPERAND_TYPES)
#undef COUNT_OPERAND_TYPES
};


// Enumeration of interpreter bytecodes.
enum class Bytecode : uint8_t {
#define DECLARE_BYTECODE(Name, ...) k##Name,
  BYTECODE_LIST(DECLARE_BYTECODE)
#undef DECLARE_BYTECODE
#define COUNT_BYTECODE(x, ...) +1
  // The COUNT_BYTECODE macro will turn this into kLast = -1 +1 +1... which will
  // evaluate to the same value as the last real bytecode.
  kLast = -1 BYTECODE_LIST(COUNT_BYTECODE)
#undef COUNT_BYTECODE
};


// An interpreter Register which is located in the function's Register file
// in its stack-frame. Register hold parameters, this, and expression values.
class Register {
 public:
  Register() : index_(kIllegalIndex) {}

  explicit Register(int index) : index_(index) {}

  int index() const {
    DCHECK(index_ != kIllegalIndex);
    return index_;
  }
  bool is_parameter() const { return index() < 0; }
  bool is_valid() const { return index_ != kIllegalIndex; }

  static Register FromParameterIndex(int index, int parameter_count);
  int ToParameterIndex(int parameter_count) const;
  static int MaxParameterIndex();

  // Returns the register for the function's closure object.
  static Register function_closure();
  bool is_function_closure() const;

  // Returns the register for the function's outer context.
  static Register function_context();
  bool is_function_context() const;

  // Returns the register for the incoming new target value.
  static Register new_target();
  bool is_new_target() const;

  static Register FromOperand(uint8_t operand);
  uint8_t ToOperand() const;

  static Register FromWideOperand(uint16_t operand);
  uint16_t ToWideOperand() const;

  static bool AreContiguous(Register reg1, Register reg2,
                            Register reg3 = Register(),
                            Register reg4 = Register(),
                            Register reg5 = Register());

  bool operator==(const Register& other) const {
    return index() == other.index();
  }
  bool operator!=(const Register& other) const {
    return index() != other.index();
  }
  bool operator<(const Register& other) const {
    return index() < other.index();
  }
  bool operator<=(const Register& other) const {
    return index() <= other.index();
  }

 private:
  static const int kIllegalIndex = kMaxInt;

  void* operator new(size_t size);
  void operator delete(void* p);

  int index_;
};


class Bytecodes {
 public:
  // Returns string representation of |bytecode|.
  static const char* ToString(Bytecode bytecode);

  // Returns string representation of |operand_type|.
  static const char* OperandTypeToString(OperandType operand_type);

  // Returns string representation of |operand_size|.
  static const char* OperandSizeToString(OperandSize operand_size);

  // Returns byte value of bytecode.
  static uint8_t ToByte(Bytecode bytecode);

  // Returns bytecode for |value|.
  static Bytecode FromByte(uint8_t value);

  // Returns the number of operands expected by |bytecode|.
  static int NumberOfOperands(Bytecode bytecode);

  // Return the i-th operand of |bytecode|.
  static OperandType GetOperandType(Bytecode bytecode, int i);

  // Return the size of the i-th operand of |bytecode|.
  static OperandSize GetOperandSize(Bytecode bytecode, int i);

  // Returns the offset of the i-th operand of |bytecode| relative to the start
  // of the bytecode.
  static int GetOperandOffset(Bytecode bytecode, int i);

  // Returns the size of the bytecode including its operands.
  static int Size(Bytecode bytecode);

  // Returns the size of |operand|.
  static OperandSize SizeOfOperand(OperandType operand);

  // Return true if the bytecode is a conditional jump taking
  // an immediate byte operand (OperandType::kImm8).
  static bool IsConditionalJumpImmediate(Bytecode bytecode);

  // Return true if the bytecode is a conditional jump taking
  // a constant pool entry (OperandType::kIdx8).
  static bool IsConditionalJumpConstant(Bytecode bytecode);

  // Return true if the bytecode is a conditional jump taking
  // a constant pool entry (OperandType::kIdx16).
  static bool IsConditionalJumpConstantWide(Bytecode bytecode);

  // Return true if the bytecode is a conditional jump taking
  // any kind of operand.
  static bool IsConditionalJump(Bytecode bytecode);

  // Return true if the bytecode is a jump or a conditional jump taking
  // an immediate byte operand (OperandType::kImm8).
  static bool IsJumpImmediate(Bytecode bytecode);

  // Return true if the bytecode is a jump or conditional jump taking a
  // constant pool entry (OperandType::kIdx8).
  static bool IsJumpConstant(Bytecode bytecode);

  // Return true if the bytecode is a jump or conditional jump taking a
  // constant pool entry (OperandType::kIdx16).
  static bool IsJumpConstantWide(Bytecode bytecode);

  // Return true if the bytecode is a jump or conditional jump taking
  // any kind of operand.
  static bool IsJump(Bytecode bytecode);

  // Return true if the bytecode is a conditional jump, a jump, or a return.
  static bool IsJumpOrReturn(Bytecode bytecode);

  // Decode a single bytecode and operands to |os|.
  static std::ostream& Decode(std::ostream& os, const uint8_t* bytecode_start,
                              int number_of_parameters);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(Bytecodes);
};

std::ostream& operator<<(std::ostream& os, const Bytecode& bytecode);
std::ostream& operator<<(std::ostream& os, const OperandType& operand_type);
std::ostream& operator<<(std::ostream& os, const OperandSize& operand_type);

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

#endif  // V8_INTERPRETER_BYTECODES_H_
