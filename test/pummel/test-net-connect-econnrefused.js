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

// verify that connect reqs are properly cleaned up

var common = require('../common');
var assert = require('assert');
var net = require('net');

var ROUNDS = 5;
var ATTEMPTS_PER_ROUND = 200;
var rounds = 1;
var reqs = 0;

pummel();

function pummel() {
  console.log('Round', rounds, '/', ROUNDS);

  for (var pending = 0; pending < ATTEMPTS_PER_ROUND; pending++) {
    net.createConnection(common.PORT).on('error', function(err) {
      assert.equal(err.code, 'ECONNREFUSED');
      if (--pending > 0) return;
      if (rounds == ROUNDS) return check();
      rounds++;
      pummel();
    });
    reqs++;
  }
}

function check() {
  setTimeout(function() {
    assert.equal(process._getActiveRequests().length, 0);
    assert.equal(process._getActiveHandles().length, 1); // the timer
    check_called = true;
  }, 0);
}
var check_called = false;

process.on('exit', function() {
  assert.equal(rounds, ROUNDS);
  assert.equal(reqs, ROUNDS * ATTEMPTS_PER_ROUND);
  assert(check_called);
});
