'use strict';
require('../common');
const assert = require('assert');
const util = require('util');

function Constructor1() {}

function Constructor2() {
  Constructor1.call(this);
}
util.inherits(Constructor2, Constructor1);

// First parameter must be an object instance
assert.throws(() => assert.isInstanceOf(undefined, Constructor1),
              assert.AssertionError);
assert.throws(() => assert.isInstanceOf(null, Array),
              assert.AssertionError);
assert.throws(() => assert.isInstanceOf(null, Constructor1),
              assert.AssertionError);
assert.throws(() => assert.isInstanceOf(1, Constructor1),
              assert.AssertionError);
assert.throws(() => assert.isInstanceOf(true, Constructor1),
              assert.AssertionError);
assert.throws(() => assert.isInstanceOf(() => {}, Constructor1),
              assert.AssertionError);
// Second parameter must be a function
assert.throws(() => assert.isInstanceOf([], null), TypeError);
// Object must be an instance of
assert.throws(() => assert.isInstanceOf({}, Constructor1),
              assert.AssertionError);

// Correct usage
assert.doesNotThrow(() => assert.isInstanceOf(new Array(), Array));
assert.doesNotThrow(() => assert.isInstanceOf([ ], Array));
assert.doesNotThrow(() => assert.isInstanceOf([ 1 ], Array));
assert.doesNotThrow(() => assert.isInstanceOf(new Constructor1(),
                                              Constructor1));
assert.doesNotThrow(() => assert.isInstanceOf(new Constructor2(),
                                              Constructor1));
assert.doesNotThrow(() => assert.isInstanceOf(new Constructor2(),
                                              Constructor2));
