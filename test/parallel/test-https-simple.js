'use strict';
const common = require('../common');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}

const assert = require('assert');
const https = require('https');
const fs = require('fs');

const options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
};

const tests = 2;
let successful = 0;

const testSucceeded = function() {
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

server.listen(0, function() {
  // Do a request ignoring the unauthorized server certs
  const noCertCheckOptions = {
    hostname: '127.0.0.1',
    port: this.address().port,
    path: '/',
    method: 'GET',
    rejectUnauthorized: false
  };
  noCertCheckOptions.Agent = new https.Agent(noCertCheckOptions);

  const req = https.request(noCertCheckOptions, function(res) {
    let responseBody = '';
    res.on('data', function(d) {
      responseBody = responseBody + d;
    });

    res.on('end', function() {
      assert.equal(responseBody, body);
      testSucceeded();
    });
  });
  req.end();

  req.on('error', function(e) {
    throw e;
  });

  // Do a request that throws error due to the invalid server certs
  const checkCertOptions = {
    hostname: '127.0.0.1',
    port: this.address().port,
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

  checkCertReq.on('error', function(e) {
    assert.equal(e.code, 'UNABLE_TO_VERIFY_LEAF_SIGNATURE');
    testSucceeded();
  });
});

process.on('exit', function() {
  assert.equal(successful, tests);
});
