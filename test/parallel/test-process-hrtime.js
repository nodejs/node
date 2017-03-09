'use strict';
require('../common');
const assert = require('assert');

// the default behavior, return an Array "tuple" of numbers
const tuple = process.hrtime();

// validate the default behavior
validateTuple(tuple);

// validate that passing an existing tuple returns another valid tuple
validateTuple(process.hrtime(tuple));

// test that only an Array may be passed to process.hrtime()
assert.throws(() => {
  process.hrtime(1);
}, /^TypeError: process.hrtime\(\) only accepts an Array tuple$/);

function validateTuple(tuple) {
  assert(Array.isArray(tuple));
  assert.strictEqual(tuple.length, 2);
  tuple.forEach((v) => {
    assert.strictEqual(typeof v, 'number');
    assert.strictEqual(isFinite(v), true);
  });
}

const diff = process.hrtime([0, 1e9 - 1]);
assert(diff[1] >= 0);  // https://github.com/nodejs/node/issues/4751
