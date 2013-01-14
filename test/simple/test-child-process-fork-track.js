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

var assert = require('assert');
var common = require('../common');
var fork = require('child_process').fork;
var net = require('net');
var count = 12;

if (process.argv[2] === 'child') {
  var sockets = [];
  var id = process.argv[3];

  process.on('message', function(m, socket) {
    if (socket) {
      sockets.push(socket);
    }

    if (m.cmd === 'close') {
      sockets[m.id].once('close', function() {
        process.send({ id: m.id, status: 'closed' });
      });
      sockets[m.id].destroy();
    }
  });
} else {
  var child = fork(process.argv[1], ['child']);

  var server = net.createServer();
  var sockets = [];
  var closed = 0;

  server.on('connection', function(socket) {
    child.send({ cmd: 'new' }, socket, { track: true });
    sockets.push(socket);

    socket.once('close', function() {
      console.error('[m] socket closed');
      closed++;
      assert.equal(closed + server.connections, count);
      if (server.connections === 0) server.close();
    });

    if (sockets.length === count) {
      closeSockets();
    }
  });

  var disconnected = 0;
  server.on('listening', function() {

    var j = count, client;
    while (j--) {
      client = net.connect(common.PORT, '127.0.0.1');
      client.on('close', function() {
        console.error('[m] CLIENT: close event');
        disconnected += 1;
      });
      // XXX This resume() should be unnecessary.
      // a stream high water mark should be enough to keep
      // consuming the input.
      client.resume();
    }
  });

  function closeSockets(i) {
    if (!i) i = 0;
    if (i === count) return;

    child.send({ id: i, cmd: 'close' });
    child.once('message', function(m) {
      assert(m.status === 'closed');
      closeSockets(i + 1);
    });
  };

  var closeEmitted = false;
  server.on('close', function() {
    console.error('[m] server close');
    closeEmitted = true;

    child.kill();
  });

  server.listen(common.PORT, '127.0.0.1');

  process.on('exit', function() {
    assert.equal(disconnected, count);
    assert.equal(closed, count);
    assert.ok(closeEmitted);
  });
}
