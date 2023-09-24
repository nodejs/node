// Flags: --expose-internals
'use strict';

const common = require('../common');
const assert = require('assert');
const {
  getSystemErrorName,
  _errnoException
} = require('util');

const { internalBinding } = require('internal/test/binding');
const uv = internalBinding('uv');
const keys = Object.keys(uv);

assert.strictEqual(uv.errname(-111111), 'Unknown system error -111111');

for (const key of keys) {
  if (!key.startsWith('UV_'))
    continue;

  const err = _errnoException(uv[key], 'test');
  const name = uv.errname(uv[key]);
  assert.strictEqual(getSystemErrorName(uv[key]), name);
  assert.strictEqual(err.code, name);
  assert.strictEqual(err.code, getSystemErrorName(err.errno));
  assert.strictEqual(err.message, `test ${name}`);
}

function runTest(fn) {
  const invalidArgType = ['test', {}, []];
  const outOfRangeArgs = [0, 1, Infinity, -Infinity, NaN];
  for (const err of invalidArgType) {
    assert.throws(
      () => fn(err),
      {
        code: 'ERR_INVALID_ARG_TYPE',
        name: 'TypeError',
        message: 'The "err" argument must be of type number.' +
                 common.invalidArgTypeHelper(err)
      });
  }

  for (const err of outOfRangeArgs) {
    assert.throws(
      () => fn(err),
      {
        code: 'ERR_OUT_OF_RANGE',
        name: 'RangeError',
        message: 'The value of "err" is out of range. ' +
                 'It must be a negative integer. ' +
                 `Received ${err}`
      });
  }
}

runTest(_errnoException);
runTest(getSystemErrorName);
