'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

// Verify that every CRL in a concatenated PEM bundle is loaded, not just the
// first one. agent3 is revoked by ca2-crl-agent3.pem, but not by ca2-crl.pem.

const fixtures = require('../common/fixtures');
const tls = require('tls');
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

// A bundle whose second entry does not parse must throw rather than quietly
// apply only the CRLs that were read.
const lines = fixtures.readKey('ca2-crl-agent3.pem', 'utf8').split('\n');
lines[2] = 'AAAA' + lines[2].slice(4);
assert.throws(() => {
  tls.createSecureContext({
    crl: fixtures.readKey('ca2-crl.pem', 'utf8') + lines.join('\n'),
  });
}, { code: 'ERR_CRYPTO_OPERATION_FAILED' });
