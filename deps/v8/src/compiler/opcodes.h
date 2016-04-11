// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_OPCODES_H_
#define V8_COMPILER_OPCODES_H_

#include <iosfwd>

// Opcodes for control operators.
#define CONTROL_OP_LIST(V) \
  V(Start)                 \
  V(Loop)                  \
  V(Branch)                \
  V(Switch)                \
  V(IfTrue)                \
  V(IfFalse)               \
  V(IfSuccess)             \
  V(IfException)           \
  V(IfValue)               \
  V(IfDefault)             \
  V(Merge)                 \
  V(Deoptimize)            \
  V(Return)                \
  V(TailCall)              \
  V(Terminate)             \
  V(OsrNormalEntry)        \
  V(OsrLoopEntry)          \
  V(Throw)                 \
  V(End)

// Opcodes for constant operators.
#define CONSTANT_OP_LIST(V) \
  V(Int32Constant)          \
  V(Int64Constant)          \
  V(Float32Constant)        \
  V(Float64Constant)        \
  V(ExternalConstant)       \
  V(NumberConstant)         \
  V(HeapConstant)

#define INNER_OP_LIST(V) \
  V(Select)              \
  V(Phi)                 \
  V(EffectSet)           \
  V(EffectPhi)           \
  V(Guard)               \
  V(BeginRegion)         \
  V(FinishRegion)        \
  V(FrameState)          \
  V(StateValues)         \
  V(TypedStateValues)    \
  V(ObjectState)         \
  V(Call)                \
  V(Parameter)           \
  V(OsrValue)            \
  V(Projection)

#define COMMON_OP_LIST(V) \
  CONSTANT_OP_LIST(V)     \
  INNER_OP_LIST(V)        \
  V(Dead)

// Opcodes for JavaScript operators.
#define JS_COMPARE_BINOP_LIST(V) \
  V(JSEqual)                     \
  V(JSNotEqual)                  \
  V(JSStrictEqual)               \
  V(JSStrictNotEqual)            \
  V(JSLessThan)                  \
  V(JSGreaterThan)               \
  V(JSLessThanOrEqual)           \
  V(JSGreaterThanOrEqual)

#define JS_BITWISE_BINOP_LIST(V) \
  V(JSBitwiseOr)                 \
  V(JSBitwiseXor)                \
  V(JSBitwiseAnd)                \
  V(JSShiftLeft)                 \
  V(JSShiftRight)                \
  V(JSShiftRightLogical)

#define JS_ARITH_BINOP_LIST(V) \
  V(JSAdd)                     \
  V(JSSubtract)                \
  V(JSMultiply)                \
  V(JSDivide)                  \
  V(JSModulus)

#define JS_SIMPLE_BINOP_LIST(V) \
  JS_COMPARE_BINOP_LIST(V)      \
  JS_BITWISE_BINOP_LIST(V)      \
  JS_ARITH_BINOP_LIST(V)

#define JS_CONVERSION_UNOP_LIST(V) \
  V(JSToBoolean)                   \
  V(JSToNumber)                    \
  V(JSToString)                    \
  V(JSToName)                      \
  V(JSToObject)

#define JS_OTHER_UNOP_LIST(V) \
  V(JSTypeOf)

#define JS_SIMPLE_UNOP_LIST(V) \
  JS_CONVERSION_UNOP_LIST(V)   \
  JS_OTHER_UNOP_LIST(V)

#define JS_OBJECT_OP_LIST(V)  \
  V(JSCreate)                 \
  V(JSCreateArguments)        \
  V(JSCreateArray)            \
  V(JSCreateClosure)          \
  V(JSCreateIterResultObject) \
  V(JSCreateLiteralArray)     \
  V(JSCreateLiteralObject)    \
  V(JSCreateLiteralRegExp)    \
  V(JSLoadProperty)           \
  V(JSLoadNamed)              \
  V(JSLoadGlobal)             \
  V(JSStoreProperty)          \
  V(JSStoreNamed)             \
  V(JSStoreGlobal)            \
  V(JSDeleteProperty)         \
  V(JSHasProperty)            \
  V(JSInstanceOf)

#define JS_CONTEXT_OP_LIST(V) \
  V(JSLoadContext)            \
  V(JSStoreContext)           \
  V(JSLoadDynamic)            \
  V(JSCreateFunctionContext)  \
  V(JSCreateCatchContext)     \
  V(JSCreateWithContext)      \
  V(JSCreateBlockContext)     \
  V(JSCreateModuleContext)    \
  V(JSCreateScriptContext)

#define JS_OTHER_OP_LIST(V) \
  V(JSCallConstruct)        \
  V(JSCallFunction)         \
  V(JSCallRuntime)          \
  V(JSConvertReceiver)      \
  V(JSForInDone)            \
  V(JSForInNext)            \
  V(JSForInPrepare)         \
  V(JSForInStep)            \
  V(JSLoadMessage)          \
  V(JSStoreMessage)         \
  V(JSYield)                \
  V(JSStackCheck)

#define JS_OP_LIST(V)     \
  JS_SIMPLE_BINOP_LIST(V) \
  JS_SIMPLE_UNOP_LIST(V)  \
  JS_OBJECT_OP_LIST(V)    \
  JS_CONTEXT_OP_LIST(V)   \
  JS_OTHER_OP_LIST(V)

// Opcodes for VirtuaMachine-level operators.
#define SIMPLIFIED_COMPARE_BINOP_LIST(V) \
  V(NumberEqual)                         \
  V(NumberLessThan)                      \
  V(NumberLessThanOrEqual)               \
  V(ReferenceEqual)                      \
  V(StringEqual)                         \
  V(StringLessThan)                      \
  V(StringLessThanOrEqual)

#define SIMPLIFIED_OP_LIST(V)      \
  SIMPLIFIED_COMPARE_BINOP_LIST(V) \
  V(BooleanNot)                    \
  V(BooleanToNumber)               \
  V(NumberAdd)                     \
  V(NumberSubtract)                \
  V(NumberMultiply)                \
  V(NumberDivide)                  \
  V(NumberModulus)                 \
  V(NumberBitwiseOr)               \
  V(NumberBitwiseXor)              \
  V(NumberBitwiseAnd)              \
  V(NumberShiftLeft)               \
  V(NumberShiftRight)              \
  V(NumberShiftRightLogical)       \
  V(NumberToInt32)                 \
  V(NumberToUint32)                \
  V(NumberIsHoleNaN)               \
  V(PlainPrimitiveToNumber)        \
  V(ChangeTaggedToInt32)           \
  V(ChangeTaggedToUint32)          \
  V(ChangeTaggedToFloat64)         \
  V(ChangeInt32ToTagged)           \
  V(ChangeUint32ToTagged)          \
  V(ChangeFloat64ToTagged)         \
  V(ChangeBoolToBit)               \
  V(ChangeBitToBool)               \
  V(Allocate)                      \
  V(LoadField)                     \
  V(LoadBuffer)                    \
  V(LoadElement)                   \
  V(StoreField)                    \
  V(StoreBuffer)                   \
  V(StoreElement)                  \
  V(ObjectIsNumber)                \
  V(ObjectIsSmi)

// Opcodes for Machine-level operators.
#define MACHINE_COMPARE_BINOP_LIST(V) \
  V(Word32Equal)                      \
  V(Word64Equal)                      \
  V(Int32LessThan)                    \
  V(Int32LessThanOrEqual)             \
  V(Uint32LessThan)                   \
  V(Uint32LessThanOrEqual)            \
  V(Int64LessThan)                    \
  V(Int64LessThanOrEqual)             \
  V(Uint64LessThan)                   \
  V(Uint64LessThanOrEqual)            \
  V(Float32Equal)                     \
  V(Float32LessThan)                  \
  V(Float32LessThanOrEqual)           \
  V(Float64Equal)                     \
  V(Float64LessThan)                  \
  V(Float64LessThanOrEqual)

#define MACHINE_OP_LIST(V)      \
  MACHINE_COMPARE_BINOP_LIST(V) \
  V(Load)                       \
  V(Store)                      \
  V(Word32And)                  \
  V(Word32Or)                   \
  V(Word32Xor)                  \
  V(Word32Shl)                  \
  V(Word32Shr)                  \
  V(Word32Sar)                  \
  V(Word32Ror)                  \
  V(Word32Clz)                  \
  V(Word32Ctz)                  \
  V(Word32Popcnt)               \
  V(Word64Popcnt)               \
  V(Word64And)                  \
  V(Word64Or)                   \
  V(Word64Xor)                  \
  V(Word64Shl)                  \
  V(Word64Shr)                  \
  V(Word64Sar)                  \
  V(Word64Ror)                  \
  V(Word64Clz)                  \
  V(Word64Ctz)                  \
  V(Int32Add)                   \
  V(Int32AddWithOverflow)       \
  V(Int32Sub)                   \
  V(Int32SubWithOverflow)       \
  V(Int32Mul)                   \
  V(Int32MulHigh)               \
  V(Int32Div)                   \
  V(Int32Mod)                   \
  V(Uint32Div)                  \
  V(Uint32Mod)                  \
  V(Uint32MulHigh)              \
  V(Int64Add)                   \
  V(Int64AddWithOverflow)       \
  V(Int64Sub)                   \
  V(Int64SubWithOverflow)       \
  V(Int64Mul)                   \
  V(Int64Div)                   \
  V(Int64Mod)                   \
  V(Uint64Div)                  \
  V(Uint64Mod)                  \
  V(ChangeFloat32ToFloat64)     \
  V(ChangeFloat64ToInt32)       \
  V(ChangeFloat64ToUint32)      \
  V(TryTruncateFloat32ToInt64)  \
  V(TryTruncateFloat64ToInt64)  \
  V(TryTruncateFloat32ToUint64) \
  V(TryTruncateFloat64ToUint64) \
  V(ChangeInt32ToFloat64)       \
  V(ChangeInt32ToInt64)         \
  V(ChangeUint32ToFloat64)      \
  V(ChangeUint32ToUint64)       \
  V(TruncateFloat64ToFloat32)   \
  V(TruncateFloat64ToInt32)     \
  V(TruncateInt64ToInt32)       \
  V(RoundInt64ToFloat32)        \
  V(RoundInt64ToFloat64)        \
  V(RoundUint64ToFloat32)       \
  V(RoundUint64ToFloat64)       \
  V(BitcastFloat32ToInt32)      \
  V(BitcastFloat64ToInt64)      \
  V(BitcastInt32ToFloat32)      \
  V(BitcastInt64ToFloat64)      \
  V(Float32Add)                 \
  V(Float32Sub)                 \
  V(Float32Mul)                 \
  V(Float32Div)                 \
  V(Float32Max)                 \
  V(Float32Min)                 \
  V(Float32Abs)                 \
  V(Float32Sqrt)                \
  V(Float32RoundDown)           \
  V(Float64Add)                 \
  V(Float64Sub)                 \
  V(Float64Mul)                 \
  V(Float64Div)                 \
  V(Float64Mod)                 \
  V(Float64Max)                 \
  V(Float64Min)                 \
  V(Float64Abs)                 \
  V(Float64Sqrt)                \
  V(Float64RoundDown)           \
  V(Float32RoundUp)             \
  V(Float64RoundUp)             \
  V(Float32RoundTruncate)       \
  V(Float64RoundTruncate)       \
  V(Float64RoundTiesAway)       \
  V(Float32RoundTiesEven)       \
  V(Float64RoundTiesEven)       \
  V(Float64ExtractLowWord32)    \
  V(Float64ExtractHighWord32)   \
  V(Float64InsertLowWord32)     \
  V(Float64InsertHighWord32)    \
  V(LoadStackPointer)           \
  V(LoadFramePointer)           \
  V(CheckedLoad)                \
  V(CheckedStore)

#define VALUE_OP_LIST(V) \
  COMMON_OP_LIST(V)      \
  SIMPLIFIED_OP_LIST(V)  \
  MACHINE_OP_LIST(V)     \
  JS_OP_LIST(V)

// The combination of all operators at all levels and the common operators.
#define ALL_OP_LIST(V) \
  CONTROL_OP_LIST(V)   \
  VALUE_OP_LIST(V)

namespace v8 {
namespace internal {
namespace compiler {

// Declare an enumeration with all the opcodes at all levels so that they
// can be globally, uniquely numbered.
class IrOpcode {
 public:
  enum Value {
#define DECLARE_OPCODE(x) k##x,
    ALL_OP_LIST(DECLARE_OPCODE)
#undef DECLARE_OPCODE
    kLast = -1
#define COUNT_OPCODE(x) +1
            ALL_OP_LIST(COUNT_OPCODE)
#undef COUNT_OPCODE
  };

  // Returns the mnemonic name of an opcode.
  static char const* Mnemonic(Value value);

  // Returns true if opcode for common operator.
  static bool IsCommonOpcode(Value value) {
    return kStart <= value && value <= kDead;
  }

  // Returns true if opcode for control operator.
  static bool IsControlOpcode(Value value) {
    return kStart <= value && value <= kEnd;
  }

  // Returns true if opcode for JavaScript operator.
  static bool IsJsOpcode(Value value) {
    return kJSEqual <= value && value <= kJSStackCheck;
  }

  // Returns true if opcode for constant operator.
  static bool IsConstantOpcode(Value value) {
    return kInt32Constant <= value && value <= kHeapConstant;
  }

  static bool IsPhiOpcode(Value value) {
    return value == kPhi || value == kEffectPhi;
  }

  static bool IsMergeOpcode(Value value) {
    return value == kMerge || value == kLoop;
  }

  static bool IsIfProjectionOpcode(Value value) {
    return kIfTrue <= value && value <= kIfDefault;
  }

  // Returns true if opcode can be inlined.
  static bool IsInlineeOpcode(Value value) {
    return value == kJSCallConstruct || value == kJSCallFunction;
  }

  // Returns true if opcode for comparison operator.
  static bool IsComparisonOpcode(Value value) {
    return (kJSEqual <= value && value <= kJSGreaterThanOrEqual) ||
           (kNumberEqual <= value && value <= kStringLessThanOrEqual) ||
           (kWord32Equal <= value && value <= kFloat64LessThanOrEqual);
  }
};

std::ostream& operator<<(std::ostream&, IrOpcode::Value);

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_OPCODES_H_
