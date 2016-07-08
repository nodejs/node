'use strict';
require('../common');
const assert = require('assert');
const fork = require('child_process').fork;
const net = require('net');

// progress tracker
function ProgressTracker(missing, callback) {
  this.missing = missing;
  this.callback = callback;
}
ProgressTracker.prototype.done = function() {
  this.missing -= 1;
  this.check();
};
ProgressTracker.prototype.check = function() {
  if (this.missing === 0) this.callback();
};

if (process.argv[2] === 'child') {

  var serverScope;

  process.on('message', function onServer(msg, server) {
    if (msg.what !== 'server') return;
    process.removeListener('message', onServer);

    serverScope = server;

    server.on('connection', function(socket) {
      console.log('CHILD: got connection');
      process.send({what: 'connection'});
      socket.destroy();
    });

    // start making connection from parent
    console.log('CHILD: server listening');
    process.send({what: 'listening'});
  });

  process.on('message', function onClose(msg) {
    if (msg.what !== 'close') return;
    process.removeListener('message', onClose);

    serverScope.on('close', function() {
      process.send({what: 'close'});
    });
    serverScope.close();
  });

  process.on('message', function onSocket(msg, socket) {
    if (msg.what !== 'socket') return;
    process.removeListener('message', onSocket);
    socket.end('echo');
    console.log('CHILD: got socket');
  });

  process.send({what: 'ready'});
} else {

  var child = fork(process.argv[1], ['child']);

  child.on('exit', function() {
    console.log('CHILD: died');
  });

  // send net.Server to child and test by connecting
  var testServer = function(callback) {

    // destroy server execute callback when done
    var progress = new ProgressTracker(2, function() {
      server.on('close', function() {
        console.log('PARENT: server closed');
        child.send({what: 'close'});
      });
      server.close();
    });

    // we expect 4 connections and close events
    var connections = new ProgressTracker(4, progress.done.bind(progress));
    var closed = new ProgressTracker(4, progress.done.bind(progress));

    // create server and send it to child
    var server = net.createServer();
    server.on('connection', function(socket) {
      console.log('PARENT: got connection');
      socket.destroy();
      connections.done();
    });
    server.on('listening', function() {
      console.log('PARENT: server listening');
      child.send({what: 'server'}, server);
    });
    server.listen(0);

    // handle client messages
    var messageHandlers = function(msg) {

      if (msg.what === 'listening') {
        // make connections
        var socket;
        for (var i = 0; i < 4; i++) {
          socket = net.connect(server.address().port, function() {
            console.log('CLIENT: connected');
          });
          socket.on('close', function() {
            closed.done();
            console.log('CLIENT: closed');
          });
        }

      } else if (msg.what === 'connection') {
        // child got connection
        connections.done();
      } else if (msg.what === 'close') {
        child.removeListener('message', messageHandlers);
        callback();
      }
    };

    child.on('message', messageHandlers);
  };

  // send net.Socket to child
  var testSocket = function(callback) {

    // create a new server and connect to it,
    // but the socket will be handled by the child
    var server = net.createServer();
    server.on('connection', function(socket) {
      socket.on('close', function() {
        console.log('CLIENT: socket closed');
      });
      child.send({what: 'socket'}, socket);
    });
    server.on('close', function() {
      console.log('PARENT: server closed');
      callback();
    });
    // don't listen on the same port, because SmartOS sometimes says
    // that the server's fd is closed, but it still cannot listen
    // on the same port again.
    //
    // An isolated test for this would be lovely, but for now, this
    // will have to do.
    server.listen(0, function() {
      console.error('testSocket, listening');
      var connect = net.connect(server.address().port);
      var store = '';
      connect.on('data', function(chunk) {
        store += chunk;
        console.log('CLIENT: got data');
      });
      connect.on('close', function() {
        console.log('CLIENT: closed');
        assert.equal(store, 'echo');
        server.close();
      });
    });
  };

  // create server and send it to child
  var serverSuccess = false;
  var socketSuccess = false;
  child.on('message', function onReady(msg) {
    if (msg.what !== 'ready') return;
    child.removeListener('message', onReady);

    testServer(function() {
      serverSuccess = true;

      testSocket(function() {
        socketSuccess = true;
        child.kill();
      });
    });

  });

  process.on('exit', function() {
    assert.ok(serverSuccess);
    assert.ok(socketSuccess);
  });

}
