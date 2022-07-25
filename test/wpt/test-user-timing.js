'use strict';
require('../common');
const { WPTRunner } = require('../common/wpt');

const runner = new WPTRunner('user-timing');

// Needed to access to DOMException.
runner.setFlags(['--expose-internals']);

runner.setInitScript(`
  const {
    PerformanceMark,
    PerformanceMeasure,
    PerformanceObserver,
    performance,
  } = require('perf_hooks');
  global.PerformanceMark = performance;
  global.PerformanceMeasure = performance;
  global.PerformanceObserver = PerformanceObserver;
  global.performance = performance;

  const { internalBinding } = require('internal/test/binding');
  const { DOMException } = internalBinding('messaging');
  global.DOMException = DOMException;
`);

runner.runJsTests();
