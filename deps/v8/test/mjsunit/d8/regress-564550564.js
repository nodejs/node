// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --bundle

// JS_BUNDLE_MODULE:bundled.mjs
export const value = 42;

// JS_BUNDLE_SCRIPT
const worker = new Worker(function() {
  onmessage = function() {
    import('bundled.mjs').then(
        m => postMessage(m.value), e => postMessage(String(e)));
  };
}, {type: 'function'});

const worker_result = new Promise(resolve => {
  worker.onmessage = function(event) {
    worker.terminate();
    resolve(event.data);
  };
});
assertPromiseResult(worker_result, value => assertEquals(42, value));

// Post from a task, so that the worker imports the bundled module only after
// the bundle's top-level code has finished.
setTimeout(() => worker.postMessage('import'), 0);
