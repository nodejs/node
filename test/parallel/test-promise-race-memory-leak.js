'use strict';

// Test for memory leak when racing immediately-resolving Promises
// Refs: https://github.com/nodejs/node/issues/51452

const common = require('../common');
const assert = require('assert');

// This test verifies that Promise.race() with immediately-resolving promises
// does not cause unbounded memory growth.
//
// Root cause: When Promise.race() settles, V8 attempts to resolve the other
// promises in the race, triggering 'multipleResolves' events. These events
// are queued in nextTick, but if the event loop never gets a chance to drain
// the queue (tight loop), memory grows unbounded.

async function promiseValue(value) {
  return value;
}

async function testPromiseRace() {
  const iterations = 100000;
  const memBefore = process.memoryUsage().heapUsed;

  for (let i = 0; i < iterations; i++) {
    await Promise.race([promiseValue('foo'), promiseValue('bar')]);

    // Allow event loop to drain nextTick queue periodically
    if (i % 1000 === 0) {
      await new Promise(setImmediate);
    }
  }

  const memAfter = process.memoryUsage().heapUsed;
  const growth = memAfter - memBefore;
  const growthMB = growth / 1024 / 1024;

  console.log(`Memory growth: ${growthMB.toFixed(2)} MB`);

  // Memory growth should be reasonable (< 50MB for 100k iterations)
  // Without the fix, this would grow 100s of MBs
  assert.ok(growthMB < 50,
    `Excessive memory growth: ${growthMB.toFixed(2)} MB (expected < 50 MB)`);
}

async function testPromiseAny() {
  const iterations = 100000;
  const memBefore = process.memoryUsage().heapUsed;

  for (let i = 0; i < iterations; i++) {
    await Promise.any([promiseValue('foo'), promiseValue('bar')]);

    // Allow event loop to drain nextTick queue periodically
    if (i % 1000 === 0) {
      await new Promise(setImmediate);
    }
  }

  const memAfter = process.memoryUsage().heapUsed;
  const growth = memAfter - memBefore;
  const growthMB = growth / 1024 / 1024;

  console.log(`Memory growth (any): ${growthMB.toFixed(2)} MB`);

  // Memory growth should be reasonable
  assert.ok(growthMB < 50,
    `Excessive memory growth: ${growthMB.toFixed(2)} MB (expected < 50 MB)`);
}

// Run tests
(async () => {
  console.log('Testing Promise.race() memory leak...');
  await testPromiseRace();

  console.log('Testing Promise.any() memory leak...');
  await testPromiseAny();

  console.log('All tests passed!');
})().catch(common.mustNotCall());
