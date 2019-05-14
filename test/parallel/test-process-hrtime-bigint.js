'use strict';

// Tests that process.hrtime.bigint() works.

require('../common');
const assert = require('assert');

const start = process.hrtime.bigint();
assert.strictEqual(typeof start, 'bigint');

const end = process.hrtime.bigint();
assert.strictEqual(typeof end, 'bigint');

assert(end - start >= 0n);
