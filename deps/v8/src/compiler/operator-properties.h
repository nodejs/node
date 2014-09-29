// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_OPERATOR_PROPERTIES_H_
#define V8_COMPILER_OPERATOR_PROPERTIES_H_

#include "src/v8.h"

namespace v8 {
namespace internal {
namespace compiler {

class Operator;

class OperatorProperties {
 public:
  static inline bool HasValueInput(Operator* node);
  static inline bool HasContextInput(Operator* node);
  static inline bool HasEffectInput(Operator* node);
  static inline bool HasControlInput(Operator* node);

  static inline int GetValueInputCount(Operator* op);
  static inline int GetContextInputCount(Operator* op);
  static inline int GetEffectInputCount(Operator* op);
  static inline int GetControlInputCount(Operator* op);
  static inline int GetTotalInputCount(Operator* op);

  static inline bool HasValueOutput(Operator* op);
  static inline bool HasEffectOutput(Operator* op);
  static inline bool HasControlOutput(Operator* op);

  static inline int GetValueOutputCount(Operator* op);
  static inline int GetEffectOutputCount(Operator* op);
  static inline int GetControlOutputCount(Operator* op);

  static inline bool IsBasicBlockBegin(Operator* op);

  static inline bool CanBeScheduled(Operator* op);
  static inline bool HasFixedSchedulePosition(Operator* op);
  static inline bool IsScheduleRoot(Operator* op);

  static inline bool CanLazilyDeoptimize(Operator* op);
};
}
}
}  // namespace v8::internal::compiler

#endif  // V8_COMPILER_OPERATOR_PROPERTIES_H_
