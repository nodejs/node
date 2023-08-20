'use strict';

// This is a regression test for https://github.com/nodejs/node/issues/7722.
//
// When nested timers have the same timeout, calling clearTimeout on the
// older timer after it has fired causes the list the newer timer is in
// to be deleted. Since the newer timer was not cleared, it still blocks
// the event loop completing for the duration of its timeout, however, since
// no reference exists to it in its list, it cannot be canceled and its
// callback is not called when the timeout elapses.

const common = require('../common');

const TIMEOUT = common.platformTimeout(100);

const handle1 = setTimeout(common.mustCall(function() {
  // Cause the old TIMEOUT list to be deleted
  clearTimeout(handle1);

  // Cause a new list with the same key (TIMEOUT) to be created for this timer
  const handle2 = setTimeout(common.mustNotCall(), TIMEOUT);

  setTimeout(common.mustCall(function() {
    // Attempt to cancel the second timer. Fix for this bug will keep the
    // newer timer from being dereferenced by keeping its list from being
    // erroneously deleted. If we are able to cancel the timer successfully,
    // the bug is fixed.
    clearTimeout(handle2);
  }), 1);

  // When this callback completes, `listOnTimeout` should now look at the
  // correct list and refrain from removing the new TIMEOUT list which
  // contains the reference to the newer timer.
}), TIMEOUT);
