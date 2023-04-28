'use strict';

// This tests that using falsy values in createHook throws an error.

require('../common');
const assert = require('assert');
const async_hooks = require('async_hooks');

[0, 1, false, true, null, 'hello'].forEach((badArg) => {
  const hookNames = ['init', 'before', 'after', 'destroy', 'promiseResolve'];
  hookNames.forEach((field) => {
    assert.throws(() => {
      async_hooks.createHook({ [field]: badArg });
    }, {
      code: 'ERR_ASYNC_CALLBACK',
      name: 'TypeError',
      message: `hook.${field} must be a function`
    });
  });
});
