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
var https = require('https');
var Buffer = require('buffer').Buffer;
var fs = require('fs');
var path = require('path');

var options = {
  key: fs.readFileSync(path.join(common.fixturesDir, 'test_key.pem')),
  cert: fs.readFileSync(path.join(common.fixturesDir, 'test_cert.pem'))
};

var buf = new Buffer(1024 * 1024);
var sent = 0;
var received = 0;

var server = https.createServer(options, function(req, res) {
  res.writeHead(200);
  for (var i = 0; i < 50; i++) {
    res.write(buf);
  }
  res.end();
});

server.listen(common.PORT, function() {
  var resumed = false;
  var req = https.request({
    method: 'POST',
    port: common.PORT,
    rejectUnauthorized: false
  }, function(res) {
    res.read(0);

    setTimeout(function() {
      // Read buffer should be somewhere near high watermark
      // (i.e. should not leak)
      assert(res._readableState.length < 100 * 1024);
      process.exit(0);
    }, 2000);
  });
  req.end();
});
