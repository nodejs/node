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
require('../common');
const assert = require('assert');
const net = require('net');

assert.strictEqual(net.isIP('127.0.0.1'), 4);
assert.strictEqual(net.isIP('x127.0.0.1'), 0);
assert.strictEqual(net.isIP('example.com'), 0);
assert.strictEqual(net.isIP('0000:0000:0000:0000:0000:0000:0000:0000'), 6);
assert.strictEqual(net.isIP('0000:0000:0000:0000:0000:0000:0000:0000::0000'),
                   0);
assert.strictEqual(net.isIP('1050:0:0:0:5:600:300c:326b'), 6);
assert.strictEqual(net.isIP('2001:252:0:1::2008:6'), 6);
assert.strictEqual(net.isIP('2001:dead:beef:1::2008:6'), 6);
assert.strictEqual(net.isIP('2001::'), 6);
assert.strictEqual(net.isIP('2001:dead::'), 6);
assert.strictEqual(net.isIP('2001:dead:beef::'), 6);
assert.strictEqual(net.isIP('2001:dead:beef:1::'), 6);
assert.strictEqual(net.isIP('ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff'), 6);
assert.strictEqual(net.isIP(':2001:252:0:1::2008:6:'), 0);
assert.strictEqual(net.isIP(':2001:252:0:1::2008:6'), 0);
assert.strictEqual(net.isIP('2001:252:0:1::2008:6:'), 0);
assert.strictEqual(net.isIP('2001:252::1::2008:6'), 0);
assert.strictEqual(net.isIP('::2001:252:1:2008:6'), 6);
assert.strictEqual(net.isIP('::2001:252:1:1.1.1.1'), 6);
assert.strictEqual(net.isIP('::2001:252:1:255.255.255.255'), 6);
assert.strictEqual(net.isIP('::2001:252:1:255.255.255.255.76'), 0);
assert.strictEqual(net.isIP('::anything'), 0);
assert.strictEqual(net.isIP('::1'), 6);
assert.strictEqual(net.isIP('::'), 6);
assert.strictEqual(net.isIP('0000:0000:0000:0000:0000:0000:12345:0000'), 0);
assert.strictEqual(net.isIP('0'), 0);
assert.strictEqual(net.isIP(), 0);
assert.strictEqual(net.isIP(''), 0);
assert.strictEqual(net.isIP(null), 0);
assert.strictEqual(net.isIP(123), 0);
assert.strictEqual(net.isIP(true), 0);
assert.strictEqual(net.isIP({}), 0);
assert.strictEqual(net.isIP({ toString: () => '::2001:252:1:255.255.255.255' }),
                   6);
assert.strictEqual(net.isIP({ toString: () => '127.0.0.1' }), 4);
assert.strictEqual(net.isIP({ toString: () => 'bla' }), 0);

assert.strictEqual(net.isIPv4('127.0.0.1'), true);
assert.strictEqual(net.isIPv4('example.com'), false);
assert.strictEqual(net.isIPv4('2001:252:0:1::2008:6'), false);
assert.strictEqual(net.isIPv4(), false);
assert.strictEqual(net.isIPv4(''), false);
assert.strictEqual(net.isIPv4(null), false);
assert.strictEqual(net.isIPv4(123), false);
assert.strictEqual(net.isIPv4(true), false);
assert.strictEqual(net.isIPv4({}), false);
assert.strictEqual(net.isIPv4({
  toString: () => '::2001:252:1:255.255.255.255'
}), false);
assert.strictEqual(net.isIPv4({ toString: () => '127.0.0.1' }), true);
assert.strictEqual(net.isIPv4({ toString: () => 'bla' }), false);

assert.strictEqual(net.isIPv6('127.0.0.1'), false);
assert.strictEqual(net.isIPv6('example.com'), false);
assert.strictEqual(net.isIPv6('2001:252:0:1::2008:6'), true);
assert.strictEqual(net.isIPv6(), false);
assert.strictEqual(net.isIPv6(''), false);
assert.strictEqual(net.isIPv6(null), false);
assert.strictEqual(net.isIPv6(123), false);
assert.strictEqual(net.isIPv6(true), false);
assert.strictEqual(net.isIPv6({}), false);
assert.strictEqual(net.isIPv6({
  toString: () => '::2001:252:1:255.255.255.255'
}), true);
assert.strictEqual(net.isIPv6({ toString: () => '127.0.0.1' }), false);
assert.strictEqual(net.isIPv6({ toString: () => 'bla' }), false);
