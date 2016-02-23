'use strict';
/*
 * This test makes sure that non-integer timer delays do not make the process
 * hang. See https://github.com/joyent/node/issues/8065 and
 * https://github.com/joyent/node/issues/8068 which have been fixed by
 * https://github.com/joyent/node/pull/8073.
 *
 * If the process hangs, this test will make the tests suite timeout,
 * otherwise it will exit very quickly (after 50 timers with a short delay
 * fire).
 *
 * We have to set at least several timers with a non-integer delay to
 * reproduce the issue. Sometimes, a timer with a non-integer delay will
 * expire correctly. 50 timers has always been more than enough to reproduce
 * it 100%.
 */

require('../common');

var TIMEOUT_DELAY = 1.1;
var NB_TIMEOUTS_FIRED = 50;

var nbTimeoutFired = 0;
var interval = setInterval(function() {
  ++nbTimeoutFired;
  if (nbTimeoutFired === NB_TIMEOUTS_FIRED) {
    clearInterval(interval);
    process.exit(0);
  }
}, TIMEOUT_DELAY);
