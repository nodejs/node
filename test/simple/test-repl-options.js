// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

var common = require('../common'),
    assert = require('assert'),
    Stream = require('stream'),
    repl = require('repl');

common.globalCheck = false;

// create a dummy stream that does nothing
var stream = new Stream();
stream.write = stream.pause = stream.resume = function(){};
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
  writer: writer
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

// test r2 for backwards compact
assert.equal(r2.rli.input, stream);
assert.equal(r2.rli.output, stream);
assert.equal(r2.rli.input, r2.inputStream);
assert.equal(r2.rli.output, r2.outputStream);
assert.equal(r2.rli.terminal, false);

