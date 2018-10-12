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
const net = require('net');
const assert = require('assert');
const N = 20;
let client_error_count = 0;
let disconnect_count = 0;

const c = net.createConnection(common.PORT);

c.on('connect', common.mustNotCall('client should not have connected'));

c.on('error', common.mustCall((e) => {
  client_error_count++;
  assert.strictEqual(e.code, 'ECONNREFUSED');
}, N + 1));

c.on('close', common.mustCall(() => {
  if (disconnect_count++ < N)
    c.connect(common.PORT); // reconnect
}, N + 1));

process.on('exit', function() {
  assert.strictEqual(disconnect_count, N + 1);
  assert.strictEqual(client_error_count, N + 1);
});
