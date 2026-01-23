'use strict';

// This test verifies that worker threads do not cache stale CWD values
// after process.chdir() has completed in the main thread.
//
// Regression test for a TOCTOU race condition where the atomic counter
// was incremented before chdir() completed, allowing workers to cache
// stale directory paths.

const common = require('../common');
const assert = require('assert');
const path = require('path');
const { Worker, isMainThread, parentPort } = require('worker_threads');

if (!isMainThread) {
  // Worker: respond to 'check' messages with current cwd
  parentPort.on('message', (msg) => {
    if (msg.type === 'check') {
      parentPort.postMessage({
        type: 'cwd',
        cwd: process.cwd(),
        expected: msg.expected,
      });
    }
  });
  return;
}

// Main thread
const testDir = __dirname;
const parentDir = path.dirname(testDir);

// Ensure we start in a known directory
process.chdir(testDir);

const worker = new Worker(__filename);

let checksCompleted = 0;
const totalChecks = 100;

worker.on('message', common.mustCall((msg) => {
  if (msg.type === 'cwd') {
    // After chdir() has returned in the main thread, the worker
    // must see the new directory, not a stale cached value
    assert.strictEqual(
      msg.cwd,
      msg.expected,
      `Worker returned stale CWD: got "${msg.cwd}", expected "${msg.expected}"`
    );
    checksCompleted++;

    if (checksCompleted < totalChecks) {
      // Alternate between directories
      const newDir = checksCompleted % 2 === 0 ? testDir : parentDir;
      process.chdir(newDir);
      // Immediately after chdir returns, ask worker for cwd
      worker.postMessage({ type: 'check', expected: newDir });
    } else {
      worker.terminate();
    }
  }
}, totalChecks));

worker.on('online', common.mustCall(() => {
  // Start the test cycle
  process.chdir(parentDir);
  worker.postMessage({ type: 'check', expected: parentDir });
}));

worker.on('exit', common.mustCall((code) => {
  assert.strictEqual(code, 1); // terminated
  assert.strictEqual(checksCompleted, totalChecks);
}));
