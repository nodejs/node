'use strict';
require('../common');

// This tests that AsyncResource throws an error if bad parameters are passed

const assert = require('assert');
const async_hooks = require('async_hooks');
const { AsyncResource } = async_hooks;

// Setup init hook such parameters are validated
async_hooks.createHook({
  init() {}
}).enable();

assert.throws(() => {
  return new AsyncResource();
}, /^TypeError \[ERR_INVALID_STRING_LENGTH\]: The string "type" \(length 0\) must be of length > 0\.$/);

assert.throws(() => {
  new AsyncResource('');
}, /^TypeError \[ERR_INVALID_STRING_LENGTH\]: The string "type" \(length 0\) must be of length > 0\.$/);

assert.throws(() => {
  new AsyncResource('type', -4);
}, /^RangeError \[ERR_NOT_ASSIGNED_INTEGER\]: triggerAsyncId must be an unsigned integer$/);

assert.throws(() => {
  new AsyncResource('type', Math.PI);
}, /^RangeError \[ERR_NOT_ASSIGNED_INTEGER\]: triggerAsyncId must be an unsigned integer$/);
