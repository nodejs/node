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

var dgram = require('dgram');
var fork = require('child_process').fork;
var assert = require('assert');
var common = require('../common');

if (process.platform === 'win32') {
  console.error('Sending dgram sockets to child processes not supported');
  process.exit(0);
}

if (process.argv[2] === 'child') {
  var childCollected = 0;
  var server;

  process.on('message', function removeMe(msg, clusterServer) {
    if (msg === 'server') {
      server = clusterServer;

      server.on('message', function () {
        process.send('gotMessage');
      });

    } else if (msg === 'stop') {
      server.close();
      process.removeListener('message', removeMe);
    }
  });

} else {
  var server = dgram.createSocket('udp4');
  var client = dgram.createSocket('udp4');
  var child = fork(__filename, ['child']);

  var msg = new Buffer('Some bytes');

  var childGotMessage = false;
  var parentGotMessage = false;

  server.on('message', function (msg, rinfo) {
    parentGotMessage = true;
  });

  server.on('listening', function () {
    child.send('server', server);

    child.once('message', function (msg) {
      if (msg === 'gotMessage') {
        childGotMessage = true;
      }
    });

    sendMessages();
  });

  var sendMessages = function () {
    var timer = setInterval(function () {
      client.send(msg, 0, msg.length, common.PORT, '127.0.0.1', function(err) {
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

  var shutdown = function () {
    child.send('stop');

    server.close();
    client.close();
  };

  server.bind(common.PORT, '127.0.0.1');

  process.once('exit', function () {
    assert(parentGotMessage);
    assert(childGotMessage);
  });
}
