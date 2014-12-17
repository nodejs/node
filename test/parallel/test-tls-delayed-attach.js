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

var assert = require('assert');
var fs = require('fs');
var net = require('net');
var tls = require('tls');

var common = require('../common');

var sent = 'hello world';
var received = '';
var ended = 0;

var options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
};

var server = net.createServer(function(c) {
  setTimeout(function() {
    var s = new tls.TLSSocket(c, {
      isServer: true,
      secureContext: tls.createSecureContext(options)
    });

    s.on('data', function(chunk) {
      received += chunk;
    });

    s.on('end', function() {
      ended++;
      server.close();
      s.destroy();
    });
  }, 200);
}).listen(common.PORT, function() {
  var c = tls.connect(common.PORT, {
    rejectUnauthorized: false
  }, function() {
    c.end(sent);
  });
});

process.on('exit', function() {
  assert.equal(received, sent);
  assert.equal(ended, 1);
});
