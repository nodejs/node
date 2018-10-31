// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/v8.h"
#include "src/api.h"
#include "src/arguments-inl.h"
#include "src/counters.h"
#include "src/execution.h"
#include "src/handles-inl.h"
#include "src/objects-inl.h"
#include "src/objects/js-weak-refs.h"
#include "src/runtime/runtime-utils.h"

namespace v8 {
namespace internal {

RUNTIME_FUNCTION(Runtime_WeakFactoryCleanupJob) {
  HandleScope scope(isolate);
  CONVERT_ARG_HANDLE_CHECKED(JSWeakFactory, weak_factory, 0);
  weak_factory->set_scheduled_for_cleanup(false);

  // It's possible that the cleared_cells list is empty, since
  // WeakCell.clear() was called on all its elements before this task ran. In
  // that case, don't call the cleanup function.
  if (!weak_factory->cleared_cells()->IsUndefined(isolate)) {
    // Construct the iterator.
    Handle<JSWeakFactoryCleanupIterator> iterator;
    {
      Handle<Map> cleanup_iterator_map(
          isolate->native_context()->js_weak_factory_cleanup_iterator_map(),
          isolate);
      iterator = Handle<JSWeakFactoryCleanupIterator>::cast(
          isolate->factory()->NewJSObjectFromMap(
              cleanup_iterator_map, NOT_TENURED,
              Handle<AllocationSite>::null()));
      iterator->set_factory(*weak_factory);
    }
    Handle<Object> cleanup(weak_factory->cleanup(), isolate);

    v8::TryCatch try_catch(reinterpret_cast<v8::Isolate*>(isolate));
    v8::Local<v8::Value> result;
    MaybeHandle<Object> exception;
    Handle<Object> args[] = {iterator};
    bool has_pending_exception = !ToLocal<Value>(
        Execution::TryCall(
            isolate, cleanup,
            handle(ReadOnlyRoots(isolate).undefined_value(), isolate), 1, args,
            Execution::MessageHandling::kReport, &exception,
            Execution::Target::kCallable),
        &result);
    // TODO(marja): (spec): What if there's an exception?
    USE(has_pending_exception);

    // TODO(marja): (spec): Should the iterator be invalidated after the
    // function returns?
  }
  return ReadOnlyRoots(isolate).undefined_value();
}

}  // namespace internal
}  // namespace v8
