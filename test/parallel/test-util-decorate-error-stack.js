// Flags: --expose_internals
'use strict';
const common = require('../common');
const assert = require('assert');
const internalUtil = require('internal/util');
const spawnSync = require('child_process').spawnSync;
const path = require('path');

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
function checkStack(stack) {
  const matches = stack.match(/var foo bar;/g);
  assert.strictEqual(Array.isArray(matches), true);
  assert.strictEqual(matches.length, 1);
}
let err;
const badSyntaxPath =
    path.join(common.fixturesDir, 'syntax', 'bad_syntax')
        .replace(/\\/g, '\\\\');

try {
  require(badSyntaxPath);
} catch (e) {
  err = e;
}

assert(typeof err, 'object');
checkStack(err.stack);

// Verify that the stack is only decorated once
internalUtil.decorateErrorStack(err);
internalUtil.decorateErrorStack(err);
checkStack(err.stack);

// Verify that the stack is only decorated once for uncaught exceptions
const args = [
  '-e',
  `require('${badSyntaxPath}')`
];
const result = spawnSync(process.argv[0], args, { encoding: 'utf8' });
checkStack(result.stderr);

// Verify that the stack is unchanged when there is no arrow message
err = new Error('foo');
const originalStack = err.stack;
internalUtil.decorateErrorStack(err);
assert.strictEqual(originalStack, err.stack);
