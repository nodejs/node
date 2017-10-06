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
// Testing to send an handle twice to the parent process.

const common = require('../common');
const assert = require('assert');
const cluster = require('cluster');
const net = require('net');

const workers = {
  toStart: 1
};

if (cluster.isMaster) {
  for (let i = 0; i < workers.toStart; ++i) {
    const worker = cluster.fork();
    worker.on('exit', common.mustCall(function(code, signal) {
      assert.strictEqual(code, 0, `Worker exited with an error code: ${code}`);
      assert.strictEqual(signal, null, `Worker exited by a signal: ${signal}`);
    }));
  }
} else {
  const server = net.createServer(function(socket) {
    process.send('send-handle-1', socket);
    process.send('send-handle-2', socket);
  });

  server.listen(0, function() {
    const client = net.connect({
      host: 'localhost',
      port: server.address().port
    });
    client.on('close', common.mustCall(() => { cluster.worker.disconnect(); }));
    setTimeout(function() { client.end(); }, 50);
  }).on('error', function(e) {
    console.error(e);
    assert.fail('server.listen failed');
    cluster.worker.disconnect();
  });
}
