'use strict';
const common = require('../common');
const assert = require('assert');
const { getActiveTimers } = require('timers');

const inputs = [
  1,
  2,
  35,
  47,
  47,
  200,
  200,
  256,
];

// Setting timeouts and intervals using values from the inputs array.
// These timers should not be called.
inputs.forEach((value) => setTimeout(common.mustNotCall(), value));
inputs.forEach((value) => setInterval(common.mustNotCall(), value));

// Setting timers that should not be showing up when
// we use clearTimeout before calling getActiveTimers.
const clearedTimers = inputs.map((value) =>
  setTimeout(common.mustNotCall(), value));

clearedTimers.forEach(clearTimeout);

const activeTimers = getActiveTimers();

// Checking to see if any of the cleared timers are
// showing up in the getActiveTimers array.
// If any of them show up, the test will fail.
const allTimers = new Set([...activeTimers, ...clearedTimers]);
if (allTimers.size !== activeTimers.length + clearedTimers.length) {
  assert.fail('cleared timer should not be returned by getActiveTimers');
}

// Clearing all timers. If the test succeeds, the test will exit.
activeTimers.forEach(clearTimeout);
