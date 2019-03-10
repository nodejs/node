'use strict';
const common = require('../common');
const fixtures = require('../common/fixtures');

// Check cert chain is received by client, and is completed with the ca cert
// known to the client.

const {
  assert, connect, debug, keys
} = require(fixtures.path('tls-connect'));

// agent6-cert.pem includes cert for agent6 and ca3
connect({
  client: {
    checkServerIdentity: (servername, cert) => { },
    ca: keys.agent6.ca,
  },
  server: {
    cert: keys.agent6.cert,
    key: keys.agent6.key,
  },
}, common.mustCall((err, pair, cleanup) => {
  assert.ifError(err);

  const peer = pair.client.conn.getPeerCertificate();
  debug('peer:\n', peer);
  assert.strictEqual(peer.subject.emailAddress, 'adam.lippai@tresorit.com');
  assert.strictEqual(peer.subject.CN, 'Ádám Lippai');
  assert.strictEqual(peer.issuer.CN, 'ca3');
  assert.strictEqual(peer.serialNumber, 'D0082F458B6EFBE8');

  const next = pair.client.conn.getPeerCertificate(true).issuerCertificate;
  const root = next.issuerCertificate;
  delete next.issuerCertificate;
  debug('next:\n', next);
  assert.strictEqual(next.subject.CN, 'ca3');
  assert.strictEqual(next.issuer.CN, 'ca1');
  assert.strictEqual(next.serialNumber, 'ECC9B856270DA9A7');

  debug('root:\n', root);
  assert.strictEqual(root.subject.CN, 'ca1');
  assert.strictEqual(root.issuer.CN, 'ca1');
  assert.strictEqual(root.serialNumber, 'CB153AE212609FC6');

  // No client cert, so empty object returned.
  assert.deepStrictEqual(pair.server.conn.getPeerCertificate(), {});
  assert.deepStrictEqual(pair.server.conn.getPeerCertificate(true), {});

  return cleanup();
}));
