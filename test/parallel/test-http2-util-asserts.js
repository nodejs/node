// Flags: --expose-internals --expose-http2
'use strict';

const common = require('../common');
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
  new (class Foo {})()
].forEach((i) => {
  assert.doesNotThrow(() => assertIsObject(i, 'foo', 'object'));
});

[
  1,
  false,
  'hello',
  NaN,
  Infinity,
  [],
  [{}]
].forEach((i) => {
  assert.throws(() => assertIsObject(i, 'foo', 'object'),
                common.expectsError({
                  code: 'ERR_INVALID_ARG_TYPE',
                  message: /^The "foo" argument must be of type object$/
                }));
});

assert.doesNotThrow(() => assertWithinRange('foo', 1, 0, 2));

assert.throws(() => assertWithinRange('foo', 1, 2, 3),
              common.expectsError({
                code: 'ERR_HTTP2_INVALID_SETTING_VALUE',
                message: /^Invalid value for setting "foo": 1$/
              }));
