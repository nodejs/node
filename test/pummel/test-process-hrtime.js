'use strict';
require('../common');
const assert = require('assert');

const start = process.hrtime();

// process.hrtime() should return an Array
assert(Array.isArray(start));

// busy-loop for 2 seconds
const now = Date.now();
while (Date.now() - now < 2000);

// get a diff reading
const diff = process.hrtime(start);

// should be at least 1 second, at most 2 seconds later
// (the while loop will usually exit a few nanoseconds before 2)
assert(diff[0] >= 1);
assert(diff[0] <= 2);
