'use strict';

/*
 * This is a regression test for https://github.com/joyent/node/issues/15447
 * and https://github.com/joyent/node/issues/9333.
 *
 * When a timer is added in another timer's callback, its underlying timer
 * handle was started with a timeout that was actually incorrect.
 *
 * The reason was that the value that represents the current time was not
 * updated between the time the original callback was called and the time
 * the added timer was processed by timers.listOnTimeout. That led the
 * logic in timers.listOnTimeout to do an incorrect computation that made
 * the added timer fire with a timeout of scheduledTimeout +
 * timeSpentInCallback.
 *
 * This test makes sure that a timer added by another timer's callback
 * fires with the expected timeout.
 *
 * It makes sure that it works when the timers list for a given timeout is
 * empty (see testAddingTimerToEmptyTimersList) and when the timers list
 * is not empty (see testAddingTimerToNonEmptyTimersList).
 */

const common = require('../common');
const assert = require('assert');
const Timer = process.binding('timer_wrap').Timer;

const TIMEOUT = 100;

let nbBlockingCallbackCalls = 0;
let latestDelay = 0;
let timeCallbackScheduled = 0;

function initTest() {
  nbBlockingCallbackCalls = 0;
  latestDelay = 0;
  timeCallbackScheduled = 0;
}

function blockingCallback(callback) {
  ++nbBlockingCallbackCalls;

  if (nbBlockingCallbackCalls > 1) {
    latestDelay = Timer.now() - timeCallbackScheduled;
    // Even if timers can fire later than when they've been scheduled
    // to fire, they shouldn't generally be more than 100% late in this case.
    // But they are guaranteed to be at least 100ms late given the bug in
    // https://github.com/nodejs/node-v0.x-archive/issues/15447 and
    // https://github.com/nodejs/node-v0.x-archive/issues/9333..
    assert(latestDelay < TIMEOUT * 2);
    if (callback)
      return callback();
  } else {
    // block by busy-looping to trigger the issue
    common.busyLoop(TIMEOUT);

    timeCallbackScheduled = Timer.now();
    setTimeout(blockingCallback.bind(null, callback), TIMEOUT);
  }
}

const testAddingTimerToEmptyTimersList = common.mustCall(function(callback) {
  initTest();
  // Call setTimeout just once to make sure the timers list is
  // empty when blockingCallback is called.
  setTimeout(blockingCallback.bind(null, callback), TIMEOUT);
});

const testAddingTimerToNonEmptyTimersList = common.mustCall(function() {
  initTest();
  // Call setTimeout twice with the same timeout to make
  // sure the timers list is not empty when blockingCallback is called.
  setTimeout(blockingCallback, TIMEOUT);
  setTimeout(blockingCallback, TIMEOUT);
});

// Run the test for the empty timers list case, and then for the non-empty
// timers list one
testAddingTimerToEmptyTimersList(testAddingTimerToNonEmptyTimersList);
