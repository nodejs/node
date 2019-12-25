'use strict';

// This tests that AsyncHooks throws an error if bad parameters are passed.

require('../common');
const assert = require('assert');
const async_hooks = require('async_hooks');
const non_function = 10;

typeErrorForFunction('init');
typeErrorForFunction('before');
typeErrorForFunction('after');
typeErrorForFunction('destroy');
typeErrorForFunction('promiseResolve');

function typeErrorForFunction(functionName) {
  assert.throws(() => {
    async_hooks.createHook({ [functionName]: non_function });
  }, {
    code: 'ERR_ASYNC_CALLBACK',
    name: 'TypeError',
    message: `hook.${functionName} must be a function`
  });
}
