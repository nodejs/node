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
const dgram = require('dgram');

{
  // IPv4 Test
  const socket = dgram.createSocket('udp4');

  socket.on('listening', common.mustCall(() => {
    const address = socket.address();

    assert.strictEqual(address.address, common.localhostIPv4);
    assert.strictEqual(typeof address.port, 'number');
    assert.ok(isFinite(address.port));
    assert.ok(address.port > 0);
    assert.strictEqual(address.family, 'IPv4');
    socket.close();
  }));

  socket.on('error', (err) => {
    socket.close();
    assert.fail(`Unexpected error on udp4 socket. ${err.toString()}`);
  });

  socket.bind(0, common.localhostIPv4);
}

if (common.hasIPv6) {
  // IPv6 Test
  const socket = dgram.createSocket('udp6');
  const localhost = '::1';

  socket.on('listening', common.mustCall(() => {
    const address = socket.address();

    assert.strictEqual(address.address, localhost);
    assert.strictEqual(typeof address.port, 'number');
    assert.ok(isFinite(address.port));
    assert.ok(address.port > 0);
    assert.strictEqual(address.family, 'IPv6');
    socket.close();
  }));

  socket.on('error', (err) => {
    socket.close();
    assert.fail(`Unexpected error on udp6 socket. ${err.toString()}`);
  });

  socket.bind(0, localhost);
}

{
  // Verify that address() throws if the socket is not bound.
  const socket = dgram.createSocket('udp4');

  assert.throws(() => {
    socket.address();
  }, /^Error: getsockname EBADF$/);
}
