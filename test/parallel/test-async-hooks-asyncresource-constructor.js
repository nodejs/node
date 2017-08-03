'use strict';

// This tests that AsyncResource throws an error if bad parameters are passed

const common = require('../common');
const assert = require('assert');
const async_hooks = require('async_hooks');
const { AsyncResource } = async_hooks;

// Setup init hook such parameters are validated
async_hooks.createHook({
  init() {}
}).enable();

assert.throws(() => {
  return new AsyncResource();
}, common.expectsError({
  code: 'ERR_ASYNC_TYPE',
  type: TypeError,
}));

assert.throws(() => {
  new AsyncResource('');
}, common.expectsError({
  code: 'ERR_ASYNC_TYPE',
  type: TypeError,
}));

assert.throws(() => {
  new AsyncResource('type', -4);
}, common.expectsError({
  code: 'ERR_INVALID_ASYNC_ID',
  type: RangeError,
}));

assert.throws(() => {
  new AsyncResource('type', Math.PI);
}, common.expectsError({
  code: 'ERR_INVALID_ASYNC_ID',
  type: RangeError,
}));
