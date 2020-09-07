'use strict';

const {
  MathMin,
  NumberIsNaN,
  NumberIsSafeInteger
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

function randomBytes(size, callback) {
  size = assertSize(size, 1, 0, Infinity);
  if (callback !== undefined && typeof callback !== 'function')
    throw new ERR_INVALID_CALLBACK(callback);

  const buf = new FastBuffer(size);

  if (!callback) return handleError(_randomBytes(buf, 0, size), buf);

  const wrap = new AsyncWrap(Providers.RANDOMBYTESREQUEST);
  wrap.ondone = (ex) => {  // Retains buf while request is in flight.
    if (ex) return callback.call(wrap, ex);
    callback.call(wrap, null, buf);
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

function randomFill(buf, offset, size, callback) {
  if (!isArrayBufferView(buf)) {
    throw new ERR_INVALID_ARG_TYPE('buf', 'ArrayBufferView', buf);
  }

  const elementSize = buf.BYTES_PER_ELEMENT || 1;

  if (typeof offset === 'function') {
    callback = offset;
    offset = 0;
    size = buf.bytesLength;
  } else if (typeof size === 'function') {
    callback = size;
    size = buf.byteLength - offset;
  } else if (typeof callback !== 'function') {
    throw new ERR_INVALID_CALLBACK(callback);
  }

  offset = assertOffset(offset, elementSize, buf.byteLength);

  if (size === undefined) {
    size = buf.byteLength - offset;
  } else {
    size = assertSize(size, elementSize, offset, buf.byteLength);
  }

  const wrap = new AsyncWrap(Providers.RANDOMBYTESREQUEST);
  wrap.ondone = (ex) => {  // Retains buf while request is in flight.
    if (ex) return callback.call(wrap, ex);
    callback.call(wrap, null, buf);
  };

  _randomBytes(buf, offset, size, wrap);
}

// Largest integer we can read from a buffer.
// e.g.: Buffer.from("ff".repeat(6), "hex").readUIntBE(0, 6);
const RAND_MAX = 0xFFFF_FFFF_FFFF;

// Generates an integer in [min, max) range where min is inclusive and max is
// exclusive.
function randomInt(min, max, callback) {
  // Detect optional min syntax
  // randomInt(max)
  // randomInt(max, callback)
  const minNotSpecified = typeof max === 'undefined' ||
    typeof max === 'function';

  if (minNotSpecified) {
    callback = max;
    max = min;
    min = 0;
  }

  const isSync = typeof callback === 'undefined';
  if (!isSync && typeof callback !== 'function') {
    throw new ERR_INVALID_CALLBACK(callback);
  }
  if (!NumberIsSafeInteger(min)) {
    throw new ERR_INVALID_ARG_TYPE('min', 'a safe integer', min);
  }
  if (!NumberIsSafeInteger(max)) {
    throw new ERR_INVALID_ARG_TYPE('max', 'a safe integer', max);
  }
  if (max <= min) {
    throw new ERR_OUT_OF_RANGE(
      'max', `greater than the value of "min" (${min})`, max
    );
  }

  // First we generate a random int between [0..range)
  const range = max - min;

  if (!(range <= RAND_MAX)) {
    throw new ERR_OUT_OF_RANGE(`max${minNotSpecified ? '' : ' - min'}`,
                               `<= ${RAND_MAX}`, range);
  }

  const excess = RAND_MAX % range;
  const randLimit = RAND_MAX - excess;

  if (isSync) {
    // Sync API
    while (true) {
      const x = randomBytes(6).readUIntBE(0, 6);
      // If x > (maxVal - (maxVal % range)), we will get "modulo bias"
      if (x > randLimit) {
        // Try again
        continue;
      }
      const n = (x % range) + min;
      return n;
    }
  } else {
    // Async API
    const pickAttempt = () => {
      randomBytes(6, (err, bytes) => {
        if (err) return callback(err);
        const x = bytes.readUIntBE(0, 6);
        // If x > (maxVal - (maxVal % range)), we will get "modulo bias"
        if (x > randLimit) {
          // Try again
          return pickAttempt();
        }
        const n = (x % range) + min;
        callback(null, n);
      });
    };

    pickAttempt();
  }
}

function handleError(ex, buf) {
  if (ex) throw ex;
  return buf;
}

module.exports = {
  randomBytes,
  randomFill,
  randomFillSync,
  randomInt
};
