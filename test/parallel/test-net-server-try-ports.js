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
// This test binds to one port, then attempts to start a server on that
// port. It should be EADDRINUSE but be able to then bind to another port.
const common = require('../common');
const assert = require('assert');
const net = require('net');

const server1 = net.Server();

const server2 = net.Server();

server2.on('error', common.mustCall(function(e) {
  assert.strictEqual(e.code, 'EADDRINUSE');

  server2.listen(0, common.mustCall(function() {
    server1.close();
    server2.close();
  }));
}));

server1.listen(0, common.mustCall(function() {
  // This should make server2 emit EADDRINUSE
  server2.listen(this.address().port);
}));
