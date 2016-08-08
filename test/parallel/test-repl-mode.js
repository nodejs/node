'use strict';
var common = require('../common');
var assert = require('assert');
var Stream = require('stream');
var repl = require('repl');

common.globalCheck = false;

var tests = [
  testSloppyMode,
  testStrictMode,
  testAutoMode
];

tests.forEach(function(test) {
  test();
});

function testSloppyMode() {
  var cli = initRepl(repl.REPL_MODE_SLOPPY);

  cli.input.emit('data', `
    x = 3
  `.trim() + '\r\n');
  assert.equal(cli.output.accumulator.join(''), '> 3\r\n> ');
  cli.output.accumulator.length = 0;

  cli.input.emit('data', `
    let y = 3
  `.trim() + '\r\n');
  assert.equal(cli.output.accumulator.join(''), 'undefined\r\n> ');
}

function testStrictMode() {
  var cli = initRepl(repl.REPL_MODE_STRICT);

  cli.input.emit('data', `
    x = 3
  `.trim() + '\r\n');
  assert.ok(/ReferenceError: x is not defined/.test(
      cli.output.accumulator.join('')));
  cli.output.accumulator.length = 0;

  cli.input.emit('data', `
    let y = 3
  `.trim() + '\r\n');
  assert.equal(cli.output.accumulator.join(''), 'undefined\r\n> ');
}

function testAutoMode() {
  var cli = initRepl(repl.REPL_MODE_MAGIC);

  cli.input.emit('data', `
    x = 3
  `.trim() + '\r\n');
  assert.equal(cli.output.accumulator.join(''), '> 3\r\n> ');
  cli.output.accumulator.length = 0;

  cli.input.emit('data', `
    let y = 3
  `.trim() + '\r\n');
  assert.equal(cli.output.accumulator.join(''), 'undefined\r\n> ');
}

function initRepl(mode) {
  var input = new Stream();
  input.write = input.pause = input.resume = function() {};
  input.readable = true;

  var output = new Stream();
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
