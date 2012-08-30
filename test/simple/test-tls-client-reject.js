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

var options = {
  key: fs.readFileSync(path.join(common.fixturesDir, 'test_key.pem')),
  cert: fs.readFileSync(path.join(common.fixturesDir, 'test_cert.pem'))
};

var connectCount = 0;

var server = tls.createServer(options, function(socket) {
  ++connectCount;
  socket.on('data', function(data) {
    common.debug(data.toString());
    assert.equal(data, 'ok');
  });
}).listen(common.PORT, function() {
  unauthorized();
});

function unauthorized() {
  var socket = tls.connect({
    port: common.PORT,
    rejectUnauthorized: false
  }, function() {
    assert(!socket.authorized);
    socket.end();
    rejectUnauthorized();
  });
  socket.on('error', function(err) {
    assert(false);
  });
  socket.write('ok');
}

function rejectUnauthorized() {
  var socket = tls.connect(common.PORT, function() {
    assert(false);
  });
  socket.on('error', function(err) {
    common.debug(err);
    authorized();
  });
  socket.write('ng');
}

function authorized() {
  var socket = tls.connect(common.PORT, {
    ca: [fs.readFileSync(path.join(common.fixturesDir, 'test_cert.pem'))]
  }, function() {
    assert(socket.authorized);
    socket.end();
    server.close();
  });
  socket.on('error', function(err) {
    assert(false);
  });
  socket.write('ok');
}

process.on('exit', function() {
  assert.equal(connectCount, 3);
});
