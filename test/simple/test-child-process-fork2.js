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
      console.log("CLIENT connected");
    });

    socket.on("close", function() {
      socketCloses++;
      console.log("CLIENT closed " + socketCloses);
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

