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
{
  const family = 4;
  const server = net.createServer();

  server.on('error', common.mustNotCall());

  server
    .listen(common.PORT + 1, common.localhostIPv4, common.mustCall(() => {
      const address4 = server.address();
      assert.strictEqual(address4.address, common.localhostIPv4);
      assert.strictEqual(address4.port, common.PORT + 1);
      assert.strictEqual(address4.family, family);
      server.close();
    }));
}

if (!common.hasIPv6) {
  common.printSkipMessage('ipv6 part of test, no IPv6 support');
  return;
}

const family6 = 6;
const anycast6 = '::';

// Test on IPv6 Server
{
  const localhost = '::1';

  const server = net.createServer();

  server.on('error', common.mustNotCall());

  server.listen(common.PORT + 2, localhost, common.mustCall(() => {
    const address = server.address();
    assert.strictEqual(address.address, localhost);
    assert.strictEqual(address.port, common.PORT + 2);
    assert.strictEqual(address.family, family6);
    server.close();
  }));
}

// Test without hostname or ip
{
  const server = net.createServer();

  server.on('error', common.mustNotCall());

  // Specify the port number
  server.listen(common.PORT + 3, common.mustCall(() => {
    const address = server.address();
    assert.strictEqual(address.address, anycast6);
    assert.strictEqual(address.port, common.PORT + 3);
    assert.strictEqual(address.family, family6);
    server.close();
  }));
}

// Test without hostname or port
{
  const server = net.createServer();

  server.on('error', common.mustNotCall());

  // Don't specify the port number
  server.listen(common.mustCall(() => {
    const address = server.address();
    assert.strictEqual(address.address, anycast6);
    assert.strictEqual(address.family, family6);
    server.close();
  }));
}

// Test without hostname, but with a false-y port
{
  const server = net.createServer();

  server.on('error', common.mustNotCall());

  // Specify a false-y port number
  server.listen(0, common.mustCall(() => {
    const address = server.address();
    assert.strictEqual(address.address, anycast6);
    assert.strictEqual(address.family, family6);
    server.close();
  }));
}
