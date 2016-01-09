'use strict';
require('../common');
const assert = require('assert');

function doSetTimeout(callback, after) {
  return function() {
    setTimeout(callback, after);
  };
}

function checkErr(err) {
  return /^'callback' must be a function/.test(err.message);
}

assert.throws(doSetTimeout('foo'), checkErr);
assert.throws(doSetTimeout({foo: 'bar'}), checkErr);
assert.throws(doSetTimeout(), checkErr);
assert.throws(doSetTimeout(undefined, 0), checkErr);
assert.throws(doSetTimeout(null, 0), checkErr);
assert.throws(doSetTimeout(false, 0), checkErr);


function doSetInterval(callback, after) {
  return function() {
    setInterval(callback, after);
  };
}

assert.throws(doSetInterval('foo'), checkErr);
assert.throws(doSetInterval({foo: 'bar'}), checkErr);
assert.throws(doSetInterval(), checkErr);
assert.throws(doSetInterval(undefined, 0), checkErr);
assert.throws(doSetInterval(null, 0), checkErr);
assert.throws(doSetInterval(false, 0), checkErr);


function doSetImmediate(callback, after) {
  return function() {
    setImmediate(callback, after);
  };
}

assert.throws(doSetImmediate('foo'), checkErr);
assert.throws(doSetImmediate({foo: 'bar'}), checkErr);
assert.throws(doSetImmediate(), checkErr);
assert.throws(doSetImmediate(undefined, 0), checkErr);
assert.throws(doSetImmediate(null, 0), checkErr);
assert.throws(doSetImmediate(false, 0), checkErr);
