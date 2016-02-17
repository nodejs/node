'use strict';

/*
 * This test is a regression test for joyent/node#8897.
 *
 * It tests some private implementation details that should not be
 * considered public interface.
 */
const common = require('../common');
const timers = require('timers');

const foo = {
  _onTimeout: common.fail
};

const bar = {
  _onTimeout: common.mustCall(function() {
    timers.unenroll(foo);
  })
};

// We use timers with expiration times that are sufficiently apart to make
// sure that they're not fired at the same time on platforms where the timer
// resolution is a bit coarse (e.g Windows with a default resolution of ~15ms).
timers.enroll(bar, 1);
timers._unrefActive(bar);

timers.enroll(foo, 50);
timers._unrefActive(foo);

// Keep the process open.
setTimeout(function() {}, 100);
