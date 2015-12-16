'use strict';
require('../common');
const assert = require('assert');
const T = require('timers');

// This test is to make sure the timer will not fire again
// when it is disarmed in callback.
// This script is called by
// ../parallel/test-timers-disarmed-in-callbak-not-fire-anymore.js
// with NODE_DEBUG=timer to make sure 'active' and 'listOnTimeout'
// will not be called again after the timer is disarmed.
// Before fix https://github.com/nodejs/node/pull/4303,
// When disarm with clearInterval, it works fine.
// When disarm with close, active and listOnTimeout will be called again.
// When disarm with unenroll, timer still works.
// After this fix, timer behaves the same as clearTimeout/clearInterval
// when it is disared by close/unenroll in callback.

var nbIntervalFired1 = 0;
var nbIntervalFired2 = 0;
var nbIntervalFired3 = 0;
const timer1 = setInterval(() => {
  nbIntervalFired1++;
  clearInterval(timer1);
}, 11);
const timer2 = setInterval(() => {
  nbIntervalFired2++;
  timer2.close();
}, 12);
const timer3 = setInterval(() => {
  nbIntervalFired3++;
  T.unenroll(timer3);
}, 13);
setTimeout(() => {
  assert.strictEqual(nbIntervalFired1, 1);
  assert.strictEqual(nbIntervalFired2, 1);
  assert.strictEqual(nbIntervalFired3, 1);
}, 100);
