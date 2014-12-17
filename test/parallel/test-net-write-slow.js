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

var SIZE = 2E5;
var N = 10;
var flushed = 0;
var received = 0;
var buf = new Buffer(SIZE);
buf.fill(0x61); // 'a'

var server = net.createServer(function(socket) {
  socket.setNoDelay();
  socket.setTimeout(1000);
  socket.on('timeout', function() {
    assert.fail('flushed: ' + flushed +
                ', received: ' + received + '/' + SIZE * N);
  });

  for (var i = 0; i < N; ++i) {
    socket.write(buf, function() {
      ++flushed;
      if (flushed === N) {
        socket.setTimeout(0);
      }
    });
  }
  socket.end();

}).listen(common.PORT, function() {
  var conn = net.connect(common.PORT);
  conn.on('data', function(buf) {
    received += buf.length;
    conn.pause();
    setTimeout(function() {
      conn.resume();
    }, 20);
  });
  conn.on('end', function() {
    server.close();
  });
});

process.on('exit', function() {
  assert.equal(received, SIZE * N);
});
