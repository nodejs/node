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
var fs = require('fs');
var path = require('path');
var net = require('net');
var tls = require('tls');
var assert = require('assert');

var options, a, b, portA, portB;
var gotHello = false;

options = {
  key: fs.readFileSync(path.join(common.fixturesDir, 'test_key.pem')),
  cert: fs.readFileSync(path.join(common.fixturesDir, 'test_cert.pem'))
};

// the "proxy" server
a = tls.createServer(options, function (socket) {
  var options = {
    host: '127.0.0.1',
    port: b.address().port,
    rejectUnauthorized: false
  };
  var dest = net.connect(options);
  dest.pipe(socket);
  socket.pipe(dest);
});

// the "target" server
b = tls.createServer(options, function (socket) {
  socket.end('hello');
});

process.on('exit', function () {
  assert(gotHello);
});

a.listen(common.PORT, function () {
  b.listen(common.PORT + 1, function () {
    options = {
      host: '127.0.0.1',
      port: a.address().port,
      rejectUnauthorized: false
    };
    var socket = tls.connect(options);
    var ssl;
    ssl = tls.connect({
      socket: socket,
      rejectUnauthorized: false
    });
    ssl.setEncoding('utf8');
    ssl.once('data', function (data) {
      assert.equal('hello', data);
      gotHello = true;
    });
    ssl.on('end', function () {
      ssl.end();
      a.close();
      b.close();
    });
  });
});
