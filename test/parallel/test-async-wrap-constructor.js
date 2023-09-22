'use strict';

// This tests that using falsy values in createHook throws an error.

require('../common');
const assert = require('assert');
const async_hooks = require('async_hooks');

const badArgs = [0, 1, false, true, null, 'hello'];
const hookNames = ['init', 'before', 'after', 'destroy', 'promiseResolve'];

for (let i = 0; i < badArgs.length; i++) {
  for (let j = 0; j < hookNames.length; j++) {
    assert.throws(() => {
      async_hooks.createHook({ [hookNames[j]]: badArgs[i] });
    }, {
      code: 'ERR_ASYNC_CALLBACK',
      name: 'TypeError',
      message: `hook.${hookNames[j]} must be a function`
    });
  }
}

