// Flags: --expose-internals
'use strict';
// Test that trackPromises: false prevents promise hooks from being installed.

require('../common');
const { internalBinding } = require('internal/test/binding');
const { getPromiseHooks } = internalBinding('async_wrap');
const { createHook } = require('node:async_hooks');
const assert = require('node:assert');

createHook({
  init() {
    // This can get called for writes to stdout due to the warning about internals.
  },
  trackPromises: false,
}).enable();

Promise.resolve(1729);
assert.deepStrictEqual(getPromiseHooks(), [undefined, undefined, undefined, undefined]);
