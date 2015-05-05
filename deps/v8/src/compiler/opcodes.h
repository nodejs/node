// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_OPCODES_H_
#define V8_COMPILER_OPCODES_H_

// Opcodes for control operators.
#define INNER_CONTROL_OP_LIST(V) \
  V(Dead)                        \
  V(Loop)                        \
  V(Branch)                      \
  V(Switch)                      \
  V(IfTrue)                      \
  V(IfFalse)                     \
  V(IfSuccess)                   \
  V(IfException)                 \
  V(IfValue)                     \
  V(IfDefault)                   \
  V(Merge)                       \
  V(Deoptimize)                  \
  V(Return)                      \
  V(OsrNormalEntry)              \
  V(OsrLoopEntry)                \
  V(Throw)

#define CONTROL_OP_LIST(V) \
  INNER_CONTROL_OP_LIST(V) \
  V(Start)                 \
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
  V(ValueEffect)         \
  V(Finish)              \
  V(FrameState)          \
  V(StateValues)         \
  V(TypedStateValues)    \
  V(Call)                \
  V(Parameter)           \
  V(OsrValue)            \
  V(Projection)

#define COMMON_OP_LIST(V) \
  CONSTANT_OP_LIST(V)     \
  INNER_OP_LIST(V)        \
  V(Always)

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

#define JS_LOGIC_UNOP_LIST(V) V(JSUnaryNot)

#define JS_CONVERSION_UNOP_LIST(V) \
  V(JSToBoolean)                   \
  V(JSToNumber)                    \
  V(JSToString)                    \
  V(JSToName)                      \
  V(JSToObject)

#define JS_OTHER_UNOP_LIST(V) \
  V(JSTypeOf)

#define JS_SIMPLE_UNOP_LIST(V) \
  JS_LOGIC_UNOP_LIST(V)        \
  JS_CONVERSION_UNOP_LIST(V)   \
  JS_OTHER_UNOP_LIST(V)

#define JS_OBJECT_OP_LIST(V) \
  V(JSCreate)                \
  V(JSLoadProperty)          \
  V(JSLoadNamed)             \
  V(JSStoreProperty)         \
  V(JSStoreNamed)            \
  V(JSDeleteProperty)        \
  V(JSHasProperty)           \
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
  V(NumberToInt32)                 \
  V(NumberToUint32)                \
  V(PlainPrimitiveToNumber)        \
  V(StringAdd)                     \
  V(ChangeTaggedToInt32)           \
  V(ChangeTaggedToUint32)          \
  V(ChangeTaggedToFloat64)         \
  V(ChangeInt32ToTagged)           \
  V(ChangeUint32ToTagged)          \
  V(ChangeFloat64ToTagged)         \
  V(ChangeBoolToBit)               \
  V(ChangeBitToBool)               \
  V(LoadField)                     \
  V(LoadBuffer)                    \
  V(LoadElement)                   \
  V(StoreField)                    \
  V(StoreBuffer)                   \
  V(StoreElement)                  \
  V(ObjectIsSmi)                   \
  V(ObjectIsNonNegativeSmi)

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
  V(Word64And)                  \
  V(Word64Or)                   \
  V(Word64Xor)                  \
  V(Word64Shl)                  \
  V(Word64Shr)                  \
  V(Word64Sar)                  \
  V(Word64Ror)                  \
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
  V(Int64Sub)                   \
  V(Int64Mul)                   \
  V(Int64Div)                   \
  V(Int64Mod)                   \
  V(Uint64Div)                  \
  V(Uint64Mod)                  \
  V(ChangeFloat32ToFloat64)     \
  V(ChangeFloat64ToInt32)       \
  V(ChangeFloat64ToUint32)      \
  V(ChangeInt32ToFloat64)       \
  V(ChangeInt32ToInt64)         \
  V(ChangeUint32ToFloat64)      \
  V(ChangeUint32ToUint64)       \
  V(TruncateFloat64ToFloat32)   \
  V(TruncateFloat64ToInt32)     \
  V(TruncateInt64ToInt32)       \
  V(Float64Add)                 \
  V(Float64Sub)                 \
  V(Float64Mul)                 \
  V(Float64Div)                 \
  V(Float64Mod)                 \
  V(Float64Max)                 \
  V(Float64Min)                 \
  V(Float64Sqrt)                \
  V(Float64RoundDown)           \
  V(Float64RoundTruncate)       \
  V(Float64RoundTiesAway)       \
  V(Float64ExtractLowWord32)    \
  V(Float64ExtractHighWord32)   \
  V(Float64InsertLowWord32)     \
  V(Float64InsertHighWord32)    \
  V(LoadStackPointer)           \
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
    return kDead <= value && value <= kAlways;
  }

  // Returns true if opcode for control operator.
  static bool IsControlOpcode(Value value) {
    return kDead <= value && value <= kEnd;
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

  // Returns true if opcode for comparison operator.
  static bool IsComparisonOpcode(Value value) {
    return (kJSEqual <= value && value <= kJSGreaterThanOrEqual) ||
           (kNumberEqual <= value && value <= kStringLessThanOrEqual) ||
           (kWord32Equal <= value && value <= kFloat64LessThanOrEqual);
  }
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_OPCODES_H_
