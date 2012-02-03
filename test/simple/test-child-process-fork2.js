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

var socketCloses = 0;
var N = 10;

var n = fork(common.fixturesDir + '/fork2.js');

var messageCount = 0;

var server = new net.Server(function(c) {
  console.log('PARENT got connection');
  c.destroy();
});

// TODO need better API for this.
server._backlog = 9;

server.listen(common.PORT, function() {
  console.log('PARENT send child server handle');
  n.send({ hello: 'world' }, server._handle);
});

function makeConnections() {
  for (var i = 0; i < N; i++) {
    var socket = net.connect(common.PORT, function() {
      console.log('CLIENT connected');
    });

    socket.on('close', function() {
      socketCloses++;
      console.log('CLIENT closed ' + socketCloses);
      if (socketCloses == N) {
        n.kill();
        server.close();
      }
    });
  }
}

n.on('message', function(m) {
  console.log('PARENT got message:', m);
  if (m.gotHandle) {
    makeConnections();
  }
  messageCount++;
});

process.on('exit', function() {
  assert.equal(10, socketCloses);
  assert.ok(messageCount > 1);
});

