// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_PENDING_OPTIMIZATION_TABLE_H_
#define V8_CODEGEN_PENDING_OPTIMIZATION_TABLE_H_

#include "src/common/globals.h"

namespace v8 {
namespace internal {

// This class adds the functionality to properly test the optimized code. This
// is only for use in tests. All these functions should only be called when
// testing_d8_flag_for_tests is set.
class PendingOptimizationTable {
 public:
  // This function should be called before we mark the function for
  // optimization. Calling this function ensures that |function| is compiled and
  // has a feedback vector allocated. This also holds on to the bytecode
  // strongly in pending optimization table preventing the bytecode to be
  // flushed.
  static void PreparedForOptimization(Isolate* isolate,
                                      Handle<JSFunction> function);

  // This function should be called when the function is marked for optimization
  // via the intrinsics. This will update the state of the bytecode array in the
  // pending optimization table, so that the entry can be removed once the
  // function is optimized. If the function is already optimized it removes the
  // entry from the table.
  static void MarkedForOptimization(Isolate* isolate,
                                    Handle<JSFunction> function);

  // This function should be called once the function is optimized. If there is
  // an entry in the pending optimization table and it is marked for removal
  // then this function removes the entry from pending optimization table.
  static void FunctionWasOptimized(Isolate* isolate,
                                   Handle<JSFunction> function);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_PENDING_OPTIMIZATION_TABLE_H_
