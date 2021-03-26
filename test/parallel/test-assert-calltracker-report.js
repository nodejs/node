'use strict';
require('../common');
const assert = require('assert');

// This test ensures that the assert.CallTracker.report() works as intended.

const tracker = new assert.CallTracker();

function foo() {}

const callsfoo = tracker.calls(foo, 1);

// Ensures that foo was added to the callChecks array.
assert.strictEqual(tracker.report()[0].operator, 'foo');

callsfoo();

// Ensures that foo was removed from the callChecks array after being called the
// expected number of times.
assert.strictEqual(typeof tracker.report()[0], 'undefined');

callsfoo();

// Ensures that foo was added back to the callChecks array after being called
// more than the expected number of times.
assert.strictEqual(tracker.report()[0].operator, 'foo');
