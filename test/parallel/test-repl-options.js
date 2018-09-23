'use strict';
var common = require('../common'),
    assert = require('assert'),
    Stream = require('stream'),
    repl = require('repl');

common.globalCheck = false;

// create a dummy stream that does nothing
var stream = new Stream();
stream.write = stream.pause = stream.resume = function() {};
stream.readable = stream.writable = true;

// 1, mostly defaults
var r1 = repl.start({
  input: stream,
  output: stream,
  terminal: true
});

assert.equal(r1.input, stream);
assert.equal(r1.output, stream);
assert.equal(r1.input, r1.inputStream);
assert.equal(r1.output, r1.outputStream);
assert.equal(r1.terminal, true);
assert.equal(r1.useColors, r1.terminal);
assert.equal(r1.useGlobal, false);
assert.equal(r1.ignoreUndefined, false);
assert.equal(r1.replMode, repl.REPL_MODE_SLOPPY);
assert.equal(r1.historySize, 30);

// test r1 for backwards compact
assert.equal(r1.rli.input, stream);
assert.equal(r1.rli.output, stream);
assert.equal(r1.rli.input, r1.inputStream);
assert.equal(r1.rli.output, r1.outputStream);
assert.equal(r1.rli.terminal, true);
assert.equal(r1.useColors, r1.rli.terminal);

// 2
function writer() {}
function evaler() {}
var r2 = repl.start({
  input: stream,
  output: stream,
  terminal: false,
  useColors: true,
  useGlobal: true,
  ignoreUndefined: true,
  eval: evaler,
  writer: writer,
  replMode: repl.REPL_MODE_STRICT
});
assert.equal(r2.input, stream);
assert.equal(r2.output, stream);
assert.equal(r2.input, r2.inputStream);
assert.equal(r2.output, r2.outputStream);
assert.equal(r2.terminal, false);
assert.equal(r2.useColors, true);
assert.equal(r2.useGlobal, true);
assert.equal(r2.ignoreUndefined, true);
assert.equal(r2.writer, writer);
assert.equal(r2.replMode, repl.REPL_MODE_STRICT);

// test r2 for backwards compact
assert.equal(r2.rli.input, stream);
assert.equal(r2.rli.output, stream);
assert.equal(r2.rli.input, r2.inputStream);
assert.equal(r2.rli.output, r2.outputStream);
assert.equal(r2.rli.terminal, false);

// testing out "magic" replMode
var r3 = repl.start({
  input: stream,
  output: stream,
  writer: writer,
  replMode: repl.REPL_MODE_MAGIC,
  historySize: 50
});

assert.equal(r3.replMode, repl.REPL_MODE_MAGIC);
assert.equal(r3.historySize, 50);
