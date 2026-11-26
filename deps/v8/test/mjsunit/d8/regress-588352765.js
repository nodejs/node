// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function workerCode() {
  Object.defineProperty(globalThis, 'onmessage', {
    get() {
      // This getter is called right before the worker enters its message
      // loop. The first task in that loop is triggered by the child's message
      // and posts the task which calls {child.onmessage}. To reproduce the
      // original failure, that task has to be the next one after the
      // terminating microtask checkpoint. Hence keep the child alive (via its
      // 'onmessage' handler), such that it does not post any other task.
      const child = new Worker(() => {
        onmessage = () => {};
        postMessage(0);
      }, {type: 'function'});
      // Not expected to be called, as this worker terminates before.
      child.onmessage = () => postMessage('processed message from child');
      // Schedule a termination which is only handled by the stack check of
      // the microtask, i.e. in the microtask checkpoint after that first task.
      Promise.resolve().then(() => {});
      d8.terminate();
      return () => {};
    }
  });
}

const worker = new Worker(workerCode, {type: 'function'});
// Wait for the worker to terminate without processing the child's message.
assertEquals(undefined, worker.getMessage());
