'use strict';

const {
  ArrayIsArray,
  ArrayPrototypeIncludes,
  ArrayPrototypeJoin,
  ArrayPrototypeMap,
  NumberIsInteger,
  NumberMAX_SAFE_INTEGER,
  NumberMIN_SAFE_INTEGER,
  NumberParseInt,
  RegExpPrototypeTest,
  String,
  StringPrototypeToUpperCase,
  StringPrototypeTrim,
} = primordials;

const {
  hideStackFrames,
  codes: {
    ERR_SOCKET_BAD_PORT,
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_ARG_VALUE,
    ERR_OUT_OF_RANGE,
    ERR_UNKNOWN_SIGNAL,
  }
} = require('internal/errors');
const { normalizeEncoding } = require('internal/util');
const {
  isAsyncFunction,
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
 * @param {number} [def] If specified, will be returned for invalid values
 * @returns {number}
 */
function parseFileMode(value, name, def) {
  value ??= def;
  if (typeof value === 'string') {
    if (!RegExpPrototypeTest(octalReg, value)) {
      throw new ERR_INVALID_ARG_VALUE(name, value, modeDesc);
    }
    value = NumberParseInt(value, 8);
  }

  validateUint32(value, name);
  return value;
}

const validateInteger = hideStackFrames(
  (value, name, min = NumberMIN_SAFE_INTEGER, max = NumberMAX_SAFE_INTEGER) => {
    if (typeof value !== 'number')
      throw new ERR_INVALID_ARG_TYPE(name, 'number', value);
    if (!NumberIsInteger(value))
      throw new ERR_OUT_OF_RANGE(name, 'an integer', value);
    if (value < min || value > max)
      throw new ERR_OUT_OF_RANGE(name, `>= ${min} && <= ${max}`, value);
  }
);

const validateInt32 = hideStackFrames(
  (value, name, min = -2147483648, max = 2147483647) => {
    // The defaults for min and max correspond to the limits of 32-bit integers.
    if (typeof value !== 'number') {
      throw new ERR_INVALID_ARG_TYPE(name, 'number', value);
    }
    if (!NumberIsInteger(value)) {
      throw new ERR_OUT_OF_RANGE(name, 'an integer', value);
    }
    if (value < min || value > max) {
      throw new ERR_OUT_OF_RANGE(name, `>= ${min} && <= ${max}`, value);
    }
  }
);

const validateUint32 = hideStackFrames((value, name, positive) => {
  if (typeof value !== 'number') {
    throw new ERR_INVALID_ARG_TYPE(name, 'number', value);
  }
  if (!NumberIsInteger(value)) {
    throw new ERR_OUT_OF_RANGE(name, 'an integer', value);
  }
  const min = positive ? 1 : 0;
  // 2 ** 32 === 4294967296
  const max = 4_294_967_295;
  if (value < min || value > max) {
    throw new ERR_OUT_OF_RANGE(name, `>= ${min} && <= ${max}`, value);
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

const validateOneOf = hideStackFrames((value, name, oneOf) => {
  if (!ArrayPrototypeIncludes(oneOf, value)) {
    const allowed = ArrayPrototypeJoin(
      ArrayPrototypeMap(oneOf, (v) =>
        (typeof v === 'string' ? `'${v}'` : String(v))),
      ', ');
    const reason = 'must be one of: ' + allowed;
    throw new ERR_INVALID_ARG_VALUE(name, value, reason);
  }
});

function validateBoolean(value, name) {
  if (typeof value !== 'boolean')
    throw new ERR_INVALID_ARG_TYPE(name, 'boolean', value);
}

/**
 * @param {unknown} value
 * @param {string} name
 * @param {{
 *   allowArray?: boolean,
 *   allowFunction?: boolean,
 *   nullable?: boolean
 * }} [options]
 */
const validateObject = hideStackFrames(
  (value, name, options) => {
    const useDefaultOptions = options == null;
    const allowArray = useDefaultOptions ? false : options.allowArray;
    const allowFunction = useDefaultOptions ? false : options.allowFunction;
    const nullable = useDefaultOptions ? false : options.nullable;
    if ((!nullable && value === null) ||
        (!allowArray && ArrayIsArray(value)) ||
        (typeof value !== 'object' && (
          !allowFunction || typeof value !== 'function'
        ))) {
      throw new ERR_INVALID_ARG_TYPE(name, 'Object', value);
    }
  });

const validateArray = hideStackFrames((value, name, minLength = 0) => {
  if (!ArrayIsArray(value)) {
    throw new ERR_INVALID_ARG_TYPE(name, 'Array', value);
  }
  if (value.length < minLength) {
    const reason = `must be longer than ${minLength}`;
    throw new ERR_INVALID_ARG_VALUE(name, value, reason);
  }
});

function validateSignalName(signal, name = 'signal') {
  validateString(signal, name);

  if (signals[signal] === undefined) {
    if (signals[StringPrototypeToUpperCase(signal)] !== undefined) {
      throw new ERR_UNKNOWN_SIGNAL(signal +
                                   ' (signals must use all capital letters)');
    }

    throw new ERR_UNKNOWN_SIGNAL(signal);
  }
}

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

// Check that the port number is not NaN when coerced to a number,
// is an integer and that it falls within the legal range of port numbers.
function validatePort(port, name = 'Port', allowZero = true) {
  if ((typeof port !== 'number' && typeof port !== 'string') ||
      (typeof port === 'string' && StringPrototypeTrim(port).length === 0) ||
      +port !== (+port >>> 0) ||
      port > 0xFFFF ||
      (port === 0 && !allowZero)) {
    throw new ERR_SOCKET_BAD_PORT(name, port, allowZero);
  }
  return port | 0;
}

const validateAbortSignal = hideStackFrames((signal, name) => {
  if (signal !== undefined &&
      (signal === null ||
       typeof signal !== 'object' ||
       !('aborted' in signal))) {
    throw new ERR_INVALID_ARG_TYPE(name, 'AbortSignal', signal);
  }
});

const validateFunction = hideStackFrames((value, name) => {
  if (typeof value !== 'function')
    throw new ERR_INVALID_ARG_TYPE(name, 'Function', value);
});

const validatePlainFunction = hideStackFrames((value, name) => {
  if (typeof value !== 'function' || isAsyncFunction(value))
    throw new ERR_INVALID_ARG_TYPE(name, 'Function', value);
});

const validateUndefined = hideStackFrames((value, name) => {
  if (value !== undefined)
    throw new ERR_INVALID_ARG_TYPE(name, 'undefined', value);
});

function validateUnion(value, name, union) {
  if (!ArrayPrototypeIncludes(union, value)) {
    throw new ERR_INVALID_ARG_TYPE(name, `('${ArrayPrototypeJoin(union, '|')}')`, value);
  }
}

module.exports = {
  isInt32,
  isUint32,
  parseFileMode,
  validateArray,
  validateBoolean,
  validateBuffer,
  validateEncoding,
  validateFunction,
  validateInt32,
  validateInteger,
  validateNumber,
  validateObject,
  validateOneOf,
  validatePlainFunction,
  validatePort,
  validateSignalName,
  validateString,
  validateUint32,
  validateUndefined,
  validateUnion,
  validateAbortSignal,
};
