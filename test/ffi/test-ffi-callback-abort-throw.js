'use strict';
const common = require('../common');
common.skipIfFFIMissing();
const { test } = require('node:test');
const { assertCallbackAborts } = require('./ffi-callback-test-common');

test('ffi aborts when a callback throws', () => {
  assertCallbackAborts('throw new Error("boom");', /Callbacks cannot throw an exception/);
});
