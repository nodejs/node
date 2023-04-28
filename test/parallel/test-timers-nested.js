// Flags: --expose-internals
'use strict';

require('../common');
const assert = require('assert');
const { sleep } = require('internal/util');

// Make sure we test 0ms timers, since they would had always wanted to run on
// the current tick, and greater than 0ms timers, for scenarios where the
// outer timer takes longer to complete than the delay of the nested timer.
// Since the process of recreating this is identical regardless of the timer
// delay, these scenarios are in one test.
const scenarios = [0, 100];

scenarios.forEach(function(delay) {
  let nestedCalled = false;

  setTimeout(function A() {
    // Create the nested timer with the same delay as the outer timer so that it
    // gets added to the current list of timers being processed by
    // listOnTimeout.
    setTimeout(function B() {
      nestedCalled = true;
    }, delay);

    // Busy loop for the same timeout used for the nested timer to ensure that
    // we are in fact expiring the nested timer.
    sleep(delay);

    // The purpose of running this assert in nextTick is to make sure it runs
    // after A but before the next iteration of the libuv event loop.
    process.nextTick(function() {
      assert.ok(!nestedCalled);
    });

    // Ensure that the nested callback is indeed called prior to process exit.
    process.on('exit', function onExit() {
      assert.ok(nestedCalled);
    });
  }, delay);
});
