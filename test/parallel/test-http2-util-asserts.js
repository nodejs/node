// Flags: --expose-internals
'use strict';

const assert = require('assert');
const {
  assertIsObject,
  assertWithinRange,
} = require('internal/http2/util');

[
  undefined,
  {},
  Object.create(null),
  new Date(),
  new (class Foo {})(),
].forEach((input) => {
  assertIsObject(input, 'foo', 'Object');
});

[
  1,
  false,
  'hello',
  NaN,
  Infinity,
  [],
  [{}],
].forEach((input) => {
  assert.throws(
    () => assertIsObject(input, 'foo', 'Object'),
    {
      code: 'ERR_INVALID_ARG_TYPE',
    });
});

assertWithinRange('foo', 1, 0, 2);

assert.throws(() => assertWithinRange('foo', 1, 2, 3),
              {
                code: 'ERR_HTTP2_INVALID_SETTING_VALUE',
              });
