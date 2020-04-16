// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/finalization-group-cleanup-task.h"

#include "src/execution/frames.h"
#include "src/execution/interrupts-scope.h"
#include "src/execution/stack-guard.h"
#include "src/execution/v8threads.h"
#include "src/heap/heap-inl.h"
#include "src/objects/js-weak-refs-inl.h"
#include "src/tracing/trace-event.h"

namespace v8 {
namespace internal {

FinalizationGroupCleanupTask::FinalizationGroupCleanupTask(Heap* heap)
    : heap_(heap) {}

void FinalizationGroupCleanupTask::SlowAssertNoActiveJavaScript() {
#ifdef ENABLE_SLOW_DCHECKS
  class NoActiveJavaScript : public ThreadVisitor {
   public:
    void VisitThread(Isolate* isolate, ThreadLocalTop* top) override {
      for (StackFrameIterator it(isolate, top); !it.done(); it.Advance()) {
        DCHECK(!it.frame()->is_java_script());
      }
    }
  };
  NoActiveJavaScript no_active_js_visitor;
  Isolate* isolate = heap_->isolate();
  no_active_js_visitor.VisitThread(isolate, isolate->thread_local_top());
  isolate->thread_manager()->IterateArchivedThreads(&no_active_js_visitor);
#endif  // ENABLE_SLOW_DCHECKS
}

void FinalizationGroupCleanupTask::Run() {
  Isolate* isolate = heap_->isolate();
  DCHECK(!isolate->host_cleanup_finalization_group_callback());
  SlowAssertNoActiveJavaScript();

  TRACE_EVENT_CALL_STATS_SCOPED(isolate, "v8",
                                "V8.FinalizationGroupCleanupTask");

  HandleScope handle_scope(isolate);
  Handle<JSFinalizationGroup> finalization_group;
  // There could be no dirty FinalizationGroups. When a context is disposed by
  // the embedder, its FinalizationGroups are removed from the dirty list.
  if (!heap_->TakeOneDirtyJSFinalizationGroup().ToHandle(&finalization_group)) {
    return;
  }
  finalization_group->set_scheduled_for_cleanup(false);

  // Since FinalizationGroup cleanup callbacks are scheduled by V8, enter the
  // FinalizationGroup's context.
  Handle<Context> context(Context::cast(finalization_group->native_context()),
                          isolate);
  Handle<Object> callback(finalization_group->cleanup(), isolate);
  v8::Context::Scope context_scope(v8::Utils::ToLocal(context));
  v8::TryCatch catcher(reinterpret_cast<v8::Isolate*>(isolate));
  catcher.SetVerbose(true);

  // Exceptions are reported via the message handler. This is ensured by the
  // verbose TryCatch.
  InvokeFinalizationGroupCleanupFromTask(context, finalization_group, callback);

  // Repost if there are remaining dirty FinalizationGroups.
  heap_->set_is_finalization_group_cleanup_task_posted(false);
  heap_->PostFinalizationGroupCleanupTaskIfNeeded();
}

}  // namespace internal
}  // namespace v8
