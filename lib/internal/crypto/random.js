'use strict';

const {
  MathMin,
  NumberIsNaN,
} = primordials;

const { AsyncWrap, Providers } = internalBinding('async_wrap');
const { kMaxLength } = require('buffer');
const { randomBytes: _randomBytes } = internalBinding('crypto');
const {
  ERR_INVALID_ARG_TYPE,
  ERR_INVALID_CALLBACK,
  ERR_OUT_OF_RANGE
} = require('internal/errors').codes;
const { validateNumber } = require('internal/validators');
const { isArrayBufferView } = require('internal/util/types');
const { FastBuffer } = require('internal/buffer');

const kMaxUint32 = 2 ** 32 - 1;
const kMaxPossibleLength = MathMin(kMaxLength, kMaxUint32);

function assertOffset(offset, elementSize, length) {
  validateNumber(offset, 'offset');
  offset *= elementSize;

  const maxLength = MathMin(length, kMaxPossibleLength);
  if (NumberIsNaN(offset) || offset > maxLength || offset < 0) {
    throw new ERR_OUT_OF_RANGE('offset', `>= 0 && <= ${maxLength}`, offset);
  }

  return offset >>> 0;  // Convert to uint32.
}

function assertSize(size, elementSize, offset, length) {
  validateNumber(size, 'size');
  size *= elementSize;

  if (NumberIsNaN(size) || size > kMaxPossibleLength || size < 0) {
    throw new ERR_OUT_OF_RANGE('size',
                               `>= 0 && <= ${kMaxPossibleLength}`, size);
  }

  if (size + offset > length) {
    throw new ERR_OUT_OF_RANGE('size + offset', `<= ${length}`, size + offset);
  }

  return size >>> 0;  // Convert to uint32.
}

function randomBytes(size, cb) {
  size = assertSize(size, 1, 0, Infinity);
  if (cb !== undefined && typeof cb !== 'function')
    throw new ERR_INVALID_CALLBACK(cb);

  const buf = new FastBuffer(size);

  if (!cb) return handleError(_randomBytes(buf, 0, size), buf);

  const wrap = new AsyncWrap(Providers.RANDOMBYTESREQUEST);
  wrap.ondone = (ex) => {  // Retains buf while request is in flight.
    if (ex) return cb.call(wrap, ex);
    cb.call(wrap, null, buf);
  };

  _randomBytes(buf, 0, size, wrap);
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

  return handleError(_randomBytes(buf, offset, size), buf);
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
    throw new ERR_INVALID_CALLBACK(cb);
  }

  offset = assertOffset(offset, elementSize, buf.byteLength);

  if (size === undefined) {
    size = buf.byteLength - offset;
  } else {
    size = assertSize(size, elementSize, offset, buf.byteLength);
  }

  const wrap = new AsyncWrap(Providers.RANDOMBYTESREQUEST);
  wrap.ondone = (ex) => {  // Retains buf while request is in flight.
    if (ex) return cb.call(wrap, ex);
    cb.call(wrap, null, buf);
  };

  _randomBytes(buf, offset, size, wrap);
}

function handleError(ex, buf) {
  if (ex) throw ex;
  return buf;
}

module.exports = {
  randomBytes,
  randomFill,
  randomFillSync
};
