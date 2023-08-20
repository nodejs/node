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
const net = require('net');

const kPoolSize = 40 * 1024;
const data = 'あ'.repeat(kPoolSize);
const encoding = 'UTF-8';

const server = net.createServer(common.mustCall(function(socket) {
  let receivedSize = 0;

  socket.setEncoding(encoding);
  socket.on('data', function(data) {
    receivedSize += data.length;
  });
  socket.on('end', common.mustCall(function() {
    assert.strictEqual(receivedSize, kPoolSize);
    socket.end();
  }));
}));

server.listen(0, function() {
  const client = net.createConnection(this.address().port);
  client.on('end', function() {
    server.close();
  });
  client.write(data, encoding);
  client.end();
});
