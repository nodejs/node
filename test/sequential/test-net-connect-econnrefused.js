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
// Verify that connect reqs are properly cleaned up.

const common = require('../common');
const assert = require('assert');
const net = require('net');

const ROUNDS = 5;
const ATTEMPTS_PER_ROUND = 50;
let rounds = 1;
let reqs = 0;

let port;
const server = net.createServer().listen(0, common.mustCall(() => {
  port = server.address().port;
  server.close(common.mustCall(pummel));
}));

function pummel() {
  let pending;
  for (pending = 0; pending < ATTEMPTS_PER_ROUND; pending++) {
    net.createConnection(port).on('error', function(err) {
      console.log('pending', pending, 'rounds', rounds);
      assert.strictEqual(err.code, 'ECONNREFUSED');
      if (--pending > 0) return;
      if (rounds === ROUNDS) return check();
      rounds++;
      pummel();
    });
    reqs++;
  }
}

function check() {
  setTimeout(common.mustCall(function() {
    assert.strictEqual(process._getActiveRequests().length, 0);
    const activeHandles = process._getActiveHandles();
    assert.ok(activeHandles.every((val) => val.constructor.name !== 'Socket'));
  }), 0);
}

process.on('exit', function() {
  assert.strictEqual(rounds, ROUNDS);
  assert.strictEqual(reqs, ROUNDS * ATTEMPTS_PER_ROUND);
});
