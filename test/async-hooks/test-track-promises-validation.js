'use strict';
// Test validation of trackPromises option.

require('../common');
const { createHook } = require('node:async_hooks');
const assert = require('node:assert');
const { inspect } = require('util');

for (const invalid of [0, null, 1, NaN, Symbol(0), function() {}, 'test']) {
  assert.throws(
    () => createHook({
      init() {},
      trackPromises: invalid,
    }),
    { code: 'ERR_INVALID_ARG_TYPE' },
    `trackPromises: ${inspect(invalid)} should throw`);
}

assert.throws(
  () => createHook({
    trackPromises: false,
    promiseResolve() {},
  }),
  { code: 'ERR_INVALID_ARG_VALUE' },
  `trackPromises: false and promiseResolve() are incompatible`);
