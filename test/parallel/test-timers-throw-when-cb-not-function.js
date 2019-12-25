'use strict';
require('../common');
const assert = require('assert');

function doSetTimeout(callback, after) {
  return function() {
    setTimeout(callback, after);
  };
}

const errMessage = {
  code: 'ERR_INVALID_CALLBACK',
  name: 'TypeError'
};

assert.throws(doSetTimeout('foo'), errMessage);
assert.throws(doSetTimeout({ foo: 'bar' }), errMessage);
assert.throws(doSetTimeout(), errMessage);
assert.throws(doSetTimeout(undefined, 0), errMessage);
assert.throws(doSetTimeout(null, 0), errMessage);
assert.throws(doSetTimeout(false, 0), errMessage);


function doSetInterval(callback, after) {
  return function() {
    setInterval(callback, after);
  };
}

assert.throws(doSetInterval('foo'), errMessage);
assert.throws(doSetInterval({ foo: 'bar' }), errMessage);
assert.throws(doSetInterval(), errMessage);
assert.throws(doSetInterval(undefined, 0), errMessage);
assert.throws(doSetInterval(null, 0), errMessage);
assert.throws(doSetInterval(false, 0), errMessage);


function doSetImmediate(callback, after) {
  return function() {
    setImmediate(callback, after);
  };
}

assert.throws(doSetImmediate('foo'), errMessage);
assert.throws(doSetImmediate({ foo: 'bar' }), errMessage);
assert.throws(doSetImmediate(), errMessage);
assert.throws(doSetImmediate(undefined, 0), errMessage);
assert.throws(doSetImmediate(null, 0), errMessage);
assert.throws(doSetImmediate(false, 0), errMessage);
