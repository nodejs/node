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
  const tcase = testCases[tindex];

  if (!tcase) return;

  const server = tls.createServer(tcase.serverOpts, (s) => {
    s.resume();
  }).listen(0, common.mustCall(function() {
    tcase.clientOpts = this.address().port;
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
