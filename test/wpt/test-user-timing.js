'use strict';

require('../common');
const { WPTRunner } = require('../common/wpt');

const runner = new WPTRunner('user-timing');

runner.pretendGlobalThisAs('Window');
runner.brandCheckGlobalScopeAttribute('performance');
runner.setInitScript(`
  const {
    PerformanceEntry,
    PerformanceMark,
    PerformanceMeasure,
    PerformanceObserver,
  } = require('perf_hooks');
  Object.defineProperty(global, 'PerformanceEntry', {
    value: PerformanceEntry,
    enumerable: false,
    writable: true,
    configurable: true,
  });
  Object.defineProperty(global, 'PerformanceMark', {
    value: PerformanceMark,
    enumerable: false,
    writable: true,
    configurable: true,
  });
  Object.defineProperty(global, 'PerformanceMeasure', {
    value: PerformanceMeasure,
    enumerable: false,
    writable: true,
    configurable: true,
  });
  Object.defineProperty(global, 'PerformanceObserver', {
    value: PerformanceObserver,
    enumerable: false,
    writable: true,
    configurable: true,
  });
`);

runner.runJsTests();
