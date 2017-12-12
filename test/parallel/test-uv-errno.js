'use strict';

const common = require('../common');
const assert = require('assert');
const util = require('util');
const uv = process.binding('uv');

const keys = Object.keys(uv);

keys.forEach((key) => {
  if (!key.startsWith('UV_'))
    return;

  assert.doesNotThrow(() => {
    const err = util._errnoException(uv[key], 'test');
    const name = uv.errname(uv[key]);
    assert.strictEqual(err.code, err.errno);
    assert.strictEqual(err.code, name);
    assert.strictEqual(err.message, `test ${name}`);
  });
});

['test', {}, []].forEach((key) => {
  common.expectsError(
    () => util._errnoException(key),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message: 'The "err" argument must be of type number. ' +
               `Received type ${typeof key}`
    });
});

[0, 1, Infinity, -Infinity, NaN].forEach((key) => {
  common.expectsError(
    () => util._errnoException(key),
    {
      code: 'ERR_VALUE_OUT_OF_RANGE',
      type: RangeError,
      message: 'The value of "err" must be a negative integer. ' +
               `Received "${key}"`
    });
});
