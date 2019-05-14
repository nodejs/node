// Flags: --expose-internals
'use strict';

const common = require('../common');
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
  [{}]
].forEach((input) => {
  common.expectsError(() => assertIsObject(input, 'foo', 'Object'),
                      {
                        code: 'ERR_INVALID_ARG_TYPE',
                        message: 'The "foo" argument must be of type Object. ' +
                                 `Received type ${typeof input}`
                      });
});

assertWithinRange('foo', 1, 0, 2);

common.expectsError(() => assertWithinRange('foo', 1, 2, 3),
                    {
                      code: 'ERR_HTTP2_INVALID_SETTING_VALUE',
                      message: 'Invalid value for setting "foo": 1'
                    });
