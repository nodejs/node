'use strict';

// This tests that AsyncResource throws an error if bad parameters are passed

const common = require('../common');
const async_hooks = require('async_hooks');
const { AsyncResource } = async_hooks;

// Setup init hook such parameters are validated
async_hooks.createHook({
  init() {}
}).enable();

common.expectsError(() => {
  return new AsyncResource();
}, {
  code: 'ERR_INVALID_ARG_TYPE',
  type: TypeError,
});

common.expectsError(() => {
  new AsyncResource('');
}, {
  code: 'ERR_ASYNC_TYPE',
  type: TypeError,
});

common.expectsError(() => {
  new AsyncResource('type', -4);
}, {
  code: 'ERR_INVALID_ASYNC_ID',
  type: RangeError,
});

common.expectsError(() => {
  new AsyncResource('type', Math.PI);
}, {
  code: 'ERR_INVALID_ASYNC_ID',
  type: RangeError,
});
