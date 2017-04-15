'use strict';

const common = require('../common');
const assert = require('assert');
const SlowBuffer = require('buffer').SlowBuffer;

// Regression test for https://github.com/nodejs/node/issues/649.
const len = 1422561062959;
const message = common.bufferMaxSizeMsg;
assert.throws(() => Buffer(len).toString('utf8'), message);
assert.throws(() => SlowBuffer(len).toString('utf8'), message);
assert.throws(() => Buffer.alloc(len).toString('utf8'), message);
assert.throws(() => Buffer.allocUnsafe(len).toString('utf8'), message);
assert.throws(() => Buffer.allocUnsafeSlow(len).toString('utf8'), message);
