'use strict';
var common = require('../common');
var assert = require('assert');
var cluster = require('cluster');
var net = require('net');

function forEach(obj, fn) {
  Object.keys(obj).forEach(function(name, index) {
    fn(obj[name], name);
  });
}

if (cluster.isWorker) {
  // Create a tcp server. This will be used as cluster-shared-server and as an
  // alternative IPC channel.
  var server = net.Server();
  var socket, message;

  function maybeReply() {
    if (!socket || !message) return;

    // Tell master using TCP socket that a message is received.
    socket.write(JSON.stringify({
      code: 'received message',
      echo: message
    }));
  }

  server.on('connection', function(socket_) {
    socket = socket_;
    maybeReply();

    // Send a message back over the IPC channel.
    process.send('message from worker');
  });

  process.on('message', function(message_) {
    message = message_;
    maybeReply();
  });

  server.listen(common.PORT, '127.0.0.1');
} else if (cluster.isMaster) {

  var checks = {
    global: {
      'receive': false,
      'correct': false
    },
    master: {
      'receive': false,
      'correct': false
    },
    worker: {
      'receive': false,
      'correct': false
    }
  };


  var client;
  var check = function(type, result) {
    checks[type].receive = true;
    checks[type].correct = result;
    console.error('check', checks);

    var missing = false;
    forEach(checks, function(type) {
      if (type.receive === false) missing = true;
    });

    if (missing === false) {
      console.error('end client');
      client.end();
    }
  };

  // Spawn worker
  var worker = cluster.fork();

  // When a IPC message is received from the worker
  worker.on('message', function(message) {
    check('master', message === 'message from worker');
  });
  cluster.on('message', function(worker_, message) {
    assert.strictEqual(worker_, worker);
    check('global', message === 'message from worker');
  });

  // When a TCP server is listening in the worker connect to it
  worker.on('listening', function() {

    client = net.connect(common.PORT, function() {
      // Send message to worker.
      worker.send('message from master');
    });

    client.on('data', function(data) {
      // All data is JSON
      data = JSON.parse(data.toString());

      if (data.code === 'received message') {
        check('worker', data.echo === 'message from master');
      } else {
        throw new Error('wrong TCP message recived: ' + data);
      }
    });

    // When the connection ends kill worker and shutdown process
    client.on('end', function() {
      worker.kill();
    });

    worker.on('exit', function() {
      process.exit(0);
    });
  });

  process.once('exit', function() {
    forEach(checks, function(check, type) {
      assert.ok(check.receive, 'The ' + type + ' did not receive any message');
      assert.ok(check.correct,
                'The ' + type + ' did not get the correct message');
    });
  });
}
