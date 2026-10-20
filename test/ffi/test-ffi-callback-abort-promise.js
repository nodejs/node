'use strict';
const common = require('../common');
common.skipIfFFIMissing();
const { test } = require('node:test');
const { assertCallbackAborts } = require('./ffi-callback-test-common');

test('ffi aborts when a callback returns a promise', () => {
  assertCallbackAborts('return Promise.resolve(1);', /Callbacks cannot return promises/);
});
