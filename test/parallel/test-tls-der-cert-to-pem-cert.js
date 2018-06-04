'use strict';
const common = require('../common');
const fixtures = require('../common/fixtures');
if (!common.hasCrypto) {
  common.skip('missing crypto');
}
const tls = require('tls');

// Verify that detailed getPeerCertificate() return value has all certs.

const {
  assert, connect, keys
} = require(fixtures.path('tls-connect'));

connect({
  client: { rejectUnauthorized: false },
  server: keys.agent1,
}, function(err, pair, cleanup) {
  assert.ifError(err);
  const socket = pair.client.conn;
  const peerCert = socket.getPeerCertificate();
  const pemCert = tls.derCertToPemCert(peerCert.raw);
  assert.strictEqual(pemCert, keys.agent1.cert);

  return cleanup();
});
