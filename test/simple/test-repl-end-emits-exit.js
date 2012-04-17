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
    repl = require('repl'),
    terminalExit = 0,
    regularExit = 0;

// create a dummy stream that does nothing
var stream = new Stream();
stream.write = stream.pause = stream.resume = function(){};
stream.readable = stream.writable = true;

function testTerminalMode() {
  var r1 = repl.start({
    input: stream,
    output: stream,
    terminal: true
  });

  process.nextTick(function() {
    // manually fire a ^D keypress
    stream.emit('data', '\u0004');
  });

  r1.on('exit', function() {
    // should be fired from the simulated ^D keypress
    terminalExit++;
    testRegularMode();
  });
}

function testRegularMode() {
  var r2 = repl.start({
    input: stream,
    output: stream,
    terminal: false
  });

  process.nextTick(function() {
    stream.emit('end');
  });

  r2.on('exit', function() {
    // should be fired from the simulated 'end' event
    regularExit++;
  });
}

process.on('exit', function() {
  assert.equal(terminalExit, 1);
  assert.equal(regularExit, 1);
});


// start
testTerminalMode();
