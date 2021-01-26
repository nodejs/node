'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');
const fixtures = require('../common/fixtures');
const { X509Certificate } = require('crypto');

const options = {
  key: fixtures.readKey('agent6-key.pem'),
  cert: fixtures.readKey('agent6-cert.pem')
};

const server = tls.createServer(options, function(cleartext) {
  cleartext.end('World');
});

server.once('secureConnection', common.mustCall(function(socket) {
  const cert = socket.getX509Certificate();
  assert(cert instanceof X509Certificate);
  assert.strictEqual(
    cert.serialNumber,
    'D0082F458B6EFBE8');
}));

server.listen(0, common.mustCall(function() {
  const socket = tls.connect({
    port: this.address().port,
    rejectUnauthorized: false
  }, common.mustCall(function() {
    const peerCert = socket.getPeerX509Certificate();
    assert(peerCert.issuerCertificate instanceof X509Certificate);
    assert.strictEqual(peerCert.issuerCertificate.issuerCertificate, undefined);
    assert.strictEqual(
      peerCert.issuerCertificate.serialNumber,
      'ECC9B856270DA9A7'
    );
    server.close();
  }));
  socket.end('Hello');
}));
