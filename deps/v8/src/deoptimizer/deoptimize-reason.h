// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_DEOPTIMIZER_DEOPTIMIZE_REASON_H_
#define V8_DEOPTIMIZER_DEOPTIMIZE_REASON_H_

#include "src/common/globals.h"

namespace v8 {
namespace internal {

#define DEOPTIMIZE_REASON_LIST(V)                                              \
  V(ArrayBufferWasDetached, "array buffer was detached")                       \
  V(BigIntTooBig, "BigInt too big")                                            \
  V(CowArrayElementsChanged, "copy-on-write array's elements changed")         \
  V(CouldNotGrowElements, "failed to grow elements store")                     \
  V(DeoptimizeNow, "%_DeoptimizeNow")                                          \
  V(DivisionByZero, "division by zero")                                        \
  V(DynamicCheckMaps, "dynamic check maps failed")                             \
  V(Hole, "hole")                                                              \
  V(InstanceMigrationFailed, "instance migration failed")                      \
  V(InsufficientTypeFeedbackForCall, "Insufficient type feedback for call")    \
  V(InsufficientTypeFeedbackForConstruct,                                      \
    "Insufficient type feedback for construct")                                \
  V(InsufficientTypeFeedbackForForIn, "Insufficient type feedback for for-in") \
  V(InsufficientTypeFeedbackForBinaryOperation,                                \
    "Insufficient type feedback for binary operation")                         \
  V(InsufficientTypeFeedbackForCompareOperation,                               \
    "Insufficient type feedback for compare operation")                        \
  V(InsufficientTypeFeedbackForGenericNamedAccess,                             \
    "Insufficient type feedback for generic named access")                     \
  V(InsufficientTypeFeedbackForGenericKeyedAccess,                             \
    "Insufficient type feedback for generic keyed access")                     \
  V(InsufficientTypeFeedbackForUnaryOperation,                                 \
    "Insufficient type feedback for unary operation")                          \
  V(LostPrecision, "lost precision")                                           \
  V(LostPrecisionOrNaN, "lost precision or NaN")                               \
  V(MinusZero, "minus zero")                                                   \
  V(NaN, "NaN")                                                                \
  V(NoCache, "no cache")                                                       \
  V(NotABigInt, "not a BigInt")                                                \
  V(NotAHeapNumber, "not a heap number")                                       \
  V(NotAJavaScriptObject, "not a JavaScript object")                           \
  V(NotAJavaScriptObjectOrNullOrUndefined,                                     \
    "not a JavaScript object, Null or Undefined")                              \
  V(NotANumberOrBoolean, "not a Number or Boolean")                            \
  V(NotANumberOrOddball, "not a Number or Oddball")                            \
  V(NotAnArrayIndex, "not an array index")                                     \
  V(NotASmi, "not a Smi")                                                      \
  V(NotAString, "not a String")                                                \
  V(NotASymbol, "not a Symbol")                                                \
  V(NotInt32, "not int32")                                                     \
  V(OutOfBounds, "out of bounds")                                              \
  V(Overflow, "overflow")                                                      \
  V(Smi, "Smi")                                                                \
  V(TransitionedToMonomorphicIC, "IC transitioned to monomorphic")             \
  V(TransitionedToMegamorphicIC, "IC transitioned to megamorphic")             \
  V(Unknown, "(unknown)")                                                      \
  V(ValueMismatch, "value mismatch")                                           \
  V(WrongCallTarget, "wrong call target")                                      \
  V(WrongEnumIndices, "wrong enum indices")                                    \
  V(WrongFeedbackCell, "wrong feedback cell")                                  \
  V(WrongInstanceType, "wrong instance type")                                  \
  V(WrongMap, "wrong map")                                                     \
  V(MissingMap, "missing map")                                                 \
  V(DeprecatedMap, "deprecated map")                                           \
  V(WrongHandler, "wrong handler")                                             \
  V(WrongName, "wrong name")                                                   \
  V(WrongValue, "wrong value")                                                 \
  V(NoInitialElement, "no initial element")

enum class DeoptimizeReason : uint8_t {
#define DEOPTIMIZE_REASON(Name, message) k##Name,
  DEOPTIMIZE_REASON_LIST(DEOPTIMIZE_REASON)
#undef DEOPTIMIZE_REASON
};

V8_EXPORT_PRIVATE std::ostream& operator<<(std::ostream&, DeoptimizeReason);

size_t hash_value(DeoptimizeReason reason);

V8_EXPORT_PRIVATE char const* DeoptimizeReasonToString(DeoptimizeReason reason);

}  // namespace internal
}  // namespace v8

#endif  // V8_DEOPTIMIZER_DEOPTIMIZE_REASON_H_
