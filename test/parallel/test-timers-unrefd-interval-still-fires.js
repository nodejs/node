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

var checkInterval = 100;
var maxNbChecks = 5;
var nbChecks = 1;
setTimeout(function check() {
  if (nbChecks >= maxNbChecks || nbIntervalFired == N) {
    assert.strictEqual(nbIntervalFired, N);
    return;
  }
  ++nbChecks;
  setTimeout(check, checkInterval);
}, checkInterval);
