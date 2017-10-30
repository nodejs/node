'use strict';
const common = require('../common');
const assert = require('assert');

const buffer = require('buffer');
const Buffer = buffer.Buffer;
const SlowBuffer = buffer.SlowBuffer;

const kMaxLength = buffer.kMaxLength;
const bufferMaxSizeMsg = common.expectsError({
  code: 'ERR_INVALID_OPT_VALUE',
  type: RangeError,
  message: /^The value "[^"]*" is invalid for option "size"$/
}, 12);

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
assert.throws(() => Buffer.allocUnsafe(0xFFFFFFFF), bufferMaxSizeMsg);
assert.throws(() => Buffer.allocUnsafe(0xFFFFFFFFF), bufferMaxSizeMsg);
