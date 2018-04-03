// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_DEOPTIMIZE_REASON_H_
#define V8_DEOPTIMIZE_REASON_H_

#include "src/globals.h"

namespace v8 {
namespace internal {

#define DEOPTIMIZE_REASON_LIST(V)                                              \
  V(ArrayBufferWasNeutered, "array buffer was neutered")                       \
  V(CowArrayElementsChanged, "copy-on-write array's elements changed")         \
  V(CouldNotGrowElements, "failed to grow elements store")                     \
  V(DeoptimizeNow, "%_DeoptimizeNow")                                          \
  V(DivisionByZero, "division by zero")                                        \
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
  V(NotAHeapNumber, "not a heap number")                                       \
  V(NotAJavaScriptObject, "not a JavaScript object")                           \
  V(NotANumberOrOddball, "not a Number or Oddball")                            \
  V(NotASmi, "not a Smi")                                                      \
  V(NotASymbol, "not a Symbol")                                                \
  V(OutOfBounds, "out of bounds")                                              \
  V(Overflow, "overflow")                                                      \
  V(ReceiverNotAGlobalProxy, "receiver was not a global proxy")                \
  V(Smi, "Smi")                                                                \
  V(Unknown, "(unknown)")                                                      \
  V(ValueMismatch, "value mismatch")                                           \
  V(WrongCallTarget, "wrong call target")                                      \
  V(WrongEnumIndices, "wrong enum indices")                                    \
  V(WrongInstanceType, "wrong instance type")                                  \
  V(WrongMap, "wrong map")                                                     \
  V(WrongName, "wrong name")                                                   \
  V(WrongValue, "wrong value")

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
