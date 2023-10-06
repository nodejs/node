'use strict';

// This tests that using falsy values in createHook throws an error.

require('../common');
const assert = require('assert');
const async_hooks = require('async_hooks');

const badArgs = [0, 1, false, true, null, 'hello'];
const hookNames = ['init', 'before', 'after', 'destroy', 'promiseResolve'];

for (const arg of badArgs) {
  for (const name of hookNames) {
    assert.throws(() => {
      async_hooks.createHook({ [name]: arg });
    }, {
      code: 'ERR_ASYNC_CALLBACK',
      name: 'TypeError',
      message: `hook.${name} must be a function`
    });
  }
}
