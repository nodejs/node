'use strict';

// This tests that AsyncHooks throws an error if bad parameters are passed.

const common = require('../common');
const async_hooks = require('async_hooks');
const non_function = 10;

common.expectsError(() => {
  async_hooks.createHook({ init: non_function });
}, typeErrorForFunction('init'));

common.expectsError(() => {
  async_hooks.createHook({ before: non_function });
}, typeErrorForFunction('before'));

common.expectsError(() => {
  async_hooks.createHook({ after: non_function });
}, typeErrorForFunction('after'));

common.expectsError(() => {
  async_hooks.createHook({ destroy: non_function });
}, typeErrorForFunction('destroy'));

common.expectsError(() => {
  async_hooks.createHook({ promiseResolve: non_function });
}, typeErrorForFunction('promiseResolve'));

function typeErrorForFunction(functionName) {
  return {
    code: 'ERR_ASYNC_CALLBACK',
    type: TypeError,
    message: `${functionName} must be a function`,
  };
}
