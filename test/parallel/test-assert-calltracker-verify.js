'use strict';
require('../common');
const assert = require('assert');

// This test ensures that assert.CallTracker.verify() works as intended.

const tracker = new assert.CallTracker();

const msg = 'Function(s) were not called the expected number of times';

function foo() {}

const callsfoo = tracker.calls(foo, 1);

// Expects an error as callsfoo() was called less than one time.
assert.throws(
  () => tracker.verify(),
  { message: msg }
);

callsfoo();

// Will throw an error if callsfoo() isn't called exactly once.
tracker.verify();

callsfoo();

// Expects an error as callsfoo() was called more than once.
assert.throws(
  () => tracker.verify(),
  { message: msg }
);
