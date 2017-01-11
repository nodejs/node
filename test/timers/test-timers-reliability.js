'use strict';
// FaketimeFlags: --exclude-monotonic -f '2014-07-21 09:00:00'

require('../common');

var Timer = process.binding('timer_wrap').Timer;
const assert = require('assert');

var timerFired = false;
var zeroTimerFired = false;
var minusTimerFired = false;
var undefinedTimerFired = false;
var intervalFired = false;
var zeroIntervalFired = false;
var minusIntervalFired = false;
var undefinedIntervalFired = false;

/*
 * This test case aims at making sure that timing utilities such
 * as setTimeout and setInterval are not vulnerable to time
 * drifting or inconsistent time changes (such as ntp time sync
 * in the past, etc.).
 *
 * It is run using faketime so that we change how
 * non-monotonic clocks perceive time movement. We freeze
 * non-monotonic time, and check if setTimeout and setInterval
 * work properly in that situation.
 *
 * We check this by setting a timer based on a monotonic clock
 * to fire after setTimeout's callback is supposed to be called.
 * This monotonic timer, by definition, is not subject to time drifting
 * and inconsistent time changes, so it can be considered as a solid
 * reference.
 *
 * When the monotonic timer fires, if the setTimeout's callback
 * hasn't been called yet, it means that setTimeout's underlying timer
 * is vulnerable to time drift or inconsistent time changes.
 */

var monoTimer = new Timer();
monoTimer[Timer.kOnTimeout] = function() {
    /*
     * Make sure that setTimeout's and setInterval's callbacks have
     * already fired, otherwise it means that they are vulnerable to
     * time drifting or inconsistent time changes.
     */
  assert(timerFired);
  assert(zeroTimerFired);
  assert(minusTimerFired);
  assert(undefinedTimerFired);
  assert(intervalFired);
  assert(zeroIntervalFired);
  assert(minusIntervalFired);
  assert(undefinedIntervalFired);
};

monoTimer.start(300);

setTimeout(function() {
  timerFired = true;
}, 200);

setTimeout(function() {
  zeroTimerFired = true;
}, 0);

setTimeout(function() {
  minusTimerFired = true;
}, -1);

setTimeout(function() {
  undefinedTimerFired = true;
});

var interval = setInterval(function() {
  intervalFired = true;
  clearInterval(interval);
}, 200);

var zeroInterval = setInterval(function() {
  zeroIntervalFired = true;
  clearInterval(zeroInterval);
}, 0);

var minusInterval = setInterval(function() {
  minusIntervalFired = true;
  clearInterval(minusInterval);
}, -1);

var undefinedInterval = setInterval(function() {
  undefinedIntervalFired = true;
  clearInterval(undefinedInterval);
});
