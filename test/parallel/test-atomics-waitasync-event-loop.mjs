import { Worker } from 'node:worker_threads';
import assert from 'node:assert';

function makeInt32(initialValue) {
    const sab = new SharedArrayBuffer(4);
    const view = new Int32Array(sab, 0, 1);
    Atomics.store(view, 0, initialValue);
    return view;
}

{
    const view = makeInt32(0);
    assert.strictEqual(
        Object.prototype.hasOwnProperty.call(Atomics.waitAsync, 'prototype'),
        false);
    assert.throws(() => new Atomics.waitAsync(view, 0, 0), {
        name: 'TypeError',
    });
}

// No timeout, notified by a worker thread
// Must not exit early and the promise must resolve with 'ok'
{
    const view = makeInt32(0);

    const workerCode = `
        import { workerData, parentPort } from 'node:worker_threads';
        const view = new Int32Array(workerData, 0, 1);
        setTimeout(() => {
            Atomics.store(view, 0, 1);
            Atomics.notify(view, 0, 1);
        }, 50);
        parentPort.postMessage('ready');
    `;

    const worker = new Worker(workerCode, { eval: true, workerData: view.buffer });
    await new Promise((resolve) => worker.once('message', resolve));
    worker.unref(); // only the waitAsync should keep the loop alive

    const result = Atomics.waitAsync(view, 0, 0);
    assert.strictEqual(result.async, true, 'should be async');
    const value = await result.value;
    assert.strictEqual(value, 'ok', `expected 'ok', got "${value}"`);
}

// With timeout, resolved by notify
// A timeout is specified but the notify arrives first.  Must resolve to "ok".
{
    const view = makeInt32(0);
  
    const workerCode = `
      import { workerData, parentPort } from 'node:worker_threads';
      const view = new Int32Array(workerData, 0, 1);
      setTimeout(() => {
        Atomics.store(view, 0, 1);
        Atomics.notify(view, 0, 1);
      }, 50);
      parentPort.postMessage('ready');
    `;
  
    const worker = new Worker(workerCode, { eval: true, workerData: view.buffer });
    await new Promise((resolve) => worker.once('message', resolve));
    worker.unref();
  
    const result = Atomics.waitAsync(view, 0, 0, 5_000 /* 5 s – won't fire */);
    assert.strictEqual(result.async, true);
    const value = await result.value;
    assert.strictEqual(value, 'ok');
}
  
// With timeout, resolved by timeout itself
// No notify ever fires; the promise must resolve to "timed-out" via the timer.
{
  const view = makeInt32(0);

  const result = Atomics.waitAsync(view, 0, 0, 30);
  assert.strictEqual(result.async, true);
  const value = await result.value;
  assert.strictEqual(value, 'timed-out');
}

// Multiple concurrent waiters
// All promises must resolve and the loop must not exit until the last one does.
{
  const view = makeInt32(0);

  const workerCode = `
    import { workerData, parentPort } from 'node:worker_threads';
    const view = new Int32Array(workerData, 0, 1);
    setTimeout(() => {
    Atomics.store(view, 0, 1);
    Atomics.notify(view, 0, 3);  // wake up to 3 waiters
    }, 50);
    parentPort.postMessage('ready');
  `;

  const worker = new Worker(workerCode, { eval: true, workerData: view.buffer });
  await new Promise((resolve) => worker.once('message', resolve));
  worker.unref();

  const [r1, r2, r3] = [
    Atomics.waitAsync(view, 0, 0),
    Atomics.waitAsync(view, 0, 0),
    Atomics.waitAsync(view, 0, 0),
  ];
  assert.strictEqual(r1.async, true);
  assert.strictEqual(r2.async, true);
  assert.strictEqual(r3.async, true);
  const values = await Promise.all([r1.value, r2.value, r3.value]);
  assert.deepStrictEqual(values, ['ok', 'ok', 'ok']);
}

// Immediate synchronous resolution (value mismatch)
// When the current value does not equal the expected value, waitAsync must
// return { async: false, value: "not-equal" } and not ref any handle.
{
  const view = makeInt32(99); // already != 0
  const result = Atomics.waitAsync(view, 0, 0);
  assert.strictEqual(result.async, false);
  assert.strictEqual(result.value, 'not-equal');
}
