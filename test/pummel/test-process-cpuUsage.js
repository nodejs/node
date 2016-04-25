'use strict';
require('../common');
const assert = require('assert');

const start = process.cpuUsage();

// process.cpuUsage() should return {user: aNumber, system: aNumber}
assert(start != null);
assert(Number.isFinite(start.user));
assert(Number.isFinite(start.system));

// Run a busy-loop for 2 seconds.
const now = Date.now();
while (Date.now() - now < 2000);

// Get a diff reading from when we started
const diff = process.cpuUsage(start);

const MICROSECONDS_PER_SECOND = 1000 * 1000;

// For a busy loop of 2 seconds, CPU usage should be > 0 but < 3 seconds.
assert(diff.user >= 0);
assert(diff.user <= 3 * MICROSECONDS_PER_SECOND);

assert(diff.system >= 0);
assert(diff.system <= 3 * MICROSECONDS_PER_SECOND);
