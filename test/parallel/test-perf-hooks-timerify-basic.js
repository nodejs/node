// Test basic functionality of timerify and PerformanceObserver.
'use strict';

const common = require('../common');
const assert = require('assert');
const { timerify, PerformanceObserver } = require('perf_hooks');

// Verifies that `performance.timerify` is an alias of `perf_hooks.timerify`.
assert.strictEqual(performance.timerify, timerify);

// Intentional non-op. Do not wrap in common.mustCall();
const n = timerify(function noop() {});

const obs = new PerformanceObserver(common.mustCall((list) => {
  const entries = list.getEntries();
  const entry = entries[0];
  assert(entry);
  assert.strictEqual(entry.name, 'noop');
  assert.strictEqual(entry.entryType, 'function');
  assert.strictEqual(typeof entry.duration, 'number');
  assert.strictEqual(typeof entry.startTime, 'number');
  obs.disconnect();
}));
obs.observe({ entryTypes: ['function'] });
n();
