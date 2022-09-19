// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils-inl.h"
#include "src/objects/module-inl.h"
#include "src/objects/objects-inl.h"

namespace v8 {
namespace internal {

BUILTIN(CallAsyncModuleFulfilled) {
  HandleScope handle_scope(isolate);
  Handle<SourceTextModule> module(args.at<SourceTextModule>(0));
  if (SourceTextModule::AsyncModuleExecutionFulfilled(isolate, module)
          .IsNothing()) {
    // The evaluation of async module can not throwing a JavaScript observable
    // exception.
    DCHECK(isolate->has_pending_exception());
    DCHECK_EQ(isolate->pending_exception(),
              ReadOnlyRoots(isolate).termination_exception());
    return ReadOnlyRoots(isolate).exception();
  }
  return ReadOnlyRoots(isolate).undefined_value();
}

BUILTIN(CallAsyncModuleRejected) {
  HandleScope handle_scope(isolate);

  // Arguments should be a SourceTextModule and an exception object.
  DCHECK_EQ(args.length(), 2);
  Handle<SourceTextModule> module(args.at<SourceTextModule>(0));
  Handle<Object> exception(args.at(1));
  SourceTextModule::AsyncModuleExecutionRejected(isolate, module, exception);
  return ReadOnlyRoots(isolate).undefined_value();
}

}  // namespace internal
}  // namespace v8
