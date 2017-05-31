'use strict';
require('../common');

// This tests that AsyncResource throws an error if bad parameters are passed

const assert = require('assert');
const AsyncResource = require('async_hooks').AsyncResource;

assert.throws(() => {
  return new AsyncResource();
}, /^TypeError: type must be a string with length > 0$/);

assert.throws(() => {
  new AsyncResource('');
}, /^TypeError: type must be a string with length > 0$/);

assert.throws(() => {
  new AsyncResource('type', -4);
}, /^RangeError: triggerId must be an unsigned integer$/);

assert.throws(() => {
  new AsyncResource('type', Math.PI);
}, /^RangeError: triggerId must be an unsigned integer$/);
