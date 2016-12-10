'use strict';

require('../common');
const assert = require('assert');
const SlowBuffer = require('buffer').SlowBuffer;

// Regression test for https://github.com/nodejs/node/issues/649.
const len = 1422561062959;
const lenLimitMsg = new RegExp('^RangeError: (Invalid typed array length' +
                               '|Array buffer allocation failed' +
                               '|Invalid array buffer length)$');

assert.throws(() => Buffer(len).toString('utf8'), lenLimitMsg);
assert.throws(() => SlowBuffer(len).toString('utf8'), lenLimitMsg);
assert.throws(() => Buffer.alloc(len).toString('utf8'), lenLimitMsg);
assert.throws(() => Buffer.allocUnsafe(len).toString('utf8'), lenLimitMsg);
assert.throws(() => Buffer.allocUnsafeSlow(len).toString('utf8'),
              lenLimitMsg);
