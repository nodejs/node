'use strict';
require('../common');
const assert = require('assert');

const start = process.cpuUsage();

// Run a busy-loop for specified # of milliseconds.
const RUN_FOR_MS = 500;

// Define slop factor for checking maximum expected diff values.
const SLOP_FACTOR = 2;

// Run a busy loop.
const now = Date.now();
while (Date.now() - now < RUN_FOR_MS);

// Get a diff reading from when we started.
const diff = process.cpuUsage(start);

const MICROSECONDS_PER_MILLISECOND = 1000;

// Diff usages should be >= 0, <= ~RUN_FOR_MS millis.
// Let's be generous with the slop factor, defined above, in case other things
// are happening on this CPU. The <= check may be invalid if the node process
// is making use of multiple CPUs, in which case, just remove it.
assert(diff.user >= 0);
assert(diff.user <= SLOP_FACTOR * RUN_FOR_MS * MICROSECONDS_PER_MILLISECOND);

assert(diff.system >= 0);
assert(diff.system <= SLOP_FACTOR * RUN_FOR_MS * MICROSECONDS_PER_MILLISECOND);
