// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_IR_H_
#define V8_MAGLEV_MAGLEV_IR_H_

#include <optional>
#include <type_traits>

#include "src/base/bit-field.h"
#include "src/base/bits.h"
#include "src/base/bounds.h"
#include "src/base/discriminated-union.h"
#include "src/base/enum-set.h"
#include "src/base/logging.h"
#include "src/base/macros.h"
#include "src/base/small-vector.h"
#include "src/base/threaded-list.h"
#include "src/codegen/external-reference.h"
#include "src/codegen/label.h"
#include "src/codegen/machine-type.h"
#include "src/codegen/reglist.h"
#include "src/codegen/source-position.h"
#include "src/common/globals.h"
#include "src/common/operation.h"
#include "src/compiler/access-info.h"
#include "src/compiler/backend/instruction.h"
#include "src/compiler/compilation-dependencies.h"
#include "src/compiler/feedback-source.h"
#include "src/compiler/heap-refs.h"
// TODO(dmercadier): move the Turboshaft utils functions to shared code (in
// particular, any_of, which is the reason we're including this Turboshaft
// header)
#include "src/base/functional/function-ref.h"
#include "src/compiler/js-heap-broker.h"
#include "src/compiler/turboshaft/snapshot-table.h"
#include "src/compiler/turboshaft/utils.h"
#include "src/deoptimizer/deoptimize-reason.h"
#include "src/interpreter/bytecode-flags-and-tokens.h"
#include "src/interpreter/bytecode-register.h"
#include "src/maglev/maglev-compilation-unit.h"
#include "src/maglev/maglev-regalloc-node-info.h"
#include "src/objects/arguments.h"
#include "src/objects/heap-number.h"
#include "src/objects/property-details.h"
#include "src/objects/smi.h"
#include "src/objects/tagged-index.h"
#include "src/roots/roots.h"
#include "src/sandbox/js-dispatch-table.h"
#include "src/utils/utils.h"
#include "src/zone/zone.h"

#ifdef V8_ENABLE_EXPERIMENTAL_UNDEFINED_DOUBLE
#define IF_UD(Macro, ...) Macro(__VA_ARGS__)
#define IF_NOT_UD(Macro, ...)
#else
#define IF_UD(Macro, ...)
#define IF_NOT_UD(Macro, ...) Macro(__VA_ARGS__)
#endif  // V8_ENABLE_EXPERIMENTAL_UNDEFINED_DOUBLE

namespace v8 {
namespace internal {

enum Condition : int;

namespace maglev {

class BasicBlock;
class ProcessingState;
class MaglevAssembler;
class MaglevCodeGenState;
class MaglevCompilationUnit;
class MaglevVregAllocationState;
class CompactInterpreterFrameState;
class MergePointInterpreterFrameState;
class ExceptionHandlerInfo;

// Nodes are either
// 1. side-effecting or value-holding SSA nodes in the body of basic blocks, or
// 2. Control nodes that store the control flow at the end of basic blocks, and
// form a separate node hierarchy to non-control nodes.
//
// The macro lists below must match the node class hierarchy.

#define GENERIC_OPERATIONS_NODE_LIST(V) \
  V(GenericAdd)                         \
  V(GenericSubtract)                    \
  V(GenericMultiply)                    \
  V(GenericDivide)                      \
  V(GenericModulus)                     \
  V(GenericExponentiate)                \
  V(GenericBitwiseAnd)                  \
  V(GenericBitwiseOr)                   \
  V(GenericBitwiseXor)                  \
  V(GenericShiftLeft)                   \
  V(GenericShiftRight)                  \
  V(GenericShiftRightLogical)           \
  V(GenericBitwiseNot)                  \
  V(GenericNegate)                      \
  V(GenericIncrement)                   \
  V(GenericDecrement)                   \
  V(GenericEqual)                       \
  V(GenericStrictEqual)                 \
  V(GenericLessThan)                    \
  V(GenericLessThanOrEqual)             \
  V(GenericGreaterThan)                 \
  V(GenericGreaterThanOrEqual)

#define INT32_OPERATIONS_NODE_LIST(V) \
  V(Int32AbsWithOverflow)             \
  V(Int32Add)                         \
  V(Int32Subtract)                    \
  V(Int32Multiply)                    \
  V(Int32MultiplyOverflownBits)       \
  V(Int32Divide)                      \
  V(Int32AddWithOverflow)             \
  V(Int32SubtractWithOverflow)        \
  V(Int32MultiplyWithOverflow)        \
  V(Int32DivideWithOverflow)          \
  V(Int32ModulusWithOverflow)         \
  V(Int32BitwiseAnd)                  \
  V(Int32BitwiseOr)                   \
  V(Int32BitwiseXor)                  \
  V(Int32ShiftLeft)                   \
  V(Int32ShiftRight)                  \
  V(Int32ShiftRightLogical)           \
  V(Int32BitwiseNot)                  \
  V(Int32NegateWithOverflow)          \
  V(Int32IncrementWithOverflow)       \
  V(Int32DecrementWithOverflow)       \
  V(Int32Compare)                     \
  V(Int32ToBoolean)

#define FLOAT64_OPERATIONS_NODE_LIST(V) \
  V(Float64Abs)                         \
  V(Float64Add)                         \
  V(Float64Subtract)                    \
  V(Float64Multiply)                    \
  V(Float64Divide)                      \
  V(Float64Exponentiate)                \
  V(Float64Modulus)                     \
  V(Float64Negate)                      \
  V(Float64Round)                       \
  V(Float64Compare)                     \
  V(Float64ToBoolean)                   \
  V(Float64Ieee754Unary)                \
  V(Float64Ieee754Binary)               \
  V(Float64Sqrt)

#define SMI_OPERATIONS_NODE_LIST(V) \
  V(CheckedSmiIncrement)            \
  V(CheckedSmiDecrement)

#define CONSTANT_VALUE_NODE_LIST(V) \
  V(Constant)                       \
  V(Float64Constant)                \
  V(Int32Constant)                  \
  V(Uint32Constant)                 \
  V(IntPtrConstant)                 \
  V(RootConstant)                   \
  V(SmiConstant)                    \
  V(TaggedIndexConstant)            \
  V(TrustedConstant)

#define INLINE_BUILTIN_NODE_LIST(V)              \
  V(BuiltinStringFromCharCode)                   \
  V(BuiltinStringPrototypeCharCodeOrCodePointAt) \
  V(BuiltinSeqOneByteStringCharCodeAt)

#define TURBOLEV_VALUE_NODE_LIST(V) \
  V(CreateFastArrayElements)        \
  V(NewConsString)                  \
  V(MapPrototypeGet)                \
  V(MapPrototypeGetInt32Key)        \
  V(SetPrototypeHas)

#define TURBOLEV_NON_VALUE_NODE_LIST(V) V(TransitionAndStoreArrayElement)

#define VALUE_NODE_LIST(V)                                            \
  V(Identity)                                                         \
  V(AllocationBlock)                                                  \
  V(ArgumentsElements)                                                \
  V(ArgumentsLength)                                                  \
  V(RestLength)                                                       \
  V(Call)                                                             \
  V(CallBuiltin)                                                      \
  V(CallCPPBuiltin)                                                   \
  V(CallForwardVarargs)                                               \
  V(CallRuntime)                                                      \
  V(CallWithArrayLike)                                                \
  V(CallWithSpread)                                                   \
  V(CallKnownApiFunction)                                             \
  V(CallKnownJSFunction)                                              \
  V(CallSelf)                                                         \
  V(Construct)                                                        \
  V(CheckConstructResult)                                             \
  V(CheckDerivedConstructResult)                                      \
  V(ConstructWithSpread)                                              \
  V(ConvertReceiver)                                                  \
  V(ConvertHoleToUndefined)                                           \
  V(CreateArrayLiteral)                                               \
  V(CreateShallowArrayLiteral)                                        \
  V(CreateObjectLiteral)                                              \
  V(CreateShallowObjectLiteral)                                       \
  V(CreateFunctionContext)                                            \
  V(CreateClosure)                                                    \
  V(FastCreateClosure)                                                \
  V(CreateRegExpLiteral)                                              \
  V(DeleteProperty)                                                   \
  V(EnsureWritableFastElements)                                       \
  V(ExtendPropertiesBackingStore)                                     \
  V(InlinedAllocation)                                                \
  V(ForInPrepare)                                                     \
  V(ForInNext)                                                        \
  V(GeneratorRestoreRegister)                                         \
  V(GetIterator)                                                      \
  V(GetSecondReturnedValue)                                           \
  V(GetTemplateObject)                                                \
  V(HasInPrototypeChain)                                              \
  V(InitialValue)                                                     \
  V(LoadTaggedField)                                                  \
  V(LoadTaggedFieldForProperty)                                       \
  V(LoadTaggedFieldForContextSlotNoCells)                             \
  V(LoadTaggedFieldForContextSlot)                                    \
  V(LoadDoubleField)                                                  \
  V(LoadFloat64)                                                      \
  V(LoadInt32)                                                        \
  V(LoadTaggedFieldByFieldIndex)                                      \
  V(LoadFixedArrayElement)                                            \
  V(LoadFixedDoubleArrayElement)                                      \
  V(LoadHoleyFixedDoubleArrayElement)                                 \
  V(LoadHoleyFixedDoubleArrayElementCheckedNotHole)                   \
  IF_UD(V, LoadHoleyFixedDoubleArrayElementCheckedNotUndefinedOrHole) \
  V(LoadSignedIntDataViewElement)                                     \
  V(LoadDoubleDataViewElement)                                        \
  V(LoadTypedArrayLength)                                             \
  V(LoadSignedIntTypedArrayElement)                                   \
  V(LoadUnsignedIntTypedArrayElement)                                 \
  V(LoadDoubleTypedArrayElement)                                      \
  V(LoadSignedIntConstantTypedArrayElement)                           \
  V(LoadUnsignedIntConstantTypedArrayElement)                         \
  V(LoadDoubleConstantTypedArrayElement)                              \
  V(LoadEnumCacheLength)                                              \
  V(LoadGlobal)                                                       \
  V(LoadNamedGeneric)                                                 \
  V(LoadNamedFromSuperGeneric)                                        \
  V(MaybeGrowFastElements)                                            \
  V(MigrateMapIfNeeded)                                               \
  V(SetNamedGeneric)                                                  \
  V(DefineNamedOwnGeneric)                                            \
  V(StoreInArrayLiteralGeneric)                                       \
  V(StoreGlobal)                                                      \
  V(GetKeyedGeneric)                                                  \
  V(SetKeyedGeneric)                                                  \
  V(DefineKeyedOwnGeneric)                                            \
  V(Phi)                                                              \
  V(RegisterInput)                                                    \
  V(CheckedSmiSizedInt32)                                             \
  V(CheckedSmiTagInt32)                                               \
  V(CheckedSmiTagUint32)                                              \
  V(CheckedSmiTagIntPtr)                                              \
  V(UnsafeSmiTagInt32)                                                \
  V(UnsafeSmiTagUint32)                                               \
  V(UnsafeSmiTagIntPtr)                                               \
  V(CheckedSmiUntag)                                                  \
  V(UnsafeSmiUntag)                                                   \
  V(CheckedInternalizedString)                                        \
  V(CheckedObjectToIndex)                                             \
  V(TruncateCheckedNumberOrOddballToInt32)                            \
  V(TruncateUnsafeNumberOrOddballToInt32)                             \
  V(CheckedInt32ToUint32)                                             \
  V(CheckedIntPtrToUint32)                                            \
  V(UnsafeInt32ToUint32)                                              \
  V(CheckedUint32ToInt32)                                             \
  V(CheckedIntPtrToInt32)                                             \
  V(ChangeInt32ToFloat64)                                             \
  V(ChangeUint32ToFloat64)                                            \
  V(ChangeIntPtrToFloat64)                                            \
  V(CheckedHoleyFloat64ToInt32)                                       \
  V(UnsafeHoleyFloat64ToInt32)                                        \
  V(CheckedHoleyFloat64ToUint32)                                      \
  V(TruncateHoleyFloat64ToInt32)                                      \
  V(TruncateUint32ToInt32)                                            \
  V(UnsafeUint32ToInt32)                                              \
  V(Int32ToUint8Clamped)                                              \
  V(Uint32ToUint8Clamped)                                             \
  V(Float64ToUint8Clamped)                                            \
  V(CheckedNumberToUint8Clamped)                                      \
  V(Int32ToNumber)                                                    \
  V(Uint32ToNumber)                                                   \
  V(Int32CountLeadingZeros)                                           \
  V(TaggedCountLeadingZeros)                                          \
  V(Float64CountLeadingZeros)                                         \
  V(IntPtrToBoolean)                                                  \
  V(IntPtrToNumber)                                                   \
  V(Float64ToTagged)                                                  \
  V(Float64ToHeapNumberForField)                                      \
  V(HoleyFloat64ToTagged)                                             \
  V(CheckedSmiTagFloat64)                                             \
  V(CheckedNumberToInt32)                                             \
  V(CheckedNumberOrOddballToFloat64)                                  \
  V(UncheckedNumberOrOddballToFloat64)                                \
  V(CheckedNumberOrOddballToHoleyFloat64)                             \
  V(CheckedHoleyFloat64ToFloat64)                                     \
  V(HoleyFloat64ToMaybeNanFloat64)                                    \
  IF_UD(V, Float64ToHoleyFloat64)                                     \
  IF_UD(V, ConvertHoleNanToUndefinedNan)                              \
  IF_UD(V, HoleyFloat64IsUndefinedOrHole)                             \
  IF_NOT_UD(V, HoleyFloat64IsHole)                                    \
  V(LogicalNot)                                                       \
  V(SetPendingMessage)                                                \
  V(StringAt)                                                         \
  V(StringEqual)                                                      \
  V(StringLength)                                                     \
  V(StringConcat)                                                     \
  V(SeqOneByteStringAt)                                               \
  V(ConsStringMap)                                                    \
  V(UnwrapStringWrapper)                                              \
  V(ToBoolean)                                                        \
  V(ToBooleanLogicalNot)                                              \
  V(AllocateElementsArray)                                            \
  V(TaggedEqual)                                                      \
  V(TaggedNotEqual)                                                   \
  V(TestInstanceOf)                                                   \
  V(TestUndetectable)                                                 \
  V(TestTypeOf)                                                       \
  V(ToName)                                                           \
  V(ToNumberOrNumeric)                                                \
  V(ToObject)                                                         \
  V(ToString)                                                         \
  V(TransitionElementsKind)                                           \
  V(NumberToString)                                                   \
  V(UpdateJSArrayLength)                                              \
  V(VirtualObject)                                                    \
  V(GetContinuationPreservedEmbedderData)                             \
  V(ReturnedValue)                                                    \
  CONSTANT_VALUE_NODE_LIST(V)                                         \
  INT32_OPERATIONS_NODE_LIST(V)                                       \
  FLOAT64_OPERATIONS_NODE_LIST(V)                                     \
  SMI_OPERATIONS_NODE_LIST(V)                                         \
  GENERIC_OPERATIONS_NODE_LIST(V)                                     \
  INLINE_BUILTIN_NODE_LIST(V)                                         \
  TURBOLEV_VALUE_NODE_LIST(V)

#define GAP_MOVE_NODE_LIST(V) \
  V(ConstantGapMove)          \
  V(GapMove)

#define NON_VALUE_NODE_LIST(V)                \
  V(AssertInt32)                              \
  V(CheckDynamicValue)                        \
  V(CheckInt32IsSmi)                          \
  V(CheckUint32IsSmi)                         \
  V(CheckIntPtrIsSmi)                         \
  V(CheckHoleyFloat64IsSmi)                   \
  V(CheckHeapObject)                          \
  V(CheckInt32Condition)                      \
  V(CheckCacheIndicesNotCleared)              \
  V(CheckJSDataViewBounds)                    \
  V(CheckTypedArrayBounds)                    \
  V(CheckTypedArrayNotDetached)               \
  V(CheckMaps)                                \
  V(CheckMapsWithMigrationAndDeopt)           \
  V(CheckMapsWithMigration)                   \
  V(CheckMapsWithAlreadyLoadedMap)            \
  V(CheckDetectableCallable)                  \
  V(CheckJSReceiverOrNullOrUndefined)         \
  V(CheckNotHole)                             \
  V(CheckHoleyFloat64NotHole)                 \
  V(CheckNumber)                              \
  V(CheckSmi)                                 \
  V(CheckString)                              \
  V(CheckSeqOneByteString)                    \
  V(CheckStringOrStringWrapper)               \
  V(CheckStringOrOddball)                     \
  V(CheckSymbol)                              \
  V(CheckValue)                               \
  V(CheckValueEqualsInt32)                    \
  V(CheckFloat64SameValue)                    \
  V(CheckValueEqualsString)                   \
  V(CheckInstanceType)                        \
  V(Dead)                                     \
  V(DebugBreak)                               \
  V(FunctionEntryStackCheck)                  \
  V(GeneratorStore)                           \
  V(TryOnStackReplacement)                    \
  V(StoreMap)                                 \
  V(StoreDoubleField)                         \
  V(StoreFixedArrayElementWithWriteBarrier)   \
  V(StoreFixedArrayElementNoWriteBarrier)     \
  V(StoreFixedDoubleArrayElement)             \
  V(StoreInt32)                               \
  V(StoreFloat64)                             \
  V(StoreIntTypedArrayElement)                \
  V(StoreDoubleTypedArrayElement)             \
  V(StoreIntConstantTypedArrayElement)        \
  V(StoreDoubleConstantTypedArrayElement)     \
  V(StoreSignedIntDataViewElement)            \
  V(StoreDoubleDataViewElement)               \
  V(StoreTaggedFieldNoWriteBarrier)           \
  V(StoreTaggedFieldWithWriteBarrier)         \
  V(StoreContextSlotWithWriteBarrier)         \
  V(StoreTrustedPointerFieldWithWriteBarrier) \
  V(HandleNoHeapWritesInterrupt)              \
  V(ReduceInterruptBudgetForLoop)             \
  V(ReduceInterruptBudgetForReturn)           \
  V(ThrowReferenceErrorIfHole)                \
  V(ThrowSuperNotCalledIfHole)                \
  V(ThrowSuperAlreadyCalledIfNotHole)         \
  V(ThrowIfNotCallable)                       \
  V(ThrowIfNotSuperConstructor)               \
  V(TransitionElementsKindOrCheckMap)         \
  V(SetContinuationPreservedEmbedderData)     \
  GAP_MOVE_NODE_LIST(V)                       \
  TURBOLEV_NON_VALUE_NODE_LIST(V)

#define NODE_LIST(V)     \
  NON_VALUE_NODE_LIST(V) \
  VALUE_NODE_LIST(V)

#define BRANCH_CONTROL_NODE_LIST(V)          \
  V(BranchIfSmi)                             \
  V(BranchIfRootConstant)                    \
  V(BranchIfToBooleanTrue)                   \
  V(BranchIfInt32ToBooleanTrue)              \
  V(BranchIfIntPtrToBooleanTrue)             \
  V(BranchIfFloat64ToBooleanTrue)            \
  V(BranchIfFloat64IsHole)                   \
  IF_UD(V, BranchIfFloat64IsUndefinedOrHole) \
  V(BranchIfReferenceEqual)                  \
  V(BranchIfInt32Compare)                    \
  V(BranchIfUint32Compare)                   \
  V(BranchIfFloat64Compare)                  \
  V(BranchIfUndefinedOrNull)                 \
  V(BranchIfUndetectable)                    \
  V(BranchIfJSReceiver)                      \
  V(BranchIfTypeOf)

#define CONDITIONAL_CONTROL_NODE_LIST(V) \
  V(Switch)                              \
  BRANCH_CONTROL_NODE_LIST(V)

#define UNCONDITIONAL_CONTROL_NODE_LIST(V) \
  V(Jump)                                  \
  V(CheckpointedJump)                      \
  V(JumpLoop)

#define TERMINAL_CONTROL_NODE_LIST(V) \
  V(Abort)                            \
  V(Return)                           \
  V(Deopt)

#define CONTROL_NODE_LIST(V)       \
  TERMINAL_CONTROL_NODE_LIST(V)    \
  CONDITIONAL_CONTROL_NODE_LIST(V) \
  UNCONDITIONAL_CONTROL_NODE_LIST(V)

#define NODE_BASE_LIST(V) \
  NODE_LIST(V)            \
  CONTROL_NODE_LIST(V)

// Define the opcode enum.
#define DEF_OPCODES(type) k##type,
enum class Opcode : uint16_t { NODE_BASE_LIST(DEF_OPCODES) };
#undef DEF_OPCODES
#define PLUS_ONE(type) +1
static constexpr int kOpcodeCount = NODE_BASE_LIST(PLUS_ONE);
static constexpr Opcode kFirstOpcode = static_cast<Opcode>(0);
static constexpr Opcode kLastOpcode = static_cast<Opcode>(kOpcodeCount - 1);
#undef PLUS_ONE

const char* OpcodeToString(Opcode opcode);
inline std::ostream& operator<<(std::ostream& os, Opcode opcode) {
  return os << OpcodeToString(opcode);
}

#define V(Name) Opcode::k##Name,
static constexpr Opcode kFirstValueNodeOpcode =
    std::min({VALUE_NODE_LIST(V) kLastOpcode});
static constexpr Opcode kLastValueNodeOpcode =
    std::max({VALUE_NODE_LIST(V) kFirstOpcode});
static constexpr Opcode kFirstConstantNodeOpcode =
    std::min({CONSTANT_VALUE_NODE_LIST(V) kLastOpcode});
static constexpr Opcode kLastConstantNodeOpcode =
    std::max({CONSTANT_VALUE_NODE_LIST(V) kFirstOpcode});
static constexpr Opcode kFirstGapMoveNodeOpcode =
    std::min({GAP_MOVE_NODE_LIST(V) kLastOpcode});
static constexpr Opcode kLastGapMoveNodeOpcode =
    std::max({GAP_MOVE_NODE_LIST(V) kFirstOpcode});

static constexpr Opcode kFirstNodeOpcode = std::min({NODE_LIST(V) kLastOpcode});
static constexpr Opcode kLastNodeOpcode = std::max({NODE_LIST(V) kFirstOpcode});

static constexpr Opcode kFirstBranchControlNodeOpcode =
    std::min({BRANCH_CONTROL_NODE_LIST(V) kLastOpcode});
static constexpr Opcode kLastBranchControlNodeOpcode =
    std::max({BRANCH_CONTROL_NODE_LIST(V) kFirstOpcode});

static constexpr Opcode kFirstConditionalControlNodeOpcode =
    std::min({CONDITIONAL_CONTROL_NODE_LIST(V) kLastOpcode});
static constexpr Opcode kLastConditionalControlNodeOpcode =
    std::max({CONDITIONAL_CONTROL_NODE_LIST(V) kFirstOpcode});

static constexpr Opcode kLastUnconditionalControlNodeOpcode =
    std::max({UNCONDITIONAL_CONTROL_NODE_LIST(V) kFirstOpcode});
static constexpr Opcode kFirstUnconditionalControlNodeOpcode =
    std::min({UNCONDITIONAL_CONTROL_NODE_LIST(V) kLastOpcode});

static constexpr Opcode kLastTerminalControlNodeOpcode =
    std::max({TERMINAL_CONTROL_NODE_LIST(V) kFirstOpcode});
static constexpr Opcode kFirstTerminalControlNodeOpcode =
    std::min({TERMINAL_CONTROL_NODE_LIST(V) kLastOpcode});

static constexpr Opcode kFirstControlNodeOpcode =
    std::min({CONTROL_NODE_LIST(V) kLastOpcode});
static constexpr Opcode kLastControlNodeOpcode =
    std::max({CONTROL_NODE_LIST(V) kFirstOpcode});
#undef V

constexpr bool IsValueNode(Opcode opcode) {
  return kFirstValueNodeOpcode <= opcode && opcode <= kLastValueNodeOpcode;
}
constexpr bool IsConstantNode(Opcode opcode) {
  return kFirstConstantNodeOpcode <= opcode &&
         opcode <= kLastConstantNodeOpcode;
}
constexpr bool IsCommutativeNode(Opcode opcode) {
  switch (opcode) {
    case Opcode::kFloat64Add:
    case Opcode::kFloat64Multiply:
    case Opcode::kGenericStrictEqual:
    case Opcode::kInt32Add:
    case Opcode::kInt32AddWithOverflow:
    case Opcode::kInt32BitwiseAnd:
    case Opcode::kInt32BitwiseOr:
    case Opcode::kInt32BitwiseXor:
    case Opcode::kInt32Multiply:
    case Opcode::kInt32MultiplyOverflownBits:
    case Opcode::kInt32MultiplyWithOverflow:
    case Opcode::kStringEqual:
    case Opcode::kTaggedEqual:
    case Opcode::kTaggedNotEqual:
      return true;
    default:
      return false;
  }
}
constexpr bool IsZeroCostNode(Opcode opcode) {
  switch (opcode) {
    case Opcode::kTruncateUint32ToInt32:
    case Opcode::kUnsafeUint32ToInt32:
    case Opcode::kIdentity:
      return true;
    default:
      return false;
  }
}
constexpr bool IsGapMoveNode(Opcode opcode) {
  return kFirstGapMoveNodeOpcode <= opcode && opcode <= kLastGapMoveNodeOpcode;
}
constexpr bool IsControlNode(Opcode opcode) {
  return kFirstControlNodeOpcode <= opcode && opcode <= kLastControlNodeOpcode;
}
constexpr bool IsBranchControlNode(Opcode opcode) {
  return kFirstBranchControlNodeOpcode <= opcode &&
         opcode <= kLastBranchControlNodeOpcode;
}
constexpr bool IsConditionalControlNode(Opcode opcode) {
  return kFirstConditionalControlNodeOpcode <= opcode &&
         opcode <= kLastConditionalControlNodeOpcode;
}
constexpr bool IsUnconditionalControlNode(Opcode opcode) {
  return kFirstUnconditionalControlNodeOpcode <= opcode &&
         opcode <= kLastUnconditionalControlNodeOpcode;
}
constexpr bool IsTerminalControlNode(Opcode opcode) {
  return kFirstTerminalControlNodeOpcode <= opcode &&
         opcode <= kLastTerminalControlNodeOpcode;
}
// Simple field stores are stores which do nothing but change a field value
// (i.e. no map transitions or calls into user code).
constexpr bool IsSimpleFieldStore(Opcode opcode) {
  return opcode == Opcode::kStoreTaggedFieldWithWriteBarrier ||
         opcode == Opcode::kStoreTaggedFieldNoWriteBarrier ||
         opcode == Opcode::kStoreDoubleField ||
         opcode == Opcode::kStoreFloat64 || opcode == Opcode::kStoreInt32 ||
         opcode == Opcode::kUpdateJSArrayLength ||
         opcode == Opcode::kStoreFixedArrayElementWithWriteBarrier ||
         opcode == Opcode::kStoreFixedArrayElementNoWriteBarrier ||
         opcode == Opcode::kStoreFixedDoubleArrayElement ||
         opcode == Opcode::kStoreTrustedPointerFieldWithWriteBarrier;
}
constexpr bool IsElementsArrayWrite(Opcode opcode) {
  return opcode == Opcode::kMaybeGrowFastElements ||
         opcode == Opcode::kEnsureWritableFastElements;
}
constexpr bool IsTypedArrayStore(Opcode opcode) {
  return opcode == Opcode::kStoreIntTypedArrayElement ||
         opcode == Opcode::kStoreDoubleTypedArrayElement;
}

constexpr bool CanTriggerTruncationPass(Opcode opcode) {
  switch (opcode) {
    case Opcode::kTruncateHoleyFloat64ToInt32:
    case Opcode::kCheckedHoleyFloat64ToInt32:
    case Opcode::kUnsafeHoleyFloat64ToInt32:
      return true;
    default:
      return false;
  }
}

constexpr bool CanBeStoreToNonEscapedObject(Opcode opcode) {
  switch (opcode) {
    case Opcode::kStoreMap:
    case Opcode::kStoreInt32:
    case Opcode::kStoreTrustedPointerFieldWithWriteBarrier:
    case Opcode::kStoreTaggedFieldWithWriteBarrier:
    case Opcode::kStoreTaggedFieldNoWriteBarrier:
    case Opcode::kStoreContextSlotWithWriteBarrier:
    case Opcode::kStoreFloat64:
      return true;
    default:
      return false;
  }
}

constexpr bool HasRangeType(Opcode opcode) {
  switch (opcode) {
    case Opcode::kFloat64Add:
    case Opcode::kFloat64Subtract:
    case Opcode::kFloat64Multiply:
    case Opcode::kFloat64Divide:
      return true;
    default:
      return false;
  }
}

// MAP_OPERATION_TO_NODES are tuples with the following format:
// - Operation name,
// - Int32 operation node,
// - Identity of int32 operation (e.g, 0 for add/sub and 1 for mul/div), if it
//   exists, or otherwise {}.
#define MAP_BINARY_OPERATION_TO_INT32_NODE(V) \
  V(Add, Int32AddWithOverflow, 0)             \
  V(Subtract, Int32SubtractWithOverflow, 0)   \
  V(Multiply, Int32MultiplyWithOverflow, 1)   \
  V(Divide, Int32DivideWithOverflow, 1)       \
  V(Modulus, Int32ModulusWithOverflow, {})    \
  V(BitwiseAnd, Int32BitwiseAnd, ~0)          \
  V(BitwiseOr, Int32BitwiseOr, 0)             \
  V(BitwiseXor, Int32BitwiseXor, 0)           \
  V(ShiftLeft, Int32ShiftLeft, 0)             \
  V(ShiftRight, Int32ShiftRight, 0)           \
  V(ShiftRightLogical, Int32ShiftRightLogical, {})

#define MAP_UNARY_OPERATION_TO_INT32_NODE(V) \
  V(BitwiseNot, Int32BitwiseNot)             \
  V(Increment, Int32IncrementWithOverflow)   \
  V(Decrement, Int32DecrementWithOverflow)   \
  V(Negate, Int32NegateWithOverflow)

// MAP_OPERATION_TO_FLOAT64_NODE are tuples with the following format:
// (Operation name, Float64 operation node).
#define MAP_OPERATION_TO_FLOAT64_NODE(V) \
  V(Add, Float64Add)                     \
  V(Subtract, Float64Subtract)           \
  V(Multiply, Float64Multiply)           \
  V(Divide, Float64Divide)               \
  V(Modulus, Float64Modulus)             \
  V(Exponentiate, Float64Exponentiate)

template <Operation kOperation>
static constexpr std::optional<int> Int32Identity() {
  switch (kOperation) {
#define CASE(op, _, identity) \
  case Operation::k##op:      \
    return identity;
    MAP_BINARY_OPERATION_TO_INT32_NODE(CASE)
#undef CASE
    default:
      UNREACHABLE();
  }
}

// Forward-declare NodeBase sub-hierarchies.
class NodeBase;
class Node;
class ControlNode;
class ConditionalControlNode;
class BranchControlNode;
class UnconditionalControlNode;
class TerminalControlNode;
class ValueNode;

enum class ValueRepresentation : uint8_t {
  kTagged,
  kInt32,
  kUint32,
  kFloat64,
  kHoleyFloat64,
  kIntPtr,
  kNone,
};

inline constexpr bool IsInt64Representable(double value) {
  constexpr double min = -9223372036854775808.0;  // -2^63.
  // INT64_MAX (2^63 - 1) is not representable to double, but 2^63 is, so we
  // check if it is strictly below it.
  constexpr double max_bound = 9223372036854775808.0;  // 2^63.
  return value >= min && value < max_bound;
}

inline constexpr bool IsSafeInteger(int64_t value) {
  return value >= kMinSafeInteger && value <= kMaxSafeInteger;
}

inline constexpr bool IsSafeInteger(double value) {
  if (!std::isfinite(value)) return false;
  if (value != std::trunc(value)) return false;
  double max = static_cast<double>(kMaxSafeInteger);
  double min = static_cast<double>(kMinSafeInteger);
  return value >= min && value <= max;
}

inline constexpr bool IsDoubleRepresentation(ValueRepresentation repr) {
  return repr == ValueRepresentation::kFloat64 ||
         repr == ValueRepresentation::kHoleyFloat64;
}

inline constexpr bool IsZeroExtendedRepresentation(ValueRepresentation repr) {
#if defined(V8_TARGET_ARCH_RISCV64)
  // on RISC-V int32 are always sign-extended
  return false;
#else
  return (repr == ValueRepresentation::kUint32 ||
          repr == ValueRepresentation::kInt32);
#endif
}

inline bool ValueRepresentationIs(ValueRepresentation got,
                                  ValueRepresentation expected) {
  // Allow Float64 values to be inputs when HoleyFloat64 is expected.
  return (got == expected) || (got == ValueRepresentation::kFloat64 &&
                               expected == ValueRepresentation::kHoleyFloat64);
}

// ValueRepresentation doesn't distinguish between Int32 and TruncatedInt32:
// both are Int32. For Phi untagging however, it's interesting to have a
// difference between the 2, as a TruncatedInt32 would allow untagging to
// Float64, whereas an Int32 use wouldn't (because it would require a deopting
// Float64->Int32 conversion, whereas the truncating version of this conversion
// cannot deopt). We thus use a UseRepresentation to record use hints for Phis.
enum class UseRepresentation : uint8_t {
  kTagged,
  kInt32,
  kTruncatedInt32,
  kUint32,
  kFloat64,
  kHoleyFloat64,
};

std::ostream& operator<<(std::ostream& os, UseRepresentation repr);

typedef base::EnumSet<ValueRepresentation, int8_t> ValueRepresentationSet;
typedef base::EnumSet<UseRepresentation, int8_t> UseRepresentationSet;

// TODO(olivf): Rename Unknown to Any.

/* Every object should belong to exactly one of these.*/
#define LEAF_NODE_TYPE_LIST(V)                      \
  V(Smi, (1 << 0))                                  \
  V(HeapNumber, (1 << 1))                           \
  V(Null, (1 << 2))                                 \
  V(Undefined, (1 << 3))                            \
  V(Boolean, (1 << 4))                              \
  V(Symbol, (1 << 5))                               \
  /* String Venn diagram:                        */ \
  /* ┌String───────────────────────────────────┐ */ \
  /* │                            OtherString  │ */ \
  /* │┌InternalizedString───┐                  │ */ \
  /* ││                     │                  │ */ \
  /* ││OtherInternalized    │                  │ */ \
  /* ││String               │                  │ */ \
  /* │├─────────────────────┼─SeqOneByteString┐│ */ \
  /* ││                     │                 ││ */ \
  /* ││OtherSeqInternalized │ OtherSeqOneByte ││ */ \
  /* ││OneByteString        │ String          ││ */ \
  /* │├──────────────────┐  │                 ││ */ \
  /* ││ROSeqInternalized │  │                 ││ */ \
  /* ││OneByteString     │  │                 ││ */ \
  /* │└──────────────────┴──┴─────────────────┘│ */ \
  /* └─────────────────────────────────────────┘ */ \
  V(ROSeqInternalizedOneByteString, (1 << 6))       \
  V(OtherSeqInternalizedOneByteString, (1 << 7))    \
  V(OtherInternalizedString, (1 << 8))              \
  V(OtherSeqOneByteString, (1 << 9))                \
  V(OtherString, (1 << 10))                         \
                                                    \
  V(Context, (1 << 11))                             \
  V(StringWrapper, (1 << 12))                       \
  V(JSArray, (1 << 13))                             \
  V(JSFunction, (1 << 14))                          \
  V(OtherCallable, (1 << 15))                       \
  V(OtherHeapObject, (1 << 16))                     \
  V(OtherJSReceiver, (1 << 17))

#define COUNT(...) +1
static constexpr int kNumberOfLeafNodeTypes = 0 LEAF_NODE_TYPE_LIST(COUNT);
#undef COUNT

#define COMBINED_NODE_TYPE_LIST(V)                                        \
  /* A value which has all the above bits set */                          \
  V(Unknown, ((1 << kNumberOfLeafNodeTypes) - 1))                         \
  V(Callable, kJSFunction | kOtherCallable)                               \
  V(NullOrUndefined, kNull | kUndefined)                                  \
  V(Oddball, kNullOrUndefined | kBoolean)                                 \
  V(Number, kSmi | kHeapNumber)                                           \
  V(NumberOrBoolean, kNumber | kBoolean)                                  \
  V(NumberOrOddball, kNumber | kOddball)                                  \
  V(InternalizedString, kROSeqInternalizedOneByteString |                 \
                            kOtherSeqInternalizedOneByteString |          \
                            kOtherInternalizedString)                     \
  V(SeqOneByteString, kROSeqInternalizedOneByteString |                   \
                          kOtherSeqInternalizedOneByteString |            \
                          kOtherSeqOneByteString)                         \
  V(String, kInternalizedString | kSeqOneByteString | kOtherString)       \
  V(StringOrStringWrapper, kString | kStringWrapper)                      \
  V(StringOrOddball, kString | kOddball)                                  \
  V(Name, kString | kSymbol)                                              \
  /* TODO(jgruber): Add kBigInt and kSymbol once they exist. */           \
  V(JSPrimitive, kNumber | kString | kBoolean | kNullOrUndefined)         \
  V(JSReceiver, kJSArray | kCallable | kStringWrapper | kOtherJSReceiver) \
  V(JSReceiverOrNullOrUndefined, kJSReceiver | kNullOrUndefined)          \
  V(AnyHeapObject, kUnknown - kSmi)

#define NODE_TYPE_LIST(V) \
  LEAF_NODE_TYPE_LIST(V)  \
  COMBINED_NODE_TYPE_LIST(V)

enum class NodeType : uint32_t {
#define DEFINE_NODE_TYPE(Name, Value) k##Name = Value,
  NODE_TYPE_LIST(DEFINE_NODE_TYPE)
#undef DEFINE_NODE_TYPE
};
using NodeTypeInt = std::underlying_type_t<NodeType>;

// Some leaf node types only exist to complement other leaf node types in a
// combined type. We never expect to see these as standalone types.
inline constexpr bool NodeTypeIsNeverStandalone(NodeType type) {
  switch (type) {
    // "Other" string types should be considered internal and never appear as
    // standalone leaf types.
    case NodeType::kOtherCallable:
    case NodeType::kOtherInternalizedString:
    case NodeType::kOtherSeqInternalizedOneByteString:
    case NodeType::kOtherSeqOneByteString:
    case NodeType::kOtherString:
      return true;
    default:
      return false;
  }
}

inline constexpr NodeType EmptyNodeType() { return static_cast<NodeType>(0); }

inline constexpr NodeType IntersectType(NodeType left, NodeType right) {
  DCHECK(!NodeTypeIsNeverStandalone(left));
  DCHECK(!NodeTypeIsNeverStandalone(right));
  return static_cast<NodeType>(static_cast<NodeTypeInt>(left) &
                               static_cast<NodeTypeInt>(right));
}
inline constexpr NodeType UnionType(NodeType left, NodeType right) {
  DCHECK(!NodeTypeIsNeverStandalone(left));
  DCHECK(!NodeTypeIsNeverStandalone(right));
  return static_cast<NodeType>(static_cast<NodeTypeInt>(left) |
                               static_cast<NodeTypeInt>(right));
}
inline constexpr bool NodeTypeIs(NodeType type, NodeType to_check) {
  DCHECK(!NodeTypeIsNeverStandalone(type));
  DCHECK(!NodeTypeIsNeverStandalone(to_check));
  NodeTypeInt right = static_cast<NodeTypeInt>(to_check);
  return (static_cast<NodeTypeInt>(type) & (~right)) == 0;
}
inline constexpr bool NodeTypeCanBe(NodeType type, NodeType to_check) {
  DCHECK(!NodeTypeIsNeverStandalone(type));
  DCHECK(!NodeTypeIsNeverStandalone(to_check));
  NodeTypeInt right = static_cast<NodeTypeInt>(to_check);
  return (static_cast<NodeTypeInt>(type) & (right)) != 0;
}

static_assert(!NodeTypeCanBe(NodeType::kJSPrimitive, NodeType::kJSReceiver));

inline constexpr bool NodeTypeIsUnstable(NodeType type) {
  DCHECK(!NodeTypeIsNeverStandalone(type));
  // Any type that can be a string might be unstable, if the string part of the
  // type is unstable.
  if (NodeTypeCanBe(type, NodeType::kString)) {
    // Extract out the string part of the node type.
    NodeType string_type = IntersectType(type, NodeType::kString);
    // RO-space strings are ok, since they can't change.
    if (string_type == NodeType::kROSeqInternalizedOneByteString) return false;
    // The generic internalized string type is ok, since it doesn't consider
    // seqness and internalized strings stay internalized.
    if (string_type == NodeType::kInternalizedString) return false;
    // The generic string type is ok, since it defines all strings.
    if (string_type == NodeType::kString) return false;
    // Otherwise, a string can get in-place externalized, or in-place converted
    // to thin if not already internalized, both of which lose seq-ness.
    // TODO(leszeks): We could probably consider byteness of internalized
    // strings to be stable, since we can't change byteness with in-place
    // externalization.
    return true;
  }
  // All other node types are stable.
  return false;
}
// Seq strings are unstable because they could be in-place converted to thin
// strings.
static_assert(NodeTypeIsUnstable(NodeType::kSeqOneByteString));
// Internalized strings are stable because they have to stay internalized.
static_assert(!NodeTypeIsUnstable(NodeType::kInternalizedString));
// RO internalized strings are stable because they are read-only.
static_assert(!NodeTypeIsUnstable(NodeType::kROSeqInternalizedOneByteString));
// The generic string type is stable because we've already erased any
// information about it.
static_assert(!NodeTypeIsUnstable(NodeType::kString));
// A type which contains an unstable string should also be unstable.
static_assert(NodeTypeIsUnstable(UnionType(NodeType::kNumber,
                                           NodeType::kSeqOneByteString)));
// A type which contains a stable string should also be stable.
static_assert(!NodeTypeIsUnstable(UnionType(NodeType::kNumber,
                                            NodeType::kInternalizedString)));

inline constexpr NodeType MakeTypeStable(NodeType type) {
  DCHECK(!NodeTypeIsNeverStandalone(type));
  if (!NodeTypeIsUnstable(type)) return type;
  // Strings can be in-place internalized, turned into thin strings, and
  // in-place externalized, and byteness can change from one->two byte (because
  // of internalized external strings with two-byte encoding of one-byte data)
  // or two->one byte (because of internalizing a two-byte slice with one-byte
  // data). The only invariant that we can preserve is that internalized strings
  // stay internalized.
  DCHECK(NodeTypeCanBe(type, NodeType::kString));
  // Extract out the string part of the node type.
  NodeType string_type = IntersectType(type, NodeType::kString);
  if (NodeTypeIs(string_type, NodeType::kInternalizedString)) {
    // Strings that can't be anything but internalized become generic
    // internalized.
    type = UnionType(type, NodeType::kInternalizedString);
  } else {
    // All other strings become fully generic.
    type = UnionType(type, NodeType::kString);
  }
  DCHECK(!NodeTypeIsUnstable(type));
  return type;
}
// Seq strings become normal strings with unspecified byteness when made stable,
// because they could have been internalized into a two-byte external string.
static_assert(MakeTypeStable(NodeType::kSeqOneByteString) == NodeType::kString);
// Generic internalized strings stay as they are.
static_assert(MakeTypeStable(NodeType::kInternalizedString) ==
              NodeType::kInternalizedString);
// Read-only seq internalized strings become generic.
static_assert(MakeTypeStable(NodeType::kROSeqInternalizedOneByteString) ==
              NodeType::kROSeqInternalizedOneByteString);
// Stabilizing a type which is partially an unstable string should generalize
// the string part of the type
static_assert(MakeTypeStable(UnionType(NodeType::kNumber,
                                       NodeType::kSeqOneByteString)) ==
              UnionType(NodeType::kNumber, NodeType::kString));

// Assert that the Unknown type is constructed correctly.
#define ADD_STATIC_ASSERT(Name, Value)                          \
  static_assert(NodeTypeIsNeverStandalone(NodeType::k##Name) || \
                NodeTypeIs(NodeType::k##Name, NodeType::kUnknown));
LEAF_NODE_TYPE_LIST(ADD_STATIC_ASSERT)
#undef ADD_STATIC_ASSERT

inline NodeType StaticTypeForMap(compiler::MapRef map,
                                 compiler::JSHeapBroker* broker) {
  if (map.IsHeapNumberMap()) return NodeType::kHeapNumber;
  if (map.IsStringMap()) {
    if (map.IsInternalizedStringMap()) {
      return NodeType::kInternalizedString;
    }
    if (map.IsSeqStringMap() && map.IsOneByteStringMap()) {
      return NodeType::kSeqOneByteString;
    }
    return NodeType::kString;
  }
  if (map.IsStringWrapperMap()) return NodeType::kStringWrapper;
  if (map.IsSymbolMap()) return NodeType::kSymbol;
  if (map.IsBooleanMap(broker)) return NodeType::kBoolean;
  if (map.IsOddballMap()) {
    // Oddball but not a Boolean.
    return NodeType::kNullOrUndefined;
  }
  if (map.IsContextMap()) return NodeType::kContext;
  if (map.IsJSArrayMap()) return NodeType::kJSArray;
  if (map.IsJSFunctionMap()) return NodeType::kJSFunction;
  if (map.is_callable()) {
    return NodeType::kCallable;
  }
  if (map.IsJSReceiverMap()) {
    // JSReceiver but not any of the above.
    return NodeType::kOtherJSReceiver;
  }
  return NodeType::kOtherHeapObject;
}

inline constexpr bool IsEmptyNodeType(NodeType type) {
  // No bits are set.
  return static_cast<int>(type) == 0;
}

inline NodeType StaticTypeForConstant(compiler::JSHeapBroker* broker,
                                      compiler::ObjectRef ref) {
  if (ref.IsSmi()) return NodeType::kSmi;
  if (ref.HoleType() != compiler::HoleType::kNone)
    return NodeType::kOtherHeapObject;
  NodeType type = StaticTypeForMap(ref.AsHeapObject().map(broker), broker);
  DCHECK(!IsEmptyNodeType(type));
  if (type == NodeType::kInternalizedString && ref.is_read_only()) {
    if (ref.AsString().IsSeqString() &&
        ref.AsString().IsOneByteRepresentation()) {
      type = NodeType::kROSeqInternalizedOneByteString;
    }
  }
  return type;
}

inline bool IsInstanceOfLeafNodeType(compiler::MapRef map, NodeType type,
                                     compiler::JSHeapBroker* broker) {
  switch (type) {
    case NodeType::kSmi:
      return false;
    case NodeType::kHeapNumber:
      return map.IsHeapNumberMap();
    case NodeType::kNull:
      return map.IsNullMap(broker);
    case NodeType::kUndefined:
      return map.IsUndefinedMap(broker);
    case NodeType::kBoolean:
      return map.IsBooleanMap(broker);
    case NodeType::kSymbol:
      return map.IsSymbolMap();
    case NodeType::kOtherString:
      // This doesn't exclude other string leaf types, which means one should
      // never test for this node type alone.
      return map.IsStringMap();
    case NodeType::kOtherSeqOneByteString:
      return map.IsSeqStringMap() && map.IsOneByteStringMap();
    // We can't prove with a map alone that an object is in RO-space, but
    // these maps will be potential candidates.
    case NodeType::kROSeqInternalizedOneByteString:
    case NodeType::kOtherSeqInternalizedOneByteString:
      return map.IsInternalizedStringMap() && map.IsSeqStringMap() &&
             map.IsOneByteStringMap();
    case NodeType::kOtherInternalizedString:
      return map.IsInternalizedStringMap();
    case NodeType::kStringWrapper:
      return map.IsStringWrapperMap();
    case NodeType::kContext:
      return map.IsContextMap();
    case NodeType::kJSArray:
      return map.IsJSArrayMap();
    case NodeType::kJSFunction:
      return map.IsJSFunctionMap();
    case NodeType::kCallable:
      return map.is_callable();
    case NodeType::kOtherCallable:
      return map.is_callable() && !map.IsJSFunctionMap();
    case NodeType::kOtherJSReceiver:
      return map.IsJSReceiverMap() && !map.IsJSArrayMap() &&
             !map.is_callable() && !map.IsStringWrapperMap();
    case NodeType::kOtherHeapObject:
      return !map.IsHeapNumberMap() && !map.IsOddballMap() &&
             !map.IsContextMap() && !map.IsSymbolMap() && !map.IsStringMap() &&
             !map.IsJSReceiverMap();
    default:
      UNREACHABLE();
  }
}

inline bool IsInstanceOfNodeType(compiler::MapRef map, NodeType type,
                                 compiler::JSHeapBroker* broker) {
  DCHECK(!NodeTypeIsNeverStandalone(type));

  // Early return for any heap object.
  if (NodeTypeIs(NodeType::kAnyHeapObject, type)) {
    // Unknown types will be handled here too.
    static_assert(NodeTypeIs(NodeType::kAnyHeapObject, NodeType::kUnknown));
    return true;
  }

  // Iterate over each leaf type bit in the type bitmask, and check if the map
  // matches it.
  NodeTypeInt type_bits = static_cast<NodeTypeInt>(type);
  while (type_bits != 0) {
    NodeTypeInt current_bit =
        1 << base::bits::CountTrailingZerosNonZero(type_bits);
    NodeType leaf_type = static_cast<NodeType>(current_bit);
    if (IsInstanceOfLeafNodeType(map, leaf_type, broker)) return true;
    type_bits = base::bits::ClearLsb(type_bits);
  }
  return false;
}

inline std::ostream& operator<<(std::ostream& out, const NodeType& type) {
  if (IsEmptyNodeType(type)) {
    out << "Empty";
    return out;
  }
  switch (type) {
#define CASE(Name, _)     \
  case NodeType::k##Name: \
    out << #Name;         \
    break;
    NODE_TYPE_LIST(CASE)
#undef CASE
    default:
#define CASE(Name, _)                                        \
  if (NodeTypeIs(NodeType::k##Name, type)) {                 \
    if constexpr (NodeType::k##Name != NodeType::kUnknown) { \
      out << #Name "|";                                      \
    }                                                        \
  }
      LEAF_NODE_TYPE_LIST(CASE)
#undef CASE
  }
  return out;
}

#define DEFINE_NODE_TYPE_CHECK(Type, _)         \
  inline bool NodeTypeIs##Type(NodeType type) { \
    return NodeTypeIs(type, NodeType::k##Type); \
  }
NODE_TYPE_LIST(DEFINE_NODE_TYPE_CHECK)
#undef DEFINE_NODE_TYPE_CHECK

inline bool NodeTypeMayBeNullOrUndefined(NodeType type) {
  return (static_cast<int>(type) &
          static_cast<int>(NodeType::kNullOrUndefined)) != 0;
}

struct RangeType {
  RangeType() = default;
  RangeType(int64_t min, int64_t max) : is_valid_(true), min_(min), max_(max) {
    DCHECK(min <= max);
  }
  explicit RangeType(int64_t value) : RangeType(value, value) {}

  bool is_valid() const { return is_valid_; }

  int64_t max() const {
    DCHECK(is_valid_);
    return max_;
  }

  int64_t min() const {
    DCHECK(is_valid_);
    return min_;
  }

  static RangeType Join(base::FunctionRef<double(double, double)> op,
                        RangeType left, RangeType right) {
    double results[4];
    results[0] =
        op(static_cast<double>(left.min()), static_cast<double>(right.min()));
    results[1] =
        op(static_cast<double>(left.min()), static_cast<double>(right.max()));
    results[2] =
        op(static_cast<double>(left.max()), static_cast<double>(right.min()));
    results[3] =
        op(static_cast<double>(left.max()), static_cast<double>(right.max()));
    double min = *std::min_element(std::begin(results), std::end(results));
    double max = *std::max_element(std::begin(results), std::end(results));
    if (!IsInt64Representable(min) || !IsInt64Representable(max)) return {};
    return RangeType(static_cast<int64_t>(min), static_cast<int64_t>(max));
  }

  bool IsSafeIntegerRange() {
    if (!is_valid_) return false;
    return min_ >= kMinSafeInteger && max_ <= kMaxSafeInteger;
  }

 private:
  bool is_valid_ = false;
  int64_t min_ = 0;
  int64_t max_ = 0;
};

inline std::ostream& operator<<(std::ostream& os, const RangeType& range) {
  if (!range.is_valid()) {
    os << "[-inf, +inf]";
    return os;
  }
  os << "[" << range.min() << ", " << range.max() << "]";
  return os;
}

enum class TaggedToFloat64ConversionType : uint8_t {
  kOnlyNumber,
  kNumberOrUndefined,
  kNumberOrBoolean,
  kNumberOrOddball,
};

constexpr TaggedToFloat64ConversionType GetTaggedToFloat64ConversionType(
    NodeType type) {
  if (NodeTypeIs(type, NodeType::kNumber)) {
    return TaggedToFloat64ConversionType::kOnlyNumber;
  }
  if (NodeTypeIs(type, NodeType::kNumberOrBoolean)) {
    return TaggedToFloat64ConversionType::kNumberOrBoolean;
  }
  DCHECK(NodeTypeIs(type, NodeType::kNumberOrOddball));
  return TaggedToFloat64ConversionType::kNumberOrOddball;
}

constexpr Condition ConditionFor(Operation cond);
constexpr Condition ConditionForNaN();

bool FromConstantToBool(LocalIsolate* local_isolate, ValueNode* node);
bool FromConstantToBool(MaglevAssembler* masm, ValueNode* node);

inline std::ostream& operator<<(std::ostream& os,
                                const ValueRepresentation& repr) {
  switch (repr) {
    case ValueRepresentation::kTagged:
      return os << "Tagged";
    case ValueRepresentation::kInt32:
      return os << "Int32";
    case ValueRepresentation::kUint32:
      return os << "Uint32";
    case ValueRepresentation::kFloat64:
      return os << "Float64";
    case ValueRepresentation::kHoleyFloat64:
      return os << "HoleyFloat64";
    case ValueRepresentation::kIntPtr:
      return os << "IntPtr";
    case ValueRepresentation::kNone:
      return os << "None";
  }
}

inline std::ostream& operator<<(
    std::ostream& os, const TaggedToFloat64ConversionType& conversion_type) {
  switch (conversion_type) {
    case TaggedToFloat64ConversionType::kOnlyNumber:
      return os << "Number";
    case TaggedToFloat64ConversionType::kNumberOrUndefined:
      return os << "NumberOrUndefined";
    case TaggedToFloat64ConversionType::kNumberOrBoolean:
      return os << "NumberOrBoolean";
    case TaggedToFloat64ConversionType::kNumberOrOddball:
      return os << "NumberOrOddball";
  }
}

inline bool HasOnlyJSTypedArrayMaps(base::Vector<const compiler::MapRef> maps) {
  for (compiler::MapRef map : maps) {
    if (!map.IsJSTypedArrayMap()) return false;
  }
  return true;
}

inline bool HasOnlyJSArrayMaps(base::Vector<const compiler::MapRef> maps) {
  for (compiler::MapRef map : maps) {
    if (!map.IsJSArrayMap()) return false;
  }
  return true;
}

inline bool HasOnlyJSObjectMaps(base::Vector<const compiler::MapRef> maps) {
  for (compiler::MapRef map : maps) {
    if (!map.IsJSObjectMap()) return false;
  }
  return true;
}

inline bool HasOnlyStringMaps(base::Vector<const compiler::MapRef> maps) {
  for (compiler::MapRef map : maps) {
    if (!map.IsStringMap()) return false;
  }
  return true;
}

inline bool HasOnlyNumberMaps(base::Vector<const compiler::MapRef> maps) {
  for (compiler::MapRef map : maps) {
    if (map.instance_type() != HEAP_NUMBER_TYPE) return false;
  }
  return true;
}

inline bool HasNumberMap(base::Vector<const compiler::MapRef> maps) {
  for (compiler::MapRef map : maps) {
    if (map.instance_type() == HEAP_NUMBER_TYPE) return true;
  }
  return false;
}

#define DEF_FORWARD_DECLARATION(type, ...) class type;
NODE_BASE_LIST(DEF_FORWARD_DECLARATION)
#undef DEF_FORWARD_DECLARATION

// Represents either a direct BasicBlock pointer, or an entry in a list of
// unresolved BasicBlockRefs which will be mutated (in place) at some point into
// direct BasicBlock pointers.
class BasicBlockRef {
  struct BasicBlockRefBuilder;

 public:
  BasicBlockRef() : next_ref_(nullptr) {
#ifdef DEBUG
    state_ = kRefList;
#endif
  }
  explicit BasicBlockRef(BasicBlock* block) : block_ptr_(block) {
#ifdef DEBUG
    state_ = kBlockPointer;
#endif
  }

  // Refs can't be copied or moved, since they are referenced by `this` pointer
  // in the ref list.
  BasicBlockRef(const BasicBlockRef&) = delete;
  BasicBlockRef(BasicBlockRef&&) = delete;
  BasicBlockRef& operator=(const BasicBlockRef&) = delete;
  BasicBlockRef& operator=(BasicBlockRef&&) = delete;

  // Construct a new ref-list mode BasicBlockRef and add it to the given ref
  // list.
  explicit BasicBlockRef(BasicBlockRef* ref_list_head) : BasicBlockRef() {
    BasicBlockRef* old_next_ptr = MoveToRefList(ref_list_head);
    USE(old_next_ptr);
    DCHECK_NULL(old_next_ptr);
  }

  // Change this ref to a direct basic block pointer, returning the old "next"
  // pointer of the current ref.
  BasicBlockRef* SetToBlockAndReturnNext(BasicBlock* block) {
    DCHECK_EQ(state_, kRefList);

    BasicBlockRef* old_next_ptr = next_ref_;
    block_ptr_ = block;
#ifdef DEBUG
    state_ = kBlockPointer;
#endif
    return old_next_ptr;
  }

  // Reset this ref list to null, returning the old ref list (i.e. the old
  // "next" pointer).
  BasicBlockRef* Reset() {
    DCHECK_EQ(state_, kRefList);

    BasicBlockRef* old_next_ptr = next_ref_;
    next_ref_ = nullptr;
    return old_next_ptr;
  }

  // Move this ref to the given ref list, returning the old "next" pointer of
  // the current ref.
  BasicBlockRef* MoveToRefList(BasicBlockRef* ref_list_head) {
    DCHECK_EQ(state_, kRefList);
    DCHECK_EQ(ref_list_head->state_, kRefList);

    BasicBlockRef* old_next_ptr = next_ref_;
    next_ref_ = ref_list_head->next_ref_;
    ref_list_head->next_ref_ = this;
    return old_next_ptr;
  }

  void Bind(BasicBlock* block) {
    DCHECK_EQ(state_, kRefList);

    BasicBlockRef* next_ref = SetToBlockAndReturnNext(block);
    while (next_ref != nullptr) {
      next_ref = next_ref->SetToBlockAndReturnNext(block);
    }
    DCHECK_EQ(block_ptr(), block);
  }

  BasicBlock* block_ptr() const {
    DCHECK_EQ(state_, kBlockPointer);
    return block_ptr_;
  }

  void set_block_ptr(BasicBlock* block) {
    DCHECK_EQ(state_, kBlockPointer);
    block_ptr_ = block;
  }

  BasicBlockRef* next_ref() const {
    DCHECK_EQ(state_, kRefList);
    return next_ref_;
  }

  bool has_ref() const {
    DCHECK_EQ(state_, kRefList);
    return next_ref_ != nullptr;
  }

 private:
  union {
    BasicBlock* block_ptr_;
    BasicBlockRef* next_ref_;
  };
#ifdef DEBUG
  enum { kBlockPointer, kRefList } state_;
#endif  // DEBUG
};

class OpProperties {
 public:
  constexpr bool is_call() const {
    // Only returns true for non-deferred calls. Use `is_any_call` to check
    // deferred calls as well.
    return kIsCallBit::decode(bitfield_);
  }
  constexpr bool is_any_call() const { return is_call() || is_deferred_call(); }
  constexpr bool can_eager_deopt() const {
    return kAttachedDeoptInfoBits::decode(bitfield_) ==
           AttachedDeoptInfo::kEager;
  }
  constexpr bool can_lazy_deopt() const {
    return kAttachedDeoptInfoBits::decode(bitfield_) ==
           AttachedDeoptInfo::kLazy;
  }
  constexpr bool is_deopt_checkpoint() const {
    return kAttachedDeoptInfoBits::decode(bitfield_) ==
           AttachedDeoptInfo::kCheckpoint;
  }
  constexpr bool can_deopt() const {
    return can_eager_deopt() || can_lazy_deopt();
  }
  constexpr bool can_throw() const {
    return kCanThrowBit::decode(bitfield_) && can_lazy_deopt();
  }
  // TODO(olivf) We should negate all of these properties which would give them
  // a less error-prone default state.
  constexpr bool can_read() const { return kCanReadBit::decode(bitfield_); }
  constexpr bool can_write() const { return kCanWriteBit::decode(bitfield_); }
  constexpr bool can_allocate() const {
    return kCanAllocateBit::decode(bitfield_);
  }
  // Only for ValueNodes, indicates that the instruction might return something
  // new every time it is executed. For example it creates an object that is
  // unique with regards to strict equality comparison or it reads a value that
  // can change in absence of an explicit write instruction.
  constexpr bool not_idempotent() const {
    return kNotIdempotentBit::decode(bitfield_);
  }
  constexpr ValueRepresentation value_representation() const {
    return kValueRepresentationBits::decode(bitfield_);
  }
  constexpr bool is_tagged() const {
    return value_representation() == ValueRepresentation::kTagged;
  }
  constexpr bool is_conversion() const {
    return kIsConversionBit::decode(bitfield_);
  }
  constexpr bool needs_register_snapshot() const {
    return kNeedsRegisterSnapshotBit::decode(bitfield_);
  }
  constexpr bool is_pure() const {
    return (bitfield_ & kPureMask) == kPureValue;
  }
  constexpr bool is_required_when_unused() const {
    if (is_conversion()) {
      // Calls in conversions are not counted as a side-effect as far as
      // is_required_when_unused is concerned, since they should always be to
      // the Allocate builtin.
      return can_write() || can_throw() || can_deopt();
    } else {
      return can_write() || can_throw() || can_deopt() || is_any_call();
    }
  }
  constexpr bool can_participate_in_cse() const {
    return !can_write() && !not_idempotent();
  }

  constexpr OpProperties operator|(const OpProperties& that) {
    return OpProperties(bitfield_ | that.bitfield_);
  }

  static constexpr OpProperties Pure() { return OpProperties(kPureValue); }
  static constexpr OpProperties Call() {
    return OpProperties(kIsCallBit::encode(true));
  }
  static constexpr OpProperties EagerDeopt() {
    return OpProperties(
        kAttachedDeoptInfoBits::encode(AttachedDeoptInfo::kEager));
  }
  static constexpr OpProperties LazyDeopt() {
    return OpProperties(
        kAttachedDeoptInfoBits::encode(AttachedDeoptInfo::kLazy));
  }
  static constexpr OpProperties DeoptCheckpoint() {
    return OpProperties(
        kAttachedDeoptInfoBits::encode(AttachedDeoptInfo::kCheckpoint));
  }
  static constexpr OpProperties CanThrow() {
    return OpProperties(kCanThrowBit::encode(true)) | LazyDeopt() |
           CanAllocate();
  }
  static constexpr OpProperties CanRead() {
    return OpProperties(kCanReadBit::encode(true));
  }
  static constexpr OpProperties CanWrite() {
    return OpProperties(kCanWriteBit::encode(true));
  }
  static constexpr OpProperties CanAllocate() {
    return OpProperties(kCanAllocateBit::encode(true));
  }
  static constexpr OpProperties NotIdempotent() {
    return OpProperties(kNotIdempotentBit::encode(true));
  }
  static constexpr OpProperties TaggedValue() {
    return OpProperties(
        kValueRepresentationBits::encode(ValueRepresentation::kTagged));
  }
  static constexpr OpProperties ExternalReference() {
    return OpProperties(
        kValueRepresentationBits::encode(ValueRepresentation::kIntPtr));
  }
  static constexpr OpProperties Int32() {
    return OpProperties(
        kValueRepresentationBits::encode(ValueRepresentation::kInt32));
  }
  static constexpr OpProperties Uint32() {
    return OpProperties(
        kValueRepresentationBits::encode(ValueRepresentation::kUint32));
  }
  static constexpr OpProperties Float64() {
    return OpProperties(
        kValueRepresentationBits::encode(ValueRepresentation::kFloat64));
  }
  static constexpr OpProperties HoleyFloat64() {
    return OpProperties(
        kValueRepresentationBits::encode(ValueRepresentation::kHoleyFloat64));
  }
  static constexpr OpProperties IntPtr() {
    return OpProperties(
        kValueRepresentationBits::encode(ValueRepresentation::kIntPtr));
  }
  static constexpr OpProperties TrustedPointer() {
    return OpProperties(
        kValueRepresentationBits::encode(ValueRepresentation::kTagged));
  }
  static constexpr OpProperties ForValueRepresentation(
      ValueRepresentation repr) {
    return OpProperties(kValueRepresentationBits::encode(repr));
  }
  static constexpr OpProperties ConversionNode() {
    return OpProperties(kIsConversionBit::encode(true));
  }
  static constexpr OpProperties CanCallUserCode() {
    return AnySideEffects() | LazyDeopt() | CanThrow();
  }
  // Without auditing the call target, we must assume it can cause a lazy deopt
  // and throw. Use this when codegen calls runtime or a builtin, unless
  // certain that the target either doesn't throw or cannot deopt.
  // TODO(jgruber): Go through all nodes marked with this property and decide
  // whether to keep it (or remove either the lazy-deopt or throw flag).
  static constexpr OpProperties GenericRuntimeOrBuiltinCall() {
    return Call() | CanCallUserCode() | NotIdempotent();
  }
  static constexpr OpProperties JSCall() { return Call() | CanCallUserCode(); }
  static constexpr OpProperties AnySideEffects() {
    return CanRead() | CanWrite() | CanAllocate();
  }
  static constexpr OpProperties DeferredCall() {
    // Operations with a deferred call need a snapshot of register state,
    // because they need to be able to push registers to save them, and annotate
    // the safepoint with information about which registers are tagged.
    return NeedsRegisterSnapshot();
  }

  constexpr explicit OpProperties(uint32_t bitfield) : bitfield_(bitfield) {}
  operator uint32_t() const { return bitfield_; }

  OpProperties WithNewValueRepresentation(ValueRepresentation new_repr) const {
    return OpProperties(kValueRepresentationBits::update(bitfield_, new_repr));
  }

  OpProperties WithoutDeopt() const {
    return OpProperties(
        kAttachedDeoptInfoBits::update(bitfield_, AttachedDeoptInfo::kNone));
  }

 private:
  enum class AttachedDeoptInfo { kNone, kEager, kLazy, kCheckpoint };
  using kIsCallBit = base::BitField<bool, 0, 1>;
  using kAttachedDeoptInfoBits = kIsCallBit::Next<AttachedDeoptInfo, 2>;
  using kCanThrowBit = kAttachedDeoptInfoBits::Next<bool, 1>;
  using kCanReadBit = kCanThrowBit::Next<bool, 1>;
  using kCanWriteBit = kCanReadBit::Next<bool, 1>;
  using kCanAllocateBit = kCanWriteBit::Next<bool, 1>;
  using kNotIdempotentBit = kCanAllocateBit::Next<bool, 1>;
  using kValueRepresentationBits =
      kNotIdempotentBit::Next<ValueRepresentation, 3>;
  using kIsConversionBit = kValueRepresentationBits::Next<bool, 1>;
  using kNeedsRegisterSnapshotBit = kIsConversionBit::Next<bool, 1>;

  static const uint32_t kPureMask =
      kCanReadBit::kMask | kCanWriteBit::kMask | kCanAllocateBit::kMask;
  static const uint32_t kPureValue = kCanReadBit::encode(false) |
                                     kCanWriteBit::encode(false) |
                                     kCanAllocateBit::encode(false);

  // NeedsRegisterSnapshot is only used for DeferredCall, and we rely on this in
  // `is_deferred_call` to detect deferred calls. If you need to use
  // NeedsRegisterSnapshot for something else that DeferredCalls, then you'll
  // have to update `is_any_call`.
  static constexpr OpProperties NeedsRegisterSnapshot() {
    return OpProperties(kNeedsRegisterSnapshotBit::encode(true));
  }

  const uint16_t bitfield_;

 public:
  static const size_t kUsedSize = kNeedsRegisterSnapshotBit::kLastUsedBit + 1;
  static const size_t kSize = 16;
  static_assert(kUsedSize <= kSize);

  constexpr bool is_deferred_call() const {
    // Currently, there is no kDeferredCall bit, but DeferredCall only sets a
    // single bit: kNeedsRegisterSnapShot. If this static assert breaks, it
    // means that you added additional properties to DeferredCall, and you
    // should update this function accordingly.
    static_assert(DeferredCall().bitfield_ ==
                  kNeedsRegisterSnapshotBit::encode(true));
    return needs_register_snapshot();
  }
};

constexpr inline OpProperties StaticPropertiesForOpcode(Opcode opcode);

class Input {
 public:
  Input(NodeBase* base, int index) : base_(base), index_(index) {}

  inline ValueNode* node() const;
  inline InputLocation* location() const;
  inline const compiler::InstructionOperand& operand() const {
    return location()->operand();
  }
  inline void clear();

  bool operator==(const Input& other) const {
    return node() == other.node() && location() == other.location();
  }

 private:
  friend class ConstInput;
  NodeBase* base_;
  int index_;
};

class ConstInput {
 public:
  ConstInput(const NodeBase* base, int index) : base_(base), index_(index) {}

  // NOLINTNEXTLINE
  ConstInput(const Input& input) {
    base_ = input.base_;
    index_ = input.index_;
  }

  inline const ValueNode* node() const;
  inline const InputLocation* location() const;
  inline const compiler::InstructionOperand& operand() const {
    return location()->operand();
  }

 private:
  const NodeBase* base_;
  int index_;
};

class VirtualObjectList;
class InterpretedDeoptFrame;
class InlinedArgumentsDeoptFrame;
class ConstructInvokeStubDeoptFrame;
class BuiltinContinuationDeoptFrame;
class DeoptFrame {
 public:
  enum class FrameType {
    kInterpretedFrame,
    kInlinedArgumentsFrame,
    kConstructInvokeStubFrame,
    kBuiltinContinuationFrame,
  };

  struct InterpretedFrameData {
    const MaglevCompilationUnit& unit;
    const CompactInterpreterFrameState* frame_state;
    ValueNode* closure;
    VirtualObject* last_virtual_object;
    const BytecodeOffset bytecode_position;
    const SourcePosition source_position;
  };

  struct InlinedArgumentsFrameData {
    const MaglevCompilationUnit& unit;
    const BytecodeOffset bytecode_position;
    ValueNode* closure;
    const base::Vector<ValueNode*> arguments;
  };

  struct ConstructInvokeStubFrameData {
    const MaglevCompilationUnit& unit;
    const SourcePosition source_position;
    ValueNode* receiver;
    ValueNode* context;
  };

  struct BuiltinContinuationFrameData {
    const Builtin builtin_id;
    const base::Vector<ValueNode*> parameters;
    ValueNode* context;
    compiler::OptionalJSFunctionRef maybe_js_target;
  };

  using FrameData = base::DiscriminatedUnion<
      FrameType, InterpretedFrameData, InlinedArgumentsFrameData,
      ConstructInvokeStubFrameData, BuiltinContinuationFrameData>;

  DeoptFrame(FrameData&& data, DeoptFrame* parent)
      : data_(std::move(data)), parent_(parent) {}

  DeoptFrame(const FrameData& data, DeoptFrame* parent)
      : data_(data), parent_(parent) {}

  FrameType type() const { return data_.tag(); }
  DeoptFrame* parent() { return parent_; }
  const DeoptFrame* parent() const { return parent_; }

  inline const InterpretedDeoptFrame& as_interpreted() const;
  inline const InlinedArgumentsDeoptFrame& as_inlined_arguments() const;
  inline const ConstructInvokeStubDeoptFrame& as_construct_stub() const;
  inline const BuiltinContinuationDeoptFrame& as_builtin_continuation() const;
  inline InterpretedDeoptFrame& as_interpreted();
  inline InlinedArgumentsDeoptFrame& as_inlined_arguments();
  inline ConstructInvokeStubDeoptFrame& as_construct_stub();
  inline BuiltinContinuationDeoptFrame& as_builtin_continuation();
  inline bool IsJsFrame() const;

  inline const MaglevCompilationUnit& GetCompilationUnit() const;
  inline BytecodeOffset GetBytecodeOffset() const;
  inline SourcePosition GetSourcePosition() const;
  inline compiler::SharedFunctionInfoRef GetSharedFunctionInfo() const;
  inline compiler::BytecodeArrayRef GetBytecodeArray() const;
  inline VirtualObjectList GetVirtualObjects() const;

 protected:
  DeoptFrame(InterpretedFrameData&& data, DeoptFrame* parent)
      : data_(std::move(data)), parent_(parent) {}
  DeoptFrame(InlinedArgumentsFrameData&& data, DeoptFrame* parent)
      : data_(std::move(data)), parent_(parent) {}
  DeoptFrame(ConstructInvokeStubFrameData&& data, DeoptFrame* parent)
      : data_(std::move(data)), parent_(parent) {}
  DeoptFrame(BuiltinContinuationFrameData&& data, DeoptFrame* parent)
      : data_(std::move(data)), parent_(parent) {}

  FrameData data_;
  DeoptFrame* const parent_;
};

class InterpretedDeoptFrame : public DeoptFrame {
 public:
  InterpretedDeoptFrame(const MaglevCompilationUnit& unit,
                        const CompactInterpreterFrameState* frame_state,
                        ValueNode* closure, VirtualObject* last_vo,
                        BytecodeOffset bytecode_position,
                        SourcePosition source_position, DeoptFrame* parent)
      : DeoptFrame(InterpretedFrameData{unit, frame_state, closure, last_vo,
                                        bytecode_position, source_position},
                   parent) {}

  const MaglevCompilationUnit& unit() const { return data().unit; }
  const CompactInterpreterFrameState* frame_state() const {
    return data().frame_state;
  }
  ValueNode*& closure() { return data().closure; }
  ValueNode* closure() const { return data().closure; }
  BytecodeOffset bytecode_position() const { return data().bytecode_position; }
  SourcePosition source_position() const { return data().source_position; }
  VirtualObject* last_virtual_object() const {
    return data().last_virtual_object;
  }

  int ComputeReturnOffset(interpreter::Register result_location,
                          int result_size) const;

 private:
  InterpretedFrameData& data() { return data_.get<InterpretedFrameData>(); }
  const InterpretedFrameData& data() const {
    return data_.get<InterpretedFrameData>();
  }
};

// Make sure storing/passing deopt frames by value doesn't truncate them.
static_assert(sizeof(InterpretedDeoptFrame) == sizeof(DeoptFrame));

inline const InterpretedDeoptFrame& DeoptFrame::as_interpreted() const {
  DCHECK_EQ(type(), FrameType::kInterpretedFrame);
  return static_cast<const InterpretedDeoptFrame&>(*this);
}
inline InterpretedDeoptFrame& DeoptFrame::as_interpreted() {
  DCHECK_EQ(type(), FrameType::kInterpretedFrame);
  return static_cast<InterpretedDeoptFrame&>(*this);
}

class InlinedArgumentsDeoptFrame : public DeoptFrame {
 public:
  InlinedArgumentsDeoptFrame(const MaglevCompilationUnit& unit,
                             BytecodeOffset bytecode_position,
                             ValueNode* closure,
                             base::Vector<ValueNode*> arguments,
                             DeoptFrame* parent)
      : DeoptFrame(InlinedArgumentsFrameData{unit, bytecode_position, closure,
                                             arguments},
                   parent) {}

  const MaglevCompilationUnit& unit() const { return data().unit; }
  BytecodeOffset bytecode_position() const { return data().bytecode_position; }
  ValueNode*& closure() { return data().closure; }
  ValueNode* closure() const { return data().closure; }
  base::Vector<ValueNode*> arguments() const { return data().arguments; }

 private:
  InlinedArgumentsFrameData& data() {
    return data_.get<InlinedArgumentsFrameData>();
  }
  const InlinedArgumentsFrameData& data() const {
    return data_.get<InlinedArgumentsFrameData>();
  }
};

// Make sure storing/passing deopt frames by value doesn't truncate them.
static_assert(sizeof(InlinedArgumentsDeoptFrame) == sizeof(DeoptFrame));

inline const InlinedArgumentsDeoptFrame& DeoptFrame::as_inlined_arguments()
    const {
  DCHECK_EQ(type(), FrameType::kInlinedArgumentsFrame);
  return static_cast<const InlinedArgumentsDeoptFrame&>(*this);
}
inline InlinedArgumentsDeoptFrame& DeoptFrame::as_inlined_arguments() {
  DCHECK_EQ(type(), FrameType::kInlinedArgumentsFrame);
  return static_cast<InlinedArgumentsDeoptFrame&>(*this);
}

class ConstructInvokeStubDeoptFrame : public DeoptFrame {
 public:
  ConstructInvokeStubDeoptFrame(const MaglevCompilationUnit& unit,
                                SourcePosition source_position,
                                ValueNode* receiver, ValueNode* context,
                                DeoptFrame* parent)
      : DeoptFrame(ConstructInvokeStubFrameData{unit, source_position, receiver,
                                                context},
                   parent) {}

  const MaglevCompilationUnit& unit() const { return data().unit; }
  ValueNode*& receiver() { return data().receiver; }
  ValueNode* receiver() const { return data().receiver; }
  ValueNode*& context() { return data().context; }
  ValueNode* context() const { return data().context; }
  SourcePosition source_position() const { return data().source_position; }

 private:
  ConstructInvokeStubFrameData& data() {
    return data_.get<ConstructInvokeStubFrameData>();
  }
  const ConstructInvokeStubFrameData& data() const {
    return data_.get<ConstructInvokeStubFrameData>();
  }
};

// Make sure storing/passing deopt frames by value doesn't truncate them.
static_assert(sizeof(ConstructInvokeStubDeoptFrame) == sizeof(DeoptFrame));

inline const ConstructInvokeStubDeoptFrame& DeoptFrame::as_construct_stub()
    const {
  DCHECK_EQ(type(), FrameType::kConstructInvokeStubFrame);
  return static_cast<const ConstructInvokeStubDeoptFrame&>(*this);
}

inline ConstructInvokeStubDeoptFrame& DeoptFrame::as_construct_stub() {
  DCHECK_EQ(type(), FrameType::kConstructInvokeStubFrame);
  return static_cast<ConstructInvokeStubDeoptFrame&>(*this);
}

class BuiltinContinuationDeoptFrame : public DeoptFrame {
 public:
  BuiltinContinuationDeoptFrame(Builtin builtin_id,
                                base::Vector<ValueNode*> parameters,
                                ValueNode* context,
                                compiler::OptionalJSFunctionRef maybe_js_target,
                                DeoptFrame* parent)
      : DeoptFrame(BuiltinContinuationFrameData{builtin_id, parameters, context,
                                                maybe_js_target},
                   parent) {}

  const Builtin& builtin_id() const { return data().builtin_id; }
  base::Vector<ValueNode*> parameters() const { return data().parameters; }
  ValueNode*& context() { return data().context; }
  ValueNode* context() const { return data().context; }
  bool is_javascript() const { return data().maybe_js_target.has_value(); }
  compiler::JSFunctionRef javascript_target() const {
    return data().maybe_js_target.value();
  }

 private:
  BuiltinContinuationFrameData& data() {
    return data_.get<BuiltinContinuationFrameData>();
  }
  const BuiltinContinuationFrameData& data() const {
    return data_.get<BuiltinContinuationFrameData>();
  }
};

// Make sure storing/passing deopt frames by value doesn't truncate them.
static_assert(sizeof(BuiltinContinuationDeoptFrame) == sizeof(DeoptFrame));

inline const BuiltinContinuationDeoptFrame&
DeoptFrame::as_builtin_continuation() const {
  DCHECK_EQ(type(), FrameType::kBuiltinContinuationFrame);
  return static_cast<const BuiltinContinuationDeoptFrame&>(*this);
}
inline BuiltinContinuationDeoptFrame& DeoptFrame::as_builtin_continuation() {
  DCHECK_EQ(type(), FrameType::kBuiltinContinuationFrame);
  return static_cast<BuiltinContinuationDeoptFrame&>(*this);
}

inline bool DeoptFrame::IsJsFrame() const {
  // This must be in sync with TRANSLATION_JS_FRAME_OPCODE_LIST in
  // translation-opcode.h or bad things happen.
  switch (data_.tag()) {
    case FrameType::kInterpretedFrame:
      return true;
    case FrameType::kBuiltinContinuationFrame:
      return as_builtin_continuation().is_javascript();
    case FrameType::kConstructInvokeStubFrame:
    case FrameType::kInlinedArgumentsFrame:
      return false;
  }
}

inline const MaglevCompilationUnit& DeoptFrame::GetCompilationUnit() const {
  switch (type()) {
    case DeoptFrame::FrameType::kInterpretedFrame:
      return as_interpreted().unit();
    case DeoptFrame::FrameType::kInlinedArgumentsFrame:
      return as_inlined_arguments().unit();
    case DeoptFrame::FrameType::kConstructInvokeStubFrame:
      return as_construct_stub().unit();
    case DeoptFrame::FrameType::kBuiltinContinuationFrame:
      return parent()->GetCompilationUnit();
  }
}

inline BytecodeOffset DeoptFrame::GetBytecodeOffset() const {
  switch (type()) {
    case DeoptFrame::FrameType::kInterpretedFrame:
      return as_interpreted().bytecode_position();
    case DeoptFrame::FrameType::kInlinedArgumentsFrame:
      DCHECK_NOT_NULL(parent());
      return parent()->GetBytecodeOffset();
    case DeoptFrame::FrameType::kConstructInvokeStubFrame:
      return BytecodeOffset::None();
    case DeoptFrame::FrameType::kBuiltinContinuationFrame:
      return Builtins::GetContinuationBytecodeOffset(
          as_builtin_continuation().builtin_id());
  }
}

inline SourcePosition DeoptFrame::GetSourcePosition() const {
  switch (type()) {
    case DeoptFrame::FrameType::kInterpretedFrame:
      return as_interpreted().source_position();
    case DeoptFrame::FrameType::kInlinedArgumentsFrame:
      DCHECK_NOT_NULL(parent());
      return parent()->GetSourcePosition();
    case DeoptFrame::FrameType::kConstructInvokeStubFrame:
      return as_construct_stub().source_position();
    case DeoptFrame::FrameType::kBuiltinContinuationFrame:
      DCHECK_NOT_NULL(parent());
      return parent()->GetSourcePosition();
  }
}

inline compiler::SharedFunctionInfoRef DeoptFrame::GetSharedFunctionInfo()
    const {
  return GetCompilationUnit().shared_function_info();
}

inline compiler::BytecodeArrayRef DeoptFrame::GetBytecodeArray() const {
  return GetCompilationUnit().bytecode();
}

class DeoptInfo {
 protected:
  DeoptInfo(Zone* zone, DeoptFrame* top_frame,
            compiler::FeedbackSource feedback_to_update);

 public:
  DeoptFrame& top_frame() { return *top_frame_; }
  const DeoptFrame& top_frame() const { return *top_frame_; }
  const compiler::FeedbackSource& feedback_to_update() const {
    return feedback_to_update_;
  }

  bool has_input_locations() const { return input_locations_ != nullptr; }
  InputLocation* input_locations() const {
    DCHECK_NOT_NULL(input_locations_);
    return input_locations_;
  }
  void InitializeInputLocations(Zone* zone, size_t count);

  Label* deopt_entry_label() { return &deopt_entry_label_; }

  int translation_index() const { return translation_index_; }
  void set_translation_index(int index) { translation_index_ = index; }

#ifdef DEBUG
  size_t input_location_count() { return input_location_count_; }
#endif  // DEBUG

 private:
  DeoptFrame* top_frame_;
  const compiler::FeedbackSource feedback_to_update_;
  InputLocation* input_locations_ = nullptr;
#ifdef DEBUG
  size_t input_location_count_ = 0;
#endif  // DEBUG
  Label deopt_entry_label_;
  int translation_index_ = -1;
};

struct RegisterSnapshot {
  RegList live_registers;
  RegList live_tagged_registers;
  DoubleRegList live_double_registers;
};

class EagerDeoptInfo : public DeoptInfo {
 public:
  EagerDeoptInfo(Zone* zone, DeoptFrame* top_frame,
                 compiler::FeedbackSource feedback_to_update)
      : DeoptInfo(zone, top_frame, feedback_to_update) {}

  DeoptimizeReason reason() const { return reason_; }
  void set_reason(DeoptimizeReason reason) { reason_ = reason; }

  template <typename Function>
  void ForEachInput(Function&& f);
  template <typename Function>
  void ForEachInput(Function&& f) const;

  inline void UnwrapIdentities();

 private:
  DeoptimizeReason reason_ = DeoptimizeReason::kUnknown;
};

class LazyDeoptInfo : public DeoptInfo {
 public:
  LazyDeoptInfo(Zone* zone, DeoptFrame* top_frame,
                interpreter::Register result_location, int result_size,
                compiler::FeedbackSource feedback_to_update)
      : DeoptInfo(zone, top_frame, feedback_to_update),
        result_location_(result_location),
        bitfield_(
            DeoptingCallReturnPcField::encode(kUninitializedCallReturnPc) |
            ResultSizeField::encode(result_size)) {}

  interpreter::Register result_location() const {
    DCHECK(IsConsideredForResultLocation());
    return result_location_;
  }
  int result_size() const {
    DCHECK(IsConsideredForResultLocation());
    return ResultSizeField::decode(bitfield_);
  }

  bool IsResultRegister(interpreter::Register reg) const;
  bool HasResultLocation() const {
    DCHECK(IsConsideredForResultLocation());
    return result_location_.is_valid();
  }

  const InterpretedDeoptFrame& GetFrameForExceptionHandler(
      const ExceptionHandlerInfo* handler_info);

  int deopting_call_return_pc() const {
    DCHECK_NE(DeoptingCallReturnPcField::decode(bitfield_),
              kUninitializedCallReturnPc);
    return DeoptingCallReturnPcField::decode(bitfield_);
  }
  void set_deopting_call_return_pc(int pc) {
    DCHECK_EQ(DeoptingCallReturnPcField::decode(bitfield_),
              kUninitializedCallReturnPc);
    bitfield_ = DeoptingCallReturnPcField::update(bitfield_, pc);
  }

  static bool InReturnValues(interpreter::Register reg,
                             interpreter::Register result_location,
                             int result_size);

  template <typename Function>
  void ForEachInput(Function&& f);
  template <typename Function>
  void ForEachInput(Function&& f) const;

  inline void UnwrapIdentities();

 private:
#ifdef DEBUG
  bool IsConsideredForResultLocation() const {
    switch (top_frame().type()) {
      case DeoptFrame::FrameType::kInterpretedFrame:
        // Interpreted frames obviously need a result location.
        return true;
      case DeoptFrame::FrameType::kInlinedArgumentsFrame:
      case DeoptFrame::FrameType::kConstructInvokeStubFrame:
        return false;
      case DeoptFrame::FrameType::kBuiltinContinuationFrame:
        // Normally if the function is going to be deoptimized then the top
        // frame should be an interpreted one, except for LazyDeoptContinuation
        // builtin.
        switch (top_frame().as_builtin_continuation().builtin_id()) {
          case Builtin::kGenericLazyDeoptContinuation:
          case Builtin::kGetIteratorWithFeedbackLazyDeoptContinuation:
          case Builtin::kCallIteratorWithFeedbackLazyDeoptContinuation:
            return true;
          default:
            return false;
        }
    }
  }
#endif  // DEBUG

  using DeoptingCallReturnPcField = base::BitField<unsigned int, 0, 30>;
  using ResultSizeField = DeoptingCallReturnPcField::Next<unsigned int, 2>;

  // The max code size is enforced by the various assemblers, but it's not
  // visible here, so static assert against the magic constant that we happen
  // to know is correct.
  static constexpr int kMaxCodeSize = 512 * MB;
  static constexpr unsigned int kUninitializedCallReturnPc =
      DeoptingCallReturnPcField::kMax;
  static_assert(DeoptingCallReturnPcField::is_valid(kMaxCodeSize));
  static_assert(kMaxCodeSize != kUninitializedCallReturnPc);

  // Lazy deopts can have at most two result registers -- temporarily three for
  // ForInPrepare.
  static_assert(ResultSizeField::kMax >= 3);

  interpreter::Register result_location_;
  uint32_t bitfield_;
};

class ExceptionHandlerInfo {
 public:
  using List = base::ThreadedList<ExceptionHandlerInfo>;
  enum Mode {
    kNoExceptionHandler = -1,
    kLazyDeopt = -2,
  };

  explicit ExceptionHandlerInfo(Mode mode = kNoExceptionHandler)
      : catch_block_(), depth_(static_cast<int>(mode)), pc_offset_(-1) {}

  ExceptionHandlerInfo(BasicBlockRef* catch_block_ref, int depth)
      : catch_block_(catch_block_ref), depth_(depth), pc_offset_(-1) {
    DCHECK_NE(depth, kNoExceptionHandler);
    DCHECK_NE(depth, kLazyDeopt);
  }

  ExceptionHandlerInfo(BasicBlock* catch_block_ref, int depth)
      : catch_block_(catch_block_ref), depth_(depth), pc_offset_(-1) {}

  bool HasExceptionHandler() const { return depth_ != kNoExceptionHandler; }

  bool ShouldLazyDeopt() const { return depth_ == kLazyDeopt; }

  Label& trampoline_entry() { return trampoline_entry_; }

  BasicBlockRef* catch_block_ref_address() { return &catch_block_; }

  BasicBlock* catch_block() const { return catch_block_.block_ptr(); }

  int depth() const {
    DCHECK_NE(depth_, kNoExceptionHandler);
    DCHECK_NE(depth_, kLazyDeopt);
    return depth_;
  }

  int pc_offset() const { return pc_offset_; }
  void set_pc_offset(int offset) {
    DCHECK_EQ(pc_offset_, -1);
    DCHECK_NE(offset, -1);
    pc_offset_ = offset;
  }

 private:
  BasicBlockRef catch_block_;
  Label trampoline_entry_;
  int depth_;
  int pc_offset_;

  ExceptionHandlerInfo* next_ = nullptr;
  ExceptionHandlerInfo** next() { return &next_; }

  friend List;
  friend base::ThreadedListTraits<ExceptionHandlerInfo>;
};

// Dummy type for the initial raw allocation.
struct NodeWithInlineInputs {};

namespace detail {
// Helper for getting the static opcode of a Node subclass. This is in a
// "detail" namespace rather than in NodeBase because we can't template
// specialize outside of namespace scopes before C++17.
template <class T>
struct opcode_of_helper;

#define DEF_OPCODE_OF(Name)                          \
  template <>                                        \
  struct opcode_of_helper<Name> {                    \
    static constexpr Opcode value = Opcode::k##Name; \
  };
NODE_BASE_LIST(DEF_OPCODE_OF)
#undef DEF_OPCODE_OF

template <typename T>
constexpr T* ObjectPtrBeforeAddress(void* address) {
  char* address_as_char_ptr = reinterpret_cast<char*>(address);
  char* object_ptr_as_char_ptr = address_as_char_ptr - sizeof(T);
  return reinterpret_cast<T*>(object_ptr_as_char_ptr);
}

template <typename T>
constexpr const T* ObjectPtrBeforeAddress(const void* address) {
  const char* address_as_char_ptr = reinterpret_cast<const char*>(address);
  const char* object_ptr_as_char_ptr = address_as_char_ptr - sizeof(T);
  return reinterpret_cast<const T*>(object_ptr_as_char_ptr);
}

}  // namespace detail

#define DEOPTIMIZE_REASON_FIELD                                             \
 private:                                                                   \
  using ReasonField =                                                       \
      NextBitField<DeoptimizeReason, base::bits::WhichPowerOfTwo<size_t>(   \
                                         base::bits::RoundUpToPowerOfTwo32( \
                                             kDeoptimizeReasonCount))>;     \
                                                                            \
 public:                                                                    \
  DeoptimizeReason deoptimize_reason() const {                              \
    return ReasonField::decode(bitfield());                                 \
  }

class KnownNodeAspects;
class NodeBase : public ZoneObject {
 private:
  // Bitfield specification.
  // [opcode:16][input_count:16][properties:16][extras:16]
  using OpcodeField = base::BitField64<Opcode, 0, 16>;
  static_assert(OpcodeField::is_valid(kLastOpcode));
  using InputCountField = OpcodeField::Next<size_t, 16>;
  using OpPropertiesField =
      InputCountField::Next<OpProperties, OpProperties::kSize>;
  using NumTemporariesNeededField = OpPropertiesField::Next<uint8_t, 2>;
  using NumDoubleTemporariesNeededField =
      NumTemporariesNeededField::Next<uint8_t, 1>;

 protected:
  // Reserved for intermediate superclasses such as ValueNode.
  using LastNodeBaseField = NumDoubleTemporariesNeededField;
  using ReservedFields = LastNodeBaseField::Next<uint8_t, 2>;
  // Subclasses may use the remaining bitfield bits.
  template <class T, int size>
  using NextBitField = ReservedFields::Next<T, size>;

 public:
  static constexpr int kMaxInputs = InputCountField::kMax;

  template <class T>
  static constexpr Opcode opcode_of = detail::opcode_of_helper<T>::value;

  template <class Derived, typename... Args>
  static Derived* New(Zone* zone, std::initializer_list<ValueNode*> inputs,
                      Args&&... args) {
    static_assert(Derived::kProperties.is_conversion(),
                  "This method does not implicitly convert input types. Use "
                  "MaglevGraphBuilder::AddNewNode instead or NodeBase::New and "
                  "initialize and convert inputs manually.");
    Derived* node =
        Allocate<Derived>(zone, inputs.size(), std::forward<Args>(args)...);

    int i = 0;
    for (ValueNode* input : inputs) {
      DCHECK_NOT_NULL(input);
      node->set_input(i++, input);
    }

    return node;
  }

  // Inputs must be initialized manually.
  template <class Derived, typename... Args>
  static Derived* New(Zone* zone, size_t input_count, Args&&... args) {
    Derived* node =
        Allocate<Derived>(zone, input_count, std::forward<Args>(args)...);
    return node;
  }

  // Overwritten by subclasses.
  static constexpr OpProperties kProperties =
      OpProperties::Pure() | OpProperties::TaggedValue();

  constexpr Opcode opcode() const { return OpcodeField::decode(bitfield_); }
  constexpr OpProperties properties() const {
    return OpPropertiesField::decode(bitfield_);
  }
  void set_properties(OpProperties properties) {
    bitfield_ = OpPropertiesField::update(bitfield_, properties);
  }

  template <class T>
  constexpr bool Is() const;

  template <class T>
  constexpr T* Cast() {
    DCHECK(Is<T>());
    return static_cast<T*>(this);
  }
  template <class T>
  constexpr const T* Cast() const {
    DCHECK(Is<T>());
    return static_cast<const T*>(this);
  }
  template <class T>
  constexpr T* TryCast() {
    return Is<T>() ? static_cast<T*>(this) : nullptr;
  }

  template <class T>
  constexpr const T* TryCast() const {
    return Is<T>() ? static_cast<const T*>(this) : nullptr;
  }

  constexpr bool has_inputs() const { return input_count() > 0; }
  constexpr int input_count() const {
    static_assert(InputCountField::kMax <= kMaxInt);
    return static_cast<int>(InputCountField::decode(bitfield_));
  }

  inline void initialize_input_null(int index);
  inline void set_input(int index, ValueNode* node);
  inline void change_input(int index, ValueNode* node);
  inline void clear_input(int index);
  inline void move_input(int to, int from);  // It does not update use counts.
  ValueNode* input_node(int index) { return *input_ptr(index); }
  const ValueNode* input_node(int index) const { return *input_ptr(index); }
  InputLocation* input_location(int index) {
    DCHECK_EQ(state_, kRegallocInfo);
    return regalloc_info_->input_location(index);
  }
  const InputLocation* input_location(int index) const {
    DCHECK_EQ(state_, kRegallocInfo);
    return regalloc_info_->input_location(index);
  }
  Input input(int index) { return Input(this, index); }
  ConstInput input(int index) const { return ConstInput(this, index); }

  std::optional<int32_t> TryGetInt32ConstantInput(int index);

  // Input iterators, use like:
  //
  //  for (Input input : node->inputs()) { ... }
  auto inputs() {
    return std::views::transform(std::views::iota(0, input_count()),
                                 [&](int i) { return input(i); });
  }

  RegallocNodeInfo* regalloc_info() const {
    DCHECK_EQ(state_, kRegallocInfo);
    return regalloc_info_;
  }
  void set_regalloc_info(RegallocNodeInfo* info) {
    DCHECK(state_ == kNull || state_ == kOwner);
#ifdef DEBUG
    state_ = kRegallocInfo;
#endif  // DEBUG
    regalloc_info_ = info;
  }

  bool has_id() const {
    DCHECK_EQ(state_, kRegallocInfo);
    return regalloc_info_->id() != kInvalidNodeId;
  }
  NodeIdT id() const {
    DCHECK_EQ(state_, kRegallocInfo);
    return regalloc_info()->id();
  }

  template <typename RegisterT>
  uint8_t num_temporaries_needed() const {
    if constexpr (std::is_same_v<RegisterT, Register>) {
      return NumTemporariesNeededField::decode(bitfield_);
    } else {
      return NumDoubleTemporariesNeededField::decode(bitfield_);
    }
  }

  enum class InputAllocationPolicy { kFixedRegister, kArbitraryRegister, kAny };

  // Some parts of Maglev require a specific iteration order of the inputs (such
  // as UseMarkingProcessor::MarkInputUses or
  // StraightForwardRegisterAllocator::AssignInputs). For such cases,
  // `ForAllInputsInRegallocAssignmentOrder` can be called with a callback `f`
  // that will be called for each input in the "correct" order.
  template <typename Function>
  void ForAllInputsInRegallocAssignmentOrder(Function&& f);

  void Print(std::ostream& os, bool has_regalloc_data = false,
             bool skip_targets = false) const;

  // For GDB: Print any Node with `print node->Print()`.
  void Print() const;

  EagerDeoptInfo* eager_deopt_info() {
    DCHECK(properties().can_eager_deopt() ||
           properties().is_deopt_checkpoint());
    DCHECK(!properties().can_lazy_deopt());
    return reinterpret_cast<EagerDeoptInfo*>(deopt_info_address());
  }

  LazyDeoptInfo* lazy_deopt_info() {
    DCHECK(properties().can_lazy_deopt());
    DCHECK(!properties().can_eager_deopt());
    return reinterpret_cast<LazyDeoptInfo*>(deopt_info_address());
  }

  const RegisterSnapshot& register_snapshot() const {
    DCHECK(properties().needs_register_snapshot());
    return *reinterpret_cast<RegisterSnapshot*>(register_snapshot_address());
  }

  ExceptionHandlerInfo* exception_handler_info() {
    DCHECK(properties().can_throw());
    return reinterpret_cast<ExceptionHandlerInfo*>(exception_handler_address());
  }

  void set_register_snapshot(RegisterSnapshot snapshot) {
    DCHECK(properties().needs_register_snapshot());
    *reinterpret_cast<RegisterSnapshot*>(register_snapshot_address()) =
        snapshot;
  }

  void change_representation(ValueRepresentation new_repr) {
    DCHECK_EQ(opcode(), Opcode::kPhi);
    bitfield_ = OpPropertiesField::update(
        bitfield_, properties().WithNewValueRepresentation(new_repr));
  }

  void set_opcode(Opcode new_opcode) {
    bitfield_ = OpcodeField::update(bitfield_, new_opcode);
  }

  void CopyEagerDeoptInfoOf(NodeBase* other, Zone* zone) {
    new (eager_deopt_info()) EagerDeoptInfo(
        zone, zone->New<DeoptFrame>(other->eager_deopt_info()->top_frame()),
        other->eager_deopt_info()->feedback_to_update());
  }

  void SetEagerDeoptInfo(Zone* zone, DeoptFrame* deopt_frame,
                         compiler::FeedbackSource feedback_to_update =
                             compiler::FeedbackSource()) {
    DCHECK(properties().can_eager_deopt() ||
           properties().is_deopt_checkpoint());
    new (eager_deopt_info())
        EagerDeoptInfo(zone, deopt_frame, feedback_to_update);
  }

  template <typename NodeT>
  void OverwriteWith() {
    OverwriteWith(NodeBase::opcode_of<NodeT>, NodeT::kProperties);
  }

  void OverwriteWith(
      Opcode new_opcode,
      std::optional<OpProperties> maybe_new_properties = std::nullopt) {
    OpProperties new_properties = maybe_new_properties.has_value()
                                      ? maybe_new_properties.value()
                                      : StaticPropertiesForOpcode(new_opcode);
#ifdef DEBUG
    CheckCanOverwriteWith(new_opcode, new_properties);
#endif
    set_opcode(new_opcode);
    set_properties(new_properties);
  }

  inline void UnwrapDeoptFrames();
  inline void OverwriteWithIdentityTo(ValueNode* node);
  inline void OverwriteWithReturnValue(ValueNode* node);

  auto options() const { return std::tuple{}; }

  void ClearUnstableNodeAspects(bool is_tracing_enabled, KnownNodeAspects&);
  void ClearElementsProperties(bool is_tracing_enabled, KnownNodeAspects&);

  void set_owner(BasicBlock* block) {
    DCHECK(state_ == kNull || state_ == kOwner);
#ifdef DEBUG
    state_ = kOwner;
#endif  // DEBUG
    owner_ = block;
  }
  BasicBlock* owner() const {
    DCHECK_EQ(state_, kOwner);
    return owner_;
  }

 protected:
  explicit NodeBase(uint64_t bitfield) : bitfield_(bitfield) {}

  union {
    BasicBlock* owner_ = nullptr;      // Valid in the frontend processors.
    RegallocNodeInfo* regalloc_info_;  // Valid in the backend processors.
  };

#ifdef DEBUG
  enum State {
    kNull,
    kOwner,
    kRegallocInfo,
  };
  State state_ = kNull;
#endif  // DEBUG

  // Allow updating bits above NextBitField from subclasses
  constexpr uint64_t bitfield() const { return bitfield_; }
  void set_bitfield(uint64_t new_bitfield) {
#ifdef DEBUG
    // Make sure that all the base bitfield bits (all bits before the next
    // bitfield start, excluding any spare bits) are equal in the new value.
    const uint64_t base_bitfield_mask =
        ((uint64_t{1} << NextBitField<bool, 1>::kShift) - 1) &
        ~ReservedFields::kMask;
    DCHECK_EQ(bitfield_ & base_bitfield_mask,
              new_bitfield & base_bitfield_mask);
#endif
    bitfield_ = new_bitfield;
  }

  // For nodes that don't have data past the input, allow trimming the input
  // count. This is used by Phis to reduce inputs when merging in dead control
  // flow.
  void reduce_input_count(int num = 1) {
    DCHECK_EQ(opcode(), Opcode::kPhi);
    DCHECK_GE(input_count(), num);
    DCHECK(!properties().can_lazy_deopt());
    DCHECK(!properties().can_eager_deopt());
    bitfield_ = InputCountField::update(bitfield_, input_count() - num);
  }

  // Specify that there need to be a certain number of registers free (i.e.
  // usable as scratch registers) on entry into this node.
  //
  // Does not include any registers requested by RequireSpecificTemporary.
  void set_temporaries_needed(uint8_t value) {
    DCHECK_EQ(num_temporaries_needed<Register>(), 0);
    bitfield_ = NumTemporariesNeededField::update(bitfield_, value);
  }

  void set_double_temporaries_needed(uint8_t value) {
    DCHECK_EQ(num_temporaries_needed<DoubleRegister>(), 0);
    bitfield_ = NumDoubleTemporariesNeededField::update(bitfield_, value);
  }

  // Require that a specific register is free (and therefore clobberable) by the
  // entry into this node.
  void RequireSpecificTemporary(Register reg) {
    DCHECK_EQ(state_, kRegallocInfo);
    regalloc_info()->general_temporaries().set(reg);
  }

  void RequireSpecificDoubleTemporary(DoubleRegister reg) {
    DCHECK_EQ(state_, kRegallocInfo);
    regalloc_info()->double_temporaries().set(reg);
  }

 private:
  constexpr ValueNode** input_base() {
    return detail::ObjectPtrBeforeAddress<ValueNode*>(this);
  }
  constexpr ValueNode* const* input_base() const {
    return detail::ObjectPtrBeforeAddress<ValueNode*>(this);
  }

  ValueNode** input_ptr(int index) {
    DCHECK_LT(index, input_count());
    return input_base() - index;
  }

  ValueNode* const* input_ptr(int index) const {
    DCHECK_LT(index, input_count());
    return input_base() - index;
  }

  Address last_input_address() const {
    return reinterpret_cast<Address>(input_ptr(input_count() - 1));
  }

  template <class Derived, typename... Args>
  static Derived* Allocate(Zone* zone, size_t input_count, Args&&... args) {
    static_assert(
        !Derived::kProperties.can_eager_deopt() ||
            !Derived::kProperties.can_lazy_deopt(),
        "The current deopt info representation, at the end of inputs, requires "
        "that we cannot have both lazy and eager deopts on a node. If we ever "
        "need this, we have to update accessors to check node->properties() "
        "for which deopts are active.");
    constexpr size_t size_before_inputs =
        ExceptionHandlerInfoSize(Derived::kProperties) +
        RegisterSnapshotSize(Derived::kProperties) +
        EagerDeoptInfoSize(Derived::kProperties) +
        LazyDeoptInfoSize(Derived::kProperties);

    static_assert(IsAligned(size_before_inputs, alignof(ValueNode*)));
    size_t size_before_node =
        size_before_inputs + input_count * sizeof(ValueNode*);

    if constexpr (Is64()) {
      DCHECK(IsAligned(size_before_node, alignof(Derived)));
    } else {
      size_before_node = RoundUp<alignof(Derived)>(size_before_node);
    }
    const size_t size = size_before_node + sizeof(Derived);
    intptr_t raw_buffer =
        reinterpret_cast<intptr_t>(zone->Allocate<NodeWithInlineInputs>(size));
#ifdef DEBUG
    memset(reinterpret_cast<void*>(raw_buffer), 0, size);
#endif

    void* node_buffer = reinterpret_cast<void*>(raw_buffer + size_before_node);
    uint64_t bitfield = OpcodeField::encode(opcode_of<Derived>) |
                        OpPropertiesField::encode(Derived::kProperties) |
                        InputCountField::encode(input_count);
    Derived* node =
        new (node_buffer) Derived(bitfield, std::forward<Args>(args)...);
    return node;
  }

  static constexpr size_t ExceptionHandlerInfoSize(OpProperties properties) {
    return RoundUp<alignof(ValueNode*)>(
        properties.can_throw() ? sizeof(ExceptionHandlerInfo) : 0);
  }

  static constexpr size_t RegisterSnapshotSize(OpProperties properties) {
    return RoundUp<alignof(ValueNode*)>(
        properties.needs_register_snapshot() ? sizeof(RegisterSnapshot) : 0);
  }

  static constexpr size_t EagerDeoptInfoSize(OpProperties properties) {
    return RoundUp<alignof(ValueNode*)>(
        (properties.can_eager_deopt() || properties.is_deopt_checkpoint())
            ? sizeof(EagerDeoptInfo)
            : 0);
  }

  static constexpr size_t LazyDeoptInfoSize(OpProperties properties) {
    return RoundUp<alignof(ValueNode*)>(
        properties.can_lazy_deopt() ? sizeof(LazyDeoptInfo) : 0);
  }

  // Returns the position of deopt info if it exists, otherwise returns
  // its position as if DeoptInfo size were zero.
  Address deopt_info_address() const {
    DCHECK(!properties().can_eager_deopt() || !properties().can_lazy_deopt());
    size_t extra =
        EagerDeoptInfoSize(properties()) + LazyDeoptInfoSize(properties());
    return last_input_address() - extra;
  }

  // Returns the position of register snapshot if it exists, otherwise returns
  // its position as if RegisterSnapshot size were zero.
  Address register_snapshot_address() const {
    size_t extra = RegisterSnapshotSize(properties());
    return deopt_info_address() - extra;
  }

  // Returns the position of exception handler info if it exists, otherwise
  // returns its position as if ExceptionHandlerInfo size were zero.
  Address exception_handler_address() const {
    size_t extra = ExceptionHandlerInfoSize(properties());
    return register_snapshot_address() - extra;
  }

  void CheckCanOverwriteWith(Opcode new_opcode, OpProperties new_properties);

  uint64_t bitfield_;

  NodeBase() = delete;
  NodeBase(const NodeBase&) = delete;
  NodeBase(NodeBase&&) = delete;
  NodeBase& operator=(const NodeBase&) = delete;
  NodeBase& operator=(NodeBase&&) = delete;
};

ValueNode* Input::node() const { return base_->input_node(index_); }
InputLocation* Input::location() const { return base_->input_location(index_); }
void Input::clear() { base_->clear_input(index_); }

const ValueNode* ConstInput::node() const { return base_->input_node(index_); }
const InputLocation* ConstInput::location() const {
  return base_->input_location(index_);
}

template <class T>
constexpr bool NodeBase::Is() const {
  return opcode() == opcode_of<T>;
}

// Specialized sub-hierarchy type checks.
template <>
constexpr bool NodeBase::Is<ValueNode>() const {
  return IsValueNode(opcode());
}
template <>
constexpr bool NodeBase::Is<ControlNode>() const {
  return IsControlNode(opcode());
}
template <>
constexpr bool NodeBase::Is<BranchControlNode>() const {
  return IsBranchControlNode(opcode());
}
template <>
constexpr bool NodeBase::Is<ConditionalControlNode>() const {
  return IsConditionalControlNode(opcode());
}
template <>
constexpr bool NodeBase::Is<UnconditionalControlNode>() const {
  return IsUnconditionalControlNode(opcode());
}
template <>
constexpr bool NodeBase::Is<TerminalControlNode>() const {
  return IsTerminalControlNode(opcode());
}

void CheckValueInputIs(const NodeBase* node, int i,
                       ValueRepresentation expected);

// The Node class hierarchy contains all non-control nodes.
class Node : public NodeBase {
 public:
  inline ValueLocation& result();

  static constexpr bool participate_in_cse(Opcode op) {
    return StaticPropertiesForOpcode(op).can_participate_in_cse() &&
           !IsConstantNode(op) && !IsControlNode(op) && !IsZeroCostNode(op) &&
           // The following are already precisely tracked by known_node_aspects
           // and tracking them with CSE would just waste time.
           op != Opcode::kCheckMaps;
  }

  static constexpr bool needs_epoch_check(Opcode op) {
    return StaticPropertiesForOpcode(op).can_read();
  }

 protected:
  using NodeBase::NodeBase;
};

// All non-control nodes with a result.
class alignas(8) ValueNode : public Node {
 private:
  using TaggedResultNeedsDecompressField =
      NodeBase::LastNodeBaseField::Next<bool, 1>;
  using CanTruncateToInt32Field =
      TaggedResultNeedsDecompressField::Next<bool, 1>;
  static_assert(CanTruncateToInt32Field::kShift +
                    CanTruncateToInt32Field::kSize ==
                ReservedFields::kShift + ReservedFields::kSize);

 protected:
  using LastNodeBaseField = void;
  using ReservedFields = void;

 public:
  int use_count() const {
    DCHECK(!unused_inputs_were_visited());
    return use_count_;
  }
  bool is_used() const { return use_count_ > 0; }
  bool unused_inputs_were_visited() const { return use_count_ == -1; }
  void add_use() {
    // Make sure a saturated use count won't overflow.
    DCHECK_LT(use_count_, kMaxInt);
    use_count_++;
  }
  inline void remove_use();
  // Avoid revisiting nodes when processing an unused node's inputs, by marking
  // it as visited.
  void mark_unused_inputs_visited() {
    DCHECK_EQ(use_count_, 0);
    use_count_ = -1;
  }

  // Used by the register allocator. Only available at the backend.
  void SetHint(compiler::InstructionOperand hint);

  /* For constants only. */
  void LoadToRegister(MaglevAssembler*, Register) const;
  void LoadToRegister(MaglevAssembler*, DoubleRegister) const;
  void DoLoadToRegister(MaglevAssembler*, Register) const;
  void DoLoadToRegister(MaglevAssembler*, DoubleRegister) const;
  DirectHandle<Object> Reify(LocalIsolate* isolate) const;

  bool has_valid_live_range() const {
    DCHECK_EQ(state_, kRegallocInfo);
    return regalloc_info()->has_valid_live_range();
  }
  LiveRange live_range() const {
    DCHECK_EQ(state_, kRegallocInfo);
    DCHECK(has_valid_live_range());
    return regalloc_info()->live_range();
  }

  constexpr bool use_double_register() const {
    return IsDoubleRepresentation(properties().value_representation());
  }

  constexpr bool is_tagged() const {
    return (properties().value_representation() ==
            ValueRepresentation::kTagged);
  }
  constexpr bool is_int32() const {
    return (properties().value_representation() == ValueRepresentation::kInt32);
  }
  constexpr bool is_uint32() const {
    return (properties().value_representation() ==
            ValueRepresentation::kUint32);
  }
  constexpr bool is_float64() const {
    return (properties().value_representation() ==
            ValueRepresentation::kFloat64);
  }
  constexpr bool is_holey_float64() const {
    return (properties().value_representation() ==
            ValueRepresentation::kHoleyFloat64);
  }
  constexpr bool is_float64_or_holey_float64() const {
    return is_float64() || is_holey_float64();
  }

#ifdef V8_COMPRESS_POINTERS
  constexpr bool decompresses_tagged_result() const {
    return TaggedResultNeedsDecompressField::decode(bitfield());
  }

  void SetTaggedResultNeedsDecompress() {
    static_assert(PointerCompressionIsEnabled());

    DCHECK_IMPLIES(!Is<Identity>(), is_tagged());
    DCHECK_IMPLIES(Is<Identity>(), input(0).node()->is_tagged());
    set_bitfield(TaggedResultNeedsDecompressField::update(bitfield(), true));
    if (Is<Phi>()) {
      for (Input input : inputs()) {
        // Avoid endless recursion by terminating on values already marked.
        if (input.node()->decompresses_tagged_result()) continue;
        input.node()->SetTaggedResultNeedsDecompress();
      }
    } else if (Is<Identity>()) {
      DCHECK_EQ(input_count(), 0);
      input(0).node()->SetTaggedResultNeedsDecompress();
    }
  }
#else
  constexpr bool decompresses_tagged_result() const { return false; }
#endif

  constexpr bool can_truncate_to_int32() const {
    return CanTruncateToInt32Field::decode(bitfield());
  }

  void set_can_truncate_to_int32(bool value) {
    set_bitfield(CanTruncateToInt32Field::update(bitfield(), value));
  }

  constexpr ValueRepresentation value_representation() const {
    return properties().value_representation();
  }

  constexpr MachineRepresentation GetMachineRepresentation() const {
    switch (properties().value_representation()) {
      case ValueRepresentation::kTagged:
        return MachineRepresentation::kTagged;
      case ValueRepresentation::kInt32:
      case ValueRepresentation::kUint32:
        return MachineRepresentation::kWord32;
      case ValueRepresentation::kIntPtr:
        return MachineType::PointerRepresentation();
      case ValueRepresentation::kFloat64:
        return MachineRepresentation::kFloat64;
      case ValueRepresentation::kHoleyFloat64:
        return MachineRepresentation::kFloat64;
      case ValueRepresentation::kNone:
        UNREACHABLE();
    }
  }

  RangeType GetRange() const;

  compiler::OptionalHeapObjectRef TryGetConstant(
      compiler::JSHeapBroker* broker);

  NodeType GetStaticType(compiler::JSHeapBroker* broker);

  bool StaticTypeIs(compiler::JSHeapBroker* broker, NodeType type) {
    return NodeTypeIs(GetStaticType(broker), type);
  }

  inline void MaybeRecordUseReprHint(UseRepresentationSet repr);
  inline void MaybeRecordUseReprHint(UseRepresentation repr);

  ValueNode* UnwrapIdentities();
  const ValueNode* UnwrapIdentities() const;

  // Unwrap identities and conversions.
  ValueNode* Unwrap();

  RegallocValueNodeInfo* regalloc_info() const {
    DCHECK_EQ(state_, kRegallocInfo);
    return static_cast<RegallocValueNodeInfo*>(regalloc_info_);
  }

 protected:
  explicit ValueNode(uint64_t bitfield) : Node(bitfield), use_count_(0) {}
  int use_count_;
};

inline void NodeBase::initialize_input_null(int index) {
  // Should already be null in debug, make sure it's null on release too.
  DCHECK_EQ(input(index).node(), nullptr);
  *input_ptr(index) = nullptr;
}

inline void NodeBase::set_input(int index, ValueNode* node) {
  DCHECK_NOT_NULL(node);
  DCHECK_NULL(input(index).node());
  node->add_use();
  *input_ptr(index) = node;
}

void NodeBase::change_input(int index, ValueNode* node) {
  DCHECK_NE(input(index).node(), nullptr);
  if (input(index).node() == node) return;
  // After the AnyUseMarkingProcessor the use count can be -1.
  if (input(index).node()->is_used()) {
    input(index).node()->remove_use();
  } else {
    DCHECK_EQ(input(index).node()->use_count(), -1);
  }

#ifdef DEBUG
  *input_ptr(index) = nullptr;
#endif
  set_input(index, node);
}

inline void NodeBase::clear_input(int index) {
  DCHECK_NOT_NULL(input_node(index));
  input_node(index)->remove_use();
  *input_ptr(index) = nullptr;
}

inline void NodeBase::move_input(int to, int from) {
  DCHECK_LT(from, input_count());
  DCHECK_LT(to, input_count());
  *input_ptr(to) = input_node(from);
}

ValueLocation& Node::result() {
  DCHECK(Is<ValueNode>());
  DCHECK_EQ(state_, kRegallocInfo);
  return Cast<ValueNode>()->regalloc_info()->result();
}

inline ValueNode* ValueNode::UnwrapIdentities() {
  ValueNode* node = this;
  while (node->Is<Identity>()) node = node->input(0).node();
  return node;
}

inline const ValueNode* ValueNode::UnwrapIdentities() const {
  const ValueNode* node = this;
  while (node->Is<Identity>()) node = node->input(0).node();
  return node;
}

inline ValueNode* ValueNode::Unwrap() {
  ValueNode* node = this;
  while (node->Is<Identity>() || node->properties().is_conversion()) {
    node = node->input(0).node();
  }
  return node;
}

// Mixin for a node with known class (and therefore known opcode and static
// properties), but possibly unknown numbers of inputs.
template <typename Base, typename Derived>
class NodeTMixin : public Base {
 public:
  // Shadowing for static knowledge.
  constexpr Opcode opcode() const { return NodeBase::opcode_of<Derived>; }
  constexpr const OpProperties& properties() const {
    return Derived::kProperties;
  }

  template <typename... Args>
  static Derived* New(Zone* zone, std::initializer_list<ValueNode*> inputs,
                      Args&&... args) {
    return NodeBase::New<Derived>(zone, inputs, std::forward<Args>...);
  }
  template <typename... Args>
  static Derived* New(Zone* zone, size_t input_count, Args&&... args) {
    return NodeBase::New<Derived>(zone, input_count, std::forward<Args>...);
  }

 protected:
  template <typename... Args>
  explicit NodeTMixin(uint64_t bitfield, Args&&... args)
      : Base(bitfield, std::forward<Args>(args)...) {
    DCHECK_EQ(this->NodeBase::opcode(), NodeBase::opcode_of<Derived>);
    DCHECK_EQ(this->NodeBase::properties(), Derived::kProperties);
  }
};

namespace detail {
// Helper class for defining input types as a std::array, but without
// accidental initialisation with the wrong sized initializer_list.
template <size_t Size>
class ArrayWrapper : public std::array<ValueRepresentation, Size> {
 public:
  template <typename... Args>
  explicit constexpr ArrayWrapper(Args&&... args)
      : std::array<ValueRepresentation, Size>({args...}) {
    static_assert(sizeof...(args) == Size);
  }
};
struct YouNeedToDefineAnInputTypesArrayInYourDerivedClass {};
}  // namespace detail

// Mixin for a node with known class (and therefore known opcode and static
// properties), and known numbers of inputs.
template <size_t InputCount, typename Base, typename Derived>
class FixedInputNodeTMixin : public NodeTMixin<Base, Derived> {
 public:
  static constexpr size_t kInputCount = InputCount;

  // Shadowing for static knowledge.
  constexpr bool has_inputs() const { return input_count() > 0; }
  constexpr uint16_t input_count() const { return kInputCount; }
  constexpr auto end() {
    return std::make_reverse_iterator(&this->input(input_count() - 1));
  }

  void VerifyInputs() const {
    if constexpr (kInputCount != 0) {
      static_assert(
          std::is_same_v<const InputTypes, decltype(Derived::kInputTypes)>);
      static_assert(kInputCount == Derived::kInputTypes.size());
      for (int i = 0; i < static_cast<int>(kInputCount); ++i) {
        CheckValueInputIs(this, i, Derived::kInputTypes[i]);
      }
    }
  }

#ifdef V8_COMPRESS_POINTERS
  void MarkTaggedInputsAsDecompressing() {
    if constexpr (kInputCount != 0) {
      static_assert(
          std::is_same_v<const InputTypes, decltype(Derived::kInputTypes)>);
      static_assert(kInputCount == Derived::kInputTypes.size());
      for (int i = 0; i < static_cast<int>(kInputCount); ++i) {
        if (Derived::kInputTypes[i] == ValueRepresentation::kTagged) {
          ValueNode* input_node = this->input(i).node();
          input_node->SetTaggedResultNeedsDecompress();
        }
      }
    }
  }
#endif

 protected:
  using InputTypes = detail::ArrayWrapper<kInputCount>;
  static constexpr detail::YouNeedToDefineAnInputTypesArrayInYourDerivedClass
      kInputTypes = {};

  template <typename... Args>
  explicit FixedInputNodeTMixin(uint64_t bitfield, Args&&... args)
      : NodeTMixin<Base, Derived>(bitfield, std::forward<Args>(args)...) {
    DCHECK_EQ(this->NodeBase::input_count(), kInputCount);
  }
};

template <class T, class = void>
struct IsFixedInputNode : public std::false_type {};
template <class T>
struct IsFixedInputNode<T, std::void_t<decltype(T::kInputCount)>>
    : public std::true_type {};

template <class Derived>
using NodeT = NodeTMixin<Node, Derived>;

template <class Derived>
using ValueNodeT = NodeTMixin<ValueNode, Derived>;

template <size_t InputCount, class Derived>
using FixedInputNodeT =
    FixedInputNodeTMixin<InputCount, NodeT<Derived>, Derived>;

template <size_t InputCount, class Derived>
using FixedInputValueNodeT =
    FixedInputNodeTMixin<InputCount, ValueNodeT<Derived>, Derived>;

class Identity : public FixedInputValueNodeT<1, Identity> {
  using Base = FixedInputValueNodeT<1, Identity>;

 public:
  static constexpr OpProperties kProperties =
      OpProperties::Pure() |
      OpProperties::ForValueRepresentation(ValueRepresentation::kNone);

  explicit Identity(uint64_t bitfield) : Base(bitfield) {}

  void VerifyInputs() const {
    // Identity is valid for all input types.
  }
#ifdef V8_COMPRESS_POINTERS
  void MarkTaggedInputsAsDecompressing() {
    // Do not mark inputs as decompressing here, since we don't yet know whether
    // this Phi needs decompression. Instead, let
    // Node::SetTaggedResultNeedsDecompress pass through phis.
  }
#endif
  void SetValueLocationConstraints() { UNREACHABLE(); }
  void GenerateCode(MaglevAssembler*, const ProcessingState&) { UNREACHABLE(); }
  void PrintParams(std::ostream&) const {}
};

template <class Derived, Operation kOperation>
class UnaryWithFeedbackNode : public FixedInputValueNodeT<1, Derived> {
  using Base = FixedInputValueNodeT<1, Derived>;

 public:
  // The implementation currently calls runtime.
  static constexpr OpProperties kProperties = OpProperties::JSCall();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  static constexpr int kOperandIndex = 0;
  Input operand_input() { return Node::input(kOperandIndex); }
  compiler::FeedbackSource feedback() const { return feedback_; }

 protected:
  explicit UnaryWithFeedbackNode(uint64_t bitfield,
                                 const compiler::FeedbackSource& feedback)
      : Base(bitfield), feedback_(feedback) {}

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}

  const compiler::FeedbackSource feedback_;
};

template <class Derived, Operation kOperation>
class BinaryWithFeedbackNode : public FixedInputValueNodeT<2, Derived> {
  using Base = FixedInputValueNodeT<2, Derived>;

 public:
  // The implementation currently calls runtime.
  static constexpr OpProperties kProperties = OpProperties::JSCall();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged};

  static constexpr int kLeftIndex = 0;
  static constexpr int kRightIndex = 1;
  Input left_input() { return Node::input(kLeftIndex); }
  Input right_input() { return Node::input(kRightIndex); }
  compiler::FeedbackSource feedback() const { return feedback_; }

 protected:
  BinaryWithFeedbackNode(uint64_t bitfield,
                         const compiler::FeedbackSource& feedback)
      : Base(bitfield), feedback_(feedback) {}

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}

  const compiler::FeedbackSource feedback_;
};

#define DEF_OPERATION_WITH_FEEDBACK_NODE(Name, Super, OpName)         \
  class Name : public Super<Name, Operation::k##OpName> {             \
    using Base = Super<Name, Operation::k##OpName>;                   \
                                                                      \
   public:                                                            \
    Name(uint64_t bitfield, const compiler::FeedbackSource& feedback) \
        : Base(bitfield, feedback) {}                                 \
    int MaxCallStackArgs() const { return 0; }                        \
    void SetValueLocationConstraints();                               \
    void GenerateCode(MaglevAssembler*, const ProcessingState&);      \
    void PrintParams(std::ostream&) const {}                          \
  };

#define DEF_UNARY_WITH_FEEDBACK_NODE(Name) \
  DEF_OPERATION_WITH_FEEDBACK_NODE(Generic##Name, UnaryWithFeedbackNode, Name)
#define DEF_BINARY_WITH_FEEDBACK_NODE(Name) \
  DEF_OPERATION_WITH_FEEDBACK_NODE(Generic##Name, BinaryWithFeedbackNode, Name)
UNARY_OPERATION_LIST(DEF_UNARY_WITH_FEEDBACK_NODE)
ARITHMETIC_OPERATION_LIST(DEF_BINARY_WITH_FEEDBACK_NODE)
COMPARISON_OPERATION_LIST(DEF_BINARY_WITH_FEEDBACK_NODE)
#undef DEF_UNARY_WITH_FEEDBACK_NODE
#undef DEF_BINARY_WITH_FEEDBACK_NODE
#undef DEF_OPERATION_WITH_FEEDBACK_NODE

template <class Derived, Operation kOperation>
class Int32BinaryWithOverflowNode : public FixedInputValueNodeT<2, Derived> {
  using Base = FixedInputValueNodeT<2, Derived>;

 public:
  static constexpr OpProperties kProperties =
      OpProperties::EagerDeopt() | OpProperties::Int32();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kInt32, ValueRepresentation::kInt32};

  static constexpr int kLeftIndex = 0;
  static constexpr int kRightIndex = 1;
  Input left_input() { return Node::input(kLeftIndex); }
  Input right_input() { return Node::input(kRightIndex); }

 protected:
  explicit Int32BinaryWithOverflowNode(uint64_t bitfield) : Base(bitfield) {}

  void PrintParams(std::ostream&) const {}
};

#define DEF_OPERATION_NODE(Name, Super, OpName)                  \
  class Name : public Super<Name, Operation::k##OpName> {        \
    using Base = Super<Name, Operation::k##OpName>;              \
                                                                 \
   public:                                                       \
    explicit Name(uint64_t bitfield) : Base(bitfield) {}         \
    void SetValueLocationConstraints();                          \
    void GenerateCode(MaglevAssembler*, const ProcessingState&); \
    void PrintParams(std::ostream&) const {}                     \
  };

#define DEF_INT32_BINARY_WITH_OVERFLOW_NODE(Name)                            \
  DEF_OPERATION_NODE(Int32##Name##WithOverflow, Int32BinaryWithOverflowNode, \
                     Name)
DEF_INT32_BINARY_WITH_OVERFLOW_NODE(Add)
DEF_INT32_BINARY_WITH_OVERFLOW_NODE(Subtract)
DEF_INT32_BINARY_WITH_OVERFLOW_NODE(Multiply)
DEF_INT32_BINARY_WITH_OVERFLOW_NODE(Divide)
DEF_INT32_BINARY_WITH_OVERFLOW_NODE(Modulus)
#undef DEF_INT32_BINARY_WITH_OVERFLOW_NODE

template <class Derived, Operation kOperation>
class Int32BinaryNode : public FixedInputValueNodeT<2, Derived> {
  using Base = FixedInputValueNodeT<2, Derived>;

 public:
  static constexpr OpProperties kProperties = OpProperties::Int32();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kInt32, ValueRepresentation::kInt32};

  static constexpr int kLeftIndex = 0;
  static constexpr int kRightIndex = 1;
  Input left_input() { return Node::input(kLeftIndex); }
  Input right_input() { return Node::input(kRightIndex); }

 protected:
  explicit Int32BinaryNode(uint64_t bitfield) : Base(bitfield) {}
};

#define DEF_INT32_BINARY_NODE(Name) \
  DEF_OPERATION_NODE(Int32##Name, Int32BinaryNode, Name)
DEF_INT32_BINARY_NODE(Add)
DEF_INT32_BINARY_NODE(Subtract)
DEF_INT32_BINARY_NODE(Multiply)
DEF_INT32_BINARY_NODE(Divide)
DEF_INT32_BINARY_NODE(BitwiseAnd)
DEF_INT32_BINARY_NODE(BitwiseOr)
DEF_INT32_BINARY_NODE(BitwiseXor)
DEF_INT32_BINARY_NODE(ShiftLeft)
DEF_INT32_BINARY_NODE(ShiftRight)
#undef DEF_INT32_BINARY_NODE

class Int32MultiplyOverflownBits
    : public FixedInputValueNodeT<2, Int32MultiplyOverflownBits> {
  using Base = FixedInputValueNodeT<2, Int32MultiplyOverflownBits>;

 public:
  explicit Int32MultiplyOverflownBits(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::Int32();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kInt32, ValueRepresentation::kInt32};

  static constexpr int kLeftIndex = 0;
  static constexpr int kRightIndex = 1;
  Input left_input() { return Node::input(kLeftIndex); }
  Input right_input() { return Node::input(kRightIndex); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class Int32BitwiseNot : public FixedInputValueNodeT<1, Int32BitwiseNot> {
  using Base = FixedInputValueNodeT<1, Int32BitwiseNot>;

 public:
  explicit Int32BitwiseNot(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::Int32();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kInt32};

  static constexpr int kValueIndex = 0;
  Input value_input() { return Node::input(kValueIndex); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

template <class Derived, Operation kOperation>
class Int32UnaryWithOverflowNode : public FixedInputValueNodeT<1, Derived> {
  using Base = FixedInputValueNodeT<1, Derived>;

 public:
  static constexpr OpProperties kProperties =
      OpProperties::EagerDeopt() | OpProperties::Int32();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kInt32};

  static constexpr int kValueIndex = 0;
  Input value_input() { return Node::input(kValueIndex); }

 protected:
  explicit Int32UnaryWithOverflowNode(uint64_t bitfield) : Base(bitfield) {}
};

#define DEF_INT32_UNARY_WITH_OVERFLOW_NODE(Name)                            \
  DEF_OPERATION_NODE(Int32##Name##WithOverflow, Int32UnaryWithOverflowNode, \
                     Name)

DEF_INT32_UNARY_WITH_OVERFLOW_NODE(Negate)
DEF_INT32_UNARY_WITH_OVERFLOW_NODE(Increment)
DEF_INT32_UNARY_WITH_OVERFLOW_NODE(Decrement)
#undef DEF_INT32_UNARY_WITH_OVERFLOW_NODE

class Int32ShiftRightLogical
    : public FixedInputValueNodeT<2, Int32ShiftRightLogical> {
  using Base = FixedInputValueNodeT<2, Int32ShiftRightLogical>;

 public:
  explicit Int32ShiftRightLogical(uint64_t bitfield) : Base(bitfield) {}

  // Unlike the other Int32 nodes, logical right shift returns a Uint32.
  static constexpr OpProperties kProperties = OpProperties::Uint32();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kInt32, ValueRepresentation::kInt32};

  static constexpr int kLeftIndex = 0;
  static constexpr int kRightIndex = 1;
  Input left_input() { return Node::input(kLeftIndex); }
  Input right_input() { return Node::input(kRightIndex); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class Int32Compare : public FixedInputValueNodeT<2, Int32Compare> {
  using Base = FixedInputValueNodeT<2, Int32Compare>;

 public:
  explicit Int32Compare(uint64_t bitfield, Operation operation)
      : Base(OperationBitField::update(bitfield, operation)) {}

  static constexpr Base::InputTypes kInputTypes{ValueRepresentation::kInt32,
                                                ValueRepresentation::kInt32};

  static constexpr int kLeftIndex = 0;
  static constexpr int kRightIndex = 1;
  Input left_input() { return Node::input(kLeftIndex); }
  Input right_input() { return Node::input(kRightIndex); }

  constexpr Operation operation() const {
    return OperationBitField::decode(bitfield());
  }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

  auto options() const { return std::tuple{operation()}; }

 private:
  using OperationBitField = NextBitField<Operation, 5>;
};

class Int32ToBoolean : public FixedInputValueNodeT<1, Int32ToBoolean> {
  using Base = FixedInputValueNodeT<1, Int32ToBoolean>;

 public:
  explicit Int32ToBoolean(uint64_t bitfield, bool flip)
      : Base(FlipBitField::update(bitfield, flip)) {}

  static constexpr Base::InputTypes kInputTypes{ValueRepresentation::kInt32};

  Input value() { return Node::input(0); }

  constexpr bool flip() const { return FlipBitField::decode(bitfield()); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

  auto options() const { return std::tuple{flip()}; }

 private:
  using FlipBitField = NextBitField<bool, 1>;
};

class IntPtrToBoolean : public FixedInputValueNodeT<1, IntPtrToBoolean> {
  using Base = FixedInputValueNodeT<1, IntPtrToBoolean>;

 public:
  explicit IntPtrToBoolean(uint64_t bitfield, bool flip)
      : Base(FlipBitField::update(bitfield, flip)) {}

  static constexpr Base::InputTypes kInputTypes{ValueRepresentation::kIntPtr};

  Input value() { return Node::input(0); }

  constexpr bool flip() const { return FlipBitField::decode(bitfield()); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

  auto options() const { return std::tuple{flip()}; }

 private:
  using FlipBitField = NextBitField<bool, 1>;
};

class CheckedSmiIncrement
    : public FixedInputValueNodeT<1, CheckedSmiIncrement> {
  using Base = FixedInputValueNodeT<1, CheckedSmiIncrement>;

 public:
  explicit CheckedSmiIncrement(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::EagerDeopt();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  static constexpr int kValueIndex = 0;
  Input value_input() { return Node::input(kValueIndex); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class CheckedSmiDecrement
    : public FixedInputValueNodeT<1, CheckedSmiDecrement> {
  using Base = FixedInputValueNodeT<1, CheckedSmiDecrement>;

 public:
  explicit CheckedSmiDecrement(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::EagerDeopt();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  static constexpr int kValueIndex = 0;
  Input value_input() { return Node::input(kValueIndex); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

template <class Derived, Operation kOperation>
class Float64BinaryNode : public FixedInputValueNodeT<2, Derived> {
  using Base = FixedInputValueNodeT<2, Derived>;

 public:
  static constexpr OpProperties kProperties = OpProperties::Float64();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kHoleyFloat64, ValueRepresentation::kHoleyFloat64};

  static constexpr int kLeftIndex = 0;
  static constexpr int kRightIndex = 1;
  Input left_input() { return Node::input(kLeftIndex); }
  Input right_input() { return Node::input(kRightIndex); }

  RangeType range() const { return range_; }
  void set_range(RangeType r) { range_ = r; }

 protected:
  explicit Float64BinaryNode(uint64_t bitfield) : Base(bitfield) {}

  void PrintParams(std::ostream&) const {}

  // TODO(victorgomes): This could be in the KNA for more use-precision.
  // However, for truncation purposes, since it depends on all uses, it is
  // simpler to store this here.
  RangeType range_ = {};
};

#define DEF_OPERATION_NODE_WITH_CALL(Name, Super, OpName)        \
  class Name : public Super<Name, Operation::k##OpName> {        \
    using Base = Super<Name, Operation::k##OpName>;              \
                                                                 \
   public:                                                       \
    explicit Name(uint64_t bitfield) : Base(bitfield) {}         \
    int MaxCallStackArgs() const;                                \
    void SetValueLocationConstraints();                          \
    void GenerateCode(MaglevAssembler*, const ProcessingState&); \
    void PrintParams(std::ostream&) const {}                     \
  };

template <class Derived, Operation kOperation>
class Float64BinaryNodeWithCall : public FixedInputValueNodeT<2, Derived> {
  using Base = FixedInputValueNodeT<2, Derived>;

 public:
  static constexpr OpProperties kProperties =
      OpProperties::Float64() | OpProperties::Call();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kHoleyFloat64, ValueRepresentation::kHoleyFloat64};

  static constexpr int kLeftIndex = 0;
  static constexpr int kRightIndex = 1;
  Input left_input() { return Node::input(kLeftIndex); }
  Input right_input() { return Node::input(kRightIndex); }

 protected:
  explicit Float64BinaryNodeWithCall(uint64_t bitfield) : Base(bitfield) {}

  void PrintParams(std::ostream&) const {}
};

#define DEF_FLOAT64_BINARY_NODE(Name) \
  DEF_OPERATION_NODE(Float64##Name, Float64BinaryNode, Name)
#define DEF_FLOAT64_BINARY_NODE_WITH_CALL(Name) \
  DEF_OPERATION_NODE_WITH_CALL(Float64##Name, Float64BinaryNodeWithCall, Name)
DEF_FLOAT64_BINARY_NODE(Add)
DEF_FLOAT64_BINARY_NODE(Subtract)
DEF_FLOAT64_BINARY_NODE(Multiply)
DEF_FLOAT64_BINARY_NODE(Divide)
#if defined(V8_TARGET_ARCH_ARM64) || defined(V8_TARGET_ARCH_ARM) || \
    defined(V8_TARGET_ARCH_RISCV64)
// On Arm/Arm64/Riscv64, floating point modulus is implemented with a call to a
// C++ function, while on x64, it's implemented natively without call.
DEF_FLOAT64_BINARY_NODE_WITH_CALL(Modulus)
#else
DEF_FLOAT64_BINARY_NODE(Modulus)
#endif
DEF_FLOAT64_BINARY_NODE_WITH_CALL(Exponentiate)
#undef DEF_FLOAT64_BINARY_NODE
#undef DEF_FLOAT64_BINARY_NODE_WITH_CALL

#undef DEF_OPERATION_NODE
#undef DEF_OPERATION_NODE_WITH_CALL

class Float64Compare : public FixedInputValueNodeT<2, Float64Compare> {
  using Base = FixedInputValueNodeT<2, Float64Compare>;

 public:
  explicit Float64Compare(uint64_t bitfield, Operation operation)
      : Base(OperationBitField::update(bitfield, operation)) {}

  static constexpr Base::InputTypes kInputTypes{ValueRepresentation::kFloat64,
                                                ValueRepresentation::kFloat64};

  static constexpr int kLeftIndex = 0;
  static constexpr int kRightIndex = 1;
  Input left_input() { return Node::input(kLeftIndex); }
  Input right_input() { return Node::input(kRightIndex); }

  constexpr Operation operation() const {
    return OperationBitField::decode(bitfield());
  }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

  auto options() const { return std::tuple{operation()}; }

 private:
  using OperationBitField = NextBitField<Operation, 5>;
};

class Float64ToBoolean : public FixedInputValueNodeT<1, Float64ToBoolean> {
  using Base = FixedInputValueNodeT<1, Float64ToBoolean>;

 public:
  explicit Float64ToBoolean(uint64_t bitfield, bool flip)
      : Base(FlipBitField::update(bitfield, flip)) {}

  static constexpr Base::InputTypes kInputTypes{
      ValueRepresentation::kHoleyFloat64};

  Input value() { return Node::input(0); }

  constexpr bool flip() const { return FlipBitField::decode(bitfield()); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

  auto options() const { return std::tuple{flip()}; }

 private:
  using FlipBitField = NextBitField<bool, 1>;
};

class Float64Negate : public FixedInputValueNodeT<1, Float64Negate> {
  using Base = FixedInputValueNodeT<1, Float64Negate>;

 public:
  explicit Float64Negate(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::Float64();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kHoleyFloat64};

  Input input() { return Node::input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

#define IEEE_754_UNARY_LIST(V) \
  V(MathAcos, acos, Acos)      \
  V(MathAcosh, acosh, Acosh)   \
  V(MathAsin, asin, Asin)      \
  V(MathAsinh, asinh, Asinh)   \
  V(MathAtan, atan, Atan)      \
  V(MathAtanh, atanh, Atanh)   \
  V(MathCbrt, cbrt, Cbrt)      \
  V(MathCos, cos, Cos)         \
  V(MathCosh, cosh, Cosh)      \
  V(MathExp, exp, Exp)         \
  V(MathExpm1, expm1, Expm1)   \
  V(MathLog, log, Log)         \
  V(MathLog1p, log1p, Log1p)   \
  V(MathLog10, log10, Log10)   \
  V(MathLog2, log2, Log2)      \
  V(MathSin, sin, Sin)         \
  V(MathSinh, sinh, Sinh)      \
  V(MathTan, tan, Tan)         \
  V(MathTanh, tanh, Tanh)
class Float64Ieee754Unary
    : public FixedInputValueNodeT<1, Float64Ieee754Unary> {
  using Base = FixedInputValueNodeT<1, Float64Ieee754Unary>;

 public:
  enum class Ieee754Function : uint8_t {
#define DECL_ENUM(MathName, ExtName, EnumName) k##EnumName,
    IEEE_754_UNARY_LIST(DECL_ENUM)
#undef DECL_ENUM
  };
  explicit Float64Ieee754Unary(uint64_t bitfield, Ieee754Function ieee_function)
      : Base(bitfield), ieee_function_(ieee_function) {}

  static constexpr OpProperties kProperties =
      OpProperties::Float64() | OpProperties::Call();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kHoleyFloat64};

  Input input() { return Node::input(0); }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

  auto options() const { return std::tuple{ieee_function_}; }

  Ieee754Function ieee_function() const { return ieee_function_; }
  ExternalReference ieee_function_ref() const;

 private:
  Ieee754Function ieee_function_;
};

#define IEEE_754_BINARY_LIST(V) \
  V(MathAtan2, atan2, Atan2)    \
  V(MathPow, pow, Power)

class Float64Ieee754Binary
    : public FixedInputValueNodeT<2, Float64Ieee754Binary> {
  using Base = FixedInputValueNodeT<2, Float64Ieee754Binary>;

 public:
  enum class Ieee754Function : uint8_t {
#define DECL_ENUM(MathName, ExtName, EnumName) k##EnumName,
    IEEE_754_BINARY_LIST(DECL_ENUM)
#undef DECL_ENUM
  };
  explicit Float64Ieee754Binary(uint64_t bitfield,
                                Ieee754Function ieee_function)
      : Base(bitfield), ieee_function_(ieee_function) {}

  static constexpr OpProperties kProperties =
      OpProperties::Float64() | OpProperties::Call();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kHoleyFloat64, ValueRepresentation::kHoleyFloat64};

  Input input_lhs() { return Node::input(0); }
  Input input_rhs() { return Node::input(1); }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

  auto options() const { return std::tuple{ieee_function_}; }

  Ieee754Function ieee_function() const { return ieee_function_; }
  ExternalReference ieee_function_ref() const;

 private:
  Ieee754Function ieee_function_;
};

class Float64Sqrt : public FixedInputValueNodeT<1, Float64Sqrt> {
  using Base = FixedInputValueNodeT<1, Float64Sqrt>;

 public:
  explicit Float64Sqrt(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::Float64();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kHoleyFloat64};

  Input input() { return Node::input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;
};

class CheckInt32IsSmi : public FixedInputNodeT<1, CheckInt32IsSmi> {
  using Base = FixedInputNodeT<1, CheckInt32IsSmi>;

 public:
  explicit CheckInt32IsSmi(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::EagerDeopt();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kInt32};

  Input input() { return Node::input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class CheckUint32IsSmi : public FixedInputNodeT<1, CheckUint32IsSmi> {
  using Base = FixedInputNodeT<1, CheckUint32IsSmi>;

 public:
  explicit CheckUint32IsSmi(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::EagerDeopt();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kUint32};

  Input input() { return Node::input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class CheckIntPtrIsSmi : public FixedInputNodeT<1, CheckIntPtrIsSmi> {
  using Base = FixedInputNodeT<1, CheckIntPtrIsSmi>;

 public:
  explicit CheckIntPtrIsSmi(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::EagerDeopt();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kIntPtr};

  Input input() { return Node::input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class CheckHoleyFloat64IsSmi
    : public FixedInputNodeT<1, CheckHoleyFloat64IsSmi> {
  using Base = FixedInputNodeT<1, CheckHoleyFloat64IsSmi>;

 public:
  explicit CheckHoleyFloat64IsSmi(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::EagerDeopt();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kHoleyFloat64};

  Input input() { return Node::input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class CheckedSmiTagInt32 : public FixedInputValueNodeT<1, CheckedSmiTagInt32> {
  using Base = FixedInputValueNodeT<1, CheckedSmiTagInt32>;

 public:
  explicit CheckedSmiTagInt32(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties =
      OpProperties::EagerDeopt() | OpProperties::ConversionNode();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kInt32};

  Input input() { return Node::input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

// This is a check disguised as a conversion node so we can use it to override
// untagging conversions.
// TODO(olivf): Support overriding bigger with smaller instruction so we can use
// CheckInt32IsSmi instead.
class CheckedSmiSizedInt32
    : public FixedInputValueNodeT<1, CheckedSmiSizedInt32> {
  using Base = FixedInputValueNodeT<1, CheckedSmiSizedInt32>;

 public:
  explicit CheckedSmiSizedInt32(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::EagerDeopt() |
                                              OpProperties::Int32() |
                                              OpProperties::ConversionNode();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kInt32};

  Input input() { return Node::input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class CheckedSmiTagUint32
    : public FixedInputValueNodeT<1, CheckedSmiTagUint32> {
  using Base = FixedInputValueNodeT<1, CheckedSmiTagUint32>;

 public:
  explicit CheckedSmiTagUint32(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties =
      OpProperties::EagerDeopt() | OpProperties::ConversionNode();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kUint32};

  Input input() { return Node::input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class CheckedSmiTagIntPtr
    : public FixedInputValueNodeT<1, CheckedSmiTagIntPtr> {
  using Base = FixedInputValueNodeT<1, CheckedSmiTagIntPtr>;

 public:
  explicit CheckedSmiTagIntPtr(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties =
      OpProperties::EagerDeopt() | OpProperties::ConversionNode();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kIntPtr};

  Input input() { return Node::input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

// Input must guarantee to fit in a Smi.
class UnsafeSmiTagInt32 : public FixedInputValueNodeT<1, UnsafeSmiTagInt32> {
  using Base = FixedInputValueNodeT<1, UnsafeSmiTagInt32>;

 public:
  explicit UnsafeSmiTagInt32(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::ConversionNode();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kInt32};

  Input input() { return Node::input(0); }

#ifdef V8_COMPRESS_POINTERS
  void MarkTaggedInputsAsDecompressing() {
    // No tagged inputs.
  }
#endif

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

// Input must guarantee to fit in a Smi.
class UnsafeSmiTagUint32 : public FixedInputValueNodeT<1, UnsafeSmiTagUint32> {
  using Base = FixedInputValueNodeT<1, UnsafeSmiTagUint32>;

 public:
  explicit UnsafeSmiTagUint32(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::ConversionNode();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kUint32};

  Input input() { return Node::input(0); }

#ifdef V8_COMPRESS_POINTERS
  void MarkTaggedInputsAsDecompressing() {
    // No tagged inputs.
  }
#endif

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

// Input must guarantee to fit in a Smi.
class UnsafeSmiTagIntPtr : public FixedInputValueNodeT<1, UnsafeSmiTagIntPtr> {
  using Base = FixedInputValueNodeT<1, UnsafeSmiTagIntPtr>;

 public:
  explicit UnsafeSmiTagIntPtr(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::ConversionNode();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kIntPtr};

  Input input() { return Node::input(0); }

#ifdef V8_COMPRESS_POINTERS
  void MarkTaggedInputsAsDecompressing() {
    // No tagged inputs.
  }
#endif

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class CheckedSmiUntag : public FixedInputValueNodeT<1, CheckedSmiUntag> {
  using Base = FixedInputValueNodeT<1, CheckedSmiUntag>;

 public:
  explicit CheckedSmiUntag(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::EagerDeopt() |
                                              OpProperties::Int32() |
                                              OpProperties::ConversionNode();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  Input input() { return Node::input(0); }

#ifdef V8_COMPRESS_POINTERS
  void MarkTaggedInputsAsDecompressing() {
    // Don't need to decompress to untag.
  }
#endif

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class UnsafeSmiUntag : public FixedInputValueNodeT<1, UnsafeSmiUntag> {
  using Base = FixedInputValueNodeT<1, UnsafeSmiUntag>;

 public:
  explicit UnsafeSmiUntag(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties =
      OpProperties::Int32() | OpProperties::ConversionNode();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  Input input() { return Node::input(0); }

#ifdef V8_COMPRESS_POINTERS
  void MarkTaggedInputsAsDecompressing() {
    // Don't need to decompress to untag.
  }
#endif

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class Int32Constant : public FixedInputValueNodeT<0, Int32Constant> {
  using Base = FixedInputValueNodeT<0, Int32Constant>;

 public:
  using OutputRegister = Register;

  explicit Int32Constant(uint64_t bitfield, int32_t value)
      : Base(bitfield), value_(value) {}

  static constexpr OpProperties kProperties = OpProperties::Int32();

  int32_t value() const { return value_; }

  bool ToBoolean(LocalIsolate* local_isolate) const { return value_ != 0; }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

  void DoLoadToRegister(MaglevAssembler*, OutputRegister) const;
  DirectHandle<Object> DoReify(LocalIsolate* isolate) const;

 private:
  const int32_t value_;
};

class Uint32Constant : public FixedInputValueNodeT<0, Uint32Constant> {
  using Base = FixedInputValueNodeT<0, Uint32Constant>;

 public:
  using OutputRegister = Register;

  explicit Uint32Constant(uint64_t bitfield, uint32_t value)
      : Base(bitfield), value_(value) {}

  static constexpr OpProperties kProperties = OpProperties::Uint32();

  uint32_t value() const { return value_; }

  bool ToBoolean(LocalIsolate* local_isolate) const { return value_ != 0; }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

  void DoLoadToRegister(MaglevAssembler*, OutputRegister) const;
  DirectHandle<Object> DoReify(LocalIsolate* isolate) const;

 private:
  const uint32_t value_;
};

class IntPtrConstant : public FixedInputValueNodeT<0, IntPtrConstant> {
  using Base = FixedInputValueNodeT<0, IntPtrConstant>;

 public:
  using OutputRegister = Register;

  explicit IntPtrConstant(uint64_t bitfield, intptr_t value)
      : Base(bitfield), value_(value) {}

  static constexpr OpProperties kProperties = OpProperties::IntPtr();

  intptr_t value() const { return value_; }

  bool ToBoolean(LocalIsolate* local_isolate) const { return value_ != 0; }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

  void DoLoadToRegister(MaglevAssembler*, OutputRegister) const;
  DirectHandle<Object> DoReify(LocalIsolate* isolate) const;

 private:
  const intptr_t value_;
};

class Float64Constant : public FixedInputValueNodeT<0, Float64Constant> {
  using Base = FixedInputValueNodeT<0, Float64Constant>;

 public:
  using OutputRegister = DoubleRegister;

  explicit Float64Constant(uint64_t bitfield, Float64 value)
      : Base(bitfield), value_(value) {}

  explicit Float64Constant(uint64_t bitfield, uint64_t bitpattern)
      : Base(bitfield), value_(Float64::FromBits(bitpattern)) {}

  static constexpr OpProperties kProperties = OpProperties::Float64();

  Float64 value() const { return value_; }

  bool ToBoolean(LocalIsolate* local_isolate) const {
    return value_.get_scalar() != 0.0 && !value_.is_nan();
  }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

  void DoLoadToRegister(MaglevAssembler*, OutputRegister) const;
  DirectHandle<Object> DoReify(LocalIsolate* isolate) const;

 private:
  const Float64 value_;
};

class Int32ToUint8Clamped
    : public FixedInputValueNodeT<1, Int32ToUint8Clamped> {
  using Base = FixedInputValueNodeT<1, Int32ToUint8Clamped>;

 public:
  explicit Int32ToUint8Clamped(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::Int32();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kInt32};

  Input input() { return Node::input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class Uint32ToUint8Clamped
    : public FixedInputValueNodeT<1, Uint32ToUint8Clamped> {
  using Base = FixedInputValueNodeT<1, Uint32ToUint8Clamped>;

 public:
  explicit Uint32ToUint8Clamped(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::Int32();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kUint32};

  Input input() { return Node::input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class Float64ToUint8Clamped
    : public FixedInputValueNodeT<1, Float64ToUint8Clamped> {
  using Base = FixedInputValueNodeT<1, Float64ToUint8Clamped>;

 public:
  explicit Float64ToUint8Clamped(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::Int32();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kHoleyFloat64};

  Input input() { return Node::input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class CheckedNumberToUint8Clamped
    : public FixedInputValueNodeT<1, CheckedNumberToUint8Clamped> {
  using Base = FixedInputValueNodeT<1, CheckedNumberToUint8Clamped>;

 public:
  explicit CheckedNumberToUint8Clamped(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties =
      OpProperties::EagerDeopt() | OpProperties::Int32();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  Input input() { return Node::input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class Int32ToNumber : public FixedInputValueNodeT<1, Int32ToNumber> {
  using Base = FixedInputValueNodeT<1, Int32ToNumber>;

 public:
  explicit Int32ToNumber(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::CanAllocate() |
                                              OpProperties::DeferredCall() |
                                              OpProperties::ConversionNode();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kInt32};

  Input input() { return Node::input(0); }

  int MaxCallStackArgs() const { return 0; }
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class Uint32ToNumber : public FixedInputValueNodeT<1, Uint32ToNumber> {
  using Base = FixedInputValueNodeT<1, Uint32ToNumber>;

 public:
  explicit Uint32ToNumber(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::CanAllocate() |
                                              OpProperties::DeferredCall() |
                                              OpProperties::ConversionNode();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kUint32};

  Input input() { return Node::input(0); }

  int MaxCallStackArgs() const { return 0; }
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class IntPtrToNumber : public FixedInputValueNodeT<1, IntPtrToNumber> {
  using Base = FixedInputValueNodeT<1, IntPtrToNumber>;

 public:
  explicit IntPtrToNumber(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::CanAllocate() |
                                              OpProperties::DeferredCall() |
                                              OpProperties::ConversionNode();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kIntPtr};

  Input input() { return Node::input(0); }

  int MaxCallStackArgs() const { return 0; }
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class Float64ToTagged : public FixedInputValueNodeT<1, Float64ToTagged> {
  using Base = FixedInputValueNodeT<1, Float64ToTagged>;

 public:
  enum class ConversionMode { kCanonicalizeSmi, kForceHeapNumber };
  explicit Float64ToTagged(uint64_t bitfield, ConversionMode mode)
      : Base(ConversionModeBitField::update(bitfield, mode)) {}
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kFloat64};

  static constexpr OpProperties kProperties = OpProperties::CanAllocate() |
                                              OpProperties::DeferredCall() |
                                              OpProperties::ConversionNode();

  Input input() { return Node::input(0); }

  int MaxCallStackArgs() const { return 0; }
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}

  void SetMode(ConversionMode mode) {
    set_bitfield(ConversionModeBitField::update(bitfield(), mode));
  }

  auto options() const { return std::tuple{conversion_mode()}; }

  ConversionMode conversion_mode() const {
    return ConversionModeBitField::decode(bitfield());
  }

 private:
  bool canonicalize_smi() {
    return ConversionModeBitField::decode(bitfield()) ==
           ConversionMode::kCanonicalizeSmi;
  }

  using ConversionModeBitField = NextBitField<ConversionMode, 1>;
};

// Essentially the same as Float64ToTagged but the result cannot be shared as it
// will be used as a mutable heap number by a store.
class Float64ToHeapNumberForField
    : public FixedInputValueNodeT<1, Float64ToHeapNumberForField> {
  using Base = FixedInputValueNodeT<1, Float64ToHeapNumberForField>;

 public:
  explicit Float64ToHeapNumberForField(uint64_t bitfield) : Base(bitfield) {}
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kHoleyFloat64};

  static constexpr OpProperties kProperties = OpProperties::NotIdempotent() |
                                              OpProperties::CanAllocate() |
                                              OpProperties::DeferredCall();

  Input input() { return Node::input(0); }

  int MaxCallStackArgs() const { return 0; }
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class HoleyFloat64ToTagged
    : public FixedInputValueNodeT<1, HoleyFloat64ToTagged> {
  using Base = FixedInputValueNodeT<1, HoleyFloat64ToTagged>;

 public:
  enum class ConversionMode { kCanonicalizeSmi, kForceHeapNumber };
  explicit HoleyFloat64ToTagged(uint64_t bitfield, ConversionMode mode)
      : Base(ConversionModeBitField::update(bitfield, mode)) {}
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kHoleyFloat64};

  static constexpr OpProperties kProperties = OpProperties::CanAllocate() |
                                              OpProperties::DeferredCall() |
                                              OpProperties::ConversionNode();

  Input input() { return Node::input(0); }

  int MaxCallStackArgs() const { return 0; }
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}

  void SetMode(ConversionMode mode) {
    set_bitfield(ConversionModeBitField::update(bitfield(), mode));
  }

  auto options() const { return std::tuple{conversion_mode()}; }

  ConversionMode conversion_mode() const {
    return ConversionModeBitField::decode(bitfield());
  }

 private:
  bool canonicalize_smi() {
    return ConversionModeBitField::decode(bitfield()) ==
           ConversionMode::kCanonicalizeSmi;
  }
  using ConversionModeBitField = NextBitField<ConversionMode, 1>;
};

class CheckedSmiTagFloat64
    : public FixedInputValueNodeT<1, CheckedSmiTagFloat64> {
  using Base = FixedInputValueNodeT<1, CheckedSmiTagFloat64>;

 public:
  explicit CheckedSmiTagFloat64(uint64_t bitfield) : Base(bitfield) {}
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kHoleyFloat64};

  static constexpr OpProperties kProperties =
      OpProperties::EagerDeopt() | OpProperties::ConversionNode();

  Input input() { return Node::input(0); }

  int MaxCallStackArgs() const { return 0; }
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class CheckedInt32ToUint32
    : public FixedInputValueNodeT<1, CheckedInt32ToUint32> {
  using Base = FixedInputValueNodeT<1, CheckedInt32ToUint32>;

 public:
  explicit CheckedInt32ToUint32(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::Uint32() |
                                              OpProperties::ConversionNode() |
                                              OpProperties::EagerDeopt();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kInt32};

  Input input() { return Node::input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class CheckedIntPtrToUint32
    : public FixedInputValueNodeT<1, CheckedIntPtrToUint32> {
  using Base = FixedInputValueNodeT<1, CheckedIntPtrToUint32>;

 public:
  explicit CheckedIntPtrToUint32(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::Uint32() |
                                              OpProperties::ConversionNode() |
                                              OpProperties::EagerDeopt();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kIntPtr};

  Input input() { return Node::input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class UnsafeInt32ToUint32
    : public FixedInputValueNodeT<1, UnsafeInt32ToUint32> {
  using Base = FixedInputValueNodeT<1, UnsafeInt32ToUint32>;

 public:
  explicit UnsafeInt32ToUint32(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties =
      OpProperties::Uint32() | OpProperties::ConversionNode();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kInt32};

  Input input() { return Node::input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class CheckedUint32ToInt32
    : public FixedInputValueNodeT<1, CheckedUint32ToInt32> {
  using Base = FixedInputValueNodeT<1, CheckedUint32ToInt32>;

 public:
  explicit CheckedUint32ToInt32(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::Int32() |
                                              OpProperties::ConversionNode() |
                                              OpProperties::EagerDeopt();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kUint32};

  Input input() { return Node::input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class UnsafeUint32ToInt32
    : public FixedInputValueNodeT<1, UnsafeUint32ToInt32> {
  using Base = FixedInputValueNodeT<1, UnsafeUint32ToInt32>;

 public:
  explicit UnsafeUint32ToInt32(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties =
      OpProperties::Int32() | OpProperties::ConversionNode();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kUint32};

  Input input() { return Node::input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class CheckedIntPtrToInt32
    : public FixedInputValueNodeT<1, CheckedIntPtrToInt32> {
  using Base = FixedInputValueNodeT<1, CheckedIntPtrToInt32>;

 public:
  explicit CheckedIntPtrToInt32(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::Int32() |
                                              OpProperties::ConversionNode() |
                                              OpProperties::EagerDeopt();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kIntPtr};

  Input input() { return Node::input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class ChangeInt32ToFloat64
    : public FixedInputValueNodeT<1, ChangeInt32ToFloat64> {
  using Base = FixedInputValueNodeT<1, ChangeInt32ToFloat64>;

 public:
  explicit ChangeInt32ToFloat64(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties =
      OpProperties::Float64() | OpProperties::ConversionNode();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kInt32};

  Input input() { return Node::input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class ChangeUint32ToFloat64
    : public FixedInputValueNodeT<1, ChangeUint32ToFloat64> {
  using Base = FixedInputValueNodeT<1, ChangeUint32ToFloat64>;

 public:
  explicit ChangeUint32ToFloat64(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties =
      OpProperties::Float64() | OpProperties::ConversionNode();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kUint32};

  Input input() { return Node::input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class ChangeIntPtrToFloat64
    : public FixedInputValueNodeT<1, ChangeIntPtrToFloat64> {
  using Base = FixedInputValueNodeT<1, ChangeIntPtrToFloat64>;

 public:
  explicit ChangeIntPtrToFloat64(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties =
      OpProperties::Float64() | OpProperties::ConversionNode();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kIntPtr};

  Input input() { return Node::input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class CheckedHoleyFloat64ToInt32
    : public FixedInputValueNodeT<1, CheckedHoleyFloat64ToInt32> {
  using Base = FixedInputValueNodeT<1, CheckedHoleyFloat64ToInt32>;

 public:
  explicit CheckedHoleyFloat64ToInt32(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::EagerDeopt() |
                                              OpProperties::Int32() |
                                              OpProperties::ConversionNode();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kHoleyFloat64};

  Input input() { return Node::input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class UnsafeHoleyFloat64ToInt32
    : public FixedInputValueNodeT<1, UnsafeHoleyFloat64ToInt32> {
  using Base = FixedInputValueNodeT<1, UnsafeHoleyFloat64ToInt32>;

 public:
  explicit UnsafeHoleyFloat64ToInt32(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties =
      OpProperties::Int32() | OpProperties::ConversionNode();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kHoleyFloat64};

  Input input() { return Node::input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class Int32AbsWithOverflow
    : public FixedInputValueNodeT<1, Int32AbsWithOverflow> {
  using Base = FixedInputValueNodeT<1, Int32AbsWithOverflow>;

 public:
  static constexpr OpProperties kProperties =
      OpProperties::EagerDeopt() | OpProperties::Int32();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kInt32};

  static constexpr int kValueIndex = 0;
  Input input() { return Node::input(kValueIndex); }

  explicit Int32AbsWithOverflow(uint64_t bitfield) : Base(bitfield) {}

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class Float64Abs : public FixedInputValueNodeT<1, Float64Abs> {
  using Base = FixedInputValueNodeT<1, Float64Abs>;

 public:
  explicit Float64Abs(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::Float64();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kHoleyFloat64};

  Input input() { return Node::input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class Int32CountLeadingZeros
    : public FixedInputValueNodeT<1, Int32CountLeadingZeros> {
  using Base = FixedInputValueNodeT<1, Int32CountLeadingZeros>;

 public:
  explicit Int32CountLeadingZeros(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::Int32();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kInt32};

  Input input() { return Node::input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class TaggedCountLeadingZeros
    : public FixedInputValueNodeT<1, TaggedCountLeadingZeros> {
  using Base = FixedInputValueNodeT<1, TaggedCountLeadingZeros>;

 public:
  explicit TaggedCountLeadingZeros(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::Int32();

  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  Input input() { return Node::input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class Float64CountLeadingZeros
    : public FixedInputValueNodeT<1, Float64CountLeadingZeros> {
  using Base = FixedInputValueNodeT<1, Float64CountLeadingZeros>;

 public:
  static Builtin continuation() { return Builtin::kMathClz32Continuation; }

  explicit Float64CountLeadingZeros(uint64_t bitfield) : Base(bitfield) {}

  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kHoleyFloat64};
  static constexpr OpProperties kProperties = OpProperties::Int32();

  Input input() { return Node::input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class Float64Round : public FixedInputValueNodeT<1, Float64Round> {
  using Base = FixedInputValueNodeT<1, Float64Round>;

 public:
  enum class Kind { kFloor, kCeil, kNearest };

  static Builtin continuation(Kind kind) {
    switch (kind) {
      case Kind::kCeil:
        return Builtin::kMathCeilContinuation;
      case Kind::kFloor:
        return Builtin::kMathFloorContinuation;
      case Kind::kNearest:
        return Builtin::kMathRoundContinuation;
    }
  }

  Float64Round(uint64_t bitfield, Kind kind) : Base(bitfield), kind_(kind) {}

  static constexpr OpProperties kProperties = OpProperties::Float64();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kHoleyFloat64};

  Input input() { return Node::input(0); }
  Kind kind() const { return kind_; }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

  auto options() const { return std::tuple{kind_}; }

 private:
  Kind kind_;
};

class CheckedHoleyFloat64ToUint32
    : public FixedInputValueNodeT<1, CheckedHoleyFloat64ToUint32> {
  using Base = FixedInputValueNodeT<1, CheckedHoleyFloat64ToUint32>;

 public:
  explicit CheckedHoleyFloat64ToUint32(uint64_t bitfield) : Base(bitfield) {}
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kHoleyFloat64};

  static constexpr OpProperties kProperties = OpProperties::EagerDeopt() |
                                              OpProperties::Uint32() |
                                              OpProperties::ConversionNode();

  Input input() { return Node::input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

#define DEFINE_TRUNCATE_NODE(name, from_repr, properties)        \
  class name : public FixedInputValueNodeT<1, name> {            \
    using Base = FixedInputValueNodeT<1, name>;                  \
                                                                 \
   public:                                                       \
    explicit name(uint64_t bitfield) : Base(bitfield) {}         \
                                                                 \
    static constexpr OpProperties kProperties = properties;      \
    static constexpr typename Base::InputTypes kInputTypes{      \
        ValueRepresentation::k##from_repr};                      \
                                                                 \
    Input input() { return Node::input(0); }                     \
                                                                 \
    void SetValueLocationConstraints();                          \
    void GenerateCode(MaglevAssembler*, const ProcessingState&); \
    void PrintParams(std::ostream&) const {}                     \
  };

DEFINE_TRUNCATE_NODE(TruncateUint32ToInt32, Uint32, OpProperties::Int32())
DEFINE_TRUNCATE_NODE(TruncateHoleyFloat64ToInt32, HoleyFloat64,
                     OpProperties::Int32())
#undef DEFINE_TRUNCATE_NODE

template <typename Derived, ValueRepresentation FloatType>
  requires(FloatType == ValueRepresentation::kFloat64 ||
           FloatType == ValueRepresentation::kHoleyFloat64)
class CheckedNumberOrOddballToFloat64OrHoleyFloat64
    : public FixedInputValueNodeT<1, Derived> {
  using Base = FixedInputValueNodeT<1, Derived>;
  using Base::result;

 public:
  explicit CheckedNumberOrOddballToFloat64OrHoleyFloat64(
      uint64_t bitfield, TaggedToFloat64ConversionType conversion_type)
      : Base(TaggedToFloat64ConversionTypeOffset::update(bitfield,
                                                         conversion_type)) {}

  static constexpr OpProperties kProperties =
      OpProperties::EagerDeopt() |
      OpProperties::ForValueRepresentation(FloatType) |
      OpProperties::ConversionNode();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  Input input() { return Node::input(0); }

  TaggedToFloat64ConversionType conversion_type() const {
    return TaggedToFloat64ConversionTypeOffset::decode(Base::bitfield());
  }

  DeoptimizeReason deoptimize_reason() const {
    return conversion_type() == TaggedToFloat64ConversionType::kNumberOrBoolean
               ? DeoptimizeReason::kNotANumberOrBoolean
               : DeoptimizeReason::kNotANumberOrOddball;
  }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

  auto options() const { return std::tuple{conversion_type()}; }

 private:
  using TaggedToFloat64ConversionTypeOffset =
      Base::template NextBitField<TaggedToFloat64ConversionType, 2>;
};

class CheckedNumberOrOddballToFloat64
    : public CheckedNumberOrOddballToFloat64OrHoleyFloat64<
          CheckedNumberOrOddballToFloat64, ValueRepresentation::kFloat64> {
  using Base = CheckedNumberOrOddballToFloat64OrHoleyFloat64<
      CheckedNumberOrOddballToFloat64, ValueRepresentation::kFloat64>;

 public:
  explicit CheckedNumberOrOddballToFloat64(
      uint64_t bitfield, TaggedToFloat64ConversionType conversion_type)
      : Base(bitfield, conversion_type) {}
};

class CheckedNumberOrOddballToHoleyFloat64
    : public FixedInputValueNodeT<1, CheckedNumberOrOddballToHoleyFloat64> {
  using Base = FixedInputValueNodeT<1, CheckedNumberOrOddballToHoleyFloat64>;

 public:
  explicit CheckedNumberOrOddballToHoleyFloat64(
      uint64_t bitfield, TaggedToFloat64ConversionType conversion_type)
      : Base(TaggedToFloat64ConversionTypeOffset::update(bitfield,
                                                         conversion_type)) {}

  static constexpr OpProperties kProperties = OpProperties::EagerDeopt() |
                                              OpProperties::HoleyFloat64() |
                                              OpProperties::ConversionNode();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  Input input() { return Node::input(0); }

  TaggedToFloat64ConversionType conversion_type() const {
    return TaggedToFloat64ConversionTypeOffset::decode(Base::bitfield());
  }

  DeoptimizeReason deoptimize_reason() const {
    switch (conversion_type()) {
      case TaggedToFloat64ConversionType::kOnlyNumber:
        return DeoptimizeReason::kNotANumber;
      case TaggedToFloat64ConversionType::kNumberOrBoolean:
        return DeoptimizeReason::kNotANumberOrBoolean;
      case TaggedToFloat64ConversionType::kNumberOrUndefined:
        // TODO(nicohartmann): We could consider a NotANumberOrUndefined reason.
      case TaggedToFloat64ConversionType::kNumberOrOddball:
        return DeoptimizeReason::kNotANumberOrOddball;
    }
  }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}

  auto options() const { return std::tuple{conversion_type()}; }

 private:
  using TaggedToFloat64ConversionTypeOffset =
      Base::template NextBitField<TaggedToFloat64ConversionType, 2>;
};

class CheckedNumberToInt32
    : public FixedInputValueNodeT<1, CheckedNumberToInt32> {
  using Base = FixedInputValueNodeT<1, CheckedNumberToInt32>;

 public:
  explicit CheckedNumberToInt32(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::EagerDeopt() |
                                              OpProperties::Int32() |
                                              OpProperties::ConversionNode();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  Input input() { return Node::input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class UncheckedNumberOrOddballToFloat64
    : public FixedInputValueNodeT<1, UncheckedNumberOrOddballToFloat64> {
  using Base = FixedInputValueNodeT<1, UncheckedNumberOrOddballToFloat64>;

 public:
  explicit UncheckedNumberOrOddballToFloat64(
      uint64_t bitfield, TaggedToFloat64ConversionType conversion_type)
      : Base(TaggedToFloat64ConversionTypeOffset::update(bitfield,
                                                         conversion_type)) {}

  static constexpr OpProperties kProperties =
      OpProperties::Float64() | OpProperties::ConversionNode();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  Input input() { return Node::input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

  TaggedToFloat64ConversionType conversion_type() const {
    return TaggedToFloat64ConversionTypeOffset::decode(bitfield());
  }

  auto options() const { return std::tuple{conversion_type()}; }

 private:
  using TaggedToFloat64ConversionTypeOffset =
      NextBitField<TaggedToFloat64ConversionType, 2>;
};

class CheckedHoleyFloat64ToFloat64
    : public FixedInputValueNodeT<1, CheckedHoleyFloat64ToFloat64> {
  using Base = FixedInputValueNodeT<1, CheckedHoleyFloat64ToFloat64>;

 public:
  explicit CheckedHoleyFloat64ToFloat64(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::EagerDeopt() |
                                              OpProperties::Float64() |
                                              OpProperties::ConversionNode();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kHoleyFloat64};

  Input input() { return Node::input(0); }

  int MaxCallStackArgs() const { return 0; }
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class HoleyFloat64ToMaybeNanFloat64
    : public FixedInputValueNodeT<1, HoleyFloat64ToMaybeNanFloat64> {
  using Base = FixedInputValueNodeT<1, HoleyFloat64ToMaybeNanFloat64>;

 public:
  explicit HoleyFloat64ToMaybeNanFloat64(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::Float64();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kHoleyFloat64};

  Input input() { return Node::input(0); }

  int MaxCallStackArgs() const { return 0; }
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

#ifdef V8_ENABLE_EXPERIMENTAL_UNDEFINED_DOUBLE
class Float64ToHoleyFloat64
    : public FixedInputValueNodeT<1, Float64ToHoleyFloat64> {
  using Base = FixedInputValueNodeT<1, Float64ToHoleyFloat64>;

 public:
  explicit Float64ToHoleyFloat64(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties =
      OpProperties::HoleyFloat64() | OpProperties::ConversionNode();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kFloat64};

  Input input() { return Node::input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class ConvertHoleNanToUndefinedNan
    : public FixedInputValueNodeT<1, ConvertHoleNanToUndefinedNan> {
  using Base = FixedInputValueNodeT<1, ConvertHoleNanToUndefinedNan>;

 public:
  explicit ConvertHoleNanToUndefinedNan(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::HoleyFloat64();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kHoleyFloat64};

  Input input() { return Node::input(0); }

  int MaxCallStackArgs() const { return 0; }
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class HoleyFloat64IsUndefinedOrHole
    : public FixedInputValueNodeT<1, HoleyFloat64IsUndefinedOrHole> {
  using Base = FixedInputValueNodeT<1, HoleyFloat64IsUndefinedOrHole>;

 public:
  explicit HoleyFloat64IsUndefinedOrHole(uint64_t bitfield) : Base(bitfield) {}

  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kHoleyFloat64};

  Input input() { return Node::input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

#else

class HoleyFloat64IsHole : public FixedInputValueNodeT<1, HoleyFloat64IsHole> {
  using Base = FixedInputValueNodeT<1, HoleyFloat64IsHole>;

 public:
  explicit HoleyFloat64IsHole(uint64_t bitfield) : Base(bitfield) {}

  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kHoleyFloat64};

  Input input() { return Node::input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

#endif  // V8_ENABLE_EXPERIMENTAL_UNDEFINED_DOUBLE

class TruncateUnsafeNumberOrOddballToInt32
    : public FixedInputValueNodeT<1, TruncateUnsafeNumberOrOddballToInt32> {
  using Base = FixedInputValueNodeT<1, TruncateUnsafeNumberOrOddballToInt32>;

 public:
  explicit TruncateUnsafeNumberOrOddballToInt32(
      uint64_t bitfield, TaggedToFloat64ConversionType conversion_type)
      : Base(TaggedToFloat64ConversionTypeOffset::update(bitfield,
                                                         conversion_type)) {}

  static constexpr OpProperties kProperties = OpProperties::Int32();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  Input input() { return Node::input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

  TaggedToFloat64ConversionType conversion_type() const {
    return TaggedToFloat64ConversionTypeOffset::decode(bitfield());
  }

  auto options() const { return std::tuple{conversion_type()}; }

 private:
  using TaggedToFloat64ConversionTypeOffset =
      NextBitField<TaggedToFloat64ConversionType, 2>;
};

class TruncateCheckedNumberOrOddballToInt32
    : public FixedInputValueNodeT<1, TruncateCheckedNumberOrOddballToInt32> {
  using Base = FixedInputValueNodeT<1, TruncateCheckedNumberOrOddballToInt32>;

 public:
  explicit TruncateCheckedNumberOrOddballToInt32(
      uint64_t bitfield, TaggedToFloat64ConversionType conversion_type)
      : Base(TaggedToFloat64ConversionTypeOffset::update(bitfield,
                                                         conversion_type)) {}

  static constexpr OpProperties kProperties =
      OpProperties::EagerDeopt() | OpProperties::Int32();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  Input input() { return Node::input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

  TaggedToFloat64ConversionType conversion_type() const {
    return TaggedToFloat64ConversionTypeOffset::decode(bitfield());
  }

  auto options() const { return std::tuple{conversion_type()}; }

 private:
  using TaggedToFloat64ConversionTypeOffset =
      NextBitField<TaggedToFloat64ConversionType, 2>;
};

class LogicalNot : public FixedInputValueNodeT<1, LogicalNot> {
  using Base = FixedInputValueNodeT<1, LogicalNot>;

 public:
  explicit LogicalNot(uint64_t bitfield) : Base(bitfield) {}

  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  Input value() { return Node::input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class SetPendingMessage : public FixedInputValueNodeT<1, SetPendingMessage> {
  using Base = FixedInputValueNodeT<1, SetPendingMessage>;

 public:
  explicit SetPendingMessage(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties =
      OpProperties::CanWrite() | OpProperties::CanRead();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  Input value() { return Node::input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

enum class CheckType { kCheckHeapObject, kOmitHeapObjectCheck };
class ToBoolean : public FixedInputValueNodeT<1, ToBoolean> {
  using Base = FixedInputValueNodeT<1, ToBoolean>;

 public:
  explicit ToBoolean(uint64_t bitfield, CheckType check_type)
      : Base(CheckTypeBitField::update(bitfield, check_type)) {}

  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  Input value() { return Node::input(0); }
  CheckType check_type() const { return CheckTypeBitField::decode(bitfield()); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}

  auto options() const { return std::tuple{check_type()}; }

 private:
  using CheckTypeBitField = NextBitField<CheckType, 1>;
};

class ToBooleanLogicalNot
    : public FixedInputValueNodeT<1, ToBooleanLogicalNot> {
  using Base = FixedInputValueNodeT<1, ToBooleanLogicalNot>;

 public:
  explicit ToBooleanLogicalNot(uint64_t bitfield, CheckType check_type)
      : Base(CheckTypeBitField::update(bitfield, check_type)) {}

  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  Input value() { return Node::input(0); }
  CheckType check_type() const { return CheckTypeBitField::decode(bitfield()); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}

  auto options() const { return std::tuple{check_type()}; }

 private:
  using CheckTypeBitField = NextBitField<CheckType, 1>;
};

// With StringEqualInputs::kStringsOrOddballs StringEqual allows non-string
// objects which are then compared with pointer equality (they will never be
// equal to a String and they're equal to each other if the pointers are equal).
enum class StringEqualInputMode { kOnlyStrings, kStringsOrOddballs };
class StringEqual : public FixedInputValueNodeT<2, StringEqual> {
  using Base = FixedInputValueNodeT<2, StringEqual>;

 public:
  explicit StringEqual(uint64_t bitfield, StringEqualInputMode input_mode)
      : Base(bitfield), input_mode_(input_mode) {}
  static constexpr OpProperties kProperties = OpProperties::Call() |
                                              OpProperties::LazyDeopt() |
                                              OpProperties::CanAllocate();

  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged};

  Input lhs() { return Node::input(0); }
  Input rhs() { return Node::input(1); }

  int MaxCallStackArgs() const { return 0; }
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}

  StringEqualInputMode input_mode() const { return input_mode_; }
  auto options() const { return std::tuple(input_mode_); }

 private:
  StringEqualInputMode input_mode_;
};

class TaggedEqual : public FixedInputValueNodeT<2, TaggedEqual> {
  using Base = FixedInputValueNodeT<2, TaggedEqual>;

 public:
  explicit TaggedEqual(uint64_t bitfield) : Base(bitfield) {}

  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged};

  Input lhs() { return Node::input(0); }
  Input rhs() { return Node::input(1); }

#ifdef V8_COMPRESS_POINTERS
  void MarkTaggedInputsAsDecompressing() {
    // Don't need to decompress to compare reference equality.
  }
#endif

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class TaggedNotEqual : public FixedInputValueNodeT<2, TaggedNotEqual> {
  using Base = FixedInputValueNodeT<2, TaggedNotEqual>;

 public:
  explicit TaggedNotEqual(uint64_t bitfield) : Base(bitfield) {}

  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged};

  Input lhs() { return Node::input(0); }
  Input rhs() { return Node::input(1); }

#ifdef V8_COMPRESS_POINTERS
  void MarkTaggedInputsAsDecompressing() {
    // Don't need to decompress to compare reference equality.
  }
#endif

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class TestInstanceOf : public FixedInputValueNodeT<3, TestInstanceOf> {
  using Base = FixedInputValueNodeT<3, TestInstanceOf>;

 public:
  explicit TestInstanceOf(uint64_t bitfield,
                          const compiler::FeedbackSource& feedback)
      : Base(bitfield), feedback_(feedback) {}

  // The implementation currently calls runtime.
  static constexpr OpProperties kProperties = OpProperties::JSCall();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged,
      ValueRepresentation::kTagged};

  Input context() { return input(0); }
  Input object() { return input(1); }
  Input callable() { return input(2); }
  const compiler::FeedbackSource& feedback() const { return feedback_; }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}

 private:
  const compiler::FeedbackSource feedback_;
};

class TestUndetectable : public FixedInputValueNodeT<1, TestUndetectable> {
  using Base = FixedInputValueNodeT<1, TestUndetectable>;

 public:
  explicit TestUndetectable(uint64_t bitfield, CheckType check_type)
      : Base(CheckTypeBitField::update(bitfield, check_type)) {}

  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  Input value() { return Node::input(0); }
  CheckType check_type() const { return CheckTypeBitField::decode(bitfield()); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}

  auto options() const { return std::tuple{check_type()}; }

 private:
  using CheckTypeBitField = NextBitField<CheckType, 1>;
};

class TestTypeOf : public FixedInputValueNodeT<1, TestTypeOf> {
  using Base = FixedInputValueNodeT<1, TestTypeOf>;

 public:
  explicit TestTypeOf(uint64_t bitfield,
                      interpreter::TestTypeOfFlags::LiteralFlag literal)
      : Base(bitfield), literal_(literal) {}

  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  Input value() { return Node::input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

  auto options() const { return std::tuple{literal_}; }

  interpreter::TestTypeOfFlags::LiteralFlag literal() const { return literal_; }

 private:
  interpreter::TestTypeOfFlags::LiteralFlag literal_;
};

class ToName : public FixedInputValueNodeT<2, ToName> {
  using Base = FixedInputValueNodeT<2, ToName>;

 public:
  explicit ToName(uint64_t bitfield) : Base(bitfield) {}

  // The implementation currently calls runtime.
  static constexpr OpProperties kProperties = OpProperties::JSCall();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged};

  Input context() { return Node::input(0); }
  Input value_input() { return Node::input(1); }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class ToNumberOrNumeric : public FixedInputValueNodeT<1, ToNumberOrNumeric> {
  using Base = FixedInputValueNodeT<1, ToNumberOrNumeric>;

 public:
  explicit ToNumberOrNumeric(uint64_t bitfield, Object::Conversion mode)
      : Base(bitfield), mode_(mode) {}

  static constexpr OpProperties kProperties =
      OpProperties::DeferredCall() | OpProperties::CanCallUserCode();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  Input value_input() { return Node::input(0); }
  Object::Conversion mode() const { return mode_; }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}

 private:
  const Object::Conversion mode_;
};

class DeleteProperty : public FixedInputValueNodeT<3, DeleteProperty> {
  using Base = FixedInputValueNodeT<3, DeleteProperty>;

 public:
  explicit DeleteProperty(uint64_t bitfield, LanguageMode mode)
      : Base(bitfield), mode_(mode) {}

  // The implementation currently calls runtime.
  static constexpr OpProperties kProperties = OpProperties::JSCall();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged,
      ValueRepresentation::kTagged};

  Input context() { return Node::input(0); }
  Input object() { return Node::input(1); }
  Input key() { return Node::input(2); }

  LanguageMode mode() const { return mode_; }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

 private:
  const LanguageMode mode_;
};

class GeneratorStore : public NodeT<GeneratorStore> {
  using Base = NodeT<GeneratorStore>;

 public:
  // We assume the context as fixed input.
  static constexpr int kContextIndex = 0;
  static constexpr int kGeneratorIndex = 1;
  static constexpr int kFixedInputCount = 2;

  // This ctor is used when for variable input counts.
  // Inputs must be initialized manually.
  GeneratorStore(uint64_t bitfield, ValueNode* context, ValueNode* generator,
                 int suspend_id, int bytecode_offset)
      : Base(bitfield),
        suspend_id_(suspend_id),
        bytecode_offset_(bytecode_offset) {
    set_input(kContextIndex, context);
    set_input(kGeneratorIndex, generator);
  }

  static constexpr OpProperties kProperties = OpProperties::DeferredCall() |
                                              OpProperties::CanRead() |
                                              OpProperties::CanWrite();

  int suspend_id() const { return suspend_id_; }
  int bytecode_offset() const { return bytecode_offset_; }

  Input context_input() { return input(kContextIndex); }
  Input generator_input() { return input(kGeneratorIndex); }

  int num_parameters_and_registers() const {
    return input_count() - kFixedInputCount;
  }
  Input parameters_and_registers(int i) { return input(i + kFixedInputCount); }
  void set_parameters_and_registers(int i, ValueNode* node) {
    set_input(i + kFixedInputCount, node);
  }

  int MaxCallStackArgs() const;
  void VerifyInputs() const;

#ifdef V8_COMPRESS_POINTERS
  void MarkTaggedInputsAsDecompressing() {
    // Don't need to decompress to store.
  }
#endif

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}

 private:
  const int suspend_id_;
  const int bytecode_offset_;
};

class TryOnStackReplacement : public FixedInputNodeT<1, TryOnStackReplacement> {
  using Base = FixedInputNodeT<1, TryOnStackReplacement>;

 public:
  explicit TryOnStackReplacement(uint64_t bitfield, int32_t loop_depth,
                                 FeedbackSlot feedback_slot,
                                 BytecodeOffset osr_offset,
                                 MaglevCompilationUnit* unit)
      : Base(bitfield),
        loop_depth_(loop_depth),
        feedback_slot_(feedback_slot),
        osr_offset_(osr_offset),
        unit_(unit) {}

  static constexpr OpProperties kProperties =
      OpProperties::DeferredCall() | OpProperties::EagerDeopt() |
      OpProperties::CanAllocate() | OpProperties::NotIdempotent();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  Input closure() { return Node::input(0); }

  const MaglevCompilationUnit* unit() const { return unit_; }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}

 private:
  // For OSR.
  const int32_t loop_depth_;
  const FeedbackSlot feedback_slot_;
  const BytecodeOffset osr_offset_;
  MaglevCompilationUnit* const unit_;
};

class ForInPrepare : public FixedInputValueNodeT<2, ForInPrepare> {
  using Base = FixedInputValueNodeT<2, ForInPrepare>;

 public:
  explicit ForInPrepare(uint64_t bitfield, compiler::FeedbackSource& feedback)
      : Base(bitfield), feedback_(feedback) {}

  static constexpr OpProperties kProperties =
      OpProperties::Call() | OpProperties::NotIdempotent();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged};

  compiler::FeedbackSource feedback() const { return feedback_; }

  Input context() { return Node::input(0); }
  Input enumerator() { return Node::input(1); }

  int ReturnCount() const { return 2; }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}

 private:
  const compiler::FeedbackSource feedback_;
};

class ForInNext : public FixedInputValueNodeT<5, ForInNext> {
  using Base = FixedInputValueNodeT<5, ForInNext>;

 public:
  explicit ForInNext(uint64_t bitfield, compiler::FeedbackSource& feedback)
      : Base(bitfield), feedback_(feedback) {}

  static constexpr OpProperties kProperties = OpProperties::JSCall();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged,
      ValueRepresentation::kTagged, ValueRepresentation::kTagged,
      ValueRepresentation::kTagged};

  compiler::FeedbackSource feedback() const { return feedback_; }

  Input context() { return Node::input(0); }
  Input receiver() { return Node::input(1); }
  Input cache_array() { return Node::input(2); }
  Input cache_type() { return Node::input(3); }
  Input cache_index() { return Node::input(4); }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}

 private:
  const compiler::FeedbackSource feedback_;
};

class GetIterator : public FixedInputValueNodeT<2, GetIterator> {
  using Base = FixedInputValueNodeT<2, GetIterator>;

 public:
  explicit GetIterator(uint64_t bitfield, int load_slot, int call_slot,
                       compiler::FeedbackVectorRef feedback)
      : Base(bitfield),
        load_slot_(load_slot),
        call_slot_(call_slot),
        feedback_(feedback.object()) {}

  static constexpr OpProperties kProperties = OpProperties::JSCall();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged};

  Input context() { return input(0); }
  Input receiver() { return input(1); }

  int load_slot() const { return load_slot_; }
  int call_slot() const { return call_slot_; }
  IndirectHandle<FeedbackVector> feedback() const { return feedback_; }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}

 private:
  const int load_slot_;
  const int call_slot_;
  const IndirectHandle<FeedbackVector> feedback_;
};

class GetSecondReturnedValue
    : public FixedInputValueNodeT<0, GetSecondReturnedValue> {
  using Base = FixedInputValueNodeT<0, GetSecondReturnedValue>;

 public:
  // TODO(olivf): This is needed because this instruction accesses the raw
  // register content. We should have tuple values instead such that we can
  // refer to both returned values properly.
  static constexpr OpProperties kProperties = OpProperties::NotIdempotent();
  explicit GetSecondReturnedValue(uint64_t bitfield) : Base(bitfield) {}

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class ToObject : public FixedInputValueNodeT<2, ToObject> {
  using Base = FixedInputValueNodeT<2, ToObject>;

 public:
  explicit ToObject(uint64_t bitfield, CheckType check_type)
      : Base(CheckTypeBitField::update(bitfield, check_type)) {}

  // The implementation currently calls runtime.
  static constexpr OpProperties kProperties = OpProperties::JSCall();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged};

  Input context() { return Node::input(0); }
  Input value_input() { return Node::input(1); }
  CheckType check_type() const { return CheckTypeBitField::decode(bitfield()); }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}

 private:
  using CheckTypeBitField = NextBitField<CheckType, 1>;
};

class ToString : public FixedInputValueNodeT<2, ToString> {
  using Base = FixedInputValueNodeT<2, ToString>;

 public:
  enum ConversionMode { kConvertSymbol, kThrowOnSymbol };
  explicit ToString(uint64_t bitfield, ConversionMode mode)
      : Base(ConversionModeBitField::update(bitfield, mode)) {}

  // The implementation currently calls runtime.
  static constexpr OpProperties kProperties = OpProperties::JSCall();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged};

  Input context() { return Node::input(0); }
  Input value_input() { return Node::input(1); }
  ConversionMode mode() const {
    return ConversionModeBitField::decode(bitfield());
  }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}

 private:
  using ConversionModeBitField = NextBitField<ConversionMode, 1>;
};

class NumberToString : public FixedInputValueNodeT<1, NumberToString> {
  using Base = FixedInputValueNodeT<1, NumberToString>;

 public:
  explicit NumberToString(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::Call() |
                                              OpProperties::CanAllocate() |
                                              OpProperties::LazyDeopt();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  Input value_input() { return Node::input(0); }

  int MaxCallStackArgs() const { return 0; }
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class GeneratorRestoreRegister
    : public FixedInputValueNodeT<2, GeneratorRestoreRegister> {
  using Base = FixedInputValueNodeT<2, GeneratorRestoreRegister>;

 public:
  explicit GeneratorRestoreRegister(uint64_t bitfield, int index)
      : Base(bitfield), index_(index) {}

  static constexpr OpProperties kProperties = OpProperties::NotIdempotent();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged};

  Input array_input() { return input(0); }
  Input stale_input() { return input(1); }
  int index() const { return index_; }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}

 private:
  const int index_;
};

class InitialValue : public FixedInputValueNodeT<0, InitialValue> {
  using Base = FixedInputValueNodeT<0, InitialValue>;

 public:
  explicit InitialValue(uint64_t bitfield, interpreter::Register source);

  interpreter::Register source() const { return source_; }
  uint32_t stack_slot() const;
  static uint32_t stack_slot(uint32_t register_idx);

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

  bool is_unused() const { return UnusedField::decode(bitfield()); }
  void mark_unused() { set_bitfield(UnusedField::update(bitfield(), true)); }

  auto options() const { return std::tuple{source()}; }

 private:
  const interpreter::Register source_;
  using UnusedField = NextBitField<bool, 1>;
};

class RegisterInput : public FixedInputValueNodeT<0, RegisterInput> {
  using Base = FixedInputValueNodeT<0, RegisterInput>;

 public:
  explicit RegisterInput(uint64_t bitfield, Register input)
      : Base(bitfield), input_(input) {}

  Register input() const { return input_; }

  static constexpr OpProperties kProperties = OpProperties::NotIdempotent();

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

 private:
  const Register input_;
};

class SmiConstant : public FixedInputValueNodeT<0, SmiConstant> {
  using Base = FixedInputValueNodeT<0, SmiConstant>;

 public:
  using OutputRegister = Register;

  explicit SmiConstant(uint64_t bitfield, int32_t value)
      : Base(bitfield), value_(Smi::FromInt(value)) {
    DCHECK(Smi::IsValid(value));
  }

  Tagged<Smi> value() const { return value_; }

  bool ToBoolean(LocalIsolate* local_isolate) const {
    return value_ != Smi::FromInt(0);
  }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

  void DoLoadToRegister(MaglevAssembler*, OutputRegister) const;
  DirectHandle<Object> DoReify(LocalIsolate* isolate) const;

 private:
  const Tagged<Smi> value_;
};

class TaggedIndexConstant
    : public FixedInputValueNodeT<0, TaggedIndexConstant> {
  using Base = FixedInputValueNodeT<0, TaggedIndexConstant>;

 public:
  using OutputRegister = Register;

  explicit TaggedIndexConstant(uint64_t bitfield, int value)
      : Base(bitfield), value_(TaggedIndex::FromIntptr(value)) {
    DCHECK(TaggedIndex::IsValid(value));
  }

  Tagged<TaggedIndex> value() const { return value_; }

  bool ToBoolean(LocalIsolate* local_isolate) const { UNREACHABLE(); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

  void DoLoadToRegister(MaglevAssembler*, OutputRegister) const;
  DirectHandle<Object> DoReify(LocalIsolate* isolate) const;

 private:
  const Tagged<TaggedIndex> value_;
};

class Constant : public FixedInputValueNodeT<0, Constant> {
  using Base = FixedInputValueNodeT<0, Constant>;

 public:
  using OutputRegister = Register;

  explicit Constant(uint64_t bitfield, compiler::HeapObjectRef object)
      : Base(bitfield), object_(object) {}

  bool ToBoolean(LocalIsolate* local_isolate) const {
    return Object::BooleanValue(*object_.object(), local_isolate);
  }

  bool IsTheHole(compiler::JSHeapBroker* broker) const {
    return object_.IsTheHole();
  }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

  compiler::HeapObjectRef object() { return object_; }

  void DoLoadToRegister(MaglevAssembler*, OutputRegister) const;
  DirectHandle<Object> DoReify(LocalIsolate* isolate) const;

  compiler::HeapObjectRef ref() const { return object_; }

 private:
  const compiler::HeapObjectRef object_;
};

class RootConstant : public FixedInputValueNodeT<0, RootConstant> {
  using Base = FixedInputValueNodeT<0, RootConstant>;

 public:
  using OutputRegister = Register;

  explicit RootConstant(uint64_t bitfield, RootIndex index)
      : Base(bitfield), index_(index) {}

  bool ToBoolean(LocalIsolate* local_isolate) const;

  RootIndex index() const { return index_; }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

  void DoLoadToRegister(MaglevAssembler*, OutputRegister) const;
  Handle<Object> DoReify(LocalIsolate* isolate) const;

 private:
  const RootIndex index_;
};

class TrustedConstant : public FixedInputValueNodeT<0, TrustedConstant> {
  using Base = FixedInputValueNodeT<0, TrustedConstant>;

 public:
  using OutputRegister = Register;

  explicit TrustedConstant(uint64_t bitfield, compiler::HeapObjectRef object,
                           IndirectPointerTag tag)
      : Base(bitfield), object_(object), tag_(tag) {}

  static constexpr OpProperties kProperties = OpProperties::TrustedPointer();

  bool ToBoolean(LocalIsolate* local_isolate) const { UNREACHABLE(); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

  compiler::HeapObjectRef object() const { return object_; }
  IndirectPointerTag tag() const { return tag_; }

  void DoLoadToRegister(MaglevAssembler*, OutputRegister) const;
  DirectHandle<Object> DoReify(LocalIsolate* isolate) const;

 private:
  const compiler::HeapObjectRef object_;
  const IndirectPointerTag tag_;
};

class CreateArrayLiteral : public FixedInputValueNodeT<0, CreateArrayLiteral> {
  using Base = FixedInputValueNodeT<0, CreateArrayLiteral>;

 public:
  explicit CreateArrayLiteral(uint64_t bitfield,
                              compiler::HeapObjectRef constant_elements,
                              const compiler::FeedbackSource& feedback,
                              int flags)
      : Base(bitfield),
        constant_elements_(constant_elements),
        feedback_(feedback),
        flags_(flags) {}

  compiler::HeapObjectRef constant_elements() { return constant_elements_; }
  compiler::FeedbackSource feedback() const { return feedback_; }
  int flags() const { return flags_; }

  // The implementation currently calls runtime.
  static constexpr OpProperties kProperties =
      OpProperties::Call() | OpProperties::CanAllocate() |
      OpProperties::CanThrow() | OpProperties::LazyDeopt() |
      OpProperties::NotIdempotent();

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}

 private:
  const compiler::HeapObjectRef constant_elements_;
  const compiler::FeedbackSource feedback_;
  const int flags_;
};

class CreateShallowArrayLiteral
    : public FixedInputValueNodeT<0, CreateShallowArrayLiteral> {
  using Base = FixedInputValueNodeT<0, CreateShallowArrayLiteral>;

 public:
  explicit CreateShallowArrayLiteral(uint64_t bitfield,
                                     compiler::HeapObjectRef constant_elements,
                                     const compiler::FeedbackSource& feedback,
                                     int flags)
      : Base(bitfield),
        constant_elements_(constant_elements),
        feedback_(feedback),
        flags_(flags) {}

  compiler::HeapObjectRef constant_elements() { return constant_elements_; }
  compiler::FeedbackSource feedback() const { return feedback_; }
  int flags() const { return flags_; }

  // The implementation currently calls runtime.
  static constexpr OpProperties kProperties =
      OpProperties::GenericRuntimeOrBuiltinCall();

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}

 private:
  const compiler::HeapObjectRef constant_elements_;
  const compiler::FeedbackSource feedback_;
  const int flags_;
};

class CreateObjectLiteral
    : public FixedInputValueNodeT<0, CreateObjectLiteral> {
  using Base = FixedInputValueNodeT<0, CreateObjectLiteral>;

 public:
  explicit CreateObjectLiteral(
      uint64_t bitfield,
      compiler::ObjectBoilerplateDescriptionRef boilerplate_descriptor,
      const compiler::FeedbackSource& feedback, int flags)
      : Base(bitfield),
        boilerplate_descriptor_(boilerplate_descriptor),
        feedback_(feedback),
        flags_(flags) {}

  compiler::ObjectBoilerplateDescriptionRef boilerplate_descriptor() {
    return boilerplate_descriptor_;
  }
  compiler::FeedbackSource feedback() const { return feedback_; }
  int flags() const { return flags_; }

  // The implementation currently calls runtime.
  static constexpr OpProperties kProperties =
      OpProperties::Call() | OpProperties::CanAllocate() |
      OpProperties::CanThrow() | OpProperties::LazyDeopt() |
      OpProperties::NotIdempotent();

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}

 private:
  const compiler::ObjectBoilerplateDescriptionRef boilerplate_descriptor_;
  const compiler::FeedbackSource feedback_;
  const int flags_;
};

class CreateShallowObjectLiteral
    : public FixedInputValueNodeT<0, CreateShallowObjectLiteral> {
  using Base = FixedInputValueNodeT<0, CreateShallowObjectLiteral>;

 public:
  explicit CreateShallowObjectLiteral(
      uint64_t bitfield,
      compiler::ObjectBoilerplateDescriptionRef boilerplate_descriptor,
      const compiler::FeedbackSource& feedback, int flags)
      : Base(bitfield),
        boilerplate_descriptor_(boilerplate_descriptor),
        feedback_(feedback),
        flags_(flags) {}

  // TODO(victorgomes): We should not need a boilerplate descriptor in
  // CreateShallowObjectLiteral.
  compiler::ObjectBoilerplateDescriptionRef boilerplate_descriptor() {
    return boilerplate_descriptor_;
  }
  compiler::FeedbackSource feedback() const { return feedback_; }
  int flags() const { return flags_; }

  // The implementation currently calls runtime.
  static constexpr OpProperties kProperties =
      OpProperties::GenericRuntimeOrBuiltinCall();

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}

 private:
  const compiler::ObjectBoilerplateDescriptionRef boilerplate_descriptor_;
  const compiler::FeedbackSource feedback_;
  const int flags_;
};

// VirtualObject is a ValueNode only for convenience, it should never be added
// to the Maglev graph.
class VirtualObject : public FixedInputValueNodeT<0, VirtualObject> {
  using Base = FixedInputValueNodeT<0, VirtualObject>;

 public:
  enum Type : uint8_t {
    kDefault,
    kHeapNumber,
    kFixedDoubleArray,
    kConsString,

    kLast = kConsString
  };

  friend std::ostream& operator<<(std::ostream& out, Type type) {
    switch (type) {
      case kDefault:
        out << "object";
        break;
      case kHeapNumber:
        out << "number";
        break;
      case kFixedDoubleArray:
        out << "double[]";
        break;
      case kConsString:
        out << "ConsString";
        break;
    }
    return out;
  }

  struct VirtualConsString {
    ValueNode* first() const { return data[0]; }
    ValueNode* second() const { return data[1]; }
    // Length and map are stored for constant folding but not actually part of
    // the virtual object as they are not needed to materialize the cons string.
    ValueNode* map;
    ValueNode* length;
    std::array<ValueNode*, 2> data;
  };

  explicit VirtualObject(uint64_t bitfield, int id,
                         const VirtualConsString& cons_string)
      : Base(bitfield), id_(id), type_(kConsString), cons_string_(cons_string) {
    DCHECK(!has_static_map());
  }

  explicit VirtualObject(uint64_t bitfield, compiler::MapRef map, int id,
                         uint32_t slot_count, ValueNode** slots)
      : Base(bitfield),
        map_(map),
        id_(id),
        type_(kDefault),
        slots_({slot_count, slots}) {
    DCHECK(has_static_map());
  }

  explicit VirtualObject(uint64_t bitfield, compiler::MapRef map, int id,
                         Float64 number)
      : Base(bitfield),
        map_(map),
        id_(id),
        type_(kHeapNumber),
        number_(number) {
    DCHECK(has_static_map());
  }

  explicit VirtualObject(uint64_t bitfield, compiler::MapRef map, int id,
                         uint32_t length,
                         compiler::FixedDoubleArrayRef elements)
      : Base(bitfield),
        map_(map),
        id_(id),
        type_(kFixedDoubleArray),
        double_array_({length, elements}) {
    DCHECK(has_static_map());
  }

  void SetValueLocationConstraints() { UNREACHABLE(); }
  void GenerateCode(MaglevAssembler*, const ProcessingState&) { UNREACHABLE(); }
  void PrintParams(std::ostream&) const;

  constexpr bool has_static_map() const {
    switch (type_) {
      case kDefault:
      case kHeapNumber:
      case kFixedDoubleArray:
        return true;
      case kConsString:
        return false;
    }
  }

  compiler::MapRef map() const {
    DCHECK(has_static_map());
    return *map_;
  }
  Type type() const { return type_; }
  uint32_t id() const { return id_; }

  size_t size() const {
    switch (type_) {
      case kDefault:
        return (slot_count() + 1) * kTaggedSize;
      case kConsString:
        return sizeof(ConsString);
      case kHeapNumber:
        return sizeof(HeapNumber);
      case kFixedDoubleArray:
        return FixedDoubleArray::SizeFor(double_elements_length());
    }
  }

  Float64 number() const {
    DCHECK_EQ(type_, kHeapNumber);
    return number_;
  }

  uint32_t double_elements_length() const {
    DCHECK_EQ(type_, kFixedDoubleArray);
    return double_array_.length;
  }

  compiler::FixedDoubleArrayRef double_elements() const {
    DCHECK_EQ(type_, kFixedDoubleArray);
    return double_array_.values;
  }

  ValueNode* get(uint32_t offset) const {
    DCHECK_NE(offset, 0);  // Don't try to get the map through this getter.
    DCHECK_EQ(type_, kDefault);
    offset -= kTaggedSize;
    SBXCHECK_LT(offset / kTaggedSize, slot_count());
    return slots_.data[offset / kTaggedSize];
  }

  void set(uint32_t offset, ValueNode* value) {
    DCHECK_NE(offset, 0);  // Don't try to set the map through this setter.
    DCHECK_EQ(type_, kDefault);
    DCHECK(!IsSnapshot());
    // Values set here can leak to the interpreter frame state. Conversions
    // should be stored in known_node_aspects/NodeInfo.
    DCHECK(!value->properties().is_conversion());
    offset -= kTaggedSize;
    SBXCHECK_LT(offset / kTaggedSize, slot_count());
    slots_.data[offset / kTaggedSize] = value;
  }

  ValueNode* string_length() const {
    DCHECK_EQ(type_, kConsString);
    return cons_string_.length;
  }

  const VirtualConsString& cons_string() const {
    DCHECK_EQ(type_, kConsString);
    return cons_string_;
  }

  void ClearSlots(int last_init_slot, ValueNode* clear_value) {
    DCHECK_EQ(type_, kDefault);
    int last_init_index = last_init_slot / kTaggedSize;
    for (uint32_t i = last_init_index; i < slot_count(); i++) {
      slots_.data[i] = clear_value;
    }
  }

  InlinedAllocation* allocation() const { return allocation_; }
  void set_allocation(InlinedAllocation* allocation) {
    allocation_ = allocation;
  }

  bool compatible_for_merge(const VirtualObject* other) const {
    if (type_ != other->type_) return false;
    if (allocation_ != other->allocation_) return false;
    // Currently, the graph builder will never change the VO map.
    if (has_static_map()) {
      if (map() != other->map()) return false;
    }
    switch (other->type_) {
      case kHeapNumber:
      case kFixedDoubleArray:
      case kConsString:
        return true;
      case kDefault:
        return slot_count() == other->slot_count();
    }
  }

  // VOs are snapshotted at branch points and when they are leaked to
  // DeoptInfos. This is because the snapshots need to preserve the original
  // values at the time of branching or deoptimization. While a VO is not yet
  // snapshotted, it can be modified freely.
  bool IsSnapshot() const { return snapshotted_; }
  void Snapshot() { snapshotted_ = true; }

  template <typename Function>
  inline void ForEachInput(Function&& callback) {
    switch (type_) {
      case kDefault:
        for (uint32_t i = 0; i < slot_count(); i++) {
          callback(slots_.data[i]);
        }
        break;
      case kConsString:
        for (ValueNode*& val : cons_string_.data) {
          callback(val);
        }
        break;
      case kHeapNumber:
        break;
      case kFixedDoubleArray:
        break;
    }
  }

  template <typename Function>
  inline void ForEachInput(Function&& callback) const {
    switch (type_) {
      case kDefault:
        for (uint32_t i = 0; i < slot_count(); i++) {
          callback(get_by_index(i));
        }
        break;
      case kConsString:
        for (ValueNode* val : cons_string_.data) {
          callback(val);
        }
        break;
      case kHeapNumber:
        break;
      case kFixedDoubleArray:
        break;
    }
  }

  // A runtime input is an input to the virtual object that has runtime
  // footprint, aka, a location.
  template <typename Function>
  inline void ForEachNestedRuntimeInput(VirtualObjectList virtual_objects,
                                        Function&& f);
  template <typename Function>
  inline void ForEachNestedRuntimeInput(VirtualObjectList virtual_objects,
                                        Function&& f) const;

  template <typename Function>
  inline std::optional<VirtualObject*> Merge(const VirtualObject* other,
                                             uint32_t new_object_id, Zone* zone,
                                             Function MergeValue) const {
    VirtualObject* result = Clone(new_object_id, zone, /* empty_clone */ true);
    DCHECK(compatible_for_merge(other));
    switch (type_) {
      // These objects are immutable and thus should never need merging.
      case kHeapNumber:
      case kFixedDoubleArray:
      case kConsString:
        UNREACHABLE();
      case kDefault: {
        for (uint32_t i = 0; i < slot_count(); i++) {
          if (auto success =
                  MergeValue(get_by_index(i), other->get_by_index(i))) {
            result->set_by_index(i, *success);
          } else {
            return {};
          }
        }
        break;
      }
    }
    return result;
  }

  VirtualObject* Clone(uint32_t new_object_id, Zone* zone,
                       bool empty_clone = false) const {
    VirtualObject* result;
    switch (type_) {
      case kHeapNumber:
      case kFixedDoubleArray:
      case kConsString:
        UNREACHABLE();
      case kDefault: {
        ValueNode** slots = zone->AllocateArray<ValueNode*>(slot_count());
        result = NodeBase::New<VirtualObject>(zone, 0, map(), new_object_id,
                                              slot_count(), slots);
        break;
      }
    }
    if (empty_clone) return result;

    // Copy content
    switch (type_) {
      case kHeapNumber:
      case kFixedDoubleArray:
      case kConsString:
        UNREACHABLE();
      case kDefault: {
        for (uint32_t i = 0; i < slot_count(); i++) {
          result->set_by_index(i, get_by_index(i));
        }
        break;
      }
    }
    result->set_allocation(allocation());
    return result;
  }

  uint32_t slot_count() const {
    DCHECK_EQ(type_, kDefault);
    return slots_.count;
  }

 private:
  ValueNode* get_by_index(uint32_t i) const {
    DCHECK_EQ(type_, kDefault);
    return slots_.data[i];
  }

  void set_by_index(uint32_t i, ValueNode* value) {
    DCHECK_EQ(type_, kDefault);
    // Values set here can leak to the interpreter. Conversions should be stored
    // in known_node_aspects/NodeInfo.
    DCHECK(!value->properties().is_conversion());
    slots_.data[i] = value;
  }

  struct DoubleArray {
    uint32_t length;
    compiler::FixedDoubleArrayRef values;
  };
  struct ObjectFields {
    uint32_t count;    // Does not count the map.
    ValueNode** data;  // Does not contain the map.
  };

  compiler::OptionalMapRef map_;
  const int id_;
  Type type_;  // We need to cache the type. We cannot do map comparison in some
               // parts of the pipeline, because we would need to dereference a
               // handle.
  bool snapshotted_ = false;  // Object should not be modified anymore.
  union {
    Float64 number_;
    DoubleArray double_array_;
    ObjectFields slots_;
    VirtualConsString cons_string_;
  };
  mutable InlinedAllocation* allocation_ = nullptr;

  VirtualObject* next_ = nullptr;
  friend VirtualObjectList;
};

class VirtualObjectList {
 public:
  VirtualObjectList() : head_(nullptr) {}

  explicit VirtualObjectList(VirtualObject* head) : head_(head) {}

  class Iterator final {
   public:
    explicit Iterator(VirtualObject* entry) : entry_(entry) {}

    Iterator& operator++() {
      entry_ = entry_->next_;
      return *this;
    }
    bool operator==(const Iterator& other) const {
      return entry_ == other.entry_;
    }
    bool operator!=(const Iterator& other) const {
      return entry_ != other.entry_;
    }
    VirtualObject*& operator*() { return entry_; }
    VirtualObject* operator->() { return entry_; }

   private:
    VirtualObject* entry_;
  };

  bool operator==(const VirtualObjectList& other) const {
    return head_ == other.head_;
  }

  void Add(VirtualObject* object) {
    DCHECK_NOT_NULL(object);
    DCHECK_NULL(object->next_);
    object->next_ = head_;
    head_ = object;
  }

  bool is_empty() const { return head_ == nullptr; }

  VirtualObject* FindAllocatedWith(const InlinedAllocation* allocation) const {
    VirtualObject* result = nullptr;
    for (VirtualObject* vo : *this) {
      if (vo->allocation() == allocation) {
        result = vo;
        break;
      }
    }
    return result;
  }

  void Print(std::ostream& os, const char* prefix) const;

  // It iterates both list in reverse other of ids until a common point.
  template <typename Function>
  static VirtualObject* WalkUntilCommon(const VirtualObjectList& list1,
                                        const VirtualObjectList& list2,
                                        Function&& f) {
    VirtualObject* vo1 = list1.head_;
    VirtualObject* vo2 = list2.head_;
    while (vo1 != nullptr && vo2 != nullptr && vo1 != vo2) {
      DCHECK_NE(vo1->id(), vo2->id());
      if (vo1->id() > vo2->id()) {
        f(vo1, list1);
        vo1 = vo1->next_;
      } else {
        f(vo2, list2);
        vo2 = vo2->next_;
      }
    }
    if (vo1 == vo2) return vo1;
    return nullptr;
  }

  void Snapshot() const {
    for (VirtualObject* vo : *this) {
      if (vo->IsSnapshot()) {
        // Stop processing once a snapshotted object is found, as all remaining
        // objects must be snapshotted.
        break;
      }
      vo->Snapshot();
    }
    SLOW_DCHECK(IsSnapshot());
  }

  bool IsSnapshot() const {
    for (VirtualObject* vo : *this) {
      if (!vo->IsSnapshot()) return false;
    }
    return true;
  }

  VirtualObject* head() const { return head_; }

  Iterator begin() const { return Iterator(head_); }
  Iterator end() const { return Iterator(nullptr); }

 private:
  VirtualObject* head_;
};

enum class EscapeAnalysisResult {
  kUnknown,
  kElided,
  kEscaped,
};

class InlinedAllocation : public FixedInputValueNodeT<1, InlinedAllocation> {
  using Base = FixedInputValueNodeT<1, InlinedAllocation>;

 public:
  using List = base::ThreadedList<InlinedAllocation>;

  explicit InlinedAllocation(uint64_t bitfield, VirtualObject* object)
      : Base(bitfield),
        object_(object),
        escape_analysis_result_(EscapeAnalysisResult::kUnknown) {}

  Input allocation_block_input() { return input(0); }
  ConstInput allocation_block_input() const { return input(0); }
  AllocationBlock* allocation_block();
  const AllocationBlock* allocation_block() const;

  static constexpr OpProperties kProperties = OpProperties::NotIdempotent();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

  void VerifyInputs() const;

  size_t size() const { return object_->size(); }

  VirtualObject* object() const { return object_; }

  int offset() const {
    DCHECK_NE(offset_, -1);
    return offset_;
  }
  void set_offset(int offset) { offset_ = offset; }

  int non_escaping_use_count() const { return non_escaping_use_count_; }

  void RemoveNonEscapingUses(int n = 1) {
    non_escaping_use_count_ = std::max(non_escaping_use_count_ - n, 0);
  }
  void AddNonEscapingUses(int n = 1) {
    DCHECK(!HasBeenAnalysed());
    non_escaping_use_count_ += n;
  }
  bool IsEscaping() const {
    DCHECK(!HasBeenAnalysed());
    return use_count_ > non_escaping_use_count_;
  }
  void ForceEscaping() {
    DCHECK(!HasBeenAnalysed());
    non_escaping_use_count_ = 0;
  }

  void SetElided() {
    DCHECK_EQ(escape_analysis_result_, EscapeAnalysisResult::kUnknown);
    escape_analysis_result_ = EscapeAnalysisResult::kElided;
  }
  void SetEscaped() {
    // We allow transitions from elided to escaped.
    DCHECK_NE(escape_analysis_result_, EscapeAnalysisResult::kEscaped);
    escape_analysis_result_ = EscapeAnalysisResult::kEscaped;
  }
  bool HasBeenElided() const {
    DCHECK(HasBeenAnalysed());
    DCHECK_GE(use_count_, non_escaping_use_count_);
    return escape_analysis_result_ == EscapeAnalysisResult::kElided;
  }
  bool HasEscaped() const {
    DCHECK(HasBeenAnalysed());
    return escape_analysis_result_ == EscapeAnalysisResult::kEscaped;
  }
  bool HasBeenAnalysed() const {
    return escape_analysis_result_ != EscapeAnalysisResult::kUnknown;
  }

  void UpdateObject(VirtualObject* object) {
    DCHECK_EQ(this, object->allocation());
    object_ = object;
  }

#ifdef DEBUG
  void set_is_returned_value_from_inline_call() {
    is_returned_value_from_inline_call_ = true;
  }

  bool is_returned_value_from_inline_call() const {
    return is_returned_value_from_inline_call_;
  }
#endif  // DEBUG

 private:
  VirtualObject* object_;
  EscapeAnalysisResult escape_analysis_result_;
  int non_escaping_use_count_ = 0;
  int offset_ = -1;  // Set by AllocationBlock.

#ifdef DEBUG
  bool is_returned_value_from_inline_call_ = false;
#endif  // DEBUG

  InlinedAllocation* next_ = nullptr;
  InlinedAllocation** next() { return &next_; }

  friend List;
  friend base::ThreadedListTraits<InlinedAllocation>;
};

void ValueNode::remove_use() {
  // Make sure a saturated use count won't drop below zero.
  DCHECK_GT(use_count_, 0);
  use_count_--;
  if (auto alloc = TryCast<InlinedAllocation>()) {
    // Unfortunately we cannot know if the removed use was escaping or not. To
    // be safe we need to assume it wasn't.
    alloc->RemoveNonEscapingUses(1);
  }
}

template <typename Function>
inline void VirtualObject::ForEachNestedRuntimeInput(
    VirtualObjectList virtual_objects, Function&& f) {
  ForEachInput([&](ValueNode*& value) {
    value = value->UnwrapIdentities();
    if (IsConstantNode(value->opcode())) {
      // No location assigned to constants.
      return;
    }
    // Special nodes.
    switch (value->opcode()) {
      case Opcode::kArgumentsElements:
      case Opcode::kArgumentsLength:
      case Opcode::kRestLength:
        // No location assigned to these opcodes.
        break;
      case Opcode::kVirtualObject:
        UNREACHABLE();
      case Opcode::kInlinedAllocation: {
        InlinedAllocation* alloc = value->Cast<InlinedAllocation>();
        VirtualObject* inner_vobject = virtual_objects.FindAllocatedWith(alloc);
        // Check if it has escaped.
        if (inner_vobject &&
            (!alloc->HasBeenAnalysed() || alloc->HasBeenElided())) {
          inner_vobject->ForEachNestedRuntimeInput(virtual_objects, f);
        } else {
          f(value);
        }
        break;
      }
      default:
        f(value);
        break;
    }
  });
}

template <typename Function>
inline void VirtualObject::ForEachNestedRuntimeInput(
    VirtualObjectList virtual_objects, Function&& f) const {
  ForEachInput([&](ValueNode* value) {
    value = value->UnwrapIdentities();
    if (IsConstantNode(value->opcode())) {
      // No location assigned to constants.
      return;
    }
    // Special nodes.
    switch (value->opcode()) {
      case Opcode::kArgumentsElements:
      case Opcode::kArgumentsLength:
      case Opcode::kRestLength:
        // No location assigned to these opcodes.
        break;
      case Opcode::kVirtualObject:
        UNREACHABLE();
      case Opcode::kInlinedAllocation: {
        InlinedAllocation* alloc = value->Cast<InlinedAllocation>();
        VirtualObject* inner_vobject = virtual_objects.FindAllocatedWith(alloc);
        // Check if it has escaped.
        if (inner_vobject &&
            (!alloc->HasBeenAnalysed() || alloc->HasBeenElided())) {
          inner_vobject->ForEachNestedRuntimeInput(virtual_objects, f);
        } else {
          f(value);
        }
        break;
      }
      default:
        f(value);
        break;
    }
  });
}

class AllocationBlock : public FixedInputValueNodeT<0, AllocationBlock> {
  using Base = FixedInputValueNodeT<0, AllocationBlock>;

 public:
  explicit AllocationBlock(uint64_t bitfield, AllocationType allocation_type)
      : Base(bitfield), allocation_type_(allocation_type) {}

  static constexpr OpProperties kProperties = OpProperties::CanAllocate() |
                                              OpProperties::DeferredCall() |
                                              OpProperties::NotIdempotent();

  int MaxCallStackArgs() const { return 0; }
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

  AllocationType allocation_type() const { return allocation_type_; }
  int size() const { return size_; }
  void set_size(int size) { size_ = size; }

  InlinedAllocation::List& allocation_list() { return allocation_list_; }

  void Add(InlinedAllocation* alloc) {
    allocation_list_.Add(alloc);
    size_ += alloc->size();
  }

  void TryPretenure(ValueNode* input);

  bool elided_write_barriers_depend_on_type() const {
    return elided_write_barriers_depend_on_type_;
  }
  void set_elided_write_barriers_depend_on_type() {
    elided_write_barriers_depend_on_type_ = true;
  }

 private:
  void TryPretenure();
  AllocationType allocation_type_;
  int size_ = 0;
  bool elided_write_barriers_depend_on_type_ = false;
  InlinedAllocation::List allocation_list_;
};

class ArgumentsLength : public FixedInputValueNodeT<0, ArgumentsLength> {
  using Base = FixedInputValueNodeT<0, ArgumentsLength>;

 public:
  explicit ArgumentsLength(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::Int32();

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class RestLength : public FixedInputValueNodeT<0, RestLength> {
  using Base = FixedInputValueNodeT<0, RestLength>;

 public:
  explicit RestLength(uint64_t bitfield, int formal_parameter_count)
      : Base(bitfield), formal_parameter_count_(formal_parameter_count) {}

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}

  int formal_parameter_count() const { return formal_parameter_count_; }

  auto options() const { return std::tuple{formal_parameter_count_}; }

 private:
  int formal_parameter_count_;
};

class ArgumentsElements : public FixedInputValueNodeT<1, ArgumentsElements> {
  using Base = FixedInputValueNodeT<1, ArgumentsElements>;

 public:
  explicit ArgumentsElements(uint64_t bitfield, CreateArgumentsType type,
                             int formal_parameter_count)
      : Base(bitfield),
        type_(type),
        formal_parameter_count_(formal_parameter_count) {}

  static constexpr OpProperties kProperties = OpProperties::Call() |
                                              OpProperties::CanAllocate() |
                                              OpProperties::NotIdempotent();

  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  Input arguments_count_input() { return input(0); }

  int MaxCallStackArgs() const { return 0; }
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}

  CreateArgumentsType type() const { return type_; }
  int formal_parameter_count() const { return formal_parameter_count_; }

 private:
  CreateArgumentsType type_;
  int formal_parameter_count_;
};

// TODO(victorgomes): This node is currently not eliminated by the escape
// analysis.
class AllocateElementsArray
    : public FixedInputValueNodeT<1, AllocateElementsArray> {
  using Base = FixedInputValueNodeT<1, AllocateElementsArray>;

 public:
  explicit AllocateElementsArray(uint64_t bitfield,
                                 AllocationType allocation_type)
      : Base(bitfield), allocation_type_(allocation_type) {}

  static constexpr OpProperties kProperties =
      OpProperties::CanAllocate() | OpProperties::EagerDeopt() |
      OpProperties::DeferredCall() | OpProperties::NotIdempotent();

  Input length_input() { return input(0); }

  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kInt32};

  int MaxCallStackArgs() const { return 0; }
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}

  AllocationType allocation_type() const { return allocation_type_; }

 private:
  AllocationType allocation_type_;
};

class CreateFunctionContext
    : public FixedInputValueNodeT<1, CreateFunctionContext> {
  using Base = FixedInputValueNodeT<1, CreateFunctionContext>;

 public:
  explicit CreateFunctionContext(uint64_t bitfield,
                                 compiler::ScopeInfoRef scope_info,
                                 uint32_t slot_count, ScopeType scope_type)
      : Base(bitfield),
        scope_info_(scope_info),
        slot_count_(slot_count),
        scope_type_(scope_type) {}

  compiler::ScopeInfoRef scope_info() const { return scope_info_; }
  uint32_t slot_count() const { return slot_count_; }
  ScopeType scope_type() const { return scope_type_; }

  Input context() { return input(0); }

  // The implementation currently calls runtime.
  static constexpr OpProperties kProperties =
      OpProperties::Call() | OpProperties::CanAllocate() |
      OpProperties::LazyDeopt() | OpProperties::NotIdempotent();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

 private:
  const compiler::ScopeInfoRef scope_info_;
  const uint32_t slot_count_;
  ScopeType scope_type_;
};

class FastCreateClosure : public FixedInputValueNodeT<1, FastCreateClosure> {
  using Base = FixedInputValueNodeT<1, FastCreateClosure>;

 public:
  explicit FastCreateClosure(
      uint64_t bitfield, compiler::SharedFunctionInfoRef shared_function_info,
      compiler::FeedbackCellRef feedback_cell)
      : Base(bitfield),
        shared_function_info_(shared_function_info),
        feedback_cell_(feedback_cell) {}

  compiler::SharedFunctionInfoRef shared_function_info() const {
    return shared_function_info_;
  }
  compiler::FeedbackCellRef feedback_cell() const { return feedback_cell_; }

  Input context() { return input(0); }

  // The implementation currently calls runtime.
  static constexpr OpProperties kProperties =
      OpProperties::Call() | OpProperties::CanAllocate() |
      OpProperties::LazyDeopt() | OpProperties::NotIdempotent();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

 private:
  const compiler::SharedFunctionInfoRef shared_function_info_;
  const compiler::FeedbackCellRef feedback_cell_;
};

class CreateRegExpLiteral
    : public FixedInputValueNodeT<0, CreateRegExpLiteral> {
  using Base = FixedInputValueNodeT<0, CreateRegExpLiteral>;

 public:
  explicit CreateRegExpLiteral(uint64_t bitfield, compiler::StringRef pattern,
                               const compiler::FeedbackSource& feedback,
                               int flags)
      : Base(bitfield), pattern_(pattern), feedback_(feedback), flags_(flags) {}

  compiler::StringRef pattern() { return pattern_; }
  compiler::FeedbackSource feedback() const { return feedback_; }
  int flags() const { return flags_; }

  // The implementation currently calls runtime.
  static constexpr OpProperties kProperties =
      OpProperties::Call() | OpProperties::CanAllocate() |
      OpProperties::NotIdempotent() | OpProperties::CanThrow();

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}

 private:
  compiler::StringRef pattern_;
  const compiler::FeedbackSource feedback_;
  const int flags_;
};

class CreateClosure : public FixedInputValueNodeT<1, CreateClosure> {
  using Base = FixedInputValueNodeT<1, CreateClosure>;

 public:
  explicit CreateClosure(uint64_t bitfield,
                         compiler::SharedFunctionInfoRef shared_function_info,
                         compiler::FeedbackCellRef feedback_cell,
                         bool pretenured)
      : Base(bitfield),
        shared_function_info_(shared_function_info),
        feedback_cell_(feedback_cell),
        pretenured_(pretenured) {}

  compiler::SharedFunctionInfoRef shared_function_info() const {
    return shared_function_info_;
  }
  compiler::FeedbackCellRef feedback_cell() const { return feedback_cell_; }
  bool pretenured() const { return pretenured_; }

  Input context() { return input(0); }

  // The implementation currently calls runtime.
  static constexpr OpProperties kProperties = OpProperties::Call() |
                                              OpProperties::CanAllocate() |
                                              OpProperties::NotIdempotent();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

 private:
  const compiler::SharedFunctionInfoRef shared_function_info_;
  const compiler::FeedbackCellRef feedback_cell_;
  const bool pretenured_;
};

#define ASSERT_CONDITION(V) \
  V(Equal)                  \
  V(NotEqual)               \
  V(LessThan)               \
  V(LessThanEqual)          \
  V(GreaterThan)            \
  V(GreaterThanEqual)       \
  V(UnsignedLessThan)       \
  V(UnsignedLessThanEqual)  \
  V(UnsignedGreaterThan)    \
  V(UnsignedGreaterThanEqual)

enum class AssertCondition {
#define D(Name) k##Name,
  ASSERT_CONDITION(D)
#undef D
};
static constexpr int kNumAssertConditions =
#define D(Name) +1
    0 ASSERT_CONDITION(D);
#undef D

inline std::ostream& operator<<(std::ostream& os, const AssertCondition cond) {
  switch (cond) {
#define CASE(Name)               \
  case AssertCondition::k##Name: \
    os << #Name;                 \
    break;
    ASSERT_CONDITION(CASE)
#undef CASE
  }
  return os;
}

class AssertInt32 : public FixedInputNodeT<2, AssertInt32> {
  using Base = FixedInputNodeT<2, AssertInt32>;

 public:
  explicit AssertInt32(uint64_t bitfield, AssertCondition condition,
                       AbortReason reason)
      : Base(bitfield), condition_(condition), reason_(reason) {}

  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kInt32, ValueRepresentation::kInt32};

  Input left_input() { return input(0); }
  Input right_input() { return input(1); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

  auto options() const { return std::tuple{condition_, reason_}; }

  AssertCondition condition() const { return condition_; }
  AbortReason reason() const { return reason_; }

 private:
  AssertCondition condition_;
  AbortReason reason_;
};

class CheckMaps : public FixedInputNodeT<1, CheckMaps> {
  using Base = FixedInputNodeT<1, CheckMaps>;

 public:
  explicit CheckMaps(uint64_t bitfield, const compiler::ZoneRefSet<Map>& maps,
                     CheckType check_type)
      : Base(CheckTypeBitField::update(bitfield, check_type)), maps_(maps) {}
  explicit CheckMaps(uint64_t bitfield,
                     base::Vector<const compiler::MapRef> maps,
                     CheckType check_type, Zone* zone)
      : Base(CheckTypeBitField::update(bitfield, check_type)),
        maps_(maps.begin(), maps.end(), zone) {}

  static constexpr OpProperties kProperties =
      OpProperties::EagerDeopt() | OpProperties::CanRead();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  const compiler::ZoneRefSet<Map>& maps() const { return maps_; }
  CheckType check_type() const { return CheckTypeBitField::decode(bitfield()); }

  static constexpr int kReceiverIndex = 0;
  Input receiver_input() { return input(kReceiverIndex); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

  auto options() const { return std::tuple{maps_, check_type()}; }

 private:
  using CheckTypeBitField = NextBitField<CheckType, 1>;
  const compiler::ZoneRefSet<Map> maps_;
};

class CheckMapsWithMigrationAndDeopt
    : public FixedInputNodeT<1, CheckMapsWithMigrationAndDeopt> {
  using Base = FixedInputNodeT<1, CheckMapsWithMigrationAndDeopt>;

 public:
  explicit CheckMapsWithMigrationAndDeopt(uint64_t bitfield,
                                          const compiler::ZoneRefSet<Map>& maps,
                                          CheckType check_type)
      : Base(CheckTypeBitField::update(bitfield, check_type)), maps_(maps) {}
  explicit CheckMapsWithMigrationAndDeopt(
      uint64_t bitfield, base::Vector<const compiler::MapRef> maps,
      CheckType check_type, Zone* zone)
      : Base(CheckTypeBitField::update(bitfield, check_type)),
        maps_(maps.begin(), maps.end(), zone) {}

  static constexpr OpProperties kProperties =
      OpProperties::EagerDeopt() | OpProperties::DeferredCall() |
      OpProperties::CanAllocate() | OpProperties::CanWrite() |
      OpProperties::CanRead();

  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  const compiler::ZoneRefSet<Map>& maps() const { return maps_; }
  CheckType check_type() const { return CheckTypeBitField::decode(bitfield()); }

  static constexpr int kReceiverIndex = 0;
  Input receiver_input() { return input(kReceiverIndex); }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

  auto options() const { return std::tuple{maps_, check_type()}; }

 private:
  using CheckTypeBitField = NextBitField<CheckType, 1>;
  const compiler::ZoneRefSet<Map> maps_;
};

class CheckMapsWithAlreadyLoadedMap
    : public FixedInputNodeT<2, CheckMapsWithAlreadyLoadedMap> {
  using Base = FixedInputNodeT<2, CheckMapsWithAlreadyLoadedMap>;

 public:
  explicit CheckMapsWithAlreadyLoadedMap(uint64_t bitfield,
                                         const compiler::ZoneRefSet<Map>& maps)
      : Base(bitfield), maps_(maps) {}
  explicit CheckMapsWithAlreadyLoadedMap(
      uint64_t bitfield, base::Vector<const compiler::MapRef> maps, Zone* zone)
      : Base(bitfield), maps_(maps.begin(), maps.end(), zone) {}

  static constexpr OpProperties kProperties =
      OpProperties::EagerDeopt() | OpProperties::CanRead();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged};

  const compiler::ZoneRefSet<Map>& maps() const { return maps_; }

  Input object_input() { return input(0); }
  Input map_input() { return input(1); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

  auto options() const { return std::tuple{maps_}; }

 private:
  const compiler::ZoneRefSet<Map> maps_;
};

class CheckValue : public FixedInputNodeT<1, CheckValue> {
  using Base = FixedInputNodeT<1, CheckValue>;

 public:
  explicit CheckValue(uint64_t bitfield, const compiler::HeapObjectRef value,
                      DeoptimizeReason reason)
      : Base(bitfield | ReasonField::encode(reason)), value_(value) {}

  static constexpr OpProperties kProperties = OpProperties::EagerDeopt();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  compiler::HeapObjectRef value() const { return value_; }

  static constexpr int kTargetIndex = 0;
  Input target_input() { return input(kTargetIndex); }

#ifdef V8_COMPRESS_POINTERS
  void MarkTaggedInputsAsDecompressing() {
    // Don't need to decompress to compare reference equality.
  }
#endif

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

  auto options() const { return std::tuple{value_, deoptimize_reason()}; }

  DEOPTIMIZE_REASON_FIELD

 private:
  const compiler::HeapObjectRef value_;
};

class CheckValueEqualsInt32 : public FixedInputNodeT<1, CheckValueEqualsInt32> {
  using Base = FixedInputNodeT<1, CheckValueEqualsInt32>;

 public:
  explicit CheckValueEqualsInt32(uint64_t bitfield, int32_t value,
                                 DeoptimizeReason reason)
      : Base(bitfield | ReasonField::encode(reason)), value_(value) {}

  static constexpr OpProperties kProperties = OpProperties::EagerDeopt();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kInt32};

  int32_t value() const { return value_; }

  static constexpr int kTargetIndex = 0;
  Input target_input() { return input(kTargetIndex); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

  auto options() const { return std::tuple{value_, deoptimize_reason()}; }

  DEOPTIMIZE_REASON_FIELD

 private:
  const int32_t value_;
};

class CheckFloat64SameValue : public FixedInputNodeT<1, CheckFloat64SameValue> {
  using Base = FixedInputNodeT<1, CheckFloat64SameValue>;

 public:
  explicit CheckFloat64SameValue(uint64_t bitfield, Float64 value,
                                 DeoptimizeReason reason)
      : Base(bitfield | ReasonField::encode(reason)), value_(value) {}

  static constexpr OpProperties kProperties = OpProperties::EagerDeopt();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kFloat64};

  Float64 value() const { return value_; }

  static constexpr int kTargetIndex = 0;
  Input target_input() { return input(kTargetIndex); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

  auto options() const { return std::tuple{value_, deoptimize_reason()}; }

  DEOPTIMIZE_REASON_FIELD

 private:
  const Float64 value_;
};

class CheckValueEqualsString
    : public FixedInputNodeT<1, CheckValueEqualsString> {
  using Base = FixedInputNodeT<1, CheckValueEqualsString>;

 public:
  explicit CheckValueEqualsString(uint64_t bitfield,
                                  compiler::InternalizedStringRef value,
                                  DeoptimizeReason reason)
      : Base(bitfield | ReasonField::encode(reason)), value_(value) {}

  // Can allocate if strings are flattened for comparison.
  static constexpr OpProperties kProperties = OpProperties::CanAllocate() |
                                              OpProperties::EagerDeopt() |
                                              OpProperties::DeferredCall();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  compiler::InternalizedStringRef value() const { return value_; }

  static constexpr int kTargetIndex = 0;
  Input target_input() { return input(kTargetIndex); }

  int MaxCallStackArgs() const { return 0; }
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

  auto options() const { return std::tuple{value_, deoptimize_reason()}; }

  DEOPTIMIZE_REASON_FIELD

 private:
  const compiler::InternalizedStringRef value_;
};

class CheckDynamicValue : public FixedInputNodeT<2, CheckDynamicValue> {
  using Base = FixedInputNodeT<2, CheckDynamicValue>;

 public:
  explicit CheckDynamicValue(uint64_t bitfield, DeoptimizeReason reason)
      : Base(bitfield | ReasonField::encode(reason)) {}

  static constexpr OpProperties kProperties = OpProperties::EagerDeopt();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged};

  static constexpr int kFirstIndex = 0;
  static constexpr int kSecondIndex = 1;
  Input first_input() { return input(kFirstIndex); }
  Input second_input() { return input(kSecondIndex); }

#ifdef V8_COMPRESS_POINTERS
  void MarkTaggedInputsAsDecompressing() {
    // Don't need to decompress to compare reference equality.
  }
#endif

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

  auto options() const { return std::tuple{deoptimize_reason()}; }

  DEOPTIMIZE_REASON_FIELD
};

class CheckSmi : public FixedInputNodeT<1, CheckSmi> {
  using Base = FixedInputNodeT<1, CheckSmi>;

 public:
  explicit CheckSmi(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::EagerDeopt();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  static constexpr int kReceiverIndex = 0;
  Input receiver_input() { return input(kReceiverIndex); }

  using Node::set_input;

#ifdef V8_COMPRESS_POINTERS
  void MarkTaggedInputsAsDecompressing() {
    // Don't need to decompress to check Smi bits.
  }
#endif

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class CheckNumber : public FixedInputNodeT<1, CheckNumber> {
  using Base = FixedInputNodeT<1, CheckNumber>;

 public:
  explicit CheckNumber(uint64_t bitfield, Object::Conversion mode)
      : Base(bitfield), mode_(mode) {}

  static constexpr OpProperties kProperties = OpProperties::EagerDeopt();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  static constexpr int kReceiverIndex = 0;
  Input receiver_input() { return input(kReceiverIndex); }
  Object::Conversion mode() const { return mode_; }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}

  auto options() const { return std::tuple{mode_}; }

 private:
  const Object::Conversion mode_;
};

class CheckHeapObject : public FixedInputNodeT<1, CheckHeapObject> {
  using Base = FixedInputNodeT<1, CheckHeapObject>;

 public:
  explicit CheckHeapObject(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::EagerDeopt();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  static constexpr int kReceiverIndex = 0;
  Input receiver_input() { return input(kReceiverIndex); }

#ifdef V8_COMPRESS_POINTERS
  void MarkTaggedInputsAsDecompressing() {
    // Don't need to decompress to check Smi bits.
  }
#endif

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class CheckSymbol : public FixedInputNodeT<1, CheckSymbol> {
  using Base = FixedInputNodeT<1, CheckSymbol>;

 public:
  explicit CheckSymbol(uint64_t bitfield, CheckType check_type)
      : Base(CheckTypeBitField::update(bitfield, check_type)) {}

  static constexpr OpProperties kProperties = OpProperties::EagerDeopt();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  static constexpr int kReceiverIndex = 0;
  Input receiver_input() { return input(kReceiverIndex); }
  CheckType check_type() const { return CheckTypeBitField::decode(bitfield()); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}

  auto options() const { return std::tuple{check_type()}; }

 private:
  using CheckTypeBitField = NextBitField<CheckType, 1>;
};

class CheckInstanceType : public FixedInputNodeT<1, CheckInstanceType> {
  using Base = FixedInputNodeT<1, CheckInstanceType>;

 public:
  explicit CheckInstanceType(uint64_t bitfield, CheckType check_type,
                             const InstanceType first_instance_type,
                             const InstanceType last_instance_type)
      : Base(CheckTypeBitField::update(bitfield, check_type)),
        first_instance_type_(first_instance_type),
        last_instance_type_(last_instance_type) {
    DCHECK_LE(first_instance_type, last_instance_type);
  }

  static constexpr OpProperties kProperties = OpProperties::EagerDeopt();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  static constexpr int kReceiverIndex = 0;
  Input receiver_input() { return input(kReceiverIndex); }

  CheckType check_type() const { return CheckTypeBitField::decode(bitfield()); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

  auto options() const {
    return std::tuple{check_type(), first_instance_type_, last_instance_type_};
  }

  InstanceType first_instance_type() const { return first_instance_type_; }
  InstanceType last_instance_type() const { return last_instance_type_; }

 private:
  using CheckTypeBitField = NextBitField<CheckType, 1>;
  const InstanceType first_instance_type_;
  const InstanceType last_instance_type_;
};

class CheckString : public FixedInputNodeT<1, CheckString> {
  using Base = FixedInputNodeT<1, CheckString>;

 public:
  explicit CheckString(uint64_t bitfield, CheckType check_type)
      : Base(CheckTypeBitField::update(bitfield, check_type)) {}

  static constexpr OpProperties kProperties = OpProperties::EagerDeopt();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  static constexpr int kReceiverIndex = 0;
  Input receiver_input() { return input(kReceiverIndex); }
  CheckType check_type() const { return CheckTypeBitField::decode(bitfield()); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}

  auto options() const { return std::tuple{check_type()}; }

 private:
  using CheckTypeBitField = NextBitField<CheckType, 1>;
};

class CheckSeqOneByteString : public FixedInputNodeT<1, CheckSeqOneByteString> {
  using Base = FixedInputNodeT<1, CheckSeqOneByteString>;

 public:
  explicit CheckSeqOneByteString(uint64_t bitfield, CheckType check_type)
      : Base(CheckTypeBitField::update(bitfield, check_type)) {}

  static constexpr OpProperties kProperties = OpProperties::EagerDeopt();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  static constexpr int kReceiverIndex = 0;
  Input receiver_input() { return input(kReceiverIndex); }
  CheckType check_type() const { return CheckTypeBitField::decode(bitfield()); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}

  auto options() const { return std::tuple{check_type()}; }

 private:
  using CheckTypeBitField = NextBitField<CheckType, 1>;
};

class CheckStringOrStringWrapper
    : public FixedInputNodeT<1, CheckStringOrStringWrapper> {
  using Base = FixedInputNodeT<1, CheckStringOrStringWrapper>;

 public:
  explicit CheckStringOrStringWrapper(uint64_t bitfield, CheckType check_type)
      : Base(CheckTypeBitField::update(bitfield, check_type)) {}

  static constexpr OpProperties kProperties = OpProperties::EagerDeopt();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  static constexpr int kReceiverIndex = 0;
  Input receiver_input() { return input(kReceiverIndex); }
  CheckType check_type() const { return CheckTypeBitField::decode(bitfield()); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}

  auto options() const { return std::tuple{check_type()}; }

 private:
  using CheckTypeBitField = NextBitField<CheckType, 1>;
};

class CheckStringOrOddball : public FixedInputNodeT<1, CheckStringOrOddball> {
  using Base = FixedInputNodeT<1, CheckStringOrOddball>;

 public:
  explicit CheckStringOrOddball(uint64_t bitfield, CheckType check_type)
      : Base(CheckTypeBitField::update(bitfield, check_type)) {}

  static constexpr OpProperties kProperties = OpProperties::EagerDeopt();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  static constexpr int kReceiverIndex = 0;
  Input receiver_input() { return input(kReceiverIndex); }
  CheckType check_type() const { return CheckTypeBitField::decode(bitfield()); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}

  auto options() const { return std::tuple{check_type()}; }

 private:
  using CheckTypeBitField = NextBitField<CheckType, 1>;
};

class CheckDetectableCallable
    : public FixedInputNodeT<1, CheckDetectableCallable> {
  using Base = FixedInputNodeT<1, CheckDetectableCallable>;

 public:
  explicit CheckDetectableCallable(uint64_t bitfield, CheckType check_type)
      : Base(CheckTypeBitField::update(bitfield, check_type)) {}

  static constexpr OpProperties kProperties = OpProperties::EagerDeopt();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  static constexpr int kReceiverIndex = 0;
  Input receiver_input() { return input(kReceiverIndex); }
  CheckType check_type() const { return CheckTypeBitField::decode(bitfield()); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream& out) const {}

  auto options() const { return std::tuple{check_type()}; }

 private:
  using CheckTypeBitField = NextBitField<CheckType, 1>;
};

class CheckMapsWithMigration
    : public FixedInputNodeT<1, CheckMapsWithMigration> {
  using Base = FixedInputNodeT<1, CheckMapsWithMigration>;

 public:
  explicit CheckMapsWithMigration(uint64_t bitfield,
                                  const compiler::ZoneRefSet<Map>& maps,
                                  CheckType check_type)
      : Base(CheckTypeBitField::update(bitfield, check_type)), maps_(maps) {}

  static constexpr OpProperties kProperties =
      OpProperties::EagerDeopt() | OpProperties::DeferredCall() |
      OpProperties::CanAllocate() | OpProperties::CanWrite() |
      OpProperties::CanRead();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  const compiler::ZoneRefSet<Map>& maps() const { return maps_; }

  static constexpr int kReceiverIndex = 0;
  Input receiver_input() { return input(kReceiverIndex); }
  CheckType check_type() const { return CheckTypeBitField::decode(bitfield()); }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

  void ClearUnstableNodeAspects(bool is_tracing_enabled,
                                KnownNodeAspects& known_node_aspects);

 private:
  using CheckTypeBitField = NextBitField<CheckType, 1>;
  const compiler::ZoneRefSet<Map> maps_;
};

class MigrateMapIfNeeded : public FixedInputValueNodeT<2, MigrateMapIfNeeded> {
  using Base = FixedInputValueNodeT<2, MigrateMapIfNeeded>;

 public:
  explicit MigrateMapIfNeeded(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties =
      OpProperties::EagerDeopt() | OpProperties::DeferredCall() |
      OpProperties::CanAllocate() | OpProperties::CanWrite() |
      OpProperties::CanRead();

  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged};

  static constexpr int kMapIndex = 0;
  static constexpr int kObjectIndex = 1;

  Input object_input() { return input(kObjectIndex); }
  Input map_input() { return input(kMapIndex); }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}

  void ClearUnstableNodeAspects(bool is_tracing_enabled,
                                KnownNodeAspects& known_node_aspects);
};

class CheckCacheIndicesNotCleared
    : public FixedInputNodeT<2, CheckCacheIndicesNotCleared> {
  using Base = FixedInputNodeT<2, CheckCacheIndicesNotCleared>;

 public:
  explicit CheckCacheIndicesNotCleared(uint64_t bitfield) : Base(bitfield) {}
  static constexpr OpProperties kProperties =
      OpProperties::EagerDeopt() | OpProperties::CanRead();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kInt32};

  static constexpr int kEnumIndices = 0;
  Input indices_input() { return input(kEnumIndices); }
  static constexpr int kCacheLength = 1;
  Input length_input() { return input(kCacheLength); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class CheckJSDataViewBounds : public FixedInputNodeT<2, CheckJSDataViewBounds> {
  using Base = FixedInputNodeT<2, CheckJSDataViewBounds>;

 public:
  explicit CheckJSDataViewBounds(uint64_t bitfield,
                                 ExternalArrayType element_type)
      : Base(bitfield), element_type_(element_type) {}

  static constexpr OpProperties kProperties = OpProperties::EagerDeopt();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kInt32};

  static constexpr int kReceiverIndex = 0;
  static constexpr int kIndexIndex = 1;
  Input receiver_input() { return input(kReceiverIndex); }
  Input index_input() { return input(kIndexIndex); }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}

  auto options() const { return std::tuple{element_type_}; }

  ExternalArrayType element_type() const { return element_type_; }

 private:
  ExternalArrayType element_type_;
};

class LoadTypedArrayLength
    : public FixedInputValueNodeT<1, LoadTypedArrayLength> {
  using Base = FixedInputValueNodeT<1, LoadTypedArrayLength>;

 public:
  explicit LoadTypedArrayLength(uint64_t bitfield, ElementsKind elements_kind)
      : Base(bitfield), elements_kind_(elements_kind) {}
  static constexpr OpProperties kProperties =
      OpProperties::IntPtr() | OpProperties::CanRead();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  static constexpr int kReceiverIndex = 0;
  Input receiver_input() { return input(kReceiverIndex); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}

  auto options() const { return std::tuple{elements_kind_}; }

  ElementsKind elements_kind() const { return elements_kind_; }

 private:
  ElementsKind elements_kind_;
};

class CheckTypedArrayNotDetached
    : public FixedInputNodeT<1, CheckTypedArrayNotDetached> {
  using Base = FixedInputNodeT<1, CheckTypedArrayNotDetached>;

 public:
  explicit CheckTypedArrayNotDetached(uint64_t bitfield) : Base(bitfield) {}
  static constexpr OpProperties kProperties =
      OpProperties::EagerDeopt() | OpProperties::CanRead();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  static constexpr int kObjectIndex = 0;
  Input object_input() { return input(kObjectIndex); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class CheckTypedArrayBounds : public FixedInputNodeT<2, CheckTypedArrayBounds> {
  using Base = FixedInputNodeT<2, CheckTypedArrayBounds>;

 public:
  explicit CheckTypedArrayBounds(uint64_t bitfield) : Base(bitfield) {}
  static constexpr OpProperties kProperties = OpProperties::EagerDeopt();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kInt32, ValueRepresentation::kIntPtr};

  static constexpr int kIndexIndex = 0;
  static constexpr int kLengthIndex = 1;
  Input index_input() { return input(kIndexIndex); }
  Input length_input() { return input(kLengthIndex); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class CheckInt32Condition : public FixedInputNodeT<2, CheckInt32Condition> {
  using Base = FixedInputNodeT<2, CheckInt32Condition>;

 public:
  explicit CheckInt32Condition(uint64_t bitfield, AssertCondition condition,
                               DeoptimizeReason reason)
      : Base(bitfield | ConditionField::encode(condition) |
             ReasonField::encode(reason)) {}

  static constexpr OpProperties kProperties = OpProperties::EagerDeopt();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kInt32, ValueRepresentation::kInt32};

  static constexpr int kLeftIndex = 0;
  static constexpr int kRightIndex = 1;
  Input left_input() { return input(kLeftIndex); }
  Input right_input() { return input(kRightIndex); }

  AssertCondition condition() const {
    return ConditionField::decode(bitfield());
  }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

  auto options() const { return std::tuple{condition(), deoptimize_reason()}; }

  DEOPTIMIZE_REASON_FIELD

 private:
  using ConditionField =
      ReasonField::Next<AssertCondition, base::bits::WhichPowerOfTwo<size_t>(
                                             base::bits::RoundUpToPowerOfTwo32(
                                                 kNumAssertConditions))>;
};

class DebugBreak : public FixedInputNodeT<0, DebugBreak> {
  using Base = FixedInputNodeT<0, DebugBreak>;

 public:
  explicit DebugBreak(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::NotIdempotent();

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class Dead : public NodeT<Dead> {
  using Base = NodeT<Dead>;

 public:
  static constexpr OpProperties kProperties =
      OpProperties::ForValueRepresentation(ValueRepresentation::kNone);

  void SetValueLocationConstraints() {}
  void GenerateCode(MaglevAssembler*, const ProcessingState&) { UNREACHABLE(); }
  void PrintParams(std::ostream&) const {}
  void VerifyInputs() const {}
  void MarkTaggedInputsAsDecompressing() { UNREACHABLE(); }

 private:
  explicit Dead(uint64_t bitfield) : Base(bitfield) {}
};

class FunctionEntryStackCheck
    : public FixedInputNodeT<0, FunctionEntryStackCheck> {
  using Base = FixedInputNodeT<0, FunctionEntryStackCheck>;

 public:
  explicit FunctionEntryStackCheck(uint64_t bitfield) : Base(bitfield) {}

  // Although FunctionEntryStackCheck calls a builtin, we don't mark it as Call
  // here. The register allocator should not spill any live registers, since the
  // builtin already handles it. The only possible live register is
  // kJavaScriptCallNewTargetRegister.
  static constexpr OpProperties kProperties =
      OpProperties::LazyDeopt() | OpProperties::CanAllocate() |
      OpProperties::DeferredCall() | OpProperties::NotIdempotent();

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class CheckedInternalizedString
    : public FixedInputValueNodeT<1, CheckedInternalizedString> {
  using Base = FixedInputValueNodeT<1, CheckedInternalizedString>;

 public:
  explicit CheckedInternalizedString(uint64_t bitfield, CheckType check_type)
      : Base(CheckTypeBitField::update(bitfield, check_type)) {
    CHECK_EQ(properties().value_representation(), ValueRepresentation::kTagged);
  }

  static constexpr OpProperties kProperties =
      OpProperties::EagerDeopt() | OpProperties::TaggedValue();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  static constexpr int kObjectIndex = 0;
  Input object_input() { return Node::input(kObjectIndex); }
  CheckType check_type() const { return CheckTypeBitField::decode(bitfield()); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}

  auto options() const { return std::tuple{check_type()}; }

 private:
  using CheckTypeBitField = NextBitField<CheckType, 1>;
};

class CheckedObjectToIndex
    : public FixedInputValueNodeT<1, CheckedObjectToIndex> {
  using Base = FixedInputValueNodeT<1, CheckedObjectToIndex>;

 public:
  explicit CheckedObjectToIndex(uint64_t bitfield, CheckType check_type)
      : Base(CheckTypeBitField::update(bitfield, check_type)) {}

  static constexpr OpProperties kProperties =
      OpProperties::EagerDeopt() | OpProperties::Int32() |
      OpProperties::DeferredCall() | OpProperties::ConversionNode();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  static constexpr int kObjectIndex = 0;
  Input object_input() { return Node::input(kObjectIndex); }
  CheckType check_type() const { return CheckTypeBitField::decode(bitfield()); }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}

  auto options() const { return std::tuple{check_type()}; }

 private:
  using CheckTypeBitField = NextBitField<CheckType, 1>;
};

class GetTemplateObject : public FixedInputValueNodeT<1, GetTemplateObject> {
  using Base = FixedInputValueNodeT<1, GetTemplateObject>;

 public:
  explicit GetTemplateObject(
      uint64_t bitfield, compiler::SharedFunctionInfoRef shared_function_info,
      const compiler::FeedbackSource& feedback)
      : Base(bitfield),
        shared_function_info_(shared_function_info),
        feedback_(feedback) {}

  // The implementation currently calls runtime.
  static constexpr OpProperties kProperties =
      OpProperties::GenericRuntimeOrBuiltinCall();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  Input description() { return input(0); }

  compiler::SharedFunctionInfoRef shared_function_info() {
    return shared_function_info_;
  }
  compiler::FeedbackSource feedback() const { return feedback_; }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}

 private:
  compiler::SharedFunctionInfoRef shared_function_info_;
  const compiler::FeedbackSource feedback_;
};

class HasInPrototypeChain
    : public FixedInputValueNodeT<1, HasInPrototypeChain> {
  using Base = FixedInputValueNodeT<1, HasInPrototypeChain>;

 public:
  explicit HasInPrototypeChain(uint64_t bitfield,
                               compiler::HeapObjectRef prototype)
      : Base(bitfield), prototype_(prototype) {}

  // The implementation can enter user code in the deferred call (due to
  // proxied getPrototypeOf).
  static constexpr OpProperties kProperties =
      OpProperties::DeferredCall() | OpProperties::CanCallUserCode();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  Input object() { return input(0); }

  compiler::HeapObjectRef prototype() { return prototype_; }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

 private:
  compiler::HeapObjectRef prototype_;
};

class BuiltinStringFromCharCode
    : public FixedInputValueNodeT<1, BuiltinStringFromCharCode> {
  using Base = FixedInputValueNodeT<1, BuiltinStringFromCharCode>;

 public:
  explicit BuiltinStringFromCharCode(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties =
      OpProperties::CanAllocate() | OpProperties::DeferredCall();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kInt32};

  Input code_input() { return input(0); }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class BuiltinStringPrototypeCharCodeOrCodePointAt
    : public FixedInputValueNodeT<2,
                                  BuiltinStringPrototypeCharCodeOrCodePointAt> {
  using Base =
      FixedInputValueNodeT<2, BuiltinStringPrototypeCharCodeOrCodePointAt>;

 public:
  enum Mode {
    kCharCodeAt,
    kCodePointAt,
  };

  explicit BuiltinStringPrototypeCharCodeOrCodePointAt(uint64_t bitfield,
                                                       Mode mode)
      : Base(bitfield | ModeField::encode(mode)) {}

  static constexpr OpProperties kProperties =
      OpProperties::CanAllocate() | OpProperties::CanRead() |
      OpProperties::DeferredCall() | OpProperties::Int32();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kInt32};

  static constexpr int kStringIndex = 0;
  static constexpr int kIndexIndex = 1;
  Input string_input() { return input(kStringIndex); }
  Input index_input() { return input(kIndexIndex); }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

  auto options() const { return std::tuple{mode()}; }

  Mode mode() const { return ModeField::decode(bitfield()); }

 private:
  using ModeField = NextBitField<Mode, 1>;
};

class BuiltinSeqOneByteStringCharCodeAt
    : public FixedInputValueNodeT<2, BuiltinSeqOneByteStringCharCodeAt> {
  using Base = FixedInputValueNodeT<2, BuiltinSeqOneByteStringCharCodeAt>;

 public:
  explicit BuiltinSeqOneByteStringCharCodeAt(uint64_t bitfield)
      : Base(bitfield) {}

  static constexpr OpProperties kProperties =
      OpProperties::CanRead() | OpProperties::Int32();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kInt32};

  static constexpr int kStringIndex = 0;
  static constexpr int kIndexIndex = 1;
  Input string_input() { return input(kStringIndex); }
  Input index_input() { return input(kIndexIndex); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class MapPrototypeGet : public FixedInputValueNodeT<2, MapPrototypeGet> {
  using Base = FixedInputValueNodeT<2, MapPrototypeGet>;

 public:
  explicit MapPrototypeGet(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties =
      OpProperties::Call() | OpProperties::CanAllocate() |
      OpProperties::CanRead() | OpProperties::TaggedValue();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged};

  Input table_input() { return input(0); }
  Input key_input() { return input(1); }

  int MaxCallStackArgs() const {
    // Only implemented in Turbolev.
    UNREACHABLE();
  }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class MapPrototypeGetInt32Key
    : public FixedInputValueNodeT<2, MapPrototypeGetInt32Key> {
  using Base = FixedInputValueNodeT<2, MapPrototypeGetInt32Key>;

 public:
  explicit MapPrototypeGetInt32Key(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::CanAllocate() |
                                              OpProperties::CanRead() |
                                              OpProperties::TaggedValue();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kInt32};

  Input table_input() { return input(0); }
  Input key_input() { return input(1); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class SetPrototypeHas : public FixedInputValueNodeT<2, SetPrototypeHas> {
  using Base = FixedInputValueNodeT<2, SetPrototypeHas>;

 public:
  explicit SetPrototypeHas(uint64_t bitfield) : Base(bitfield) {}

  // CanAllocate is needed, since finding strings in hash tables does an
  // equality comparison which flattens strings.
  static constexpr OpProperties kProperties =
      OpProperties::Call() | OpProperties::CanAllocate() |
      OpProperties::CanRead() | OpProperties::TaggedValue();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged};

  Input table_input() { return input(0); }
  Input key_input() { return input(1); }

  int MaxCallStackArgs() const {
    // Only implemented in Turbolev.
    UNREACHABLE();
  }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class CreateFastArrayElements
    : public FixedInputValueNodeT<1, CreateFastArrayElements> {
  using Base = FixedInputValueNodeT<1, CreateFastArrayElements>;

 public:
  explicit CreateFastArrayElements(uint64_t bitfield,
                                   AllocationType allocation_type)
      : Base(bitfield), allocation_type_(allocation_type) {}

  AllocationType allocation_type() const { return allocation_type_; }

  static constexpr OpProperties kProperties =
      OpProperties::CanAllocate() | OpProperties::NotIdempotent();

  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kInt32};

  Input length_input() { return input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}

 private:
  const AllocationType allocation_type_;
};

class NewConsString : public FixedInputValueNodeT<3, NewConsString> {
  using Base = FixedInputValueNodeT<3, NewConsString>;

 public:
  explicit NewConsString(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::CanAllocate();

  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kInt32, ValueRepresentation::kTagged,
      ValueRepresentation::kTagged};

  Input length_input() { return input(0); }
  Input first_input() { return input(1); }
  Input second_input() { return input(2); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class TransitionAndStoreArrayElement
    : public FixedInputValueNodeT<3, TransitionAndStoreArrayElement> {
  using Base = FixedInputValueNodeT<3, TransitionAndStoreArrayElement>;

 public:
  explicit TransitionAndStoreArrayElement(uint64_t bitfield,
                                          const compiler::MapRef& fast_map,
                                          const compiler::MapRef& double_map)
      : Base(bitfield), fast_map_(fast_map), double_map_(double_map) {}

  static constexpr OpProperties kProperties =
      OpProperties::AnySideEffects() | OpProperties::DeferredCall();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kInt32,
      ValueRepresentation::kTagged};

  Input array_input() { return input(0); }
  Input index_input() { return input(1); }
  Input value_input() { return input(2); }

  compiler::MapRef fast_map() const { return fast_map_; }
  compiler::MapRef double_map() const { return double_map_; }

  int MaxCallStackArgs() const {
    // Only implemented in Turbolev.
    UNREACHABLE();
  }
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);

  void PrintParams(std::ostream&) const {}

 private:
  const compiler::MapRef fast_map_;
  const compiler::MapRef double_map_;
};

class PolymorphicAccessInfo {
 public:
  enum Kind {
    kNotFound,
    kConstant,
    kConstantDouble,
    kDataLoad,
    kModuleExport,
    kStringLength,
  };

  static PolymorphicAccessInfo NotFound(
      const ZoneVector<compiler::MapRef>& maps) {
    return PolymorphicAccessInfo(kNotFound, maps, Representation::Tagged());
  }
  static PolymorphicAccessInfo Constant(
      const ZoneVector<compiler::MapRef>& maps, compiler::ObjectRef constant) {
    return PolymorphicAccessInfo(kConstant, maps, Representation::Tagged(),
                                 constant);
  }
  static PolymorphicAccessInfo ConstantDouble(
      const ZoneVector<compiler::MapRef>& maps, Float64 constant) {
    return PolymorphicAccessInfo(kConstantDouble, maps, constant);
  }
  static PolymorphicAccessInfo DataLoad(
      const ZoneVector<compiler::MapRef>& maps, Representation representation,
      compiler::OptionalJSObjectRef holder, FieldIndex field_index) {
    return PolymorphicAccessInfo(kDataLoad, maps, representation, holder,
                                 field_index);
  }
  static PolymorphicAccessInfo ModuleExport(
      const ZoneVector<compiler::MapRef>& maps, compiler::CellRef cell) {
    return PolymorphicAccessInfo(kModuleExport, maps, Representation::Tagged(),
                                 cell);
  }
  static PolymorphicAccessInfo StringLength(
      const ZoneVector<compiler::MapRef>& maps) {
    return PolymorphicAccessInfo(kStringLength, maps, Representation::Smi());
  }

  Kind kind() const { return kind_; }

  const ZoneVector<compiler::MapRef>& maps() const { return maps_; }

  DirectHandle<Object> constant() const {
    DCHECK_EQ(kind_, kConstant);
    return constant_.object();
  }

  double constant_double() const {
    DCHECK_EQ(kind_, kConstantDouble);
    return constant_double_.get_scalar();
  }

  DirectHandle<Cell> cell() const {
    DCHECK_EQ(kind_, kModuleExport);
    return constant_.AsCell().object();
  }

  compiler::OptionalJSObjectRef holder() const {
    DCHECK_EQ(kind_, kDataLoad);
    return data_load_.holder_;
  }

  FieldIndex field_index() const {
    DCHECK_EQ(kind_, kDataLoad);
    return data_load_.field_index_;
  }

  Representation field_representation() const { return representation_; }

  bool operator==(const PolymorphicAccessInfo& other) const {
    if (kind_ != other.kind_ || !(representation_ == other.representation_)) {
      return false;
    }
    if (maps_.size() != other.maps_.size()) {
      return false;
    }
    for (size_t i = 0; i < maps_.size(); ++i) {
      if (maps_[i] != other.maps_[i]) {
        return false;
      }
    }
    switch (kind_) {
      case kNotFound:
      case kStringLength:
        return true;
      case kModuleExport:
      case kConstant:
        return constant_.equals(other.constant_);
      case kConstantDouble:
        return constant_double_ == other.constant_double_;
      case kDataLoad:
        return data_load_.holder_.equals(other.data_load_.holder_) &&
               data_load_.field_index_ == other.data_load_.field_index_;
    }
  }

  size_t hash_value() const {
    size_t hash = base::hash_value(kind_);
    hash = base::hash_combine(hash, base::hash_value(representation_.kind()));
    for (auto map : maps()) {
      hash = base::hash_combine(hash, map.hash_value());
    }
    switch (kind_) {
      case kNotFound:
      case kStringLength:
        break;
      case kModuleExport:
      case kConstant:
        hash = base::hash_combine(hash, constant_.hash_value());
        break;
      case kConstantDouble:
        hash = base::hash_combine(hash, base::hash_value(constant_double_));
        break;
      case kDataLoad:
        hash = base::hash_combine(
            hash, base::hash_value(data_load_.holder_.hash_value()));
        hash = base::hash_combine(
            hash, base::hash_value(data_load_.field_index_.index()));
        break;
    }
    return hash;
  }

 private:
  explicit PolymorphicAccessInfo(Kind kind,
                                 const ZoneVector<compiler::MapRef>& maps,
                                 Representation representation)
      : kind_(kind), maps_(maps), representation_(representation) {
    DCHECK(kind == kNotFound || kind == kStringLength);
  }

  PolymorphicAccessInfo(Kind kind, const ZoneVector<compiler::MapRef>& maps,
                        Representation representation,
                        compiler::ObjectRef constant)
      : kind_(kind),
        maps_(maps),
        representation_(representation),
        constant_(constant) {
    DCHECK(kind == kConstant || kind == kModuleExport);
  }

  PolymorphicAccessInfo(Kind kind, const ZoneVector<compiler::MapRef>& maps,
                        Float64 constant)
      : kind_(kind),
        maps_(maps),
        representation_(Representation::Double()),
        constant_double_(constant) {
    DCHECK_EQ(kind, kConstantDouble);
  }

  PolymorphicAccessInfo(Kind kind, const ZoneVector<compiler::MapRef>& maps,
                        Representation representation,
                        compiler::OptionalJSObjectRef holder,
                        FieldIndex field_index)
      : kind_(kind),
        maps_(maps),
        representation_(representation),
        data_load_{holder, field_index} {
    DCHECK_EQ(kind, kDataLoad);
  }

  const Kind kind_;
  // TODO(victorgomes): Create a PolymorphicMapChecks and avoid the maps here.
  const ZoneVector<compiler::MapRef> maps_;
  const Representation representation_;
  union {
    const compiler::ObjectRef constant_;
    const Float64 constant_double_;
    struct {
      const compiler::OptionalJSObjectRef holder_;
      const FieldIndex field_index_;
    } data_load_;
  };
};

template <typename Derived = LoadTaggedField>
class AbstractLoadTaggedField : public FixedInputValueNodeT<1, Derived> {
  using Base = FixedInputValueNodeT<1, Derived>;
  using Base::result;

 public:
  explicit AbstractLoadTaggedField(uint64_t bitfield, const int offset)
      : Base(bitfield), offset_(offset) {}

  static constexpr OpProperties kProperties = OpProperties::CanRead();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  int offset() const { return offset_; }

  using Base::input;
  static constexpr int kObjectIndex = 0;
  Input object_input() { return input(kObjectIndex); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

  auto options() const { return std::tuple{offset()}; }

  using Base::decompresses_tagged_result;

 private:
  const int offset_;
};

class LoadTaggedField : public AbstractLoadTaggedField<LoadTaggedField> {
  using Base = AbstractLoadTaggedField<LoadTaggedField>;

 public:
  explicit LoadTaggedField(uint64_t bitfield, const int offset)
      : Base(bitfield, offset) {}
};

class LoadTaggedFieldForProperty
    : public AbstractLoadTaggedField<LoadTaggedFieldForProperty> {
  using Base = AbstractLoadTaggedField<LoadTaggedFieldForProperty>;

 public:
  explicit LoadTaggedFieldForProperty(uint64_t bitfield, const int offset,
                                      compiler::NameRef name)
      : Base(bitfield, offset), name_(name) {}
  compiler::NameRef name() { return name_; }

  auto options() const { return std::tuple{offset(), name_}; }

 private:
  compiler::NameRef name_;
};

class LoadTaggedFieldForContextSlotNoCells
    : public AbstractLoadTaggedField<LoadTaggedFieldForContextSlotNoCells> {
  using Base = AbstractLoadTaggedField<LoadTaggedFieldForContextSlotNoCells>;

 public:
  explicit LoadTaggedFieldForContextSlotNoCells(uint64_t bitfield,
                                                const int offset)
      : Base(bitfield, offset) {}
};

class LoadTaggedFieldForContextSlot
    : public FixedInputValueNodeT<1, LoadTaggedFieldForContextSlot> {
  using Base = FixedInputValueNodeT<1, LoadTaggedFieldForContextSlot>;

 public:
  explicit LoadTaggedFieldForContextSlot(uint64_t bitfield, const int offset)
      : Base(bitfield), offset_(offset) {}

  static constexpr OpProperties kProperties = OpProperties::CanRead() |
                                              OpProperties::CanAllocate() |
                                              OpProperties::DeferredCall();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  int offset() const { return offset_; }

  using Base::input;
  static constexpr int kContextIndex = 0;
  Input context() { return input(kContextIndex); }

  int MaxCallStackArgs() const { return 0; }
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

  auto options() const { return std::tuple{offset()}; }

  using Base::decompresses_tagged_result;

 private:
  const int offset_;
};

class LoadDoubleField : public FixedInputValueNodeT<1, LoadDoubleField> {
  using Base = FixedInputValueNodeT<1, LoadDoubleField>;

 public:
  explicit LoadDoubleField(uint64_t bitfield, int offset)
      : Base(bitfield), offset_(offset) {}

  static constexpr OpProperties kProperties =
      OpProperties::CanRead() | OpProperties::Float64();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  int offset() const { return offset_; }

  static constexpr int kObjectIndex = 0;
  Input object_input() { return input(kObjectIndex); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

  auto options() const { return std::tuple{offset()}; }

 private:
  const int offset_;
};

class LoadFloat64 : public FixedInputValueNodeT<1, LoadFloat64> {
  using Base = FixedInputValueNodeT<1, LoadFloat64>;

 public:
  explicit LoadFloat64(uint64_t bitfield, int offset)
      : Base(bitfield), offset_(offset) {}

  static constexpr OpProperties kProperties =
      OpProperties::CanRead() | OpProperties::Float64();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  int offset() const { return offset_; }

  static constexpr int kObjectIndex = 0;
  Input object_input() { return input(kObjectIndex); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

  auto options() const { return std::tuple{offset()}; }

 private:
  const int offset_;
};

class LoadInt32 : public FixedInputValueNodeT<1, LoadInt32> {
  using Base = FixedInputValueNodeT<1, LoadInt32>;

 public:
  explicit LoadInt32(uint64_t bitfield, int offset)
      : Base(bitfield), offset_(offset) {}

  static constexpr OpProperties kProperties =
      OpProperties::CanRead() | OpProperties::Int32();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  int offset() const { return offset_; }

  static constexpr int kObjectIndex = 0;
  Input object_input() { return input(kObjectIndex); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

  auto options() const { return std::tuple{offset()}; }

 private:
  const int offset_;
};

class LoadTaggedFieldByFieldIndex
    : public FixedInputValueNodeT<2, LoadTaggedFieldByFieldIndex> {
  using Base = FixedInputValueNodeT<2, LoadTaggedFieldByFieldIndex>;

 public:
  explicit LoadTaggedFieldByFieldIndex(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::CanAllocate() |
                                              OpProperties::CanRead() |
                                              OpProperties::DeferredCall();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged};

  static constexpr int kObjectIndex = 0;
  static constexpr int kIndexIndex = 1;
  Input object_input() { return input(kObjectIndex); }
  Input index_input() { return input(kIndexIndex); }

#ifdef V8_COMPRESS_POINTERS
  void MarkTaggedInputsAsDecompressing() {
    // Only need to decompress the object, the index should be a Smi.
    object_input().node()->SetTaggedResultNeedsDecompress();
  }
#endif

  int MaxCallStackArgs() const { return 0; }
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class LoadFixedArrayElement
    : public FixedInputValueNodeT<2, LoadFixedArrayElement> {
  using Base = FixedInputValueNodeT<2, LoadFixedArrayElement>;

 public:
  explicit LoadFixedArrayElement(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::CanRead();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kInt32};

  static constexpr int kElementsIndex = 0;
  static constexpr int kIndexIndex = 1;
  Input elements_input() { return input(kElementsIndex); }
  Input index_input() { return input(kIndexIndex); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;
};

class EnsureWritableFastElements
    : public FixedInputValueNodeT<2, EnsureWritableFastElements> {
  using Base = FixedInputValueNodeT<2, EnsureWritableFastElements>;

 public:
  explicit EnsureWritableFastElements(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::CanAllocate() |
                                              OpProperties::DeferredCall() |
                                              OpProperties::CanWrite();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged};

  static constexpr int kElementsIndex = 0;
  static constexpr int kObjectIndex = 1;
  Input elements_input() { return input(kElementsIndex); }
  Input object_input() { return input(kObjectIndex); }

  int MaxCallStackArgs() const { return 0; }
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class ExtendPropertiesBackingStore
    : public FixedInputValueNodeT<2, ExtendPropertiesBackingStore> {
  using Base = FixedInputValueNodeT<2, ExtendPropertiesBackingStore>;

 public:
  explicit ExtendPropertiesBackingStore(uint64_t bitfield, int old_length)
      : Base(bitfield), old_length_(old_length) {}

  static constexpr OpProperties kProperties =
      OpProperties::CanAllocate() | OpProperties::CanRead() |
      OpProperties::CanWrite() | OpProperties::DeferredCall() |
      OpProperties::EagerDeopt() | OpProperties::NotIdempotent();

  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged};

  static constexpr int kPropertyArrayIndex = 0;
  static constexpr int kObjectIndex = 1;
  Input property_array_input() { return input(kPropertyArrayIndex); }
  Input object_input() { return input(kObjectIndex); }

  int MaxCallStackArgs() const { return 0; }
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

  int old_length() const { return old_length_; }

 private:
  const int old_length_;
};

class MaybeGrowFastElements
    : public FixedInputValueNodeT<4, MaybeGrowFastElements> {
  using Base = FixedInputValueNodeT<4, MaybeGrowFastElements>;

 public:
  explicit MaybeGrowFastElements(uint64_t bitfield, ElementsKind elements_kind)
      : Base(bitfield), elements_kind_(elements_kind) {}

  static constexpr OpProperties kProperties =
      OpProperties::CanAllocate() | OpProperties::DeferredCall() |
      OpProperties::CanWrite() | OpProperties::EagerDeopt();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged,
      ValueRepresentation::kInt32, ValueRepresentation::kInt32};

  static constexpr int kElementsIndex = 0;
  static constexpr int kObjectIndex = 1;
  static constexpr int kIndexIndex = 2;
  static constexpr int kElementsLengthIndex = 3;
  Input elements_input() { return input(kElementsIndex); }
  Input object_input() { return input(kObjectIndex); }
  Input index_input() { return input(kIndexIndex); }
  Input elements_length_input() { return input(kElementsLengthIndex); }

  ElementsKind elements_kind() const { return elements_kind_; }

  int MaxCallStackArgs() const { return 0; }
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}

  auto options() const { return std::tuple{elements_kind()}; }

 private:
  const ElementsKind elements_kind_;
};

class StoreFixedArrayElementWithWriteBarrier
    : public FixedInputNodeT<3, StoreFixedArrayElementWithWriteBarrier> {
  using Base = FixedInputNodeT<3, StoreFixedArrayElementWithWriteBarrier>;

 public:
  explicit StoreFixedArrayElementWithWriteBarrier(uint64_t bitfield)
      : Base(bitfield) {}

  static constexpr OpProperties kProperties =
      OpProperties::CanWrite() | OpProperties::DeferredCall();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kInt32,
      ValueRepresentation::kTagged};

  static constexpr int kElementsIndex = 0;
  static constexpr int kIndexIndex = 1;
  static constexpr int kValueIndex = 2;
  Input elements_input() { return input(kElementsIndex); }
  Input index_input() { return input(kIndexIndex); }
  Input value_input() { return input(kValueIndex); }

  int MaxCallStackArgs() const { return 0; }
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

// StoreFixedArrayElementNoWriteBarrier never does a Deferred Call. However,
// PhiRepresentationSelector can cause some StoreFixedArrayElementNoWriteBarrier
// to become StoreFixedArrayElementWithWriteBarrier, which can do Deferred
// Calls, and thus need the register snapshot. We thus set the DeferredCall
// property in StoreFixedArrayElementNoWriteBarrier so that it's allocated with
// enough space for the register snapshot.
class StoreFixedArrayElementNoWriteBarrier
    : public FixedInputNodeT<3, StoreFixedArrayElementNoWriteBarrier> {
  using Base = FixedInputNodeT<3, StoreFixedArrayElementNoWriteBarrier>;

 public:
  explicit StoreFixedArrayElementNoWriteBarrier(uint64_t bitfield)
      : Base(bitfield) {}

  static constexpr OpProperties kProperties =
      OpProperties::CanWrite() | OpProperties::DeferredCall();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kInt32,
      ValueRepresentation::kTagged};

  static constexpr int kElementsIndex = 0;
  static constexpr int kIndexIndex = 1;
  static constexpr int kValueIndex = 2;
  Input elements_input() { return input(kElementsIndex); }
  Input index_input() { return input(kIndexIndex); }
  Input value_input() { return input(kValueIndex); }

  int MaxCallStackArgs() const {
    // StoreFixedArrayElementNoWriteBarrier never really does any call.
    return 0;
  }
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class LoadFixedDoubleArrayElement
    : public FixedInputValueNodeT<2, LoadFixedDoubleArrayElement> {
  using Base = FixedInputValueNodeT<2, LoadFixedDoubleArrayElement>;

 public:
  explicit LoadFixedDoubleArrayElement(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties =
      OpProperties::CanRead() | OpProperties::Float64();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kInt32};

  static constexpr int kElementsIndex = 0;
  static constexpr int kIndexIndex = 1;
  Input elements_input() { return input(kElementsIndex); }
  Input index_input() { return input(kIndexIndex); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class LoadHoleyFixedDoubleArrayElement
    : public FixedInputValueNodeT<2, LoadHoleyFixedDoubleArrayElement> {
  using Base = FixedInputValueNodeT<2, LoadHoleyFixedDoubleArrayElement>;

 public:
  explicit LoadHoleyFixedDoubleArrayElement(uint64_t bitfield)
      : Base(bitfield) {}

  static constexpr OpProperties kProperties =
      OpProperties::CanRead() | OpProperties::HoleyFloat64();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kInt32};

  static constexpr int kElementsIndex = 0;
  static constexpr int kIndexIndex = 1;
  Input elements_input() { return input(kElementsIndex); }
  Input index_input() { return input(kIndexIndex); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class LoadHoleyFixedDoubleArrayElementCheckedNotHole
    : public FixedInputValueNodeT<
          2, LoadHoleyFixedDoubleArrayElementCheckedNotHole> {
  using Base =
      FixedInputValueNodeT<2, LoadHoleyFixedDoubleArrayElementCheckedNotHole>;

 public:
  explicit LoadHoleyFixedDoubleArrayElementCheckedNotHole(uint64_t bitfield)
      : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::CanRead() |
                                              OpProperties::Float64() |
                                              OpProperties::EagerDeopt();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kInt32};

  static constexpr int kElementsIndex = 0;
  static constexpr int kIndexIndex = 1;
  Input elements_input() { return input(kElementsIndex); }
  Input index_input() { return input(kIndexIndex); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

#ifdef V8_ENABLE_EXPERIMENTAL_UNDEFINED_DOUBLE
class LoadHoleyFixedDoubleArrayElementCheckedNotUndefinedOrHole
    : public FixedInputValueNodeT<
          2, LoadHoleyFixedDoubleArrayElementCheckedNotUndefinedOrHole> {
  using Base = FixedInputValueNodeT<
      2, LoadHoleyFixedDoubleArrayElementCheckedNotUndefinedOrHole>;

 public:
  explicit LoadHoleyFixedDoubleArrayElementCheckedNotUndefinedOrHole(
      uint64_t bitfield)
      : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::CanRead() |
                                              OpProperties::Float64() |
                                              OpProperties::EagerDeopt();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kInt32};

  static constexpr int kElementsIndex = 0;
  static constexpr int kIndexIndex = 1;
  Input elements_input() { return input(kElementsIndex); }
  Input index_input() { return input(kIndexIndex); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};
#endif  // V8_ENABLE_EXPERIMENTAL_UNDEFINED_DOUBLE

class StoreFixedDoubleArrayElement
    : public FixedInputNodeT<3, StoreFixedDoubleArrayElement> {
  using Base = FixedInputNodeT<3, StoreFixedDoubleArrayElement>;

 public:
  explicit StoreFixedDoubleArrayElement(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::CanWrite();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kInt32,
      ValueRepresentation::kFloat64};

  static constexpr int kElementsIndex = 0;
  static constexpr int kIndexIndex = 1;
  static constexpr int kValueIndex = 2;
  Input elements_input() { return input(kElementsIndex); }
  Input index_input() { return input(kIndexIndex); }
  Input value_input() { return input(kValueIndex); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class LoadSignedIntDataViewElement
    : public FixedInputValueNodeT<3, LoadSignedIntDataViewElement> {
  using Base = FixedInputValueNodeT<3, LoadSignedIntDataViewElement>;

 public:
  explicit LoadSignedIntDataViewElement(uint64_t bitfield,
                                        ExternalArrayType type)
      : Base(bitfield), type_(type) {
    DCHECK(type == ExternalArrayType::kExternalInt8Array ||
           type == ExternalArrayType::kExternalInt16Array ||
           type == ExternalArrayType::kExternalInt32Array);
  }

  static constexpr OpProperties kProperties =
      OpProperties::CanRead() | OpProperties::Int32();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kInt32,
      ValueRepresentation::kTagged};

  static constexpr int kObjectIndex = 0;
  static constexpr int kIndexIndex = 1;
  static constexpr int kIsLittleEndianIndex = 2;
  Input object_input() { return input(kObjectIndex); }
  Input index_input() { return input(kIndexIndex); }
  Input is_little_endian_input() { return input(kIsLittleEndianIndex); }

  bool is_little_endian_constant() {
    return IsConstantNode(is_little_endian_input().node()->opcode());
  }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}

  auto options() const { return std::tuple{type_}; }

  ExternalArrayType type() const { return type_; }

 private:
  ExternalArrayType type_;
};

class LoadDoubleDataViewElement
    : public FixedInputValueNodeT<3, LoadDoubleDataViewElement> {
  using Base = FixedInputValueNodeT<3, LoadDoubleDataViewElement>;
  static constexpr ExternalArrayType type_ =
      ExternalArrayType::kExternalFloat64Array;

 public:
  explicit LoadDoubleDataViewElement(uint64_t bitfield, ExternalArrayType type)
      : Base(bitfield) {
    DCHECK_EQ(type, type_);
  }

  static constexpr OpProperties kProperties =
      OpProperties::CanRead() | OpProperties::Float64();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kInt32,
      ValueRepresentation::kTagged};

  static constexpr int kObjectIndex = 0;
  static constexpr int kIndexIndex = 1;
  static constexpr int kIsLittleEndianIndex = 2;
  Input object_input() { return input(kObjectIndex); }
  Input index_input() { return input(kIndexIndex); }
  Input is_little_endian_input() { return input(kIsLittleEndianIndex); }

  bool is_little_endian_constant() {
    return IsConstantNode(is_little_endian_input().node()->opcode());
  }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}

  auto options() const { return std::tuple{type_}; }
};

#define LOAD_TYPED_ARRAY(name, properties, ...)                        \
  class name : public FixedInputValueNodeT<2, name> {                  \
    using Base = FixedInputValueNodeT<2, name>;                        \
                                                                       \
   public:                                                             \
    explicit name(uint64_t bitfield, ElementsKind elements_kind)       \
        : Base(bitfield), elements_kind_(elements_kind) {              \
      DCHECK(elements_kind ==                                          \
             v8::internal::compiler::turboshaft::any_of(__VA_ARGS__)); \
    }                                                                  \
                                                                       \
    static constexpr OpProperties kProperties =                        \
        OpProperties::CanRead() | properties;                          \
    static constexpr typename Base::InputTypes kInputTypes{            \
        ValueRepresentation::kTagged, ValueRepresentation::kInt32};    \
                                                                       \
    static constexpr int kObjectIndex = 0;                             \
    static constexpr int kIndexIndex = 1;                              \
    Input object_input() { return input(kObjectIndex); }               \
    Input index_input() { return input(kIndexIndex); }                 \
                                                                       \
    void SetValueLocationConstraints();                                \
    void GenerateCode(MaglevAssembler*, const ProcessingState&);       \
    void PrintParams(std::ostream&) const {}                           \
                                                                       \
    auto options() const { return std::tuple{elements_kind_}; }        \
                                                                       \
    ElementsKind elements_kind() const { return elements_kind_; }      \
                                                                       \
   private:                                                            \
    ElementsKind elements_kind_;                                       \
  };

LOAD_TYPED_ARRAY(LoadSignedIntTypedArrayElement, OpProperties::Int32(),
                 INT8_ELEMENTS, INT16_ELEMENTS, INT32_ELEMENTS)

LOAD_TYPED_ARRAY(LoadUnsignedIntTypedArrayElement, OpProperties::Uint32(),
                 UINT8_ELEMENTS, UINT8_CLAMPED_ELEMENTS, UINT16_ELEMENTS,
                 UINT16_ELEMENTS, UINT32_ELEMENTS)

LOAD_TYPED_ARRAY(LoadDoubleTypedArrayElement, OpProperties::Float64(),
                 FLOAT32_ELEMENTS, FLOAT64_ELEMENTS)

#undef LOAD_TYPED_ARRAY

#define LOAD_CONSTANT_TYPED_ARRAY(name, properties, ...)                      \
  class name : public FixedInputValueNodeT<1, name> {                         \
    using Base = FixedInputValueNodeT<1, name>;                               \
                                                                              \
   public:                                                                    \
    explicit name(uint64_t bitfield, compiler::JSTypedArrayRef typed_array,   \
                  ElementsKind elements_kind)                                 \
        : Base(bitfield),                                                     \
          typed_array_(typed_array),                                          \
          elements_kind_(elements_kind) {                                     \
      DCHECK(elements_kind ==                                                 \
             v8::internal::compiler::turboshaft::any_of(__VA_ARGS__));        \
    }                                                                         \
                                                                              \
    static constexpr OpProperties kProperties =                               \
        OpProperties::CanRead() | properties;                                 \
    static constexpr                                                          \
        typename Base::InputTypes kInputTypes{ValueRepresentation::kInt32};   \
                                                                              \
    static constexpr int kIndexIndex = 0;                                     \
    Input index_input() { return input(kIndexIndex); }                        \
                                                                              \
    void SetValueLocationConstraints();                                       \
    void GenerateCode(MaglevAssembler*, const ProcessingState&);              \
    void PrintParams(std::ostream&) const {}                                  \
                                                                              \
    auto options() const { return std::tuple{typed_array_, elements_kind_}; } \
                                                                              \
    ElementsKind elements_kind() const { return elements_kind_; }             \
    compiler::JSTypedArrayRef typed_array() const { return typed_array_; }    \
                                                                              \
   private:                                                                   \
    compiler::JSTypedArrayRef typed_array_;                                   \
    ElementsKind elements_kind_;                                              \
  };

LOAD_CONSTANT_TYPED_ARRAY(LoadSignedIntConstantTypedArrayElement,
                          OpProperties::Int32(), INT8_ELEMENTS, INT16_ELEMENTS,
                          INT32_ELEMENTS)

LOAD_CONSTANT_TYPED_ARRAY(LoadUnsignedIntConstantTypedArrayElement,
                          OpProperties::Uint32(), UINT8_ELEMENTS,
                          UINT8_CLAMPED_ELEMENTS, UINT16_ELEMENTS,
                          UINT16_ELEMENTS, UINT32_ELEMENTS)

LOAD_CONSTANT_TYPED_ARRAY(LoadDoubleConstantTypedArrayElement,
                          OpProperties::Float64(), FLOAT32_ELEMENTS,
                          FLOAT64_ELEMENTS)

#undef LOAD_CONSTANT_TYPED_ARRAY

#define STORE_TYPED_ARRAY(name, properties, type, ...)                    \
  class name : public FixedInputNodeT<3, name> {                          \
    using Base = FixedInputNodeT<3, name>;                                \
                                                                          \
   public:                                                                \
    explicit name(uint64_t bitfield, ElementsKind elements_kind)          \
        : Base(bitfield), elements_kind_(elements_kind) {                 \
      DCHECK(elements_kind ==                                             \
             v8::internal::compiler::turboshaft::any_of(__VA_ARGS__));    \
    }                                                                     \
                                                                          \
    static constexpr OpProperties kProperties = properties;               \
    static constexpr typename Base::InputTypes kInputTypes{               \
        ValueRepresentation::kTagged, ValueRepresentation::kInt32, type}; \
                                                                          \
    static constexpr int kObjectIndex = 0;                                \
    static constexpr int kIndexIndex = 1;                                 \
    static constexpr int kValueIndex = 2;                                 \
    Input object_input() { return input(kObjectIndex); }                  \
    Input index_input() { return input(kIndexIndex); }                    \
    Input value_input() { return input(kValueIndex); }                    \
                                                                          \
    void SetValueLocationConstraints();                                   \
    void GenerateCode(MaglevAssembler*, const ProcessingState&);          \
    void PrintParams(std::ostream&) const {}                              \
                                                                          \
    ElementsKind elements_kind() const { return elements_kind_; }         \
                                                                          \
   private:                                                               \
    ElementsKind elements_kind_;                                          \
  };

STORE_TYPED_ARRAY(StoreIntTypedArrayElement, OpProperties::CanWrite(),
                  ValueRepresentation::kInt32, INT8_ELEMENTS, INT16_ELEMENTS,
                  INT32_ELEMENTS, UINT8_ELEMENTS, UINT8_CLAMPED_ELEMENTS,
                  UINT16_ELEMENTS, UINT16_ELEMENTS, UINT32_ELEMENTS)
STORE_TYPED_ARRAY(StoreDoubleTypedArrayElement, OpProperties::CanWrite(),
                  ValueRepresentation::kHoleyFloat64, FLOAT32_ELEMENTS,
                  FLOAT64_ELEMENTS)
#undef STORE_TYPED_ARRAY

#define STORE_CONSTANT_TYPED_ARRAY(name, properties, type, ...)             \
  class name : public FixedInputNodeT<2, name> {                            \
    using Base = FixedInputNodeT<2, name>;                                  \
                                                                            \
   public:                                                                  \
    explicit name(uint64_t bitfield, compiler::JSTypedArrayRef typed_array, \
                  ElementsKind elements_kind)                               \
        : Base(bitfield),                                                   \
          typed_array_(typed_array),                                        \
          elements_kind_(elements_kind) {                                   \
      DCHECK(elements_kind ==                                               \
             v8::internal::compiler::turboshaft::any_of(__VA_ARGS__));      \
    }                                                                       \
                                                                            \
    static constexpr OpProperties kProperties = properties;                 \
    static constexpr typename Base::InputTypes kInputTypes{                 \
        ValueRepresentation::kInt32, type};                                 \
                                                                            \
    static constexpr int kIndexIndex = 0;                                   \
    static constexpr int kValueIndex = 1;                                   \
    Input index_input() { return input(kIndexIndex); }                      \
    Input value_input() { return input(kValueIndex); }                      \
                                                                            \
    void SetValueLocationConstraints();                                     \
    void GenerateCode(MaglevAssembler*, const ProcessingState&);            \
    void PrintParams(std::ostream&) const {}                                \
                                                                            \
    ElementsKind elements_kind() const { return elements_kind_; }           \
    compiler::JSTypedArrayRef typed_array() const { return typed_array_; }  \
                                                                            \
   private:                                                                 \
    compiler::JSTypedArrayRef typed_array_;                                 \
    ElementsKind elements_kind_;                                            \
  };

STORE_CONSTANT_TYPED_ARRAY(StoreIntConstantTypedArrayElement,
                           OpProperties::CanWrite(),
                           ValueRepresentation::kInt32, INT8_ELEMENTS,
                           INT16_ELEMENTS, INT32_ELEMENTS, UINT8_ELEMENTS,
                           UINT8_CLAMPED_ELEMENTS, UINT16_ELEMENTS,
                           UINT16_ELEMENTS, UINT32_ELEMENTS)
STORE_CONSTANT_TYPED_ARRAY(StoreDoubleConstantTypedArrayElement,
                           OpProperties::CanWrite(),
                           ValueRepresentation::kHoleyFloat64, FLOAT32_ELEMENTS,
                           FLOAT64_ELEMENTS)
#undef STORE_CONSTANT_TYPED_ARRAY

class StoreSignedIntDataViewElement
    : public FixedInputNodeT<4, StoreSignedIntDataViewElement> {
  using Base = FixedInputNodeT<4, StoreSignedIntDataViewElement>;

 public:
  explicit StoreSignedIntDataViewElement(uint64_t bitfield,
                                         ExternalArrayType type)
      : Base(bitfield), type_(type) {
    DCHECK(type == ExternalArrayType::kExternalInt8Array ||
           type == ExternalArrayType::kExternalInt16Array ||
           type == ExternalArrayType::kExternalInt32Array);
  }

  static constexpr OpProperties kProperties = OpProperties::CanWrite();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kInt32,
      ValueRepresentation::kInt32, ValueRepresentation::kTagged};

  static constexpr int kObjectIndex = 0;
  static constexpr int kIndexIndex = 1;
  static constexpr int kValueIndex = 2;
  static constexpr int kIsLittleEndianIndex = 3;
  Input object_input() { return input(kObjectIndex); }
  Input index_input() { return input(kIndexIndex); }
  Input value_input() { return input(kValueIndex); }
  Input is_little_endian_input() { return input(kIsLittleEndianIndex); }

  bool is_little_endian_constant() {
    return IsConstantNode(is_little_endian_input().node()->opcode());
  }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}

  ExternalArrayType type() const { return type_; }

 private:
  ExternalArrayType type_;
};

class StoreDoubleDataViewElement
    : public FixedInputNodeT<4, StoreDoubleDataViewElement> {
  using Base = FixedInputNodeT<4, StoreDoubleDataViewElement>;

 public:
  explicit StoreDoubleDataViewElement(uint64_t bitfield, ExternalArrayType type)
      : Base(bitfield) {
    DCHECK_EQ(type, ExternalArrayType::kExternalFloat64Array);
  }

  static constexpr OpProperties kProperties = OpProperties::CanWrite();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kInt32,
      ValueRepresentation::kHoleyFloat64, ValueRepresentation::kTagged};

  static constexpr int kObjectIndex = 0;
  static constexpr int kIndexIndex = 1;
  static constexpr int kValueIndex = 2;
  static constexpr int kIsLittleEndianIndex = 3;
  Input object_input() { return input(kObjectIndex); }
  Input index_input() { return input(kIndexIndex); }
  Input value_input() { return input(kValueIndex); }
  Input is_little_endian_input() { return input(kIsLittleEndianIndex); }

  bool is_little_endian_constant() {
    return IsConstantNode(is_little_endian_input().node()->opcode());
  }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class StoreDoubleField : public FixedInputNodeT<2, StoreDoubleField> {
  using Base = FixedInputNodeT<2, StoreDoubleField>;

 public:
  explicit StoreDoubleField(uint64_t bitfield, int offset)
      : Base(bitfield), offset_(offset) {}

  static constexpr OpProperties kProperties = OpProperties::CanWrite();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kFloat64};

  int offset() const { return offset_; }

  static constexpr int kObjectIndex = 0;
  static constexpr int kValueIndex = 1;
  Input object_input() { return input(kObjectIndex); }
  Input value_input() { return input(kValueIndex); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

 private:
  const int offset_;
};

class StoreInt32 : public FixedInputNodeT<2, StoreInt32> {
  using Base = FixedInputNodeT<2, StoreInt32>;

 public:
  explicit StoreInt32(uint64_t bitfield, int offset)
      : Base(bitfield), offset_(offset) {}

  static constexpr OpProperties kProperties = OpProperties::CanWrite();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kInt32};

  int offset() const { return offset_; }

  static constexpr int kObjectIndex = 0;
  static constexpr int kValueIndex = 1;
  Input object_input() { return input(kObjectIndex); }
  Input value_input() { return input(kValueIndex); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

 private:
  const int offset_;
};

class StoreFloat64 : public FixedInputNodeT<2, StoreFloat64> {
  using Base = FixedInputNodeT<2, StoreFloat64>;

 public:
  explicit StoreFloat64(uint64_t bitfield, int offset)
      : Base(bitfield), offset_(offset) {}

  static constexpr OpProperties kProperties = OpProperties::CanWrite();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kFloat64};

  int offset() const { return offset_; }

  static constexpr int kObjectIndex = 0;
  static constexpr int kValueIndex = 1;
  Input object_input() { return input(kObjectIndex); }
  Input value_input() { return input(kValueIndex); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

 private:
  const int offset_;
};

enum class StoreTaggedMode : uint8_t {
  kDefault,
  kInitializing,
  kTransitioning
};
inline bool IsInitializingOrTransitioning(StoreTaggedMode mode) {
  return mode == StoreTaggedMode::kInitializing ||
         mode == StoreTaggedMode::kTransitioning;
}

class StoreTaggedFieldNoWriteBarrier
    : public FixedInputNodeT<2, StoreTaggedFieldNoWriteBarrier> {
  using Base = FixedInputNodeT<2, StoreTaggedFieldNoWriteBarrier>;

 public:
  explicit StoreTaggedFieldNoWriteBarrier(uint64_t bitfield, int offset,
                                          StoreTaggedMode store_mode)
      : Base(bitfield | InitializingOrTransitioningField::encode(
                            IsInitializingOrTransitioning(store_mode))),
        offset_(offset) {}

  // StoreTaggedFieldNoWriteBarrier never does a Deferred Call. However,
  // PhiRepresentationSelector can cause some StoreTaggedFieldNoWriteBarrier to
  // become StoreTaggedFieldWithWriteBarrier, which can do Deferred Calls, and
  // thus need the register snapshot. We thus set the DeferredCall property in
  // StoreTaggedFieldNoWriteBarrier so that it's allocated with enough space for
  // the register snapshot.
  static constexpr OpProperties kProperties =
      OpProperties::CanWrite() | OpProperties::DeferredCall();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged};

  int offset() const { return offset_; }
  bool initializing_or_transitioning() const {
    return InitializingOrTransitioningField::decode(bitfield());
  }

  static constexpr int kObjectIndex = 0;
  static constexpr int kValueIndex = 1;
  Input object_input() { return input(kObjectIndex); }
  Input value_input() { return input(kValueIndex); }

#ifdef V8_COMPRESS_POINTERS
  void MarkTaggedInputsAsDecompressing() {
    object_input().node()->SetTaggedResultNeedsDecompress();
    // Don't need to decompress value to store it.
  }
#endif

  int MaxCallStackArgs() const {
    // StoreTaggedFieldNoWriteBarrier never really does any call.
    return 0;
  }
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

  void VerifyInputs() const;

 private:
  using InitializingOrTransitioningField = NextBitField<bool, 1>;

  const int offset_;
};

class StoreMap : public FixedInputNodeT<1, StoreMap> {
  using Base = FixedInputNodeT<1, StoreMap>;

 public:
  enum class Kind {
    kInitializing,
    kInlinedAllocation,
    kTransitioning,
  };
  explicit StoreMap(uint64_t bitfield, compiler::MapRef map, Kind kind)
      : Base(bitfield | KindField::encode(kind)), map_(map) {}

  static constexpr OpProperties kProperties =
      OpProperties::CanWrite() | OpProperties::DeferredCall();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  static constexpr int kObjectIndex = 0;
  Input object_input() { return input(kObjectIndex); }

  compiler::MapRef map() const { return map_; }
  Kind kind() const { return KindField::decode(bitfield()); }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

  void ClearUnstableNodeAspects(bool is_tracing_enabled, KnownNodeAspects&);

 private:
  using KindField = NextBitField<Kind, 3>;
  const compiler::MapRef map_;
};
std::ostream& operator<<(std::ostream& os, StoreMap::Kind);

class StoreTaggedFieldWithWriteBarrier
    : public FixedInputNodeT<2, StoreTaggedFieldWithWriteBarrier> {
  using Base = FixedInputNodeT<2, StoreTaggedFieldWithWriteBarrier>;

 public:
  explicit StoreTaggedFieldWithWriteBarrier(uint64_t bitfield, int offset,
                                            StoreTaggedMode store_mode)
      : Base(bitfield | InitializingOrTransitioningField::encode(
                            IsInitializingOrTransitioning(store_mode))),
        offset_(offset) {}

  static constexpr OpProperties kProperties =
      OpProperties::CanWrite() | OpProperties::DeferredCall();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged};

  int offset() const { return offset_; }
  bool initializing_or_transitioning() const {
    return InitializingOrTransitioningField::decode(bitfield());
  }

  static constexpr int kObjectIndex = 0;
  static constexpr int kValueIndex = 1;
  Input object_input() { return input(kObjectIndex); }
  Input value_input() { return input(kValueIndex); }

#ifdef V8_COMPRESS_POINTERS
  void MarkTaggedInputsAsDecompressing() {
    object_input().node()->SetTaggedResultNeedsDecompress();
    // Don't need to decompress value to store it.
  }
#endif

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

 private:
  using InitializingOrTransitioningField = NextBitField<bool, 1>;

  const int offset_;
};

class StoreContextSlotWithWriteBarrier
    : public FixedInputNodeT<2, StoreContextSlotWithWriteBarrier> {
  using Base = FixedInputNodeT<2, StoreContextSlotWithWriteBarrier>;

 public:
  explicit StoreContextSlotWithWriteBarrier(uint64_t bitfield, int index)
      : Base(bitfield), index_(index) {}

  static constexpr OpProperties kProperties = OpProperties::CanWrite() |
                                              OpProperties::DeferredCall() |
                                              OpProperties::LazyDeopt();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged};

  int offset() const { return Context::OffsetOfElementAt(index()); }
  int index() const { return index_; }

  static constexpr int kContextIndex = 0;
  static constexpr int kNewValueIndex = 1;
  Input context_input() { return input(kContextIndex); }
  Input new_value_input() { return input(kNewValueIndex); }

#ifdef V8_COMPRESS_POINTERS
  void MarkTaggedInputsAsDecompressing() {
    context_input().node()->SetTaggedResultNeedsDecompress();
    new_value_input().node()->SetTaggedResultNeedsDecompress();
  }
#endif

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

 private:
  const int index_;
};

class StoreTrustedPointerFieldWithWriteBarrier
    : public FixedInputNodeT<2, StoreTrustedPointerFieldWithWriteBarrier> {
  using Base = FixedInputNodeT<2, StoreTrustedPointerFieldWithWriteBarrier>;

 public:
  explicit StoreTrustedPointerFieldWithWriteBarrier(uint64_t bitfield,
                                                    int offset,
                                                    IndirectPointerTag tag,
                                                    StoreTaggedMode store_mode)
      : Base(bitfield | InitializingOrTransitioningField::encode(
                            IsInitializingOrTransitioning(store_mode))),
        offset_(offset),
        tag_(tag) {}

  static constexpr OpProperties kProperties =
      OpProperties::CanWrite() | OpProperties::DeferredCall();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged};

  int offset() const { return offset_; }
  IndirectPointerTag tag() const { return tag_; }
  bool initializing_or_transitioning() const {
    return InitializingOrTransitioningField::decode(bitfield());
  }

  static constexpr int kObjectIndex = 0;
  static constexpr int kValueIndex = 1;
  Input object_input() { return input(kObjectIndex); }
  Input value_input() { return input(kValueIndex); }

#ifdef V8_COMPRESS_POINTERS
  void MarkTaggedInputsAsDecompressing() {
    object_input().node()->SetTaggedResultNeedsDecompress();
    // value is never compressed.
  }
#endif

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

 private:
  using InitializingOrTransitioningField = NextBitField<bool, 1>;

  const int offset_;
  const IndirectPointerTag tag_;
};

class LoadGlobal : public FixedInputValueNodeT<1, LoadGlobal> {
  using Base = FixedInputValueNodeT<1, LoadGlobal>;

 public:
  explicit LoadGlobal(uint64_t bitfield, compiler::NameRef name,
                      const compiler::FeedbackSource& feedback,
                      TypeofMode typeof_mode)
      : Base(bitfield),
        name_(name),
        feedback_(feedback),
        typeof_mode_(typeof_mode) {}

  // The implementation currently calls runtime.
  static constexpr OpProperties kProperties = OpProperties::JSCall();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  compiler::NameRef name() const { return name_; }
  compiler::FeedbackSource feedback() const { return feedback_; }
  TypeofMode typeof_mode() const { return typeof_mode_; }

  Input context() { return input(0); }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

 private:
  const compiler::NameRef name_;
  const compiler::FeedbackSource feedback_;
  const TypeofMode typeof_mode_;
};

class StoreGlobal : public FixedInputValueNodeT<2, StoreGlobal> {
  using Base = FixedInputValueNodeT<2, StoreGlobal>;

 public:
  explicit StoreGlobal(uint64_t bitfield, compiler::NameRef name,
                       const compiler::FeedbackSource& feedback)
      : Base(bitfield), name_(name), feedback_(feedback) {}

  // The implementation currently calls runtime.
  static constexpr OpProperties kProperties = OpProperties::JSCall();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged};

  compiler::NameRef name() const { return name_; }
  compiler::FeedbackSource feedback() const { return feedback_; }

  Input context() { return input(0); }
  Input value() { return input(1); }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

 private:
  const compiler::NameRef name_;
  const compiler::FeedbackSource feedback_;
};

class UpdateJSArrayLength
    : public FixedInputValueNodeT<3, UpdateJSArrayLength> {
  using Base = FixedInputValueNodeT<3, UpdateJSArrayLength>;

 public:
  explicit UpdateJSArrayLength(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::CanWrite();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kInt32, ValueRepresentation::kTagged,
      ValueRepresentation::kInt32};

  // TODO(pthier): Use a more natural order once we can define the result
  // register to be equal to any input register.
  // The current order avoids any extra moves in the common case where index is
  // less than length
  static constexpr int kLengthIndex = 0;
  static constexpr int kObjectIndex = 1;
  static constexpr int kIndexIndex = 2;
  Input length_input() { return input(kLengthIndex); }
  Input object_input() { return input(kObjectIndex); }
  Input index_input() { return input(kIndexIndex); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class LoadNamedGeneric : public FixedInputValueNodeT<2, LoadNamedGeneric> {
  using Base = FixedInputValueNodeT<2, LoadNamedGeneric>;

 public:
  explicit LoadNamedGeneric(uint64_t bitfield, compiler::NameRef name,
                            const compiler::FeedbackSource& feedback)
      : Base(bitfield), name_(name), feedback_(feedback) {}

  // The implementation currently calls runtime.
  static constexpr OpProperties kProperties = OpProperties::JSCall();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged};

  compiler::NameRef name() const { return name_; }
  compiler::FeedbackSource feedback() const { return feedback_; }

  static constexpr int kContextIndex = 0;
  static constexpr int kObjectIndex = 1;
  Input context() { return input(kContextIndex); }
  Input object_input() { return input(kObjectIndex); }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

 private:
  const compiler::NameRef name_;
  const compiler::FeedbackSource feedback_;
};

class LoadNamedFromSuperGeneric
    : public FixedInputValueNodeT<3, LoadNamedFromSuperGeneric> {
  using Base = FixedInputValueNodeT<3, LoadNamedFromSuperGeneric>;

 public:
  explicit LoadNamedFromSuperGeneric(uint64_t bitfield, compiler::NameRef name,
                                     const compiler::FeedbackSource& feedback)
      : Base(bitfield), name_(name), feedback_(feedback) {}

  // The implementation currently calls runtime.
  static constexpr OpProperties kProperties = OpProperties::JSCall();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged,
      ValueRepresentation::kTagged};

  compiler::NameRef name() const { return name_; }
  compiler::FeedbackSource feedback() const { return feedback_; }

  static constexpr int kContextIndex = 0;
  static constexpr int kReceiverIndex = 1;
  static constexpr int kLookupStartObjectIndex = 2;
  Input context() { return input(kContextIndex); }
  Input receiver() { return input(kReceiverIndex); }
  Input lookup_start_object() { return input(kLookupStartObjectIndex); }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

 private:
  const compiler::NameRef name_;
  const compiler::FeedbackSource feedback_;
};

class SetNamedGeneric : public FixedInputValueNodeT<3, SetNamedGeneric> {
  using Base = FixedInputValueNodeT<3, SetNamedGeneric>;

 public:
  explicit SetNamedGeneric(uint64_t bitfield, compiler::NameRef name,
                           const compiler::FeedbackSource& feedback)
      : Base(bitfield), name_(name), feedback_(feedback) {}

  // The implementation currently calls runtime.
  static constexpr OpProperties kProperties = OpProperties::JSCall();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged,
      ValueRepresentation::kTagged};

  compiler::NameRef name() const { return name_; }
  compiler::FeedbackSource feedback() const { return feedback_; }

  static constexpr int kContextIndex = 0;
  static constexpr int kObjectIndex = 1;
  static constexpr int kValueIndex = 2;
  Input context() { return input(kContextIndex); }
  Input object_input() { return input(kObjectIndex); }
  Input value_input() { return input(kValueIndex); }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

 private:
  const compiler::NameRef name_;
  const compiler::FeedbackSource feedback_;
};

class LoadEnumCacheLength
    : public FixedInputValueNodeT<1, LoadEnumCacheLength> {
  using Base = FixedInputValueNodeT<1, LoadEnumCacheLength>;

 public:
  explicit LoadEnumCacheLength(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties =
      OpProperties::CanRead() | OpProperties::Int32();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  static constexpr int kMapInput = 0;
  Input map_input() { return input(kMapInput); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class StringAt : public FixedInputValueNodeT<2, StringAt> {
  using Base = FixedInputValueNodeT<2, StringAt>;

 public:
  explicit StringAt(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::CanRead() |
                                              OpProperties::CanAllocate() |
                                              OpProperties::DeferredCall();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kInt32};

  static constexpr int kStringIndex = 0;
  static constexpr int kIndexIndex = 1;
  Input string_input() { return input(kStringIndex); }
  Input index_input() { return input(kIndexIndex); }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class SeqOneByteStringAt : public FixedInputValueNodeT<2, SeqOneByteStringAt> {
  using Base = FixedInputValueNodeT<2, SeqOneByteStringAt>;

 public:
  explicit SeqOneByteStringAt(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::CanRead();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kInt32};

  static constexpr int kStringIndex = 0;
  static constexpr int kIndexIndex = 1;
  Input string_input() { return input(kStringIndex); }
  Input index_input() { return input(kIndexIndex); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class StringLength : public FixedInputValueNodeT<1, StringLength> {
  using Base = FixedInputValueNodeT<1, StringLength>;

 public:
  explicit StringLength(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties =
      OpProperties::CanRead() | OpProperties::Int32();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  static constexpr int kObjectIndex = 0;
  Input object_input() { return input(kObjectIndex); }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class StringConcat : public FixedInputValueNodeT<2, StringConcat> {
  using Base = FixedInputValueNodeT<2, StringConcat>;

 public:
  explicit StringConcat(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties =
      OpProperties::Call() | OpProperties::CanAllocate() |
      OpProperties::LazyDeopt() | OpProperties::CanThrow();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged};

  Input lhs() { return Node::input(0); }
  Input rhs() { return Node::input(1); }

  int MaxCallStackArgs() const { return 0; }
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

/*
 * This instruction takes two string map inputs and returns the result map that
 * is required for a cons string of these two inputs types (i.e.,
 * Const(One|Two)ByteStringMap).
 *
 * TODO(olivf): Remove this instruction and instead select the result map using
 * normal branches. This needs allocation folding support across the resulting
 * sub-graph.
 *
 */
class ConsStringMap : public FixedInputValueNodeT<2, ConsStringMap> {
  using Base = FixedInputValueNodeT<2, ConsStringMap>;

 public:
  explicit ConsStringMap(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::TaggedValue();

  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged};

  Input lhs() { return Node::input(0); }
  Input rhs() { return Node::input(1); }

#ifdef V8_STATIC_ROOTS
  void MarkTaggedInputsAsDecompressing() const {
    // Not needed as we just check some bits on the map ptr.
  }
#endif

  int MaxCallStackArgs() const { return 0; }
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class UnwrapStringWrapper
    : public FixedInputValueNodeT<1, UnwrapStringWrapper> {
  using Base = FixedInputValueNodeT<1, UnwrapStringWrapper>;

 public:
  explicit UnwrapStringWrapper(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::TaggedValue();

  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  Input value_input() { return Node::input(0); }

  int MaxCallStackArgs() const { return 0; }
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class DefineNamedOwnGeneric
    : public FixedInputValueNodeT<3, DefineNamedOwnGeneric> {
  using Base = FixedInputValueNodeT<3, DefineNamedOwnGeneric>;

 public:
  explicit DefineNamedOwnGeneric(uint64_t bitfield, compiler::NameRef name,
                                 const compiler::FeedbackSource& feedback)
      : Base(bitfield), name_(name), feedback_(feedback) {}

  // The implementation currently calls runtime.
  static constexpr OpProperties kProperties = OpProperties::JSCall();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged,
      ValueRepresentation::kTagged};

  compiler::NameRef name() const { return name_; }
  compiler::FeedbackSource feedback() const { return feedback_; }

  static constexpr int kContextIndex = 0;
  static constexpr int kObjectIndex = 1;
  static constexpr int kValueIndex = 2;
  Input context() { return input(kContextIndex); }
  Input object_input() { return input(kObjectIndex); }
  Input value_input() { return input(kValueIndex); }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

 private:
  const compiler::NameRef name_;
  const compiler::FeedbackSource feedback_;
};

class StoreInArrayLiteralGeneric
    : public FixedInputValueNodeT<4, StoreInArrayLiteralGeneric> {
  using Base = FixedInputValueNodeT<4, StoreInArrayLiteralGeneric>;

 public:
  explicit StoreInArrayLiteralGeneric(uint64_t bitfield,
                                      const compiler::FeedbackSource& feedback)
      : Base(bitfield), feedback_(feedback) {}

  // The implementation currently calls runtime.
  static constexpr OpProperties kProperties = OpProperties::JSCall();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged,
      ValueRepresentation::kTagged, ValueRepresentation::kTagged};

  compiler::FeedbackSource feedback() const { return feedback_; }

  static constexpr int kContextIndex = 0;
  static constexpr int kObjectIndex = 1;
  static constexpr int kNameIndex = 2;
  static constexpr int kValueIndex = 3;
  Input context() { return input(kContextIndex); }
  Input object_input() { return input(kObjectIndex); }
  Input name_input() { return input(kNameIndex); }
  Input value_input() { return input(kValueIndex); }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}

 private:
  const compiler::FeedbackSource feedback_;
};

class GetKeyedGeneric : public FixedInputValueNodeT<3, GetKeyedGeneric> {
  using Base = FixedInputValueNodeT<3, GetKeyedGeneric>;

 public:
  explicit GetKeyedGeneric(uint64_t bitfield,
                           const compiler::FeedbackSource& feedback)
      : Base(bitfield), feedback_(feedback) {}

  // The implementation currently calls runtime.
  static constexpr OpProperties kProperties = OpProperties::JSCall();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged,
      ValueRepresentation::kTagged};

  compiler::FeedbackSource feedback() const { return feedback_; }

  static constexpr int kContextIndex = 0;
  static constexpr int kObjectIndex = 1;
  static constexpr int kKeyIndex = 2;
  Input context() { return input(kContextIndex); }
  Input object_input() { return input(kObjectIndex); }
  Input key_input() { return input(kKeyIndex); }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}

 private:
  const compiler::FeedbackSource feedback_;
};

class SetKeyedGeneric : public FixedInputValueNodeT<4, SetKeyedGeneric> {
  using Base = FixedInputValueNodeT<4, SetKeyedGeneric>;

 public:
  explicit SetKeyedGeneric(uint64_t bitfield,
                           const compiler::FeedbackSource& feedback)
      : Base(bitfield), feedback_(feedback) {}

  // The implementation currently calls runtime.
  static constexpr OpProperties kProperties = OpProperties::JSCall();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged,
      ValueRepresentation::kTagged, ValueRepresentation::kTagged};

  compiler::FeedbackSource feedback() const { return feedback_; }

  static constexpr int kContextIndex = 0;
  static constexpr int kObjectIndex = 1;
  static constexpr int kKeyIndex = 2;
  static constexpr int kValueIndex = 3;
  Input context() { return input(kContextIndex); }
  Input object_input() { return input(kObjectIndex); }
  Input key_input() { return input(kKeyIndex); }
  Input value_input() { return input(kValueIndex); }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}

 private:
  const compiler::FeedbackSource feedback_;
};

class DefineKeyedOwnGeneric
    : public FixedInputValueNodeT<5, DefineKeyedOwnGeneric> {
  using Base = FixedInputValueNodeT<5, DefineKeyedOwnGeneric>;

 public:
  explicit DefineKeyedOwnGeneric(uint64_t bitfield,
                                 const compiler::FeedbackSource& feedback)
      : Base(bitfield), feedback_(feedback) {}

  // The implementation currently calls runtime.
  static constexpr OpProperties kProperties = OpProperties::JSCall();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged,
      ValueRepresentation::kTagged, ValueRepresentation::kTagged,
      ValueRepresentation::kTagged};

  compiler::FeedbackSource feedback() const { return feedback_; }

  static constexpr int kContextIndex = 0;
  static constexpr int kObjectIndex = 1;
  static constexpr int kKeyIndex = 2;
  static constexpr int kValueIndex = 3;
  static constexpr int kFlagsIndex = 4;
  Input context() { return input(kContextIndex); }
  Input object_input() { return input(kObjectIndex); }
  Input key_input() { return input(kKeyIndex); }
  Input value_input() { return input(kValueIndex); }
  Input flags_input() { return input(kFlagsIndex); }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}

 private:
  const compiler::FeedbackSource feedback_;
};

class GapMove : public FixedInputNodeT<0, GapMove> {
  using Base = FixedInputNodeT<0, GapMove>;

 public:
  GapMove(uint64_t bitfield, compiler::AllocatedOperand source,
          compiler::AllocatedOperand target)
      : Base(bitfield), source_(source), target_(target) {}

  compiler::AllocatedOperand source() const { return source_; }
  compiler::AllocatedOperand target() const { return target_; }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

 private:
  compiler::AllocatedOperand source_;
  compiler::AllocatedOperand target_;
};

class ConstantGapMove : public FixedInputNodeT<0, ConstantGapMove> {
  using Base = FixedInputNodeT<0, ConstantGapMove>;

 public:
  ConstantGapMove(uint64_t bitfield, ValueNode* node,
                  compiler::AllocatedOperand target)
      : Base(bitfield), node_(node), target_(target) {}

  compiler::AllocatedOperand target() const { return target_; }
  ValueNode* node() const { return node_; }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

 private:
  ValueNode* node_;
  compiler::InstructionOperand source_;
  compiler::AllocatedOperand target_;
};

class MergePointInterpreterFrameState;


// TODO(verwaest): It may make more sense to buffer phis in merged_states until
// we set up the interpreter frame state for code generation. At that point we
// can generate correctly-sized phis.
class Phi : public ValueNodeT<Phi> {
  using Base = ValueNodeT<Phi>;

 public:
  using List = base::ThreadedList<Phi>;

  // TODO(jgruber): More intuitive constructors, if possible.
  Phi(uint64_t bitfield, MergePointInterpreterFrameState* merge_state,
      interpreter::Register owner)
      : Base(bitfield),
        owner_(owner),
        merge_state_(merge_state),
        type_(NodeType::kUnknown),
        post_loop_type_(NodeType::kUnknown) {
    DCHECK_NOT_NULL(merge_state);
  }

  Input backedge_input() { return input(input_count() - 1); }

  interpreter::Register owner() const { return owner_; }
  const MergePointInterpreterFrameState* merge_state() const {
    return merge_state_;
  }

  using Node::initialize_input_null;
  using Node::reduce_input_count;
  using Node::set_input;

  bool is_exception_phi() const { return input_count() == 0; }
  bool is_loop_phi() const;

  bool is_backedge_offset(int i) const {
    return is_loop_phi() && i == input_count() - 1;
  }

  void VerifyInputs() const;

#ifdef V8_COMPRESS_POINTERS
  void MarkTaggedInputsAsDecompressing() {
    // Do not mark inputs as decompressing here, since we don't yet know whether
    // this Phi needs decompression. Instead, let
    // Node::SetTaggedResultNeedsDecompress pass through phis.
  }
#endif

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

  BasicBlock* predecessor_at(int i);

  void RecordUseReprHint(UseRepresentationSet repr_mask);

  UseRepresentationSet get_uses_repr_hints() { return uses_repr_hint_; }
  UseRepresentationSet get_same_loop_uses_repr_hints() {
    return same_loop_uses_repr_hint_;
  }

  NodeType post_loop_type() const { return post_loop_type_; }
  void merge_post_loop_type(NodeType type) {
    DCHECK(!has_key());
    post_loop_type_ = UnionType(post_loop_type_, type);
  }
  void set_post_loop_type(NodeType type) {
    DCHECK(!has_key());
    DCHECK(is_unmerged_loop_phi());
    post_loop_type_ = type;
  }
  void promote_post_loop_type() {
    DCHECK(!has_key());
    DCHECK(is_unmerged_loop_phi());
    DCHECK(NodeTypeIs(post_loop_type_, type_));
    type_ = post_loop_type_;
  }

  void merge_type(NodeType type) {
    DCHECK(!has_key());
    type_ = UnionType(type_, type);
  }
  void set_type(NodeType type) {
    DCHECK(!has_key());
    type_ = type;
  }
  NodeType type() const {
    DCHECK(!has_key());
    return type_;
  }

  using Key = compiler::turboshaft::SnapshotTable<ValueNode*>::Key;
  Key key() const {
    DCHECK(has_key());
    return key_;
  }
  void set_key(Key key) {
    set_bitfield(bitfield() | HasKeyFlag::encode(true));
    key_ = key;
  }

  // True if the {key_} field has been initialized.
  bool has_key() const { return HasKeyFlag::decode(bitfield()); }

  // Remembers if a use is unsafely untagged. If that happens we must ensure to
  // stay within the smi range, even when untagging.
  void SetUseRequires31BitValue();
  bool uses_require_31_bit_value() const {
    return Requires31BitValueFlag::decode(bitfield());
  }
  void set_uses_require_31_bit_value() {
    set_bitfield(bitfield() | Requires31BitValueFlag::encode(true));
  }

  // Check if a phi has cleared the loop.
  bool is_unmerged_loop_phi() const;

 private:
  Phi** next() { return &next_; }

  using HasKeyFlag = NextBitField<bool, 1>;
  using Requires31BitValueFlag = HasKeyFlag::Next<bool, 1>;
  using LoopPhiAfterLoopFlag = Requires31BitValueFlag::Next<bool, 1>;

  const interpreter::Register owner_;

  UseRepresentationSet uses_repr_hint_ = {};
  UseRepresentationSet same_loop_uses_repr_hint_ = {};

  Phi* next_ = nullptr;
  MergePointInterpreterFrameState* const merge_state_;

  union {
    struct {
      // The type of this Phi based on its predecessors' types.
      NodeType type_;
      // {type_} for loop Phis should always be Unknown until their backedge has
      // been bound (because we don't know what will be the type of the
      // backedge). However, once the backedge is bound, we might be able to
      // refine it. {post_loop_type_} is thus used to keep track of loop Phi
      // types: for loop Phis, we update {post_loop_type_} when we merge
      // predecessors, but keep {type_} as Unknown. Once the backedge is bound,
      // we set {type_} as {post_loop_type_}.
      NodeType post_loop_type_;
    };
    // After graph building, {type_} and {post_loop_type_} are not used anymore,
    // so we reuse this memory to store the SnapshotTable Key for this Phi for
    // phi untagging.
    Key key_;
  };

  friend base::ThreadedListTraits<Phi>;
};

class Call : public ValueNodeT<Call> {
  using Base = ValueNodeT<Call>;

 public:
  enum class TargetType { kJSFunction, kAny };
  // We assume function and context as fixed inputs.
  static constexpr int kFunctionIndex = 0;
  static constexpr int kContextIndex = 1;
  static constexpr int kFixedInputCount = 2;

  // We need enough inputs to have these fixed inputs plus the maximum arguments
  // to a function call.
  static_assert(kMaxInputs >= kFixedInputCount + Code::kMaxArguments);

  // This ctor is used when for variable input counts.
  // Inputs must be initialized manually.
  Call(uint64_t bitfield, ConvertReceiverMode mode, TargetType target_type,
       ValueNode* function, ValueNode* context)
      : Base(bitfield), receiver_mode_(mode), target_type_(target_type) {
    set_input(kFunctionIndex, function);
    set_input(kContextIndex, context);
  }

  static constexpr OpProperties kProperties = OpProperties::JSCall();

  Input function() { return input(kFunctionIndex); }
  ConstInput function() const { return input(kFunctionIndex); }
  Input context() { return input(kContextIndex); }
  ConstInput context() const { return input(kContextIndex); }
  int num_args() const { return input_count() - kFixedInputCount; }
  Input arg(int i) { return input(i + kFixedInputCount); }
  void set_arg(int i, ValueNode* node) {
    set_input(i + kFixedInputCount, node);
  }
  auto args() {
    return std::views::transform(std::views::iota(0, num_args()),
                                 [&](int i) { return arg(i); });
  }

  void VerifyInputs() const;
#ifdef V8_COMPRESS_POINTERS
  void MarkTaggedInputsAsDecompressing();
#endif
  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

  ConvertReceiverMode receiver_mode() const { return receiver_mode_; }
  TargetType target_type() const { return target_type_; }

 private:
  ConvertReceiverMode receiver_mode_;
  TargetType target_type_;
};

class Construct : public ValueNodeT<Construct> {
  using Base = ValueNodeT<Construct>;

 public:
  // We assume function and context as fixed inputs.
  static constexpr int kFunctionIndex = 0;
  static constexpr int kNewTargetIndex = 1;
  static constexpr int kContextIndex = 2;
  static constexpr int kFixedInputCount = 3;

  // We need enough inputs to have these fixed inputs plus the maximum arguments
  // to a function call.
  static_assert(kMaxInputs >= kFixedInputCount + Code::kMaxArguments);

  // This ctor is used when for variable input counts.
  // Inputs must be initialized manually.
  Construct(uint64_t bitfield, const compiler::FeedbackSource& feedback,
            ValueNode* function, ValueNode* new_target, ValueNode* context)
      : Base(bitfield), feedback_(feedback) {
    set_input(kFunctionIndex, function);
    set_input(kNewTargetIndex, new_target);
    set_input(kContextIndex, context);
  }

  static constexpr OpProperties kProperties = OpProperties::JSCall();

  Input function() { return input(kFunctionIndex); }
  ConstInput function() const { return input(kFunctionIndex); }
  Input new_target() { return input(kNewTargetIndex); }
  ConstInput new_target() const { return input(kNewTargetIndex); }
  Input context() { return input(kContextIndex); }
  ConstInput context() const { return input(kContextIndex); }
  int num_args() const { return input_count() - kFixedInputCount; }
  Input arg(int i) { return input(i + kFixedInputCount); }
  void set_arg(int i, ValueNode* node) {
    set_input(i + kFixedInputCount, node);
  }
  auto args() {
    return std::views::transform(std::views::iota(0, num_args()),
                                 [&](int i) { return arg(i); });
  }

  compiler::FeedbackSource feedback() const { return feedback_; }

  void VerifyInputs() const;
#ifdef V8_COMPRESS_POINTERS
  void MarkTaggedInputsAsDecompressing();
#endif
  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}

 private:
  const compiler::FeedbackSource feedback_;
};

class CallBuiltin : public ValueNodeT<CallBuiltin> {
  using Base = ValueNodeT<CallBuiltin>;

 public:
  enum FeedbackSlotType { kTaggedIndex, kSmi };

  // This ctor is used when for variable input counts.
  // Inputs must be initialized manually.
  CallBuiltin(uint64_t bitfield, Builtin builtin)
      : Base(bitfield), builtin_(builtin) {
    DCHECK(
        !Builtins::CallInterfaceDescriptorFor(builtin).HasContextParameter());
  }

  // This ctor is used when for variable input counts.
  // Inputs must be initialized manually.
  CallBuiltin(uint64_t bitfield, Builtin builtin, ValueNode* context)
      : Base(bitfield), builtin_(builtin) {
    DCHECK(Builtins::CallInterfaceDescriptorFor(builtin).HasContextParameter());
    // We use the last valid input for the context.
    set_input(input_count() - 1, context);
  }

  // This is an overestimation, since some builtins might not call JS code.
  static constexpr OpProperties kProperties = OpProperties::JSCall();

  bool has_feedback() const { return feedback_.has_value(); }
  compiler::FeedbackSource feedback() const {
    DCHECK(has_feedback());
    return feedback_.value();
  }
  FeedbackSlotType slot_type() const {
    DCHECK(has_feedback());
    return slot_type_;
  }
  void set_feedback(compiler::FeedbackSource const& feedback,
                    FeedbackSlotType slot_type) {
    feedback_ = feedback;
    slot_type_ = slot_type;
  }

  Builtin builtin() const { return builtin_; }
  Input context_input() {
    DCHECK(
        Builtins::CallInterfaceDescriptorFor(builtin()).HasContextParameter());
    return input(input_count() - 1);
  }

  int InputCountWithoutContext() const {
    auto descriptor = Builtins::CallInterfaceDescriptorFor(builtin_);
    bool has_context = descriptor.HasContextParameter();
    int extra_input_count = has_context ? 1 : 0;
    return input_count() - extra_input_count;
  }

  int InputsInRegisterCount() const {
    auto descriptor = Builtins::CallInterfaceDescriptorFor(builtin_);
    if (has_feedback()) {
      int slot_index = InputCountWithoutContext();
      int vector_index = slot_index + 1;

      // There are three possibilities:
      // 1. Feedback slot and vector are in register.
      // 2. Feedback slot is in register and vector is on stack.
      // 3. Feedback slot and vector are on stack.
      if (vector_index < descriptor.GetRegisterParameterCount()) {
        return descriptor.GetRegisterParameterCount() - 2;
      } else if (vector_index == descriptor.GetRegisterParameterCount()) {
        return descriptor.GetRegisterParameterCount() - 1;
      } else {
        return descriptor.GetRegisterParameterCount();
      }
    }
    return descriptor.GetRegisterParameterCount();
  }

  auto stack_args() {
    return std::views::transform(
        std::views::iota(InputsInRegisterCount(), InputCountWithoutContext()),
        [&](int i) { return input(i); });
  }

  void set_arg(int i, ValueNode* node) { set_input(i, node); }

  int ReturnCount() const {
    return Builtins::CallInterfaceDescriptorFor(builtin_).GetReturnCount();
  }

  void VerifyInputs() const;
#ifdef V8_COMPRESS_POINTERS
  void MarkTaggedInputsAsDecompressing();
#endif
  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

 private:
  template <typename... Args>
  void PushArguments(MaglevAssembler* masm, Args... extra_args);
  void PassFeedbackSlotInRegister(MaglevAssembler*);
  void PushFeedbackAndArguments(MaglevAssembler*);

  Builtin builtin_;
  std::optional<compiler::FeedbackSource> feedback_;
  FeedbackSlotType slot_type_ = kTaggedIndex;
};

class CallCPPBuiltin : public ValueNodeT<CallCPPBuiltin> {
  using Base = ValueNodeT<CallCPPBuiltin>;
  // Only 1 return value with arguments on the stack is supported.
  static constexpr Builtin kCEntry_Builtin =
      Builtin::kCEntry_Return1_ArgvOnStack_BuiltinExit;

 public:
  static constexpr int kTargetIndex = 0;
  static constexpr int kNewTargetIndex = 1;
  static constexpr int kContextIndex = 2;
  static constexpr int kFixedInputCount = 3;

  CallCPPBuiltin(uint64_t bitfield, Builtin builtin, ValueNode* target,
                 ValueNode* new_target, ValueNode* context)
      : Base(bitfield), builtin_(builtin) {
    DCHECK(Builtins::CallInterfaceDescriptorFor(builtin).HasContextParameter());
    DCHECK_EQ(Builtins::CallInterfaceDescriptorFor(builtin).GetReturnCount(),
              1);
    set_input(kTargetIndex, target);
    set_input(kNewTargetIndex, new_target);
    set_input(kContextIndex, context);
  }

  // This is an overestimation, since some builtins might not call JS code.
  static constexpr OpProperties kProperties = OpProperties::JSCall();

  Builtin builtin() const { return builtin_; }

  Input target() { return input(kTargetIndex); }
  ConstInput target() const { return input(kTargetIndex); }
  Input new_target() { return input(kNewTargetIndex); }
  ConstInput new_target() const { return input(kNewTargetIndex); }
  Input context() { return input(kContextIndex); }
  ConstInput context() const { return input(kContextIndex); }

  int num_args() const { return input_count() - kFixedInputCount; }
  Input arg(int i) { return input(i + kFixedInputCount); }
  void set_arg(int i, ValueNode* node) {
    set_input(i + kFixedInputCount, node);
  }

  auto args() {
    return std::views::transform(std::views::iota(0, num_args()),
                                 [&](int i) { return arg(i); });
  }

  void VerifyInputs() const;
#ifdef V8_COMPRESS_POINTERS
  void MarkTaggedInputsAsDecompressing();
#endif
  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

 private:
  Builtin builtin_;
};

class CallForwardVarargs : public ValueNodeT<CallForwardVarargs> {
  using Base = ValueNodeT<CallForwardVarargs>;

 public:
  static constexpr int kFunctionIndex = 0;
  static constexpr int kContextIndex = 1;
  static constexpr int kFixedInputCount = 2;

  // We need enough inputs to have these fixed inputs plus the maximum arguments
  // to a function call.
  static_assert(kMaxInputs >= kFixedInputCount + Code::kMaxArguments);

  // This ctor is used when for variable input counts.
  // Inputs must be initialized manually.
  CallForwardVarargs(uint64_t bitfield, ValueNode* function, ValueNode* context,
                     int start_index, Call::TargetType target_type)
      : Base(bitfield), start_index_(start_index), target_type_(target_type) {
    set_input(kFunctionIndex, function);
    set_input(kContextIndex, context);
  }

  static constexpr OpProperties kProperties = OpProperties::JSCall();

  Input function() { return input(kFunctionIndex); }
  ConstInput function() const { return input(kFunctionIndex); }
  Input context() { return input(kContextIndex); }
  ConstInput context() const { return input(kContextIndex); }
  int num_args() const { return input_count() - kFixedInputCount; }
  Input arg(int i) { return input(i + kFixedInputCount); }
  void set_arg(int i, ValueNode* node) {
    set_input(i + kFixedInputCount, node);
  }
  auto args() {
    return std::views::transform(std::views::iota(0, num_args()),
                                 [&](int i) { return arg(i); });
  }

  void VerifyInputs() const;
#ifdef V8_COMPRESS_POINTERS
  void MarkTaggedInputsAsDecompressing();
#endif
  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

  int start_index() const { return start_index_; }
  Call::TargetType target_type() const { return target_type_; }

 private:
  int start_index_;
  Call::TargetType target_type_;
};

class CallRuntime : public ValueNodeT<CallRuntime> {
  using Base = ValueNodeT<CallRuntime>;

 public:
  // We assume the context as fixed input.
  static constexpr int kContextIndex = 0;
  static constexpr int kFixedInputCount = 1;

  // This ctor is used when for variable input counts.
  // Inputs must be initialized manually.
  CallRuntime(uint64_t bitfield, Runtime::FunctionId function_id,
              ValueNode* context)
      : Base(bitfield), function_id_(function_id) {
    set_input(kContextIndex, context);
  }

  static constexpr OpProperties kProperties = OpProperties::JSCall();

  Runtime::FunctionId function_id() const { return function_id_; }

  Input context() { return input(kContextIndex); }
  ConstInput context() const { return input(kContextIndex); }
  int num_args() const { return input_count() - kFixedInputCount; }
  Input arg(int i) { return input(i + kFixedInputCount); }
  void set_arg(int i, ValueNode* node) {
    set_input(i + kFixedInputCount, node);
  }
  auto args() {
    return std::views::transform(std::views::iota(0, num_args()),
                                 [&](int i) { return arg(i); });
  }

  int ReturnCount() const {
    return Runtime::FunctionForId(function_id())->result_size;
  }

  void VerifyInputs() const;
#ifdef V8_COMPRESS_POINTERS
  void MarkTaggedInputsAsDecompressing();
#endif
  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

 private:
  Runtime::FunctionId function_id_;
};

class CallWithSpread : public ValueNodeT<CallWithSpread> {
  using Base = ValueNodeT<CallWithSpread>;

 public:
  // We assume function and context as fixed inputs.
  static constexpr int kFunctionIndex = 0;
  static constexpr int kContextIndex = 1;
  static constexpr int kFixedInputCount = 2;

  // This ctor is used when for variable input counts.
  // Inputs must be initialized manually.
  CallWithSpread(uint64_t bitfield, ValueNode* function, ValueNode* context)
      : Base(bitfield) {
    set_input(kFunctionIndex, function);
    set_input(kContextIndex, context);
  }

  static constexpr OpProperties kProperties = OpProperties::JSCall();

  Input function() { return input(kFunctionIndex); }
  ConstInput function() const { return input(kFunctionIndex); }
  Input context() { return input(kContextIndex); }
  ConstInput context() const { return input(kContextIndex); }
  int num_args() const { return input_count() - kFixedInputCount; }
  int num_args_no_spread() const {
    DCHECK_GT(num_args(), 0);
    return num_args() - 1;
  }
  Input arg(int i) { return input(i + kFixedInputCount); }
  void set_arg(int i, ValueNode* node) {
    set_input(i + kFixedInputCount, node);
  }
  auto args_no_spread() {
    return std::views::transform(std::views::iota(0, num_args_no_spread()),
                                 [&](int i) { return arg(i); });
  }

  Input spread() {
    // Spread is the last argument/input.
    return input(input_count() - 1);
  }
  Input receiver() { return arg(0); }

  void VerifyInputs() const;
#ifdef V8_COMPRESS_POINTERS
  void MarkTaggedInputsAsDecompressing();
#endif
  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class CallWithArrayLike : public FixedInputValueNodeT<4, CallWithArrayLike> {
  using Base = FixedInputValueNodeT<4, CallWithArrayLike>;

 public:
  // We assume function and context as fixed inputs.
  static constexpr int kFunctionIndex = 0;
  static constexpr int kReceiverIndex = 1;
  static constexpr int kArgumentsListIndex = 2;
  static constexpr int kContextIndex = 3;

  // This ctor is used when for variable input counts.
  // Inputs must be initialized manually.
  explicit CallWithArrayLike(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::JSCall();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged,
      ValueRepresentation::kTagged, ValueRepresentation::kTagged};

  Input function() { return input(kFunctionIndex); }
  Input receiver() { return input(kReceiverIndex); }
  Input arguments_list() { return input(kArgumentsListIndex); }
  Input context() { return input(kContextIndex); }

  void VerifyInputs() const;
#ifdef V8_COMPRESS_POINTERS
  void MarkTaggedInputsAsDecompressing();
#endif
  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class CallSelf : public ValueNodeT<CallSelf> {
  using Base = ValueNodeT<CallSelf>;

 public:
  static constexpr int kClosureIndex = 0;
  static constexpr int kContextIndex = 1;
  static constexpr int kReceiverIndex = 2;
  static constexpr int kNewTargetIndex = 3;
  static constexpr int kFixedInputCount = 4;

  // We need enough inputs to have these fixed inputs plus the maximum arguments
  // to a function call.
  static_assert(kMaxInputs >= kFixedInputCount + Code::kMaxArguments);

  // This ctor is used when for variable input counts.
  // Inputs must be initialized manually.
  CallSelf(uint64_t bitfield, int expected_parameter_count, ValueNode* closure,
           ValueNode* context, ValueNode* receiver, ValueNode* new_target)
      : Base(bitfield), expected_parameter_count_(expected_parameter_count) {
    set_input(kClosureIndex, closure);
    set_input(kContextIndex, context);
    set_input(kReceiverIndex, receiver);
    set_input(kNewTargetIndex, new_target);
  }

  static constexpr OpProperties kProperties = OpProperties::JSCall();

  Input closure() { return input(kClosureIndex); }
  ConstInput closure() const { return input(kClosureIndex); }
  Input context() { return input(kContextIndex); }
  ConstInput context() const { return input(kContextIndex); }
  Input receiver() { return input(kReceiverIndex); }
  ConstInput receiver() const { return input(kReceiverIndex); }
  Input new_target() { return input(kNewTargetIndex); }
  ConstInput new_target() const { return input(kNewTargetIndex); }
  int num_args() const { return input_count() - kFixedInputCount; }
  Input arg(int i) { return input(i + kFixedInputCount); }
  void set_arg(int i, ValueNode* node) {
    set_input(i + kFixedInputCount, node);
  }
  auto args() {
    return std::views::transform(std::views::iota(0, num_args()),
                                 [&](int i) { return arg(i); });
  }

  void VerifyInputs() const;
#ifdef V8_COMPRESS_POINTERS
  void MarkTaggedInputsAsDecompressing();
#endif
  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

 private:
  int expected_parameter_count_;
};

class CallKnownJSFunction : public ValueNodeT<CallKnownJSFunction> {
  using Base = ValueNodeT<CallKnownJSFunction>;

 public:
  static constexpr int kClosureIndex = 0;
  static constexpr int kContextIndex = 1;
  static constexpr int kReceiverIndex = 2;
  static constexpr int kNewTargetIndex = 3;
  static constexpr int kFixedInputCount = 4;

  // We need enough inputs to have these fixed inputs plus the maximum arguments
  // to a function call.
  static_assert(kMaxInputs >= kFixedInputCount + Code::kMaxArguments);

  // This ctor is used when for variable input counts.
  // Inputs must be initialized manually.
  inline CallKnownJSFunction(
      uint64_t bitfield,
#ifdef V8_ENABLE_LEAPTIERING
      JSDispatchHandle dispatch_handle,
#endif
      compiler::SharedFunctionInfoRef shared_function_info, ValueNode* closure,
      ValueNode* context, ValueNode* receiver, ValueNode* new_target);

  // This node might eventually be overwritten by conversion nodes that need
  // to do a deferred call.
  static constexpr OpProperties kProperties =
      OpProperties::JSCall() | OpProperties::DeferredCall();

  Input closure() { return input(kClosureIndex); }
  ConstInput closure() const { return input(kClosureIndex); }
  Input context() { return input(kContextIndex); }
  ConstInput context() const { return input(kContextIndex); }
  Input receiver() { return input(kReceiverIndex); }
  ConstInput receiver() const { return input(kReceiverIndex); }
  Input new_target() { return input(kNewTargetIndex); }
  ConstInput new_target() const { return input(kNewTargetIndex); }
  int num_args() const { return input_count() - kFixedInputCount; }
  Input arg(int i) { return input(i + kFixedInputCount); }
  void set_arg(int i, ValueNode* node) {
    set_input(i + kFixedInputCount, node);
  }
  auto args() {
    return std::views::transform(std::views::iota(0, num_args()),
                                 [&](int i) { return arg(i); });
  }

  compiler::SharedFunctionInfoRef shared_function_info() const {
    return shared_function_info_;
  }

  void VerifyInputs() const;
#ifdef V8_COMPRESS_POINTERS
  void MarkTaggedInputsAsDecompressing();
#endif
  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

  int expected_parameter_count() const { return expected_parameter_count_; }

  void RecordUseReprHint(UseRepresentationSet repr_mask) {
    uses_repr_hint_.Add(repr_mask);
  }
  UseRepresentationSet get_uses_repr_hints() { return uses_repr_hint_; }

 private:
#ifdef V8_ENABLE_LEAPTIERING
  JSDispatchHandle dispatch_handle_;
#endif
  const compiler::SharedFunctionInfoRef shared_function_info_;
  // Cache the expected parameter count so that we can access it in
  // MaxCallStackArgs without needing to unpark the local isolate.
  int expected_parameter_count_;

  UseRepresentationSet uses_repr_hint_ = {};
};

// This node overwrites CallKnownJSFunction in-place after inlining.
// Although ReturnedValue has only a single input, it is not a
// FixedInputValueNode, since it accepts any input type and it
// cannot declare a kInputTypes.
class ReturnedValue : public ValueNodeT<ReturnedValue> {
  using Base = ValueNodeT<ReturnedValue>;

 public:
  static_assert(CallKnownJSFunction::kFixedInputCount > 1);
  explicit ReturnedValue(uint64_t bitfield) : Base(bitfield) {}
  static constexpr OpProperties kProperties =
      OpProperties::CanAllocate() | OpProperties::DeferredCall();
  void VerifyInputs() const {
    // It doesn't make sense if the input is already tagged. Otherwise it can be
    // anything.
    DCHECK(!input(0).node()->is_tagged());
    DCHECK_EQ(input_count(), 1);
  }
#ifdef V8_COMPRESS_POINTERS
  void MarkTaggedInputsAsDecompressing() { UNREACHABLE(); }
#endif
  int MaxCallStackArgs() const { return 0; }
  void SetValueLocationConstraints() { UNREACHABLE(); }
  void GenerateCode(MaglevAssembler*, const ProcessingState&) { UNREACHABLE(); }
  void PrintParams(std::ostream&) const {}
};
static_assert(sizeof(ReturnedValue) <= sizeof(CallKnownJSFunction));

class CallKnownApiFunction : public ValueNodeT<CallKnownApiFunction> {
  using Base = ValueNodeT<CallKnownApiFunction>;

 public:
  enum Mode {
    // Use Builtin::kCallApiCallbackOptimizedNoProfiling.
    kNoProfiling,
    // Inline API call sequence into the generated code.
    kNoProfilingInlined,
    // Use Builtin::kCallApiCallbackOptimized.
    kGeneric,
  };

  static constexpr int kReceiverIndex = 0;
  static constexpr int kFixedInputCount = 1;

  // We need enough inputs to have these fixed inputs plus the maximum arguments
  // to a function call.
  static_assert(kMaxInputs >= kFixedInputCount + Code::kMaxArguments);

  // This ctor is used when for variable input counts.
  // Inputs must be initialized manually.
  CallKnownApiFunction(uint64_t bitfield, Mode mode,
                       compiler::FunctionTemplateInfoRef function_template_info,
                       ValueNode* receiver)
      : Base(bitfield | ModeField::encode(mode)),
        function_template_info_(function_template_info) {
    set_input(kReceiverIndex, receiver);
  }

  // TODO(ishell): introduce JSApiCall() which will take C++ ABI into account
  // when deciding which registers to splill.
  static constexpr OpProperties kProperties = OpProperties::JSCall();

  Input receiver() { return input(kReceiverIndex); }
  ConstInput receiver() const { return input(kReceiverIndex); }
  int num_args() const { return input_count() - kFixedInputCount; }
  Input arg(int i) { return input(i + kFixedInputCount); }
  void set_arg(int i, ValueNode* node) {
    set_input(i + kFixedInputCount, node);
  }
  auto args() {
    return std::views::transform(std::views::iota(0, num_args()),
                                 [&](int i) { return arg(i); });
  }

  Mode mode() const { return ModeField::decode(bitfield()); }

  compiler::FunctionTemplateInfoRef function_template_info() const {
    return function_template_info_;
  }

  bool inline_builtin() const { return mode() == kNoProfilingInlined; }

  void VerifyInputs() const;
#ifdef V8_COMPRESS_POINTERS
  void MarkTaggedInputsAsDecompressing();
#endif
  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

 private:
  using ModeField = NextBitField<Mode, 2>;

  void GenerateCallApiCallbackOptimizedInline(MaglevAssembler* masm,
                                              const ProcessingState& state);

  const compiler::FunctionTemplateInfoRef function_template_info_;
  const compiler::OptionalJSObjectRef api_holder_;
};

void ValueNode::MaybeRecordUseReprHint(UseRepresentation repr) {
  MaybeRecordUseReprHint(UseRepresentationSet{repr});
}

void ValueNode::MaybeRecordUseReprHint(UseRepresentationSet repr_mask) {
  if (Phi* phi = TryCast<Phi>()) {
    phi->RecordUseReprHint(repr_mask);
  }
  if (CallKnownJSFunction* call = TryCast<CallKnownJSFunction>()) {
    // std::cout << "Recording UseRepr hint for call : " << repr_mask << "\n";
    call->RecordUseReprHint(repr_mask);
  }
}

class ConstructWithSpread : public ValueNodeT<ConstructWithSpread> {
  using Base = ValueNodeT<ConstructWithSpread>;

 public:
  // We assume function and context as fixed inputs.
  static constexpr int kFunctionIndex = 0;
  static constexpr int kNewTargetIndex = 1;
  static constexpr int kContextIndex = 2;
  static constexpr int kFixedInputCount = 3;

  // This ctor is used when for variable input counts.
  // Inputs must be initialized manually.
  ConstructWithSpread(uint64_t bitfield, compiler::FeedbackSource feedback,
                      ValueNode* function, ValueNode* new_target,
                      ValueNode* context)
      : Base(bitfield), feedback_(feedback) {
    set_input(kFunctionIndex, function);
    set_input(kNewTargetIndex, new_target);
    set_input(kContextIndex, context);
  }

  static constexpr OpProperties kProperties = OpProperties::JSCall();

  Input function() { return input(kFunctionIndex); }
  ConstInput function() const { return input(kFunctionIndex); }
  Input new_target() { return input(kNewTargetIndex); }
  ConstInput new_target() const { return input(kNewTargetIndex); }
  Input context() { return input(kContextIndex); }
  ConstInput context() const { return input(kContextIndex); }
  int num_args() const { return input_count() - kFixedInputCount; }
  int num_args_no_spread() const {
    DCHECK_GT(num_args(), 0);
    return num_args() - 1;
  }
  Input arg(int i) { return input(i + kFixedInputCount); }
  void set_arg(int i, ValueNode* node) {
    set_input(i + kFixedInputCount, node);
  }
  Input spread() {
    // Spread is the last argument/input.
    return input(input_count() - 1);
  }
  auto args_no_spread() {
    return std::views::transform(std::views::iota(0, num_args_no_spread()),
                                 [&](int i) { return arg(i); });
  }
  compiler::FeedbackSource feedback() const { return feedback_; }

  void VerifyInputs() const;
#ifdef V8_COMPRESS_POINTERS
  void MarkTaggedInputsAsDecompressing();
#endif
  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}

 private:
  const compiler::FeedbackSource feedback_;
};

class ConvertReceiver : public FixedInputValueNodeT<1, ConvertReceiver> {
  using Base = FixedInputValueNodeT<1, ConvertReceiver>;

 public:
  explicit ConvertReceiver(uint64_t bitfield,
                           compiler::NativeContextRef native_context,
                           ConvertReceiverMode mode)
      : Base(bitfield), native_context_(native_context), mode_(mode) {}

  Input receiver_input() { return input(0); }

  // The implementation currently calls runtime.
  static constexpr OpProperties kProperties = OpProperties::Call() |
                                              OpProperties::CanAllocate() |
                                              OpProperties::NotIdempotent();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}

  auto options() const { return std::tuple{native_context_, mode_}; }

  compiler::NativeContextRef native_context() const { return native_context_; }
  ConvertReceiverMode mode() const { return mode_; }

 private:
  const compiler::NativeContextRef native_context_;
  ConvertReceiverMode mode_;
};

class CheckConstructResult
    : public FixedInputValueNodeT<2, CheckConstructResult> {
  using Base = FixedInputValueNodeT<2, CheckConstructResult>;

 public:
  explicit CheckConstructResult(uint64_t bitfield) : Base(bitfield) {}

  Input construct_result_input() { return input(0); }
  Input implicit_receiver_input() { return input(1); }

  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged};

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class CheckDerivedConstructResult
    : public FixedInputValueNodeT<1, CheckDerivedConstructResult> {
  using Base = FixedInputValueNodeT<1, CheckDerivedConstructResult>;

 public:
  explicit CheckDerivedConstructResult(uint64_t bitfield) : Base(bitfield) {}

  Input construct_result_input() { return input(0); }

  static constexpr OpProperties kProperties = OpProperties::CanThrow() |
                                              OpProperties::CanAllocate() |
                                              OpProperties::DeferredCall();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  bool for_derived_constructor();

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class CheckJSReceiverOrNullOrUndefined
    : public FixedInputNodeT<1, CheckJSReceiverOrNullOrUndefined> {
  using Base = FixedInputNodeT<1, CheckJSReceiverOrNullOrUndefined>;

 public:
  explicit CheckJSReceiverOrNullOrUndefined(uint64_t bitfield,
                                            CheckType check_type)
      : Base(CheckTypeBitField::update(bitfield, check_type)) {}

  static constexpr OpProperties kProperties = OpProperties::EagerDeopt();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  Input object_input() { return input(0); }
  CheckType check_type() const { return CheckTypeBitField::decode(bitfield()); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}

  auto options() const { return std::tuple{check_type()}; }

 private:
  using CheckTypeBitField = NextBitField<CheckType, 1>;
};

class CheckNotHole : public FixedInputNodeT<1, CheckNotHole> {
  using Base = FixedInputNodeT<1, CheckNotHole>;

 public:
  explicit CheckNotHole(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::EagerDeopt();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  Input object_input() { return input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class CheckHoleyFloat64NotHole
    : public FixedInputNodeT<1, CheckHoleyFloat64NotHole> {
  using Base = FixedInputNodeT<1, CheckHoleyFloat64NotHole>;

 public:
  explicit CheckHoleyFloat64NotHole(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::EagerDeopt();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kHoleyFloat64};

  Input float64_input() { return input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class ConvertHoleToUndefined
    : public FixedInputValueNodeT<1, ConvertHoleToUndefined> {
  using Base = FixedInputValueNodeT<1, ConvertHoleToUndefined>;

 public:
  explicit ConvertHoleToUndefined(uint64_t bitfield) : Base(bitfield) {}

  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  Input object_input() { return input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class HandleNoHeapWritesInterrupt
    : public FixedInputNodeT<0, HandleNoHeapWritesInterrupt> {
  using Base = FixedInputNodeT<0, HandleNoHeapWritesInterrupt>;

 public:
  explicit HandleNoHeapWritesInterrupt(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties = OpProperties::DeferredCall() |
                                              OpProperties::LazyDeopt() |
                                              OpProperties::NotIdempotent();

  void SetValueLocationConstraints() {}
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
  int MaxCallStackArgs() const { return 0; }
};

class ReduceInterruptBudgetForLoop
    : public FixedInputNodeT<1, ReduceInterruptBudgetForLoop> {
  using Base = FixedInputNodeT<1, ReduceInterruptBudgetForLoop>;

 public:
  explicit ReduceInterruptBudgetForLoop(uint64_t bitfield, int amount)
      : Base(bitfield), amount_(amount) {
    DCHECK_GT(amount, 0);
  }

  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  static constexpr OpProperties kProperties =
      OpProperties::DeferredCall() | OpProperties::CanAllocate() |
      OpProperties::LazyDeopt() | OpProperties::NotIdempotent();

  int amount() const { return amount_; }

  Input feedback_cell() { return input(0); }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

 private:
  const int amount_;
};

class ReduceInterruptBudgetForReturn
    : public FixedInputNodeT<1, ReduceInterruptBudgetForReturn> {
  using Base = FixedInputNodeT<1, ReduceInterruptBudgetForReturn>;

 public:
  explicit ReduceInterruptBudgetForReturn(uint64_t bitfield, int amount)
      : Base(bitfield), amount_(amount) {
    DCHECK_GT(amount, 0);
  }

  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  static constexpr OpProperties kProperties =
      OpProperties::DeferredCall() | OpProperties::NotIdempotent();

  int amount() const { return amount_; }

  Input feedback_cell() { return input(0); }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

 private:
  const int amount_;
};

class ThrowReferenceErrorIfHole
    : public FixedInputNodeT<1, ThrowReferenceErrorIfHole> {
  using Base = FixedInputNodeT<1, ThrowReferenceErrorIfHole>;

 public:
  explicit ThrowReferenceErrorIfHole(uint64_t bitfield,
                                     const compiler::NameRef name)
      : Base(bitfield), name_(name) {}

  static constexpr OpProperties kProperties =
      OpProperties::CanThrow() | OpProperties::DeferredCall();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  compiler::NameRef name() const { return name_; }

  Input value() { return Node::input(0); }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}

  auto options() const { return std::tuple{name_}; }

 private:
  const compiler::NameRef name_;
};

class ThrowSuperNotCalledIfHole
    : public FixedInputNodeT<1, ThrowSuperNotCalledIfHole> {
  using Base = FixedInputNodeT<1, ThrowSuperNotCalledIfHole>;

 public:
  explicit ThrowSuperNotCalledIfHole(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties =
      OpProperties::CanThrow() | OpProperties::DeferredCall();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  Input value() { return Node::input(0); }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class ThrowSuperAlreadyCalledIfNotHole
    : public FixedInputNodeT<1, ThrowSuperAlreadyCalledIfNotHole> {
  using Base = FixedInputNodeT<1, ThrowSuperAlreadyCalledIfNotHole>;

 public:
  explicit ThrowSuperAlreadyCalledIfNotHole(uint64_t bitfield)
      : Base(bitfield) {}

  static constexpr OpProperties kProperties =
      OpProperties::CanThrow() | OpProperties::DeferredCall();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  Input value() { return Node::input(0); }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class ThrowIfNotCallable : public FixedInputNodeT<1, ThrowIfNotCallable> {
  using Base = FixedInputNodeT<1, ThrowIfNotCallable>;

 public:
  explicit ThrowIfNotCallable(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties =
      OpProperties::CanThrow() | OpProperties::DeferredCall();
  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  Input value() { return Node::input(0); }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class ThrowIfNotSuperConstructor
    : public FixedInputNodeT<2, ThrowIfNotSuperConstructor> {
  using Base = FixedInputNodeT<2, ThrowIfNotSuperConstructor>;

 public:
  explicit ThrowIfNotSuperConstructor(uint64_t bitfield) : Base(bitfield) {}

  static constexpr OpProperties kProperties =
      OpProperties::CanThrow() | OpProperties::DeferredCall();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged};

  Input constructor() { return Node::input(0); }
  Input function() { return Node::input(1); }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class TransitionElementsKind
    : public FixedInputValueNodeT<2, TransitionElementsKind> {
  using Base = FixedInputValueNodeT<2, TransitionElementsKind>;

 public:
  explicit TransitionElementsKind(
      uint64_t bitfield, const ZoneVector<compiler::MapRef>& transition_sources,
      compiler::MapRef transition_target)
      : Base(bitfield),
        transition_sources_(transition_sources),
        transition_target_(transition_target) {}

  // TODO(leszeks): Special case the case where all transitions are fast.
  static constexpr OpProperties kProperties =
      OpProperties::AnySideEffects() | OpProperties::DeferredCall();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged};

  Input object_input() { return input(0); }
  Input map_input() { return input(1); }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}

  const ZoneVector<compiler::MapRef>& transition_sources() const {
    return transition_sources_;
  }
  const compiler::MapRef transition_target() const {
    return transition_target_;
  }

 private:
  ZoneVector<compiler::MapRef> transition_sources_;
  const compiler::MapRef transition_target_;
};

class TransitionElementsKindOrCheckMap
    : public FixedInputNodeT<2, TransitionElementsKindOrCheckMap> {
  using Base = FixedInputNodeT<2, TransitionElementsKindOrCheckMap>;

 public:
  explicit TransitionElementsKindOrCheckMap(
      uint64_t bitfield, const ZoneVector<compiler::MapRef>& transition_sources,
      compiler::MapRef transition_target)
      : Base(bitfield),
        transition_sources_(transition_sources),
        transition_target_(transition_target) {}

  // TODO(leszeks): Special case the case where all transitions are fast.
  static constexpr OpProperties kProperties = OpProperties::AnySideEffects() |
                                              OpProperties::DeferredCall() |
                                              OpProperties::EagerDeopt();
  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged};

  Input object_input() { return Node::input(0); }
  Input map_input() { return Node::input(1); }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

  const ZoneVector<compiler::MapRef>& transition_sources() const {
    return transition_sources_;
  }
  const compiler::MapRef transition_target() const {
    return transition_target_;
  }

 private:
  ZoneVector<compiler::MapRef> transition_sources_;
  const compiler::MapRef transition_target_;
};

class GetContinuationPreservedEmbedderData
    : public FixedInputValueNodeT<0, GetContinuationPreservedEmbedderData> {
  using Base = FixedInputValueNodeT<0, GetContinuationPreservedEmbedderData>;

 public:
  explicit GetContinuationPreservedEmbedderData(uint64_t bitfield)
      : Base(bitfield) {}

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}

  static constexpr OpProperties kProperties =
      OpProperties::CanRead() | OpProperties::TaggedValue();
};

class SetContinuationPreservedEmbedderData
    : public FixedInputNodeT<1, SetContinuationPreservedEmbedderData> {
  using Base = FixedInputNodeT<1, SetContinuationPreservedEmbedderData>;

 public:
  explicit SetContinuationPreservedEmbedderData(uint64_t bitfield)
      : Base(bitfield) {}

  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  Input data_input() { return input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}

  static constexpr OpProperties kProperties = OpProperties::CanWrite();
};

class ControlNode : public NodeBase {
 public:
  // A "hole" in control flow is a control node that unconditionally interrupts
  // linear control flow (either by jumping or by exiting).
  //
  // A "post-dominating" hole is a hole that is guaranteed to be be reached in
  // control flow after this node (i.e. it is a hole that is a post-dominator
  // of this node).
  ControlNode* next_post_dominating_hole() const {
    return next_post_dominating_hole_;
  }
  void set_next_post_dominating_hole(ControlNode* node) {
    DCHECK_IMPLIES(node != nullptr, node->Is<UnconditionalControlNode>() ||
                                        node->Is<TerminalControlNode>() ||
                                        node->Is<Switch>());
    next_post_dominating_hole_ = node;
  }

 protected:
  using NodeBase::NodeBase;

 private:
  ControlNode* next_post_dominating_hole_ = nullptr;
};

class UnconditionalControlNode : public ControlNode {
 public:
  BasicBlock* target() const { return target_.block_ptr(); }
  int predecessor_id() const { return predecessor_id_; }
  void set_predecessor_id(int id) { predecessor_id_ = id; }

  void set_target(BasicBlock* block) { target_.set_block_ptr(block); }

 protected:
  explicit UnconditionalControlNode(uint64_t bitfield,
                                    BasicBlockRef* target_refs)
      : ControlNode(bitfield), target_(target_refs) {}
  explicit UnconditionalControlNode(uint64_t bitfield, BasicBlock* target)
      : ControlNode(bitfield), target_(target) {}

 private:
  BasicBlockRef target_;
  int predecessor_id_ = 0;
};

template <class Derived>
class UnconditionalControlNodeT
    : public FixedInputNodeTMixin<0, UnconditionalControlNode, Derived> {
  static_assert(IsUnconditionalControlNode(NodeBase::opcode_of<Derived>));

 protected:
  explicit UnconditionalControlNodeT(uint64_t bitfield,
                                     BasicBlockRef* target_refs)
      : FixedInputNodeTMixin<0, UnconditionalControlNode, Derived>(
            bitfield, target_refs) {}
  explicit UnconditionalControlNodeT(uint64_t bitfield, BasicBlock* target)
      : FixedInputNodeTMixin<0, UnconditionalControlNode, Derived>(bitfield,
                                                                   target) {}
};

class ConditionalControlNode : public ControlNode {
 public:
  explicit ConditionalControlNode(uint64_t bitfield) : ControlNode(bitfield) {}
};

class BranchControlNode : public ConditionalControlNode {
 public:
  BranchControlNode(uint64_t bitfield, BasicBlockRef* if_true_refs,
                    BasicBlockRef* if_false_refs)
      : ConditionalControlNode(bitfield),
        if_true_(if_true_refs),
        if_false_(if_false_refs) {}

  BasicBlock* if_true() const { return if_true_.block_ptr(); }
  BasicBlock* if_false() const { return if_false_.block_ptr(); }

  void set_if_true(BasicBlock* block) { if_true_.set_block_ptr(block); }
  void set_if_false(BasicBlock* block) { if_false_.set_block_ptr(block); }

 private:
  BasicBlockRef if_true_;
  BasicBlockRef if_false_;
};

class TerminalControlNode : public ControlNode {
 protected:
  explicit TerminalControlNode(uint64_t bitfield) : ControlNode(bitfield) {}
};

template <size_t InputCount, class Derived>
class TerminalControlNodeT
    : public FixedInputNodeTMixin<InputCount, TerminalControlNode, Derived> {
  static_assert(IsTerminalControlNode(NodeBase::opcode_of<Derived>));

 protected:
  explicit TerminalControlNodeT(uint64_t bitfield)
      : FixedInputNodeTMixin<InputCount, TerminalControlNode, Derived>(
            bitfield) {}
};

template <size_t InputCount, class Derived>
class BranchControlNodeT
    : public FixedInputNodeTMixin<InputCount, BranchControlNode, Derived> {
  static_assert(IsBranchControlNode(NodeBase::opcode_of<Derived>));

 protected:
  explicit BranchControlNodeT(uint64_t bitfield, BasicBlockRef* if_true_refs,
                              BasicBlockRef* if_false_refs)
      : FixedInputNodeTMixin<InputCount, BranchControlNode, Derived>(
            bitfield, if_true_refs, if_false_refs) {}
};

class Jump : public UnconditionalControlNodeT<Jump> {
  using Base = UnconditionalControlNodeT<Jump>;

 public:
  Jump(uint64_t bitfield, BasicBlockRef* target_refs)
      : Base(bitfield, target_refs) {}

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

// TODO(olivf): Unify implementation with Jump.
class CheckpointedJump : public UnconditionalControlNodeT<CheckpointedJump> {
  using Base = UnconditionalControlNodeT<CheckpointedJump>;

 public:
  CheckpointedJump(uint64_t bitfield, BasicBlockRef* target_refs)
      : Base(bitfield, target_refs) {}

  static constexpr OpProperties kProperties =
      OpProperties::DeoptCheckpoint() | Base::kProperties;

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class JumpLoop : public UnconditionalControlNodeT<JumpLoop> {
  using Base = UnconditionalControlNodeT<JumpLoop>;

 public:
  explicit JumpLoop(uint64_t bitfield, BasicBlock* target)
      : Base(bitfield, target) {}

  explicit JumpLoop(uint64_t bitfield, BasicBlockRef* ref)
      : Base(bitfield, ref) {}

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}

  base::Vector<std::pair<ValueNode*, InputLocation>> used_nodes() {
    return used_node_locations_;
  }
  void set_used_nodes(
      base::Vector<std::pair<ValueNode*, InputLocation>> locations) {
    used_node_locations_ = locations;
  }

 private:
  base::Vector<std::pair<ValueNode*, InputLocation>> used_node_locations_;
};

class Abort : public TerminalControlNodeT<0, Abort> {
  using Base = TerminalControlNodeT<0, Abort>;

 public:
  explicit Abort(uint64_t bitfield, AbortReason reason)
      : Base(bitfield), reason_(reason) {
    DCHECK_EQ(NodeBase::opcode(), opcode_of<Abort>);
  }

  static constexpr OpProperties kProperties = OpProperties::Call();

  AbortReason reason() const { return reason_; }

  int MaxCallStackArgs() const;
  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

 private:
  const AbortReason reason_;
};

class Return : public TerminalControlNodeT<1, Return> {
  using Base = TerminalControlNodeT<1, Return>;

 public:
  explicit Return(uint64_t bitfield) : Base(bitfield) {
    DCHECK_EQ(NodeBase::opcode(), opcode_of<Return>);
  }

  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  Input value_input() { return input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class Deopt : public TerminalControlNodeT<0, Deopt> {
  using Base = TerminalControlNodeT<0, Deopt>;

 public:
  explicit Deopt(uint64_t bitfield, DeoptimizeReason reason)
      : Base(bitfield | ReasonField::encode(reason)) {
    DCHECK_EQ(NodeBase::opcode(), opcode_of<Deopt>);
  }

  static constexpr OpProperties kProperties = OpProperties::EagerDeopt();

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

  DEOPTIMIZE_REASON_FIELD
};

class Switch : public FixedInputNodeTMixin<1, ConditionalControlNode, Switch> {
  using Base = FixedInputNodeTMixin<1, ConditionalControlNode, Switch>;

 public:
  explicit Switch(uint64_t bitfield, int value_base, BasicBlockRef* targets,
                  int size)
      : Base(bitfield),
        value_base_(value_base),
        targets_(targets),
        size_(size),
        fallthrough_() {}

  explicit Switch(uint64_t bitfield, int value_base, BasicBlockRef* targets,
                  int size, BasicBlockRef* fallthrough)
      : Base(bitfield),
        value_base_(value_base),
        targets_(targets),
        size_(size),
        fallthrough_(fallthrough) {}

  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kInt32};

  int value_base() const { return value_base_; }
  BasicBlockRef* targets() const { return targets_; }
  int size() const { return size_; }

  bool has_fallthrough() const { return fallthrough_.has_value(); }
  BasicBlock* fallthrough() const {
    DCHECK(has_fallthrough());
    return fallthrough_.value().block_ptr();
  }

  void set_fallthrough(BasicBlock* fallthrough) {
    DCHECK(has_fallthrough());
    fallthrough_.value().set_block_ptr(fallthrough);
  }

  Input value() { return input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}

 private:
  const int value_base_;
  BasicBlockRef* targets_;
  const int size_;
  std::optional<BasicBlockRef> fallthrough_;
};

class BranchIfSmi : public BranchControlNodeT<1, BranchIfSmi> {
  using Base = BranchControlNodeT<1, BranchIfSmi>;

 public:
  explicit BranchIfSmi(uint64_t bitfield, BasicBlockRef* if_true_refs,
                       BasicBlockRef* if_false_refs)
      : Base(bitfield, if_true_refs, if_false_refs) {}

  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  Input condition_input() { return input(0); }

#ifdef V8_COMPRESS_POINTERS
  void MarkTaggedInputsAsDecompressing() {
    // Don't need to decompress values to reference compare.
  }
#endif

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class BranchIfRootConstant
    : public BranchControlNodeT<1, BranchIfRootConstant> {
  using Base = BranchControlNodeT<1, BranchIfRootConstant>;

 public:
  explicit BranchIfRootConstant(uint64_t bitfield, RootIndex root_index,
                                BasicBlockRef* if_true_refs,
                                BasicBlockRef* if_false_refs)
      : Base(bitfield, if_true_refs, if_false_refs), root_index_(root_index) {}

  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  RootIndex root_index() { return root_index_; }
  Input condition_input() { return input(0); }

#ifdef V8_COMPRESS_POINTERS
  void MarkTaggedInputsAsDecompressing() {
    // Don't need to decompress values to reference compare.
  }
#endif

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

 private:
  RootIndex root_index_;
};

class BranchIfUndefinedOrNull
    : public BranchControlNodeT<1, BranchIfUndefinedOrNull> {
  using Base = BranchControlNodeT<1, BranchIfUndefinedOrNull>;

 public:
  explicit BranchIfUndefinedOrNull(uint64_t bitfield,
                                   BasicBlockRef* if_true_refs,
                                   BasicBlockRef* if_false_refs)
      : Base(bitfield, if_true_refs, if_false_refs) {}

  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  Input condition_input() { return input(0); }

#ifdef V8_COMPRESS_POINTERS
  void MarkTaggedInputsAsDecompressing() {
    // Don't need to decompress values to reference compare.
  }
#endif

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class BranchIfUndetectable
    : public BranchControlNodeT<1, BranchIfUndetectable> {
  using Base = BranchControlNodeT<1, BranchIfUndetectable>;

 public:
  explicit BranchIfUndetectable(uint64_t bitfield, CheckType check_type,
                                BasicBlockRef* if_true_refs,
                                BasicBlockRef* if_false_refs)
      : Base(CheckTypeBitField::update(bitfield, check_type), if_true_refs,
             if_false_refs) {}

  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  Input condition_input() { return input(0); }
  CheckType check_type() const { return CheckTypeBitField::decode(bitfield()); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}

 private:
  using CheckTypeBitField = NextBitField<CheckType, 1>;
};

class BranchIfJSReceiver : public BranchControlNodeT<1, BranchIfJSReceiver> {
  using Base = BranchControlNodeT<1, BranchIfJSReceiver>;

 public:
  explicit BranchIfJSReceiver(uint64_t bitfield, BasicBlockRef* if_true_refs,
                              BasicBlockRef* if_false_refs)
      : Base(bitfield, if_true_refs, if_false_refs) {}

  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  Input condition_input() { return input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class BranchIfToBooleanTrue
    : public BranchControlNodeT<1, BranchIfToBooleanTrue> {
  using Base = BranchControlNodeT<1, BranchIfToBooleanTrue>;

 public:
  explicit BranchIfToBooleanTrue(uint64_t bitfield, CheckType check_type,
                                 BasicBlockRef* if_true_refs,
                                 BasicBlockRef* if_false_refs)
      : Base(CheckTypeBitField::update(bitfield, check_type), if_true_refs,
             if_false_refs) {}

  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  Input condition_input() { return input(0); }
  CheckType check_type() const { return CheckTypeBitField::decode(bitfield()); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}

 private:
  using CheckTypeBitField = NextBitField<CheckType, 1>;
};

class BranchIfInt32ToBooleanTrue
    : public BranchControlNodeT<1, BranchIfInt32ToBooleanTrue> {
  using Base = BranchControlNodeT<1, BranchIfInt32ToBooleanTrue>;

 public:
  explicit BranchIfInt32ToBooleanTrue(uint64_t bitfield,
                                      BasicBlockRef* if_true_refs,
                                      BasicBlockRef* if_false_refs)
      : Base(bitfield, if_true_refs, if_false_refs) {}

  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kInt32};

  Input condition_input() { return input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class BranchIfIntPtrToBooleanTrue
    : public BranchControlNodeT<1, BranchIfIntPtrToBooleanTrue> {
  using Base = BranchControlNodeT<1, BranchIfIntPtrToBooleanTrue>;

 public:
  explicit BranchIfIntPtrToBooleanTrue(uint64_t bitfield,
                                       BasicBlockRef* if_true_refs,
                                       BasicBlockRef* if_false_refs)
      : Base(bitfield, if_true_refs, if_false_refs) {}

  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kIntPtr};

  Input condition_input() { return input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class BranchIfFloat64ToBooleanTrue
    : public BranchControlNodeT<1, BranchIfFloat64ToBooleanTrue> {
  using Base = BranchControlNodeT<1, BranchIfFloat64ToBooleanTrue>;

 public:
  explicit BranchIfFloat64ToBooleanTrue(uint64_t bitfield,
                                        BasicBlockRef* if_true_refs,
                                        BasicBlockRef* if_false_refs)
      : Base(bitfield, if_true_refs, if_false_refs) {}

  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kHoleyFloat64};

  Input condition_input() { return input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

#ifdef V8_ENABLE_EXPERIMENTAL_UNDEFINED_DOUBLE
class BranchIfFloat64IsUndefinedOrHole
    : public BranchControlNodeT<1, BranchIfFloat64IsUndefinedOrHole> {
  using Base = BranchControlNodeT<1, BranchIfFloat64IsUndefinedOrHole>;

 public:
  explicit BranchIfFloat64IsUndefinedOrHole(uint64_t bitfield,
                                            BasicBlockRef* if_true_refs,
                                            BasicBlockRef* if_false_refs)
      : Base(bitfield, if_true_refs, if_false_refs) {}

  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kHoleyFloat64};

  Input condition_input() { return input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};
#endif  // V8_ENABLE_EXPERIMENTAL_UNDEFINED_DOUBLE

class BranchIfFloat64IsHole
    : public BranchControlNodeT<1, BranchIfFloat64IsHole> {
  using Base = BranchControlNodeT<1, BranchIfFloat64IsHole>;

 public:
  explicit BranchIfFloat64IsHole(uint64_t bitfield, BasicBlockRef* if_true_refs,
                                 BasicBlockRef* if_false_refs)
      : Base(bitfield, if_true_refs, if_false_refs) {}

  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kHoleyFloat64};

  Input condition_input() { return input(0); }

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class BranchIfInt32Compare
    : public BranchControlNodeT<2, BranchIfInt32Compare> {
  using Base = BranchControlNodeT<2, BranchIfInt32Compare>;

 public:
  static constexpr int kLeftIndex = 0;
  static constexpr int kRightIndex = 1;
  Input left_input() { return NodeBase::input(kLeftIndex); }
  Input right_input() { return NodeBase::input(kRightIndex); }

  explicit BranchIfInt32Compare(uint64_t bitfield, Operation operation,
                                BasicBlockRef* if_true_refs,
                                BasicBlockRef* if_false_refs)
      : Base(bitfield, if_true_refs, if_false_refs), operation_(operation) {}

  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kInt32, ValueRepresentation::kInt32};

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

  Operation operation() const { return operation_; }

 private:
  Operation operation_;
};

class BranchIfUint32Compare
    : public BranchControlNodeT<2, BranchIfUint32Compare> {
  using Base = BranchControlNodeT<2, BranchIfUint32Compare>;

 public:
  static constexpr int kLeftIndex = 0;
  static constexpr int kRightIndex = 1;
  Input left_input() { return NodeBase::input(kLeftIndex); }
  Input right_input() { return NodeBase::input(kRightIndex); }

  explicit BranchIfUint32Compare(uint64_t bitfield, Operation operation,
                                 BasicBlockRef* if_true_refs,
                                 BasicBlockRef* if_false_refs)
      : Base(bitfield, if_true_refs, if_false_refs), operation_(operation) {}

  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kUint32, ValueRepresentation::kUint32};

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

  Operation operation() const { return operation_; }

 private:
  Operation operation_;
};

class BranchIfFloat64Compare
    : public BranchControlNodeT<2, BranchIfFloat64Compare> {
  using Base = BranchControlNodeT<2, BranchIfFloat64Compare>;

 public:
  static constexpr int kLeftIndex = 0;
  static constexpr int kRightIndex = 1;
  Input left_input() { return NodeBase::input(kLeftIndex); }
  Input right_input() { return NodeBase::input(kRightIndex); }

  explicit BranchIfFloat64Compare(uint64_t bitfield, Operation operation,
                                  BasicBlockRef* if_true_refs,
                                  BasicBlockRef* if_false_refs)
      : Base(bitfield, if_true_refs, if_false_refs), operation_(operation) {}

  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kFloat64, ValueRepresentation::kFloat64};

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

  Operation operation() const { return operation_; }

 private:
  Operation operation_;
};

class BranchIfReferenceEqual
    : public BranchControlNodeT<2, BranchIfReferenceEqual> {
  using Base = BranchControlNodeT<2, BranchIfReferenceEqual>;

 public:
  static constexpr int kLeftIndex = 0;
  static constexpr int kRightIndex = 1;
  Input left_input() { return NodeBase::input(kLeftIndex); }
  Input right_input() { return NodeBase::input(kRightIndex); }

  explicit BranchIfReferenceEqual(uint64_t bitfield,
                                  BasicBlockRef* if_true_refs,
                                  BasicBlockRef* if_false_refs)
      : Base(bitfield, if_true_refs, if_false_refs) {}

  static constexpr typename Base::InputTypes kInputTypes{
      ValueRepresentation::kTagged, ValueRepresentation::kTagged};

#ifdef V8_COMPRESS_POINTERS
  void MarkTaggedInputsAsDecompressing() {
    // Don't need to decompress values to reference compare.
  }
#endif

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const {}
};

class BranchIfTypeOf : public BranchControlNodeT<1, BranchIfTypeOf> {
  using Base = BranchControlNodeT<1, BranchIfTypeOf>;

 public:
  static constexpr int kValueIndex = 0;
  Input value_input() { return NodeBase::input(kValueIndex); }

  explicit BranchIfTypeOf(uint64_t bitfield,
                          interpreter::TestTypeOfFlags::LiteralFlag literal,
                          BasicBlockRef* if_true_refs,
                          BasicBlockRef* if_false_refs)
      : Base(bitfield, if_true_refs, if_false_refs), literal_(literal) {}

  static constexpr
      typename Base::InputTypes kInputTypes{ValueRepresentation::kTagged};

  void SetValueLocationConstraints();
  void GenerateCode(MaglevAssembler*, const ProcessingState&);
  void PrintParams(std::ostream&) const;

 private:
  interpreter::TestTypeOfFlags::LiteralFlag literal_;
};

constexpr inline OpProperties StaticPropertiesForOpcode(Opcode opcode) {
  switch (opcode) {
#define CASE(op)      \
  case Opcode::k##op: \
    return op::kProperties;
    NODE_BASE_LIST(CASE)
#undef CASE
  }
}

template <typename Function>
inline void NodeBase::ForAllInputsInRegallocAssignmentOrder(Function&& f) {
  auto iterate_inputs = [&](InputAllocationPolicy category) {
    for (Input input : inputs()) {
      switch (compiler::UnallocatedOperand::cast(input.operand())
                  .extended_policy()) {
        case compiler::UnallocatedOperand::MUST_HAVE_REGISTER:
          if (category == InputAllocationPolicy::kArbitraryRegister)
            f(category, input);
          break;

        case compiler::UnallocatedOperand::REGISTER_OR_SLOT_OR_CONSTANT:
          if (category == InputAllocationPolicy::kAny) f(category, input);
          break;

        case compiler::UnallocatedOperand::FIXED_REGISTER:
        case compiler::UnallocatedOperand::FIXED_FP_REGISTER:
          if (category == InputAllocationPolicy::kFixedRegister)
            f(category, input);
          break;

        case compiler::UnallocatedOperand::REGISTER_OR_SLOT:
        case compiler::UnallocatedOperand::SAME_AS_INPUT:
        case compiler::UnallocatedOperand::NONE:
        case compiler::UnallocatedOperand::MUST_HAVE_SLOT:
          UNREACHABLE();
      }
    }
  };

  iterate_inputs(InputAllocationPolicy::kFixedRegister);
  iterate_inputs(InputAllocationPolicy::kArbitraryRegister);
  iterate_inputs(InputAllocationPolicy::kAny);
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_IR_H_
