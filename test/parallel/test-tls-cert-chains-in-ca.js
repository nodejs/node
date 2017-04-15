'use strict';
const common = require('../common');

// Check cert chain is received by client, and is completed with the ca cert
// known to the client.

const join = require('path').join;
const {
  assert, connect, debug, keys
} = require(join(common.fixturesDir, 'tls-connect'));


// agent6-cert.pem includes cert for agent6 and ca3, split it apart and
// provide ca3 in the .ca property.
const agent6Chain = keys.agent6.cert.split(/(?=-----BEGIN CERTIFICATE-----)/);
const agent6End = agent6Chain[0];
const agent6Middle = agent6Chain[1];
connect({
  client: {
    checkServerIdentity: (servername, cert) => { },
    ca: keys.agent6.ca,
  },
  server: {
    cert: agent6End,
    key: keys.agent6.key,
    ca: agent6Middle,
  },
}, function(err, pair, cleanup) {
  assert.ifError(err);

  const peer = pair.client.conn.getPeerCertificate();
  debug('peer:\n', peer);
  assert.strictEqual(peer.serialNumber, 'C4CD893EF9A75DCC');

  const next = pair.client.conn.getPeerCertificate(true).issuerCertificate;
  const root = next.issuerCertificate;
  delete next.issuerCertificate;
  debug('next:\n', next);
  assert.strictEqual(next.serialNumber, '9A84ABCFB8A72ABF');

  debug('root:\n', root);
  assert.strictEqual(root.serialNumber, '8DF21C01468AF393');

  return cleanup();
});
