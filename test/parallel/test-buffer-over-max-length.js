'use strict';
const common = require('../common');
const assert = require('assert');

const buffer = require('buffer');
const Buffer = buffer.Buffer;
const SlowBuffer = buffer.SlowBuffer;

const kMaxLength = buffer.kMaxLength;
const bufferMaxSizeMsg = common.bufferMaxSizeMsg;

assert.throws(() => { return Buffer((-1 >>> 0) + 1); }, bufferMaxSizeMsg);
assert.throws(() => { return SlowBuffer((-1 >>> 0) + 1); }, bufferMaxSizeMsg);
assert.throws(() => { return Buffer.alloc((-1 >>> 0) + 1); }, bufferMaxSizeMsg);
assert.throws(
  () => { return Buffer.allocUnsafe((-1 >>> 0) + 1); },
  bufferMaxSizeMsg
);
assert.throws(
  () => { return Buffer.allocUnsafeSlow((-1 >>> 0) + 1); },
  bufferMaxSizeMsg
);

assert.throws(() => { return Buffer(kMaxLength + 1); }, bufferMaxSizeMsg);
assert.throws(() => { return SlowBuffer(kMaxLength + 1); }, bufferMaxSizeMsg);
assert.throws(() => { return Buffer.alloc(kMaxLength + 1); }, bufferMaxSizeMsg);
assert.throws(
  () => { return Buffer.allocUnsafe(kMaxLength + 1); },
  bufferMaxSizeMsg
);
assert.throws(
  () => { return Buffer.allocUnsafeSlow(kMaxLength + 1); },
  bufferMaxSizeMsg
);

// issue GH-4331
assert.throws(
  () => { return Buffer.allocUnsafe(0xFFFFFFFF); },
  bufferMaxSizeMsg
);
assert.throws(
  () => { return Buffer.allocUnsafe(0xFFFFFFFFF); },
  bufferMaxSizeMsg
);
