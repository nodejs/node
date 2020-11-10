'use strict';
require('../common');
const assert = require('assert');

const buffer = require('buffer');
const SlowBuffer = buffer.SlowBuffer;

const kMaxLength = buffer.kMaxLength;
const bufferMaxSizeMsg = {
  code: 'ERR_INVALID_OPT_VALUE',
  name: 'RangeError',
  message: /^The value "[^"]*" is invalid for option "size"$/
};

assert.throws(() => Buffer((-1 >>> 0) + 1), bufferMaxSizeMsg);
assert.throws(() => SlowBuffer((-1 >>> 0) + 1), bufferMaxSizeMsg);
assert.throws(() => Buffer.alloc((-1 >>> 0) + 1), bufferMaxSizeMsg);
assert.throws(() => Buffer.allocUnsafe((-1 >>> 0) + 1), bufferMaxSizeMsg);
assert.throws(() => Buffer.allocUnsafeSlow((-1 >>> 0) + 1), bufferMaxSizeMsg);

assert.throws(() => Buffer(kMaxLength + 1), bufferMaxSizeMsg);
assert.throws(() => SlowBuffer(kMaxLength + 1), bufferMaxSizeMsg);
assert.throws(() => Buffer.alloc(kMaxLength + 1), bufferMaxSizeMsg);
assert.throws(() => Buffer.allocUnsafe(kMaxLength + 1), bufferMaxSizeMsg);
assert.throws(() => Buffer.allocUnsafeSlow(kMaxLength + 1), bufferMaxSizeMsg);

// issue GH-4331
assert.throws(() => Buffer.allocUnsafe(0x100000000), bufferMaxSizeMsg);
assert.throws(() => Buffer.allocUnsafe(0xFFFFFFFFF), bufferMaxSizeMsg);
