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

const net = require('net');

let conns_closed = 0;

const remoteAddrCandidates = [ common.localhostIPv4,
                               '::1',
                               '::ffff:127.0.0.1' ];

const remoteFamilyCandidates = ['IPv4', 'IPv6'];

const server = net.createServer(common.mustCall(function(socket) {
  assert.ok(remoteAddrCandidates.includes(socket.remoteAddress),
            `Invalid remoteAddress: ${socket.remoteAddress}`);
  assert.ok(remoteFamilyCandidates.includes(socket.remoteFamily),
            `Invalid remoteFamily: ${socket.remoteFamily}`);
  assert.ok(socket.remotePort);
  assert.notStrictEqual(socket.remotePort, this.address().port);
  socket.on('end', function() {
    if (++conns_closed === 2) server.close();
  });
  socket.on('close', function() {
    assert.ok(remoteAddrCandidates.includes(socket.remoteAddress));
    assert.ok(remoteFamilyCandidates.includes(socket.remoteFamily));
  });
  socket.resume();
}, 2));

server.listen(0, function() {
  const client = net.createConnection(this.address().port, '127.0.0.1');
  const client2 = net.createConnection(this.address().port);

  assert.strictEqual(client.remoteAddress, undefined);
  assert.strictEqual(client.remoteFamily, undefined);
  assert.strictEqual(client.remotePort, undefined);
  assert.strictEqual(client2.remoteAddress, undefined);
  assert.strictEqual(client2.remoteFamily, undefined);
  assert.strictEqual(client2.remotePort, undefined);

  client.on('connect', function() {
    assert.ok(remoteAddrCandidates.includes(client.remoteAddress));
    assert.ok(remoteFamilyCandidates.includes(client.remoteFamily));
    assert.strictEqual(client.remotePort, server.address().port);
    client.end();
  });
  client.on('close', function() {
    assert.ok(remoteAddrCandidates.includes(client.remoteAddress));
    assert.ok(remoteFamilyCandidates.includes(client.remoteFamily));
  });
  client2.on('connect', function() {
    assert.ok(remoteAddrCandidates.includes(client2.remoteAddress));
    assert.ok(remoteFamilyCandidates.includes(client2.remoteFamily));
    assert.strictEqual(client2.remotePort, server.address().port);
    client2.end();
  });
  client2.on('close', function() {
    assert.ok(remoteAddrCandidates.includes(client2.remoteAddress));
    assert.ok(remoteFamilyCandidates.includes(client2.remoteFamily));
  });
});
