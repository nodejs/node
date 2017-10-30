'use strict';

const errors = require('internal/errors');
const { isArrayBufferView } = require('internal/util/types');
const {
  randomBytes: _randomBytes,
  randomFill: _randomFill
} = process.binding('crypto');

const { kMaxLength } = require('buffer');
const kMaxUint32 = Math.pow(2, 32) - 1;

function assertOffset(offset, length) {
  if (typeof offset !== 'number' || offset !== offset) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'offset', 'number');
  }

  if (offset > kMaxUint32 || offset < 0) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'offset', 'uint32');
  }

  if (offset > kMaxLength || offset > length) {
    throw new errors.RangeError('ERR_OUT_OF_RANGE', 'offset');
  }
}

function assertSize(size, offset = 0, length = Infinity) {
  if (typeof size !== 'number' || size !== size) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'size', 'number');
  }

  if (size > kMaxUint32 || size < 0) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE', 'size', 'uint32');
  }

  if (size + offset > length || size > kMaxLength) {
    throw new errors.RangeError('ERR_OUT_OF_RANGE', 'size');
  }
}

function randomBytes(size, cb) {
  assertSize(size);
  if (cb !== undefined && typeof cb !== 'function')
    throw new errors.TypeError('ERR_INVALID_CALLBACK');
  return _randomBytes(size, cb);
}

function randomFillSync(buf, offset = 0, size) {
  if (!isArrayBufferView(buf)) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE',
                               'buf', 'ArrayBufferView');
  }

  const elementSize = buf.BYTES_PER_ELEMENT || 1;

  offset *= elementSize;
  assertOffset(offset, buf.byteLength);

  if (size === undefined) {
    size = buf.byteLength - offset;
  } else {
    size *= elementSize;
  }

  assertSize(size, offset, buf.byteLength);

  return _randomFill(buf, offset, size);
}

function randomFill(buf, offset, size, cb) {
  if (!isArrayBufferView(buf)) {
    throw new errors.TypeError('ERR_INVALID_ARG_TYPE',
                               'buf', 'ArrayBufferView');
  }

  const elementSize = buf.BYTES_PER_ELEMENT || 1;

  if (typeof offset === 'function') {
    cb = offset;
    offset = 0;
    size = buf.bytesLength;
  } else if (typeof size === 'function') {
    cb = size;
    offset *= elementSize;
    size = buf.byteLength - offset;
  } else if (typeof cb !== 'function') {
    throw new errors.TypeError('ERR_INVALID_CALLBACK');
  }
  if (size === undefined) {
    size = buf.byteLength - offset;
  } else {
    size *= elementSize;
  }

  assertOffset(offset, buf.byteLength);
  assertSize(size, offset, buf.byteLength);

  return _randomFill(buf, offset, size, cb);
}

module.exports = {
  randomBytes,
  randomFill,
  randomFillSync
};
