'use strict';

const {
  NumberIsInteger,
  NumberMAX_SAFE_INTEGER,
  NumberMIN_SAFE_INTEGER,
} = primordials;

const {
  hideStackFrames,
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_ARG_VALUE,
    ERR_INVALID_OPT_TYPE,
    ERR_INVALID_OPT_VALUE,
    ERR_OUT_OF_RANGE,
    ERR_UNKNOWN_SIGNAL
  }
} = require('internal/errors');
const { normalizeEncoding } = require('internal/util');
const {
  isArrayBufferView
} = require('internal/util/types');
const { signals } = internalBinding('constants').os;

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
function parseFileMode(value, name, def) {
  if (value == null && def !== undefined) {
    return def;
  }

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

  throw new ERR_INVALID_ARG_VALUE(name, value, modeDesc);
}

const handleValueName = (name) => {
  let TypeErr = ERR_INVALID_ARG_TYPE;
  let ValueErr = ERR_INVALID_ARG_VALUE;
  if (typeof name === 'object') {
    if (name.type === 'option') {
      TypeErr = ERR_INVALID_OPT_TYPE;
      ValueErr = ERR_INVALID_OPT_VALUE;
    }
    name = name.name;
  }
  return { valueName: name, TypeErr, ValueErr };
};

const validateInteger = hideStackFrames(
  (value, name, min = NumberMIN_SAFE_INTEGER, max = NumberMAX_SAFE_INTEGER) => {
    const { valueName, TypeErr } = handleValueName(name);
    if (typeof value !== 'number')
      throw new TypeErr(valueName, 'number', value);
    if (!NumberIsInteger(value))
      throw new ERR_OUT_OF_RANGE(valueName, 'an integer', value);
    if (value < min || value > max)
      throw new ERR_OUT_OF_RANGE(valueName, `>= ${min} && <= ${max}`, value);
  }
);

const validateInt32 = hideStackFrames(
  (value, name, min = -2147483648, max = 2147483647) => {
    // The defaults for min and max correspond to the limits of 32-bit integers.
    const { valueName, TypeErr } = handleValueName(name);
    if (!isInt32(value)) {
      if (typeof value !== 'number') {
        throw new TypeErr(valueName, 'number', value);
      }
      if (!NumberIsInteger(value)) {
        throw new ERR_OUT_OF_RANGE(valueName, 'an integer', value);
      }
      throw new ERR_OUT_OF_RANGE(valueName, `>= ${min} && <= ${max}`, value);
    }
    if (value < min || value > max) {
      throw new ERR_OUT_OF_RANGE(valueName, `>= ${min} && <= ${max}`, value);
    }
  }
);

const validateUint32 = hideStackFrames((value, name, positive) => {
  const { valueName, TypeErr } = handleValueName(name);
  if (!isUint32(value)) {
    if (typeof value !== 'number') {
      throw new TypeErr(valueName, 'number', value);
    }
    if (!NumberIsInteger(value)) {
      throw new ERR_OUT_OF_RANGE(valueName, 'an integer', value);
    }
    const min = positive ? 1 : 0;
    // 2 ** 32 === 4294967296
    throw new ERR_OUT_OF_RANGE(valueName, `>= ${min} && < 4294967296`, value);
  }
  if (positive && value === 0) {
    throw new ERR_OUT_OF_RANGE(valueName, '>= 1 && < 4294967296', value);
  }
});

function validateString(value, name) {
  const { valueName, TypeErr } = handleValueName(name);
  if (typeof value !== 'string')
    throw new TypeErr(valueName, 'string', value);
}

function validateNumber(value, name) {
  const { valueName, TypeErr } = handleValueName(name);
  if (typeof value !== 'number')
    throw new TypeErr(valueName, 'number', value);
}

function validateSignalName(signal, name = 'signal') {
  const { valueName, TypeErr } = handleValueName(name);
  if (typeof signal !== 'string')
    throw new TypeErr(valueName, 'string', signal);

  if (signals[signal] === undefined) {
    if (signals[signal.toUpperCase()] !== undefined) {
      throw new ERR_UNKNOWN_SIGNAL(signal +
                                   ' (signals must use all capital letters)');
    }

    throw new ERR_UNKNOWN_SIGNAL(signal);
  }
}

const validateBuffer = hideStackFrames((buffer, name = 'buffer') => {
  const { valueName, TypeErr } = handleValueName(name);
  if (!isArrayBufferView(buffer))
    throw new TypeErr(valueName, ['Buffer', 'TypedArray', 'DataView'], buffer);
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
  parseFileMode,
  validateBuffer,
  validateEncoding,
  validateInteger,
  validateInt32,
  validateUint32,
  validateString,
  validateNumber,
  validateSignalName
};
