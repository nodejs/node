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

// This tests that the family option of net.connect is hornored.

'use strict';
const common = require('../common');

common.skipIfNoIpv6Localhost((ipv6Host) => {
  const assert = require('assert');
  const net = require('net');

  const server = net.createServer({
    allowHalfOpen: true
  }, common.mustCall((socket) => {
    assert.strictEqual('::1', socket.remoteAddress);
    socket.resume();
    socket.on('end', common.mustCall());
    socket.end();
  }));

  server.listen(0, '::1', common.mustCall(tryConnect));

  function tryConnect() {
    const client = net.connect({
      host: ipv6Host,
      port: server.address().port,
      family: 6,
      allowHalfOpen: true
    }, common.mustCall(() => {
      console.error('client connect cb');
      client.resume();
      client.on('end', common.mustCall(function() {
        setTimeout(function() {
          assert(client.writable);
          client.end();
        }, 10);
      }));
      client.on('close', function() {
        server.close();
      });
    }));
  }
});
