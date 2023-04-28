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
}, common.mustSucceed((pair, cleanup) => {
  const peer = pair.client.conn.getPeerCertificate();
  debug('peer:\n', peer);
  assert.strictEqual(peer.subject.emailAddress, 'adam.lippai@tresorit.com');
  assert.strictEqual(peer.subject.CN, 'Ádám Lippai');
  assert.strictEqual(peer.issuer.CN, 'ca3');
  assert.match(peer.serialNumber, /5B75D77EDC7FB5B7FA9F1424DA4C64FB815DCBDE/i);

  const next = pair.client.conn.getPeerCertificate(true).issuerCertificate;
  const root = next.issuerCertificate;
  delete next.issuerCertificate;
  debug('next:\n', next);
  assert.strictEqual(next.subject.CN, 'ca3');
  assert.strictEqual(next.issuer.CN, 'ca1');
  assert.match(next.serialNumber, /147D36C1C2F74206DE9FAB5F2226D78ADB00A425/i);

  debug('root:\n', root);
  assert.strictEqual(root.subject.CN, 'ca1');
  assert.strictEqual(root.issuer.CN, 'ca1');
  assert.match(root.serialNumber, /4AB16C8DFD6A7D0D2DFCABDF9C4B0E92C6AD0229/i);

  // No client cert, so empty object returned.
  assert.deepStrictEqual(pair.server.conn.getPeerCertificate(), {});
  assert.deepStrictEqual(pair.server.conn.getPeerCertificate(true), {});

  return cleanup();
}));
