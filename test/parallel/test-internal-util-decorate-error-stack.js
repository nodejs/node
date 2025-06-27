// Flags: --expose-internals
'use strict';
require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const internalUtil = require('internal/util');
const { internalBinding } = require('internal/test/binding');
const {
  privateSymbols: {
    arrow_message_private_symbol,
    decorated_private_symbol,
  }
} = internalBinding('util');
const spawnSync = require('child_process').spawnSync;

const decorateErrorStack = internalUtil.decorateErrorStack;

// Verify that decorateErrorStack does not throw with non-objects.
decorateErrorStack();
decorateErrorStack(null);
decorateErrorStack(1);
decorateErrorStack(true);

// Verify that a stack property is not added to non-Errors.
const obj = {};
decorateErrorStack(obj);
assert.strictEqual(obj.stack, undefined);

// Verify that the stack is decorated when possible.
function checkStack(stack) {
  // Matching only on a minimal piece of the stack because the string will vary
  // greatly depending on the JavaScript engine. V8 includes `;` because it
  // displays the line of code (`var foo bar;`) that is causing a problem.
  // ChakraCore does not display the line of code but includes `;` in the phrase
  // `Expected ';' `.
  assert.match(stack, /;/g);
  // Test that it's a multiline string.
  assert.match(stack, /\n/g);
}
let err;
const badSyntaxPath =
  fixtures.path('syntax', 'bad_syntax').replace(/\\/g, '\\\\');

try {
  require(badSyntaxPath);
} catch (e) {
  err = e;
}

assert(typeof err, 'object');
checkStack(err.stack);

// Verify that the stack is only decorated once.
decorateErrorStack(err);
decorateErrorStack(err);
checkStack(err.stack);

// Verify that the stack is only decorated once for uncaught exceptions.
const args = [
  '-e',
  `require(${JSON.stringify(badSyntaxPath)})`,
];
const result = spawnSync(process.argv[0], args, { encoding: 'utf8' });
checkStack(result.stderr);

// Verify that the stack is unchanged when there is no arrow message.
err = new Error('foo');
let originalStack = err.stack;
decorateErrorStack(err);
assert.strictEqual(originalStack, err.stack);

// Verify that the arrow message is added to the start of the stack when it
// exists.
const arrowMessage = 'arrow_message';
err = new Error('foo');
originalStack = err.stack;

err[arrow_message_private_symbol] = arrowMessage;
decorateErrorStack(err);

assert.strictEqual(err.stack, `${arrowMessage}${originalStack}`);
assert.strictEqual(err[decorated_private_symbol], true);
