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
var tls = require('tls');
var net = require('net');
var fs = require('fs');
var path = require('path');

var serverConnected = 0;
var clientConnected = 0;

var options = {
  key: fs.readFileSync(path.join(common.fixturesDir, 'test_key.pem')),
  cert: fs.readFileSync(path.join(common.fixturesDir, 'test_cert.pem'))
};

var server = tls.createServer(options, function(socket) {
  serverConnected++;
  socket.end('Hello');
}).listen(common.PORT, function() {
  var waiting = 2;
  function establish(socket) {
    var client = tls.connect({
      rejectUnauthorized: false,
      socket: socket
    }, function() {
      clientConnected++;
      var data = '';
      client.on('data', function(chunk) {
        data += chunk.toString();
      });
      client.on('end', function() {
        assert.equal(data, 'Hello');
        if (--waiting === 0)
          server.close();
      });
    });
  }

  // Already connected socket
  var connected = net.connect(common.PORT, function() {
    establish(connected);
  });

  // Connecting socket
  var connecting = net.connect(common.PORT);
  establish(connecting);
});

process.on('exit', function() {
  assert.equal(serverConnected, 2);
  assert.equal(clientConnected, 2);
});
