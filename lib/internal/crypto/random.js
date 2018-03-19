'use strict';

const {
  ERR_INVALID_ARG_TYPE,
  ERR_INVALID_CALLBACK,
  ERR_OUT_OF_RANGE
} = require('internal/errors').codes;
const { isArrayBufferView } = require('internal/util/types');
const {
  randomBytes: _randomBytes,
  randomFill: _randomFill
} = process.binding('crypto');

const { kMaxLength } = require('buffer');
const kMaxUint32 = Math.pow(2, 32) - 1;
const kMaxPossibleLength = Math.min(kMaxLength, kMaxUint32);

function assertOffset(offset, elementSize, length) {
  if (typeof offset !== 'number') {
    throw new ERR_INVALID_ARG_TYPE('offset', 'number', offset);
  }

  offset *= elementSize;

  const maxLength = Math.min(length, kMaxPossibleLength);
  if (Number.isNaN(offset) || offset > maxLength || offset < 0) {
    throw new ERR_OUT_OF_RANGE('offset', `>= 0 && <= ${maxLength}`, offset);
  }

  return offset;
}

function assertSize(size, elementSize, offset, length) {
  if (typeof size !== 'number') {
    throw new ERR_INVALID_ARG_TYPE('size', 'number', size);
  }

  size *= elementSize;

  if (Number.isNaN(size) || size > kMaxPossibleLength || size < 0) {
    throw new ERR_OUT_OF_RANGE('size',
                               `>= 0 && <= ${kMaxPossibleLength}`, size);
  }

  if (size + offset > length) {
    throw new ERR_OUT_OF_RANGE('size + offset', `<= ${length}`, size + offset);
  }

  return size;
}

function randomBytes(size, cb) {
  assertSize(size, 1, 0, Infinity);
  if (cb !== undefined && typeof cb !== 'function')
    throw new ERR_INVALID_CALLBACK();
  return _randomBytes(size, cb);
}

function randomFillSync(buf, offset = 0, size) {
  if (!isArrayBufferView(buf)) {
    throw new ERR_INVALID_ARG_TYPE('buf', 'ArrayBufferView', buf);
  }

  const elementSize = buf.BYTES_PER_ELEMENT || 1;

  offset = assertOffset(offset, elementSize, buf.byteLength);

  if (size === undefined) {
    size = buf.byteLength - offset;
  } else {
    size = assertSize(size, elementSize, offset, buf.byteLength);
  }

  return _randomFill(buf, offset, size);
}

function randomFill(buf, offset, size, cb) {
  if (!isArrayBufferView(buf)) {
    throw new ERR_INVALID_ARG_TYPE('buf', 'ArrayBufferView', buf);
  }

  const elementSize = buf.BYTES_PER_ELEMENT || 1;

  if (typeof offset === 'function') {
    cb = offset;
    offset = 0;
    size = buf.bytesLength;
  } else if (typeof size === 'function') {
    cb = size;
    size = buf.byteLength - offset;
  } else if (typeof cb !== 'function') {
    throw new ERR_INVALID_CALLBACK();
  }

  offset = assertOffset(offset, elementSize, buf.byteLength);

  if (size === undefined) {
    size = buf.byteLength - offset;
  } else {
    size = assertSize(size, elementSize, offset, buf.byteLength);
  }

  return _randomFill(buf, offset, size, cb);
}

module.exports = {
  randomBytes,
  randomFill,
  randomFillSync
};
