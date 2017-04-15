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
const assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
const https = require('https');

const fs = require('fs');
const path = require('path');

const options = {
  key: fs.readFileSync(path.join(common.fixturesDir, 'keys/agent3-key.pem')),
  cert: fs.readFileSync(path.join(common.fixturesDir, 'keys/agent3-cert.pem'))
};

const server = https.createServer(options, common.mustCall(function(req, res) {
  res.writeHead(200);
  res.end();
  req.resume();
})).listen(0, function() {
  authorized();
});

function authorized() {
  const req = https.request({
    port: server.address().port,
    rejectUnauthorized: true,
    ca: [fs.readFileSync(path.join(common.fixturesDir, 'keys/ca2-cert.pem'))]
  }, common.mustNotCall());
  req.on('error', function(err) {
    override();
  });
  req.end();
}

function override() {
  const options = {
    port: server.address().port,
    rejectUnauthorized: true,
    ca: [fs.readFileSync(path.join(common.fixturesDir, 'keys/ca2-cert.pem'))],
    checkServerIdentity: function(host, cert) {
      return false;
    }
  };
  options.agent = new https.Agent(options);
  const req = https.request(options, function(res) {
    assert(req.socket.authorized);
    server.close();
  });
  req.on('error', function(err) {
    throw err;
  });
  req.end();
}
