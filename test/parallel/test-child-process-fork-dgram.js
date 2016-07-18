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
 * message. The worst case scenario is when either one never receives
 * a message. In this case the test runner will timeout after 60 secs
 * and the test will fail.
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
  process.on('message', function removeMe(msg, clusterServer) {
    if (msg === 'server') {
      server = clusterServer;

      server.on('message', function() {
        process.send('gotMessage');
      });

    } else if (msg === 'stop') {
      server.close();
      process.removeListener('message', removeMe);
    }
  });

} else {
  server = dgram.createSocket('udp4');
  var client = dgram.createSocket('udp4');
  var child = fork(__filename, ['child']);

  var msg = Buffer.from('Some bytes');

  var childGotMessage = false;
  var parentGotMessage = false;

  server.on('message', function(msg, rinfo) {
    parentGotMessage = true;
  });

  server.on('listening', function() {
    child.send('server', server);

    child.once('message', function(msg) {
      if (msg === 'gotMessage') {
        childGotMessage = true;
      }
    });

    sendMessages();
  });

  var sendMessages = function() {
    var timer = setInterval(function() {
      client.send(
        msg,
        0,
        msg.length,
        server.address().port,
        '127.0.0.1',
        function(err) {
          if (err) throw err;
        }
      );

      /*
       * Both the parent and the child got at least one message,
       * test passed, clean up everyting.
       */
      if (parentGotMessage && childGotMessage) {
        clearInterval(timer);
        shutdown();
      }

    }, 1);
  };

  var shutdown = function() {
    child.send('stop');

    server.close();
    client.close();
  };

  server.bind(0, '127.0.0.1');

  process.once('exit', function() {
    assert(parentGotMessage);
    assert(childGotMessage);
  });
}
