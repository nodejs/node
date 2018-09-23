'use strict';
/*
 * This test is a regression test for joyent/node#8900.
 */
const common = require('../common');
const assert = require('assert');

const TEST_DURATION = common.platformTimeout(100);
const N = 5;
var nbIntervalFired = 0;

const keepOpen = setTimeout(() => {
  console.error('[FAIL] Interval fired %d/%d times.', nbIntervalFired, N);
  throw new Error('Test timed out. keepOpen was not canceled.');
}, TEST_DURATION);

const timer = setInterval(() => {
  ++nbIntervalFired;
  if (nbIntervalFired === N) {
    clearInterval(timer);
    timer._onTimeout = () => {
      throw new Error('Unrefd interval fired after being cleared.');
    };
    setImmediate(() => clearTimeout(keepOpen));
  }
}, 1);

timer.unref();
