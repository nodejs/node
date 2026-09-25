'use strict';

// Simulate @sinonjs/fake-timers: patch the timers module BEFORE
// the test runner is loaded, so the test runner captures the patched
// versions at import time.
const nodeTimers = require('node:timers');
const originalSetTimeout = nodeTimers.setTimeout;
const originalClearTimeout = nodeTimers.clearTimeout;

const fakeTimers = new Map();
let nextId = 1;

nodeTimers.setTimeout = (fn, delay, ...args) => {
  const id = nextId++;
  const timer = originalSetTimeout(fn, delay, ...args);
  fakeTimers.set(id, timer);
  // Sinon fake timers return an object with unref/ref but without
  // Symbol.dispose, which would cause the test runner to throw.
  return { id, unref() {}, ref() {} };
};

nodeTimers.clearTimeout = (id) => {
  if (id != null && typeof id === 'object') id = id.id;
  const timer = fakeTimers.get(id);
  if (timer) {
    originalClearTimeout(timer);
    fakeTimers.delete(id);
  }
};

// Now load the test runner - it will capture our patched setTimeout/clearTimeout
const { test } = require('node:test');

test('test with fake timers and timeout', { timeout: 10_000 }, () => {
  // This test verifies that the test runner works when setTimeout returns
  // an object without Symbol.dispose (like sinon fake timers).
  // Previously, the test runner called timer[Symbol.dispose]() which would
  // throw TypeError on objects returned by fake timer implementations.
});

// Restore
nodeTimers.setTimeout = originalSetTimeout;
nodeTimers.clearTimeout = originalClearTimeout;
