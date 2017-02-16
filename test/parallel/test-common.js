'use strict';
const common = require('../common');
const assert = require('assert');


// test for leaked global detection
global.gc = 42;  // Not a valid global unless --expose_gc is set.
assert.deepStrictEqual(common.leakedGlobals(), ['gc']);
delete global.gc;


// common.mustCall() tests
assert.throws(function() {
  common.mustCall(function() {}, 'foo');
}, /^TypeError: Invalid expected value: foo$/);

assert.throws(function() {
  common.mustCall(function() {}, /foo/);
}, /^TypeError: Invalid expected value: \/foo\/$/);


// common.fail() tests
assert.throws(
  () => { common.fail('fhqwhgads'); },
  /^AssertionError: fhqwhgads$/
);
