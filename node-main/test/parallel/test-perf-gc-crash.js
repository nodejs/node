'use strict';

require('../common');

// Refers to https://github.com/nodejs/node/issues/39548

// The test fails if this crashes. If it closes normally,
// then all is good.

const {
  PerformanceObserver,
} = require('perf_hooks');

// We don't actually care if the observer callback is called here.
const gcObserver = new PerformanceObserver(() => {});

gcObserver.observe({ entryTypes: ['gc'] });

gcObserver.disconnect();

const gcObserver2 = new PerformanceObserver(() => {});

gcObserver2.observe({ entryTypes: ['gc'] });

gcObserver2.disconnect();
