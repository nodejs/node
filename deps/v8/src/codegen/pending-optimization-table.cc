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

enum class FunctionStatus : int {
  kPrepareForOptimize = 1 << 0,
  kMarkForOptimize = 1 << 1,
  kAllowHeuristicOptimization = 1 << 2,
};

using FunctionStatusFlags = base::Flags<FunctionStatus>;

void PendingOptimizationTable::PreparedForOptimization(
    Isolate* isolate, Handle<JSFunction> function,
    bool allow_heuristic_optimization) {
  DCHECK(FLAG_testing_d8_test_runner);

  FunctionStatusFlags status = FunctionStatus::kPrepareForOptimize;
  if (allow_heuristic_optimization) {
    status |= FunctionStatus::kAllowHeuristicOptimization;
  }

  Handle<ObjectHashTable> table =
      isolate->heap()->pending_optimize_for_test_bytecode().IsUndefined()
          ? ObjectHashTable::New(isolate, 1)
          : handle(ObjectHashTable::cast(
                       isolate->heap()->pending_optimize_for_test_bytecode()),
                   isolate);
  Handle<Tuple2> tuple = isolate->factory()->NewTuple2(
      handle(function->shared().GetBytecodeArray(isolate), isolate),
      handle(Smi::FromInt(status), isolate), AllocationType::kYoung);
  table =
      ObjectHashTable::Put(table, handle(function->shared(), isolate), tuple);
  isolate->heap()->SetPendingOptimizeForTestBytecode(*table);
}

bool PendingOptimizationTable::IsHeuristicOptimizationAllowed(
    Isolate* isolate, JSFunction function) {
  DCHECK(FLAG_testing_d8_test_runner);

  Handle<Object> table =
      handle(isolate->heap()->pending_optimize_for_test_bytecode(), isolate);
  Handle<Object> entry =
      table->IsUndefined()
          ? handle(ReadOnlyRoots(isolate).the_hole_value(), isolate)
          : handle(Handle<ObjectHashTable>::cast(table)->Lookup(
                       handle(function.shared(), isolate)),
                   isolate);
  if (entry->IsTheHole()) {
    return true;
  }
  DCHECK(entry->IsTuple2());
  DCHECK(Handle<Tuple2>::cast(entry)->value2().IsSmi());
  FunctionStatusFlags status(Smi::ToInt(Handle<Tuple2>::cast(entry)->value2()));
  return status & FunctionStatus::kAllowHeuristicOptimization;
}

void PendingOptimizationTable::MarkedForOptimization(
    Isolate* isolate, Handle<JSFunction> function) {
  DCHECK(FLAG_testing_d8_test_runner);

  Handle<Object> table =
      handle(isolate->heap()->pending_optimize_for_test_bytecode(), isolate);
  Handle<Object> entry =
      table->IsUndefined()
          ? handle(ReadOnlyRoots(isolate).the_hole_value(), isolate)
          : handle(Handle<ObjectHashTable>::cast(table)->Lookup(
                       handle(function->shared(), isolate)),
                   isolate);
  if (entry->IsTheHole()) {
    PrintF("Error: Function ");
    function->ShortPrint();
    PrintF(
        " should be prepared for optimization with "
        "%%PrepareFunctionForOptimization before  "
        "%%OptimizeFunctionOnNextCall / %%OptimizeOSR ");
    UNREACHABLE();
  }

  DCHECK(entry->IsTuple2());
  DCHECK(Handle<Tuple2>::cast(entry)->value2().IsSmi());
  FunctionStatusFlags status(Smi::ToInt(Handle<Tuple2>::cast(entry)->value2()));
  status = status.without(FunctionStatus::kPrepareForOptimize) |
           FunctionStatus::kMarkForOptimize;
  Handle<Tuple2>::cast(entry)->set_value2(Smi::FromInt(status));
  table = ObjectHashTable::Put(Handle<ObjectHashTable>::cast(table),
                               handle(function->shared(), isolate), entry);
  isolate->heap()->SetPendingOptimizeForTestBytecode(*table);
}

void PendingOptimizationTable::FunctionWasOptimized(
    Isolate* isolate, Handle<JSFunction> function) {
  DCHECK(FLAG_testing_d8_test_runner);

  if (isolate->heap()->pending_optimize_for_test_bytecode().IsUndefined()) {
    return;
  }

  Handle<ObjectHashTable> table =
      handle(ObjectHashTable::cast(
                 isolate->heap()->pending_optimize_for_test_bytecode()),
             isolate);
  Handle<Object> value(table->Lookup(handle(function->shared(), isolate)),
                       isolate);
  // Remove only if we have already seen %OptimizeFunctionOnNextCall. If it is
  // optimized for other reasons, still keep holding the bytecode since we may
  // optimize it later.
  if (!value->IsTheHole() &&
      Smi::cast(Handle<Tuple2>::cast(value)->value2()).value() ==
          static_cast<int>(FunctionStatus::kMarkForOptimize)) {
    bool was_present;
    table = table->Remove(isolate, table, handle(function->shared(), isolate),
                          &was_present);
    DCHECK(was_present);
    isolate->heap()->SetPendingOptimizeForTestBytecode(*table);
  }
}

}  // namespace internal
}  // namespace v8
