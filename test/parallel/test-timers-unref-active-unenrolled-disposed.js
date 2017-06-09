'use strict';

// https://github.com/nodejs/node/pull/2540/files#r38231197

const common = require('../common');
const timers = require('timers');
const assert = require('assert');
const domain = require('domain');

// Crazy stuff to keep the process open,
// then close it when we are actually done.
const TEST_DURATION = common.platformTimeout(1000);
const keepOpen = setTimeout(function() {
  throw new Error('Test timed out. keepOpen was not canceled.');
}, TEST_DURATION);

const endTest = makeTimer(2);

const someTimer = makeTimer(1);
someTimer.domain = domain.create();
someTimer.domain.dispose();
someTimer._onTimeout = function() {
  throw new Error('someTimer was not supposed to fire!');
};

endTest._onTimeout = common.mustCall(function() {
  assert.strictEqual(someTimer._idlePrev, null);
  assert.strictEqual(someTimer._idleNext, null);
  clearTimeout(keepOpen);
});

const cancelsTimer = makeTimer(1);
cancelsTimer._onTimeout = common.mustCall(function() {
  someTimer._idleTimeout = 0;
});

timers._unrefActive(cancelsTimer);
timers._unrefActive(someTimer);
timers._unrefActive(endTest);

function makeTimer(msecs) {
  const timer = {};
  timers.unenroll(timer);
  timers.enroll(timer, msecs);
  return timer;
}
