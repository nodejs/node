'use strict';
const common = require('../common');

// Verify that every CRL in a concatenated PEM bundle is loaded, not just the
// first one. agent3 is revoked by ca2-crl-agent3.pem, but not by ca2-crl.pem.

const fixtures = require('../common/fixtures');
const {
  assert, connect, keys
} = require(fixtures.path('tls-connect'));

const crl = fixtures.readKey('ca2-crl.pem') +
            fixtures.readKey('ca2-crl-agent3.pem');

connect({
  client: {
    servername: 'agent3',
    ca: keys.agent3.ca,
    crl,
  },
  server: {
    cert: keys.agent3.cert,
    key: keys.agent3.key,
  },
}, common.mustCall((err, pair, cleanup) => {
  assert(err);
  assert.strictEqual(err.code, 'CERT_REVOKED');
  return cleanup();
}));
