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
// Testing mutual send of handles: from master to worker, and from worker to
// master.

const common = require('../common');
const assert = require('assert');
const cluster = require('cluster');
const net = require('net');

if (cluster.isMaster) {
  const worker = cluster.fork();
  worker.on('exit', function(code, signal) {
    assert.strictEqual(code, 0, 'Worker exited with an error code');
    assert(!signal, 'Worker exited by a signal');
    server.close();
  });

  const server = net.createServer(function(socket) {
    worker.send('handle', socket);
  });

  server.listen(common.PORT, function() {
    worker.send('listen');
  });
} else {
  process.on('message', function(msg, handle) {
    if (msg === 'listen') {
      const client1 = net.connect({ host: 'localhost', port: common.PORT });
      const client2 = net.connect({ host: 'localhost', port: common.PORT });
      let waiting = 2;
      client1.on('close', onclose);
      client2.on('close', onclose);
      function onclose() {
        if (--waiting === 0)
          cluster.worker.disconnect();
      }
      setTimeout(function() {
        client1.end();
        client2.end();
      }, 50);
    } else {
      process.send('reply', handle);
    }
  });
}
