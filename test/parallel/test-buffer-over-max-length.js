'use strict';
require('../common');

const assert = require('assert');

const buffer = require('buffer');

const kMaxLength = buffer.kMaxLength;
const bufferMaxSizeMsg = {
  code: 'ERR_OUT_OF_RANGE',
  name: 'RangeError',
};

assert.throws(() => Buffer(kMaxLength + 1), bufferMaxSizeMsg);
assert.throws(() => Buffer.alloc(kMaxLength + 1), bufferMaxSizeMsg);
assert.throws(() => Buffer.allocUnsafe(kMaxLength + 1), bufferMaxSizeMsg);
assert.throws(() => Buffer.allocUnsafeSlow(kMaxLength + 1), bufferMaxSizeMsg);
