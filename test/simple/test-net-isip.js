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

var common = require('../common');
var assert = require('assert');
var net = require('net');

assert.equal(net.isIP('127.0.0.1'), 4);
assert.equal(net.isIP('x127.0.0.1'), 0);
assert.equal(net.isIP('example.com'), 0);
assert.equal(net.isIP('0000:0000:0000:0000:0000:0000:0000:0000'), 6);
assert.equal(net.isIP('0000:0000:0000:0000:0000:0000:0000:0000::0000'), 0);
assert.equal(net.isIP('1050:0:0:0:5:600:300c:326b'), 6);
assert.equal(net.isIP('2001:252:0:1::2008:6'), 6);
assert.equal(net.isIP('2001:dead:beef:1::2008:6'), 6);
assert.equal(net.isIP('ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff'), 6);

assert.equal(net.isIPv4('127.0.0.1'), true);
assert.equal(net.isIPv4('example.com'), false);
assert.equal(net.isIPv4('2001:252:0:1::2008:6'), false);

assert.equal(net.isIPv6('127.0.0.1'), false);
assert.equal(net.isIPv4('example.com'), false);
assert.equal(net.isIPv6('2001:252:0:1::2008:6'), true);

