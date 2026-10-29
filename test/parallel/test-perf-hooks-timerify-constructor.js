// Test that timerify works with class constructors and creates performance
// entries with the correct name.

'use strict';

const common = require('../common');
const assert = require('assert');
const { timerify, PerformanceObserver } = require('perf_hooks');

class N {}
const n = timerify(N);

const obs = new PerformanceObserver(common.mustCall((list) => {
  const entries = list.getEntries();
  const entry = entries[0];
  assert.strictEqual(entry[0], 1);
  assert.strictEqual(entry[1], 'abc');
  assert(entry);
  assert.strictEqual(entry.name, 'N');
  assert.strictEqual(entry.entryType, 'function');
  assert.strictEqual(typeof entry.duration, 'number');
  assert.strictEqual(typeof entry.startTime, 'number');
  obs.disconnect();
}));
obs.observe({ entryTypes: ['function'] });

new n(1, 'abc');
