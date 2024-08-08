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
const dns = require('dns');

if (!common.hasIPv6) {
  common.printSkipMessage('ipv6 part of test, no IPv6 support');
  return;
}

// Test on IPv6 Server, throws EADDRNOTAVAIL
{
  const host = 'fe80::1';

  const server = net.createServer();

  server.on('error', common.mustCall((e) => {
    assert.strictEqual(e.address, host);
    assert.strictEqual(e.syscall, 'listen');
    assert.strictEqual(e.code, 'EADDRNOTAVAIL');
    assert.strictEqual(e.errno, -49);
  }));

  server.listen(common.PORT + 2, host);
}

// Test on IPv6 Server, picks 127.0.0.1 between that and fe80::1
{

  // Mock dns.lookup
  const originalLookup = dns.lookup;
  dns.lookup = (hostname, options, callback) => {
    if (hostname === 'ipv6_loopback_with_double_entry') {
      callback(null, [{ address: 'fe80::1', family: 6 },
                      { address: '127.0.0.1', family: 4 }]);
    } else {
      originalLookup(hostname, options, callback);
    }
  };

  const host = 'ipv6_loopback_with_double_entry';
  const family4 = 'IPv4';

  const server = net.createServer();

  server.on('error', common.mustNotCall());

  server.listen(common.PORT + 2, host, common.mustCall(() => {
    const address = server.address();
    assert.strictEqual(address.address, '127.0.0.1');
    assert.strictEqual(address.port, common.PORT + 2);
    assert.strictEqual(address.family, family4);
    server.close();
    dns.lookup = originalLookup;
  }));
}
