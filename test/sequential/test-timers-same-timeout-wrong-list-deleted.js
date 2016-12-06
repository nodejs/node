'use strict';

/*
 * This is a regression test for https://github.com/nodejs/node/issues/7722.
 *
 * When nested timers have the same timeout, calling clearTimeout on the
 * older timer after it has fired causes the list the newer timer is in
 * to be deleted. Since the newer timer was not cleared, it still blocks
 * the event loop completing for the duration of its timeout, however, since
 * no reference exists to it in its list, it cannot be canceled and its
 * callback is not called when the timeout elapses.
 */

const common = require('../common');
const assert = require('assert');
const Timer = process.binding('timer_wrap').Timer;

const TIMEOUT = common.platformTimeout(100);
const start = Timer.now();

// This bug also prevents the erroneously dereferenced timer's callback
// from being called, so we can't use it's execution or lack thereof
// to assert that the bug is fixed.
process.on('exit', function() {
  const end = Timer.now();
  assert.equal(end - start < TIMEOUT * 2, true,
               'Elapsed time does not include second timer\'s timeout.');
});

const handle1 = setTimeout(common.mustCall(function() {
  // Cause the old TIMEOUT list to be deleted
  clearTimeout(handle1);

  // Cause a new list with the same key (TIMEOUT) to be created for this timer
  const handle2 = setTimeout(function() {
    common.fail('Inner callback is not called');
  }, TIMEOUT);

  setTimeout(common.mustCall(function() {
    // Attempt to cancel the second timer. Fix for this bug will keep the
    // newer timer from being dereferenced by keeping its list from being
    // erroneously deleted. If we are able to cancel the timer successfully,
    // the bug is fixed.
    clearTimeout(handle2);
    setImmediate(common.mustCall(function() {
      setImmediate(common.mustCall(function() {
        const activeHandles = process._getActiveHandles();
        const activeTimers = activeHandles.filter(function(handle) {
          return handle instanceof Timer;
        });

        // Make sure our clearTimeout succeeded. One timer finished and
        // the other was canceled, so none should be active.
        assert.equal(activeTimers.length, 0, 'No Timers remain.');
      }));
    }));
  }), 10);

  // Make sure our timers got added to the list.
  const activeHandles = process._getActiveHandles();
  const activeTimers = activeHandles.filter(function(handle) {
    return handle instanceof Timer;
  });
  const shortTimer = activeTimers.find(function(handle) {
    return handle._list.msecs === 10;
  });
  const longTimers = activeTimers.filter(function(handle) {
    return handle._list.msecs === TIMEOUT;
  });

  // Make sure our clearTimeout succeeded. One timer finished and
  // the other was canceled, so none should be active.
  assert.equal(activeTimers.length, 3, 'There are 3 timers in the list.');
  assert(shortTimer instanceof Timer, 'The shorter timer is in the list.');
  assert.equal(longTimers.length, 2, 'Both longer timers are in the list.');

  // When this callback completes, `listOnTimeout` should now look at the
  // correct list and refrain from removing the new TIMEOUT list which
  // contains the reference to the newer timer.
}), TIMEOUT);
