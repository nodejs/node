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
const assert = require('assert');
const cluster = require('cluster');

const SENTINEL = 42;

// workers forcibly exit when control channel is disconnected, if
// their .exitedAfterDisconnect flag isn't set
//
// test this by:
//
// 1 setup worker to wait a short time after disconnect, and exit
//   with a sentinel value
// 2 disconnect worker with cluster's disconnect, confirm sentinel
// 3 disconnect worker with child_process's disconnect, confirm
//   no sentinel value
if (cluster.isWorker) {
  process.on('disconnect', function(msg) {
    setTimeout(function() {
      process.exit(SENTINEL);
    }, 10);
  });
  return;
}

checkUnforced();
checkForced();

function checkUnforced() {
  cluster.fork()
    .on('online', function() { this.disconnect(); })
    .on('exit', common.mustCall(function(status) {
      assert.strictEqual(status, SENTINEL);
    }));
}

function checkForced() {
  cluster.fork()
    .on('online', function() { this.process.disconnect(); })
    .on('exit', common.mustCall(function(status) {
      assert.strictEqual(status, 0);
    }));
}
