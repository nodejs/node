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

// Test that the family option of net.connect is honored.

'use strict';
const common = require('../common');
if (!common.hasIPv6)
  common.skip('no IPv6 support');

const assert = require('assert');
const net = require('net');

const hostAddrIPv6 = '::1';
const HOSTNAME = 'dummy';

const server = net.createServer({ allowHalfOpen: true }, (socket) => {
  socket.resume();
  socket.on('end', common.mustCall());
  socket.end();
});

function tryConnect() {
  const connectOpt = {
    host: HOSTNAME,
    port: server.address().port,
    family: 6,
    allowHalfOpen: true,
    lookup: common.mustCall((addr, opt, cb) => {
      assert.strictEqual(addr, HOSTNAME);
      assert.strictEqual(opt.family, 6);
      cb(null, hostAddrIPv6, opt.family);
    })
  };
  // No `mustCall`, since test could skip, and it's the only path to `close`.
  const client = net.connect(connectOpt, () => {
    client.resume();
    client.on('end', () => {
      // Wait for next uv tick and make sure the socket stream is writable.
      setTimeout(function() {
        assert(client.writable);
        client.end();
      }, 10);
    });
    client.on('close', () => server.close());
  });
}

server.listen(0, hostAddrIPv6, tryConnect);
