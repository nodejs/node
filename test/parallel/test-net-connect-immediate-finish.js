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

// This tests that if the socket is still in the 'connecting' state
// when the user calls socket.end() ('finish'), the socket would emit
// 'connect' and defer the handling until the 'connect' event is handled.

const common = require('../common');
const assert = require('assert');
const net = require('net');

const { addresses } = require('../common/internet');
const {
  errorLookupMock,
  mockedErrorCode,
  mockedSysCall
} = require('../common/dns');

const client = net.connect({
  host: addresses.INVALID_HOST,
  port: 80, // Port number doesn't matter because host name is invalid
  lookup: common.mustCall(errorLookupMock())
}, common.mustNotCall());

client.once('error', common.mustCall((err) => {
  assert(err);
  assert.strictEqual(err.code, err.errno);
  assert.strictEqual(err.code, mockedErrorCode);
  assert.strictEqual(err.host, err.hostname);
  assert.strictEqual(err.host, addresses.INVALID_HOST);
  assert.strictEqual(err.syscall, mockedSysCall);
}));

client.end();
