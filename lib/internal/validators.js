'use strict';

const {
  ERR_INVALID_ARG_TYPE,
  ERR_INVALID_ARG_VALUE,
  ERR_OUT_OF_RANGE
} = require('internal/errors').codes;

function isInt32(value) {
  return value === (value | 0);
}

function isUint32(value) {
  return value === (value >>> 0);
}

const octalReg = /^[0-7]+$/;
const modeDesc = 'must be a 32-bit unsigned integer or an octal string';

/**
 * Validate values that will be converted into mode_t (the S_* constants).
 * Only valid numbers and octal strings are allowed. They could be converted
 * to 32-bit unsigned integers or non-negative signed integers in the C++
 * land, but any value higher than 0o777 will result in platform-specific
 * behaviors.
 *
 * @param {*} value Values to be validated
 * @param {string} name Name of the argument
 * @param {number} def If specified, will be returned for invalid values
 * @returns {number}
 */
function validateMode(value, name, def) {
  if (isUint32(value)) {
    return value;
  }

  if (typeof value === 'number') {
    if (!Number.isInteger(value)) {
      throw new ERR_OUT_OF_RANGE(name, 'an integer', value);
    } else {
      // 2 ** 32 === 4294967296
      throw new ERR_OUT_OF_RANGE(name, '>= 0 && < 4294967296', value);
    }
  }

  if (typeof value === 'string') {
    if (!octalReg.test(value)) {
      throw new ERR_INVALID_ARG_VALUE(name, value, modeDesc);
    }
    const parsed = parseInt(value, 8);
    return parsed;
  }

  // TODO(BridgeAR): Only return `def` in case `value == null`
  if (def !== undefined) {
    return def;
  }

  throw new ERR_INVALID_ARG_VALUE(name, value, modeDesc);
}

function validateInteger(value, name) {
  let err;

  if (typeof value !== 'number')
    err = new ERR_INVALID_ARG_TYPE(name, 'number', value);
  else if (!Number.isSafeInteger(value))
    err = new ERR_OUT_OF_RANGE(name, 'an integer', value);

  if (err) {
    Error.captureStackTrace(err, validateInteger);
    throw err;
  }

  return value;
}

function validateInt32(value, name, min = -2147483648, max = 2147483647) {
  // The defaults for min and max correspond to the limits of 32-bit integers.
  if (!isInt32(value)) {
    let err;
    if (typeof value !== 'number') {
      err = new ERR_INVALID_ARG_TYPE(name, 'number', value);
    } else if (!Number.isInteger(value)) {
      err = new ERR_OUT_OF_RANGE(name, 'an integer', value);
    } else {
      err = new ERR_OUT_OF_RANGE(name, `>= ${min} && <= ${max}`, value);
    }
    Error.captureStackTrace(err, validateInt32);
    throw err;
  } else if (value < min || value > max) {
    const err = new ERR_OUT_OF_RANGE(name, `>= ${min} && <= ${max}`, value);
    Error.captureStackTrace(err, validateInt32);
    throw err;
  }

  return value;
}

function validateUint32(value, name, positive) {
  if (!isUint32(value)) {
    let err;
    if (typeof value !== 'number') {
      err = new ERR_INVALID_ARG_TYPE(name, 'number', value);
    } else if (!Number.isInteger(value)) {
      err = new ERR_OUT_OF_RANGE(name, 'an integer', value);
    } else {
      const min = positive ? 1 : 0;
      // 2 ** 32 === 4294967296
      err = new ERR_OUT_OF_RANGE(name, `>= ${min} && < 4294967296`, value);
    }
    Error.captureStackTrace(err, validateUint32);
    throw err;
  } else if (positive && value === 0) {
    const err = new ERR_OUT_OF_RANGE(name, '>= 1 && < 4294967296', value);
    Error.captureStackTrace(err, validateUint32);
    throw err;
  }

  return value;
}

module.exports = {
  isInt32,
  isUint32,
  validateMode,
  validateInteger,
  validateInt32,
  validateUint32
};
