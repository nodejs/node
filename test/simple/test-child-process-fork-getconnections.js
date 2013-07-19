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
    if (m.cmd === 'new') {
      assert(socket);
      assert(socket instanceof net.Socket, 'should be a net.Socket');
      sockets.push(socket);
      socket.on('end', function() {
        if (!this.closingOnPurpose)
          throw new Error('[c] closing by accident!');
      });
    }

    if (m.cmd === 'close') {
      assert.equal(socket, undefined);
      sockets[m.id].once('close', function() {
        process.send({ id: m.id, status: 'closed' });
      });
      sockets[m.id].destroy();
    }
  });

} else {
  var child = fork(process.argv[1], ['child']);

  child.on('exit', function(code, signal) {
    if (!childKilled)
      throw new Error('child died unexpectedly!');
  });

  var server = net.createServer();
  var sockets = [];
  var sent = 0;

  server.on('connection', function(socket) {
    child.send({ cmd: 'new' }, socket, { track: false });
    sockets.push(socket);

    if (sockets.length === count) {
      closeSockets(0);
    }
  });

  var disconnected = 0;
  var clients = [];
  server.on('listening', function() {
    var j = count, client;
    while (j--) {
      client = net.connect(common.PORT, '127.0.0.1');
      client.id = j;
      client.on('close', function() {
        disconnected += 1;
      });
      clients.push(client);
    }
  });

  var childKilled = false;
  function closeSockets(i) {
    if (i === count) {
      childKilled = true;
      server.close();
      child.kill();
      return;
    }

    sent++;
    child.send({ id: i, cmd: 'close' });
    child.once('message', function(m) {
      assert(m.status === 'closed');
      server.getConnections(function(err, num) {
        closeSockets(i + 1);
      });
    });
  };

  var closeEmitted = false;
  server.on('close', function() {
    closeEmitted = true;
  });

  server.listen(common.PORT, '127.0.0.1');

  process.on('exit', function() {
    assert.equal(sent, count);
    assert.equal(disconnected, count);
    assert.ok(closeEmitted);
    console.log('ok');
  });
}
