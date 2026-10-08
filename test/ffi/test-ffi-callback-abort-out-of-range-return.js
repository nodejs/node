'use strict';
const common = require('../common');
common.skipIfFFIMissing();
const { test } = require('node:test');
const { assertCallbackAborts } = require('./ffi-callback-test-common');

test('ffi aborts on out-of-range callback return values', () => {
  assertCallbackAborts('return 2 ** 40;', /Callback returned invalid value for declared FFI type/);
});
