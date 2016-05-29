'use strict';
var common = require('../common');
var assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}

var tls = require('tls');
var fs = require('fs');
var path = require('path');
var finished = 0;

function filenamePEM(n) {
  return path.join(common.fixturesDir, 'keys', n + '.pem');
}

function loadPEM(n) {
  return fs.readFileSync(filenamePEM(n));
}

var testCases = [
  { // Test 0: for the check of a cert not existed in the whitelist.
    // agent7-cert.pem is issued by the fake CNNIC root CA so that its
    // hash is not listed in the whitelist.
    // fake-cnnic-root-cert has the same subject name as the original
    // rootCA.
    serverOpts: {
      key: loadPEM('agent7-key'),
      cert: loadPEM('agent7-cert')
    },
    clientOpts: {
      port: undefined,
      rejectUnauthorized: true,
      ca: [loadPEM('fake-cnnic-root-cert')]
    },
    errorCode: 'CERT_REVOKED'
  },
  // Test 1: for the fix of node#2061
  // agent6-cert.pem is signed by intermidate cert of ca3.
  // The server has a cert chain of agent6->ca3->ca1(root) but
  // tls.connect should be failed with an error of
  // UNABLE_TO_GET_ISSUER_CERT_LOCALLY since the root CA of ca1 is not
  // installed locally.
  {
    serverOpts: {
      ca: loadPEM('ca3-key'),
      key: loadPEM('agent6-key'),
      cert: loadPEM('agent6-cert')
    },
    clientOpts: {
      port: undefined,
      rejectUnauthorized: true
    },
    errorCode: 'UNABLE_TO_GET_ISSUER_CERT_LOCALLY'
  }
];

function runTest(tindex) {
  var tcase = testCases[tindex];

  if (!tcase) return;

  var server = tls.createServer(tcase.serverOpts, function(s) {
    s.resume();
  }).listen(0, function() {
    tcase.clientOpts = this.address().port;
    var client = tls.connect(tcase.clientOpts);
    client.on('error', function(e) {
      assert.strictEqual(e.code, tcase.errorCode);
      server.close(function() {
        finished++;
        runTest(tindex + 1);
      });
    });
  });
}

runTest(0);

process.on('exit', function() {
  assert.equal(finished, testCases.length);
});
