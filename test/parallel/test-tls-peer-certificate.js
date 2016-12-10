'use strict';
const common = require('../common');
const assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
const tls = require('tls');

const fs = require('fs');
const util = require('util');
const join = require('path').join;

const options = {
  key: fs.readFileSync(join(common.fixturesDir, 'keys', 'agent1-key.pem')),
  cert: fs.readFileSync(join(common.fixturesDir, 'keys', 'agent1-cert.pem')),
  ca: [ fs.readFileSync(join(common.fixturesDir, 'keys', 'ca1-cert.pem')) ]
};

const server = tls.createServer(options, function(cleartext) {
  cleartext.end('World');
});
server.listen(0, common.mustCall(function() {
  const socket = tls.connect({
    port: this.address().port,
    rejectUnauthorized: false
  }, common.mustCall(function() {
    let peerCert = socket.getPeerCertificate();
    assert.ok(!peerCert.issuerCertificate);

    // Verify that detailed return value has all certs
    peerCert = socket.getPeerCertificate(true);
    assert.ok(peerCert.issuerCertificate);

    console.error(util.inspect(peerCert));
    assert.strictEqual(peerCert.subject.emailAddress, 'ry@tinyclouds.org');
    assert.strictEqual(peerCert.serialNumber, '9A84ABCFB8A72AC0');
    assert.strictEqual(peerCert.exponent, '0x10001');
    assert.strictEqual(
      peerCert.fingerprint,
      '8D:06:3A:B3:E5:8B:85:29:72:4F:7D:1B:54:CD:95:19:3C:EF:6F:AA'
    );
    assert.deepStrictEqual(peerCert.infoAccess['OCSP - URI'],
                           [ 'http://ocsp.nodejs.org/' ]);

    const issuer = peerCert.issuerCertificate;
    assert.strictEqual(issuer.issuerCertificate, issuer);
    assert.strictEqual(issuer.serialNumber, '8DF21C01468AF393');
    server.close();
  }));
  socket.end('Hello');
}));
