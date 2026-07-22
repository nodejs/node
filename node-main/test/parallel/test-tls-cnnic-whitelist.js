// Flags: --use-bundled-ca
'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');
const fixtures = require('../common/fixtures');

function loadPEM(n) {
  return fixtures.readKey(`${n}.pem`);
}

const testCases = [
  // Test 1: for the fix of node#2061
  // agent6-cert.pem is signed by intermediate cert of ca3.
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
  },
];

function runTest(tindex) {
  const tcase = testCases[tindex];

  if (!tcase) return;

  const server = tls.createServer(tcase.serverOpts, (s) => {
    s.resume();
  }).listen(0, common.mustCall(function() {
    tcase.clientOpts.port = this.address().port;
    const client = tls.connect(tcase.clientOpts);
    client.on('error', common.mustCall((e) => {
      assert.strictEqual(e.code, tcase.errorCode);
      server.close(common.mustCall(() => {
        runTest(tindex + 1);
      }));
    }));
  }));
}

runTest(0);
