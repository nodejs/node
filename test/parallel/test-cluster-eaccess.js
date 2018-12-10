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

// Test that errors propagated from cluster workers are properly
// received in their master. Creates an EADDRINUSE condition by forking
// a process in child cluster and propagates the error to the master.

const assert = require('assert');
const cluster = require('cluster');
const fork = require('child_process').fork;
const net = require('net');

if (cluster.isMaster && process.argv.length !== 3) {
  // cluster.isMaster
  const tmpdir = require('../common/tmpdir');
  tmpdir.refresh();
  const PIPE_NAME = common.PIPE;
  const worker = cluster.fork({ PIPE_NAME });

  // makes sure master is able to fork the worker
  cluster.on('fork', common.mustCall());

  // makes sure the worker is ready
  worker.on('online', common.mustCall());

  worker.on('message', common.mustCall(function(err) {
    // Disconnect first, so that we will not leave zombies
    worker.disconnect();
    assert.strictEqual(err.code, 'EADDRINUSE');
  }));
} else if (process.argv.length !== 3) {
  // cluster.worker
  const PIPE_NAME = process.env.PIPE_NAME;
  const cp = fork(__filename, [PIPE_NAME], { stdio: 'inherit' });

  // Message from the child indicates it's ready and listening
  cp.on('message', common.mustCall(function() {
    const server = net.createServer().listen(PIPE_NAME, function() {
      // message child process so that it can exit
      cp.send('end');
      // inform master about the unexpected situation
      process.send('PIPE should have been in use.');
    });

    server.on('error', function(err) {
      // message to child process tells it to exit
      cp.send('end');
      // propagate error to parent
      process.send(err);
    });
  }));
} else if (process.argv.length === 3) {
  // child process (of cluster.worker)
  const PIPE_NAME = process.argv[2];

  const server = net.createServer().listen(PIPE_NAME, common.mustCall(() => {
    process.send('listening');
  }));
  process.once('message', common.mustCall(() => server.close()));
} else {
  assert.fail('Impossible state');
}
