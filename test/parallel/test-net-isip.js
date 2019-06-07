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

const ip = [
  ['127.0.0.1', 4],
  ['8.8.8.8', 4],
  ['x127.0.0.1', 0],
  ['example.com', 0],
  ['0000:0000:0000:0000:0000:0000:0000:0000', 6],
  ['0000:0000:0000:0000:0000:0000:0000:0000::0000', 0],
  ['1050:0:0:0:5:600:300c:326b', 6],
  ['2001:252:0:1::2008:6', 6],
  ['2001::', 6],
  ['2001:dead::', 6],
  ['2001:dead:beef::', 6],
  ['2001:dead:beef:1::', 6],
  ['ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff', 6],
  [':2001:252:0:1::2008:6:', 0],
  [':2001:252:0:1::2008:6', 0],
  ['2001:252:0:1::2008:6:', 0],
  ['2001:252::1::2008:6', 0],
  ['::2001:252:1:2008:6', 6],
  ['::2001:252:1:1.1.1.1', 6],
  ['::2001:252:1:255.255.255.255', 6],
  ['::2001:252:1:255.255.255.255.76', 0],
  ['::anything', 0],
  ['::1', 6],
  ['::', 6],
  ['0000:0000:0000:0000:0000:0000:12345:0000', 0],
  ['0', 0]
];

ip.forEach(([ip, version]) => {
  assert.strictEqual(net.isIP(ip), version);
  assert.strictEqual(net.isIP({ toString: () => ip }), version);
});
