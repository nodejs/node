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

var received = false;

// issue 3118
var client = net.connect(12345, "localhost");
client.on('error', function(err) {
  assert.equal(err.message, 'connect ECONNREFUSED');
});
client.pause();
client.resume();

var server = net.createServer(function(socket) {
  socket.on('data', function() {
    socket.write('important data\r\n');
  });

  socket.on('end', function() {
    socket.end();
  });

  socket.on('close', function(hadError) {
    assert.equal(false, hadError);
  });
});

server.listen(common.PORT, function() {
  var resumed = false;

  var client = net.createConnection(common.PORT);
  client.setEncoding('UTF8');

  client.on('connect', function() {
    client.write('start\r\n');
    client.pause();

    setTimeout(function() {
      client.resume();
      resumed = true;
    }, 500);
  });

  client.on('data', function(chunk) {
    assert.equal('important data\r\n', chunk);

    assert(resumed, 'got data before resume');

    received = true;
    client.end();
  });

  client.on('close', function(hadError) {
    assert.equal(false, hadError);
    server.close();
  });
});

process.on('exit', function() {
  assert(received);
});

