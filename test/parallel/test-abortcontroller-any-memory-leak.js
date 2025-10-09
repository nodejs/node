'use strict';

// Test for memory leak when using AbortSignal.any() in tight loops
// Refs: https://github.com/nodejs/node/issues/54614

const common = require('../common');
const assert = require('assert');

// This test verifies that AbortSignal.any() with non-aborted signals
// does not cause unbounded memory growth when no abort listeners are attached.
//
// Root cause: AbortSignal.any() was adding signals to gcPersistentSignals
// immediately, preventing garbage collection even when no listeners existed.
// The fix ensures signals are only added to gcPersistentSignals when abort
// listeners are actually registered (handled by kNewListener).

async function testAbortSignalAnyBasic() {
  const iterations = 100000;
  const memBefore = process.memoryUsage().heapUsed;

  for (let i = 0; i < iterations; i++) {
    const abortController = new AbortController();
    const signal = abortController.signal;
    const composedSignal = AbortSignal.any([signal]);

    // Periodically allow event loop to process
    if (i % 1000 === 0) {
      await new Promise(setImmediate);
    }
  }

  // Force GC if available
  if (global.gc) {
    global.gc();
    await new Promise(setImmediate);
  }

  const memAfter = process.memoryUsage().heapUsed;
  const growth = memAfter - memBefore;
  const growthMB = growth / 1024 / 1024;

  console.log(`Memory growth (basic): ${growthMB.toFixed(2)} MB`);

  // Memory growth should be reasonable (< 50MB for 100k iterations)
  // Without the fix, this would grow 100s of MBs
  assert.ok(growthMB < 50,
    `Excessive memory growth: ${growthMB.toFixed(2)} MB (expected < 50 MB)`);
}

async function testAbortSignalAnyMultiple() {
  const iterations = 100000;
  const memBefore = process.memoryUsage().heapUsed;

  for (let i = 0; i < iterations; i++) {
    const controller1 = new AbortController();
    const controller2 = new AbortController();
    const composedSignal = AbortSignal.any([
      controller1.signal,
      controller2.signal,
    ]);

    // Periodically allow event loop to process
    if (i % 1000 === 0) {
      await new Promise(setImmediate);
    }
  }

  // Force GC if available
  if (global.gc) {
    global.gc();
    await new Promise(setImmediate);
  }

  const memAfter = process.memoryUsage().heapUsed;
  const growth = memAfter - memBefore;
  const growthMB = growth / 1024 / 1024;

  console.log(`Memory growth (multiple): ${growthMB.toFixed(2)} MB`);

  assert.ok(growthMB < 50,
    `Excessive memory growth: ${growthMB.toFixed(2)} MB (expected < 50 MB)`);
}

async function testAbortSignalAnyWithTimeout() {
  const iterations = 50000; // Fewer iterations due to timeout overhead
  const memBefore = process.memoryUsage().heapUsed;

  for (let i = 0; i < iterations; i++) {
    const abortController = new AbortController();
    const composedSignal = AbortSignal.any([
      abortController.signal,
      AbortSignal.timeout(1000), // 1 second timeout
    ]);

    // Periodically allow event loop to process
    if (i % 500 === 0) {
      await new Promise(setImmediate);
    }
  }

  // Force GC if available
  if (global.gc) {
    global.gc();
    await new Promise(setImmediate);
  }

  const memAfter = process.memoryUsage().heapUsed;
  const growth = memAfter - memBefore;
  const growthMB = growth / 1024 / 1024;

  console.log(`Memory growth (with timeout): ${growthMB.toFixed(2)} MB`);

  // Timeout signals create some overhead, but should still be reasonable
  assert.ok(growthMB < 100,
    `Excessive memory growth: ${growthMB.toFixed(2)} MB (expected < 100 MB)`);
}

// Run tests
(async () => {
  console.log('Testing AbortSignal.any() memory leak...');

  await testAbortSignalAnyBasic();
  await testAbortSignalAnyMultiple();
  await testAbortSignalAnyWithTimeout();

  console.log('All tests passed!');
})().catch(common.mustNotCall());
