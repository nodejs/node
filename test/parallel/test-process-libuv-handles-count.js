'use strict';

const common = require('../common');
const assert = require('assert');

if (!common.isMainThread)
  common.skip('process.libuvHandlesCount is not available in Workers');

const count = process.libuvHandlesCount();

assert.strictEqual(typeof count.total, 'number');
assert.strictEqual(count.each.length, 18);

const sum = count.each.reduce((sum, c) => sum + c, 0);
assert.strictEqual(sum, count.total);
