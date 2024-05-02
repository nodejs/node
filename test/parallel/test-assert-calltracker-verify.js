'use strict';
require('../common');
const assert = require('assert');

// This test ensures that assert.CallTracker.verify() works as intended.

const tracker = new assert.CallTracker();

const generic_msg = 'Functions were not called the expected number of times';

function foo() {}

function bar() {}

const callsfoo = tracker.calls(foo, 1);
const callsbar = tracker.calls(bar, 1);

// Expects an error as callsfoo() and callsbar() were called less than one time.
assert.throws(
  () => tracker.verify(),
  { message: generic_msg }
);

callsfoo();

// Expects an error as callsbar() was called less than one time.
assert.throws(
  () => tracker.verify(),
  { message: 'Expected the bar function to be executed 1 time(s) but was executed 0 time(s).' }
);
callsbar();

// Will throw an error if callsfoo() and callsbar isn't called exactly once.
tracker.verify();

const callsfoobar = tracker.calls(foo, 1);

callsfoo();

// Expects an error as callsfoo() was called more than once and callsfoobar() was called less than one time.
assert.throws(
  () => tracker.verify(),
  { message: generic_msg }
);

callsfoobar();


// Expects an error as callsfoo() was called more than once
assert.throws(
  () => tracker.verify(),
  { message: 'Expected the foo function to be executed 1 time(s) but was executed 2 time(s).' }
);
