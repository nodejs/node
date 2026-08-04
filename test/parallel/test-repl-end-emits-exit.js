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

'use strict';
require('../common');
const { startNewREPLServer } = require('../common/repl');
const assert = require('assert');
let terminalExit = 0;
let regularExit = 0;

function testTerminalMode() {
  const { replServer, input } = startNewREPLServer({ terminal: true });

  process.nextTick(function() {
    // Manually fire a ^D keypress
    input.emit('data', '\u0004');
  });

  replServer.on('exit', function() {
    // Should be fired from the simulated ^D keypress
    terminalExit++;
    testRegularMode();
  });
}

function testRegularMode() {
  const { replServer, input } = startNewREPLServer({ terminal: true });

  process.nextTick(function() {
    input.emit('end');
  });

  replServer.on('exit', function() {
    // Should be fired from the simulated 'end' event
    regularExit++;
  });
}

process.on('exit', function() {
  assert.strictEqual(terminalExit, 1);
  assert.strictEqual(regularExit, 1);
});


// start
testTerminalMode();
