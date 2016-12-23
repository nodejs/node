// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_DEOPTIMIZE_REASON_H_
#define V8_DEOPTIMIZE_REASON_H_

#include "src/globals.h"

namespace v8 {
namespace internal {

#define DEOPTIMIZE_REASON_LIST(V)                                             \
  V(AccessCheck, "Access check needed")                                       \
  V(NoReason, "no reason")                                                    \
  V(ConstantGlobalVariableAssignment, "Constant global variable assignment")  \
  V(ConversionOverflow, "conversion overflow")                                \
  V(DivisionByZero, "division by zero")                                       \
  V(ElementsKindUnhandledInKeyedLoadGenericStub,                              \
    "ElementsKind unhandled in KeyedLoadGenericStub")                         \
  V(ExpectedHeapNumber, "Expected heap number")                               \
  V(ExpectedSmi, "Expected smi")                                              \
  V(ForcedDeoptToRuntime, "Forced deopt to runtime")                          \
  V(Hole, "hole")                                                             \
  V(InstanceMigrationFailed, "instance migration failed")                     \
  V(InsufficientTypeFeedbackForCall, "Insufficient type feedback for call")   \
  V(InsufficientTypeFeedbackForCallWithArguments,                             \
    "Insufficient type feedback for call with arguments")                     \
  V(FastPathFailed, "Falling off the fast path")                              \
  V(InsufficientTypeFeedbackForCombinedTypeOfBinaryOperation,                 \
    "Insufficient type feedback for combined type of binary operation")       \
  V(InsufficientTypeFeedbackForGenericNamedAccess,                            \
    "Insufficient type feedback for generic named access")                    \
  V(InsufficientTypeFeedbackForGenericKeyedAccess,                            \
    "Insufficient type feedback for generic keyed access")                    \
  V(InsufficientTypeFeedbackForLHSOfBinaryOperation,                          \
    "Insufficient type feedback for LHS of binary operation")                 \
  V(InsufficientTypeFeedbackForRHSOfBinaryOperation,                          \
    "Insufficient type feedback for RHS of binary operation")                 \
  V(KeyIsNegative, "key is negative")                                         \
  V(LostPrecision, "lost precision")                                          \
  V(LostPrecisionOrNaN, "lost precision or NaN")                              \
  V(MementoFound, "memento found")                                            \
  V(MinusZero, "minus zero")                                                  \
  V(NaN, "NaN")                                                               \
  V(NegativeKeyEncountered, "Negative key encountered")                       \
  V(NegativeValue, "negative value")                                          \
  V(NoCache, "no cache")                                                      \
  V(NonStrictElementsInKeyedLoadGenericStub,                                  \
    "non-strict elements in KeyedLoadGenericStub")                            \
  V(NotAHeapNumber, "not a heap number")                                      \
  V(NotAHeapNumberUndefinedBoolean, "not a heap number/undefined/true/false") \
  V(NotAHeapNumberUndefined, "not a heap number/undefined")                   \
  V(NotAJavaScriptObject, "not a JavaScript object")                          \
  V(NotASmi, "not a Smi")                                                     \
  V(OutOfBounds, "out of bounds")                                             \
  V(OutsideOfRange, "Outside of range")                                       \
  V(Overflow, "overflow")                                                     \
  V(Proxy, "proxy")                                                           \
  V(ReceiverWasAGlobalObject, "receiver was a global object")                 \
  V(Smi, "Smi")                                                               \
  V(TooManyArguments, "too many arguments")                                   \
  V(TracingElementsTransitions, "Tracing elements transitions")               \
  V(TypeMismatchBetweenFeedbackAndConstant,                                   \
    "Type mismatch between feedback and constant")                            \
  V(UnexpectedCellContentsInConstantGlobalStore,                              \
    "Unexpected cell contents in constant global store")                      \
  V(UnexpectedCellContentsInGlobalStore,                                      \
    "Unexpected cell contents in global store")                               \
  V(UnexpectedObject, "unexpected object")                                    \
  V(UnexpectedRHSOfBinaryOperation, "Unexpected RHS of binary operation")     \
  V(UnknownMapInPolymorphicAccess, "Unknown map in polymorphic access")       \
  V(UnknownMapInPolymorphicCall, "Unknown map in polymorphic call")           \
  V(UnknownMapInPolymorphicElementAccess,                                     \
    "Unknown map in polymorphic element access")                              \
  V(UnknownMap, "Unknown map")                                                \
  V(ValueMismatch, "value mismatch")                                          \
  V(WrongInstanceType, "wrong instance type")                                 \
  V(WrongMap, "wrong map")                                                    \
  V(UndefinedOrNullInForIn, "null or undefined in for-in")                    \
  V(UndefinedOrNullInToObject, "null or undefined in ToObject")

enum class DeoptimizeReason : uint8_t {
#define DEOPTIMIZE_REASON(Name, message) k##Name,
  DEOPTIMIZE_REASON_LIST(DEOPTIMIZE_REASON)
#undef DEOPTIMIZE_REASON
};

std::ostream& operator<<(std::ostream&, DeoptimizeReason);

size_t hash_value(DeoptimizeReason reason);

char const* DeoptimizeReasonToString(DeoptimizeReason reason);

}  // namespace internal
}  // namespace v8

#endif  // V8_DEOPTIMIZE_REASON_H_
