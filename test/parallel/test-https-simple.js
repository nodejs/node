'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  return;
}
var https = require('https');

var fs = require('fs');

var options = {
  key: fs.readFileSync(common.fixturesDir + '/keys/agent1-key.pem'),
  cert: fs.readFileSync(common.fixturesDir + '/keys/agent1-cert.pem')
};

var reqReceivedCount = 0;
const tests = 2;
var successful = 0;

var testSucceeded = function() {
  successful = successful + 1;
  if (successful === tests) {
    server.close();
  }
};

var body = 'hello world\n';

var server = https.createServer(options, function(req, res) {
  reqReceivedCount++;
  res.writeHead(200, { 'content-type': 'text/plain' });
  res.end(body);
});


server.listen(common.PORT, function() {
  // Do a request ignoring the invalid server certs
  var noCertCheckOptions = {
    hostname: '127.0.0.1',
    port: common.PORT,
    path: '/',
    method: 'GET',
    rejectUnauthorized: false
  };
  noCertCheckOptions.Agent = new https.Agent(noCertCheckOptions);

  var req = https.request(noCertCheckOptions, function(res) {
    var responseBody = '';
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
  var checkCertOptions = {
    hostname: '127.0.0.1',
    port: common.PORT,
    path: '/',
    method: 'GET'
  };

  var checkCertReq = https.request(checkCertOptions, function(res) {
    res.on('data', function() {
      throw Error('data should not be received');
    });

    res.on('end', function() {
      throw Error('connection should not be established');
    });
  });
  checkCertReq.end();

  checkCertReq.on('error', function(e) {
    assert.equal(e.code, 'UNABLE_TO_VERIFY_LEAF_SIGNATURE');
    testSucceeded();
  });
});

process.on('exit', function() {
  assert.equal(1, reqReceivedCount);
  assert.equal(successful, tests);
});
