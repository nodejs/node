'use strict';

// Flags: --expose-internals

require('../common');
const { WPTRunner } = require('../common/wpt');
const { performance, PerformanceObserver } = require('perf_hooks');

const runner = new WPTRunner('hr-time');

runner.copyGlobalsFromObject(global, [
  'setInterval',
  'clearInterval',
  'setTimeout',
  'clearTimeout'
]);

runner.defineGlobal('performance', {
  get() {
    return performance;
  }
});
runner.defineGlobal('PerformanceObserver', {
  value: PerformanceObserver
});

runner.runJsTests();
