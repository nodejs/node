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
// This example sets a timeout then immediately attempts to disable the timeout
// https://github.com/joyent/node/pull/2245

const common = require('../common');
const net = require('net');
const assert = require('assert');

const T = 100;

const server = net.createServer(common.mustCall((c) => {
  c.write('hello');
}));

server.listen(0, function() {
  const socket = net.createConnection(this.address().port, 'localhost');

  const s = socket.setTimeout(T, common.mustNotCall());
  assert.ok(s instanceof net.Socket);

  socket.on('data', common.mustCall(() => {
    setTimeout(function() {
      socket.destroy();
      server.close();
    }, T * 2);
  }));

  socket.setTimeout(0);
});
