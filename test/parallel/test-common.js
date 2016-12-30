'use strict';
const common = require('../common');
const assert = require('assert');

common.globalCheck = false;
global.gc = 42;  // Not a valid global unless --expose_gc is set.
assert.deepStrictEqual(common.leakedGlobals(), ['gc']);

assert.throws(function() {
  common.mustCall(function() {}, 'foo');
}, /^TypeError: Invalid expected value: foo$/);

assert.throws(function() {
  common.mustCall(function() {}, /foo/);
}, /^TypeError: Invalid expected value: \/foo\/$/);
