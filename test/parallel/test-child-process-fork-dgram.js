'use strict';
/*
 * The purpose of this test is to make sure that when forking a process,
 * sending a fd representing a UDP socket to the child and sending messages
 * to this endpoint, these messages are distributed to the parent and the
 * child process.
 *
 * Because it's not really possible to predict how the messages will be
 * distributed among the parent and the child processes, we keep sending
 * messages until both the parent and the child received at least one
 * message. When either the parent or child receives a message we close
 * the server on that side so the next message is guaranteed to be
 * received by the other.
 */

const common = require('../common');
var dgram = require('dgram');
var fork = require('child_process').fork;
var assert = require('assert');

if (common.isWindows) {
  common.skip('Sending dgram sockets to child processes is ' +
              'not supported');
  return;
}

var server;
if (process.argv[2] === 'child') {
  var serverClosed = false;
  process.on('message', function removeMe(msg, clusterServer) {
    if (msg === 'server') {
      server = clusterServer;

      server.on('message', function() {
        process.send('gotMessage');
        // got a message so close the server to make sure
        // the parent also gets a message
        serverClosed = true;
        server.close();
      });

    } else if (msg === 'stop') {
      process.removeListener('message', removeMe);
      if (!serverClosed) {
        server.close();
      }
    }
  });

} else {
  server = dgram.createSocket('udp4');
  var serverPort = null;
  var client = dgram.createSocket('udp4');
  var child = fork(__filename, ['child']);

  var msg = Buffer.from('Some bytes');

  var childGotMessage = false;
  var parentGotMessage = false;

  server.on('message', function(msg, rinfo) {
    parentGotMessage = true;
    // got a message so close the server to make sure
    // the child also gets a message
    server.close();
  });

  server.on('listening', function() {
    serverPort = server.address().port;
    child.send('server', server);

    child.once('message', function(msg) {
      if (msg === 'gotMessage') {
        childGotMessage = true;
      }
    });

    sendMessages();
  });

  var iterations = 0;
  var sendMessages = function() {
    var timer = setInterval(function() {
      iterations++;

      client.send(
        msg,
        0,
        msg.length,
        serverPort,
        '127.0.0.1',
        function(err) {
          if (err) throw err;
        }
      );

      /*
       * Both the parent and the child got at least one message,
       * test passed, clean up everyting.
       */
      if ((parentGotMessage && childGotMessage) ||
          ((iterations / 1000) > 45)) {
        clearInterval(timer);
        shutdown();
      }

    }, 1);
  };

  var shutdown = function() {
    child.send('stop');

    if (!parentGotMessage) {
      server.close();
    }
    client.close();
  };

  server.bind(0, '127.0.0.1');

  process.once('exit', function() {
    assert(parentGotMessage);
    assert(childGotMessage);
  });
}
