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
var fs = require('fs');
var path = require('path');

var options = {
  key: fs.readFileSync(path.join(common.fixturesDir, 'keys/agent3-key.pem')),
  cert: fs.readFileSync(path.join(common.fixturesDir, 'keys/agent3-cert.pem'))
};

var reqCount = 0;

var server = https.createServer(options, function (req, res) {
  ++reqCount;
  res.writeHead(200);
  res.end();
  req.resume();
}).listen(common.PORT, function () {
  authorized();
});

function authorized() {
  var req = https.request({
    port: common.PORT,
    rejectUnauthorized: true,
    ca: [fs.readFileSync(path.join(common.fixturesDir, 'keys/ca2-cert.pem'))]
  }, function (res) {
    assert(false);
  });
  req.on('error', function (err) {
    override();
  });
  req.end();
}

function override() {
  var options = {
    port: common.PORT,
    rejectUnauthorized: true,
    ca: [fs.readFileSync(path.join(common.fixturesDir, 'keys/ca2-cert.pem'))],
    checkServerIdentity: function (host, cert) {
      return false;
    }
  };
  options.agent = new https.Agent(options);
  var req = https.request(options, function (res) {
    assert(req.socket.authorized);
    server.close();
  });
  req.on('error', function (err) {
    throw err;
  });
  req.end();
}

process.on('exit', function () {
  assert.equal(reqCount, 1);
});
