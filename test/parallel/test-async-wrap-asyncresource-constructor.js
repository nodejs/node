'use strict';
require('../common');

// This tests that AsyncResource throws an error if bad parameters are passed

const assert = require('assert');
const AsyncResource = require('async_hooks').AsyncResource;
const async_wrap = process.binding('async_wrap');

assert.throws(() => {
  return new AsyncResource();
}, new RegExp('^TypeError: type must be a string with length > 0$'));

assert.throws(() => {
  new AsyncResource('');
}, new RegExp('^TypeError: type must be a string with length > 0$'));

assert.throws(() => {
  new AsyncResource('type', -4);
}, new RegExp('^RangeError: triggerId must be an unsigned integer$'));

assert.throws(() => {
  new AsyncResource('type', Math.PI);
}, new RegExp('^RangeError: triggerId must be an unsigned integer$'));
