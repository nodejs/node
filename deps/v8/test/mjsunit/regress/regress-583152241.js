// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-async-hooks --strict-termination-checks

if (typeof Worker !== 'undefined') {
  // 1. ResolveAsyncWaiterPromises entered while execution is already
  // terminating.
  const w1 = new Worker(`
    let first = true;
    onmessage = () => {
      if (first) {
        first = false;
        postMessage('ready');
        return;
      }
      const ia = new Int32Array(new SharedArrayBuffer(8));
      Atomics.waitAsync(ia, 1, 0).value.then(() => {
        Atomics.waitAsync(ia, 0, 0, 1).value.then(() => {
          postMessage('done');
        });
        Atomics.notify(ia, 0);
        d8.terminateNow();
      });
      Atomics.notify(ia, 1);
    };
  `, {type: 'string'});
  w1.postMessage(0);
  assertEquals('ready', w1.getMessage());
  w1.postMessage(0);
  w1.getMessage();
  w1.terminate();

  // 2. Termination triggered by promiseResolve hook on the first waiter node
  // while a second waiter node with a timeout task is queued in the same
  // ResolveAsyncWaiterPromises batch.
  const w2 = new Worker(`
    const ia = new Int32Array(new SharedArrayBuffer(4));
    async_hooks.createHook({
      promiseResolve() {
        d8.terminateNow();
      }
    }).enable();
    Atomics.waitAsync(ia, 0, 0, 1);
    Atomics.waitAsync(ia, 0, 0, 1);
    Atomics.notify(ia, 0);
  `, {type: 'string'});
  w2.getMessage();

  // 3. HandleAsyncWaiterTimeout entered while execution is already terminating.
  const w3 = new Worker(`
    let first = true;
    onmessage = () => {
      if (first) {
        first = false;
        postMessage('ready');
        return;
      }
      const ia = new Int32Array(new SharedArrayBuffer(8));
      Atomics.waitAsync(ia, 1, 0).value.then(() => {
        Atomics.waitAsync(ia, 0, 0, 1).value.then(() => {
          postMessage('done');
        });
        Atomics.wait(ia, 1, 0, 5);
        d8.terminateNow();
      });
      Atomics.notify(ia, 1);
    };
  `, {type: 'string'});
  w3.postMessage(0);
  assertEquals('ready', w3.getMessage());
  w3.postMessage(0);
  w3.getMessage();
  w3.terminate();
}
