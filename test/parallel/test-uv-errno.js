'use strict';

const common = require('../common');
const assert = require('assert');
const {
  getSystemErrorName,
  _errnoException
} = require('util');

const uv = process.binding('uv');
const keys = Object.keys(uv);

keys.forEach((key) => {
  if (!key.startsWith('UV_'))
    return;

  assert.doesNotThrow(() => {
    const err = _errnoException(uv[key], 'test');
    const name = uv.errname(uv[key]);
    assert.strictEqual(getSystemErrorName(uv[key]), name);
    assert.strictEqual(err.code, name);
    assert.strictEqual(err.code, err.errno);
    assert.strictEqual(err.message, `test ${name}`);
  });
});

function runTest(fn) {
  ['test', {}, []].forEach((err) => {
    common.expectsError(
      () => fn(err),
      {
        code: 'ERR_INVALID_ARG_TYPE',
        type: TypeError,
        message: 'The "err" argument must be of type number. ' +
                 `Received type ${typeof err}`
      });
  });

  [0, 1, Infinity, -Infinity, NaN].forEach((err) => {
    common.expectsError(
      () => fn(err),
      {
        code: 'ERR_OUT_OF_RANGE',
        type: RangeError,
        message: 'The value of "err" is out of range. ' +
                 'It must be a negative integer. ' +
                 `Received ${err}`
      });
  });
}

runTest(_errnoException);
runTest(getSystemErrorName);
