'use strict';
require('../common');
const assert = require('assert');

function doSetTimeout(callback, after) {
  return function() {
    setTimeout(callback, after);
  };
}

assert.throws(doSetTimeout('foo'),
              /"callback" argument must be a function/);
assert.throws(doSetTimeout({foo: 'bar'}),
              /"callback" argument must be a function/);
assert.throws(doSetTimeout(),
              /"callback" argument must be a function/);
assert.throws(doSetTimeout(undefined, 0),
              /"callback" argument must be a function/);
assert.throws(doSetTimeout(null, 0),
              /"callback" argument must be a function/);
assert.throws(doSetTimeout(false, 0),
              /"callback" argument must be a function/);


function doSetInterval(callback, after) {
  return function() {
    setInterval(callback, after);
  };
}

assert.throws(doSetInterval('foo'),
              /"callback" argument must be a function/);
assert.throws(doSetInterval({foo: 'bar'}),
              /"callback" argument must be a function/);
assert.throws(doSetInterval(),
              /"callback" argument must be a function/);
assert.throws(doSetInterval(undefined, 0),
              /"callback" argument must be a function/);
assert.throws(doSetInterval(null, 0),
              /"callback" argument must be a function/);
assert.throws(doSetInterval(false, 0),
              /"callback" argument must be a function/);


function doSetImmediate(callback, after) {
  return function() {
    setImmediate(callback, after);
  };
}

assert.throws(doSetImmediate('foo'),
              /"callback" argument must be a function/);
assert.throws(doSetImmediate({foo: 'bar'}),
              /"callback" argument must be a function/);
assert.throws(doSetImmediate(),
              /"callback" argument must be a function/);
assert.throws(doSetImmediate(undefined, 0),
              /"callback" argument must be a function/);
assert.throws(doSetImmediate(null, 0),
              /"callback" argument must be a function/);
assert.throws(doSetImmediate(false, 0),
              /"callback" argument must be a function/);
