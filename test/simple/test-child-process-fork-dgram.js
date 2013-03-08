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
        childCollected += 1;
      });

    } else if (msg === 'stop') {
      server.close();
      process.send(childCollected);
      process.removeListener('message', removeMe);
    }
  });

} else {
  var server = dgram.createSocket('udp4');
  var client = dgram.createSocket('udp4');
  var child = fork(__filename, ['child']);
  var msg = new Buffer('Some bytes');

  var parentCollected = 0;
  var childCollected = 0;
  server.on('message', function (msg, rinfo) {
    parentCollected += 1;
  });

  server.on('listening', function () {
    child.send('server', server);

    sendMessages();
  });

  var sendMessages = function () {
    var wait = 0;
    var send = 0;
    var total = 100;

    var timer = setInterval(function () {
      send += 1;
      if (send === total) {
        clearInterval(timer);
      }

      client.send(msg, 0, msg.length, common.PORT, '127.0.0.1', function(err) {
          if (err) throw err;

          wait += 1;
          if (wait === total) {
            shutdown();
          }
        }
      );
    }, 1);
  };

  var shutdown = function () {
    child.send('stop');
    child.once('message', function (collected) {
      childCollected = collected;
    });

    server.close();
    client.close();
  };

  server.bind(common.PORT, '127.0.0.1');

  process.once('exit', function () {
    assert(childCollected > 0);
    assert(parentCollected > 0);
  });
}
