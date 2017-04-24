'use strict';
require('../common');
const assert = require('assert');

const error = /^TypeError: "callback" argument must be a function$/;

function doSetTimeout(callback, after) {
  return function() {
    setTimeout(callback, after);
  };
}

assert.throws(doSetTimeout('foo'), error);
assert.throws(doSetTimeout({foo: 'bar'}), error);
assert.throws(doSetTimeout(), error);
assert.throws(doSetTimeout(undefined, 0), error);
assert.throws(doSetTimeout(null, 0), error);
assert.throws(doSetTimeout(false, 0), error);

function doSetInterval(callback, after) {
  return function() {
    setInterval(callback, after);
  };
}

assert.throws(doSetInterval('foo'), error);
assert.throws(doSetInterval({foo: 'bar'}), error);
assert.throws(doSetInterval(), error);
assert.throws(doSetInterval(undefined, 0), error);
assert.throws(doSetInterval(null, 0), error);
assert.throws(doSetInterval(false, 0), error);

function doSetImmediate(callback, after) {
  return function() {
    setImmediate(callback, after);
  };
}

assert.throws(doSetImmediate('foo'), error);
assert.throws(doSetImmediate({foo: 'bar'}), error);
assert.throws(doSetImmediate(), error);
assert.throws(doSetImmediate(undefined, 0), error);
assert.throws(doSetImmediate(null, 0), error);
assert.throws(doSetImmediate(false, 0), error);
