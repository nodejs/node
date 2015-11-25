'use strict';
const common = require('../common');
const assert = require('assert');
const util = require('util');

assert.doesNotThrow(function() {
  util.decorateErrorStack();
  util.decorateErrorStack(null);
  util.decorateErrorStack(1);
  util.decorateErrorStack(true);
});

// Verify that a stack property is not added to non-Errors
const obj = {};
util.decorateErrorStack(obj);
assert.strictEqual(obj.stack, undefined);

// Verify that the stack is decorated when possible
let err;

try {
  require('../fixtures/syntax/bad_syntax');
} catch (e) {
  err = e;
  assert(!/var foo bar;/.test(err.stack));
  util.decorateErrorStack(err);
}

assert(/var foo bar;/.test(err.stack));

// Verify that the stack is unchanged when there is no arrow message
err = new Error('foo');
const originalStack = err.stack;
util.decorateErrorStack(err);
assert.strictEqual(originalStack, err.stack);
