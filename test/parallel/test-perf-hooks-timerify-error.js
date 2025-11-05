// Test that errors thrown in timerified functions bubble up without creating
// performance timeline entries.

'use strict';

const common = require('../common');
const assert = require('assert');
const { timerify, PerformanceObserver } = require('perf_hooks');

const obs = new PerformanceObserver(common.mustNotCall());
obs.observe({ entryTypes: ['function'] });
const n = timerify(() => {
  throw new Error('test');
});
assert.throws(() => n(), /^Error: test$/);
obs.disconnect();
