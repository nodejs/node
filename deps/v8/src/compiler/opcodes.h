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
  V(DeoptimizeIf)          \
  V(DeoptimizeUnless)      \
  V(Return)                \
  V(TailCall)              \
  V(Terminate)             \
  V(OsrNormalEntry)        \
  V(OsrLoopEntry)          \
  V(Throw)                 \
  V(End)

// Opcodes for constant operators.
#define CONSTANT_OP_LIST(V)   \
  V(Int32Constant)            \
  V(Int64Constant)            \
  V(Float32Constant)          \
  V(Float64Constant)          \
  V(ExternalConstant)         \
  V(NumberConstant)           \
  V(HeapConstant)             \
  V(RelocatableInt32Constant) \
  V(RelocatableInt64Constant)

#define INNER_OP_LIST(V) \
  V(Select)              \
  V(Phi)                 \
  V(EffectPhi)           \
  V(CheckPoint)          \
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
  V(JSToInteger)                   \
  V(JSToLength)                    \
  V(JSToName)                      \
  V(JSToNumber)                    \
  V(JSToObject)                    \
  V(JSToString)

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
  V(NumberImul)                    \
  V(NumberClz32)                   \
  V(NumberCeil)                    \
  V(NumberFloor)                   \
  V(NumberRound)                   \
  V(NumberTrunc)                   \
  V(NumberToInt32)                 \
  V(NumberToUint32)                \
  V(NumberIsHoleNaN)               \
  V(StringToNumber)                \
  V(ChangeTaggedSignedToInt32)     \
  V(ChangeTaggedToInt32)           \
  V(ChangeTaggedToUint32)          \
  V(ChangeTaggedToFloat64)         \
  V(ChangeInt31ToTaggedSigned)     \
  V(ChangeInt32ToTagged)           \
  V(ChangeUint32ToTagged)          \
  V(ChangeFloat64ToTagged)         \
  V(ChangeTaggedToBit)             \
  V(ChangeBitToTagged)             \
  V(TruncateTaggedToWord32)        \
  V(Allocate)                      \
  V(LoadField)                     \
  V(LoadBuffer)                    \
  V(LoadElement)                   \
  V(StoreField)                    \
  V(StoreBuffer)                   \
  V(StoreElement)                  \
  V(ObjectIsCallable)              \
  V(ObjectIsNumber)                \
  V(ObjectIsReceiver)              \
  V(ObjectIsSmi)                   \
  V(ObjectIsString)                \
  V(ObjectIsUndetectable)          \
  V(TypeGuard)

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
  V(StackSlot)                  \
  V(Word32And)                  \
  V(Word32Or)                   \
  V(Word32Xor)                  \
  V(Word32Shl)                  \
  V(Word32Shr)                  \
  V(Word32Sar)                  \
  V(Word32Ror)                  \
  V(Word32Clz)                  \
  V(Word32Ctz)                  \
  V(Word32ReverseBits)          \
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
  V(Word64ReverseBits)          \
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
  V(BitcastWordToTagged)        \
  V(TruncateFloat64ToWord32)    \
  V(ChangeFloat32ToFloat64)     \
  V(ChangeFloat64ToInt32)       \
  V(ChangeFloat64ToUint32)      \
  V(TruncateFloat64ToUint32)    \
  V(TruncateFloat32ToInt32)     \
  V(TruncateFloat32ToUint32)    \
  V(TryTruncateFloat32ToInt64)  \
  V(TryTruncateFloat64ToInt64)  \
  V(TryTruncateFloat32ToUint64) \
  V(TryTruncateFloat64ToUint64) \
  V(ChangeInt32ToFloat64)       \
  V(ChangeInt32ToInt64)         \
  V(ChangeUint32ToFloat64)      \
  V(ChangeUint32ToUint64)       \
  V(TruncateFloat64ToFloat32)   \
  V(TruncateInt64ToInt32)       \
  V(RoundFloat64ToInt32)        \
  V(RoundInt32ToFloat32)        \
  V(RoundInt64ToFloat32)        \
  V(RoundInt64ToFloat64)        \
  V(RoundUint32ToFloat32)       \
  V(RoundUint64ToFloat32)       \
  V(RoundUint64ToFloat64)       \
  V(BitcastFloat32ToInt32)      \
  V(BitcastFloat64ToInt64)      \
  V(BitcastInt32ToFloat32)      \
  V(BitcastInt64ToFloat64)      \
  V(Float32Add)                 \
  V(Float32Sub)                 \
  V(Float32SubPreserveNan)      \
  V(Float32Mul)                 \
  V(Float32Div)                 \
  V(Float32Max)                 \
  V(Float32Min)                 \
  V(Float32Abs)                 \
  V(Float32Sqrt)                \
  V(Float32RoundDown)           \
  V(Float64Add)                 \
  V(Float64Sub)                 \
  V(Float64SubPreserveNan)      \
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
  V(LoadParentFramePointer)     \
  V(CheckedLoad)                \
  V(CheckedStore)               \
  V(Int32PairAdd)               \
  V(Int32PairSub)               \
  V(Int32PairMul)               \
  V(Word32PairShl)              \
  V(Word32PairShr)              \
  V(Word32PairSar)              \
  V(AtomicLoad)                 \
  V(AtomicStore)

#define MACHINE_SIMD_RETURN_SIMD_OP_LIST(V) \
  V(CreateFloat32x4)                        \
  V(Float32x4ReplaceLane)                   \
  V(Float32x4Abs)                           \
  V(Float32x4Neg)                           \
  V(Float32x4Sqrt)                          \
  V(Float32x4RecipApprox)                   \
  V(Float32x4RecipSqrtApprox)               \
  V(Float32x4Add)                           \
  V(Float32x4Sub)                           \
  V(Float32x4Mul)                           \
  V(Float32x4Div)                           \
  V(Float32x4Min)                           \
  V(Float32x4Max)                           \
  V(Float32x4MinNum)                        \
  V(Float32x4MaxNum)                        \
  V(Float32x4Equal)                         \
  V(Float32x4NotEqual)                      \
  V(Float32x4LessThan)                      \
  V(Float32x4LessThanOrEqual)               \
  V(Float32x4GreaterThan)                   \
  V(Float32x4GreaterThanOrEqual)            \
  V(Float32x4Select)                        \
  V(Float32x4Swizzle)                       \
  V(Float32x4Shuffle)                       \
  V(Float32x4FromInt32x4)                   \
  V(Float32x4FromUint32x4)                  \
  V(CreateInt32x4)                          \
  V(Int32x4ReplaceLane)                     \
  V(Int32x4Neg)                             \
  V(Int32x4Add)                             \
  V(Int32x4Sub)                             \
  V(Int32x4Mul)                             \
  V(Int32x4Min)                             \
  V(Int32x4Max)                             \
  V(Int32x4ShiftLeftByScalar)               \
  V(Int32x4ShiftRightByScalar)              \
  V(Int32x4Equal)                           \
  V(Int32x4NotEqual)                        \
  V(Int32x4LessThan)                        \
  V(Int32x4LessThanOrEqual)                 \
  V(Int32x4GreaterThan)                     \
  V(Int32x4GreaterThanOrEqual)              \
  V(Int32x4Select)                          \
  V(Int32x4Swizzle)                         \
  V(Int32x4Shuffle)                         \
  V(Int32x4FromFloat32x4)                   \
  V(Uint32x4Min)                            \
  V(Uint32x4Max)                            \
  V(Uint32x4ShiftLeftByScalar)              \
  V(Uint32x4ShiftRightByScalar)             \
  V(Uint32x4LessThan)                       \
  V(Uint32x4LessThanOrEqual)                \
  V(Uint32x4GreaterThan)                    \
  V(Uint32x4GreaterThanOrEqual)             \
  V(Uint32x4FromFloat32x4)                  \
  V(CreateBool32x4)                         \
  V(Bool32x4ReplaceLane)                    \
  V(Bool32x4And)                            \
  V(Bool32x4Or)                             \
  V(Bool32x4Xor)                            \
  V(Bool32x4Not)                            \
  V(Bool32x4Swizzle)                        \
  V(Bool32x4Shuffle)                        \
  V(Bool32x4Equal)                          \
  V(Bool32x4NotEqual)                       \
  V(CreateInt16x8)                          \
  V(Int16x8ReplaceLane)                     \
  V(Int16x8Neg)                             \
  V(Int16x8Add)                             \
  V(Int16x8AddSaturate)                     \
  V(Int16x8Sub)                             \
  V(Int16x8SubSaturate)                     \
  V(Int16x8Mul)                             \
  V(Int16x8Min)                             \
  V(Int16x8Max)                             \
  V(Int16x8ShiftLeftByScalar)               \
  V(Int16x8ShiftRightByScalar)              \
  V(Int16x8Equal)                           \
  V(Int16x8NotEqual)                        \
  V(Int16x8LessThan)                        \
  V(Int16x8LessThanOrEqual)                 \
  V(Int16x8GreaterThan)                     \
  V(Int16x8GreaterThanOrEqual)              \
  V(Int16x8Select)                          \
  V(Int16x8Swizzle)                         \
  V(Int16x8Shuffle)                         \
  V(Uint16x8AddSaturate)                    \
  V(Uint16x8SubSaturate)                    \
  V(Uint16x8Min)                            \
  V(Uint16x8Max)                            \
  V(Uint16x8ShiftLeftByScalar)              \
  V(Uint16x8ShiftRightByScalar)             \
  V(Uint16x8LessThan)                       \
  V(Uint16x8LessThanOrEqual)                \
  V(Uint16x8GreaterThan)                    \
  V(Uint16x8GreaterThanOrEqual)             \
  V(CreateBool16x8)                         \
  V(Bool16x8ReplaceLane)                    \
  V(Bool16x8And)                            \
  V(Bool16x8Or)                             \
  V(Bool16x8Xor)                            \
  V(Bool16x8Not)                            \
  V(Bool16x8Swizzle)                        \
  V(Bool16x8Shuffle)                        \
  V(Bool16x8Equal)                          \
  V(Bool16x8NotEqual)                       \
  V(CreateInt8x16)                          \
  V(Int8x16ReplaceLane)                     \
  V(Int8x16Neg)                             \
  V(Int8x16Add)                             \
  V(Int8x16AddSaturate)                     \
  V(Int8x16Sub)                             \
  V(Int8x16SubSaturate)                     \
  V(Int8x16Mul)                             \
  V(Int8x16Min)                             \
  V(Int8x16Max)                             \
  V(Int8x16ShiftLeftByScalar)               \
  V(Int8x16ShiftRightByScalar)              \
  V(Int8x16Equal)                           \
  V(Int8x16NotEqual)                        \
  V(Int8x16LessThan)                        \
  V(Int8x16LessThanOrEqual)                 \
  V(Int8x16GreaterThan)                     \
  V(Int8x16GreaterThanOrEqual)              \
  V(Int8x16Select)                          \
  V(Int8x16Swizzle)                         \
  V(Int8x16Shuffle)                         \
  V(Uint8x16AddSaturate)                    \
  V(Uint8x16SubSaturate)                    \
  V(Uint8x16Min)                            \
  V(Uint8x16Max)                            \
  V(Uint8x16ShiftLeftByScalar)              \
  V(Uint8x16ShiftRightByScalar)             \
  V(Uint8x16LessThan)                       \
  V(Uint8x16LessThanOrEqual)                \
  V(Uint8x16GreaterThan)                    \
  V(Uint8x16GreaterThanOrEqual)             \
  V(CreateBool8x16)                         \
  V(Bool8x16ReplaceLane)                    \
  V(Bool8x16And)                            \
  V(Bool8x16Or)                             \
  V(Bool8x16Xor)                            \
  V(Bool8x16Not)                            \
  V(Bool8x16Swizzle)                        \
  V(Bool8x16Shuffle)                        \
  V(Bool8x16Equal)                          \
  V(Bool8x16NotEqual)                       \
  V(Simd128Load)                            \
  V(Simd128Load1)                           \
  V(Simd128Load2)                           \
  V(Simd128Load3)                           \
  V(Simd128Store)                           \
  V(Simd128Store1)                          \
  V(Simd128Store2)                          \
  V(Simd128Store3)                          \
  V(Simd128And)                             \
  V(Simd128Or)                              \
  V(Simd128Xor)                             \
  V(Simd128Not)

#define MACHINE_SIMD_RETURN_NUM_OP_LIST(V) \
  V(Float32x4ExtractLane)                  \
  V(Int32x4ExtractLane)                    \
  V(Int16x8ExtractLane)                    \
  V(Int8x16ExtractLane)

#define MACHINE_SIMD_RETURN_BOOL_OP_LIST(V) \
  V(Bool32x4ExtractLane)                    \
  V(Bool32x4AnyTrue)                        \
  V(Bool32x4AllTrue)                        \
  V(Bool16x8ExtractLane)                    \
  V(Bool16x8AnyTrue)                        \
  V(Bool16x8AllTrue)                        \
  V(Bool8x16ExtractLane)                    \
  V(Bool8x16AnyTrue)                        \
  V(Bool8x16AllTrue)

#define MACHINE_SIMD_OP_LIST(V)       \
  MACHINE_SIMD_RETURN_SIMD_OP_LIST(V) \
  MACHINE_SIMD_RETURN_NUM_OP_LIST(V)  \
  MACHINE_SIMD_RETURN_BOOL_OP_LIST(V)

#define VALUE_OP_LIST(V)  \
  COMMON_OP_LIST(V)       \
  SIMPLIFIED_OP_LIST(V)   \
  MACHINE_OP_LIST(V)      \
  MACHINE_SIMD_OP_LIST(V) \
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
    return kInt32Constant <= value && value <= kRelocatableInt64Constant;
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
