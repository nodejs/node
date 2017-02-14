'use strict';
const common = require('../common');
const assert = require('assert');

function doSetTimeout(callback, after) {
  return function() {
    setTimeout(callback, after);
  };
}

const expectedError = common.expectsError('ERR_INVALID_CALLBACK', TypeError);

assert.throws(doSetTimeout('foo'),
              expectedError);
assert.throws(doSetTimeout({foo: 'bar'}),
              expectedError);
assert.throws(doSetTimeout(),
              expectedError);
assert.throws(doSetTimeout(undefined, 0),
              expectedError);
assert.throws(doSetTimeout(null, 0),
              expectedError);
assert.throws(doSetTimeout(false, 0),
              expectedError);


function doSetInterval(callback, after) {
  return function() {
    setInterval(callback, after);
  };
}

assert.throws(doSetInterval('foo'),
              expectedError);
assert.throws(doSetInterval({foo: 'bar'}),
              expectedError);
assert.throws(doSetInterval(),
              expectedError);
assert.throws(doSetInterval(undefined, 0),
              expectedError);
assert.throws(doSetInterval(null, 0),
              expectedError);
assert.throws(doSetInterval(false, 0),
              expectedError);


function doSetImmediate(callback, after) {
  return function() {
    setImmediate(callback, after);
  };
}

assert.throws(doSetImmediate('foo'),
              expectedError);
assert.throws(doSetImmediate({foo: 'bar'}),
              expectedError);
assert.throws(doSetImmediate(),
              expectedError);
assert.throws(doSetImmediate(undefined, 0),
              expectedError);
assert.throws(doSetImmediate(null, 0),
              expectedError);
assert.throws(doSetImmediate(false, 0),
              expectedError);
