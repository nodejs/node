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

// IPv4 Test
const socket_ipv4 = dgram.createSocket('udp4');

socket_ipv4.on('listening', common.mustNotCall());

socket_ipv4.on('error', common.mustCall(function(e) {
  assert.strictEqual(e.port, undefined);
  assert.strictEqual(e.message, 'bind EADDRNOTAVAIL 1.1.1.1');
  assert.strictEqual(e.address, '1.1.1.1');
  assert.strictEqual(e.code, 'EADDRNOTAVAIL');
  socket_ipv4.close();
}));

socket_ipv4.bind(0, '1.1.1.1');

// IPv6 Test
const socket_ipv6 = dgram.createSocket('udp6');

socket_ipv6.on('listening', common.mustNotCall());

socket_ipv6.on('error', common.mustCall(function(e) {
  // EAFNOSUPPORT or EPROTONOSUPPORT means IPv6 is disabled on this system.
  const allowed = ['EADDRNOTAVAIL', 'EAFNOSUPPORT', 'EPROTONOSUPPORT'];
  assert(allowed.includes(e.code), `'${e.code}' was not one of ${allowed}.`);
  assert.strictEqual(e.port, undefined);
  assert.strictEqual(e.message, `bind ${e.code} 111::1`);
  assert.strictEqual(e.address, '111::1');
  socket_ipv6.close();
}));

socket_ipv6.bind(0, '111::1');
