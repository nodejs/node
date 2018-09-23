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

'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const fixtures = require('../common/fixtures');

const assert = require('assert');
const https = require('https');

const options = {
  key: fixtures.readSync('test_key.pem'),
  cert: fixtures.readSync('test_cert.pem')
};

const server = https.createServer(options, common.mustCall(function(req, res) {
  res.writeHead(200);
  res.end();
  req.resume();
}, 2)).listen(0, function() {
  unauthorized();
});

function unauthorized() {
  const req = https.request({
    port: server.address().port,
    rejectUnauthorized: false
  }, function(res) {
    assert(!req.socket.authorized);
    res.resume();
    rejectUnauthorized();
  });
  req.on('error', function(err) {
    throw err;
  });
  req.end();
}

function rejectUnauthorized() {
  const options = {
    port: server.address().port
  };
  options.agent = new https.Agent(options);
  const req = https.request(options, common.mustNotCall());
  req.on('error', function(err) {
    authorized();
  });
  req.end();
}

function authorized() {
  const options = {
    port: server.address().port,
    ca: [fixtures.readSync('test_cert.pem')]
  };
  options.agent = new https.Agent(options);
  const req = https.request(options, function(res) {
    res.resume();
    assert(req.socket.authorized);
    server.close();
  });
  req.on('error', common.mustNotCall());
  req.end();
}
