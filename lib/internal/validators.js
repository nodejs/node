'use strict';

const {
  hideStackFrames,
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_ARG_VALUE,
    ERR_OUT_OF_RANGE,
    ERR_UNKNOWN_SIGNAL
  }
} = require('internal/errors');
const { normalizeEncoding } = require('internal/util');
const {
  isArrayBufferView
} = require('internal/util/types');
const { signals } = internalBinding('constants').os;
const { MAX_SAFE_INTEGER, MIN_SAFE_INTEGER } = Number;

function isInt32(value) {
  return value === (value | 0);
}

function isUint32(value) {
  return value === (value >>> 0);
}

const octalReg = /^[0-7]+$/;
const modeDesc = 'must be a 32-bit unsigned integer or an octal string';

/**
 * Parse and validate values that will be converted into mode_t (the S_*
 * constants). Only valid numbers and octal strings are allowed. They could be
 * converted to 32-bit unsigned integers or non-negative signed integers in the
 * C++ land, but any value higher than 0o777 will result in platform-specific
 * behaviors.
 *
 * @param {*} value Values to be validated
 * @param {string} name Name of the argument
 * @param {number} def If specified, will be returned for invalid values
 * @returns {number}
 */
function parseMode(value, name, def) {
  if (isUint32(value)) {
    return value;
  }

  if (typeof value === 'number') {
    validateInt32(value, name, 0, 2 ** 32 - 1);
  }

  if (typeof value === 'string') {
    if (!octalReg.test(value)) {
      throw new ERR_INVALID_ARG_VALUE(name, value, modeDesc);
    }
    return parseInt(value, 8);
  }

  if (def !== undefined && value == null) {
    return def;
  }

  throw new ERR_INVALID_ARG_VALUE(name, value, modeDesc);
}

const validateInteger = hideStackFrames(
  (value, name, min = MIN_SAFE_INTEGER, max = MAX_SAFE_INTEGER) => {
    if (typeof value !== 'number')
      throw new ERR_INVALID_ARG_TYPE(name, 'number', value);
    if (!Number.isInteger(value))
      throw new ERR_OUT_OF_RANGE(name, 'an integer', value);
    if (value < min || value > max)
      throw new ERR_OUT_OF_RANGE(name, `>= ${min} && <= ${max}`, value);
  }
);

const validateInt32 = hideStackFrames(
  (value, name, min = -2147483648, max = 2147483647) => {
    // The defaults for min and max correspond to the limits of 32-bit integers.
    if (!isInt32(value)) {
      if (typeof value !== 'number') {
        throw new ERR_INVALID_ARG_TYPE(name, 'number', value);
      }
      if (!Number.isInteger(value)) {
        throw new ERR_OUT_OF_RANGE(name, 'an integer', value);
      }
      throw new ERR_OUT_OF_RANGE(name, `>= ${min} && <= ${max}`, value);
    }
    if (value < min || value > max) {
      throw new ERR_OUT_OF_RANGE(name, `>= ${min} && <= ${max}`, value);
    }
  }
);

const validateUint32 = hideStackFrames((value, name, positive) => {
  if (!isUint32(value)) {
    if (typeof value !== 'number') {
      throw new ERR_INVALID_ARG_TYPE(name, 'number', value);
    }
    if (!Number.isInteger(value)) {
      throw new ERR_OUT_OF_RANGE(name, 'an integer', value);
    }
    const min = positive ? 1 : 0;
    // 2 ** 32 === 4294967296
    throw new ERR_OUT_OF_RANGE(name, `>= ${min} && < 4294967296`, value);
  }
  if (positive && value === 0) {
    throw new ERR_OUT_OF_RANGE(name, '>= 1 && < 4294967296', value);
  }
});

function validateString(value, name) {
  if (typeof value !== 'string')
    throw new ERR_INVALID_ARG_TYPE(name, 'string', value);
}

function validateNumber(value, name) {
  if (typeof value !== 'number')
    throw new ERR_INVALID_ARG_TYPE(name, 'number', value);
}

function validateSignalName(signal, name = 'signal') {
  if (typeof signal !== 'string')
    throw new ERR_INVALID_ARG_TYPE(name, 'string', signal);

  if (signals[signal] === undefined) {
    if (signals[signal.toUpperCase()] !== undefined) {
      throw new ERR_UNKNOWN_SIGNAL(signal +
                                   ' (signals must use all capital letters)');
    }

    throw new ERR_UNKNOWN_SIGNAL(signal);
  }
}

// TODO(BridgeAR): We have multiple validation functions that call
// `require('internal/utils').toBuf()` before validating for array buffer views.
// Those should likely also be consolidated in here.
const validateBuffer = hideStackFrames((buffer, name = 'buffer') => {
  if (!isArrayBufferView(buffer)) {
    throw new ERR_INVALID_ARG_TYPE(name,
                                   ['Buffer', 'TypedArray', 'DataView'],
                                   buffer);
  }
});

function validateEncoding(data, encoding) {
  const normalizedEncoding = normalizeEncoding(encoding);
  const length = data.length;

  if (normalizedEncoding === 'hex' && length % 2 !== 0) {
    throw new ERR_INVALID_ARG_VALUE('encoding', encoding,
                                    `is invalid for data of length ${length}`);
  }
}

module.exports = {
  isInt32,
  isUint32,
  parseMode,
  validateBuffer,
  validateEncoding,
  validateInteger,
  validateInt32,
  validateUint32,
  validateString,
  validateNumber,
  validateSignalName
};
