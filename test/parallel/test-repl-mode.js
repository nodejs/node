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

  cli.callbacks.push(common.mustCall((err, result) => {
    assert.ifError(err);
    assert.strictEqual(result, 3);
  }));
  cli.input.emit('data', 'x = 3\n');

  cli.callbacks.push(common.mustCall((err, result) => {
    assert.ifError(err);
    assert.strictEqual(result, undefined);
  }));
  cli.input.emit('data', 'let y = 3\n');
}

function testStrictMode() {
  const cli = initRepl(repl.REPL_MODE_STRICT);

  cli._domain.once('error', common.mustCall((err) => {
    assert.ok(err);
    assert.ok(/ReferenceError: x is not defined/.test(err.message));
  }));
  cli.input.emit('data', 'x = 3\n');

  cli.callbacks.push(common.mustCall((err, result) => {
    assert.ifError(err);
    assert.strictEqual(result, undefined);
  }));
  cli.input.emit('data', 'let y = 3\n');
}

function testAutoMode() {
  const cli = initRepl(repl.REPL_MODE_MAGIC);

  cli.callbacks.push(common.mustCall((err, result) => {
    assert.ifError(err);
    assert.strictEqual(result, 3);
  }));
  cli.input.emit('data', 'x = 3\n');

  cli.callbacks.push(common.mustCall((err, result) => {
    assert.ifError(err);
    assert.strictEqual(result, undefined);
  }));
  cli.input.emit('data', 'let y = 3\n');
}

function initRepl(mode) {
  const input = new Stream();
  input.write = input.pause = input.resume = () => {};
  input.readable = true;

  const output = new Stream();
  output.write = output.pause = output.resume = function(buf) {
    output.accumulator.push(buf);
  };
  output.accumulator = [];
  output.writable = true;

  const replserver = repl.start({
    input: input,
    output: output,
    useColors: false,
    terminal: false,
    replMode: mode
  });
  const callbacks = [];
  const $eval = replserver.eval;
  replserver.eval = function(code, context, file, cb) {
    const expected = callbacks.shift();
    return $eval.call(this, code, context, file, (...args) => {
      console.log('EVAL RET', args);
      try {
        expected(...args);
      } catch (e) {
        console.error(e);
        process.exit(1);
      }
      cb(...args);
    });
  };
  replserver.callbacks = callbacks;
  return replserver;
}
