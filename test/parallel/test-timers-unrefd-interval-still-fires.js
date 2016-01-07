'use strict';
/*
 * This test is a regression test for joyent/node#8900.
 *
 * Node.js 0.10.34 contains the bug that this test detects.
 * Node.js 0.10.35 contains the fix.
 *
 * Due to the introduction of ES6-isms, comment out `'use strict'` and
 * `require('../common');` to run the test on Node.js 0.10.x.
 */
require('../common');

const N = 3;
var nbIntervalFired = 0;

const keepOpen = setInterval(function() {}, 9999);

const timer = setInterval(function() {
  ++nbIntervalFired;
  if (nbIntervalFired === N) {
    clearInterval(timer);
    clearInterval(keepOpen);
  }
}, 1);

timer.unref();
