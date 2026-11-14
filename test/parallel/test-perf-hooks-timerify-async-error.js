// Test that a timerified function which returns a rejected promise behaves
// the same as one that throws synchronously: no performance timeline entry
// and no histogram record.

'use strict';

const common = require('../common');
const assert = require('assert');
const { timerify, PerformanceObserver, createHistogram } = require('perf_hooks');

const obs = new PerformanceObserver(common.mustNotCall());
obs.observe({ entryTypes: ['function'] });

const histogram = createHistogram();
const n = timerify(async () => {
  throw new Error('test');
}, { histogram });

assert.rejects(n(), /^Error: test$/).then(common.mustCall(() => {
  assert.strictEqual(histogram.count, 0);
  obs.disconnect();
}));
