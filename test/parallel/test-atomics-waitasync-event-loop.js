'use strict';

// Verify that Atomics.waitAsync() keeps the event loop alive until the
// promise settles. Without the fix, the process exits before the promise
// resolves because V8's internal wait mechanism does not create a libuv handle.
// Ref: https://github.com/nodejs/node/issues/61941

require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');

// Test 1: Atomics.waitAsync with timeout keeps the loop alive.
// The waitAsync promise is the only pending async work; the event loop must
// not exit before it settles.
{
  const result = spawnSync(process.execPath, [
    '--no-warnings',
    '-e',
    `
      const assert = require('assert');
      const sab = new SharedArrayBuffer(4);
      const i32 = new Int32Array(sab);

      const result = Atomics.waitAsync(i32, 0, 0, 50);
      assert.strictEqual(result.async, true);

      result.value.then((val) => {
        assert.strictEqual(val, 'timed-out');
        process.exitCode = 0;
      });
    `,
  ], { timeout: 10_000 });

  assert.strictEqual(
    result.status, 0,
    `expected exit code 0, got ${result.status}. ` +
    `stderr: ${result.stderr?.toString()}`
  );
}

// Test 2: Synchronous waitAsync (value already changed) does not leak a ref.
// The process should exit normally without hanging.
{
  const result = spawnSync(process.execPath, [
    '--no-warnings',
    '-e',
    `
      const sab = new SharedArrayBuffer(4);
      const i32 = new Int32Array(sab);

      // Value is already 1, waitAsync expects 0, so it returns synchronously.
      Atomics.store(i32, 0, 1);
      const result = Atomics.waitAsync(i32, 0, 0);

      // Sync case: the result is not async, no ref should be added.
      if (!result.async) {
        process.exitCode = 0;
      }
    `,
  ], { timeout: 5_000 });

  assert.strictEqual(
    result.status, 0,
    `expected exit code 0, got ${result.status}. ` +
    `stderr: ${result.stderr?.toString()}`
  );
}

// Test 3: Multiple concurrent waitAsync calls properly ref-count.
// All pending waits must resolve before the process exits.
{
  const result = spawnSync(process.execPath, [
    '--no-warnings',
    '-e',
    `
      const assert = require('assert');
      const sab = new SharedArrayBuffer(12);
      const i32 = new Int32Array(sab);

      let resolved = 0;
      for (let idx = 0; idx < 3; idx++) {
        const result = Atomics.waitAsync(i32, idx, 0, 50);
        assert.strictEqual(result.async, true);
        result.value.then((val) => {
          assert.strictEqual(val, 'timed-out');
          resolved++;
          if (resolved === 3) {
            process.exitCode = 0;
          }
        });
      }
    `,
  ], { timeout: 10_000 });

  assert.strictEqual(
    result.status, 0,
    `expected exit code 0, got ${result.status}. ` +
    `stderr: ${result.stderr?.toString()}`
  );
}

// Test 4: Worker thread notifies Atomics.waitAsync on the main thread.
// This is the real-world Emscripten/pthreads pattern from the issue.
{
  const result = spawnSync(process.execPath, [
    '--no-warnings',
    '-e',
    `
      const assert = require('assert');
      const { Worker } = require('worker_threads');
      const sab = new SharedArrayBuffer(4);
      const view = new Int32Array(sab, 0, 1);
      view[0] = 0;

      const code = \`
        const { workerData, parentPort } = require('worker_threads');
        const view = new Int32Array(workerData, 0, 1);
        setTimeout(() => {
          Atomics.store(view, 0, 1);
          Atomics.notify(view, 0, 1);
        }, 50);
        parentPort.postMessage('ready');
      \`;

      const w = new Worker(code, { eval: true, workerData: sab });
      w.on('message', async () => {
        w.unref();
        const result = Atomics.waitAsync(view, 0, 0);
        const v = await result.value;
        assert.strictEqual(v, 'ok');
        process.exitCode = 0;
      });
    `,
  ], { timeout: 10_000 });

  assert.strictEqual(
    result.status, 0,
    `expected exit code 0, got ${result.status}. ` +
    `stderr: ${result.stderr?.toString()}`
  );
}
