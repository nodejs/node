'use strict';

const { AsyncWrap, Providers } = process.binding('async_wrap');
const { Buffer } = require('buffer');
const { randomBytes: _randomBytes } = process.binding('crypto');
const {
  ERR_INVALID_ARG_TYPE,
  ERR_INVALID_CALLBACK,
  ERR_OUT_OF_RANGE
} = require('internal/errors').codes;
const { isArrayBufferView } = require('internal/util/types');

const { kMaxLength } = require('buffer');
const kMaxUint32 = 2 ** 32 - 1;
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

  return offset >>> 0;  // Convert to uint32.
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

  return size >>> 0;  // Convert to uint32.
}

function randomBytes(size, cb) {
  size = assertSize(size, 1, 0, Infinity);
  if (cb !== undefined && typeof cb !== 'function')
    throw new ERR_INVALID_CALLBACK();

  const buf = Buffer.alloc(size);

  if (!cb) return handleError(buf, 0, size);

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

  return handleError(buf, offset, size);
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

  const wrap = new AsyncWrap(Providers.RANDOMBYTESREQUEST);
  wrap.ondone = (ex) => {  // Retains buf while request is in flight.
    if (ex) return cb.call(wrap, ex);
    cb.call(wrap, null, buf);
  };

  _randomBytes(buf, offset, size, wrap);
}

function handleError(buf, offset, size) {
  const ex = _randomBytes(buf, offset, size);
  if (ex) throw ex;
  return buf;
}

module.exports = {
  randomBytes,
  randomFill,
  randomFillSync
};
