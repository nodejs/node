'use strict';

const {
  MathMin,
  NumberIsNaN,
  NumberIsSafeInteger
} = primordials;

const { AsyncWrap, Providers } = internalBinding('async_wrap');
const {
  Buffer,
  kMaxLength,
} = require('buffer');
const {
  randomBytes: _randomBytes,
  secureBuffer,
} = internalBinding('crypto');
const {
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_CALLBACK,
    ERR_OUT_OF_RANGE,
    ERR_OPERATION_FAILED,
  }
} = require('internal/errors');
const {
  validateBoolean,
  validateNumber,
  validateObject,
} = require('internal/validators');
const { isArrayBufferView } = require('internal/util/types');
const { FastBuffer } = require('internal/buffer');

const kMaxInt32 = 2 ** 31 - 1;
const kMaxPossibleLength = MathMin(kMaxLength, kMaxInt32);

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

  // For (x % range) to produce an unbiased value greater than or equal to 0 and
  // less than range, x must be drawn randomly from the set of integers greater
  // than or equal to 0 and less than randLimit.
  const randLimit = RAND_MAX - (RAND_MAX % range);

  if (isSync) {
    // Sync API
    while (true) {
      const x = randomBytes(6).readUIntBE(0, 6);
      if (x >= randLimit) {
        // Try again.
        continue;
      }
      return (x % range) + min;
    }
  } else {
    // Async API
    const pickAttempt = () => {
      randomBytes(6, (err, bytes) => {
        if (err) return callback(err);
        const x = bytes.readUIntBE(0, 6);
        if (x >= randLimit) {
          // Try again.
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

// Implements an RFC 4122 version 4 random UUID.
// To improve performance, random data is generated in batches
// large enough to cover kBatchSize UUID's at a time. The uuidData
// and uuid buffers are reused. Each call to randomUUID() consumes
// 16 bytes from the buffer.

const kHexDigits = [
  48, 49, 50, 51, 52, 53, 54, 55,
  56, 57, 97, 98, 99, 100, 101, 102,
];

const kBatchSize = 128;
let uuidData;
let uuidNotBuffered;
let uuid;
let uuidBatch = 0;

function getBufferedUUID() {
  if (uuidData === undefined) {
    uuidData = secureBuffer(16 * kBatchSize);
    if (uuidData === undefined)
      throw new ERR_OPERATION_FAILED('Out of memory');
  }

  if (uuidBatch === 0) randomFillSync(uuidData);
  uuidBatch = (uuidBatch + 1) % kBatchSize;
  return uuidData.slice(uuidBatch * 16, (uuidBatch * 16) + 16);
}

function randomUUID(options) {
  if (options !== undefined)
    validateObject(options, 'options');
  const {
    disableEntropyCache = false,
  } = { ...options };

  validateBoolean(disableEntropyCache, 'options.disableEntropyCache');

  if (uuid === undefined) {
    uuid = Buffer.alloc(36, '-');
    uuid[14] = 52; // '4', identifies the UUID version
  }

  let uuidBuf;
  if (!disableEntropyCache) {
    uuidBuf = getBufferedUUID();
  } else {
    uuidBuf = uuidNotBuffered;
    if (uuidBuf === undefined)
      uuidBuf = uuidNotBuffered = secureBuffer(16);
    if (uuidBuf === undefined)
      throw new ERR_OPERATION_FAILED('Out of memory');
    randomFillSync(uuidBuf);
  }

  // Variant byte: 10xxxxxx (variant 1)
  uuidBuf[8] = (uuidBuf[8] & 0x3f) | 0x80;

  // This function is structured the way it is for performance.
  // The uuid buffer stores the serialization of the random
  // bytes from uuidData.
  // xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
  let n = 0;
  uuid[0] = kHexDigits[uuidBuf[n] >> 4];
  uuid[1] = kHexDigits[uuidBuf[n++] & 0xf];
  uuid[2] = kHexDigits[uuidBuf[n] >> 4];
  uuid[3] = kHexDigits[uuidBuf[n++] & 0xf];
  uuid[4] = kHexDigits[uuidBuf[n] >> 4];
  uuid[5] = kHexDigits[uuidBuf[n++] & 0xf];
  uuid[6] = kHexDigits[uuidBuf[n] >> 4];
  uuid[7] = kHexDigits[uuidBuf[n++] & 0xf];
  // -
  uuid[9] = kHexDigits[uuidBuf[n] >> 4];
  uuid[10] = kHexDigits[uuidBuf[n++] & 0xf];
  uuid[11] = kHexDigits[uuidBuf[n] >> 4];
  uuid[12] = kHexDigits[uuidBuf[n++] & 0xf];
  // -
  // 4, uuid[14] is set already...
  uuid[15] = kHexDigits[uuidBuf[n++] & 0xf];
  uuid[16] = kHexDigits[uuidBuf[n] >> 4];
  uuid[17] = kHexDigits[uuidBuf[n++] & 0xf];
  // -
  uuid[19] = kHexDigits[uuidBuf[n] >> 4];
  uuid[20] = kHexDigits[uuidBuf[n++] & 0xf];
  uuid[21] = kHexDigits[uuidBuf[n] >> 4];
  uuid[22] = kHexDigits[uuidBuf[n++] & 0xf];
  // -
  uuid[24] = kHexDigits[uuidBuf[n] >> 4];
  uuid[25] = kHexDigits[uuidBuf[n++] & 0xf];
  uuid[26] = kHexDigits[uuidBuf[n] >> 4];
  uuid[27] = kHexDigits[uuidBuf[n++] & 0xf];
  uuid[28] = kHexDigits[uuidBuf[n] >> 4];
  uuid[29] = kHexDigits[uuidBuf[n++] & 0xf];
  uuid[30] = kHexDigits[uuidBuf[n] >> 4];
  uuid[31] = kHexDigits[uuidBuf[n++] & 0xf];
  uuid[32] = kHexDigits[uuidBuf[n] >> 4];
  uuid[33] = kHexDigits[uuidBuf[n++] & 0xf];
  uuid[34] = kHexDigits[uuidBuf[n] >> 4];
  uuid[35] = kHexDigits[uuidBuf[n] & 0xf];

  return uuid.latin1Slice(0, 36);
}

module.exports = {
  randomBytes,
  randomFill,
  randomFillSync,
  randomInt,
  randomUUID,
};
