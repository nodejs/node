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
const net = require('net');

if (cluster.isMaster) {
  const worker1 = cluster.fork();

  worker1.on('message', common.mustCall(function(msg) {
    assert.strictEqual(msg, 'success');
    const worker2 = cluster.fork();

    worker2.on('message', common.mustCall(function(msg) {
      assert.strictEqual(msg, 'server2:EADDRINUSE');
      worker1.kill();
      worker2.kill();
    }));
  }));
} else {
  const server1 = net.createServer(common.mustNotCall());
  const server2 = net.createServer(common.mustNotCall());

  server1.on('error', function(err) {
    // no errors expected
    process.send(`server1:${err.code}`);
  });

  server2.on('error', function(err) {
    // an error is expected on the second worker
    process.send(`server2:${err.code}`);
  });

  server1.listen({
    host: 'localhost',
    port: common.PORT,
    exclusive: false
  }, common.mustCall(function() {
    server2.listen({ port: common.PORT + 1, exclusive: true },
                   common.mustCall(function() {
                     // the first worker should succeed
                     process.send('success');
                   })
    );
  }));
}
