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
// Check that having a worker bind to a port that's already taken doesn't
// leave the master process in a confused state. Releasing the port and
// trying again should Just Work[TM].

const common = require('../common');
const assert = require('assert');
const fork = require('child_process').fork;
const net = require('net');

const id = String(process.argv[2]);
const port = String(process.argv[3]);

if (id === 'undefined') {
  const server = net.createServer(common.mustNotCall());
  server.listen(0, function() {
    const worker = fork(__filename, ['worker', server.address().port]);
    worker.on('message', function(msg) {
      if (msg !== 'stop-listening') return;
      server.close(function() {
        worker.send('stopped-listening');
      });
    });
  });
} else if (id === 'worker') {
  let server = net.createServer(common.mustNotCall());
  server.listen(port, common.mustNotCall());
  server.on('error', common.mustCall(function(e) {
    assert(e.code, 'EADDRINUSE');
    process.send('stop-listening');
    process.once('message', function(msg) {
      if (msg !== 'stopped-listening') return;
      server = net.createServer(common.mustNotCall());
      server.listen(port, common.mustCall(function() {
        server.close();
      }));
    });
  }));
} else {
  assert(0);  // Bad argument.
}
