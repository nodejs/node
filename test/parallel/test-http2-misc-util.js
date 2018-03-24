// Flags: --expose-internals
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');

const {
  assertIsObject,
  assertWithinRange,
  sessionName
} = require('internal/http2/util');

// Code coverage for sessionName utility function
assert.strictEqual(sessionName(0), 'server');
assert.strictEqual(sessionName(1), 'client');
[2, '', 'test', {}, [], true].forEach((i) => {
  assert.strictEqual(sessionName(2), '<invalid>');
});

// Code coverage for assertWithinRange function
common.expectsError(
  () => assertWithinRange('test', -1),
  {
    code: 'ERR_HTTP2_INVALID_SETTING_VALUE',
    type: RangeError,
    message: 'Invalid value for setting "test": -1'
  });

assertWithinRange('test', 1);

common.expectsError(
  () => assertIsObject('foo', 'test'),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: 'The "test" argument must be of type Object. Received type string'
  });

common.expectsError(
  () => assertIsObject('foo', 'test', ['Date']),
  {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: 'The "test" argument must be of type Date. Received type string'
  });

assertIsObject({}, 'test');
