// Flags: --expose-internals
'use strict';

const assert = require('assert');
const {
  getSystemErrorName,
  _errnoException
} = require('util');

const { internalBinding } = require('internal/test/binding');
const uv = internalBinding('uv');
const keys = Object.keys(uv);

keys.forEach((key) => {
  if (!key.startsWith('UV_'))
    return;

  const err = _errnoException(uv[key], 'test');
  const name = uv.errname(uv[key]);
  assert.strictEqual(getSystemErrorName(uv[key]), name);
  assert.strictEqual(err.code, name);
  assert.strictEqual(err.code, getSystemErrorName(err.errno));
});

function runTest(fn) {
  ['test', {}, []].forEach((err) => {
    assert.throws(
      () => fn(err),
      {
        code: 'ERR_INVALID_ARG_TYPE',
        name: 'TypeError',
      });
  });

  [0, 1, Infinity, -Infinity, NaN].forEach((err) => {
    assert.throws(
      () => fn(err),
      {
        code: 'ERR_OUT_OF_RANGE',
        name: 'RangeError',
      });
  });
}

runTest(_errnoException);
runTest(getSystemErrorName);
