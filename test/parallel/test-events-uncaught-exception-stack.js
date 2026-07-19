'use strict';
const common = require('../common');
const assert = require('assert');
const EventEmitter = require('events');

// Tests that the error stack where the exception was thrown is *not* appended.

process.on('uncaughtException', common.mustCall((err) => {
  const [firstLine, ...lines] = err.stack.split('\n');
  assert.strictEqual(firstLine, 'Error');
  lines.forEach((line) => {
    assert.match(line, /^ {4}at/);
  });
}));

new EventEmitter().emit('error', new Error());
