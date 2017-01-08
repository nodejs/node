'use strict';
const common = require('../common');
const assert = require('assert');

const inputs = [
  undefined,
  null,
  true,
  false,
  '',
  [],
  {},
  NaN,
  +Infinity,
  -Infinity,
  (1.0 / 0.0),      // sanity check
  parseFloat('x'),  // NaN
  -10,
  -1,
  -0.5,
  -0.1,
  -0.0,
  0,
  0.0,
  0.1,
  0.5,
  1,
  1.0,
  2147483648,     // browser behaviour: timeouts > 2^31-1 run on next tick
  12345678901234  // ditto
];

const timeouts = [];
const intervals = [];

inputs.forEach(function(value, index) {
  setTimeout(function() {
    timeouts[index] = true;
  }, value);

  const handle = setInterval(function() {
    clearInterval(handle); // disarm timer or we'll never finish
    intervals[index] = true;
  }, value);
});

// All values in inputs array coerce to 1 ms. Therefore, they should all run
// before a timer set here for 2 ms.

setTimeout(common.mustCall(function() {
  // assert that all other timers have run
  inputs.forEach(function(value, index) {
    assert.strictEqual(true, timeouts[index]);
    assert.strictEqual(true, intervals[index]);
  });
}), 2);

// Test 10 ms timeout separately.
setTimeout(common.mustCall(function() {}), 10);
setInterval(common.mustCall(function() { clearInterval(this); }), 10);
