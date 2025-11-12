'use strict';

require('../common');

// Verifies that setting up two observers to listen
// to gc performance does not crash.

const {
  PerformanceObserver,
} = require('perf_hooks');

// We don't actually care if the callback is ever invoked in this test
const obs = new PerformanceObserver(() => {});
const obs2 = new PerformanceObserver(() => {});

obs.observe({ type: 'gc' });
obs2.observe({ type: 'gc' });
