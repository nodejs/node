'use strict';
require('../common');
var assert = require('assert');

var start = process.cpuUsage();

// process.cpuUsage() should return {user: [int, int], system: [int, int]}
assert(start != null);
assert(Array.isArray(start.user));
assert(Array.isArray(start.system));

// busy-loop for 2 seconds
var now = Date.now();
while (Date.now() - now < 2000);

// get a diff reading
var diff = process.cpuUsage(start);

// user should be at least 1 second, at most 2 seconds later
// (the while loop will usually exit a few nanoseconds before 2)
assert(diff.user[0] >= 1);
assert(diff.user[0] <= 2);

// hard to tell what the system number should be, but at least > 0, < 2
assert(diff.system[0] >= 0);
assert(diff.system[0] <= 2);

// make sure nanosecond value is not negative
assert(diff.user[1] >= 0);
assert(diff.system[1] >= 0);
