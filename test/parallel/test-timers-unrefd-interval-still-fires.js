'use strict';
/*
 * This test is a regression test for joyent/node#8900.
 */
require('../common');
var assert = require('assert');

var N = 5;
var nbIntervalFired = 0;
var timer = setInterval(function() {
  ++nbIntervalFired;
  if (nbIntervalFired === N)
    clearInterval(timer);
}, 1);

timer.unref();

setTimeout(function onTimeout() {
  assert.strictEqual(nbIntervalFired, N);
}, 100);
