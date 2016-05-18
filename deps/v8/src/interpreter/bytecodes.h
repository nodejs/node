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

#define INVALID_OPERAND_TYPE_LIST(V) \
  V(None, OperandSize::kNone)

#define REGISTER_INPUT_OPERAND_TYPE_LIST(V) \
  /* Byte operands. */                      \
  V(MaybeReg8, OperandSize::kByte)          \
  V(Reg8, OperandSize::kByte)               \
  V(RegPair8, OperandSize::kByte)           \
  /* Short operands. */                     \
  V(MaybeReg16, OperandSize::kShort)        \
  V(Reg16, OperandSize::kShort)             \
  V(RegPair16, OperandSize::kShort)

#define REGISTER_OUTPUT_OPERAND_TYPE_LIST(V) \
  /* Byte operands. */                       \
  V(RegOut8, OperandSize::kByte)             \
  V(RegOutPair8, OperandSize::kByte)         \
  V(RegOutTriple8, OperandSize::kByte)       \
  /* Short operands. */                      \
  V(RegOut16, OperandSize::kShort)           \
  V(RegOutPair16, OperandSize::kShort)       \
  V(RegOutTriple16, OperandSize::kShort)

#define SCALAR_OPERAND_TYPE_LIST(V) \
  /* Byte operands. */              \
  V(Idx8, OperandSize::kByte)       \
  V(Imm8, OperandSize::kByte)       \
  V(RegCount8, OperandSize::kByte)  \
  /* Short operands. */             \
  V(Idx16, OperandSize::kShort)     \
  V(RegCount16, OperandSize::kShort)

#define REGISTER_OPERAND_TYPE_LIST(V) \
  REGISTER_INPUT_OPERAND_TYPE_LIST(V) \
  REGISTER_OUTPUT_OPERAND_TYPE_LIST(V)

#define NON_REGISTER_OPERAND_TYPE_LIST(V) \
  INVALID_OPERAND_TYPE_LIST(V)            \
  SCALAR_OPERAND_TYPE_LIST(V)

// The list of operand types used by bytecodes.
#define OPERAND_TYPE_LIST(V)        \
  NON_REGISTER_OPERAND_TYPE_LIST(V) \
  REGISTER_OPERAND_TYPE_LIST(V)

// Define one debug break bytecode for each operands size.
#define DEBUG_BREAK_BYTECODE_LIST(V)                                           \
  V(DebugBreak0, OperandType::kNone)                                           \
  V(DebugBreak1, OperandType::kReg8)                                           \
  V(DebugBreak2, OperandType::kReg16)                                          \
  V(DebugBreak3, OperandType::kReg16, OperandType::kReg8)                      \
  V(DebugBreak4, OperandType::kReg16, OperandType::kReg16)                     \
  V(DebugBreak5, OperandType::kReg16, OperandType::kReg16, OperandType::kReg8) \
  V(DebugBreak6, OperandType::kReg16, OperandType::kReg16,                     \
    OperandType::kReg16)                                                       \
  V(DebugBreak7, OperandType::kReg16, OperandType::kReg16,                     \
    OperandType::kReg16, OperandType::kReg8)                                   \
  V(DebugBreak8, OperandType::kReg16, OperandType::kReg16,                     \
    OperandType::kReg16, OperandType::kReg16)

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
  V(LdaGlobal, OperandType::kIdx8, OperandType::kIdx8)                         \
  V(LdaGlobalInsideTypeof, OperandType::kIdx8, OperandType::kIdx8)             \
  V(LdaGlobalWide, OperandType::kIdx16, OperandType::kIdx16)                   \
  V(LdaGlobalInsideTypeofWide, OperandType::kIdx16, OperandType::kIdx16)       \
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
  V(Star, OperandType::kRegOut8)                                               \
                                                                               \
  /* Register-register transfers */                                            \
  V(Mov, OperandType::kReg8, OperandType::kRegOut8)                            \
  V(MovWide, OperandType::kReg16, OperandType::kRegOut16)                      \
                                                                               \
  /* LoadIC operations */                                                      \
  V(LoadIC, OperandType::kReg8, OperandType::kIdx8, OperandType::kIdx8)        \
  V(KeyedLoadIC, OperandType::kReg8, OperandType::kIdx8)                       \
  V(LoadICWide, OperandType::kReg8, OperandType::kIdx16, OperandType::kIdx16)  \
  V(KeyedLoadICWide, OperandType::kReg8, OperandType::kIdx16)                  \
                                                                               \
  /* StoreIC operations */                                                     \
  V(StoreICSloppy, OperandType::kReg8, OperandType::kIdx8, OperandType::kIdx8) \
  V(StoreICStrict, OperandType::kReg8, OperandType::kIdx8, OperandType::kIdx8) \
  V(KeyedStoreICSloppy, OperandType::kReg8, OperandType::kReg8,                \
    OperandType::kIdx8)                                                        \
  V(KeyedStoreICStrict, OperandType::kReg8, OperandType::kReg8,                \
    OperandType::kIdx8)                                                        \
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
                                                                               \
  /* Call operations */                                                        \
  V(Call, OperandType::kReg8, OperandType::kReg8, OperandType::kRegCount8,     \
    OperandType::kIdx8)                                                        \
  V(CallWide, OperandType::kReg16, OperandType::kReg16,                        \
    OperandType::kRegCount16, OperandType::kIdx16)                             \
  V(TailCall, OperandType::kReg8, OperandType::kReg8, OperandType::kRegCount8, \
    OperandType::kIdx8)                                                        \
  V(TailCallWide, OperandType::kReg16, OperandType::kReg16,                    \
    OperandType::kRegCount16, OperandType::kIdx16)                             \
  V(CallRuntime, OperandType::kIdx16, OperandType::kMaybeReg8,                 \
    OperandType::kRegCount8)                                                   \
  V(CallRuntimeWide, OperandType::kIdx16, OperandType::kMaybeReg16,            \
    OperandType::kRegCount8)                                                   \
  V(CallRuntimeForPair, OperandType::kIdx16, OperandType::kMaybeReg8,          \
    OperandType::kRegCount8, OperandType::kRegOutPair8)                        \
  V(CallRuntimeForPairWide, OperandType::kIdx16, OperandType::kMaybeReg16,     \
    OperandType::kRegCount8, OperandType::kRegOutPair16)                       \
  V(CallJSRuntime, OperandType::kIdx16, OperandType::kReg8,                    \
    OperandType::kRegCount8)                                                   \
  V(CallJSRuntimeWide, OperandType::kIdx16, OperandType::kReg16,               \
    OperandType::kRegCount16)                                                  \
                                                                               \
  /* New operator */                                                           \
  V(New, OperandType::kReg8, OperandType::kMaybeReg8, OperandType::kRegCount8) \
  V(NewWide, OperandType::kReg16, OperandType::kMaybeReg16,                    \
    OperandType::kRegCount16)                                                  \
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
  V(CreateRestParameter, OperandType::kNone)                                   \
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
  V(JumpIfNotHole, OperandType::kImm8)                                         \
  V(JumpIfNotHoleConstant, OperandType::kIdx8)                                 \
  V(JumpIfNotHoleConstantWide, OperandType::kIdx16)                            \
                                                                               \
  /* Complex flow control For..in */                                           \
  V(ForInPrepare, OperandType::kRegOutTriple8)                                 \
  V(ForInPrepareWide, OperandType::kRegOutTriple16)                            \
  V(ForInDone, OperandType::kReg8, OperandType::kReg8)                         \
  V(ForInNext, OperandType::kReg8, OperandType::kReg8, OperandType::kRegPair8) \
  V(ForInNextWide, OperandType::kReg16, OperandType::kReg16,                   \
    OperandType::kRegPair16)                                                   \
  V(ForInStep, OperandType::kReg8)                                             \
                                                                               \
  /* Perform a stack guard check */                                            \
  V(StackCheck, OperandType::kNone)                                            \
                                                                               \
  /* Non-local flow control */                                                 \
  V(Throw, OperandType::kNone)                                                 \
  V(ReThrow, OperandType::kNone)                                               \
  V(Return, OperandType::kNone)                                                \
                                                                               \
  /* Debugger */                                                               \
  V(Debugger, OperandType::kNone)                                              \
  DEBUG_BREAK_BYTECODE_LIST(V)

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
  explicit Register(int index = kInvalidIndex) : index_(index) {}

  int index() const { return index_; }
  bool is_parameter() const { return index() < 0; }
  bool is_valid() const { return index_ != kInvalidIndex; }
  bool is_byte_operand() const;
  bool is_short_operand() const;

  static Register FromParameterIndex(int index, int parameter_count);
  int ToParameterIndex(int parameter_count) const;
  static int MaxParameterIndex();
  static int MaxRegisterIndex();
  static int MaxRegisterIndexForByteOperand();

  // Returns an invalid register.
  static Register invalid_value() { return Register(); }

  // Returns the register for the function's closure object.
  static Register function_closure();
  bool is_function_closure() const;

  // Returns the register which holds the current context object.
  static Register current_context();
  bool is_current_context() const;

  // Returns the register for the incoming new target value.
  static Register new_target();
  bool is_new_target() const;

  static Register FromOperand(uint8_t operand);
  uint8_t ToOperand() const;

  static Register FromWideOperand(uint16_t operand);
  uint16_t ToWideOperand() const;

  static Register FromRawOperand(uint32_t raw_operand);
  uint32_t ToRawOperand() const;

  static bool AreContiguous(Register reg1, Register reg2,
                            Register reg3 = Register(),
                            Register reg4 = Register(),
                            Register reg5 = Register());

  std::string ToString(int parameter_count);

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
  bool operator>(const Register& other) const {
    return index() > other.index();
  }
  bool operator>=(const Register& other) const {
    return index() >= other.index();
  }

 private:
  static const int kInvalidIndex = kMaxInt;

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

  // Returns the number of register operands expected by |bytecode|.
  static int NumberOfRegisterOperands(Bytecode bytecode);

  // Returns the i-th operand of |bytecode|.
  static OperandType GetOperandType(Bytecode bytecode, int i);

  // Returns the size of the i-th operand of |bytecode|.
  static OperandSize GetOperandSize(Bytecode bytecode, int i);

  // Returns the offset of the i-th operand of |bytecode| relative to the start
  // of the bytecode.
  static int GetOperandOffset(Bytecode bytecode, int i);

  // Returns a zero-based bitmap of the register operand positions of
  // |bytecode|.
  static int GetRegisterOperandBitmap(Bytecode bytecode);

  // Returns a debug break bytecode with a matching operand size.
  static Bytecode GetDebugBreak(Bytecode bytecode);

  // Returns the size of the bytecode including its operands.
  static int Size(Bytecode bytecode);

  // Returns the size of |operand|.
  static OperandSize SizeOfOperand(OperandType operand);

  // Returns true if the bytecode is a conditional jump taking
  // an immediate byte operand (OperandType::kImm8).
  static bool IsConditionalJumpImmediate(Bytecode bytecode);

  // Returns true if the bytecode is a conditional jump taking
  // a constant pool entry (OperandType::kIdx8).
  static bool IsConditionalJumpConstant(Bytecode bytecode);

  // Returns true if the bytecode is a conditional jump taking
  // a constant pool entry (OperandType::kIdx16).
  static bool IsConditionalJumpConstantWide(Bytecode bytecode);

  // Returns true if the bytecode is a conditional jump taking
  // any kind of operand.
  static bool IsConditionalJump(Bytecode bytecode);

  // Returns true if the bytecode is a jump or a conditional jump taking
  // an immediate byte operand (OperandType::kImm8).
  static bool IsJumpImmediate(Bytecode bytecode);

  // Returns true if the bytecode is a jump or conditional jump taking a
  // constant pool entry (OperandType::kIdx8).
  static bool IsJumpConstant(Bytecode bytecode);

  // Returns true if the bytecode is a jump or conditional jump taking a
  // constant pool entry (OperandType::kIdx16).
  static bool IsJumpConstantWide(Bytecode bytecode);

  // Returns true if the bytecode is a jump or conditional jump taking
  // any kind of operand.
  static bool IsJump(Bytecode bytecode);

  // Returns true if the bytecode is a conditional jump, a jump, or a return.
  static bool IsJumpOrReturn(Bytecode bytecode);

  // Returns true if the bytecode is a call or a constructor call.
  static bool IsCallOrNew(Bytecode bytecode);

  // Returns true if the bytecode is a debug break.
  static bool IsDebugBreak(Bytecode bytecode);

  // Returns true if |operand_type| is a register index operand (kIdx8/kIdx16).
  static bool IsIndexOperandType(OperandType operand_type);

  // Returns true if |operand_type| represents an immediate.
  static bool IsImmediateOperandType(OperandType operand_type);

  // Returns true if |operand_type| is a register count operand
  // (kRegCount8/kRegCount16).
  static bool IsRegisterCountOperandType(OperandType operand_type);

  // Returns true if |operand_type| is any type of register operand.
  static bool IsRegisterOperandType(OperandType operand_type);

  // Returns true if |operand_type| represents a register used as an input.
  static bool IsRegisterInputOperandType(OperandType operand_type);

  // Returns true if |operand_type| represents a register used as an output.
  static bool IsRegisterOutputOperandType(OperandType operand_type);

  // Returns true if |operand_type| is a maybe register operand
  // (kMaybeReg8/kMaybeReg16).
  static bool IsMaybeRegisterOperandType(OperandType operand_type);

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
