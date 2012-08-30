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

if (!process.versions.openssl) {
  console.error('Skipping because node compiled without OpenSSL.');
  process.exit(0);
}

var common = require('../common');
var assert = require('assert');
var tls = require('tls');
var fs = require('fs');
var path = require('path');

var serverClosed = false;
var serverSocketClosed = false;
var clientClosed = false;
var clientSocketClosed = false;

var options = {
  key: fs.readFileSync(path.join(common.fixturesDir, 'test_key.pem')),
  cert: fs.readFileSync(path.join(common.fixturesDir, 'test_cert.pem'))
};

var server = tls.createServer(options, function(s) {
  console.log('server connected');
  s.socket.on('end', function() {
    console.log('server socket ended');
  });
  s.socket.on('close', function() {
    console.log('server socket closed');
    serverSocketClosed = true;
  });
  s.on('end', function() {
    console.log('server ended');
  });
  s.on('close', function() {
    console.log('server closed');
    serverClosed = true;
  });
  s.pause();
  console.log('server paused');
  process.nextTick(function() {
    s.resume();
    console.log('server resumed');
  });
  s.end();
});

server.listen(common.PORT, function() {
  var c = tls.connect({
    port: common.PORT,
    rejectUnauthorized: false
  }, function() {
    console.log('client connected');
    c.socket.on('end', function() {
      console.log('client socket ended');
    });
    c.socket.on('close', function() {
      console.log('client socket closed');
      clientSocketClosed = true;
    });
    c.pause();
    console.log('client paused');
    process.nextTick(function() {
      c.resume();
      console.log('client resumed');
    });
  });
  c.on('end', function() {
    console.log('client ended');
  });
  c.on('close', function() {
    console.log('client closed');
    clientClosed = true;
    server.close();
  });
});

process.on('exit', function() {
  assert(serverClosed);
  assert(serverSocketClosed);
  assert(clientClosed);
  assert(clientSocketClosed);
});
