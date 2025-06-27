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

// Ref: https://github.com/nodejs/node-v0.x-archive/issues/481

const net = require('net');

const server = net.createServer(common.mustCall(function(stream) {
  stream.setTimeout(100);

  stream.resume();

  stream.once('timeout', common.mustCall(function() {
    console.log('timeout');
    // Try to reset the timeout.
    stream.write('WHAT.');
  }));

  stream.on('end', common.mustCall(function() {
    console.log('server side end');
    stream.end();
  }));
}));

server.listen(0, common.mustCall(function() {
  const c = net.createConnection(this.address().port);

  c.on('data', function() {
    c.end();
  });

  c.on('end', function() {
    console.log('client side end');
    server.close();
  });
}));
