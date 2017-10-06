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
const fixtures = require('../common/fixtures');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const https = require('https');

const options = {
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem')
};

const tests = 2;
let successful = 0;

const testSucceeded = () => {
  successful = successful + 1;
  if (successful === tests) {
    server.close();
  }
};

const body = 'hello world\n';

const serverCallback = common.mustCall(function(req, res) {
  res.writeHead(200, { 'content-type': 'text/plain' });
  res.end(body);
});

const server = https.createServer(options, serverCallback);

server.listen(0, common.mustCall(() => {
  // Do a request ignoring the unauthorized server certs
  const port = server.address().port;

  const noCertCheckOptions = {
    hostname: '127.0.0.1',
    port: port,
    path: '/',
    method: 'GET',
    rejectUnauthorized: false
  };

  noCertCheckOptions.Agent = new https.Agent(noCertCheckOptions);

  const req = https.request(noCertCheckOptions, common.mustCall((res) => {
    let responseBody = '';
    res.on('data', function(d) {
      responseBody = responseBody + d;
    });

    res.on('end', common.mustCall(() => {
      assert.strictEqual(responseBody, body);
      testSucceeded();
    }));
  }));
  req.end();

  req.on('error', function(e) {
    throw e;
  });

  // Do a request that throws error due to the invalid server certs
  const checkCertOptions = {
    hostname: '127.0.0.1',
    port: port,
    path: '/',
    method: 'GET'
  };

  const checkCertReq = https.request(checkCertOptions, function(res) {
    res.on('data', function() {
      throw new Error('data should not be received');
    });

    res.on('end', function() {
      throw new Error('connection should not be established');
    });
  });
  checkCertReq.end();

  checkCertReq.on('error', common.mustCall((e) => {
    assert.strictEqual(e.code, 'UNABLE_TO_VERIFY_LEAF_SIGNATURE');
    testSucceeded();
  }));
}));

process.on('exit', function() {
  assert.strictEqual(successful, tests);
});
