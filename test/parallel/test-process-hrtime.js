'use strict';
var common = require('../common');
var assert = require('assert');

// the default behavior, return an Array "tuple" of numbers
var tuple = process.hrtime();

// validate the default behavior
validateTuple(tuple);

// validate that passing an existing tuple returns another valid tuple
validateTuple(process.hrtime(tuple));

// test that only an Array may be passed to process.hrtime()
assert.throws(function() {
  process.hrtime(1);
});

function validateTuple(tuple) {
  assert(Array.isArray(tuple));
  assert.equal(2, tuple.length);
  tuple.forEach(function(v) {
    assert.equal('number', typeof v);
    assert(isFinite(v));
  });
}
