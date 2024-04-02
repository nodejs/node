'use strict';

// This tests that AsyncHooks throws an error if bad parameters are passed.

require('../common');
const assert = require('assert');
const async_hooks = require('async_hooks');
const nonFunctionArray = [null, -1, 1, {}, []];

['init', 'before', 'after', 'destroy', 'promiseResolve'].forEach(
  (functionName) => {
    nonFunctionArray.forEach((nonFunction) => {
      assert.throws(() => {
        async_hooks.createHook({ [functionName]: nonFunction });
      }, {
        code: 'ERR_ASYNC_CALLBACK',
        name: 'TypeError',
        message: `hook.${functionName} must be a function`,
      });
    });
  });
