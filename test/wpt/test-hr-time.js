'use strict';

require('../common');
const { WPTRunner } = require('../common/wpt');

const runner = new WPTRunner('hr-time');

runner.setInitScript(`
  const { performance, PerformanceObserver } = require('perf_hooks');
  global.performance = performance;
  global.PerformanceObserver = PerformanceObserver;
`);

runner.runJsTests();
