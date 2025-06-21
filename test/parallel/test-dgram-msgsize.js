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

// Send a too big datagram. The destination doesn't matter because it's
// not supposed to get sent out anyway.
const buf = Buffer.allocUnsafe(256 * 1024);
const sock = dgram.createSocket('udp4');
sock.send(buf, 0, buf.length, 12345, '127.0.0.1', common.mustCall(cb));
function cb(err) {
  assert(err instanceof Error);
  assert.strictEqual(err.code, 'EMSGSIZE');
  assert.strictEqual(err.address, '127.0.0.1');
  assert.strictEqual(err.port, 12345);
  assert.strictEqual(err.message, 'send EMSGSIZE 127.0.0.1:12345');
  sock.close();
}
