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

// Test on IPv4 Server
const family_ipv4 = 'IPv4';
const server_ipv4 = net.createServer();

server_ipv4.on('error', common.mustNotCall());

server_ipv4
  .listen(common.PORT + 1, common.localhostIPv4, common.mustCall(() => {
    const address_ipv4 = server_ipv4.address();
    assert.strictEqual(address_ipv4.address, common.localhostIPv4);
    assert.strictEqual(address_ipv4.port, common.PORT + 1);
    assert.strictEqual(address_ipv4.family, family_ipv4);
    server_ipv4.close();
  }));

if (!common.hasIPv6) {
  common.printSkipMessage('ipv6 part of test, no IPv6 support');
  return;
}

// Test on IPv6 Server
const localhost_ipv6 = '::1';
const family_ipv6 = 'IPv6';
const server_ipv6 = net.createServer();

server_ipv6.on('error', common.mustNotCall());

server_ipv6.listen(common.PORT + 2, localhost_ipv6, common.mustCall(() => {
  const address_ipv6 = server_ipv6.address();
  assert.strictEqual(address_ipv6.address, localhost_ipv6);
  assert.strictEqual(address_ipv6.port, common.PORT + 2);
  assert.strictEqual(address_ipv6.family, family_ipv6);
  server_ipv6.close();
}));

// Test without hostname or ip
const anycast_ipv6 = '::';
const server1 = net.createServer();

server1.on('error', common.mustNotCall());

// Specify the port number
server1.listen(common.PORT + 3, common.mustCall(() => {
  const address = server1.address();
  assert.strictEqual(address.address, anycast_ipv6);
  assert.strictEqual(address.port, common.PORT + 3);
  assert.strictEqual(address.family, family_ipv6);
  server1.close();
}));

// Test without hostname or port
const server2 = net.createServer();

server2.on('error', common.mustNotCall());

// Don't specify the port number
server2.listen(common.mustCall(() => {
  const address = server2.address();
  assert.strictEqual(address.address, anycast_ipv6);
  assert.strictEqual(address.family, family_ipv6);
  server2.close();
}));

// Test without hostname, but with a false-y port
const server3 = net.createServer();

server3.on('error', common.mustNotCall());

// Specify a false-y port number
server3.listen(0, common.mustCall(() => {
  const address = server3.address();
  assert.strictEqual(address.address, anycast_ipv6);
  assert.strictEqual(address.family, family_ipv6);
  server3.close();
}));
