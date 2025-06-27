'use strict';
require('../common');
const fixtures = require('../common/fixtures');

// Check cert chain is received by client, and is completed with the ca cert
// known to the client.

const {
  assert, connect, debug, keys
} = require(fixtures.path('tls-connect'));


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
  assert.match(peer.serialNumber, /5B75D77EDC7FB5B7FA9F1424DA4C64FB815DCBDE/i);

  const next = pair.client.conn.getPeerCertificate(true).issuerCertificate;
  const root = next.issuerCertificate;
  delete next.issuerCertificate;
  debug('next:\n', next);
  assert.match(next.serialNumber, /147D36C1C2F74206DE9FAB5F2226D78ADB00A425/i);

  debug('root:\n', root);
  assert.match(root.serialNumber, /4AB16C8DFD6A7D0D2DFCABDF9C4B0E92C6AD0229/i);

  return cleanup();
});
