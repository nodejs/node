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
// Validator for mode_t (the S_* constants). Valid numbers or octal strings
// will be masked with 0o777 to be consistent with the behavior in POSIX APIs.
function validateAndMaskMode(value, name, def) {
  if (isUint32(value)) {
    return value & 0o777;
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
    return parsed & 0o777;
  }

  // TODO(BridgeAR): Only return `def` in case `value == null`
  if (def !== undefined) {
    return def;
  }

  throw new ERR_INVALID_ARG_VALUE(name, value, modeDesc);
}

function validateInt32(value, name) {
  if (!isInt32(value)) {
    let err;
    if (typeof value !== 'number') {
      err = new ERR_INVALID_ARG_TYPE(name, 'number', value);
    } else if (!Number.isInteger(value)) {
      err = new ERR_OUT_OF_RANGE(name, 'an integer', value);
    } else {
      // 2 ** 31 === 2147483648
      err = new ERR_OUT_OF_RANGE(name, '> -2147483649 && < 2147483648', value);
    }
    Error.captureStackTrace(err, validateInt32);
    throw err;
  }
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
}

module.exports = {
  isInt32,
  isUint32,
  validateAndMaskMode,
  validateInt32,
  validateUint32
};
