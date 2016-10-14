'use strict';
/*
 * The purpose of this test is to make sure that when forking a process,
 * sending a fd representing a UDP socket to the child and sending messages
 * to this endpoint, these messages are distributed to the parent and the
 * child process.
 */

const common = require('../common');
const dgram = require('dgram');
const fork = require('child_process').fork;
const assert = require('assert');

if (common.isWindows) {
  common.skip('Sending dgram sockets to child processes is not supported');
  return;
}

if (process.argv[2] === 'child') {
  let childServer;

  process.once('message', function(msg, clusterServer) {
    childServer = clusterServer;

    childServer.once('message', function() {
      process.send('gotMessage');
      childServer.close();
    });

    process.send('handleReceived');
  });

} else {
  const parentServer = dgram.createSocket('udp4');
  const client = dgram.createSocket('udp4');
  const child = fork(__filename, ['child']);

  const msg = Buffer.from('Some bytes');

  var childGotMessage = false;
  var parentGotMessage = false;

  parentServer.once('message', function(msg, rinfo) {
    parentGotMessage = true;
    parentServer.close();
  });

  parentServer.on('listening', function() {
    child.send('server', parentServer);

    child.on('message', function(msg) {
      if (msg === 'gotMessage') {
        childGotMessage = true;
      } else if (msg = 'handlReceived') {
        sendMessages();
      }
    });
  });

  const sendMessages = function() {
    const serverPort = parentServer.address().port;

    const timer = setInterval(function() {
      /*
       * Both the parent and the child got at least one message,
       * test passed, clean up everyting.
       */
      if (parentGotMessage && childGotMessage) {
        clearInterval(timer);
        client.close();
      } else {
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
      }
    }, 1);
  };

  parentServer.bind(0, '127.0.0.1');

  process.once('exit', function() {
    assert(parentGotMessage);
    assert(childGotMessage);
  });
}
