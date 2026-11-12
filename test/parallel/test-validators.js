// Flags: --expose-internals
'use strict';

require('../common');
const assert = require('assert');
const {
  validateArray,
  validateBigIntOrInt,
  validateBoolean,
  validateInteger,
  validateNumber,
  validateObject,
  kValidateObjectAllowNullable,
  kValidateObjectAllowArray,
  kValidateObjectAllowFunction,
  validateString,
  validateInt32,
  validateUint32,
  validateLinkHeaderValue,
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
  validateInteger(MAX_SAFE_INTEGER + 1, 'foo', 0, MAX_SAFE_INTEGER + 1);
  validateInteger(MIN_SAFE_INTEGER - 1, 'foo', MIN_SAFE_INTEGER - 1);

  // validateBigIntOrInt() accepts bigints and integer numbers within the
  // bigint range, returning a bigint.
  assert.strictEqual(validateBigIntOrInt(0n, 'foo', 0n, 10n), 0n);
  assert.strictEqual(validateBigIntOrInt(10, 'foo', 0n, 10n), 10n);
  assert.strictEqual(validateBigIntOrInt(-5, 'foo', -10n, 10n), -5n);
  assert.strictEqual(
    validateBigIntOrInt(2n ** 64n - 1n, 'foo', 0n, 2n ** 64n - 1n),
    2n ** 64n - 1n);
  [
    Symbol(), {}, [], false, true, undefined, null, () => {}, '', '1',
  ].forEach((val) => assert.throws(
    () => validateBigIntOrInt(val, 'foo', 0n, 10n), invalidArgTypeError));
  [
    -1, 11, -1n, 11n, 1.5, NaN, Infinity,
  ].forEach((val) => assert.throws(
    () => validateBigIntOrInt(val, 'foo', 0n, 10n), outOfRangeError));

  // validateInt32() and validateUint32()
  [
    Symbol(), 1n, {}, [], false, true, undefined, null, () => {}, '', '1',
  ].forEach((val) => assert.throws(() => validateInt32(val, 'name'), {
    code: 'ERR_INVALID_ARG_TYPE'
  }));
  [
    2147483647 + 1, -2147483648 - 1, NaN,
  ].forEach((val) => assert.throws(() => validateInt32(val, 'name'), {
    code: 'ERR_OUT_OF_RANGE'
  }));
  [
    0, 1, -1,
  ].forEach((val) => validateInt32(val, 'name'));
  [
    Symbol(), 1n, {}, [], false, true, undefined, null, () => {}, '', '1',
  ].forEach((val) => assert.throws(() => validateUint32(val, 'name'), {
    code: 'ERR_INVALID_ARG_TYPE'
  }));
  [
    4294967296, -1, NaN,
  ].forEach((val) => assert.throws(() => validateUint32(val, 'name'), {
    code: 'ERR_OUT_OF_RANGE'
  }));
  [
    0, 1,
  ].forEach((val) => validateUint32(val, 'name'));
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

  validateArray([1], 'foo', 1);
  assert.throws(() => {
    validateArray([], 'foo', 1);
  }, invalidArgValueError);

  validateArray([1, 2, 3], 'foo', 3);
  assert.throws(() => {
    validateArray([1, 2], 'foo', 3);
  }, (err) => {
    assert.ok(err.message.includes('at least 3'), `Expected "at least 3" in: ${err.message}`);
    return true;
  });
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
}

{
  // validateObject tests.
  validateObject({}, 'foo');
  validateObject({ a: 42, b: 'foo' }, 'foo');

  [undefined, null, true, false, 0, 0.0, 42, '', 'string', [], () => {}]
    .forEach((val) => {
      assert.throws(() => {
        validateObject(val, 'foo');
      }, invalidArgTypeError);
    });

  // validateObject options tests:
  validateObject(null, 'foo', kValidateObjectAllowNullable);
  validateObject([], 'foo', kValidateObjectAllowArray);
  validateObject(() => {}, 'foo', kValidateObjectAllowFunction);

  // validateObject should not be affected by Object.prototype tampering.
  assert.throws(() => validateObject(null, 'foo', kValidateObjectAllowArray), invalidArgTypeError);
  assert.throws(() => validateObject([], 'foo', kValidateObjectAllowNullable), invalidArgTypeError);
  assert.throws(() => validateObject(() => {}, 'foo', kValidateObjectAllowNullable), invalidArgTypeError);
}

{
  // validateString type validation.
  [
    -1, {}, [], false, true,
    1, Infinity, -Infinity, NaN,
    undefined, null, 1.1,
  ].forEach((i) => assert.throws(() => validateString(i, 'name'), {
    code: 'ERR_INVALID_ARG_TYPE'
  }));
}
{
  // validateNumber type validation.
  [
    'a', {}, [], false, true,
    undefined, null, '', ' ', '0x',
    '-0x1', '-0o1', '-0b1', '0o', '0b',
  ].forEach((i) => assert.throws(() => validateNumber(i, 'name'), {
    code: 'ERR_INVALID_ARG_TYPE'
  }));
}

{
  // validateLinkHeaderValue type validation.
  [
    ['</styles.css>; rel=preload; as=style', '</styles.css>; rel=preload; as=style'],
    ['</styles.css>; rel=preload; title=hello', '</styles.css>; rel=preload; title=hello'],
    ['</styles.css>; rel=preload; crossorigin=hello', '</styles.css>; rel=preload; crossorigin=hello'],
    ['</styles.css>; rel=preload; disabled=true', '</styles.css>; rel=preload; disabled=true'],
    ['</styles.css>; rel=preload; fetchpriority=high', '</styles.css>; rel=preload; fetchpriority=high'],
    ['</styles.css>; rel=preload; referrerpolicy=origin', '</styles.css>; rel=preload; referrerpolicy=origin'],
  ].forEach(([value, expected]) => assert.strictEqual(validateLinkHeaderValue(value), expected));
}
