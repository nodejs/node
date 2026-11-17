// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-defer-import-eval

// Dynamic import() starts evaluating the top-level-await module so it enters
// kEvaluatingAsync. In the same microtask checkpoint, the d8.terminate
// reaction requests termination right before the import.defer() task calls
// Module::EvaluateForImportDefer -> JSPromise::PerformPromiseAll, where the
// perform_promise_then entry stack check throws the termination exception.
const url = 'data:text/javascript,await 0';
import(url);
Promise.resolve().then(d8.terminate);
import.defer(url);
