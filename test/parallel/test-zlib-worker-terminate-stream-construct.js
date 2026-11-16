// Flags: --expose-gc
'use strict';
const common = require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');

// Tests that terminating a worker thread while constructing a zlib stream
// (or garbage-collecting an uninitialized zlib stream handle) does not abort
// the process with assertion: `init_done_ && "close before init"`.
// Refs: https://github.com/nodejs/node/issues/67485

if (process.argv[2] !== 'child') {
  // Parent process: runs the test in a child process and asserts it does not abort.
  const result = spawnSync(process.execPath, [
    '--expose-gc',
    __filename,
    'child',
  ], { encoding: 'utf8' });

  assert.strictEqual(
    result.status,
    0,
    `Child process exited with code ${result.status} (signal: ${result.signal}).\nStderr:\n${result.stderr}`
  );
  assert.strictEqual(result.signal, null);
  assert.doesNotMatch(result.stderr, /Assertion failed/);
  return;
}

// Child process execution:
const { Worker } = require('worker_threads');
const zlib = require('zlib');

// Test 1: Worker thread terminated while constructing a zlib stream
{
  const workerCode = `
    const zlib = require('zlib');
    const { parentPort } = require('worker_threads');
    new zlib.Gzip({
      get rejectGarbageAfterEnd() {
        parentPort.postMessage('in_constructor');
        const start = Date.now();
        while (Date.now() - start < 2000) {}
        return false;
      }
    });
  `;

  const worker = new Worker(workerCode, { eval: true });
  worker.on('message', common.mustCall((msg) => {
    assert.strictEqual(msg, 'in_constructor');
    worker.terminate();
  }));

  worker.on('exit', common.mustCall((exitCode) => {
    assert.strictEqual(exitCode, 1);
  }));
}

// Test 2: Uninitialized zlib stream handle garbage-collected after error during construction
{
  assert.throws(() => {
    new zlib.Gzip({
      get rejectGarbageAfterEnd() {
        throw new Error('boom');
      }
    });
  }, { message: 'boom' });

  if (typeof global.gc === 'function') {
    // Run multiple GC passes to ensure weak callbacks run
    global.gc();
    setImmediate(() => {
      global.gc();
    });
  }
}
