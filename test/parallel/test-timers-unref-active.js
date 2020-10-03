'use strict';

// This test is aimed at making sure that unref timers queued with
// timers._unrefActive work correctly.
//
// Basically, it queues one timer in the unref queue, and then queues
// it again each time its timeout callback is fired until the callback
// has been called ten times.
//
// At that point, it unenrolls the unref timer so that its timeout callback
// is not fired ever again.
//
// Finally, a ref timeout is used with a delay large enough to make sure that
// all 10 timeouts had the time to expire.

require('../common');
const timers = require('timers');
const assert = require('assert');

const someObject = {};
let nbTimeouts = 0;

// libuv 0.10.x uses GetTickCount on Windows to implement timers, which uses
// system's timers whose resolution is between 10 and 16ms. See
// http://msdn.microsoft.com/en-us/library/windows/desktop/ms724408.aspx
// for more information. That's the lowest resolution for timers across all
// supported platforms. We're using it as the lowest common denominator,
// and thus expect 5 timers to be able to fire in under 100 ms.
const N = 5;
const TEST_DURATION = 1000;

timers.unenroll(someObject);
timers.enroll(someObject, 1);

someObject._onTimeout = function _onTimeout() {
  ++nbTimeouts;

  if (nbTimeouts === N) timers.unenroll(someObject);

  timers._unrefActive(someObject);
};

timers._unrefActive(someObject);

setTimeout(function() {
  assert.strictEqual(nbTimeouts, N);
}, TEST_DURATION);
