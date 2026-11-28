'use strict';

// Tests that every line of error.stack after the message, including the
// last one, is included in the report's javascriptStack.stack.

require('../common');
const assert = require('assert');

function stackOf(stack) {
  const err = new Error('boom');
  err.stack = stack;
  return process.report.getReport(err).javascriptStack;
}

{
  const { message, stack } = stackOf(
    'Error: boom\n    at a (file.js:1:1)\n    at b (file.js:2:2)');
  assert.strictEqual(message, 'Error: boom');
  assert.deepStrictEqual(stack, ['at a (file.js:1:1)', 'at b (file.js:2:2)']);
}

{
  // A trailing newline does not produce an empty frame.
  const { stack } = stackOf('Error: boom\n    at a (file.js:1:1)\n');
  assert.deepStrictEqual(stack, ['at a (file.js:1:1)']);
}

{
  const { message, stack } = stackOf('Error: boom');
  assert.strictEqual(message, 'Error: boom');
  assert.strictEqual(stack, undefined);
}

{
  // A real stack is reported in full, including its last frame.
  const err = new Error('real');
  const lines = err.stack.split('\n').slice(1).map((line) => line.trim());
  const { stack } = process.report.getReport(err).javascriptStack;
  assert.deepStrictEqual(stack, lines);
}
