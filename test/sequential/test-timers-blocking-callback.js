// Flags: --expose-internals
'use strict';

/*
 * This is a regression test for
 * https://github.com/nodejs/node-v0.x-archive/issues/15447 and
 * https://github.com/nodejs/node-v0.x-archive/issues/9333.
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
const { sleep } = require('internal/util');

const TIMEOUT = 100;

let nbBlockingCallbackCalls;
let latestDelay;
let timeCallbackScheduled;

// These tests are timing dependent so they may fail even when the bug is
// not present (if the host is sufficiently busy that the timers are delayed
// significantly). However, they fail 100% of the time when the bug *is*
// present, so to increase reliability, allow for a small number of retries.
let retries = 2;

function initTest() {
  nbBlockingCallbackCalls = 0;
  latestDelay = 0;
  timeCallbackScheduled = 0;
}

function blockingCallback(retry, callback) {
  ++nbBlockingCallbackCalls;

  if (nbBlockingCallbackCalls > 1) {
    latestDelay = Date.now() - timeCallbackScheduled;
    // Even if timers can fire later than when they've been scheduled
    // to fire, they shouldn't generally be more than 100% late in this case.
    // But they are guaranteed to be at least 100ms late given the bug in
    // https://github.com/nodejs/node-v0.x-archive/issues/15447 and
    // https://github.com/nodejs/node-v0.x-archive/issues/9333.
    if (latestDelay >= TIMEOUT * 2) {
      if (retries > 0) {
        retries--;
        return retry(callback);
      }
      assert.fail(`timeout delayed by more than 100% (${latestDelay}ms)`);
    }
    if (callback)
      return callback();
  } else {
    // Block by busy-looping to trigger the issue
    sleep(TIMEOUT);

    timeCallbackScheduled = Date.now();
    setTimeout(blockingCallback.bind(null, retry, callback), TIMEOUT);
  }
}

function testAddingTimerToEmptyTimersList(callback) {
  initTest();
  // Call setTimeout just once to make sure the timers list is
  // empty when blockingCallback is called.
  setTimeout(
    blockingCallback.bind(null, testAddingTimerToEmptyTimersList, callback),
    TIMEOUT
  );
}

function testAddingTimerToNonEmptyTimersList() {
  // If both timers fail and attempt a retry, only actually do anything for one
  // of them.
  let retryOK = true;
  const retry = () => {
    if (retryOK)
      testAddingTimerToNonEmptyTimersList();
    retryOK = false;
  };

  initTest();
  // Call setTimeout twice with the same timeout to make
  // sure the timers list is not empty when blockingCallback is called.
  setTimeout(
    blockingCallback.bind(null, retry),
    TIMEOUT
  );
  setTimeout(
    blockingCallback.bind(null, retry),
    TIMEOUT
  );
}

// Run the test for the empty timers list case, and then for the non-empty
// timers list one.
testAddingTimerToEmptyTimersList(
  common.mustCall(testAddingTimerToNonEmptyTimersList)
);
