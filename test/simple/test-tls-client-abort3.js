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
var common = require('../common');
var tls = require('tls');
var fs = require('fs');
var assert = require('assert');

var options = {
  key: fs.readFileSync(common.fixturesDir + '/test_key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/test_cert.pem')
};

var gotError = 0,
    gotRequest = 0,
    connected = 0;

var server = tls.createServer(options, function(c) {
  gotRequest++;
  c.on('data', function(data) {
    console.log(data.toString());
  });

  c.on('close', function() {
    server.close();
  });
}).listen(common.PORT, function() {
  var c = tls.connect(common.PORT, { rejectUnauthorized: false }, function() {
    connected++;
    c.pair.ssl.shutdown();
    c.write('123');
    c.destroy();
  });

  c.once('error', function() {
    gotError++;
  });
});

process.once('exit', function() {
  assert.equal(gotError, 1);
  assert.equal(gotRequest, 1);
  assert.equal(connected, 1);
});
