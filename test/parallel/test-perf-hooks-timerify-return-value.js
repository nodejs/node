// Regression tests for https://github.com/nodejs/node/issues/40623
// Test that timerify preserves return values and class constructor behavior.

'use strict';

require('../common');
const assert = require('assert');
const { timerify } = require('perf_hooks');

assert.strictEqual(timerify(function func() {
  return 1;
})(), 1);
assert.strictEqual(timerify(function() {
  return 1;
})(), 1);
assert.strictEqual(timerify(() => {
  return 1;
})(), 1);
class C {}
const wrap = timerify(C);
assert.ok(new wrap() instanceof C);
assert.throws(() => wrap(), {
  name: 'TypeError',
});
