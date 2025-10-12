'use strict';
const common = require('../common');
const assert = require('assert');
const repl = require('repl');
const { startNewREPLServer } = require('../common/repl');

if (process.env.TERM === 'dumb') {
  common.skip('skipping - dumb terminal');
}

const tests = [
  testSloppyMode,
  testStrictMode,
  testAutoMode,
  testStrictModeTerminal,
];

tests.forEach(function(test) {
  test();
});

function testSloppyMode() {
  const { input, output } = startNewREPLServer({ replMode: repl.REPL_MODE_SLOPPY, terminal: false, prompt: '> ' });

  input.emit('data', 'x = 3\n');
  assert.strictEqual(output.accumulator, '> 3\n> ');
  output.accumulator = '';

  input.emit('data', 'let y = 3\n');
  assert.strictEqual(output.accumulator, 'undefined\n> ');
}

function testStrictMode() {
  const { input, output } = startNewREPLServer({ replMode: repl.REPL_MODE_STRICT, terminal: false, prompt: '> ' }, {
    disableDomainErrorAssert: true,
  });

  input.emit('data', 'x = 3\n');
  assert.match(output.accumulator, /ReferenceError: x is not defined/);
  output.accumulator = '';

  input.emit('data', 'let y = 3\n');
  assert.strictEqual(output.accumulator, 'undefined\n> ');
}

function testStrictModeTerminal() {
  if (!process.features.inspector) {
    console.warn('Test skipped: V8 inspector is disabled');
    return;
  }
  // Verify that ReferenceErrors are reported in strict mode previews.
  const { input, output } = startNewREPLServer({ replMode: repl.REPL_MODE_STRICT, prompt: '> ' });

  input.emit('data', 'xyz ');
  assert.ok(
    output.accumulator.includes('\n// ReferenceError: xyz is not defined')
  );
}

function testAutoMode() {
  const { input, output } = startNewREPLServer({ replMode: repl.REPL_MODE_MAGIC, terminal: false, prompt: '> ' });

  input.emit('data', 'x = 3\n');
  assert.strictEqual(output.accumulator, '> 3\n> ');
  output.accumulator = '';

  input.emit('data', 'let y = 3\n');
  assert.strictEqual(output.accumulator, 'undefined\n> ');
}
