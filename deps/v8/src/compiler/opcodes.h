// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_OPCODES_H_
#define V8_COMPILER_OPCODES_H_

// Opcodes for control operators.
#define CONTROL_OP_LIST(V) \
  V(Start)                 \
  V(Dead)                  \
  V(Loop)                  \
  V(End)                   \
  V(Branch)                \
  V(IfTrue)                \
  V(IfFalse)               \
  V(Merge)                 \
  V(Return)                \
  V(Throw)                 \
  V(Continuation)          \
  V(LazyDeoptimization)    \
  V(Deoptimize)

// Opcodes for common operators.
#define LEAF_OP_LIST(V) \
  V(Int32Constant)      \
  V(Int64Constant)      \
  V(Float64Constant)    \
  V(ExternalConstant)   \
  V(NumberConstant)     \
  V(HeapConstant)

#define INNER_OP_LIST(V) \
  V(Phi)                 \
  V(EffectPhi)           \
  V(FrameState)          \
  V(StateValues)         \
  V(Call)                \
  V(Parameter)           \
  V(Projection)

#define COMMON_OP_LIST(V) \
  LEAF_OP_LIST(V)         \
  INNER_OP_LIST(V)

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

#define JS_OTHER_UNOP_LIST(V) V(JSTypeOf)

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
  V(JSCreateGlobalContext)

#define JS_OTHER_OP_LIST(V) \
  V(JSCallConstruct)        \
  V(JSCallFunction)         \
  V(JSCallRuntime)          \
  V(JSYield)                \
  V(JSDebugger)

#define JS_OP_LIST(V)     \
  JS_SIMPLE_BINOP_LIST(V) \
  JS_SIMPLE_UNOP_LIST(V)  \
  JS_OBJECT_OP_LIST(V)    \
  JS_CONTEXT_OP_LIST(V)   \
  JS_OTHER_OP_LIST(V)

// Opcodes for VirtuaMachine-level operators.
#define SIMPLIFIED_OP_LIST(V) \
  V(BooleanNot)               \
  V(NumberEqual)              \
  V(NumberLessThan)           \
  V(NumberLessThanOrEqual)    \
  V(NumberAdd)                \
  V(NumberSubtract)           \
  V(NumberMultiply)           \
  V(NumberDivide)             \
  V(NumberModulus)            \
  V(NumberToInt32)            \
  V(NumberToUint32)           \
  V(ReferenceEqual)           \
  V(StringEqual)              \
  V(StringLessThan)           \
  V(StringLessThanOrEqual)    \
  V(StringAdd)                \
  V(ChangeTaggedToInt32)      \
  V(ChangeTaggedToUint32)     \
  V(ChangeTaggedToFloat64)    \
  V(ChangeInt32ToTagged)      \
  V(ChangeUint32ToTagged)     \
  V(ChangeFloat64ToTagged)    \
  V(ChangeBoolToBit)          \
  V(ChangeBitToBool)          \
  V(LoadField)                \
  V(LoadElement)              \
  V(StoreField)               \
  V(StoreElement)

// Opcodes for Machine-level operators.
#define MACHINE_OP_LIST(V) \
  V(Load)                  \
  V(Store)                 \
  V(Word32And)             \
  V(Word32Or)              \
  V(Word32Xor)             \
  V(Word32Shl)             \
  V(Word32Shr)             \
  V(Word32Sar)             \
  V(Word32Equal)           \
  V(Word64And)             \
  V(Word64Or)              \
  V(Word64Xor)             \
  V(Word64Shl)             \
  V(Word64Shr)             \
  V(Word64Sar)             \
  V(Word64Equal)           \
  V(Int32Add)              \
  V(Int32AddWithOverflow)  \
  V(Int32Sub)              \
  V(Int32SubWithOverflow)  \
  V(Int32Mul)              \
  V(Int32Div)              \
  V(Int32UDiv)             \
  V(Int32Mod)              \
  V(Int32UMod)             \
  V(Int32LessThan)         \
  V(Int32LessThanOrEqual)  \
  V(Uint32LessThan)        \
  V(Uint32LessThanOrEqual) \
  V(Int64Add)              \
  V(Int64Sub)              \
  V(Int64Mul)              \
  V(Int64Div)              \
  V(Int64UDiv)             \
  V(Int64Mod)              \
  V(Int64UMod)             \
  V(Int64LessThan)         \
  V(Int64LessThanOrEqual)  \
  V(ConvertInt64ToInt32)   \
  V(ConvertInt32ToInt64)   \
  V(ChangeInt32ToFloat64)  \
  V(ChangeUint32ToFloat64) \
  V(ChangeFloat64ToInt32)  \
  V(ChangeFloat64ToUint32) \
  V(Float64Add)            \
  V(Float64Sub)            \
  V(Float64Mul)            \
  V(Float64Div)            \
  V(Float64Mod)            \
  V(Float64Equal)          \
  V(Float64LessThan)       \
  V(Float64LessThanOrEqual)

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
  static const char* Mnemonic(Value val) {
    switch (val) {
#define RETURN_NAME(x) \
  case k##x:           \
    return #x;
      ALL_OP_LIST(RETURN_NAME)
#undef RETURN_NAME
      default:
        return "UnknownOpcode";
    }
  }

  static bool IsJsOpcode(Value val) {
    switch (val) {
#define RETURN_NAME(x) \
  case k##x:           \
    return true;
      JS_OP_LIST(RETURN_NAME)
#undef RETURN_NAME
      default:
        return false;
    }
  }

  static bool IsControlOpcode(Value val) {
    switch (val) {
#define RETURN_NAME(x) \
  case k##x:           \
    return true;
      CONTROL_OP_LIST(RETURN_NAME)
#undef RETURN_NAME
      default:
        return false;
    }
  }

  static bool IsCommonOpcode(Value val) {
    switch (val) {
#define RETURN_NAME(x) \
  case k##x:           \
    return true;
      CONTROL_OP_LIST(RETURN_NAME)
      COMMON_OP_LIST(RETURN_NAME)
#undef RETURN_NAME
      default:
        return false;
    }
  }
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_OPCODES_H_
