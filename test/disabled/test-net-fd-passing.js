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




var common = require('../common');
var assert = require('assert');
var net = require('net');

var tests_run = 0;

function fdPassingTest(path, port) {
  var greeting = 'howdy';
  var message = 'beep toot';
  var expectedData = ['[greeting] ' + greeting, '[echo] ' + message];

  var receiverArgs = [common.fixturesDir + '/net-fd-passing-receiver.js',
                      path,
                      greeting];
  var receiver = process.createChildProcess(process.ARGV[0], receiverArgs);

  var initializeSender = function() {
    var fdHighway = new net.Socket();

    fdHighway.on('connect', function() {
      var sender = net.createServer(function(socket) {
        fdHighway.sendFD(socket);
        socket.flush();
        socket.forceClose(); // want to close() the fd, not shutdown()
      });

      sender.on('listening', function() {
        var client = net.createConnection(port);

        client.on('connect', function() {
          client.write(message);
        });

        client.on('data', function(data) {
          assert.equal(expectedData[0], data);
          if (expectedData.length > 1) {
            expectedData.shift();
          }
          else {
            // time to shut down
            fdHighway.close();
            sender.close();
            client.forceClose();
          }
        });
      });

      tests_run += 1;
      sender.listen(port);
    });

    fdHighway.connect(path);


  };

  receiver.on('output', function(data) {
    var initialized = false;
    if ((! initialized) && (data == 'ready')) {
      initializeSender();
      initialized = true;
    }
  });
}

fdPassingTest('/tmp/passing-socket-test', 31075);

process.on('exit', function() {
  assert.equal(1, tests_run);
});
