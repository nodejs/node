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

var tcpPort = common.PORT;
var fooWritten = false;
var connectHappened = false;

var tcp = net.Server(function(s) {
  tcp.close();

  console.log('tcp server connection');

  var buf = '';
  s.on('data', function(d) {
    buf += d;
  });

  s.on('end', function() {
    assert.equal('foobar', buf);
    console.log('tcp socket disconnect');
    s.end();
  });

  s.on('error', function(e) {
    console.log('tcp server-side error: ' + e.message);
    process.exit(1);
  });
});

tcp.listen(common.PORT, function () {
  var socket = net.Stream();

  console.log('Connecting to socket');

  socket.connect(tcpPort, function() {
    console.log('socket connected');
    connectHappened = true;
  });

  assert.equal('opening', socket.readyState);

  var r = socket.write('foo', function () {
    fooWritten = true;
    assert.ok(connectHappened);
    console.error("foo written");
  });

  assert.equal(false, r);
  socket.end('bar');

  assert.equal('opening', socket.readyState);
});

process.on('exit', function () {
  assert.ok(connectHappened);
  assert.ok(fooWritten);
});
