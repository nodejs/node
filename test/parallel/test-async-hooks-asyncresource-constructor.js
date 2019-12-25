'use strict';

// This tests that AsyncResource throws an error if bad parameters are passed

require('../common');
const assert = require('assert');
const async_hooks = require('async_hooks');
const { AsyncResource } = async_hooks;

// Setup init hook such parameters are validated
async_hooks.createHook({
  init() {}
}).enable();

assert.throws(() => {
  return new AsyncResource();
}, {
  code: 'ERR_INVALID_ARG_TYPE',
  name: 'TypeError',
});

assert.throws(() => {
  new AsyncResource('');
}, {
  code: 'ERR_ASYNC_TYPE',
  name: 'TypeError',
});

assert.throws(() => {
  new AsyncResource('type', -4);
}, {
  code: 'ERR_INVALID_ASYNC_ID',
  name: 'RangeError',
});

assert.throws(() => {
  new AsyncResource('type', Math.PI);
}, {
  code: 'ERR_INVALID_ASYNC_ID',
  name: 'RangeError',
});
