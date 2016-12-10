'use strict';
// FaketimeFlags: --exclude-monotonic -f '2014-07-21 09:00:00'

require('../common');

var Timer = process.binding('timer_wrap').Timer;
var assert = require('assert');

var timerFired = false;
var intervalFired = false;

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
  assert(intervalFired);
};

monoTimer.start(300);

setTimeout(function() {
  timerFired = true;
}, 200);

var interval = setInterval(function() {
  intervalFired = true;
  clearInterval(interval);
}, 200);
