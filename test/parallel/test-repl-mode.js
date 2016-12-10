'use strict';
const common = require('../common');
const assert = require('assert');
const Stream = require('stream');
const repl = require('repl');

common.globalCheck = false;

const tests = [
  testSloppyMode,
  testStrictMode,
  testAutoMode
];

tests.forEach(function(test) {
  test();
});

function testSloppyMode() {
  const cli = initRepl(repl.REPL_MODE_SLOPPY);

  cli.input.emit('data', `
    x = 3
  `.trim() + '\n');
  assert.strictEqual(cli.output.accumulator.join(''), '> 3\n> ');
  cli.output.accumulator.length = 0;

  cli.input.emit('data', `
    let y = 3
  `.trim() + '\n');
  assert.strictEqual(cli.output.accumulator.join(''), 'undefined\n> ');
}

function testStrictMode() {
  const cli = initRepl(repl.REPL_MODE_STRICT);

  cli.input.emit('data', `
    x = 3
  `.trim() + '\n');
  assert.ok(/ReferenceError: x is not defined/.test(
      cli.output.accumulator.join('')));
  cli.output.accumulator.length = 0;

  cli.input.emit('data', `
    let y = 3
  `.trim() + '\n');
  assert.strictEqual(cli.output.accumulator.join(''), 'undefined\n> ');
}

function testAutoMode() {
  const cli = initRepl(repl.REPL_MODE_MAGIC);

  cli.input.emit('data', `
    x = 3
  `.trim() + '\n');
  assert.strictEqual(cli.output.accumulator.join(''), '> 3\n> ');
  cli.output.accumulator.length = 0;

  cli.input.emit('data', `
    let y = 3
  `.trim() + '\n');
  assert.strictEqual(cli.output.accumulator.join(''), 'undefined\n> ');
}

function initRepl(mode) {
  const input = new Stream();
  input.write = input.pause = input.resume = function() {};
  input.readable = true;

  const output = new Stream();
  output.write = output.pause = output.resume = function(buf) {
    output.accumulator.push(buf);
  };
  output.accumulator = [];
  output.writable = true;

  return repl.start({
    input: input,
    output: output,
    useColors: false,
    terminal: false,
    replMode: mode
  });
}
