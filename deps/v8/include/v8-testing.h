// Copyright 2010 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_V8_TEST_H_
#define V8_V8_TEST_H_

#include "v8.h"  // NOLINT(build/include)

/**
 * Testing support for the V8 JavaScript engine.
 */
namespace v8 {
class V8_EXPORT Testing {
 public:
  enum V8_DEPRECATED("Don't use this (d8-specific testing logic).") StressType {
    kStressTypeOpt,
    kStressTypeDeopt
  };

  /**
   * Set the type of stressing to do. The default if not set is kStressTypeOpt.
   */
  V8_DEPRECATED("Don't use this (d8-specific testing logic).")
  static void SetStressRunType(StressType type);

  /**
   * Get the number of runs of a given test that is required to get the full
   * stress coverage.
   */
  V8_DEPRECATED("Don't use this (d8-specific testing logic).")
  static int GetStressRuns();

  /**
   * Indicate the number of the run which is about to start. The value of run
   * should be between 0 and one less than the result from GetStressRuns()
   */
  V8_DEPRECATED("Don't use this (d8-specific testing logic).")
  static void PrepareStressRun(int run);

  /**
   * Force deoptimization of all functions.
   */
  V8_DEPRECATED("Don't use this (d8-specific testing logic).")
  static void DeoptimizeAll(Isolate* isolate);
};

}  // namespace v8

#endif  // V8_V8_TEST_H_
