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

var kPoolSize = 40 * 1024;
var data = '';
for (var i = 0; i < kPoolSize; ++i) {
  data += 'あ'; // 3bytes
}
var receivedSize = 0;
var encoding = 'UTF-8';

var server = net.createServer(function(socket) {
  socket.setEncoding(encoding);
  socket.on('data', function(data) {
    receivedSize += data.length;
  });
  socket.on('end', function() {
    socket.end();
  });
});

server.listen(common.PORT, function() {
  var client = net.createConnection(common.PORT);
  client.on('end', function() {
    server.close();
  });
  client.write(data, encoding);
  client.end();
});

process.on('exit', function() {
  assert.equal(receivedSize, kPoolSize);
});
