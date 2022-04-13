'use strict';

require('../common');
const { WPTRunner } = require('../common/wpt');

const runner = new WPTRunner('hr-time');

runner.setInitScript(`
  const { Blob } = require('buffer');
  global.Blob = Blob;

  const { PerformanceObserver } = require('perf_hooks');
  global.PerformanceObserver = PerformanceObserver;
`);

runner.runJsTests();
