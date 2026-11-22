'use strict';

// The OCSPRequest event reports the certificate that the connection uses,
// also when the SNI context only carries a CA.

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const crypto = require('crypto');
const tls = require('tls');
const fixtures = require('../common/fixtures');

const key = fixtures.readKey('agent1-key.pem');
const cert = fixtures.readKey('agent1-cert.pem');
const ca = fixtures.readKey('ca1-cert.pem');

const server = tls.createServer({
  key,
  cert,
  SNICallback: common.mustCall((servername, callback) => {
    callback(null, tls.createSecureContext({ ca }));
  }),
});

server.on('OCSPRequest', common.mustCall((certificate, issuer, callback) => {
  assert.deepStrictEqual(
    certificate,
    new crypto.X509Certificate(cert).raw);
  callback(null, null);
}));

server.listen(0, common.mustCall(() => {
  const client = tls.connect({
    port: server.address().port,
    servername: 'sni.example.com',
    requestOCSP: true,
    rejectUnauthorized: false,
  });

  client.on('OCSPResponse', common.mustCall(() => {
    client.destroy();
    server.close();
  }));
}));
