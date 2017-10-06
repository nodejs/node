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
const common = require('../common');
const net = require('net');

process.once('beforeExit', common.mustCall(tryImmediate));

function tryImmediate() {
  setImmediate(common.mustCall(() => {
    process.once('beforeExit', common.mustCall(tryTimer));
  }));
}

function tryTimer() {
  setTimeout(common.mustCall(() => {
    process.once('beforeExit', common.mustCall(tryListen));
  }), 1);
}

function tryListen() {
  net.createServer()
    .listen(0)
    .on('listening', common.mustCall(function() {
      this.close();
      process.once('beforeExit', common.mustCall(tryRepeatedTimer));
    }));
}

// test that a function invoked from the beforeExit handler can use a timer
// to keep the event loop open, which can use another timer to keep the event
// loop open, etc.
function tryRepeatedTimer() {
  const N = 5;
  let n = 0;
  const repeatedTimer = common.mustCall(function() {
    if (++n < N)
      setTimeout(repeatedTimer, 1);
  }, N);
  setTimeout(repeatedTimer, 1);
}
