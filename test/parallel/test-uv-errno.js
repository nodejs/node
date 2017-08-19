'use strict';

const common = require('../common');
const assert = require('assert');
const util = require('util');
const uv = process.binding('uv');

const keys = Object.keys(uv);

keys.forEach((key) => {
  if (key === 'errname')
    return;

  assert.doesNotThrow(() => {
    const err = util._errnoException(uv[key], 'test');
    const name = uv.errname(uv[key]);
    assert.strictEqual(err.code, err.errno);
    assert.strictEqual(err.code, name);
    assert.strictEqual(err.message, `test ${name}`);
  });
});

[0, 1, 'test', {}, [], Infinity, -Infinity, NaN].forEach((key) => {
  common.expectsError(
    () => util._errnoException(key),
    {
      code: 'ERR_INVALID_ARG_TYPE',
      type: TypeError,
      message: 'The "err" argument must be of type negative number'
    });
});
