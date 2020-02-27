'use strict';
require('../common');
const assert = require('assert');

// This test ensures that the assert.CallTracker.report() works as intended.

const tracker = new assert.CallTracker();

function foo() {}

const callsfoo = tracker.calls(foo, 1);

// Ensures that foo was added to the callChecks array.
if (tracker.report()[0].operator !== 'foo') {
  process.exit(1);
}

callsfoo();

// Ensures that foo was removed from the callChecks array after being called the
// expected number of times.
if (typeof tracker.report()[0] === undefined) {
  process.exit(1);
}

callsfoo();

// Ensures that foo was added back to the callChecks array after being called
// more than the expected number of times.
if (tracker.report()[0].operator !== 'foo') {
  process.exit(1);
}
