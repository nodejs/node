'use strict';

const {
  FunctionPrototypeBind,
  FunctionPrototypeCall,
  MathMin,
  NumberIsNaN,
  NumberIsSafeInteger,
} = primordials;

const {
  RandomBytesJob,
  kCryptoJobAsync,
  kCryptoJobSync,
} = internalBinding('crypto');

const {
  lazyDOMException,
} = require('internal/crypto/util');

const { kMaxLength } = require('buffer');

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_OUT_OF_RANGE,
  }
} = require('internal/errors');

const {
  validateNumber,
  validateCallback,
} = require('internal/validators');

const {
  isArrayBufferView,
  isAnyArrayBuffer,
  isBigInt64Array,
  isFloat32Array,
  isFloat64Array,
} = require('internal/util/types');

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
  if (callback !== undefined) {
    validateCallback(callback);
  }

  const buf = new FastBuffer(size);

  if (callback === undefined) {
    randomFillSync(buf.buffer, 0, size);
    return buf;
  }

  // Keep the callback as a regular function so this is propagated.
  randomFill(buf.buffer, 0, size, function(error) {
    if (error) FunctionPrototypeCall(callback, this, error);
    FunctionPrototypeCall(callback, this, null, buf);
  });
}

function randomFillSync(buf, offset = 0, size) {
  if (!isAnyArrayBuffer(buf) && !isArrayBufferView(buf)) {
    throw new ERR_INVALID_ARG_TYPE(
      'buf',
      ['ArrayBuffer', 'ArrayBufferView'],
      buf);
  }

  const elementSize = buf.BYTES_PER_ELEMENT || 1;

  offset = assertOffset(offset, elementSize, buf.byteLength);

  if (size === undefined) {
    size = buf.byteLength - offset;
  } else {
    size = assertSize(size, elementSize, offset, buf.byteLength);
  }

  if (size === 0)
    return buf;

  const job = new RandomBytesJob(
    kCryptoJobSync,
    buf,
    offset,
    size);

  const [ err ] = job.run();
  if (err)
    throw err;

  return buf;
}

function randomFill(buf, offset, size, callback) {
  if (!isAnyArrayBuffer(buf) && !isArrayBufferView(buf)) {
    throw new ERR_INVALID_ARG_TYPE(
      'buf',
      ['ArrayBuffer', 'ArrayBufferView'],
      buf);
  }

  const elementSize = buf.BYTES_PER_ELEMENT || 1;

  if (typeof offset === 'function') {
    callback = offset;
    offset = 0;
    size = buf.bytesLength;
  } else if (typeof size === 'function') {
    callback = size;
    size = buf.byteLength - offset;
  } else {
    validateCallback(callback);
  }

  offset = assertOffset(offset, elementSize, buf.byteLength);

  if (size === undefined) {
    size = buf.byteLength - offset;
  } else {
    size = assertSize(size, elementSize, offset, buf.byteLength);
  }

  if (size === 0) {
    callback(null, buf);
    return;
  }

  // TODO(@jasnell): This is not yet handling byte offsets right
  const job = new RandomBytesJob(
    kCryptoJobAsync,
    buf,
    offset,
    size);
  job.ondone = FunctionPrototypeBind(onJobDone, job, buf, callback);
  job.run();
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
  if (!isSync) {
    validateCallback(callback);
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


function onJobDone(buf, callback, error) {
  if (error) return FunctionPrototypeCall(callback, this, error);
  FunctionPrototypeCall(callback, this, null, buf);
}

// Really just the Web Crypto API alternative
// to require('crypto').randomFillSync() with an
// additional limitation that the input buffer is
// not allowed to exceed 65536 bytes, and can only
// be an integer-type TypedArray.
function getRandomValues(data) {
  if (!isArrayBufferView(data) ||
      isBigInt64Array(data) ||
      isFloat32Array(data) ||
      isFloat64Array(data)) {
    // Ordinarily this would be an ERR_INVALID_ARG_TYPE. However,
    // the Web Crypto API and web platform tests expect this to
    // be a DOMException with type TypeMismatchError.
    throw lazyDOMException(
      'The data argument must be an integer-type TypedArray',
      'TypeMismatchError');
  }
  if (data.byteLength > 65536) {
    throw lazyDOMException(
      'The requested length exceeds 65,536 bytes',
      'QuotaExceededError');
  }
  randomFillSync(data, 0);
  return data;
}

module.exports = {
  randomBytes,
  randomFill,
  randomFillSync,
  randomInt,
  getRandomValues,
};
