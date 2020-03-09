// Flags: --expose-internals
'use strict';

require('../common');
const assert = require('assert');
const { Buffer } = require('buffer');
const {
  validateArray,
  validateBoolean,
  validateBuffer,
  validateInteger,
  validateObject,
  validateString,
} = require('internal/validators');
const { MAX_SAFE_INTEGER, MIN_SAFE_INTEGER } = Number;
const outOfRangeError = {
  code: 'ERR_OUT_OF_RANGE',
  name: 'RangeError',
};
const invalidArgTypeError = {
  code: 'ERR_INVALID_ARG_TYPE',
  name: 'TypeError',
};
const invalidArgValueError = {
  code: 'ERR_INVALID_ARG_VALUE',
  name: 'TypeError',
};

{
  // validateString tests
  validateString('hi', 'foo');

  [1, false, [], {}, NaN, 1n, null, undefined].forEach((i) => {
    assert.throws(() => validateString(i, 'foo'), invalidArgTypeError);
  });

  validateString(undefined, 'foo', { allowUndefined: true });
}

{
  // validateBuffer tests
  validateBuffer(Buffer.from('hi'), 'foo');
  validateBuffer(Buffer.alloc(10), 'foo');
  validateBuffer(new Uint8Array(10), 'foo');
  validateBuffer(new DataView((new Uint8Array(10)).buffer));

  [1, false, '', {}, [], 1n, null, undefined].forEach((i) => {
    assert.throws(() => validateBuffer(i, 'foo'), invalidArgTypeError);
  });

  validateBuffer(undefined, 'foo', { allowUndefined: true });
}

{
  // validateInteger tests.

  // validateInteger() defaults to validating safe integers.
  validateInteger(MAX_SAFE_INTEGER, 'foo');
  validateInteger(MIN_SAFE_INTEGER, 'foo');
  assert.throws(() => {
    validateInteger(MAX_SAFE_INTEGER + 1, 'foo');
  }, outOfRangeError);
  assert.throws(() => {
    validateInteger(MIN_SAFE_INTEGER - 1, 'foo');
  }, outOfRangeError);

  // validateInteger() works with unsafe integers.
  validateInteger(
    MAX_SAFE_INTEGER + 1,
    'foo',
    { min: 0, max: MAX_SAFE_INTEGER + 1 });
  validateInteger(
    MIN_SAFE_INTEGER - 1,
    'foo',
    { min: MIN_SAFE_INTEGER - 1 });
  validateInteger(undefined, 'foo', { allowUndefined: true });
}

{
  // validateArray tests.
  validateArray([], 'foo');
  validateArray([1, 2, 3], 'foo');

  [undefined, null, true, false, 0, 0.0, 42, '', 'string', {}]
    .forEach((val) => {
      assert.throws(() => {
        validateArray(val, 'foo');
      }, invalidArgTypeError);
    });

  validateArray([1], 'foo', { minLength: 1 });
  assert.throws(() => {
    validateArray([], 'foo', { minLength: 1 });
  }, invalidArgValueError);
}

{
  // validateBoolean tests.
  validateBoolean(true, 'foo');
  validateBoolean(false, 'foo');

  [undefined, null, 0, 0.0, 42, '', 'string', {}, []].forEach((val) => {
    assert.throws(() => {
      validateBoolean(val, 'foo');
    }, invalidArgTypeError);
  });

  validateBoolean(undefined, 'foo', { allowUndefined: true });
}

{
  // validateObject tests.
  validateObject({}, 'foo');
  validateObject({ a: 42, b: 'foo' }, 'foo');

  [undefined, null, true, false, 0, 0.0, 42, '', 'string', []]
    .forEach((val) => {
      assert.throws(() => {
        validateObject(val, 'foo');
      }, invalidArgTypeError);
    });

  validateObject(null, 'foo', { nullable: true });
}
