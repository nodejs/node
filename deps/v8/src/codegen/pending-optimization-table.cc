// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/codegen/pending-optimization-table.h"

#include "src/base/flags.h"
#include "src/execution/isolate-inl.h"
#include "src/heap/heap-inl.h"
#include "src/objects/hash-table.h"
#include "src/objects/js-objects.h"

namespace v8 {
namespace internal {

void ManualOptimizationTable::MarkFunctionForManualOptimization(
    Isolate* isolate, Handle<JSFunction> function,
    IsCompiledScope* is_compiled_scope) {
  DCHECK(v8_flags.testing_d8_test_runner);
  DCHECK(is_compiled_scope->is_compiled());
  DCHECK(function->has_feedback_vector());

  Handle<SharedFunctionInfo> shared_info(function->shared(), isolate);

  Handle<ObjectHashTable> table =
      isolate->heap()->functions_marked_for_manual_optimization().IsUndefined()
          ? ObjectHashTable::New(isolate, 1)
          : handle(ObjectHashTable::cast(
                       isolate->heap()
                           ->functions_marked_for_manual_optimization()),
                   isolate);
  table = ObjectHashTable::Put(
      table, shared_info,
      handle(shared_info->GetBytecodeArray(isolate), isolate));
  isolate->heap()->SetFunctionsMarkedForManualOptimization(*table);
}

void ManualOptimizationTable::CheckMarkedForManualOptimization(
    Isolate* isolate, JSFunction function) {
  if (!IsMarkedForManualOptimization(isolate, function)) {
    PrintF("Error: Function ");
    function.ShortPrint();
    PrintF(
        " should be prepared for optimization with "
        "%%PrepareFunctionForOptimization before  "
        "%%OptimizeFunctionOnNextCall / %%OptimizeOSR ");
    UNREACHABLE();
  }
}

bool ManualOptimizationTable::IsMarkedForManualOptimization(
    Isolate* isolate, JSFunction function) {
  DCHECK(v8_flags.testing_d8_test_runner);

  Handle<Object> table = handle(
      isolate->heap()->functions_marked_for_manual_optimization(), isolate);
  Handle<Object> entry =
      table->IsUndefined()
          ? handle(ReadOnlyRoots(isolate).the_hole_value(), isolate)
          : handle(Handle<ObjectHashTable>::cast(table)->Lookup(
                       handle(function.shared(), isolate)),
                   isolate);
  return !entry->IsTheHole();
}

}  // namespace internal
}  // namespace v8
