// Flags: --expose-internals
'use strict';

require('../common');
const assert = require('assert');
const {
  validateArray,
  validateBoolean,
  validateInteger,
  validateNumber,
  validateString,
  validateObject,
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
  // validateNumber tests
  validateNumber(1, 'foo');
  validateNumber(1.1, 'foo');
  validateNumber(1, 'foo', { min: 0 });
  validateNumber(2, 'foo', { max: 2 });

  ['test', true, {}, [], null].forEach((i) => {
    assert.throws(() => validateNumber(i, 'foo'), invalidArgTypeError);
  });
  assert.throws(() => validateNumber(1, 'foo', { min: 2 }), outOfRangeError);
  assert.throws(() => validateNumber(2, 'foo', { max: 1 }), outOfRangeError);
}

{
  // validateString tests
  validateString('test', 'foo');
  validateString(undefined, 'foo', { allowUndefined: true });

  [1, NaN, null, undefined, {}, []].forEach((i) => {
    assert.throws(() => validateString(i, 'foo'), {
      code: 'ERR_INVALID_ARG_TYPE'
    });
  });
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
  validateBoolean(undefined, 'foo', { allowUndefined: true });

  [undefined, null, 0, 0.0, 42, '', 'string', {}, []].forEach((val) => {
    assert.throws(() => {
      validateBoolean(val, 'foo');
    }, invalidArgTypeError);
  });
}

{
  // validateObject tests.
  validateObject({}, 'foo');
  validateObject({ a: 42, b: 'foo' }, 'foo');
  validateObject(undefined, 'foo', { allowUndefined: true });

  [undefined, null, true, false, 0, 0.0, 42, '', 'string', []]
    .forEach((val) => {
      assert.throws(() => {
        validateObject(val, 'foo');
      }, invalidArgTypeError);
    });

  validateObject(null, 'foo', { nullable: true });
}
