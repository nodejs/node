'use strict';
const common = require('../common');
const assert = require('assert');

const now = Date.now();
const diffFromNow = now - process.startTime;

// Make it a bit flexible
assert.ok(diffFromNow > 0);
assert.ok(diffFromNow < common.platformTimeout(100));
