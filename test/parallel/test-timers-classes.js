'use strict';
require('../common');
const assert = require('assert');

const timers = require('timers');

const immediate = setImmediate(() => {});
const timeout = setTimeout(() => {}, 0);
const interval = setInterval(() => {}, 0);

assert(immediate instanceof timers.Immediate);
assert(timeout instanceof timers.Timeout);
assert(interval instanceof timers.Timeout);

// Clean up
clearImmediate(immediate);
clearTimeout(timeout);
clearInterval(interval);
