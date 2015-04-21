// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

/*
 * This is a regression test for https://github.com/joyent/node/issues/15447
 * and https://github.com/joyent/node/issues/9333.
 *
 * When a timer is added in another timer's callback, its underlying timer
 * handle was started with a timeout that was actually incorrect.
 *
 * The reason was that the value that represents the current time was not
 * updated between the time the original callback was called and the time
 * the added timer was processed by timers.listOnTimeout. That lead the
 * logic in timers.listOnTimeout to do an incorrect computation that made
 * the added timer fire with a timeout of scheduledTimeout +
 * timeSpentInCallback.
 *
 * This test makes sure that a timer added by another timer's callback
 * fire with the expected timeout.
 *
 * It makes sure that it works when the timers list for a given timeout is
 * empty (see testAddingTimerToEmptyTimersList) and when the timers list
 * is not empty (see testAddingTimerToNonEmptyTimersList).
 */

var assert = require('assert');
var common = require('../common');

var TIMEOUT = 100;

var nbBlockingCallbackCalls = 0;
var latestDelay = 0;
var timeCallbackScheduled = 0;

function initTest() {
  nbBlockingCallbackCalls = 0;
  latestDelay = 0;
  timeCallbackScheduled = 0;
}

function blockingCallback(callback) {
  ++nbBlockingCallbackCalls;

  if (nbBlockingCallbackCalls > 1) {
    latestDelay = new Date().getTime() - timeCallbackScheduled;
    // Even if timers can fire later than when they've been scheduled
    // to fire, they should more than 50% later with a timeout of
    // 100ms. Firing later than that would mean that we hit the regression
    // highlighted in
    // https://github.com/joyent/node/issues/15447 and
    // https://github.com/joyent/node/issues/9333.
    assert(latestDelay < TIMEOUT * 1.5);
    if (callback)
      return callback();
  } else {
    // block by busy-looping to trigger the issue
    common.busyLoop(TIMEOUT);

    timeCallbackScheduled = new Date().getTime();
    setTimeout(blockingCallback, TIMEOUT);
  }
}

function testAddingTimerToEmptyTimersList(callback) {
  initTest();
  // Call setTimeout just once to make sure the timers list is
  // empty when blockingCallback is called.
  setTimeout(blockingCallback.bind(global, callback), TIMEOUT);
}

function testAddingTimerToNonEmptyTimersList() {
  initTest();
  // Call setTimeout twice with the same timeout to make
  // sure the timers list is not empty when blockingCallback is called.
  setTimeout(blockingCallback, TIMEOUT);
  setTimeout(blockingCallback, TIMEOUT);
}

// Run the test for the empty timers list case, and then for the non-empty
// timers list one
testAddingTimerToEmptyTimersList(testAddingTimerToNonEmptyTimersList);
