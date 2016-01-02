// Flags: --expose_internals
'use strict';
require('../common');
const assert = require('assert');
const internalUtil = require('internal/util');

assert.doesNotThrow(function() {
  internalUtil.decorateErrorStack();
  internalUtil.decorateErrorStack(null);
  internalUtil.decorateErrorStack(1);
  internalUtil.decorateErrorStack(true);
});

// Verify that a stack property is not added to non-Errors
const obj = {};
internalUtil.decorateErrorStack(obj);
assert.strictEqual(obj.stack, undefined);

// Verify that the stack is decorated when possible
let err;

try {
  require('../fixtures/syntax/bad_syntax');
} catch (e) {
  err = e;
  assert(!/var foo bar;/.test(err.stack));
  internalUtil.decorateErrorStack(err);
}

assert(/var foo bar;/.test(err.stack));

// Verify that the stack is unchanged when there is no arrow message
err = new Error('foo');
const originalStack = err.stack;
internalUtil.decorateErrorStack(err);
assert.strictEqual(originalStack, err.stack);
